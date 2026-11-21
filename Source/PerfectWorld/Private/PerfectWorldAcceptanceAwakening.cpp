#include "PerfectWorldAcceptanceAwakening.h"

#include "Engine/World.h"
#include "PerfectWorldAcceptanceAwakeningFloorSteps.h"
#include "PerfectWorldAwakeningResultBridge.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldAcceptanceSchoolR7Steps.h"
#include "PerfectWorldSchoolPresenter.h"
#include "UI/SPWAwakeningResultPanel.h"
#include "UI/SPWCountdownPanel.h"

namespace
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;
using PWAcceptance::AwakeningFloor::AwFlFindFloor;

FStepDef P38Action(const TCHAR* Name, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run)
{
    FStepDef Step;
    Step.Name = Name;
    Step.TimeoutSeconds = 30.0;
    Step.Run = MoveTemp(Run);
    return Step;
}

bool P310PawnInFloor(UPWAcceptanceSubsystem& Sub, const APWAwakeningFloorActor& Floor)
{
    const auto* Pawn = Sub.GetPawn();
    return Pawn != nullptr && Floor.GetLayout().Bounds.IsInside(Pawn->GetActorLocation());
}

// P310: use the unobstructed X=300 aisle in the room's local frame, away from every hazard and mover pose.
FStepDef P310FloorWalk(const FString& Name, const int32 Station)
{
    FStepDef Step;
    Step.Name = Name;
    Step.TimeoutSeconds = 60.0;
    Step.Run = [Station](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        const auto* Floor = AwFlFindFloor(Sub);
        if (Floor == nullptr || !Floor->IsOccupied() || !P310PawnInFloor(Sub, *Floor))
            return FStepOutcome::Fail(TEXT("the pawn is outside the occupied tutorial room"));
        const auto& L = Floor->GetLayout();
        TArray<FVector> Points;
        if (Station < 0)
        {
            Points.Add(PWAwakening::AwWorld(L, PWAwakening::AwNumbers::StandX, 400.0, 0.0));
            Points.Add(L.LeavePad.Centre - FVector(0.0, 70.0, 0.0));
        }
        else
        {
            if (!L.Stations.IsValidIndex(Station)) return FStepOutcome::Fail(TEXT("no such tutorial station"));
            if (Station == 0)
                Points.Add(PWAwakening::AwWorld(L, PWAwakening::AwNumbers::StandX, 260.0, 0.0));
            Points.Add(L.Stations[Station].Stand);
        }
        const bool bLast = Rt.Phase == Points.Num() - 1;
        const auto Outcome = Sub.TickWalk(Points[Rt.Phase], 20.0, Rt);
        if (Outcome.State == PWAcceptance::EStepState::Passed && !bLast)
        {
            Rt.NextPhase();
            Rt.LastProbe = 0.0;
            Rt.StuckSince = 0.0;
            return FStepOutcome::Wait();
        }
        if (Outcome.State == PWAcceptance::EStepState::Passed && Station < 0)
            Sub.FaceWorldPoint(L.LeavePad.Point);
        return Outcome;
    };
    return Step;
}

bool P38AdvanceTo(UPWLaneLifeSubsystem& Life, const PerfectWorld::GameInstant Target, FString& Why)
{
    // Use the bound live sim, never ScAdvanceLaneClock's unbound copy. Every partial/settlement must actually succeed.
    for (int32 Round = 0; Round < 128; ++Round)
    {
        const auto& State = Life.GetWorldState();
        if (Life.GetNow() > Target) { Why = TEXT("the clock is already past the requested instant"); return false; }
        if (!PerfectWorld::IsWorldQuiescent(State))
        {
            const auto Done = Life.SubmitAwakeningCommand({PerfectWorld::CommandSource::System, PerfectWorld::SettleDueEvents{}});
            if (!Done.Accepted) { Why = PWLaneLife::ToFString(Done.Why.Code); return false; }
            continue;
        }
        if (Life.GetNow() == Target) return true;
        const auto Done = Life.SubmitAwakeningCommand({PerfectWorld::CommandSource::Player, PerfectWorld::AdvanceTime{Target - Life.GetNow()}});
        if (!Done.Accepted) { Why = PWLaneLife::ToFString(Done.Why.Code) + TEXT(": ") + PWLaneLife::ToFString(Done.Why.Message); return false; }
    }
    Why = TEXT("the live clock exhausted the bounded continuation budget");
    return false;
}

// Keep MakeInteractStep's real E-key path, but report a core refusal immediately with its stable code.
FStepDef P38PadStep(const FString& Name, const int32 Station, const int32 PadIndex)
{
    auto Step = PWAcceptance::MakeInteractStep(Name,
        [Station, PadIndex](UPWAcceptanceSubsystem& Sub) -> AActor*
        {
            const auto* Floor = AwFlFindFloor(Sub);
            if (Floor == nullptr) return nullptr;
            return Floor->FindPad(Station < 0 ? Floor->GetLayout().LeavePad.Id : Floor->GetLayout().Stations[Station].Pads[PadIndex].Id);
        },
        [Station](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const auto* Floor = AwFlFindFloor(Sub);
            const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
            if (Floor == nullptr || Life == nullptr || Floor->IsTransitioning()) return false;
            if (Station < 0)
            {
                const auto* Pawn = Sub.GetPawn();
                if (Pawn == nullptr) return false;
                const FVector Here = Pawn->GetActorLocation();
                const FVector Stand = Floor->GetLayout().Site.EntryStand;
                Detail = FString::Printf(TEXT("return pad placed the pawn at (%.0f, %.0f, %.0f); campus stand (%.0f, %.0f, %.0f)"),
                    Here.X, Here.Y, Here.Z, Stand.X, Stand.Y, Stand.Z);
                return !Floor->IsOccupied() && FVector::Dist2D(Here, Stand) < 40.0 &&
                    FMath::Abs(Here.Z - Stand.Z - PWPlayer::CapsuleHalfHeight) < 10.0;
            }
            const auto& Tutorial = Life->GetWorldState().Tutorial;
            Detail = TEXT("the completed contextual pad action advanced the authoritative tutorial");
            return Tutorial.NextSituation == Station + 1 && Floor->GetPhase(Station) == PWAwakening::EAwPhase::Complete;
        }, nullptr, 25.0);
    auto Run = MoveTemp(Step.Run);
    Step.Run = [Run = MoveTemp(Run)](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        const auto Outcome = Run(Sub, Rt);
        const auto* Floor = AwFlFindFloor(Sub);
        if (Rt.Phase > 0 && Floor != nullptr && !Floor->GetLastCode().IsEmpty())
            return FStepOutcome::Fail(TEXT("the core refused the pad action: ") + Floor->GetLastCode());
        return Outcome;
    };
    return Step;
}
} // namespace

namespace PWAcceptance
{
void P38AppendAwakeningSteps(TArray<FStepDef>& Out)
{
    using namespace AwakeningFloor;
    // P310: the same home door and twelve plan waypoints used by school_lesson_round9, then the campus forecourt.
    SchoolR7::ScAWalkToGate(Out);
    Out.Add(P38Action(TEXT("live_seed_and_bindings"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        const auto* Floor = AwFlFindFloor(Sub);
        if (Life == nullptr || Floor == nullptr) return FStepOutcome::Fail(TEXT("no live sim or spawned floor"));
        const auto& Sim = Life->GetSim();
        if (!Sim.State().Awakening.Seeded || Sim.State().Awakening.Granted || Sim.AwakeningContent().Recipes == nullptr ||
            Sim.AwakeningContent().Countdown.Player.empty() || Sim.AwakeningContent().Countdown.Contacts.empty())
            return FStepOutcome::Fail(TEXT("requires a newly seeded life, recipes and the live countdown contacts"));
        if (!PWAwakening::AwBridge::Resolve(Sub.GetPlayWorld()).IsValid()) return FStepOutcome::Fail(TEXT("no live tutorial bridge"));
        return FStepOutcome::Pass(TEXT("creation seeded the real world; recipes and authored countdown contacts bound"));
    }));
    const auto Campus = PWSchool::ScReadCampus();
    const auto Site = PWAwakening::AwSiteFromCampus();
    TArray<FVector> ToTerminal;
    ToTerminal.Add(FVector(Campus.GateX, Campus.Site.Y0 + 150.0, PWSchool::ScGroundZ));
    const double CrossY = Campus.Forecourt.Y0 + 550.0;
    ToTerminal.Add(FVector(Campus.GateX, CrossY, PWSchool::ScGroundZ));
    ToTerminal.Add(FVector(Site.EntryStand.X, CrossY, PWSchool::ScGroundZ));
    ToTerminal.Add(Site.EntryStand);
    const FVector TerminalPoint = Site.EntryKiosk + FVector(0.0, -2.0, 85.0);
    // Cross south of the flag plinth (Y 5600..5800), then north at X 7300; never cut through a verandah pillar.
    Out.Add(SchoolR7::ScAPointsWalk(TEXT("walk_school_gate_to_forecourt_terminal"), ToTerminal, 60.0, &TerminalPoint));
    Out.Add(P38Action(TEXT("clock_to_awakening_day_and_enter_floor"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        auto* Floor = AwFlFindFloor(Sub);
        auto* Interaction = Sub.GetInteraction();
        if (Life == nullptr || Floor == nullptr || Interaction == nullptr) return FStepOutcome::Fail(TEXT("no live entry context"));
        if (Rt.Phase == 0)
        {
            const auto* Pad = Floor->FindPad(Floor->GetLayout().EntryPad.Id);
            if (Pad == nullptr) return FStepOutcome::Fail(TEXT("no campus terminal pad"));
            Sub.FaceWorldPoint(Pad->GetInteractionPoint());
            if (Interaction->GetTarget() != Pad || !Interaction->IsPromptVisible()) return FStepOutcome::Wait();
            if (UPWInteractionComponent::IsWorldInputBlocked(Sub.GetPawn())) return FStepOutcome::Fail(TEXT("world input blocked before entry"));
            FString Why;
            // Advance and press E within the same callback: no free-roam tick may overshoot the exact appointment.
            if (!P38AdvanceTo(*Life, Life->GetWorldState().Awakening.AppointmentAt, Why)) return FStepOutcome::Fail(Why);
            if (!Interaction->TryInteract() || !Floor->GetLastCode().IsEmpty())
                return FStepOutcome::Fail(TEXT("tutorial entry refused: ") + Floor->GetLastCode());
            if (Life->GetWorldState().Tutorial.Phase != PerfectWorld::TutorialDungeonPhase::Active)
                return FStepOutcome::Fail(TEXT("the core holds no active tutorial after entering"));
            Rt.NextPhase();
        }
        if (Floor->IsTransitioning()) return FStepOutcome::Wait();
        if (!Floor->IsOccupied() || !P310PawnInFloor(Sub, *Floor))
            return FStepOutcome::Fail(TEXT("entry completed without the pawn inside the tutorial room"));
        if (Rt.Phase == 1) { Rt.NextPhase(); return FStepOutcome::Wait(); }
        // Occupied alone used to pass even when fall recovery had already returned the pawn home.
        if (Rt.InPhase() < 0.5) return FStepOutcome::Wait();
        const FVector Here = Sub.GetPawn()->GetActorLocation();
        const FVector Arrival = Floor->GetLayout().Arrival;
        if (FVector::Dist2D(Here, Arrival) > 20.0 || FMath::Abs(Here.Z - Arrival.Z - PWPlayer::CapsuleHalfHeight) > 10.0)
            return FStepOutcome::Fail(TEXT("the pawn did not remain at the authored arrival after entry"));
        return FStepOutcome::Pass(FString::Printf(TEXT("terminal submitted tutorial.enter; pawn remained at (%.0f, %.0f, %.0f), floor Z %.0f"),
            Here.X, Here.Y, Here.Z, Arrival.Z));
    }));
    Out.Add(MakeShotStep(TEXT("shot_live_floor_arrival")));
    for (int32 Station = 0; Station < PWAwakening::SituationCount; ++Station)
    {
        const FString Slug = FString::Printf(TEXT("situation_%d"), Station + 1);
        Out.Add(P310FloorWalk(TEXT("walk_to_") + Slug, Station));
        // Help a classmate, shield the group, then arrange supervised handover. No combat input.
        Out.Add(P38PadStep(TEXT("non_combat_") + Slug, Station, Station == 2 ? 2 : 0));
    }
    Out.Add(P38Action(TEXT("core_grant_and_visible_result"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        const auto* Presenter = UPWSchoolPresenter::Get(Sub.GetPlayWorld());
        if (Life == nullptr || Presenter == nullptr || Presenter->GetKind() != EPWSchoolPanel::AwakeningResult || !Presenter->GetPanel().IsValid())
            return FStepOutcome::Fail(TEXT("the real result panel did not open after the grant"));
        const auto Panel = StaticCastSharedPtr<SPWAwakeningResultPanel>(Presenter->GetPanel());
        const auto& State = Life->GetWorldState();
        if (!State.Awakening.Granted || !State.LiveAwakenedProfile || State.Tutorial.Phase != PerfectWorld::TutorialDungeonPhase::Completed ||
            !State.Tutorial.StartedAt || State.Awakening.Granted->At != *State.Tutorial.StartedAt + PerfectWorld::TutorialSessionMinutes ||
            !Panel->GetModel().bGranted || Panel->GetModel().Tattvas.Num() != PerfectWorld::TattvaCount ||
            Panel->GetModel().Capabilities.Num() != PerfectWorld::PowerDimensionCount || !State.CountdownText.empty())
            return FStepOutcome::Fail(TEXT("grant, one-hour charge, live profile, panel rows or unopened countdown disagree"));
        return FStepOutcome::Pass(TEXT("the core holds a completed supervised grant and live profile; the result panel shows five Tattvas and six capabilities; countdown remains closed"));
    }));
    Out.Add(MakeShotStep(TEXT("shot_live_awakening_result")));
    Out.Add(P38Action(TEXT("close_result_restore_world_input"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* Presenter = UPWSchoolPresenter::Get(Sub.GetPlayWorld());
        if (Presenter == nullptr) return FStepOutcome::Fail(TEXT("no panel host"));
        if (Rt.Phase == 0) { Presenter->Close(); Rt.NextPhase(); return FStepOutcome::Wait(); }
        return !Presenter->IsOpen() && !UPWInteractionComponent::IsWorldInputBlocked(Sub.GetPawn())
            ? FStepOutcome::Pass(TEXT("result closed and world input restored")) : FStepOutcome::Fail(TEXT("result still owns the screen or input"));
    }));
    Out.Add(P310FloorWalk(TEXT("walk_to_campus_return"), -1));
    Out.Add(P38PadStep(TEXT("return_to_ordinary_campus"), -1, 0));
    Out.Add(P38Action(TEXT("countdown_closed_1759_opens_1800"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (Life == nullptr || !Life->GetWorldState().Awakening.Granted) return FStepOutcome::Fail(TEXT("no live grant"));
        const auto GrantAt = Life->GetWorldState().Awakening.Granted->At;
        FString Why;
        const auto Opening = PerfectWorld::CountdownStartInstant();
        if (!P38AdvanceTo(*Life, Opening - 1, Why)) return FStepOutcome::Fail(Why);
        if (Life->GetSim().Countdown() || !Life->GetWorldState().CountdownText.empty()) return FStepOutcome::Fail(TEXT("countdown opened before 18:00"));
        if (!P38AdvanceTo(*Life, Opening, Why)) return FStepOutcome::Fail(Why);
        const auto State = Life->GetSim().Countdown();
        const auto View = Life->GetSim().CountdownView();
        if (!State || !View.Issue.Ok() || !View.PreparationAvailable || View.OpeningAt != Opening || Life->GetNow() != Opening ||
            Life->GetWorldState().Awakening.Granted->At != GrantAt)
            return FStepOutcome::Fail(TEXT("the bound countdown failed to open at 18:00, or the grant changed"));
        return FStepOutcome::Pass(TEXT("real commands: closed at 17:59, open at exactly 18:00; preparation available and grant retained"));
    }));
    Out.Add(P38Action(TEXT("read_countdown_keep_ordinary_day"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Presenter = UPWSchoolPresenter::Get(Sub.GetPlayWorld());
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (Presenter == nullptr || Life == nullptr || !Presenter->OpenAwakeningCountdown()) return FStepOutcome::Fail(TEXT("countdown panel could not open"));
        const auto Panel = StaticCastSharedPtr<SPWCountdownPanel>(Presenter->GetPanel());
        const auto Before = Life->GetNow();
        const auto Countdown = Life->GetSim().Countdown();
        if (!Panel->GetModel().bAvailable || !Countdown || Panel->GetModel().Obligations.Num() != static_cast<int32>(Countdown->Obligations.size()) ||
            !Panel->TriggerKeepOrdinaryDay() || Life->GetNow() != Before)
            return FStepOutcome::Fail(TEXT("the live ordinary-day action, displayed obligations or free time contract disagree"));
        return FStepOutcome::Pass(TEXT("live countdown displayed core obligations; keeping an ordinary day cost no time"));
    }));
    Out.Add(MakeShotStep(TEXT("shot_live_countdown_ordinary_day")));
    Out.Add(P38Action(TEXT("close_countdown"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Presenter = UPWSchoolPresenter::Get(Sub.GetPlayWorld());
        if (Presenter == nullptr) return FStepOutcome::Fail(TEXT("no panel host"));
        Presenter->Close();
        return FStepOutcome::Pass(TEXT("countdown closed"));
    }));
}
} // namespace PWAcceptance
