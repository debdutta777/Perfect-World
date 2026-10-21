#include "PerfectWorldAcceptanceMahua.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldProprietorBindings.h"

namespace
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;
const FName R55Mahua(PWLane::MahuaPersonId);

struct FR55StorePlan
{
    FNpBindingProvider Provider;
    FVector Out;
    FVector Point(const TCHAR* Id) const
    {
        const auto* Anchor = Provider.Anchors.Find(FName(Id));
        return Anchor != nullptr ? Anchor->Feet.GetLocation() : FVector::ZeroVector;
    }
    FVector RoutePoint(const TCHAR* Id, int32 Index) const
    {
        const auto* Route = Provider.Routes.Find(FName(Id));
        return Route != nullptr && Route->Points.IsValidIndex(Index) ? Route->Points[Index] : FVector::ZeroVector;
    }
};

bool R55StorePlan(FR55StorePlan& Plan)
{
    PWLane::FLaneBuilding Building;
    if (!PWLane::MakeProprietorBuilding(PWLane::MahuaBuildingId, Building)) { return false; }
    Plan.Provider = PWLane::NpMahuaBindings(Building);
    Plan.Out = -Building.Frame.V;
    return Plan.Provider.Anchors.Num() >= 6 && Plan.Provider.Routes.Num() >= 5;
}

FStepDef R55MahuaAction(const TCHAR* Name, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run, double Timeout = 30.0)
{
    FStepDef Step;
    Step.Name = Name;
    Step.Run = MoveTemp(Run);
    Step.TimeoutSeconds = Timeout;
    return Step;
}

FStepDef R55MahuaWalk(const TCHAR* Name, TFunction<FVector(const FR55StorePlan&)> Pick)
{
    return PWAcceptance::MakeWalkStep(Name, [Pick](UPWAcceptanceSubsystem&, FVector& Target, FString& Why)
    {
        FR55StorePlan Plan;
        if (!R55StorePlan(Plan)) { Why = TEXT("hardware store provider unavailable"); return false; }
        Target = Pick(Plan);
        return true;
    }, 30.0, 40.0);
}

APWDoorActor* R55MahuaDoor(UPWAcceptanceSubsystem& Sub)
{
    for (TActorIterator<APWDoorActor> It(Sub.GetPlayWorld()); It; ++It)
        if (It->GetDoorId() == FName(PWLane::MahuaDoorId)) { return *It; }
    return nullptr;
}

FStepDef R55MahuaAt(const TCHAR* Name, const TCHAR* AnchorId, const EPWNpcState State, const TCHAR* Action = TEXT(""))
{
    return R55MahuaAction(Name, [AnchorId, State, Action](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        FR55StorePlan Plan;
        const auto* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
        const auto* Actor = Presence != nullptr ? Presence->FindPerson(R55Mahua) : nullptr;
        if (!R55StorePlan(Plan)) { return FStepOutcome::Fail(TEXT("store anchor unavailable")); }
        if (Actor == nullptr || FVector::Dist2D(Actor->GetActorLocation(), Plan.Point(AnchorId)) > 100.0 ||
            Actor->GetNpcMovement()->GetPresentedState() != State ||
            (Action[0] != 0 && Actor->GetNpcMovement()->GetIntent().WorkActionId != FName(Action)))
        {
            Rt.Phase = 0;
            return FStepOutcome::Wait();
        }
        if (Rt.Phase == 0) { Rt.NextPhase(); }
        return Rt.InPhase() >= 2.0 ? FStepOutcome::Pass(TEXT("Mahua reached her authored anchor and held the expected state/action")) : FStepOutcome::Wait();
    }, 75.0);
}
}

namespace PWAcceptance
{
void AppendMahuaWorkdaySteps(TArray<FStepDef>& Out)
{
    Out.Add(MakeWorldReadyStep());
    Out.Add(MakeEnterWorldStep());
    Out.Add(R55MahuaAction(TEXT("declared_start_outside_hardware_store"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        FR55StorePlan Plan;
        auto* Pawn = Sub.GetPawn();
        if (Pawn == nullptr || !R55StorePlan(Plan)) { return FStepOutcome::Fail(TEXT("no pawn or store plan")); }
        if (Rt.Phase == 0)
        {
            const FVector Feet = Plan.Point(PWLane::MahuaAnchorFrontDoor) + Plan.Out * 150.0;
            if (!Pawn->TeleportTo(Feet + FVector(0.0, 0.0, PWPlayer::CapsuleHalfHeight + 5.0), FRotator::ZeroRotator))
                return FStepOutcome::Fail(TEXT("declared-start placement refused"));
            Pawn->GetCharacterMovement()->StopMovementImmediately();
            Sub.FaceWorldPoint(Plan.Point(PWLane::MahuaAnchorFrontDoor));
            Rt.NextPhase();
        }
        return Rt.InPhase() >= 0.8 && Pawn->GetCharacterMovement()->IsMovingOnGround() ?
            FStepOutcome::Pass(TEXT("one declared start outside Mahua's threshold; all remaining movement is physical")) : FStepOutcome::Wait();
    }));
    Out.Add(R55PilotTimeStep(TEXT("mahua_clock_to_0900"), 9, 0));
    Out.Add(R55MahuaWalk(TEXT("mahua_walk_to_threshold"), [](const FR55StorePlan& P) { return P.Point(PWLane::MahuaAnchorFrontDoor); }));
    Out.Add(MakeInteractStep(TEXT("open_hardware_store_door"),
        [](UPWAcceptanceSubsystem& S) -> AActor* { return R55MahuaDoor(S); },
        [](UPWAcceptanceSubsystem& S, FString& Detail)
        {
            const auto* Door = R55MahuaDoor(S);
            Detail = TEXT("hardware store front door is open");
            return Door != nullptr && Door->IsOpen();
        }, [](UPWAcceptanceSubsystem& S) { const auto* Door = R55MahuaDoor(S); return Door != nullptr && Door->IsOpen(); }, 20.0));
    Out.Add(R55MahuaWalk(TEXT("mahua_walk_inside_front_aisle"), [](const FR55StorePlan& P)
        { return P.RoutePoint(TEXT("route.hardware_store.counter_to_door"), 3); }));
    Out.Add(R55MahuaWalk(TEXT("mahua_walk_to_customer_counter"), [](const FR55StorePlan& P) { return P.Point(PWLane::MahuaAnchorCustomer); }));
    Out.Add(R55MahuaAt(TEXT("mahua_serves_at_counter"), PWLane::MahuaAnchorCounter, EPWNpcState::Work, TEXT("work.serve")));
    Out.Add(MakeShotStep(TEXT("shot_mahua_physical_visit")));
    Out.Add(R55SafeTalkStep(TEXT("mahua_talk_waits_for_safe_pause"), R55Mahua));
    Out.Add(MakeShotStep(TEXT("shot_mahua_accepted_talk")));
    Out.Add(R55LeaveResumeStep(TEXT("mahua_leave_resumes_unfinished_work"), R55Mahua));
    Out.Add(R55MahuaAt(TEXT("mahua_resumes_serving"), PWLane::MahuaAnchorCounter, EPWNpcState::Work, TEXT("work.serve")));
    Out.Add(R55MahuaWalk(TEXT("mahua_walk_to_front_of_staff_gap"), [](const FR55StorePlan& P)
        { return P.RoutePoint(TEXT("route.hardware_store.counter_to_door"), 2); }));
    Out.Add(R55MahuaWalk(TEXT("mahua_obstruct_staff_route"), [](const FR55StorePlan& P)
        { return P.RoutePoint(TEXT("route.hardware_store.counter_to_weigh_station"), 1); }));
    Out.Add(R55ObstructionStep(TEXT("mahua_yields_without_pushing_player"), R55Mahua));
    Out.Add(MakeShotStep(TEXT("shot_mahua_obstruction")));
    Out.Add(R55MahuaWalk(TEXT("mahua_step_aside"), [](const FR55StorePlan& P)
        { return P.RoutePoint(TEXT("route.hardware_store.counter_to_door"), 2); }));
    Out.Add(R55MahuaAt(TEXT("mahua_recovers_to_weigh_station"), PWLane::MahuaAnchorWeigh, EPWNpcState::Work, TEXT("work.weigh")));
    Out.Add(R55MahuaWalk(TEXT("mahua_walk_back_to_front_aisle"), [](const FR55StorePlan& P)
        { return P.RoutePoint(TEXT("route.hardware_store.counter_to_door"), 3); }));
    Out.Add(R55MahuaWalk(TEXT("mahua_walk_outside"), [](const FR55StorePlan& P)
        { return P.Point(PWLane::MahuaAnchorFrontDoor) + P.Out * 120.0; }));
    Out.Add(R55MahuaWalk(TEXT("mahua_clear_threshold"), [](const FR55StorePlan& P)
        { return P.Point(PWLane::MahuaAnchorFrontDoor) + P.Out * 120.0 + FVector(250.0, 0.0, 0.0); }));
    Out.Add(R55PilotTimeStep(TEXT("mahua_clock_to_meal_1310"), 13, 10));
    Out.Add(R55DecisionStep(TEXT("mahua_midday_meal_priority"), R55Mahua, PerfectWorld::NpcDecisionReason::Meal));
    Out.Add(R55MahuaAt(TEXT("mahua_takes_meal_in_stock_room"), PWLane::MahuaAnchorStockRoom, EPWNpcState::Idle));
    Out.Add(R55PilotTimeStep(TEXT("mahua_clock_to_tea_1331"), 13, 31));
    Out.Add(R55DecisionStep(TEXT("mahua_tea_break_priority"), R55Mahua, PerfectWorld::NpcDecisionReason::TeaBreak));
    Out.Add(R55MahuaAt(TEXT("mahua_takes_tea_in_stock_room"), PWLane::MahuaAnchorStockRoom, EPWNpcState::Idle));
    Out.Add(R55PilotTimeStep(TEXT("mahua_clock_to_afternoon_1502"), 15, 2));
    Out.Add(R55MahuaAt(TEXT("mahua_returns_to_afternoon_counter"), PWLane::MahuaAnchorCounter, EPWNpcState::Work, TEXT("work.serve")));
    Out.Add(R55PilotTimeStep(TEXT("mahua_clock_to_closing_2010"), 20, 10));
    Out.Add(R55DecisionStep(TEXT("mahua_closing_up_reason"), R55Mahua, PerfectWorld::NpcDecisionReason::Closing));
    Out.Add(R55MahuaAt(TEXT("mahua_closing_at_scale"), PWLane::MahuaAnchorWeigh, EPWNpcState::Work, TEXT("work.weigh")));
    Out.Add(R55MahuaAction(TEXT("mahua_walks_to_threshold_then_leaves"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        FR55StorePlan Plan;
        if (!R55StorePlan(Plan)) { return FStepOutcome::Fail(TEXT("store plan unavailable")); }
        const auto* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
        const auto* Actor = Presence != nullptr ? Presence->FindPerson(R55Mahua) : nullptr;
        if (!Rt.bHaveTarget) { Rt.Target = FVector(0.0, 1.0e9, 0.0); Rt.bHaveTarget = true; }
        if (Actor == nullptr)
        {
            return Rt.Target.X >= 150.0 && Rt.Target.Y <= 250.0 ?
                FStepOutcome::Pass(TEXT("Mahua walked from scale to within 250 cm of threshold, then left the lane")) :
                FStepOutcome::Fail(TEXT("Mahua disappeared without an observed physical departure"));
        }
        Rt.Target.X = FMath::Max(Rt.Target.X, FVector::Dist2D(Actor->GetActorLocation(), Plan.Point(PWLane::MahuaAnchorWeigh)));
        Rt.Target.Y = FMath::Min(Rt.Target.Y, FVector::Dist2D(Actor->GetActorLocation(), Plan.Point(PWLane::MahuaAnchorFrontDoor)));
        return FStepOutcome::Wait();
    }, 120.0));
}
}
