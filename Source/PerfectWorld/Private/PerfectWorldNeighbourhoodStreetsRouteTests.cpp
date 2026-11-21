// R7-02: the walk from the lane to the school gate and the main road is free of blocking collision. The routes are the plan's own
// (RouteNeighbourhood), sampled every 10 cm along every outdoor link with a 34 cm capsule against the overlay's walls, solid props and
// Pawn barriers (plus the lane's own props and barriers across the reopened north alley), with a footpath, carriageway or ramp underfoot.
// Run under "PerfectWorld.Neighbourhood.Streets.WalkRoutes".

#include "PerfectWorldNeighbourhoodStreetsTestSupport.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsStreetsRoutesTest, "PerfectWorld.Neighbourhood.Streets.WalkRoutes",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNsStreetsRoutesTest::RunTest(const FString& Parameters)
{
    const PWNsTest::FFixture& F = PWNsTest::Get();
    struct FPair
    {
        const char* From;
        const char* To;
    };
    const FPair Pairs[] = {
        {"walk.door.nabapara.home_front", "walk.nabapara.school_gate"},
        {"walk.door.nabapara.home_front", "walk.nabapara.main_west"},
        {"walk.nabapara.lane_north_3340", "walk.nabapara.school_gate"},
        {"walk.nabapara.lane_north_3340", "walk.nabapara.bus_west"},
        {"walk.nabapara.lane_north_3340", "walk.nabapara.bus_east"},
        {"walk.nabapara.school_gate", "walk.nabapara.club_benches"},
        {"walk.nabapara.school_gate", "walk.nabapara.pandal"},
        {"walk.nabapara.school_gate", "walk.nabapara.club_approach"}, // the club's own door is sealed, so the approach is the end
    };
    for (const FPair& Pair : Pairs)
    {
        const PerfectWorld::NeighbourhoodRoute Route = PerfectWorld::RouteNeighbourhood(F.Ctx.Plan, Pair.From, Pair.To);
        if (!TestTrue(*FString::Printf(TEXT("a route %hs to %hs exists"), Pair.From, Pair.To), Route.Found))
        {
            continue;
        }
        int32 Samples = 0;
        int32 Blocked = 0;
        int32 NoFloor = 0;
        for (const std::string& LinkId : Route.LinkIds)
        {
            const PerfectWorld::NeighbourhoodLink* Link = nullptr;
            for (const PerfectWorld::NeighbourhoodLink& Candidate : F.Ctx.Plan.Links)
            {
                if (Candidate.Id == LinkId)
                {
                    Link = &Candidate;
                    break;
                }
            }
            if (Link == nullptr || Link->Kind == PerfectWorld::NeighbourhoodLinkKind::Interior || Link->Kind == PerfectWorld::NeighbourhoodLinkKind::Stair)
            {
                continue;
            }
            for (std::size_t I = 1; I < Link->Points.size(); ++I)
            {
                const auto& A = Link->Points[I - 1];
                const auto& B = Link->Points[I];
                const int32 Steps = FMath::Max(1, static_cast<int32>((FMath::Abs(B.X - A.X) + FMath::Abs(B.Y - A.Y)) / 10));
                for (int32 T = 0; T <= Steps; ++T)
                {
                    const double X = A.X + (B.X - A.X) * static_cast<double>(T) / Steps;
                    const double Y = A.Y + (B.Y - A.Y) * static_cast<double>(T) / Steps;
                    if (Y < 250.0)
                    {
                        continue; // inside the lane's own massing, which this order does not touch
                    }
                    ++Samples;
                    bool bHit = PWNsTest::SolidAt(F.Log, X, Y, PWNsTest::CapsuleRadius, PWNsTest::WalkLo, PWNsTest::WalkHi);
                    if (!bHit && X > 2700.0 && X < 3500.0)
                    {
                        // The reopened north alley: the lane's own solid props and barriers must not close it either.
                        for (const FBox& Box : F.Lane.Blocks.BlockingBoxes())
                        {
                            const double Dx = FMath::Max3(Box.Min.X - X, 0.0, X - Box.Max.X);
                            const double Dy = FMath::Max3(Box.Min.Y - Y, 0.0, Y - Box.Max.Y);
                            if (Box.Max.Z > PWNsTest::WalkLo && Box.Min.Z < PWNsTest::WalkHi && Dx * Dx + Dy * Dy < PWNsTest::CapsuleRadius * PWNsTest::CapsuleRadius)
                            {
                                bHit = true;
                                break;
                            }
                        }
                    }
                    Blocked += bHit ? 1 : 0;
                    NoFloor += PWNsTest::FloorAt(F, X, Y) ? 0 : 1;
                }
            }
        }
        TestTrue(*FString::Printf(TEXT("%hs to %hs was sampled"), Pair.From, Pair.To), Samples > 100);
        TestEqual(*FString::Printf(TEXT("samples blocked by collision on %hs to %hs"), Pair.From, Pair.To), Blocked, 0);
        TestEqual(*FString::Printf(TEXT("samples with no footpath, road or ramp under %hs to %hs"), Pair.From, Pair.To), NoFloor, 0);
    }
    return true;
}
#endif
