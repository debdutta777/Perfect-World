#include "PerfectWorldLaneLifeSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "DoorBell.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "LaneOrigin.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldFurnitureActor.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneLifeConversation.h"
#include "NpcBehaviour.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldReadingPresenter.h"

DEFINE_LOG_CATEGORY(LogPWLaneLife);

#define LOCTEXT_NAMESPACE "PerfectWorldLaneLife"

namespace
{
const PerfectWorld::DoorEntry* BrFindDoorEntry(const PerfectWorld::DoorState& Doors, const std::string_view Id)
{
    for (const PerfectWorld::DoorEntry& Entry : Doors.Doors)
    {
        if (Entry.Id == Id)
        {
            return &Entry;
        }
    }
    return nullptr;
}

/** "door.locked" back to DoorResult::Locked. The reducer names a refusal "door.<DoorResultCode>". */
PerfectWorld::DoorResult BrDoorResultFromCode(const FString& Code)
{
    for (int32 Value = 0; Value <= static_cast<int32>(PerfectWorld::DoorResult::Knocked); ++Value)
    {
        const PerfectWorld::DoorResult Candidate = static_cast<PerfectWorld::DoorResult>(Value);
        if (Code == PWLaneLife::ToFString("door." + std::string(PerfectWorld::DoorResultCode(Candidate))))
        {
            return Candidate;
        }
    }
    return PerfectWorld::DoorResult::NotAvailable;
}

/** Player-facing wording for a command the core refused. Empty means the refusal needs no words (a stale click). */
FText BrRefusalText(const FString& Code)
{
    if (Code == TEXT("time.asleep") || Code == TEXT("conversation.player_asleep"))
    {
        return LOCTEXT("RefuseAsleep", "Not while you are asleep.");
    }
    if (Code == TEXT("time.activity_running") || Code == TEXT("conversation.activity_running"))
    {
        return LOCTEXT("RefuseBusy", "Finish what you are doing first.");
    }
    if (Code == TEXT("world.unsettled") || Code == TEXT("conversation.charge_interrupted"))
    {
        return LOCTEXT("RefuseUnsettled", "Too much is happening at once. Try again in a moment.");
    }
    if (Code == TEXT("conversation.already_open"))
    {
        return LOCTEXT("RefuseAlreadyOpen", "You are already in a conversation.");
    }
    if (Code == TEXT("conversation.stale_choice") || Code == TEXT("conversation.already_charged") || Code == TEXT("conversation.already_taken") ||
        Code == TEXT("conversation.stale_session") || Code == TEXT("conversation.not_open"))
    {
        return FText::GetEmpty();
    }
    return LOCTEXT("RefuseOther", "That did not work.");
}
} // namespace

namespace PWLaneLife
{
FString ToFString(const std::string_view Text)
{
    return FString(UTF8_TO_TCHAR(std::string(Text).c_str()));
}

FText ToText(const std::string_view Text)
{
    return FText::FromString(ToFString(Text));
}

FName ToName(const std::string_view Text)
{
    return FName(*ToFString(Text));
}

std::string ToStd(const FName& Name)
{
    return std::string(TCHAR_TO_UTF8(*Name.ToString()));
}
} // namespace PWLaneLife

UPWLaneLifeSubsystem* UPWLaneLifeSubsystem::Get(const UObject* WorldContext)
{
    const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
    return World != nullptr ? World->GetSubsystem<UPWLaneLifeSubsystem>() : nullptr;
}

bool UPWLaneLifeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    // Only worlds being played; the editor's own worlds have no doors to keep or people to talk to.
    const UWorld* World = Cast<UWorld>(Outer);
    return Super::ShouldCreateSubsystem(Outer) && World != nullptr && World->IsGameWorld();
}

void UPWLaneLifeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    ReportedUnknownDoors.Reset();
    RegisteredDoors.Reset();
    DoorInsideSides.Reset();
    ResolveOriginResidency();
    // Scheduled people (LL-02) are presented by their own subsystem, which must exist before the clock first moves.
    UPWNpcPresenceSubsystem* NpPresence = Collection.InitializeDependency<UPWNpcPresenceSubsystem>();
    BuildWorld();
    if (NpPresence != nullptr)
    {
        NpPresence->Reconcile(GetNow());
    }

    // The conversation panel talks to people through whatever session factory it holds: install ours, over the core.
    if (UPWConversationPresenter* Presenter = Collection.InitializeDependency<UPWConversationPresenter>())
    {
        const TWeakObjectPtr<UPWLaneLifeSubsystem> Weak(this);
        Presenter->SetSessionFactory([Weak](const FName PersonId, const FText& DisplayName) -> TSharedPtr<IPWConversationSession>
        {
            UPWLaneLifeSubsystem* Life = Weak.Get();
            if (Life == nullptr)
            {
                return nullptr;
            }
            return Life->MakeConversationSession(PersonId, DisplayName);
        });
    }
    else
    {
        UE_LOG(LogPWLaneLife, Error, TEXT("No conversation presenter in this world; people cannot be talked to."));
    }

    // Readable objects raise one process-wide event; this world's reading presenter shows the text. Reading is free: it costs
    // no world time (LANE_LIFE_PLAN section B), and the clock holds while the reading panel is open (UpdateClockHold).
    Collection.InitializeDependency<UPWReadingPresenter>();
    APWReadableActor::GlobalReadRequested().RemoveAll(this);
    APWReadableActor::GlobalReadRequested().AddUObject(this, &UPWLaneLifeSubsystem::HandleReadRequested);

    // F5: the household closes an open dwelling leaf behind a visitor who has gone (PerfectWorldLaneLifeKnock.cpp).
    StartDoorKeeper();
}

void UPWLaneLifeSubsystem::Deinitialize()
{
    RemoveClockReadout();
    DoorInsideSides.Reset();
    KnockedDoors.Reset();
    FTSTicker::GetCoreTicker().RemoveTicker(KnockTicker);
    KnockTicker.Reset();
    CancelRings();
    OnClockChanged.Clear();
    APWReadableActor::GlobalReadRequested().RemoveAll(this);
    if (UPWConversationPresenter* Presenter = UPWConversationPresenter::Get(this))
    {
        Presenter->SetSessionFactory(FPWConversationSessionFactory());
    }
    Super::Deinitialize();
}

void UPWLaneLifeSubsystem::HandleReadRequested(const FName ReadableId, const FName ContentId, const FText Title)
{
    // The event is process-wide: only the presenter of the world that owns the readable answers.
    UPWReadingPresenter* Reading = UPWReadingPresenter::Get(this);
    if (Reading == nullptr || !Reading->OpenBuiltIn(ContentId))
    {
        UE_LOG(LogPWLaneLife, Warning, TEXT("Readable %s could not open %s (\"%s\")."), *ReadableId.ToString(), *ContentId.ToString(),
               *Title.ToString());
    }
}

void UPWLaneLifeSubsystem::ResolveOriginResidency()
{
    // Residency is asked of the character creator at the moment a door is used (IsResidentOf), because this subsystem is
    // initialised before the life is chosen, and Start, Load and ResetWorld all change whose home is whose. Nothing cached.
    ResidentLocations.Reset();
}

bool UPWLaneLifeSubsystem::IsResidentOf(const PerfectWorld::DoorDef& Door) const
{
    // The core's resolved origin of the current life decides (LaneOrigin via the creator). No life, or no creator: fail
    // closed, so no private door opens as a resident's.
    const UWorld* World = GetWorld();
    const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
    const UPerfectWorldCharacterCreatorSubsystem* Creator =
        GameInstance != nullptr ? GameInstance->GetSubsystem<UPerfectWorldCharacterCreatorSubsystem>() : nullptr;
    if (Creator == nullptr)
    {
        return false;
    }
    if (Creator->HasLaneOriginResidency(PWLaneLife::ToName(Door.LocationId)))
    {
        return true;
    }
    // F5: every family origin starts in the one built opening-home shell, whose residency the origin table does not list for them
    // (DoorBell.h IsFamilyStartShell); a care origin is not a resident of it.
    const FPWLaneOriginView Origin = Creator->GetLaneOrigin();
    return Origin.bResolved && PerfectWorld::IsFamilyStartShell(Origin.bGrewUpInCare, Door.LocationId);
}

void UPWLaneLifeSubsystem::RegisterDoor(APWDoorActor& Door)
{
    const std::string Id = PWLaneLife::ToStd(Door.GetDoorId());
    const PerfectWorld::DoorDef* Def = PerfectWorld::FindDoor(Id);
    if (Def == nullptr)
    {
        if (!ReportedUnknownDoors.Contains(Door.GetDoorId()))
        {
            ReportedUnknownDoors.Add(Door.GetDoorId());
            UE_LOG(LogPWLaneLife, Error, TEXT("Door %s is not in the core's AuthoredDoors(); it cannot be opened. Add it to Doors.cpp or fix the id."),
                   *Door.GetDoorId().ToString());
        }
        return;
    }
    Door.OnSettled.RemoveAll(this);
    Door.OnSettled.AddUObject(this, &UPWLaneLifeSubsystem::HandleDoorSettled);
    RegisteredDoors.AddUnique(TWeakObjectPtr<APWDoorActor>(&Door));
    // Late registration also uses the adopted world's saved state and swing.
    const PerfectWorld::DoorEntry* Entry = BrFindDoorEntry(Sim->State().Doors, Id);
    Door.SetOpenImmediate(Entry != nullptr ? Entry->Open : Def->bInitiallyOpen, Entry != nullptr ? static_cast<int8>(Entry->SwingSide) : int8(0));
}

void UPWLaneLifeSubsystem::RegisterDoorInsideSide(const APWDoorActor& Door, const double InsideSide)
{
    DoorInsideSides.Add(Door.GetDoorId(), InsideSide >= 0.0 ? 1.0 : -1.0);
}

void UPWLaneLifeSubsystem::HandleDoorSettled(const FName DoorId, const bool bReachedTarget, const bool bIsOpenNow, const int8 SwingSide)
{
    // Record where the leaf really is, including which way a two-way door swung. This is a physical fact, not a
    // permission question, so it goes in as a Presentation-source door.settle (an interrupted swing overrides the provisional
    // state door.set made; a completed one records its side). "No change" is the normal case when the leaf got where it was told.
    PerfectWorld::DoorSettle Settle;
    Settle.DoorId = PWLaneLife::ToStd(DoorId);
    Settle.IsOpenNow = bIsOpenNow;
    Settle.SwingSide = static_cast<std::int8_t>(SwingSide);
    if (!bIsOpenNow)
    {
        KnockedDoors.Remove(DoorId); // a closed door ends the knock's approach
    }
    PerfectWorld::Command Command;
    Command.Source = PerfectWorld::CommandSource::Presentation;
    Command.Payload = Settle;
    const PerfectWorld::CommandResult Result = SubmitCommand(Command);
    if (!Result.Accepted && Result.Why.Code != "door.no_change")
    {
        UE_LOG(LogPWLaneLife, Warning, TEXT("Door %s settled (open %d, side %d, reached %d) but the core refused it: %hs"),
               *DoorId.ToString(), bIsOpenNow ? 1 : 0, static_cast<int32>(SwingSide), bReachedTarget ? 1 : 0, Result.Why.Code.c_str());
    }
}

bool UPWLaneLifeSubsystem::IsDoorOpen(const FName DoorId, const bool bFallback) const
{
    return PerfectWorld::QueryDoorOpen(Sim->State().Doors, PWLaneLife::ToStd(DoorId)).value_or(bFallback);
}

void UPWLaneLifeSubsystem::RequestDoorToggle(APWDoorActor& Door, APawn* By)
{
    const std::string Id = PWLaneLife::ToStd(Door.GetDoorId());
    const PerfectWorld::DoorDef* Def = PerfectWorld::FindDoor(Id);
    const PerfectWorld::DoorEntry* Entry = Def != nullptr ? BrFindDoorEntry(Sim->State().Doors, Id) : nullptr;
    if (Entry == nullptr)
    {
        RegisterDoor(Door); // reports the unknown id once
        ShowDoorRefusal(By, PerfectWorld::DoorResult::UnknownDoor, nullptr);
        return;
    }
    const bool bWasOpen = Entry->Open;
    const std::int8_t WasSide = Entry->SwingSide;

    // Use the authored inside half-plane of the CLOSED frame, never the swinging leaf's current side.
    // Missing actor/geometry remains Entry. The core owns permission; Exit remains available after business hours.
    const double* PbInsideSide = DoorInsideSides.Find(Door.GetDoorId());
    const bool PbIsInside = By != nullptr && PbInsideSide != nullptr &&
        FVector::DotProduct(By->GetActorLocation() - Door.GetActorLocation(), Door.GetActorRightVector()) * *PbInsideSide > 0.0;
    PerfectWorld::DoorSet Set;
    Set.DoorId = Id;
    Set.Open = !bWasOpen;
    Set.Context.Direction = PbIsInside ? PerfectWorld::DoorDirection::Exit : PerfectWorld::DoorDirection::Entry;
    Set.Context.IsResident = IsResidentOf(*Def);
    // NL-04 access bridge. IsInvited stays false until a fact says otherwise: TODO(invitations) set it from the world's invitation
    // facts (a visitor asked to this home by a resident) once such a fact exists; nothing in the core records one yet.
    Set.Context.IsInvited = false;
    Set.Context.IsHostHome = Def->Access == PerfectWorld::DoorAccess::Private && IsHostHome(*Def);
    Set.Context.HasKnocked = KnockedDoors.Contains(Door.GetDoorId());
    PerfectWorld::Command Command;
    Command.Source = PerfectWorld::CommandSource::Player;
    Command.Payload = Set;
    const PerfectWorld::CommandResult Result = SubmitCommand(Command);
    if (!Result.Accepted)
    {
        UE_LOG(LogPWLaneLife, Display, TEXT("Door %s refused by the core: %hs"), *Door.GetDoorId().ToString(), Result.Why.Code.c_str());
        const PerfectWorld::DoorResult Why = BrDoorResultFromCode(PWLaneLife::ToFString(Result.Why.Code));
        if (Why == PerfectWorld::DoorResult::Private && Set.Open && Set.Context.Direction == PerfectWorld::DoorDirection::Entry)
        {
            // F5: the leaf is never knocked on. Whoever has not been let in rings the bell (a room door has none: it is just private).
            ShowPlayerNotice(IsRingPending(Door.GetDoorId()) ? LOCTEXT("WaitForAnswer", "Someone may be coming. Wait a moment.")
                             : PerfectWorld::DoorHasBell(*Def) ? LOCTEXT("RingFirst", "Ring the bell first.")
                                                                : LOCTEXT("PrivateRoom", "That room is private."),
                             By);
            return;
        }
        ShowDoorRefusal(By, Why, Def);
        return;
    }
    if (!Door.TryMoveTo(Set.Open, By))
    {
        // Allowed by the core, but a person or a piece of furniture is in the leaf's way: put the world back as it was.
        PerfectWorld::DoorSettle Revert;
        Revert.DoorId = Id;
        Revert.IsOpenNow = bWasOpen;
        Revert.SwingSide = WasSide;
        PerfectWorld::Command Undo;
        Undo.Source = PerfectWorld::CommandSource::Presentation;
        Undo.Payload = Revert;
        const PerfectWorld::CommandResult Undone = SubmitCommand(Undo);
        if (!Undone.Accepted)
        {
            UE_LOG(LogPWLaneLife, Warning, TEXT("Door %s could not swing and the core would not take it back: %hs"),
                   *Door.GetDoorId().ToString(), Undone.Why.Code.c_str());
        }
        ShowPlayerNotice(LOCTEXT("InTheWay", "Something is in the way."), By);
    }
    // Otherwise the state is provisional: HandleDoorSettled corrects it if the swing is interrupted.
}

void UPWLaneLifeSubsystem::ShowPlayerNotice(const FText& Message, const APawn* By) const
{
    if (Message.IsEmpty())
    {
        return;
    }
    const APawn* Target = By != nullptr ? By : UGameplayStatics::GetPlayerPawn(this, 0);
    if (UPWInteractionComponent* Interaction = Target != nullptr ? Target->FindComponentByClass<UPWInteractionComponent>() : nullptr)
    {
        Interaction->ShowNotice(Message);
    }
}

void UPWLaneLifeSubsystem::ShowDoorRefusal(const APawn* By, const PerfectWorld::DoorResult Result, const PerfectWorld::DoorDef* Def) const
{
    // The core gives the reason as a key (DoorResultReasonKey); the wording lives here.
    const std::string_view Key = PerfectWorld::DoorResultReasonKey(Result);
    FText Message;
    if (Key == "door.reason.private")
    {
        Message = LOCTEXT("ReasonPrivate", "It's someone's home. Ring the bell first.");
    }
    else if (Key == "door.reason.locked")
    {
        Message = LOCTEXT("ReasonLocked", "It's locked.");
    }
    else if (Key == "door.reason.closed_hours")
    {
        const int32 Opens = Def != nullptr ? PerfectWorld::DoorOpensAtMinute(*Def) : -1;
        Message = Opens >= 0 ? FText::Format(LOCTEXT("ReasonClosedUntil", "Closed until {0}."),
                                             FText::FromString(FString::Printf(TEXT("%02d:%02d"), Opens / 60, Opens % 60)))
                             : LOCTEXT("ReasonClosed", "It's closed.");
    }
    else
    {
        Message = LOCTEXT("ReasonOther", "It won't open.");
    }
    ShowPlayerNotice(Message, By);
}

TSharedPtr<IPWConversationSession> UPWLaneLifeSubsystem::MakeConversationSession(const FName PersonId, const FText& DisplayName)
{
    PerfectWorld::PersonAvailability Availability = PerfectWorld::PersonAvailability::Awake;
    if (UPWNpcPresenceSubsystem* NpPresence = GetWorld()->GetSubsystem<UPWNpcPresenceSubsystem>())
    {
        // A person on the presenter talks as their schedule says now; one with no valid intent refuses the session. Static
        // people (the household adult) keep the Awake default.
        NpPresence->Reconcile(GetNow());
        if (NpPresence->IsRegistered(PersonId))
        {
            const PerfectWorld::NpcIntent* NpIntent = NpPresence->FindIntent(PersonId);
            if (NpIntent == nullptr)
            {
                return nullptr;
            }
            Availability = NpIntent->Availability;
        }
    }
    // A session abandoned without a Leave (a panel that failed to open after the core opened it, a world torn down mid-talk)
    // would refuse this one and keep the clock still: close it so a new one can start.
    const std::string_view StillOpen = PerfectWorld::OpenConversationPersonId(Sim->State());
    if (!StillOpen.empty())
    {
        UE_LOG(LogPWLaneLife, Warning, TEXT("Closing the abandoned conversation with %s."), *PWLaneLife::ToFString(StillOpen));
        LeaveConversation(PWLaneLife::ToName(StillOpen));
    }
    SettleUntilQuiescent();

    PerfectWorld::ConversationOpen Open;
    Open.PersonId = PWLaneLife::ToStd(PersonId);
    Open.Availability = Availability;
    PerfectWorld::Command Command;
    Command.Source = PerfectWorld::CommandSource::Player;
    Command.Payload = Open;
    const PerfectWorld::CommandResult Result = SubmitCommand(Command);
    if (!Result.Accepted)
    {
        UE_LOG(LogPWLaneLife, Warning, TEXT("Cannot talk to %s: the core refused (%hs)."), *PersonId.ToString(), Result.Why.Code.c_str());
        ShowPlayerNotice(BrRefusalText(PWLaneLife::ToFString(Result.Why.Code)));
        return nullptr;
    }
    const PerfectWorld::ConversationState* State = FindConversation(PersonId);
    const uint32 Session = State != nullptr ? State->SessionCount : 0;

    // The household adult is known to the core by a role label; any other person keeps the name the actor carries.
    const bool bHouseholdAdult = PersonId == PWLaneLife::ToName(PerfectWorld::HouseholdAdultId);
    const FText Speaker = bHouseholdAdult ? PWLaneLife::ToText(PerfectWorld::HouseholdAdultLabel) : DisplayName;
    return MakeShared<FPWCoreConversationSession>(TWeakObjectPtr<UPWLaneLifeSubsystem>(this), PersonId, Speaker, Session);
}

const PerfectWorld::ConversationState* UPWLaneLifeSubsystem::FindConversation(const FName PersonId) const
{
    const auto Found = Sim->State().Conversations.find(PWLaneLife::ToStd(PersonId));
    return Found != Sim->State().Conversations.end() ? &Found->second : nullptr;
}

FPWLaneCommandOutcome UPWLaneLifeSubsystem::ChooseConversation(const FName PersonId, const FName ChoiceId, const uint32 Session)
{
    PerfectWorld::ConversationChoose Choose;
    Choose.PersonId = PWLaneLife::ToStd(PersonId);
    Choose.ChoiceId = PWLaneLife::ToStd(ChoiceId);
    Choose.Session = Session;
    PerfectWorld::Command Command;
    Command.Source = PerfectWorld::CommandSource::Player;
    Command.Payload = Choose;

    SettleUntilQuiescent();
    PerfectWorld::CommandResult Result = SubmitCommand(Command);
    if (!Result.Accepted && Result.Why.Code == "conversation.charge_interrupted")
    {
        // The event cap stopped the paid choice's charge short and the core rolled it back: settle and choose again, once.
        SettleUntilQuiescent();
        Result = SubmitCommand(Command);
    }

    FPWLaneCommandOutcome Outcome;
    Outcome.bAccepted = Result.Accepted;
    if (!Result.Accepted)
    {
        Outcome.Code = PWLaneLife::ToFString(Result.Why.Code);
        Outcome.Reason = BrRefusalText(Outcome.Code);
        UE_LOG(LogPWLaneLife, Display, TEXT("Choice %s with %s refused by the core: %s."), *ChoiceId.ToString(), *PersonId.ToString(), *Outcome.Code);
    }
    return Outcome;
}

void UPWLaneLifeSubsystem::LeaveConversation(const FName PersonId)
{
    const PerfectWorld::ConversationState* State = FindConversation(PersonId);
    if (State == nullptr || !State->Open)
    {
        return;
    }
    PerfectWorld::ConversationLeave Leave;
    Leave.PersonId = PWLaneLife::ToStd(PersonId);
    PerfectWorld::Command Command;
    Command.Source = PerfectWorld::CommandSource::Player;
    Command.Payload = Leave;
    const PerfectWorld::CommandResult Result = SubmitCommand(Command);
    if (!Result.Accepted)
    {
        UE_LOG(LogPWLaneLife, Warning, TEXT("Could not leave the conversation with %s: %hs"), *PersonId.ToString(), Result.Why.Code.c_str());
    }
}

#undef LOCTEXT_NAMESPACE
