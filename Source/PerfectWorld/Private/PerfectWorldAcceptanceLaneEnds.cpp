#include "PerfectWorldAcceptanceLaneEnds.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PerfectWorldLaneEndInteriors.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldShopDisplayActor.h"

#include <algorithm>
#include <string>
#include <vector>

namespace
{
namespace R65LE = PerfectWorld::LaneEnds;
namespace R65LI = PerfectWorld::LaneInterior;
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

constexpr double R65PointToleranceCm = 40.0;
constexpr double R65ApproachToleranceCm = 60.0;
constexpr double R65LegSeconds = 100.0;
constexpr double R65LaneLegSeconds = 150.0;

FString R65Name(const R65LE::EndHall Hall)
{
    return Hall == R65LE::EndHall::CommunityHall ? TEXT("community") : TEXT("market");
}

FVector R65Point(const R65LE::EndHall Hall, const double U, const double V)
{
    const R65LE::WorldPoint P = R65LE::HallWorldPoint(Hall, static_cast<int>(U), static_cast<int>(V));
    return FVector(P.X, P.Y, PWLane::GroundZ + PWLane::FootpathHeight);
}

bool R65Inside(const R65LE::EndHall Hall, const FVector& At)
{
    const R65LE::WorldPoint A = R65LE::HallWorldPoint(Hall, 0, 0);
    const R65LE::WorldPoint B = R65LE::HallWorldPoint(Hall, R65LE::HallWidthCm, R65LE::HallDepthCm);
    return At.X > std::min(A.X, B.X) && At.X < std::max(A.X, B.X) && At.Y > std::min(A.Y, B.Y) && At.Y < std::max(A.Y, B.Y);
}

FStepDef R65Action(const FString& Name, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run)
{
    FStepDef Def;
    Def.Name = Name;
    Def.Run = MoveTemp(Run);
    return Def;
}

FStepDef R65Walk(const FString& Name, const FVector& Target, const double Tolerance, const double Seconds)
{
    return PWAcceptance::MakeWalkStep(Name, [Target](UPWAcceptanceSubsystem&, FVector& Out, FString&) { Out = Target; return true; }, Tolerance, Seconds);
}

FStepDef R65Camera(const FString& Name, const bool bThird)
{
    return R65Action(Name, [bThird](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (Sub.GetPawn() == nullptr)
        {
            return FStepOutcome::Fail(TEXT("no pawn"));
        }
        Sub.GetPawn()->SetCameraMode(bThird ? EPWCameraMode::ThirdPerson : EPWCameraMode::FirstPerson);
        return FStepOutcome::Pass(bThird ? TEXT("third person") : TEXT("first person"));
    });
}

FStepDef R65DeclaredStart()
{
    return R65Action(TEXT("declared_start_west"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
        if (Pawn == nullptr)
        {
            return FStepOutcome::Fail(TEXT("no pawn"));
        }
        if (Rt.Phase == 0)
        {
            // On the lane's axis, 400 cm from the community hall's archway: the sole teleport; every later move is walked.
            const FVector Start = R65Point(R65LE::EndHall::CommunityHall, R65LE::GetHallInfo(R65LE::EndHall::CommunityHall).EntranceCentreU, -400.0) +
                                  FVector(0.0, 0.0, PWPlayer::CapsuleHalfHeight + 5.0);
            if (!Pawn->TeleportTo(Start, FRotator(0.0, 180.0, 0.0)))
            {
                return FStepOutcome::Fail(TEXT("the declared start on the lane axis was blocked"));
            }
            Pawn->GetCharacterMovement()->Velocity = FVector::ZeroVector;
            Rt.NextPhase();
        }
        if (Rt.InPhase() < 0.8 || !Pawn->GetCharacterMovement()->IsMovingOnGround())
        {
            return FStepOutcome::Wait();
        }
        return FStepOutcome::Pass(TEXT("standing on the lane axis 400 cm from the community hall; the rest is walked"));
    });
}

/** The posted board of a hall: present, with the portable text, on the lane side of the wall. */
FStepOutcome R65CheckSign(UPWAcceptanceSubsystem& Sub, const R65LE::EndHall Hall)
{
    const FName Id(UTF8_TO_TCHAR(std::string(R65LE::GetHallInfo(Hall).BuildingId).c_str()));
    for (TActorIterator<APWShopHoursSignActor> It(Sub.GetPlayWorld()); It; ++It)
    {
        if (It->GetBuildingId() == Id)
        {
            return It->GetPostedText() == PWLane::LaneEndSignText(Hall) && It->GetActorLocation().Equals(PWLane::LaneEndSignLocation(Hall), 1.0)
                       ? FStepOutcome::Pass(It->GetPostedText().Replace(TEXT("\n"), TEXT(" / ")))
                       : FStepOutcome::Fail(TEXT("the board text or place differs from the portable hall"));
        }
    }
    return FStepOutcome::Fail(TEXT("no posted hours board for this hall"));
}

/** The authored route to the named anchor: the points of the route whose id ends in `Suffix` (outside the archway first). */
std::vector<R65LI::RoutePoint> R65Route(const R65LE::EndHall Hall, const char* Suffix)
{
    const R65LI::InteriorLayout Layout = R65LE::GenerateEndLayout(Hall);
    if (const R65LI::Route* Route = R65LI::FindRoute(Layout, std::string(R65LE::GetHallInfo(Hall).BuildingId) + ".route." + Suffix))
    {
        return Route->Points;
    }
    return {};
}

/** Walks the route point by point, forward (in) or reversed and ending 110 cm outside the archway (out). */
FStepDef R65WalkRoute(const FString& Name, const R65LE::EndHall Hall, const char* Suffix, const bool bOut)
{
    std::vector<R65LI::RoutePoint> Points = R65Route(Hall, Suffix);
    if (bOut)
    {
        std::reverse(Points.begin(), Points.end());
        Points.push_back({R65LE::GetHallInfo(Hall).EntranceCentreU, -110});
    }
    FStepDef Def = R65Action(Name, [Hall, Points, bOut](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        const size_t Index = static_cast<size_t>(Rt.Phase);
        if (Points.empty() || Index >= Points.size())
        {
            return FStepOutcome::Fail(TEXT("no authored route"));
        }
        const FStepOutcome Outcome = Sub.TickWalk(R65Point(Hall, Points[Index].U, Points[Index].V), R65PointToleranceCm, Rt);
        if (Outcome.State != PWAcceptance::EStepState::Passed)
        {
            return Outcome;
        }
        if (Index + 1 < Points.size())
        {
            Rt.NextPhase();
            Rt.LastProbe = Rt.StuckSince = 0;
            return FStepOutcome::Wait();
        }
        return FStepOutcome::Pass(bOut ? TEXT("walked back out through the archway") : TEXT("walked the aisle to its stand"));
    });
    Def.TimeoutSeconds = R65LegSeconds;
    return Def;
}

FStepDef R65InsideCheck(const FString& Name, const R65LE::EndHall Hall, const bool bExpectInside)
{
    return R65Action(Name, [Hall, bExpectInside](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (Sub.GetPawn() == nullptr)
        {
            return FStepOutcome::Fail(TEXT("no pawn"));
        }
        const FVector At = Sub.GetPawn()->GetActorLocation();
        return R65Inside(Hall, At) == bExpectInside
                   ? FStepOutcome::Pass(FString::Printf(TEXT("pawn at (%.0f, %.0f): %s"), At.X, At.Y, bExpectInside ? TEXT("inside the hall") : TEXT("outside the hall")))
                   : FStepOutcome::Fail(FString::Printf(TEXT("pawn at (%.0f, %.0f) is %s the hall"), At.X, At.Y, bExpectInside ? TEXT("not inside") : TEXT("still inside")));
    });
}

/** One visit: approach on the lane axis, through the archway, the route to the stand, a screenshot, and back out. */
void R65Visit(TArray<FStepDef>& Out, const R65LE::EndHall Hall, const char* RouteSuffix, const FString& Camera)
{
    const FString Hall65 = R65Name(Hall) + Camera;
    const int32 Centre = R65LE::GetHallInfo(Hall).EntranceCentreU;
    Out.Add(R65Walk(Hall65 + TEXT("_approach"), R65Point(Hall, Centre, -150.0), R65ApproachToleranceCm, R65LegSeconds));
    Out.Add(R65InsideCheck(Hall65 + TEXT("_outside_before"), Hall, false));
    Out.Add(R65Walk(Hall65 + TEXT("_through_archway"), R65Point(Hall, Centre, 85.0), R65PointToleranceCm, R65LegSeconds));
    Out.Add(R65InsideCheck(Hall65 + TEXT("_inside"), Hall, true));
    Out.Add(R65WalkRoute(Hall65 + TEXT("_walk_aisle"), Hall, RouteSuffix, false));
    Out.Add(R65InsideCheck(Hall65 + TEXT("_inside_at_stand"), Hall, true));
    Out.Add(PWAcceptance::MakeShotStep(Hall65 + TEXT("_stand")));
    Out.Add(R65WalkRoute(Hall65 + TEXT("_walk_out"), Hall, RouteSuffix, true));
    Out.Add(R65InsideCheck(Hall65 + TEXT("_outside_after"), Hall, false));
    Out.Add(PWAcceptance::MakeShotStep(Hall65 + TEXT("_outside")));
}
} // namespace

namespace PWAcceptance
{
void AppendLaneEndsSteps(TArray<FStepDef>& Out)
{
    Out.Add(MakeWorldReadyStep());
    Out.Add(MakeEnterWorldStep());
    Out.Add(R65Action(TEXT("boards_posted"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const FStepOutcome West = R65CheckSign(Sub, R65LE::EndHall::CommunityHall);
        if (West.State != EStepState::Passed)
        {
            return West;
        }
        const FStepOutcome East = R65CheckSign(Sub, R65LE::EndHall::MarketHall);
        return East.State == EStepState::Passed ? FStepOutcome::Pass(West.Detail + TEXT(" | ") + East.Detail) : East;
    }));
    Out.Add(R65DeclaredStart());
    Out.Add(MakeShotStep(TEXT("west_end_from_lane")));
    for (const bool bThird : {false, true})
    {
        const FString Camera = bThird ? TEXT("_third") : TEXT("_first");
        Out.Add(R65Camera(R65Name(R65LE::EndHall::CommunityHall) + Camera + TEXT("_camera"), bThird));
        R65Visit(Out, R65LE::EndHall::CommunityHall, "door_to_stage_front", Camera);
        if (!bThird)
        {
            // The notice board too, once, in first person: out and in again by its own route.
            Out.Add(R65Walk(TEXT("community_first_notice_approach"), R65Point(R65LE::EndHall::CommunityHall, 1300, 85.0), R65PointToleranceCm, R65LegSeconds));
            Out.Add(R65WalkRoute(TEXT("community_first_notice_aisle"), R65LE::EndHall::CommunityHall, "door_to_notice_board", false));
            Out.Add(MakeShotStep(TEXT("community_first_notice_board")));
            Out.Add(R65WalkRoute(TEXT("community_first_notice_out"), R65LE::EndHall::CommunityHall, "door_to_notice_board", true));
        }
    }
    // The whole lane to the east end, walked along its axis in legs the pawn can see its way along.
    for (const double X : {0.0, 2000.0, 4000.0, 6000.0})
    {
        Out.Add(R65Walk(FString::Printf(TEXT("lane_east_to_%d"), static_cast<int32>(X)), FVector(X, 0.0, PWLane::GroundZ + PWLane::FootpathHeight), 80.0, R65LaneLegSeconds));
    }
    for (const bool bThird : {false, true})
    {
        const FString Camera = bThird ? TEXT("_third") : TEXT("_first");
        Out.Add(R65Camera(R65Name(R65LE::EndHall::MarketHall) + Camera + TEXT("_camera"), bThird));
        R65Visit(Out, R65LE::EndHall::MarketHall, "door_to_counter_b1", Camera);
    }
}
} // namespace PWAcceptance
