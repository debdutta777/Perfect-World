#include "PerfectWorldAcceptanceStreetsR7.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NeighbourhoodPlan.h"
#include "PerfectWorldLaneBlocks.h"
#include "PerfectWorldPlayerCharacter.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

constexpr double R72WaypointToleranceCm = 40.0;
constexpr double R72ArrivalToleranceCm = 150.0;
constexpr double R72LegSeconds = 120.0;

struct FR72Waypoint
{
    FVector Point = FVector::ZeroVector;
    FString ShotLabel; // a screenshot is taken on reaching it when non-empty
};

const PerfectWorld::NeighbourhoodPlan& R72Plan()
{
    static const PerfectWorld::NeighbourhoodPlan Plan = PerfectWorld::BuildNeighbourhoodPlan(0);
    return Plan;
}

FString R72Label(const std::string& NodeId)
{
    static const std::pair<const char*, const char*> Labels[] = {
        {"walk.door.nabapara.home_front", "home"},
        {"walk.nabapara.lane_north_2860", "alley_mouth"},
        {"walk.nabapara.cross_west", "cross_street_top"},
        {"walk.nabapara.cross_east", "para_corner"},
        {"walk.nabapara.para_cross_south", "para_road"},
        {"walk.nabapara.school_approach", "school_approach"},
        {"walk.nabapara.school_gate", "school_gate"},
        {"walk.nabapara.main_north_west", "main_junction"},
        {"walk.nabapara.main_cross_west", "main_road"},
        {"walk.nabapara.bus_west", "bus_stop"},
    };
    for (const auto& Entry : Labels)
    {
        if (NodeId == Entry.first)
        {
            return FString(UTF8_TO_TCHAR(Entry.second));
        }
    }
    return FString();
}

/** The plan's route between two nodes as corner points (collinear points dropped), with a label on every milestone node. */
TArray<FR72Waypoint> R72Waypoints(const char* From, const char* To)
{
    TArray<FR72Waypoint> Out;
    const PerfectWorld::NeighbourhoodPlan& Plan = R72Plan();
    const PerfectWorld::NeighbourhoodRoute Route = PerfectWorld::RouteNeighbourhood(Plan, From, To);
    if (!Route.Found)
    {
        return Out;
    }
    // Every point of the route in walking order, each tagged with its node id when it is a node.
    std::vector<std::pair<PerfectWorld::NeighbourhoodPoint, std::string>> Chain;
    for (std::size_t I = 0; I < Route.LinkIds.size(); ++I)
    {
        for (const PerfectWorld::NeighbourhoodLink& Link : Plan.Links)
        {
            if (Link.Id != Route.LinkIds[I])
            {
                continue;
            }
            std::vector<PerfectWorld::NeighbourhoodPoint> Points = Link.Points;
            if (Link.From != Route.NodeIds[I])
            {
                std::reverse(Points.begin(), Points.end());
            }
            for (std::size_t P = 0; P < Points.size(); ++P)
            {
                if (P == 0 && !Chain.empty())
                {
                    continue; // the previous link's last point is this link's first
                }
                const std::string Node = (P == 0) ? Route.NodeIds[I] : ((P + 1 == Points.size()) ? Route.NodeIds[I + 1] : std::string());
                Chain.push_back({Points[P], Node});
            }
            break;
        }
    }
    for (std::size_t I = 1; I < Chain.size(); ++I)
    {
        const auto& Prev = Chain[I - 1].first;
        const auto& Here = Chain[I].first;
        const bool bLast = I + 1 == Chain.size();
        const bool bTurn = !bLast && ((Here.X - Prev.X) != 0) != ((Chain[I + 1].first.X - Here.X) != 0);
        const FString Label = R72Label(Chain[I].second);
        if (bLast || bTurn || !Label.IsEmpty())
        {
            FR72Waypoint W;
            W.Point = FVector(Here.X, Here.Y, PWLane::GroundZ + PWLane::FootpathHeight);
            W.ShotLabel = Label;
            Out.Add(W);
        }
    }
    return Out;
}

FVector R72NodePoint(const char* NodeId)
{
    const PerfectWorld::NeighbourhoodLinkNode* Node = PerfectWorld::FindNeighbourhoodNode(R72Plan(), NodeId);
    return Node == nullptr ? FVector::ZeroVector : FVector(Node->Position.X, Node->Position.Y, Node->Position.Z);
}

FStepDef R72Action(const FString& Name, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run, double Timeout = 20.0)
{
    FStepDef Def;
    Def.Name = Name;
    Def.TimeoutSeconds = Timeout;
    Def.Run = MoveTemp(Run);
    return Def;
}

/** Evidence that the overlay is in the world and the old stopping barrier is gone, by traces at plan coordinates. */
FStepDef R72StreetsPresent()
{
    return R72Action(TEXT("streets_present"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        UWorld* World = Sub.GetPlayWorld();
        if (World == nullptr)
        {
            return FStepOutcome::Fail(TEXT("no play world"));
        }
        FCollisionQueryParams Params(SCENE_QUERY_STAT(R72Streets), false);
        const auto TopAt = [&](double X, double Y, double& OutZ)
        {
            FHitResult Hit;
            if (!World->LineTraceSingleByChannel(Hit, FVector(X, Y, 300.0), FVector(X, Y, -500.0), ECC_Visibility, Params))
            {
                return false;
            }
            OutZ = Hit.ImpactPoint.Z;
            return true;
        };
        struct FProbe
        {
            const TCHAR* Name;
            double X, Y, Expect;
        };
        // The road surfaces sit at -200 and the footpaths at -185; the bare ground the lane lies on is at -205.
        const FProbe Probes[] = {{TEXT("cross street carriageway"), 3100, 1000, -200}, {TEXT("para road carriageway"), 8000, 4200, -200},
                                 {TEXT("main road carriageway"), 15650, 0, -200},       {TEXT("para south footpath"), 8000, 3700, -185},
                                 {TEXT("para north footpath"), 8000, 4700, -185},        {TEXT("main west footpath"), 15175, 8000, -185}};
        for (const FProbe& P : Probes)
        {
            double Z = 0.0;
            if (!TopAt(P.X, P.Y, Z) || FMath::Abs(Z - P.Expect) > 1.0)
            {
                return FStepOutcome::Fail(FString::Printf(TEXT("%s: surface at %.1f, expected %.0f"), P.Name, Z, P.Expect));
            }
        }
        // The north alley's old stopping barrier stood at Y 1060 to 1100 across X 2790 to 3410: a Pawn trace across it must be clear now.
        FHitResult Hit;
        FCollisionQueryParams PawnParams(SCENE_QUERY_STAT(R72Barrier), false);
        if (World->LineTraceSingleByChannel(Hit, FVector(2850.0, 1080.0, -100.0), FVector(3350.0, 1080.0, -100.0), ECC_Pawn, PawnParams) ||
            World->LineTraceSingleByChannel(Hit, FVector(3100.0, 700.0, -100.0), FVector(3100.0, 3000.0, -100.0), ECC_Pawn, PawnParams))
        {
            return FStepOutcome::Fail(FString::Printf(TEXT("the north alley is still blocked for the pawn at (%.0f, %.0f)"), Hit.ImpactPoint.X, Hit.ImpactPoint.Y));
        }
        return FStepOutcome::Pass(TEXT("six surfaces found at plan coordinates and the cross street is open to the pawn from the lane to the para road"));
    });
}

FStepDef R72DeclaredStart()
{
    return R72Action(TEXT("declared_start_home"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
        if (Pawn == nullptr)
        {
            return FStepOutcome::Fail(TEXT("no pawn"));
        }
        if (Rt.Phase == 0)
        {
            // On the footpath, 70 cm out from the home's front threshold; the sole teleport of the scenario.
            const FVector Door = R72NodePoint("walk.door.nabapara.home_front");
            const FVector Start(Door.X, Door.Y + 70.0, PWLane::GroundZ + PWLane::FootpathHeight + PWPlayer::CapsuleHalfHeight + 5.0);
            if (!Pawn->TeleportTo(Start, FRotator(0.0, 90.0, 0.0)))
            {
                return FStepOutcome::Fail(TEXT("the declared start outside the home was blocked"));
            }
            Pawn->GetCharacterMovement()->Velocity = FVector::ZeroVector;
            Rt.NextPhase();
        }
        if (Rt.InPhase() < 0.8 || !Pawn->GetCharacterMovement()->IsMovingOnGround())
        {
            return FStepOutcome::Wait();
        }
        return FStepOutcome::Pass(TEXT("standing on the footpath outside the home's front door; every later move is walked"));
    });
}

FStepDef R72Camera(const FString& Name, bool bThird)
{
    return R72Action(Name, [bThird](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (Sub.GetPawn() == nullptr)
        {
            return FStepOutcome::Fail(TEXT("no pawn"));
        }
        Sub.GetPawn()->SetCameraMode(bThird ? EPWCameraMode::ThirdPerson : EPWCameraMode::FirstPerson);
        return FStepOutcome::Pass(bThird ? TEXT("third person") : TEXT("first person"));
    });
}

FStepDef R72Arrived(const FString& Name, const FVector& Target)
{
    return R72Action(Name, [Target](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
        if (Pawn == nullptr)
        {
            return FStepOutcome::Fail(TEXT("no pawn"));
        }
        const FVector Here = Pawn->GetActorLocation();
        const double Distance = FVector::Dist2D(Here, Target);
        if (Distance > R72ArrivalToleranceCm || !Pawn->GetCharacterMovement()->IsMovingOnGround())
        {
            return FStepOutcome::Fail(FString::Printf(TEXT("%.0f cm from the destination (%.0f, %.0f), grounded %d"), Distance, Target.X, Target.Y,
                                                      Pawn->GetCharacterMovement()->IsMovingOnGround()));
        }
        return FStepOutcome::Pass(FString::Printf(TEXT("arrived within %.0f cm of (%.0f, %.0f)"), Distance, Target.X, Target.Y));
    });
}

/** One walked leg of a plan route: a walk step per corner, a screenshot at each milestone, an arrival assertion. */
void R72AddLeg(TArray<FStepDef>& Out, const FString& Leg, const char* From, const char* To)
{
    TArray<FR72Waypoint> Points = R72Waypoints(From, To);
    FVector Destination = R72NodePoint(To);
    if (!Points.IsEmpty() && std::string(To) == "walk.door.nabapara.home_front")
    {
        // The front door's threshold is in the wall line; the walk ends on the footpath 70 cm out, where the capsule fits.
        Points[Points.Num() - 1].Point.Y += 70.0;
        Destination.Y += 70.0;
    }
    if (Points.IsEmpty())
    {
        Out.Add(R72Action(Leg + TEXT("_route"), [](UPWAcceptanceSubsystem&, FStepRuntime&) { return FStepOutcome::Fail(TEXT("the plan has no route for this leg")); }));
        return;
    }
    int32 Index = 0;
    TSet<FString> Shots;
    for (const FR72Waypoint& W : Points)
    {
        const FVector Target = W.Point;
        FStepDef Walk = PWAcceptance::MakeWalkStep(FString::Printf(TEXT("%s_wp%02d"), *Leg, Index++),
                                                   [Target](UPWAcceptanceSubsystem&, FVector& OutTarget, FString&) { OutTarget = Target; return true; },
                                                   R72WaypointToleranceCm, R72LegSeconds);
        Out.Add(MoveTemp(Walk));
        if (!W.ShotLabel.IsEmpty() && !Shots.Contains(W.ShotLabel))
        {
            Shots.Add(W.ShotLabel);
            Out.Add(PWAcceptance::MakeShotStep(Leg + TEXT("_") + W.ShotLabel));
        }
    }
    Out.Add(R72Arrived(Leg + TEXT("_arrived"), Destination));
}
} // namespace

namespace PWAcceptance
{
void AppendStreetsRound7Steps(TArray<FStepDef>& Out)
{
    Out.Add(MakeWorldReadyStep());
    Out.Add(MakeEnterWorldStep());
    Out.Add(R72StreetsPresent());
    Out.Add(R72DeclaredStart());
    Out.Add(R72Camera(TEXT("camera_first"), false));
    Out.Add(MakeShotStep(TEXT("first_home_start")));
    R72AddLeg(Out, TEXT("first_home_to_school_gate"), "walk.door.nabapara.home_front", "walk.nabapara.school_gate");
    R72AddLeg(Out, TEXT("first_school_gate_to_main_road"), "walk.nabapara.school_gate", "walk.nabapara.bus_west");
    Out.Add(R72Camera(TEXT("camera_third"), true));
    Out.Add(MakeShotStep(TEXT("third_bus_stop_start")));
    R72AddLeg(Out, TEXT("third_main_road_to_school_gate"), "walk.nabapara.bus_west", "walk.nabapara.school_gate");
    R72AddLeg(Out, TEXT("third_school_gate_to_home"), "walk.nabapara.school_gate", "walk.door.nabapara.home_front");
}
} // namespace PWAcceptance
