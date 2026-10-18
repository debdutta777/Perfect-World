// NL-04: the access bridge of UPWLaneLifeSubsystem for Private dwellings. The core decides (Doors.h: EvaluateDoorKnock and the
// Private branch of EvaluateDoorPassage); this file only supplies the two facts it asks of the caller -- is a resident of the
// home actually there, and did this visitor knock on this approach -- and says the answer in words.

#include "PerfectWorldLaneLifeSubsystem.h"

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

FString KnkClockText(const int32 MinuteOfDay)
{
    const int32 Wrapped = ((MinuteOfDay % 1440) + 1440) % 1440;
    return FString::Printf(TEXT("%02d:%02d"), Wrapped / 60, Wrapped % 60);
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
        Message = LOCTEXT("KnockAdmitted", "Someone answers. You may come in.");
        KnockedDoors.Add(Door.GetDoorId(), &Door); // the next use of the door on this approach opens it
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

#undef LOCTEXT_NAMESPACE
