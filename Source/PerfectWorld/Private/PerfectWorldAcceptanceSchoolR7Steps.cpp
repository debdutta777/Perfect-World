#include "PerfectWorldAcceptanceSchoolR7Steps.h"

#include "Algo/Reverse.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameClock.h"
#include "NeighbourhoodPlan.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneHomeFurniture.h"
#include "PerfectWorldLaneInteriors.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldSchoolCampus.h"
#include "PerfectWorldSchoolGate.h"

// school_round7 and school_round7_late (see the header). What the scenario concludes about attendance it reads from the portable core's
// world (the gate seam's QueryArrival and the DayLoop attendance record), never from the notice text alone.

namespace PWAcceptance::SchoolR7
{

const FName ScAHomeDoorId(TEXT("door.nabapara.home_front"));

PWLane::FLaneFrame ScAHomeFrame()
{
    return PWLane::MakeLaneFrame(-300.0, -400.0, FVector(1.0, 0.0, 0.0), FVector(0.0, -1.0, 0.0));
}

FVector ScAHomePoint(double U, double V)
{
    return PWLane::HomeWorldPoint(ScAHomeFrame(), U, V, 0.0);
}

FStepDef ScAAction(const FString& Name, double Timeout, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run)
{
    FStepDef Def;
    Def.Name = Name;
    Def.TimeoutSeconds = Timeout;
    Def.Run = MoveTemp(Run);
    return Def;
}

APWDoorActor* ScAFindDoor(UWorld* World, FName Id)
{
    if (World != nullptr)
    {
        for (TActorIterator<APWDoorActor> It(World); It; ++It)
        {
            if (It->GetDoorId() == Id)
            {
                return *It;
            }
        }
    }
    return nullptr;
}

APWSchoolGateActor* ScAFindRegister(UWorld* World)
{
    if (World != nullptr)
    {
        for (TActorIterator<APWSchoolGateActor> It(World); It; ++It)
        {
            return *It;
        }
    }
    return nullptr;
}

APWSchoolBoardActor* ScAFindBoard(UWorld* World)
{
    if (World != nullptr)
    {
        for (TActorIterator<APWSchoolBoardActor> It(World); It; ++It)
        {
            return *It;
        }
    }
    return nullptr;
}

/** The core's gate projection through the same seam the register uses. */
bool ScAView(UPWAcceptanceSubsystem& Sub, PerfectWorld::SchoolArrivalView& OutView, FString& OutWhy)
{
    const TSharedPtr<PWSchool::IScGateWorld> Gate = PWSchool::ScGate::Resolve(Sub.GetPlayWorld());
    if (!Gate.IsValid())
    {
        OutWhy = TEXT("no gate seam");
        return false;
    }
    return Gate->QueryArrival(OutView, OutWhy);
}

FStepDef ScACamera(const FString& Name, bool bThird)
{
    return ScAAction(Name, 10.0, [bThird](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (Sub.GetPawn() == nullptr)
        {
            return FStepOutcome::Fail(TEXT("no pawn"));
        }
        Sub.GetPawn()->SetCameraMode(bThird ? EPWCameraMode::ThirdPerson : EPWCameraMode::FirstPerson);
        return FStepOutcome::Pass(bThird ? TEXT("third-person camera") : TEXT("first-person camera"));
    });
}

/** Points of a plan route (world cm), consecutive duplicates removed, the first SkipPoints dropped. */
bool ScARoute(const char* From, const char* To, int32 SkipPoints, TArray<FVector>& Out, FString& Why)
{
    const PerfectWorld::NeighbourhoodPlan Plan = PerfectWorld::BuildNeighbourhoodPlan(0);
    const PerfectWorld::NeighbourhoodRoute Route = PerfectWorld::RouteNeighbourhood(Plan, From, To);
    if (!Route.Found)
    {
        Why = FString::Printf(TEXT("the plan has no route from %hs to %hs"), From, To);
        return false;
    }
    TArray<FVector> All;
    for (size_t Leg = 0; Leg < Route.LinkIds.size(); ++Leg)
    {
        for (const PerfectWorld::NeighbourhoodLink& Link : Plan.Links)
        {
            if (Link.Id != Route.LinkIds[Leg])
            {
                continue;
            }
            TArray<FVector> Points;
            for (const PerfectWorld::NeighbourhoodPoint& P : Link.Points)
            {
                Points.Add(FVector(P.X, P.Y, ScAFloor));
            }
            if (Link.From != Route.NodeIds[Leg])
            {
                Algo::Reverse(Points);
            }
            for (const FVector& P : Points)
            {
                if (All.IsEmpty() || FVector::Dist2D(All.Last(), P) > 1.0)
                {
                    All.Add(P);
                }
            }
        }
    }
    for (int32 I = FMath::Max(0, SkipPoints); I < All.Num(); ++I)
    {
        Out.Add(All[I]);
    }
    if (Out.IsEmpty())
    {
        Why = TEXT("the route has no points to walk");
        return false;
    }
    return true;
}

/** Walks the real pawn along a plan route, waypoint by waypoint (the stuck detector restarts at each). */
FStepDef ScARouteWalk(const FString& Name, const char* From, const char* To, int32 SkipPoints, double Timeout)
{
    const TSharedRef<TArray<FVector>> Cache = MakeShared<TArray<FVector>>();
    return ScAAction(Name, Timeout, [Cache, From, To, SkipPoints](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        if (Cache->IsEmpty())
        {
            FString Why;
            if (!ScARoute(From, To, SkipPoints, *Cache, Why))
            {
                return FStepOutcome::Fail(Why);
            }
        }
        const bool bLast = Rt.Phase >= Cache->Num() - 1;
        const FStepOutcome Outcome = Sub.TickWalk((*Cache)[FMath::Min(Rt.Phase, Cache->Num() - 1)], bLast ? 60.0 : 45.0, Rt);
        if (Outcome.State == EStepState::Passed && !bLast)
        {
            Rt.NextPhase();
            Rt.LastProbe = 0.0;
            Rt.StuckSince = 0.0;
            return FStepOutcome::Wait();
        }
        if (Outcome.State == EStepState::Failed)
        {
            return FStepOutcome::Fail(FString::Printf(TEXT("waypoint %d of %d: %s"), Rt.Phase + 1, Cache->Num(), *Outcome.Detail));
        }
        if (Outcome.State == EStepState::Passed)
        {
            return FStepOutcome::Pass(FString::Printf(TEXT("%d waypoints of the plan route; %s"), Cache->Num(), *Outcome.Detail));
        }
        return Outcome;
    });
}

/** Walks a fixed list of points. */
FStepDef ScAPointsWalk(const FString& Name, const TArray<FVector>& Points, double Timeout, const FVector* FaceAfter)
{
    const FVector Face = FaceAfter != nullptr ? *FaceAfter : FVector::ZeroVector;
    const bool bFace = FaceAfter != nullptr;
    return ScAAction(Name, Timeout, [Points, Face, bFace](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        const bool bLast = Rt.Phase >= Points.Num() - 1;
        const FStepOutcome Outcome = Sub.TickWalk(Points[FMath::Min(Rt.Phase, Points.Num() - 1)], bLast ? 40.0 : 45.0, Rt);
        if (Outcome.State == EStepState::Passed && !bLast)
        {
            Rt.NextPhase();
            Rt.LastProbe = 0.0;
            Rt.StuckSince = 0.0;
            return FStepOutcome::Wait();
        }
        if (Outcome.State == EStepState::Passed && bFace)
        {
            Sub.FaceWorldPoint(Face);
        }
        if (Outcome.State == EStepState::Failed)
        {
            return FStepOutcome::Fail(FString::Printf(TEXT("point %d of %d: %s"), Rt.Phase + 1, Points.Num(), *Outcome.Detail));
        }
        return Outcome;
    });
}

// ---- Home ----

FStepDef ScAHomeToDoorStep()
{
    return ScAAction(TEXT("home_walk_to_front_door"), 40.0, [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        const APerfectWorldFrontEndHUD* Hud = Sub.GetHud();
        const APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
        if (Hud == nullptr || Hud->GetCreator() == nullptr || Pawn == nullptr || Hud->GetCreator()->GetLaneOrigin().bGrewUpInCare)
        {
            return FStepOutcome::Fail(TEXT("this scenario starts at the family home; the life has a care origin (use -PWAcceptanceStart=new with a clean save directory)"));
        }
        // The home's entry is 1.8 m inside the front door; stop 1.5 m in, clear of the inward-swinging leaf's 90 cm sweep.
        const FVector Target = ScAHomePoint(PWLane::HomeFrontDoorU, 150.0);
        if (FVector::Dist2D(Pawn->GetActorLocation(), Target) > 400.0 && Rt.Phase == 0)
        {
            return FStepOutcome::Fail(TEXT("the pawn does not start in the family home's entry (the save placed it elsewhere); use -PWAcceptanceStart=new"));
        }
        Rt.Phase = FMath::Max(Rt.Phase, 1);
        return Sub.TickWalk(Target, 30.0, Rt);
    });
}

FStepDef ScAHomeDoorStep()
{
    const auto IsOpen = [](UPWAcceptanceSubsystem& Sub)
    {
        const APWDoorActor* Door = ScAFindDoor(Sub.GetPlayWorld(), ScAHomeDoorId);
        const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        return Door != nullptr && Life != nullptr && !Door->IsMoving() && Door->IsOpen() && Door->GetOpenFraction() >= 0.99f && Life->IsDoorOpen(ScAHomeDoorId, false);
    };
    return MakeInteractStep(TEXT("home_open_front_door"),
        [](UPWAcceptanceSubsystem& Sub) -> AActor* { return ScAFindDoor(Sub.GetPlayWorld(), ScAHomeDoorId); },
        [IsOpen](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            Detail = TEXT("the leaf is open and the core agrees");
            return IsOpen(Sub);
        },
        IsOpen, 20.0);
}

// ---- Campus ----

FStepDef ScACampusBuiltStep()
{
    return ScAAction(TEXT("school_campus_built"), 20.0, [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        UWorld* World = Sub.GetPlayWorld();
        bool bBlocks = false;
        for (TActorIterator<AActor> It(World); It && !bBlocks; ++It)
        {
            bBlocks = It->ActorHasTag(PWSchool::ScBlocksActorTag);
        }
        if (!bBlocks || ScAFindRegister(World) == nullptr || ScAFindBoard(World) == nullptr)
        {
            return FStepOutcome::Fail(TEXT("the campus is not in the world: call PWSchool::SpawnSchoolCampus from BuildKolkataLane (see the R7-03 hook)"));
        }
        PerfectWorld::SchoolArrivalView View;
        FString Why;
        if (!ScAView(Sub, View, Why) || !View.HasSchool)
        {
            return FStepOutcome::Fail(FString::Printf(TEXT("the core has no school bound to the world: %s %hs"), *Why, View.Refusal.Code.c_str()));
        }
        return FStepOutcome::Pass(FString::Printf(TEXT("campus, register and board spawned; core: gates %s, first bell %s, dismissal %s"),
            *PWSchool::ScFormatMinute(View.GatesOpenMinute), *PWSchool::ScFormatMinute(View.FirstBellMinute), *PWSchool::ScFormatMinute(View.DismissalMinute)));
    });
}

FStepDef ScAReadBoardStep()
{
    return MakeInteractStep(TEXT("read_timetable_board"),
        [](UPWAcceptanceSubsystem& Sub) -> AActor* { return ScAFindBoard(Sub.GetPlayWorld()); },
        [](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const TSharedPtr<PWSchool::IScGateWorld> Gate = PWSchool::ScGate::Resolve(Sub.GetPlayWorld());
            const PerfectWorld::BellSchedule* Bell = Gate.IsValid() ? Gate->GetBellSchedule() : nullptr;
            const UPWInteractionComponent* Interaction = Sub.GetInteraction();
            if (Bell == nullptr || Interaction == nullptr)
            {
                return false;
            }
            const FString Notice = Interaction->GetNotice().ToString();
            Detail = TEXT("the notice shows every line of the authoritative bell schedule");
            for (const FString& Line : PWSchool::ScBellBoardLines(*Bell))
            {
                if (!Notice.Contains(Line))
                {
                    return false;
                }
            }
            return true;
        },
        nullptr, 15.0);
}

/** Interacts with the register and passes when the core's result has the expected code (empty: accepted). */
FStepDef ScASignInStep(const FString& Name, const TSharedRef<int32>& ExpectedCalls, const FString& WantCode, TFunction<bool(UPWAcceptanceSubsystem&)> SkipIf)
{
    return MakeInteractStep(Name,
        [](UPWAcceptanceSubsystem& Sub) -> AActor* { return ScAFindRegister(Sub.GetPlayWorld()); },
        [ExpectedCalls, WantCode](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const APWSchoolGateActor* Register = ScAFindRegister(Sub.GetPlayWorld());
            if (Register == nullptr || Register->GetSignInCount() < *ExpectedCalls)
            {
                return false;
            }
            const PWSchool::FScSignInResult& Result = Register->GetLastResult();
            Detail = FString::Printf(TEXT("core code '%s', accepted %d: %s"), *Result.Code, Result.bAccepted, *Register->GetLastMessage().ToString());
            return WantCode.IsEmpty() ? Result.bAccepted : (!Result.bAccepted && Result.Code == WantCode);
        },
        MoveTemp(SkipIf), 15.0);
}

/** Remembers how many times the register has been used, so the next sign-in step waits for exactly one more. */
FStepDef ScABaselineStep(const FString& Name, const TSharedRef<int32>& ExpectedCalls)
{
    return ScAAction(Name, 5.0, [ExpectedCalls](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const APWSchoolGateActor* Register = ScAFindRegister(Sub.GetPlayWorld());
        if (Register == nullptr)
        {
            return FStepOutcome::Fail(TEXT("no register"));
        }
        *ExpectedCalls = Register->GetSignInCount() + 1;
        return FStepOutcome::Pass(FString::Printf(TEXT("the register has been used %d times so far"), Register->GetSignInCount()));
    });
}

/** Moves the world clock to at least Minute of today (declared jump through the core's own commands); fails when the walk already ran past MaxMinute. */
FStepDef ScAClockStep(const FString& Name, int32 Minute, int32 MaxMinute)
{
    return ScAAction(Name, 15.0, [Minute, MaxMinute](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        UWorld* World = Sub.GetPlayWorld();
        UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(World);
        if (Life == nullptr)
        {
            return FStepOutcome::Fail(TEXT("no lane-life subsystem"));
        }
        const int32 Now = Life->GetMinuteOfDay();
        if (Now > MaxMinute)
        {
            return FStepOutcome::Fail(FString::Printf(TEXT("the walk ran past %s (clock %s)"), *PWSchool::ScFormatMinute(MaxMinute), *Life->GetClockText().ToString()));
        }
        if (Now >= Minute)
        {
            return FStepOutcome::Pass(FString::Printf(TEXT("the clock already reads %s, inside the window"), *Life->GetClockText().ToString()));
        }
        const PerfectWorld::GameInstant Target = PerfectWorld::GameClock::StartOfDay(Life->GetNow()) + Minute;
        FString Why;
        if (!PWSchool::ScAdvanceLaneClock(*World, Target, Why))
        {
            return FStepOutcome::Fail(Why);
        }
        return FStepOutcome::Pass(FString::Printf(TEXT("clock moved to %s through time.advance and AdoptSavedWorld"), *Life->GetClockText().ToString()));
    });
}

/** The core's attendance record for today holds the outcome. */
FStepDef ScARecordStep(const FString& Name, PerfectWorld::AttendanceOutcome Want, const TCHAR* Phrase)
{
    return ScAAction(Name, 10.0, [Want, Phrase](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        const UPWInteractionComponent* Interaction = Sub.GetInteraction();
        if (Life == nullptr || Interaction == nullptr)
        {
            return FStepOutcome::Fail(TEXT("no lane-life subsystem or interaction component"));
        }
        const auto& Days = Life->GetWorldState().DayLoop.Attendance.Days;
        if (Days.empty())
        {
            return FStepOutcome::Fail(TEXT("the core's attendance record is empty"));
        }
        const PerfectWorld::AttendanceDay& Day = Days.back();
        const FString Notice = Interaction->GetNotice().ToString();
        if (Day.Outcome != Want)
        {
            return FStepOutcome::Fail(FString::Printf(TEXT("the core recorded outcome %d (arrival %s), wanted %d. Notice: %s"), static_cast<int32>(Day.Outcome),
                *PWSchool::ScFormatMinute(Day.ArrivalMinute), static_cast<int32>(Want), *Notice));
        }
        if (!Notice.Contains(Phrase) || !Notice.Contains(PWSchool::ScFormatMinute(Day.ArrivalMinute)))
        {
            return FStepOutcome::Fail(FString::Printf(TEXT("the notice does not show '%s' and the arrival time: %s"), Phrase, *Notice));
        }
        return FStepOutcome::Pass(FString::Printf(TEXT("the core recorded arrival %s as outcome %d; the notice reads: %s"), *PWSchool::ScFormatMinute(Day.ArrivalMinute),
            static_cast<int32>(Day.Outcome), *Notice));
    });
}

/** The common opening: world, home door, the plan's route to the gate. Returns after the pawn stands at the gate node. */
void ScAWalkToGate(TArray<FStepDef>& Out)
{
    Out.Add(MakeWorldReadyStep());
    Out.Add(MakeEnterWorldStep());
    Out.Add(ScACampusBuiltStep());
    Out.Add(ScACamera(TEXT("camera_first"), false));
    Out.Add(ScAHomeToDoorStep());
    Out.Add(ScAHomeDoorStep());
    TArray<FVector> Out1;
    Out1.Add(ScAHomePoint(PWLane::HomeFrontDoorU, -110.0));
    Out.Add(ScAPointsWalk(TEXT("home_walk_out_to_the_lane"), Out1, 20.0));
    Out.Add(ScARouteWalk(TEXT("walk_home_to_school_gate"), "walk.door.nabapara.home_front", "walk.nabapara.school_gate", 1, 300.0));
}

PWSchool::FScPoints ScAPoints()
{
    PWSchool::FScBuilt Built;
    PWSchool::ScBuildCampus(Built, PWSchool::ScReadCampus());
    return Built.Points;
}
} // namespace PWAcceptance::SchoolR7
