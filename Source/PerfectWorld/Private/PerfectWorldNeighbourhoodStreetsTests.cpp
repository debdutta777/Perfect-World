// R7-02: the neighbourhood overlay's presentation, checked without a world. The overlay is built exactly as SpawnNeighbourhoodStreets builds
// it into a logging sink; the lane is built the way BuildKolkataLane builds it (rows, props, barriers). Run in the editor under
// "PerfectWorld.Neighbourhood.Streets" (Session Frontend, or the Unreal MCP AutomationTestToolset).
//  - FeaturesAtPlanCoordinates: carriageways, footpaths, crossings with ramps and zebra bars, the tram bed, the shops' walls and counters,
//    the pitches, the stand, the club, the bus shelters and the signboards stand where the plan says; the plan validates.
//  - BarrierRemoved: the north-alley stopping barrier is gone, the south-alley stop and every other seal remain.
//  - NoOverlapWithLaneFrontages: no generated building or wall overlaps a lane frontage, a plan footprint, a street or another lot; the
//    lane's nineteen first-row frontages are unchanged.
//  - AnchorsClear: every destination anchor of the plan has a clear 34 cm capsule.
//  - Sealed: nothing in the overlay is a bare gap in a frontage line (every street edge is a building, hoarding, plan lot or boundary).

#include "PerfectWorldNeighbourhoodStreetsTestSupport.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

namespace
{
using PWLane::ELaneColour;
using PWNs::FNsBox;
using PWNs::FNsRect;

bool NsBoxCovers(const TArray<FNsBox>& Log, ELaneColour Colour, double X, double Y)
{
    for (const FNsBox& B : Log)
    {
        if (B.Colour == Colour && X >= B.Min.X && X <= B.Max.X && Y >= B.Min.Y && Y <= B.Max.Y)
        {
            return true;
        }
    }
    return false;
}

int32 NsCountInRect(const TArray<FNsBox>& Log, ELaneColour Colour, const FNsRect& R, double MaxHeight = 1.0e9)
{
    int32 N = 0;
    for (const FNsBox& B : Log)
    {
        const bool bIn = B.Min.X >= R.X0 - 0.5 && B.Max.X <= R.X1 + 0.5 && B.Min.Y >= R.Y0 - 0.5 && B.Max.Y <= R.Y1 + 0.5;
        if (B.Colour == Colour && bIn && (B.Max.Z - B.Min.Z) <= MaxHeight)
        {
            ++N;
        }
    }
    return N;
}

FNsRect NsRectOf(const PerfectWorld::NeighbourhoodFeature& F)
{
    return FNsRect(F.Bounds.X0, F.Bounds.Y0, F.Bounds.X1, F.Bounds.Y1);
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsStreetsFeaturesTest, "PerfectWorld.Neighbourhood.Streets.FeaturesAtPlanCoordinates",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNsStreetsFeaturesTest::RunTest(const FString& Parameters)
{
    const PWNsTest::FFixture& F = PWNsTest::Get();
    const PerfectWorld::NeighbourhoodPlan& Plan = F.Ctx.Plan;
    const std::vector<std::string> Errors = PerfectWorld::ValidateNeighbourhoodPlan(Plan);
    TestTrue(TEXT("the R7-01 plan validates"), Errors.empty());

    // Surfaces at plan coordinates.
    TestTrue(TEXT("cross street carriageway"), NsBoxCovers(F.Log, ELaneColour::Asphalt, 3100.0, 1000.0));
    TestTrue(TEXT("para road carriageway"), NsBoxCovers(F.Log, ELaneColour::Asphalt, 8000.0, 4200.0));
    TestTrue(TEXT("main road carriageway"), NsBoxCovers(F.Log, ELaneColour::Asphalt, 15650.0, 0.0));
    TestTrue(TEXT("cross street footpath, east"), NsBoxCovers(F.Log, ELaneColour::Footpath, 3340.0, 1000.0));
    TestTrue(TEXT("cross street footpath, west"), NsBoxCovers(F.Log, ELaneColour::Footpath, 2860.0, 1000.0));
    TestTrue(TEXT("para south footpath"), NsBoxCovers(F.Log, ELaneColour::Footpath, 8000.0, 3700.0));
    TestTrue(TEXT("para north footpath"), NsBoxCovers(F.Log, ELaneColour::Footpath, 8000.0, 4700.0));
    TestTrue(TEXT("main west footpath"), NsBoxCovers(F.Log, ELaneColour::Footpath, 15175.0, 8000.0));
    TestTrue(TEXT("main east footpath"), NsBoxCovers(F.Log, ELaneColour::Footpath, 16825.0, 8000.0));
    TestTrue(TEXT("the apron in front of the school gate"), NsBoxCovers(F.Log, ELaneColour::Footpath, 6500.0, 4825.0));

    // The tram strip and its rails down the middle of the main road.
    const double TramX = PWNs::NsTramCentreX(F.Ctx);
    TestTrue(TEXT("tram bed"), NsBoxCovers(F.Log, ELaneColour::Concrete, TramX, 2000.0));
    TestTrue(TEXT("left rail"), NsBoxCovers(F.Log, ELaneColour::Chrome, TramX - 71.75, 2000.0));
    TestTrue(TEXT("right rail"), NsBoxCovers(F.Log, ELaneColour::Chrome, TramX + 71.75, 2000.0));

    // Crossings: two ramps each and a run of zebra bars on the road.
    int32 Crossings = 0;
    for (const PerfectWorld::NeighbourhoodFeature& Feature : Plan.Features)
    {
        if (Feature.Kind != PerfectWorld::NeighbourhoodFeatureKind::Crossing)
        {
            continue;
        }
        ++Crossings;
        const FNsRect R = NsRectOf(Feature);
        TestTrue(*FString::Printf(TEXT("zebra bars at %hs"), Feature.Id.c_str()), NsCountInRect(F.Log, ELaneColour::TrimLight, R.Grown(1.0), 1.6) >= 3);
    }
    TestEqual(TEXT("five crossings, two ramps each"), F.Ctx.Ramps.Num(), Crossings * 2);

    // The new shop lots: walls inside their footprints, counters (solid props) between the keeper's anchor and the entrance, signboards.
    for (const char* Stem : {"sweet_shop", "pharmacy", "tea", "vendor", "rickshaw_auto"})
    {
        const PerfectWorld::NeighbourhoodFeature* Anchor = F.Ctx.Feature((std::string("anchor.nabapara.") + Stem + "_counter").c_str());
        const PerfectWorld::NeighbourhoodFeature* Entrance = F.Ctx.Feature((std::string("entrance.nabapara.") + Stem).c_str());
        if (!TestNotNull(*FString::Printf(TEXT("plan has the %hs anchor and entrance"), Stem), Anchor) || !TestNotNull(TEXT("entrance"), Entrance))
        {
            continue;
        }
        const double X = Anchor->Geometry[0].X;
        const FNsRect Counter(X - 400.0, Anchor->Geometry[0].Y - 60.0, X + 400.0, Entrance->Geometry[0].Y);
        TestTrue(*FString::Printf(TEXT("a counter stands between the %hs keeper and its entrance"), Stem),
                 NsCountInRect(F.Log, ELaneColour::PropBlock, Counter) >= 1);
    }
    for (const char* Lot : {"building.nabapara.sweet_shop", "building.nabapara.pharmacy", "building.nabapara.club"})
    {
        const PerfectWorld::NeighbourhoodFeature* Feature = F.Ctx.Feature(Lot);
        if (TestNotNull(*FString::Printf(TEXT("plan has %hs"), Lot), Feature))
        {
            int32 Walls = 0;
            for (const FNsBox& B : F.Log)
            {
                Walls += (PWNsTest::IsWall(B.Colour) && NsRectOf(*Feature).Contains(0.5 * (B.Min.X + B.Max.X), 0.5 * (B.Min.Y + B.Max.Y))) ? 1 : 0;
            }
            TestTrue(*FString::Printf(TEXT("walls inside %hs"), Lot), Walls >= 4);
        }
    }

    // Both bus stops: a shelter's solid posts within 300 cm of each stop feature.
    int32 Stops = 0;
    for (const PerfectWorld::NeighbourhoodFeature& Feature : Plan.Features)
    {
        if (Feature.Kind == PerfectWorld::NeighbourhoodFeatureKind::BusStop)
        {
            ++Stops;
            const FNsRect Near(Feature.Geometry[0].X - 300.0, Feature.Geometry[0].Y - 300.0, Feature.Geometry[0].X + 300.0, Feature.Geometry[0].Y + 300.0);
            TestTrue(*FString::Printf(TEXT("a shelter with solid posts at %hs"), Feature.Id.c_str()), NsCountInRect(F.Log, ELaneColour::PropBlock, Near) >= 3);
        }
    }
    TestEqual(TEXT("two bus stops"), Stops, 2);

    TestTrue(TEXT("generated frontage buildings"), F.Ctx.Lots.Num() > 100);
    TestTrue(TEXT("block-lettered signboards"), F.Ctx.SignCount >= 40);
    TestTrue(TEXT("every sign name can be lettered"), PWNs::NsCanDraw(PWNs::NsShopName(0u, 64)) && PWNs::NsCanDraw(PWNs::NsAdName(3u)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsStreetsBarrierTest, "PerfectWorld.Neighbourhood.Streets.BarrierRemoved",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNsStreetsBarrierTest::RunTest(const FString& Parameters)
{
    const PWNsTest::FFixture& F = PWNsTest::Get();
    int32 NorthAlleyStops = 0;
    int32 SouthAlleyStops = 0;
    for (const FBox& Box : F.Lane.Blocks.BlockingBoxes())
    {
        const bool bWide = Box.Max.X - Box.Min.X > 500.0 && Box.Max.Y - Box.Min.Y < 60.0;
        const double Lo = FMath::Min(FMath::Abs(Box.Min.Y), FMath::Abs(Box.Max.Y));
        const double Hi = FMath::Max(FMath::Abs(Box.Min.Y), FMath::Abs(Box.Max.Y));
        const bool bStopY = Lo > 1050.0 && Lo < 1070.0 && Hi > 1090.0 && Hi < 1110.0;
        if (bWide && bStopY && Box.Min.Y > 0.0 && Box.Min.X < 2800.0 && Box.Max.X > 3400.0)
        {
            ++NorthAlleyStops;
        }
        if (bWide && bStopY && Box.Max.Y < 0.0 && Box.Min.X < 4300.0 && Box.Max.X > 4900.0)
        {
            ++SouthAlleyStops;
        }
    }
    TestEqual(TEXT("the north-alley stopping barrier is gone"), NorthAlleyStops, 0);
    TestEqual(TEXT("the south-alley stop stays"), SouthAlleyStops, 1);
    TestEqual(TEXT("the lane keeps its nineteen first-row frontages: -Y"), F.Lane.Fronts[0].Num(), 10);
    TestEqual(TEXT("the lane keeps its nineteen first-row frontages: +Y"), F.Lane.Fronts[1].Num(), 9);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsStreetsOverlapTest, "PerfectWorld.Neighbourhood.Streets.NoOverlapWithLaneFrontages",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNsStreetsOverlapTest::RunTest(const FString& Parameters)
{
    const PWNsTest::FFixture& F = PWNsTest::Get();
    TArray<FNsRect> Lane;
    for (const PerfectWorld::NeighbourhoodFeature& Feature : F.Ctx.Plan.Features)
    {
        if (Feature.Kind == PerfectWorld::NeighbourhoodFeatureKind::Footprint && Feature.Existing)
        {
            Lane.Add(NsRectOf(Feature));
        }
    }
    for (int32 Side = 0; Side < 2; ++Side)
    {
        for (const PWLane::FLaneFront& Front : F.Lane.Fronts[Side])
        {
            // A recorded frontage runs from its front face (|Y| = AbsY) back to the lane's reserved massing (|Y| = 1500) at most.
            Lane.Add(Side == 0 ? FNsRect(Front.X0, -1500.0, Front.X1, -Front.AbsY) : FNsRect(Front.X0, Front.AbsY, Front.X1, 1500.0));
        }
    }
    TestTrue(TEXT("the lane's frontages and the plan's existing footprints are known"), Lane.Num() >= 28);

    int32 Overlaps = 0;
    for (const PWNs::FNsLot& Lot : F.Ctx.Lots)
    {
        for (const FNsRect& R : Lane)
        {
            Overlaps += Lot.R.Overlaps(R) ? 1 : 0;
        }
        for (const FNsRect& Solid : F.Ctx.PlanSolids())
        {
            Overlaps += Lot.R.Overlaps(Solid) ? 1 : 0;
        }
    }
    for (const PWNs::FNsWall& Wall : F.Ctx.Walls)
    {
        for (const FNsRect& R : Lane)
        {
            Overlaps += Wall.R.Overlaps(R) ? 1 : 0;
        }
    }
    TestEqual(TEXT("generated buildings and walls overlapping a lane frontage, plan footprint or street"), Overlaps, 0);

    int32 LotOverlaps = 0;
    for (int32 I = 0; I < F.Ctx.Lots.Num(); ++I)
    {
        for (int32 J = I + 1; J < F.Ctx.Lots.Num(); ++J)
        {
            LotOverlaps += F.Ctx.Lots[I].R.Overlaps(F.Ctx.Lots[J].R) ? 1 : 0;
        }
        for (const PWNs::FNsWall& Wall : F.Ctx.Walls)
        {
            LotOverlaps += F.Ctx.Lots[I].R.Overlaps(Wall.R) ? 1 : 0;
        }
    }
    TestEqual(TEXT("generated buildings overlapping each other or a wall"), LotOverlaps, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsStreetsAnchorsTest, "PerfectWorld.Neighbourhood.Streets.AnchorsClear",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNsStreetsAnchorsTest::RunTest(const FString& Parameters)
{
    const PWNsTest::FFixture& F = PWNsTest::Get();
    int32 Anchors = 0;
    for (const PerfectWorld::NeighbourhoodFeature& Feature : F.Ctx.Plan.Features)
    {
        if (Feature.Kind != PerfectWorld::NeighbourhoodFeatureKind::Destination && Feature.Kind != PerfectWorld::NeighbourhoodFeatureKind::BusStop)
        {
            continue;
        }
        ++Anchors;
        TestFalse(*FString::Printf(TEXT("%hs has a clear standing capsule"), Feature.Id.c_str()),
                  PWNsTest::SolidAt(F.Log, Feature.Geometry[0].X, Feature.Geometry[0].Y, PWNsTest::CapsuleRadius, PWNsTest::WalkLo, PWNsTest::WalkHi));
    }
    TestTrue(TEXT("the plan's destinations were checked"), Anchors >= 10);
    return true;
}
#endif
