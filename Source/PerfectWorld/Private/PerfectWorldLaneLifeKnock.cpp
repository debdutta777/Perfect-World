// NL-04 and F5: the access bridge of UPWLaneLifeSubsystem for Private dwellings. The core decides (Doors.h: EvaluateDoorKnock and the
// Private branch of EvaluateDoorPassage; DoorBell.h names which doors have a bell and wraps the same knock as a ring); this file only
// supplies the two facts the core asks of the caller -- is a resident of the home actually there, and did this visitor ring on this
// approach -- and says the answer in words.
//
// F5, the calling bell. The owner's feedback after playing: the player should not enter everyone's house; first they ring the bell.
// The visitor now rings the bell (APWDoorBellActor), never the leaf. The leaf says "Ring the bell first." to anyone who has not been
// let in. An answered ring is the same transient HasKnocked fact a knock was: it is not saved, it lasts while the visitor stays near
// the door, and it ends when the leaf closes. The household closes an open dwelling leaf behind a visitor who has gone, and the
// threshold guard (UPWDoorGuardComponent) stops an un-admitted player crossing an open doorway.

#include "PerfectWorldLaneLifeSubsystem.h"

#include "DoorBell.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NpcBehaviour.h"
#include "NpcScheduleContent.h"
#include "PerfectWorldContentSubsystem.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldNpcPresenceSubsystem.h"

#define LOCTEXT_NAMESPACE "PerfectWorldLaneLifeKnock"

namespace
{
/** A knock holds only while the visitor stays near the door. */
constexpr double KnkForgetDistanceCm = 300.0;
constexpr float KnkTickSeconds = 0.25f;
/** Real seconds between the press and the resident reaching the door, and between "Someone is coming..." and the door being unlatched. */
constexpr float DbAnswerDelaySeconds = 1.2f;
constexpr float DbUnlatchDelaySeconds = 1.4f;
/** A visitor who has walked this far from the door before it is answered has left: nobody comes. */
constexpr double DbRingForgetDistanceCm = 600.0;
constexpr float DbKeeperSeconds = 0.5f;

FString KnkClockText(const int32 MinuteOfDay)
{
    const int32 Wrapped = ((MinuteOfDay % 1440) + 1440) % 1440;
    return FString::Printf(TEXT("%02d:%02d"), Wrapped / 60, Wrapped % 60);
}

std::int8_t DbCoreSide(const PerfectWorld::DoorState& Doors, const std::string_view Id)
{
    for (const PerfectWorld::DoorEntry& Entry : Doors.Doors)
    {
        if (Entry.Id == Id)
        {
            return Entry.SwingSide;
        }
    }
    return 0;
}
} // namespace

bool UPWLaneLifeSubsystem::IsHostHome(const PerfectWorld::DoorDef& Door)
{
    UWorld* World = GetWorld();
    UPWNpcPresenceSubsystem* Presence = World != nullptr ? World->GetSubsystem<UPWNpcPresenceSubsystem>() : nullptr;
    const UGameInstance* Instance = World != nullptr ? World->GetGameInstance() : nullptr;
    const UPWContentSubsystem* Content = Instance != nullptr ? Instance->GetSubsystem<UPWContentSubsystem>() : nullptr;
    const PerfectWorld::ContentDb* Db = Content != nullptr ? Content->GetDb() : nullptr;
    if (Presence == nullptr || Db == nullptr)
    {
        return false; // fail closed: nobody is known to be home
    }
    if (!bResidentsByHomeBuilt)
    {
        // The presenter keeps its schedules private, but the validated content names every person and their home zone.
        bResidentsByHomeBuilt = true;
        for (const PerfectWorld::ContentRecord* Record : Db->OfType("npc_schedule"))
        {
            const PerfectWorld::NpcScheduleResult Loaded = PerfectWorld::LoadNpcSchedule(*Db, Record->Id);
            if (Loaded.Ok() && !Loaded.Value->Person.HomeZone.empty())
            {
                ResidentsByHome.FindOrAdd(PWLaneLife::ToName(Loaded.Value->Person.HomeZone)).AddUnique(PWLaneLife::ToName(Loaded.Value->Person.PersonId));
            }
        }
    }
    const TArray<FName>* Residents = ResidentsByHome.Find(PWLaneLife::ToName(Door.LocationId));
    if (Residents == nullptr)
    {
        return false; // no resident exists for this home yet: the player meets "Nobody answers."
    }
    Presence->Reconcile(GetNow());
    for (const FName Person : *Residents)
    {
        const PerfectWorld::NpcIntent* Intent = Presence->IsRegistered(Person) ? Presence->FindIntent(Person) : nullptr;
        if (Intent == nullptr || Intent->AnchorId != Door.LocationId)
        {
            continue;
        }
        // A chat or a yield keeps the suspended state; someone travelling or asleep cannot come to the door.
        const PerfectWorld::NpcState State =
            Intent->State == PerfectWorld::NpcState::Chat || Intent->State == PerfectWorld::NpcState::Yield ? Intent->ResumeState : Intent->State;
        if (State == PerfectWorld::NpcState::Idle || State == PerfectWorld::NpcState::Work)
        {
            return true;
        }
    }
    return false;
}

void UPWLaneLifeSubsystem::HandleDoorKnock(APWDoorActor& Door, const PerfectWorld::DoorDef& Def, const PerfectWorld::DoorUseContext& Context,
                                           const APawn* By)
{
    const std::int32_t Minute = PerfectWorld::DoorMinuteOfDay(GetNow());
    const PerfectWorld::DoorKnockResult Knock = PerfectWorld::EvaluateDoorKnock(Def, Minute, Context);
    FText Message;
    switch (Knock.Answer)
    {
    case PerfectWorld::DoorKnockAnswer::Admitted:
        Message = LOCTEXT("RingAnswered", "Someone is coming...");
        KnockedDoors.Add(Door.GetDoorId(), &Door); // the door is unlatched for this visit: the next use of the leaf on this approach opens it
        if (!KnockTicker.IsValid())
        {
            KnockTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UPWLaneLifeSubsystem::TickKnocks), KnkTickSeconds);
        }
        break;
    case PerfectWorld::DoorKnockAnswer::NoAnswer:
        Message = LOCTEXT("KnockNoAnswer", "Nobody answers.");
        KnockedDoors.Remove(Door.GetDoorId());
        break;
    case PerfectWorld::DoorKnockAnswer::NotReceiving:
        Message = Knock.MinutesUntilVisiting >= 0
                      ? FText::Format(LOCTEXT("KnockNotReceiving", "They aren't receiving visitors now — visiting from {0}."),
                                      FText::FromString(KnkClockText(Minute + Knock.MinutesUntilVisiting)))
                      : LOCTEXT("KnockNotReceivingNoHours", "They aren't receiving visitors.");
        KnockedDoors.Remove(Door.GetDoorId());
        break;
    default:
        // The core would not knock (an invitation, a locked door): the ordinary refusal words.
        ShowDoorRefusal(By, Knock.Result, &Def);
        return;
    }
    ShowPlayerNotice(Message, By);
}

bool UPWLaneLifeSubsystem::TickKnocks(float DeltaTime)
{
    const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    for (auto It = KnockedDoors.CreateIterator(); It; ++It)
    {
        const APWDoorActor* Door = It.Value().Get();
        if (Door == nullptr || Player == nullptr || FVector::Dist(Door->GetActorLocation(), Player->GetActorLocation()) > KnkForgetDistanceCm)
        {
            It.RemoveCurrent();
        }
    }
    if (KnockedDoors.IsEmpty())
    {
        KnockTicker.Reset();
        return false; // stop; the next admitted knock registers it again
    }
    return true;
}

// ---- The calling bell ----

APWDoorActor* UPWLaneLifeSubsystem::FindRegisteredDoor(const FName DoorId) const
{
    for (const TWeakObjectPtr<APWDoorActor>& Weak : RegisteredDoors)
    {
        APWDoorActor* Door = Weak.Get();
        if (Door != nullptr && Door->GetDoorId() == DoorId)
        {
            return Door;
        }
    }
    return nullptr;
}

double UPWLaneLifeSubsystem::GetDoorInsideSide(const FName DoorId) const
{
    const double* Side = DoorInsideSides.Find(DoorId);
    return Side != nullptr ? *Side : 0.0;
}

bool UPWLaneLifeSubsystem::IsBellUnneeded(const FName DoorId) const
{
    const PerfectWorld::DoorDef* Def = PerfectWorld::FindDoor(PWLaneLife::ToStd(DoorId));
    return Def != nullptr && (KnockedDoors.Contains(DoorId) || IsResidentOf(*Def));
}

PerfectWorld::DoorUseContext UPWLaneLifeSubsystem::MakeEntryContext(const PerfectWorld::DoorDef& Def, const FName DoorId)
{
    PerfectWorld::DoorUseContext Context;
    Context.Direction = PerfectWorld::DoorDirection::Entry;
    Context.IsResident = IsResidentOf(Def);
    // TODO(invitations): IsInvited stays false until a fact says otherwise (see RequestDoorToggle); nothing in the core records one yet.
    Context.IsInvited = false;
    Context.IsHostHome = Def.Access == PerfectWorld::DoorAccess::Private && !Context.IsResident && IsHostHome(Def);
    Context.HasKnocked = KnockedDoors.Contains(DoorId);
    return Context;
}

bool UPWLaneLifeSubsystem::MayEnterThroughDoor(const FName DoorId, FText& OutWhy)
{
    const PerfectWorld::DoorDef* Def = PerfectWorld::FindDoor(PWLaneLife::ToStd(DoorId));
    if (Def == nullptr)
    {
        return true; // not a door the core knows: nothing to enforce here (RegisterDoor has already reported it)
    }
    const PerfectWorld::DoorUseContext Context = MakeEntryContext(*Def, DoorId);
    const PerfectWorld::DoorThresholdVerdict Verdict = PerfectWorld::EvaluateDoorThreshold(*Def, PerfectWorld::DoorMinuteOfDay(GetNow()), Context);
    if (Verdict.bMayCross)
    {
        return true;
    }
    if (Context.HasKnocked)
    {
        OutWhy = LOCTEXT("GuardNotReceiving", "They aren't receiving visitors now.");
    }
    else if (PerfectWorld::DoorHasBell(*Def))
    {
        OutWhy = LOCTEXT("RingFirst", "Ring the bell first.");
    }
    else
    {
        OutWhy = LOCTEXT("PrivateRoom", "That room is private.");
    }
    return false;
}

void UPWLaneLifeSubsystem::RingDoorBell(const FName DoorId, APawn* By)
{
    APWDoorActor* Door = FindRegisteredDoor(DoorId);
    const PerfectWorld::DoorDef* Def = PerfectWorld::FindDoor(PWLaneLife::ToStd(DoorId));
    if (Door == nullptr || Def == nullptr || !PerfectWorld::DoorHasBell(*Def) || RingTimers.Contains(DoorId))
    {
        return;
    }
    const PerfectWorld::DoorUseContext Context = MakeEntryContext(*Def, DoorId);
    const PerfectWorld::DoorRingResult Ring = PerfectWorld::EvaluateDoorRing(*Def, PerfectWorld::DoorMinuteOfDay(GetNow()), Context);
    switch (Ring.Outcome)
    {
    case PerfectWorld::DoorRingOutcome::NoBellNeeded:
        ShowPlayerNotice(Context.IsResident ? LOCTEXT("RingOwnDoor", "This is your own door; no need to ring.")
                                            : LOCTEXT("RingAlreadyAnswered", "Someone has already answered. Come in."),
                         By);
        return;
    case PerfectWorld::DoorRingOutcome::NoBell:
    case PerfectWorld::DoorRingOutcome::Refused:
        ShowDoorRefusal(By, Ring.Knock.Result, Def);
        return;
    default:
        break; // Answered, NoAnswer or NotReceiving: said when the resident reaches the door
    }
    // No sound system exists in the project yet: the visible pulse of the bell and this notice are the ring.
    ShowPlayerNotice(LOCTEXT("RingRung", "You ring the bell."), By);
    const TWeakObjectPtr<APawn> WeakBy(By);
    RingTimers.Add(DoorId, FTSTicker::GetCoreTicker().AddTicker(
                               FTickerDelegate::CreateWeakLambda(this, [this, DoorId, WeakBy](float)
                               {
                                   AnswerBell(DoorId, WeakBy);
                                   return false;
                               }),
                               DbAnswerDelaySeconds));
}

void UPWLaneLifeSubsystem::AnswerBell(const FName DoorId, const TWeakObjectPtr<APawn> By)
{
    RingTimers.Remove(DoorId); // the one-shot ticker that called this has returned false and is gone
    APWDoorActor* Door = FindRegisteredDoor(DoorId);
    const PerfectWorld::DoorDef* Def = PerfectWorld::FindDoor(PWLaneLife::ToStd(DoorId));
    APawn* Visitor = By.Get() != nullptr ? By.Get() : UGameplayStatics::GetPlayerPawn(this, 0);
    if (Door == nullptr || Def == nullptr || Visitor == nullptr ||
        FVector::Dist(Door->GetActorLocation(), Visitor->GetActorLocation()) > DbRingForgetDistanceCm)
    {
        return; // the visitor has gone: nobody comes to an empty step
    }
    // The core's knock, asked now that the resident could have reached the door.
    HandleDoorKnock(*Door, *Def, MakeEntryContext(*Def, DoorId), Visitor);
    if (!KnockedDoors.Contains(DoorId) || IsDoorOpen(DoorId, false))
    {
        return;
    }
    // The resident unlatches the door for this visit; the leaf itself stays shut until the visitor opens it (the interact key on it).
    const TWeakObjectPtr<APawn> WeakVisitor(Visitor);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this, DoorId, WeakVisitor](float)
    {
        if (KnockedDoors.Contains(DoorId) && !IsDoorOpen(DoorId, false))
        {
            ShowPlayerNotice(LOCTEXT("RingUnlatched", "The door is unlatched. Come in."), WeakVisitor.Get());
        }
        return false;
    }), DbUnlatchDelaySeconds);
}

void UPWLaneLifeSubsystem::StartDoorKeeper()
{
    if (!KeeperTicker.IsValid())
    {
        KeeperTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UPWLaneLifeSubsystem::TickDoorKeeper), DbKeeperSeconds);
    }
}

void UPWLaneLifeSubsystem::CancelRings()
{
    for (const TPair<FName, FTSTicker::FDelegateHandle>& Ring : RingTimers)
    {
        FTSTicker::GetCoreTicker().RemoveTicker(Ring.Value);
    }
    RingTimers.Reset();
    FTSTicker::GetCoreTicker().RemoveTicker(KeeperTicker);
    KeeperTicker.Reset();
}

bool UPWLaneLifeSubsystem::TickDoorKeeper(float DeltaTime)
{
    if (!Sim.IsValid() || UGameplayStatics::IsGamePaused(this))
    {
        return true;
    }
    const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    for (const TWeakObjectPtr<APWDoorActor>& Weak : RegisteredDoors)
    {
        APWDoorActor* Door = Weak.Get();
        if (Door == nullptr || !Door->IsOpen() || Door->IsMoving() || RingTimers.Contains(Door->GetDoorId()) || !IsDoorOpen(Door->GetDoorId(), false))
        {
            continue;
        }
        const PerfectWorld::DoorDef* Def = PerfectWorld::FindDoor(PWLaneLife::ToStd(Door->GetDoorId()));
        if (Def == nullptr || !PerfectWorld::DoorHasBell(*Def))
        {
            continue;
        }
        const bool bVisitorNearby = Player != nullptr && FVector::Dist(Door->GetActorLocation(), Player->GetActorLocation()) <= KnkForgetDistanceCm;
        if (PerfectWorld::ShouldHostCloseDoor(*Def, true, IsResidentOf(*Def), false, bVisitorNearby))
        {
            CloseDoorByHost(*Door);
        }
    }
    return true;
}

void UPWLaneLifeSubsystem::CloseDoorByHost(APWDoorActor& Door)
{
    const std::string Id = PWLaneLife::ToStd(Door.GetDoorId());
    const std::int8_t WasSide = DbCoreSide(Sim->State().Doors, Id);
    PerfectWorld::DoorSet Set;
    Set.DoorId = Id;
    Set.Open = false;
    Set.Context.Direction = PerfectWorld::DoorDirection::Exit; // shutting a leaf asks no entry permission
    PerfectWorld::Command Command;
    Command.Source = PerfectWorld::CommandSource::Player;
    Command.Payload = Set;
    if (!SubmitCommand(Command).Accepted)
    {
        return;
    }
    if (!Door.TryMoveTo(false, nullptr))
    {
        // Someone is standing in the swing: the household tries again next time. Put the core back as it was.
        PerfectWorld::DoorSettle Revert;
        Revert.DoorId = Id;
        Revert.IsOpenNow = true;
        Revert.SwingSide = WasSide;
        PerfectWorld::Command Undo;
        Undo.Source = PerfectWorld::CommandSource::Presentation;
        Undo.Payload = Revert;
        if (!SubmitCommand(Undo).Accepted)
        {
            UE_LOG(LogPWLaneLife, Warning, TEXT("Door %s could not be closed behind the visitor and the core would not take it back."), *Door.GetDoorId().ToString());
        }
    }
}

#undef LOCTEXT_NAMESPACE
