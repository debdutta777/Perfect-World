#include "PerfectWorldAcceptanceHomes.h"

#include "PerfectWorldAcceptanceDoorBell.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameClock.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NpcBehaviour.h"
#include "NpcScheduleContent.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldContentSubsystem.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneInteriorKit.h"
#include "PerfectWorldLaneKitFrontages.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldSubrataBindings.h"
#include "Misc/FileHelper.h"
#include "WorldSave.h"

// resident_homes (NR-03): the clock, door, walk and knock steps. See PerfectWorldAcceptanceHomes.cpp for the scenario.

namespace PWAcceptance::Homes
{
namespace
{
const TCHAR* HmAnswerText(const EHmAnswer Answer)
{
    switch (Answer)
    {
    case EHmAnswer::Admitted: return TEXT("admitted");
    case EHmAnswer::NoAnswer: return TEXT("no answer");
    case EHmAnswer::NotReceiving: return TEXT("not receiving");
    default: return TEXT("none");
    }
}

EHmAnswer HmClassify(const FText& Notice)
{
    const FString Text = Notice.ToString();
    if (PWAcceptance::DoorBell::IsAnsweredNotice(Text)) { return EHmAnswer::Admitted; } // F5: "Someone is coming..." (the ring's answer)
    if (Text.Contains(TEXT("Nobody answers"))) { return EHmAnswer::NoAnswer; }
    if (Text.Contains(TEXT("receiving visitors"))) { return EHmAnswer::NotReceiving; }
    return EHmAnswer::None;
}

}

FHmLedger& HmLedger()
{
    static FHmLedger Ledger;
    return Ledger;
}

const PerfectWorld::ContentDb* HmDb(UWorld* World)
{
    const UGameInstance* Instance = World != nullptr ? World->GetGameInstance() : nullptr;
    const UPWContentSubsystem* Content = Instance != nullptr ? Instance->GetSubsystem<UPWContentSubsystem>() : nullptr;
    return Content != nullptr ? Content->GetDb() : nullptr;
}

TArray<PerfectWorld::NpcSchedule> HmResidentsOf(UWorld* World, const PerfectWorld::DoorDef& Def)
{
    TArray<PerfectWorld::NpcSchedule> Out;
    const PerfectWorld::ContentDb* Db = HmDb(World);
    if (Db == nullptr) { return Out; }
    for (const PerfectWorld::ContentRecord* Record : Db->OfType("npc_schedule"))
    {
        const PerfectWorld::NpcScheduleResult Loaded = PerfectWorld::LoadNpcSchedule(*Db, Record->Id);
        if (Loaded.Ok() && Loaded.Value->Person.HomeZone == Def.LocationId) { Out.Add(*Loaded.Value); }
    }
    return Out;
}

PerfectWorld::GameInstant HmInstant(const int32 Day, const int32 Hour, const int32 Minute)
{
    return PerfectWorld::GameClock::FromCalendar(PerfectWorld::CampaignStartYear, PerfectWorld::CampaignStartMonth,
                                                 PerfectWorld::CampaignStartDay + Day, Hour, Minute);
}

FName HmName(const std::string& Id) { return FName(UTF8_TO_TCHAR(Id.c_str())); }

/** The schedule's own answer: is this resident at the home zone, idle or at work, at this instant? */
bool HmHomeBySchedule(const PerfectWorld::NpcSchedule& Schedule, const PerfectWorld::DoorDef& Def, const PerfectWorld::GameInstant At)
{
    const auto Intent = PerfectWorld::ResolveNpcIntentAt(Schedule.Person, Schedule.Travel, At, false, false);
    return Intent.Ok() && Intent.Intent.AnchorId == Def.LocationId &&
           (Intent.Intent.State == PerfectWorld::NpcState::Idle || Intent.Intent.State == PerfectWorld::NpcState::Work);
}

bool HmAnyHomeBySchedule(UWorld* World, const PerfectWorld::DoorDef& Def, const PerfectWorld::GameInstant At)
{
    for (const PerfectWorld::NpcSchedule& Schedule : HmResidentsOf(World, Def))
    {
        if (HmHomeBySchedule(Schedule, Def, At)) { return true; }
    }
    return false;
}

APWDoorActor* HmFindDoor(UWorld* World, const FName DoorId)
{
    if (World == nullptr) { return nullptr; }
    for (TActorIterator<APWDoorActor> It(World); It; ++It)
    {
        if (It->GetDoorId() == DoorId) { return *It; }
    }
    return nullptr;
}

/** The visitor's outside spot (and the point inside) of a kit door, from the portable layout and the door's own spec. */
bool HmDoorSpot(const TCHAR* DoorId, const bool bInside, FVector& Out, FString& Why)
{
    PWLane::FLaneBuilding Building;
    PWLane::FLaneKitFrontage Kit;
    if (!PWLane::MakeSubrataBuilding(PWLane::SubrataTallFlatsId, Building) || !PWLane::IsKitFrontage(Building, Kit)) { Why = TEXT("tall flats row missing"); return false; }
    const PWLane::LI::InteriorLayout Layout = PWLane::MakeKitFrontageLayout(Building, Kit);
    const std::string Id(TCHAR_TO_UTF8(DoorId));
    for (const PWLane::LI::Opening& Opening : Layout.Openings)
    {
        if (!Layout.Ok() || Opening.DoorId != Id) { continue; }
        const PWLane::FLaneDoorSpec Spec = PWLane::KitDoorSpec(Building, Layout, Opening);
        const FVector Dir = FRotator(0.0, Spec.ClosedYaw, 0.0).Vector();
        const FVector Into = FVector(-Dir.Y, Dir.X, 0.0) * Spec.PreferredSwing;
        const FVector Centre = Spec.Hinge + Dir * HmLeafHalfCm;
        Out = bInside ? Centre + Into * 170.0 : Centre - Into * HmReachSpotCm;
        return true;
    }
    Why = FString::Printf(TEXT("the geometry of %s does not resolve from the kit layout"), DoorId);
    return false;
}

TUniquePtr<PerfectWorld::WorldSim> HmAdvancedCopy(const PerfectWorld::WorldState& Live, const PerfectWorld::GameInstant Target, FString& Why)
{
    auto Sim = MakeUnique<PerfectWorld::WorldSim>(Live);
    const auto Settle = [&Sim]
    {
        for (int32 Round = 0; Round < 64 && !PerfectWorld::IsWorldQuiescent(Sim->State()); ++Round)
        {
            PerfectWorld::Command Command;
            Command.Source = PerfectWorld::CommandSource::System;
            Command.Payload = PerfectWorld::SettleDueEvents{};
            if (!Sim->Submit(Command).Accepted) { return; }
        }
    };
    Settle();
    for (int32 Round = 0; Round < 64 && Sim->State().Clock.Now() < Target; ++Round)
    {
        PerfectWorld::AdvanceTime Advance;
        Advance.Minutes = Target - Sim->State().Clock.Now();
        PerfectWorld::Command Command;
        Command.Source = PerfectWorld::CommandSource::System;
        Command.Payload = Advance;
        if (!Sim->Submit(Command).Accepted) { Why = TEXT("the core refused time.advance"); return nullptr; }
        Settle();
    }
    if (Sim->State().Clock.Now() != Target) { Why = TEXT("the advance did not reach the target instant"); return nullptr; }
    return Sim;
}

FStepDef HmClockStep(const FString& Name, const int32 Day, const int32 Hour, const int32 Minute)
{
    FStepDef Def;
    Def.Name = Name;
    Def.TimeoutSeconds = 10.0;
    Def.Run = [Day, Hour, Minute](UPWAcceptanceSubsystem& Sub, FStepRuntime&) -> FStepOutcome
    {
        UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        const APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
        if (Life == nullptr || Pawn == nullptr) { return FStepOutcome::Fail(TEXT("no lane-life subsystem or no pawn")); }
        if (Pawn->IsSeated() || !PerfectWorld::OpenConversationPersonId(Life->GetWorldState()).empty() || Life->HasMovingDoors())
        {
            return FStepOutcome::Fail(TEXT("the world is not at rest, so the clock is not moved"));
        }
        const PerfectWorld::GameInstant Target = HmInstant(Day, Hour, Minute);
        const FString Before = Life->GetClockText().ToString();
        if (Life->GetNow() >= Target) { return FStepOutcome::Fail(FString::Printf(TEXT("the clock already stands at %s, at or after the target"), *Before)); }
        FString Why;
        TUniquePtr<PerfectWorld::WorldSim> Moved = HmAdvancedCopy(Life->GetWorldState(), Target, Why);
        if (!Moved.IsValid()) { return FStepOutcome::Fail(Why); }
        Life->AdoptSavedWorld(MoveTemp(Moved));
        if (Life->GetNow() != Target) { return FStepOutcome::Fail(TEXT("the clock is not at the target after adopting the moved world")); }
        if (UPWNpcPresenceSubsystem* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>()) { Presence->Reconcile(Life->GetNow()); }
        return FStepOutcome::Pass(FString::Printf(TEXT("clock moved from %s to %s"), *Before, *Life->GetClockText().ToString()));
    };
    return Def;
}

FStepDef HmWalkToDoor(const FString& Name, const TCHAR* DoorId, const bool bInside)
{
    FStepDef Def;
    Def.Name = Name;
    Def.TimeoutSeconds = 70.0;
    Def.Run = [DoorId, bInside](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        if (!Rt.bHaveTarget)
        {
            FString Why;
            if (!HmDoorSpot(DoorId, bInside, Rt.Target, Why)) { return FStepOutcome::Fail(Why); }
            Rt.bHaveTarget = true;
        }
        return Sub.TickWalk(Rt.Target, HmWalkTolerance, Rt);
    };
    return Def;
}

FStepDef HmStartStep()
{
    FStepDef Def;
    Def.Name = TEXT("declared_start_outside_tall_flats");
    Def.TimeoutSeconds = 15.0;
    Def.Run = [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
        if (Pawn == nullptr) { return FStepOutcome::Fail(TEXT("no pawn")); }
        if (Rt.Phase == 0)
        {
            FVector Outside;
            FString Why;
            if (!HmDoorSpot(HmFront, false, Outside, Why)) { return FStepOutcome::Fail(Why); }
            HmLedger() = FHmLedger();
            // The one teleport of this scenario: its declared start on the footpath facing the public front door. Every later move is walked.
            Pawn->TeleportTo(Outside + FVector(0.0, 0.0, PWPlayer::CapsuleHalfHeight + 5.0), FRotator(0.0, 90.0, 0.0));
            Pawn->GetCharacterMovement()->Velocity = FVector::ZeroVector;
            Sub.FaceWorldPoint(Outside + FVector(0.0, 300.0, 0.0));
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        if (Rt.InPhase() < 0.8 || !Pawn->GetCharacterMovement()->IsMovingOnGround()) { return FStepOutcome::Wait(); }
        return FStepOutcome::Pass(TEXT("declared start in front of the tall flats' public entry; standing on the ground"));
    };
    return Def;
}

FStepDef HmDoorStep(const FString& Name, const TCHAR* DoorId, const bool bWantOpen, const TCHAR* Say, TFunction<bool()> ExtraSkip)
{
    const FName Id(DoorId);
    const FString Text(Say);
    return PWAcceptance::MakeInteractStep(Name, [Id](UPWAcceptanceSubsystem& Sub) -> AActor* { return HmFindDoor(Sub.GetPlayWorld(), Id); },
        [Id, bWantOpen, Text](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const APWDoorActor* DoorActor = HmFindDoor(Sub.GetPlayWorld(), Id);
            if (DoorActor == nullptr || DoorActor->IsMoving() || DoorActor->IsOpen() != bWantOpen) { return false; }
            Detail = Text;
            return true;
        },
        [Id, bWantOpen, ExtraSkip](UPWAcceptanceSubsystem& Sub)
        {
            if (ExtraSkip && ExtraSkip()) { return true; }
            const APWDoorActor* DoorActor = HmFindDoor(Sub.GetPlayWorld(), Id);
            return DoorActor != nullptr && !DoorActor->IsMoving() && DoorActor->IsOpen() == bWantOpen;
        },
        15.0);
}

/**
 * E on the BELL (F5): bFirst records and judges the answer against the schedules; the second press is E on the DOOR for an admitted
 * visitor (it opens the unlatched door), and a second ring for anyone else (the same answer, the door stays shut).
 */
FStepDef HmKnockStep(const FString& Name, const TCHAR* DoorId, const bool bFirst)
{
    const FName Id(DoorId);
    return PWAcceptance::MakeInteractStep(Name,
        [Id, bFirst](UPWAcceptanceSubsystem& Sub) -> AActor*
        {
            if (!bFirst && HmLedger().LastObserved == EHmAnswer::Admitted) { return HmFindDoor(Sub.GetPlayWorld(), Id); }
            return PWAcceptance::DoorBell::FindBell(Sub.GetPlayWorld(), Id);
        },
        [Id, bFirst](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            FHmLedger& Ledger = HmLedger();
            UWorld* World = Sub.GetPlayWorld();
            UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(World);
            const UPWInteractionComponent* Interaction = Sub.GetInteraction();
            const APWDoorActor* DoorActor = HmFindDoor(World, Id);
            const PerfectWorld::DoorDef* Def = PerfectWorld::FindDoor(std::string(TCHAR_TO_UTF8(*Id.ToString())));
            if (Interaction == nullptr || DoorActor == nullptr || Life == nullptr || Def == nullptr) { return false; }
            if (bFirst)
            {
                const EHmAnswer Observed = HmClassify(Interaction->GetNotice());
                if (Observed == EHmAnswer::None) { return false; }
                const int32 Minute = PerfectWorld::DoorMinuteOfDay(Life->GetNow());
                const bool bHome = HmAnyHomeBySchedule(World, *Def, Life->GetNow());
                const EHmAnswer Expected = !bHome ? EHmAnswer::NoAnswer
                    : (PerfectWorld::DoorMinutesUntilVisiting(*Def, Minute) == 0 ? EHmAnswer::Admitted : EHmAnswer::NotReceiving);
                Ledger.LastObserved = Observed;
                ++Ledger.Probes;
                const bool bMatch = Observed == Expected && Life->IsHostHomeForAcceptance(*Def) == bHome;
                Detail = FString::Printf(TEXT("%s at %02d:%02d: host home by schedule %d, expected %s, observed %s%s"), *Id.ToString(), Minute / 60,
                                         Minute % 60, bHome, HmAnswerText(Expected), HmAnswerText(Observed), bMatch ? TEXT("") : TEXT(" MISMATCH"));
                if (!bMatch) { Ledger.Mismatches.Add(Detail); }
                return true;
            }
            if (Ledger.LastObserved == EHmAnswer::Admitted)
            {
                if (!DoorActor->IsOpen()) { return false; }
                Detail = FString::Printf(TEXT("%s is open: the visitor whose knock was answered was let in"), *Id.ToString());
                return true;
            }
            if (HmClassify(Interaction->GetNotice()) != Ledger.LastObserved) { return false; }
            Detail = FString::Printf(TEXT("%s knocked again: same answer (%s), door %s"), *Id.ToString(), HmAnswerText(Ledger.LastObserved),
                                     DoorActor->IsOpen() ? TEXT("OPEN") : TEXT("still shut"));
            if (DoorActor->IsOpen()) { Ledger.Mismatches.Add(Detail); }
            return true;
        },
        nullptr, 15.0);
}

FStepDef HmWaitStep(const FString& Name, const double Seconds, TFunction<bool()> Skip)
{
    FStepDef Def;
    Def.Name = Name;
    Def.TimeoutSeconds = Seconds + 5.0;
    Def.Run = [Seconds, Skip](UPWAcceptanceSubsystem&, FStepRuntime& Rt) -> FStepOutcome
    {
        if (Skip && Skip()) { return FStepOutcome::Pass(TEXT("nothing to wait for")); }
        return Rt.Elapsed < Seconds ? FStepOutcome::Wait() : FStepOutcome::Pass(TEXT("the last notice has expired"));
    };
    return Def;
}

void HmAddKnock(TArray<FStepDef>& Out, const FString& Tag)
{
    const auto Admitted = [] { return HmLedger().LastObserved == EHmAnswer::Admitted; };
    Out.Add(HmKnockStep(Tag + TEXT("_knock"), HmUnitB, true));
    Out.Add(PWAcceptance::MakeShotStep(Tag + TEXT("_shot_answer")));
    Out.Add(HmWaitStep(Tag + TEXT("_notice_expires"), 3.0, Admitted));
    Out.Add(HmKnockStep(Tag + TEXT("_knock_again"), HmUnitB, false));
}

}
