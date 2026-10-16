// Layout rules of the care interior (PerfectWorld.CareHome.*): the building selection, door clear widths and swing zones, route
// widths, the care worker's spot, the seat, readable and storage anchors, the rooms with their personal objects, the reserved
// stair, ceiling fans and room lights. The home is built exactly as the lane builds it (BuildCareHome), so these tests read the
// real collision boxes. Run in the editor under "PerfectWorld.CareHome".

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "PerfectWorldLaneCareInterior.h"
#include "PerfectWorldLaneInteriors.h"
#include "PerfectWorldLaneTeaShop.h"

namespace PWCareTest
{
using namespace PWLane;

constexpr int32 CellsU = 221; // a 5 cm grid over U 0-1100
constexpr int32 CellsV = 181; // and V 0-900
constexpr double Tol = 1.0e-6;

struct FCRect { double U0 = 0.0, U1 = 0.0, V0 = 0.0, V1 = 0.0; };
struct FCBlock { FCRect R; double H0 = 0.0, H1 = 0.0; }; // heights above the finished floor

struct FCHome
{
    FLaneBuilding Building;
    FLaneScene Scene;
    TArray<FHomeSolid> Solids;
    TArray<FCBlock> Blocks; // every collision box of the shell and the furniture, in building coordinates
    TArray<FCRect> Zones;   // every swing zone of every door (both sides for a two-way door)
};

FCRect ToRect(const FCareRect& R) { return FCRect{R.U0, R.U1, R.V0, R.V1}; }
FCRect RectOf(const FHomeSolid& S) { return FCRect{S.U0, S.U1, S.V0, S.V1}; }
bool Overlaps(const FCRect& A, const FCRect& B) { return A.U0 < B.U1 && B.U0 < A.U1 && A.V0 < B.V1 && B.V0 < A.V1; }
/** True for something that stops a standing person (not the floor slab, not the lintel). */
bool IsBodyBlock(const FCBlock& Block) { return Block.H1 > 5.0 && Block.H0 < 180.0; }

double DistToRect(double U, double V, const FCRect& R)
{
    return FMath::Sqrt(FMath::Square(FMath::Max3(R.U0 - U, 0.0, U - R.U1)) + FMath::Square(FMath::Max3(R.V0 - V, 0.0, V - R.V1)));
}

void ToLocal(const FLaneFrame& F, const FVector& P, double& U, double& V)
{
    U = FVector::DotProduct(P - F.Origin, F.U);
    V = FVector::DotProduct(P - F.Origin, F.V);
}

FLaneBuilding MakeCareBuilding()
{
    FLaneBuilding B;
    B.Frame = MakeLaneFrame(-1400.0, -400.0, FVector(1.0, 0.0, 0.0), FVector(0.0, -1.0, 0.0));
    B.Width = 1100.0;
    B.Depth = 900.0;
    B.Setback = 0.0;
    B.Storeys = 3;
    B.Side = -1;
    B.Index = 1;
    B.Wall = ELaneColour::WallButter;
    B.Seed = CareGeneratorSeed; // Detail is Full by default
    return B;
}

const FLaneDoorSpec& DoorOf(const FCHome& Home, const TCHAR* Id)
{
    return *Home.Scene.Doors.FindByPredicate([Id](const FLaneDoorSpec& D) { return D.Id == FName(Id); });
}

/** The floor the door's leaf can reach while it swings (leaf 90 + thickness, opening 95 degrees), on one side. */
FCRect SwingZone(const FLaneFrame& F, const FLaneDoorSpec& D, double SideSign)
{
    const double Yaw = FMath::DegreesToRadians(D.ClosedYaw);
    const FVector Dir(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.0);
    const FVector Side = FVector(-Dir.Y, Dir.X, 0.0) * SideSign;
    FCRect Zone{1.0e9, -1.0e9, 1.0e9, -1.0e9};
    for (const double Along : {-9.0, 92.0})
    {
        for (const double Across : {-2.0, 92.0})
        {
            double U = 0.0;
            double V = 0.0;
            ToLocal(F, D.Hinge + Dir * Along + Side * Across, U, V);
            Zone = FCRect{FMath::Min(Zone.U0, U), FMath::Max(Zone.U1, U), FMath::Min(Zone.V0, V), FMath::Max(Zone.V1, V)};
        }
    }
    return Zone;
}

/** The common door's leaf at its default (open) state: hinged at its back jamb, lying 90 cm toward the entry along the across partition. */
FCRect OpenLeafRect(const FCHome& Home)
{
    double U = 0.0;
    double V = 0.0;
    ToLocal(Home.Building.Frame, DoorOf(Home, CareDoorCommonId).Hinge, U, V);
    return FCRect{U - 92.0, U + 2.0, V - 2.0, V + 12.0};
}

/** Builds the care home like the lane does and gathers every blocking box and swing zone. False if the building is not recognised. */
bool BuildCare(FCHome& Home)
{
    Home.Building = MakeCareBuilding();
    if (!IsCareHomeBuilding(Home.Building)) { return false; }
    BuildCareHome(Home.Scene, Home.Building, &Home.Solids);
    const double FloorZ = GroundZ + HomeFloorW;
    for (const FBox& Box : Home.Scene.Blocks.BlockingBoxes())
    {
        double UA = 0.0;
        double VA = 0.0;
        double UB = 0.0;
        double VB = 0.0;
        ToLocal(Home.Building.Frame, Box.Min, UA, VA);
        ToLocal(Home.Building.Frame, Box.Max, UB, VB);
        Home.Blocks.Add(FCBlock{FCRect{FMath::Min(UA, UB), FMath::Max(UA, UB), FMath::Min(VA, VB), FMath::Max(VA, VB)}, Box.Min.Z - FloorZ, Box.Max.Z - FloorZ});
    }
    for (const FLaneDoorSpec& D : Home.Scene.Doors)
    {
        Home.Zones.Add(SwingZone(Home.Building.Frame, D, D.PreferredSwing));
        if (D.bTwoWay) { Home.Zones.Add(SwingZone(Home.Building.Frame, D, -D.PreferredSwing)); }
    }
    return Home.Blocks.Num() > 0 && Home.Solids.Num() > 0 && Home.Scene.Doors.Num() == 4 && Home.Zones.Num() == 6;
}

/** Everything a person walking around bumps into: shell, furniture, the open common leaf and the reserved stair. */
TArray<FCRect> WalkObstacles(const FCHome& Home)
{
    TArray<FCRect> Obstacles;
    for (const FCBlock& Block : Home.Blocks) { if (IsBodyBlock(Block)) { Obstacles.Add(Block.R); } }
    Obstacles.Add(OpenLeafRect(Home));
    Obstacles.Add(FCRect{CareStairSpec().U0, CareStairSpec().U1, CareStairSpec().V0, CareStairSpec().V1});
    return Obstacles;
}

/** Flood fill over a 5 cm grid of the points where a disc of the given radius clears every obstacle and stays inside the region. */
TArray<uint8> FloodRoutes(const TArray<FCRect>& Obstacles, double Radius, const FCRect& Region, double SeedU, double SeedV)
{
    auto IsFree = [&](int32 I, int32 J)
    {
        const double U = 5.0 * I;
        const double V = 5.0 * J;
        bool bFree = I >= 0 && J >= 0 && I < CellsU && J < CellsV && U - Radius >= Region.U0 - Tol && U + Radius <= Region.U1 + Tol &&
                     V - Radius >= Region.V0 - Tol && V + Radius <= Region.V1 + Tol;
        for (int32 K = 0; bFree && K < Obstacles.Num(); ++K)
        {
            bFree = DistToRect(U, V, Obstacles[K]) >= Radius - Tol;
        }
        return bFree;
    };
    TArray<uint8> Reached;
    Reached.Init(0, CellsU * CellsV);
    TArray<FIntPoint> Stack;
    const FIntPoint Start(FMath::RoundToInt(SeedU / 5.0), FMath::RoundToInt(SeedV / 5.0));
    if (IsFree(Start.X, Start.Y))
    {
        Reached[Start.Y * CellsU + Start.X] = 1;
        Stack.Add(Start);
    }
    while (!Stack.IsEmpty())
    {
        const FIntPoint Cell = Stack.Pop();
        for (const FIntPoint& Step : {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)})
        {
            const FIntPoint Next = Cell + Step;
            if (IsFree(Next.X, Next.Y) && Reached[Next.Y * CellsU + Next.X] == 0)
            {
                Reached[Next.Y * CellsU + Next.X] = 1;
                Stack.Add(Next);
            }
        }
    }
    return Reached;
}

bool IsReached(const TArray<uint8>& Reached, double U, double V)
{
    return Reached[FMath::Clamp(FMath::RoundToInt(V / 5.0), 0, CellsV - 1) * CellsU + FMath::Clamp(FMath::RoundToInt(U / 5.0), 0, CellsU - 1)] != 0;
}

/** Distance to the nearest body-height collision box (shell, jambs, furniture), and to the nearest door swing zone. */
double MinDistToBodyBlocks(const FCHome& Home, double U, double V)
{
    double Best = 1.0e9;
    for (const FCBlock& Block : Home.Blocks) { Best = IsBodyBlock(Block) ? FMath::Min(Best, DistToRect(U, V, Block.R)) : Best; }
    return Best;
}

double MinDistToZones(const FCHome& Home, double U, double V)
{
    double Best = 1.0e9;
    for (const FCRect& Zone : Home.Zones) { Best = FMath::Min(Best, DistToRect(U, V, Zone)); }
    return Best;
}

/** The free gap along a wall line at a doorway: from the nearest body-height box on one side of the centre to the other. */
bool MeasureDoorGap(const FCHome& Home, bool bAlongU, double Line, double Centre, double& OutClear, double& OutHeadroom)
{
    double Lo = -1.0e9;
    double Hi = 1.0e9;
    OutHeadroom = 1.0e9;
    for (const FCBlock& Block : Home.Blocks)
    {
        const double A0 = bAlongU ? Block.R.U0 : Block.R.V0;
        const double A1 = bAlongU ? Block.R.U1 : Block.R.V1;
        if (Line < (bAlongU ? Block.R.V0 : Block.R.U0) || Line > (bAlongU ? Block.R.V1 : Block.R.U1)) { continue; }
        if (!IsBodyBlock(Block) && Block.H0 < 100.0) { continue; } // the floor slab and the like
        if (A0 <= Centre && Centre <= A1)
        {
            if (Block.H0 < 100.0)
            {
                return false; // something solid stands in the doorway itself
            }
            OutHeadroom = FMath::Min(OutHeadroom, Block.H0); // the lintel
        }
        else if (IsBodyBlock(Block))
        {
            Lo = A1 <= Centre ? FMath::Max(Lo, A1) : Lo;
            Hi = A0 >= Centre ? FMath::Min(Hi, A0) : Hi;
        }
    }
    OutClear = Hi - Lo;
    return Hi < 1.0e8 && Lo > -1.0e8;
}
} // namespace PWCareTest

// Builds the care home into Home, or fails the test.
#define PW_CARE_HOME() PWCareTest::FCHome Home; if (!TestTrue(TEXT("the care home builds with four doors"), PWCareTest::BuildCare(Home))) { return false; }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWCareSelectionTest, "PerfectWorld.CareHome.BuildingSelection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWCareSelectionTest::RunTest(const FString& Parameters)
{
    using namespace PWCareTest;
    // The generator values documented in the header: LaneHash(-124, 1, 11) and what BuildFirstRows derives from it.
    const uint32 H = LaneHash(-1 * 131 + 7, 1, 11);
    TestEqual(TEXT("the generator hash"), H, CareGeneratorSeed);
    TestTrue(TEXT("width 1100, depth 900, no setback, three storeys, butter wall, no extra gap after it"),
             600.0 + 100.0 * static_cast<double>(H % 6u) == CareLayout::Width && 700.0 + 100.0 * static_cast<double>((H >> 16) % 3u) == CareLayout::Depth &&
                 (H >> 12) % 4u == 3u && (H >> 8) % 3u == 0u && (H >> 20) % 8u == static_cast<uint32>(ELaneColour::WallButter) && (H >> 24) % 7u != 0u);

    const FLaneBuilding Reference = MakeCareBuilding();
    TestTrue(TEXT("the reference building is the care home (at least 900 wide), and neither the opening home nor the tea shop"),
             IsCareHomeBuilding(Reference) && Reference.Width >= 900.0 && !IsOpeningHome(Reference) && !IsTeaShopBuilding(Reference));
    FLaneBuilding Others[5] = {Reference, Reference, Reference, Reference, Reference};
    Others[0].Side = 1; Others[1].Index = 2; Others[2].Width = 1000.0; Others[3].Setback = 60.0; Others[4].Frame.Origin.X = -300.0;
    for (const FLaneBuilding& Other : Others) { TestFalse(TEXT("any other side, index, width, setback or position is not the care home"), IsCareHomeBuilding(Other)); }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWCareDoorsTest, "PerfectWorld.CareHome.DoorsAndSwingZones", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWCareDoorsTest::RunTest(const FString& Parameters)
{
    using namespace PWCareTest;
    PW_CARE_HOME();
    struct FProbe { const TCHAR* Id; bool bAlongU; double Line; double Centre; };
    const FProbe Probes[] = {
        {CareDoorFrontId, true, 12.5, CareLayout::FrontDoorU},
        {CareDoorCommonId, false, CareLayout::EntryEastU, CareLayout::CommonDoorV},
        {CareDoorDormitoryId, true, CareLayout::AcrossV, CareLayout::DormDoorU},
        {CareDoorWashroomId, true, CareLayout::AcrossV, CareLayout::WashDoorU},
    };
    for (const FProbe& Probe : Probes)
    {
        double Clear = 0.0;
        double Headroom = 0.0;
        if (TestTrue(*FString::Printf(TEXT("%s: the doorway can be measured"), Probe.Id), MeasureDoorGap(Home, Probe.bAlongU, Probe.Line, Probe.Centre, Clear, Headroom)))
        {
            TestTrue(*FString::Printf(TEXT("%s: clear width %.1f cm (at least 90) and height %.1f cm (at least 210)"), Probe.Id, Clear, Headroom), Clear >= 90.0 && Headroom >= 210.0);
        }
    }
    TestTrue(TEXT("HasCareHome, with the front door (as the core registers it) and the common door one-way"),
             HasCareHome(Home.Scene) && !DoorOf(Home, CareDoorFrontId).bTwoWay && !DoorOf(Home, CareDoorCommonId).bTwoWay);

    // Nothing stands in any swing zone, including the reserved stair.
    const FHomeStairSpec Stair = CareStairSpec();
    for (const FCRect& Zone : Home.Zones)
    {
        TestFalse(TEXT("the reserved stair overlaps a swing zone"), Overlaps(Zone, FCRect{Stair.U0, Stair.U1, Stair.V0, Stair.V1}));
        for (const FHomeSolid& Solid : Home.Solids) { TestFalse(*FString::Printf(TEXT("furniture U %.0f-%.0f V %.0f-%.0f stands in the swing zone U %.0f-%.0f V %.0f-%.0f"), Solid.U0, Solid.U1, Solid.V0, Solid.V1, Zone.U0, Zone.U1, Zone.V0, Zone.V1), Overlaps(Zone, RectOf(Solid))); }
    }
    // The common door opens into the entry only, its open leaf against the across partition (face at V 501); the front door's zone stays in the entry.
    const FLaneDoorSpec& Common = DoorOf(Home, CareDoorCommonId);
    const FCRect Leaf = OpenLeafRect(Home);
    TestTrue(TEXT("the common door's swing is on the entry side only"), SwingZone(Home.Building.Frame, Common, Common.PreferredSwing).U1 <= CareLayout::EntryEastU + 6.0);
    TestTrue(TEXT("the open common leaf lies against the across partition"), Leaf.V1 >= CareLayout::AcrossV - 31.0 && Leaf.U1 <= CareLayout::EntryEastU + 6.0);
    const FLaneDoorSpec& Front = DoorOf(Home, CareDoorFrontId);
    const FCRect FrontZone = SwingZone(Home.Building.Frame, Front, Front.PreferredSwing);
    TestTrue(TEXT("the front door swings into the building and stays in the entry"), FrontZone.V1 > 25.0 && FrontZone.V0 >= 0.0 && FrontZone.U0 >= CareLayout::Entry.U0 && FrontZone.U1 <= CareLayout::Entry.U1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWCareRoutesTest, "PerfectWorld.CareHome.RouteWidths", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWCareRoutesTest::RunTest(const FString& Parameters)
{
    using namespace PWCareTest;
    PW_CARE_HOME();
    const TArray<FCRect> Obstacles = WalkObstacles(Home);
    const FCRect Floor{25.0, 1075.0, 25.0, 875.0};
    const FCareStaffSpot Staff = CareStaffSpotLocal();
    struct FTarget { const TCHAR* Name; double U; double V; };
    TArray<FTarget> Targets = {
        {TEXT("front door, inside"), CareLayout::FrontDoorU, 85.0},
        {TEXT("entry, middle"), 235.0, 300.0},
        {TEXT("entry, in front of the common doorway"), 430.0, 390.0},
        {TEXT("common doorway"), CareLayout::EntryEastU, CareLayout::CommonDoorV},
        {TEXT("common room, behind the study benches"), 700.0, 175.0},
        {TEXT("common room, the aisle"), 960.0, 430.0},
        {TEXT("dormitory door"), CareLayout::DormDoorU, CareLayout::AcrossV},
        {TEXT("washroom door"), CareLayout::WashDoorU, CareLayout::AcrossV},
        {TEXT("dormitory, front strip (west)"), 250.0, 580.0},
        {TEXT("dormitory, front strip (east)"), 700.0, 580.0},
        {TEXT("washroom, middle"), 950.0, 700.0},
        {TEXT("washroom, at the basins"), 980.0, 610.0},
        {TEXT("washroom, in front of the toilet"), 890.0, 750.0},
        {TEXT("care worker spot"), Staff.U, Staff.V},
        {TEXT("trunk, standing point"), CareStorageSpec().StandU, CareStorageSpec().StandV},
    };
    for (const FHomeSeatSpec& Seat : CareSeatSpecs()) { Targets.Add(FTarget{TEXT("seat, stand-up point"), Seat.StandU, Seat.StandV}); }
    for (const FHomeReadableSpec& Paper : CareReadableSpecs()) { Targets.Add(FTarget{TEXT("readable, reading spot"), Paper.StandU, Paper.StandV}); }

    // 90 cm routes (a 45 cm disc) from the front door to every part of the home, every door and every anchor; then again with
    // the care worker standing at their spot (a 34 cm capsule).
    TArray<FCRect> WithStaff = Obstacles;
    WithStaff.Add(FCRect{Staff.U - HomePersonRadiusCm, Staff.U + HomePersonRadiusCm, Staff.V - HomePersonRadiusCm, Staff.V + HomePersonRadiusCm});
    const TArray<uint8> Reach90 = FloodRoutes(Obstacles, 45.0, Floor, CareLayout::FrontDoorU, 85.0);
    const TArray<uint8> ReachStaff = FloodRoutes(WithStaff, 45.0, Floor, CareLayout::FrontDoorU, 85.0);
    for (const FTarget& Target : Targets)
    {
        TestTrue(*FString::Printf(TEXT("a 90 cm route from the front door to: %s (U %.0f, V %.0f)"), Target.Name, Target.U, Target.V), IsReached(Reach90, Target.U, Target.V));
        const bool bIsStaff = Target.U == Staff.U && Target.V == Staff.V;
        TestTrue(*FString::Printf(TEXT("with the care worker in place, a 90 cm route to: %s"), Target.Name), bIsStaff || IsReached(ReachStaff, Target.U, Target.V));
    }

    // 120 cm corridors (a 60 cm disc): in the entry to the doorway's approach, along the common room's aisle to the washroom door,
    // and along the dormitory's front strip. The doorways themselves are 94 cm clear, like every door.
    const TArray<uint8> Entry120 = FloodRoutes(Obstacles, 60.0, ToRect(CareLayout::Entry), CareLayout::FrontDoorU, 85.0);
    const TArray<uint8> Common120 = FloodRoutes(Obstacles, 60.0, ToRect(CareLayout::Common), 590.0, 430.0);
    const TArray<uint8> Dorm120 = FloodRoutes(Obstacles, 60.0, ToRect(CareLayout::Dormitory), 120.0, 575.0);
    TestTrue(TEXT("a 120 cm route in the entry to the doorway's approach, along the common room's aisle, and along the dormitory's front strip"),
             IsReached(Entry120, 235.0, 300.0) && IsReached(Entry120, 430.0, 390.0) && IsReached(Common120, 850.0, 430.0) && IsReached(Common120, 950.0, 430.0) &&
                 IsReached(Dorm120, 250.0, 575.0) && IsReached(Dorm120, 700.0, 575.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWCareAnchorsTest, "PerfectWorld.CareHome.StaffSpotAndAnchors", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWCareAnchorsTest::RunTest(const FString& Parameters)
{
    using namespace PWCareTest;
    PW_CARE_HOME();
    const FCareStaffSpot Spot = CareStaffSpotLocal();
    TestTrue(TEXT("the spot is at most 150 cm from the doorway's threshold, which is the doorway's centre"),
             FMath::Sqrt(FMath::Square(Spot.FaceU - Spot.U) + FMath::Square(Spot.FaceV - Spot.V)) <= 150.0 &&
                 FMath::IsNearlyEqual(Spot.FaceU, CareLayout::EntryEastU) && FMath::IsNearlyEqual(Spot.FaceV, CareLayout::CommonDoorV));
    TestTrue(TEXT("the spot is in the common room"), DistToRect(Spot.U, Spot.V, ToRect(CareLayout::Common)) <= Tol);
    TestTrue(TEXT("45 cm clear of every wall, jamb and furniture box, outside every door swing and the open common leaf"),
             MinDistToBodyBlocks(Home, Spot.U, Spot.V) >= 45.0 && MinDistToZones(Home, Spot.U, Spot.V) >= 45.0 && DistToRect(Spot.U, Spot.V, OpenLeafRect(Home)) >= 45.0);
    for (const double U : {CareLayout::EntryEastU - 30.0, CareLayout::EntryEastU}) // a player stepping through, facing east
    {
        for (double V = CareLayout::CommonDoorV - 35.0; V <= CareLayout::CommonDoorV + 35.0; V += 10.0)
        {
            const double Distance = FMath::Sqrt(FMath::Square(Spot.U - U) + FMath::Square(Spot.V - V));
            const double Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp((Spot.U - U) / Distance, -1.0, 1.0)));
            TestTrue(*FString::Printf(TEXT("from the doorway at U %.0f V %.0f the care worker is %.0f cm away at %.0f degrees (limits 200, 50)"), U, V, Distance, Angle), Distance <= 200.0 && Angle <= 50.0);
        }
    }
    const FLaneFrame& F = Home.Building.Frame;
    const double Yaw = FMath::DegreesToRadians(CareStaffSpotYaw(F));
    TestTrue(TEXT("the care worker's yaw faces the doorway, and the world point stands on the finished floor"),
             FVector::DotProduct(FVector(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.0), (CareWorldPoint(F, Spot.FaceU, Spot.FaceV) - CareStaffSpot(F)).GetSafeNormal2D()) > 0.999 &&
                 FMath::IsNearlyEqual(CareStaffSpot(F).Z, GroundZ + HomeFloorW, 0.01));

    // Seats, readables and the trunk. OnSolid: a solid piece contains (U, V) and its top is at the height (or, above, within 3 cm).
    auto OnSolid = [&](double U, double V, double Height, bool bAbove)
    {
        for (const FHomeSolid& Solid : Home.Solids)
        {
            const bool bHeight = bAbove ? (Height > Solid.H1 && Height - Solid.H1 <= 3.0) : FMath::Abs(Solid.H1 - Height) <= 1.0;
            if (U >= Solid.U0 && U <= Solid.U1 && V >= Solid.V0 && V <= Solid.V1 && bHeight) { return true; }
        }
        return false;
    };
    auto StandOk = [&](const FString& Name, double StandU, double StandV, double Clear, double FromU, double FromV)
    {
        TestTrue(*(Name + TEXT(": the stand point has 45 cm of clear floor, is outside every door swing and within 120 cm of the object")),
                 Clear >= 45.0 && MinDistToBodyBlocks(Home, StandU, StandV) >= Clear && MinDistToZones(Home, StandU, StandV) >= 45.0 &&
                     FMath::Sqrt(FMath::Square(StandU - FromU) + FMath::Square(StandV - FromV)) <= 120.0);
    };
    TestTrue(TEXT("at least three seats, a noticeboard and a book"), CareSeatSpecs().Num() >= 3 && CareReadableSpecs().Num() >= 2);
    for (const FHomeSeatSpec& Seat : CareSeatSpecs())
    {
        const FString Name = Seat.Id.ToString();
        TestTrue(*(Name + TEXT(": on top of a solid piece at the stated seat height, facing a unit vector")),
                 OnSolid(Seat.SeatU, Seat.SeatV, Seat.SeatHeightCm, false) && FMath::IsNearlyEqual(FMath::Sqrt(Seat.FaceU * Seat.FaceU + Seat.FaceV * Seat.FaceV), 1.0, 1.0e-6));
        StandOk(Name, Seat.StandU, Seat.StandV, Seat.StandClearCm, Seat.SeatU, Seat.SeatV);
    }
    for (const FHomeReadableSpec& Paper : CareReadableSpecs())
    {
        const FString Name = Paper.Id.ToString();
        const double ToWall = FMath::Min(FMath::Min(Paper.U - 25.0, 1075.0 - Paper.U), FMath::Min(Paper.V - 25.0, 875.0 - Paper.V));
        TestTrue(*(Name + TEXT(": ids set, and it lies on a surface (low) or is a wall sheet at reading height (high)")),
                 !Paper.Id.IsNone() && !Paper.ContentId.IsNone() && (Paper.HeightCm < 100.0 ? OnSolid(Paper.U, Paper.V, Paper.HeightCm, true) : (ToWall <= 5.0 && Paper.HeightCm <= 175.0)));
        StandOk(Name, Paper.StandU, Paper.StandV, Paper.StandClearCm, Paper.U, Paper.V);
    }
    const FCareStorageSpec Trunk = CareStorageSpec();
    TestTrue(TEXT("the resident's trunk is a solid piece of the stated height, in the dormitory"),
             OnSolid(Trunk.U, Trunk.V, Trunk.HeightCm, false) && DistToRect(Trunk.U, Trunk.V, ToRect(CareLayout::Dormitory)) <= Tol);
    StandOk(Trunk.Id.ToString(), Trunk.StandU, Trunk.StandV, Trunk.StandClearCm, Trunk.U, Trunk.V);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWCareRoomsTest, "PerfectWorld.CareHome.RoomsStairAndPersonalObjects", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWCareRoomsTest::RunTest(const FString& Parameters)
{
    using namespace PWCareTest;
    PW_CARE_HOME();
    const FCareRect Rooms[] = {CareLayout::Entry, CareLayout::Common, CareLayout::Dormitory, CareLayout::Washroom};
    int32 SolidsIn[4] = {0, 0, 0, 0};
    int32 PerRoom[4] = {0, 0, 0, 0};
    int32 Trunks = 0;
    int32 Mattresses = 0;
    for (const FHomeSolid& Solid : Home.Solids)
    {
        int32 Inside = 0;
        for (int32 I = 0; I < 4; ++I)
        {
            const bool bIn = Solid.U0 >= Rooms[I].U0 && Solid.U1 <= Rooms[I].U1 && Solid.V0 >= Rooms[I].V0 && Solid.V1 <= Rooms[I].V1;
            Inside += bIn ? 1 : 0;
            SolidsIn[I] += bIn ? 1 : 0;
        }
        TestEqual(*FString::Printf(TEXT("the solid piece U %.0f-%.0f V %.0f-%.0f lies wholly inside one room"), Solid.U0, Solid.U1, Solid.V0, Solid.V1), Inside, 1);
        Trunks += FMath::IsNearlyEqual(Solid.U1 - Solid.U0, 50.0) && FMath::IsNearlyEqual(Solid.V1 - Solid.V0, 40.0) && FMath::IsNearlyEqual(Solid.H1, 50.0) ? 1 : 0;
        Mattresses += FMath::IsNearlyEqual(Solid.U1 - Solid.U0, 86.0) && FMath::IsNearlyEqual(Solid.V1 - Solid.V0, 186.0) ? 1 : 0;
    }
    for (const FCarePersonalObject& Object : GetCarePersonalObjects())
    {
        const int32 Room = FMath::Clamp(static_cast<int32>(Object.Room), 0, 3);
        ++PerRoom[Room];
        TestTrue(*FString::Printf(TEXT("%s lies inside its room"), Object.Id), DistToRect(Object.U, Object.V, ToRect(Rooms[Room])) <= Tol);
    }
    for (int32 I = 0; I < 4; ++I) { TestTrue(*FString::Printf(TEXT("room %d has at least four solid pieces and a personal object (as each room of the family home)"), I), SolidsIn[I] >= 4 && PerRoom[I] >= 1); }
    TestEqual(TEXT("five beds in the dormitory"), Mattresses, 5);
    TestEqual(TEXT("each with a trunk at its foot"), Trunks, 5);
    for (const FCarePiece& Piece : GetCarePieces()) // inside the 25 cm walls, above the floor and below the ceiling
    {
        TestTrue(TEXT("a piece lies inside the shell"), Piece.U0 >= 25.0 && Piece.U1 <= 1075.0 && Piece.V0 >= 25.0 && Piece.V1 <= 875.0 && Piece.H0 >= 0.0 && Piece.H1 <= 274.0 && Piece.H1 > Piece.H0);
    }

    // The reserved stair: the home's dog-leg, kept clear, with the corridor past its foot at least 120 cm.
    const FHomeStairSpec Stair = CareStairSpec();
    TestTrue(TEXT("the stair lies in the entry"), Stair.U0 >= CareLayout::Entry.U0 && Stair.U1 <= CareLayout::Entry.U1 && Stair.V0 >= CareLayout::Entry.V0 && Stair.V1 <= CareLayout::Entry.V1);
    TestTrue(TEXT("two lanes of at least 90 cm, 17 equal risers of the storey height, 25 cm treads"),
             Stair.LaneWidthCm >= 90.0 && Stair.U1 - Stair.U0 >= 180.0 && Stair.Risers == 17 && FMath::IsNearlyEqual(Stair.RiserCm * Stair.Risers, Stair.FloorToFloorCm, 0.01) && FMath::IsNearlyEqual(Stair.TreadCm, 25.0));
    TestTrue(TEXT("the flights meet the landing and the upper floor"),
             FMath::IsNearlyEqual(Stair.FootV - 8.0 * Stair.TreadCm, Stair.LandingV1, 0.01) && Stair.LandingV1 - Stair.LandingV0 >= 90.0 && FMath::IsNearlyEqual(Stair.LandingV1 + 7.0 * Stair.TreadCm, Stair.UpperEdgeV, 0.01));
    TestTrue(TEXT("the corridor between the stair's foot and the across partition is 120 cm or more"), CareLayout::AcrossV - 6.0 - Stair.V1 >= 120.0);
    for (const FCBlock& Block : Home.Blocks) { TestFalse(TEXT("nothing solid stands in the reserved stair footprint"), IsBodyBlock(Block) && Overlaps(Block.R, FCRect{Stair.U0, Stair.U1, Stair.V0, Stair.V1})); }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWCareLightingTest, "PerfectWorld.CareHome.FansAndLights", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWCareLightingTest::RunTest(const FString& Parameters)
{
    using namespace PWCareTest;
    PW_CARE_HOME();
    const double FloorZ = GroundZ + HomeFloorW;
    const TArray<FHomeLightSpec> Specs = CareLightSpecs();
    const TArray<FHomeFanSpec> Fans = CareFanSpecs();
    TestTrue(TEXT("a modest light count"), Specs.Num() >= 5 && Specs.Num() <= 9);
    if (!TestEqual(TEXT("Scene.RoomLights matches the light specs"), Home.Scene.RoomLights.Num(), Specs.Num())) { return false; }
    for (int32 I = 0; I < Specs.Num(); ++I)
    {
        const FHomeLightSpec& Spec = Specs[I];
        const FLaneRoomLight& Light = Home.Scene.RoomLights[I];
        double U = 0.0;
        double V = 0.0;
        ToLocal(Home.Building.Frame, Light.Position, U, V);
        const FString Name = FString::Printf(TEXT("%s light %d"), *Spec.Room.ToString(), I);
        TestTrue(*(Name + TEXT(" is published at its spec position, radius and lumens")),
                 FMath::IsNearlyEqual(U, Spec.U, 0.01) && FMath::IsNearlyEqual(V, Spec.V, 0.01) && FMath::IsNearlyEqual(Light.Radius, Spec.AttenuationCm) && FMath::IsNearlyEqual(Light.Lumens, Spec.Lumens));
        TestTrue(*(Name + TEXT(" casts shadows and hangs below the ceiling")), Spec.bCastShadows && Light.Position.Z > FloorZ + 200.0 && Light.Position.Z < GroundZ + HomeCeilingW);
        TestTrue(*(Name + TEXT(" is 20 cm or more inside its room and reaches no farther than the room's shorter side")),
                 U >= Spec.RoomU0 + 20.0 && U <= Spec.RoomU1 - 20.0 && V >= Spec.RoomV0 + 20.0 && V <= Spec.RoomV1 - 20.0 && Spec.AttenuationCm <= FMath::Min(Spec.RoomU1 - Spec.RoomU0, Spec.RoomV1 - Spec.RoomV0) + Tol);
        for (const FHomeFanSpec& Fan : Fans) { TestTrue(*(Name + TEXT(" clears every fan's blades")), FMath::Sqrt(FMath::Square(U - Fan.U) + FMath::Square(V - Fan.V)) >= Fan.RadiusCm + 10.0); }
    }
    TestTrue(TEXT("a fan in the entry, the common room and the dormitory"), Fans.Num() >= 3);
    for (const FHomeFanSpec& Fan : Fans)
    {
        const FString Where = FString::Printf(TEXT("fan at U %.0f V %.0f"), Fan.U, Fan.V);
        bool bInRoom = false;
        for (const FHomeLightSpec& Room : Specs) { bInRoom |= Fan.U - Fan.RadiusCm >= Room.RoomU0 && Fan.U + Fan.RadiusCm <= Room.RoomU1 && Fan.V - Fan.RadiusCm >= Room.RoomV0 && Fan.V + Fan.RadiusCm <= Room.RoomV1; }
        TestTrue(*(Where + TEXT(": underside at least 240 cm above the floor, turning clear of the walls")), Fan.UndersideCm >= 240.0 && bInRoom);
        for (const FHomeSolid& Solid : Home.Solids) { TestFalse(*FString::Printf(TEXT("%s: furniture up to %.0f cm high is under the blades"), *Where, Solid.H1), DistToRect(Fan.U, Fan.V, RectOf(Solid)) < Fan.RadiusCm && Solid.H1 > Fan.UndersideCm - 55.0); }
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
