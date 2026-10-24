// Layout checks for Shanti Chai Corner (PerfectWorldLaneTeaShop.h): which generated building it is, that the furniture and
// walls leave the staff and customer routes clear, that the work anchors are usable, and that the build keeps the footpath
// route and the entrance open. These read the layout data and run the builder on a bare scene; no world is needed.
// Run in the editor under "PerfectWorld.TeaShop" (Session Frontend, or the Unreal MCP AutomationTestToolset).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "PerfectWorldLaneTeaShop.h"

namespace PWTeaShopTest
{
using namespace PWLane;

constexpr double CapsuleRadius = 34.0; // the walking capsule is 68 wide
constexpr double WalkingHeight = 165.0;

FLaneBuilding MakeBuilding()
{
    FLaneBuilding B;
    B.Frame = MakeLaneFrame(600.0, 520.0, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0));
    B.Width = 700.0;
    B.Depth = 900.0;
    B.Setback = 120.0;
    B.Storeys = 3;
    B.Side = 1;
    B.Index = 3;
    B.Detail = ELaneDetail::Full;
    B.Seed = TeaShopGeneratorSeed;
    B.Wall = ELaneColour::WallTerracotta;
    return B;
}

double RectDistance(const FVector2D& P, double U0, double U1, double V0, double V1)
{
    const double Du = FMath::Max3(U0 - P.X, 0.0, P.X - U1);
    const double Dv = FMath::Max3(V0 - P.Y, 0.0, P.Y - V1);
    return FMath::Sqrt(Du * Du + Dv * Dv);
}

/** Solid pieces a walking capsule can meet (anything that starts below head height). */
TArray<FTeaShopPiece> WalkingSolids()
{
    TArray<FTeaShopPiece> Out;
    for (const FTeaShopPiece& P : GetTeaShopPieces())
    {
        if (P.bSolid && P.H0 < WalkingHeight)
        {
            Out.Add(P);
        }
    }
    return Out;
}

/** The walls as blocking rectangles: the front wall without the 94 cm entrance, the sides, the back, the partition without its doorway. */
TArray<FTeaShopRect> WallRects()
{
    using namespace TeaShopLayout;
    const double D0 = DoorCentreU - DoorHalfClear;
    const double D1 = DoorCentreU + DoorHalfClear;
    const double P0 = PartitionDoorCentreU - PartitionDoorHalfClear;
    const double P1 = PartitionDoorCentreU + PartitionDoorHalfClear;
    return {{-100.0, 25.0, -100.0, 1000.0}, {675.0, 800.0, -100.0, 1000.0}, {0.0, D0, 0.0, 25.0},
            {D1, 700.0, 0.0, 25.0},         {0.0, 700.0, 875.0, 1000.0},    {25.0, P0, PartitionV0, PartitionV1},
            {P1, 675.0, PartitionV0, PartitionV1}};
}

double Clearance(const FVector2D& P, const TArray<FTeaShopPiece>& Solids, const TArray<FTeaShopRect>& Walls)
{
    double Best = 1.0e9;
    for (const FTeaShopPiece& S : Solids)
    {
        Best = FMath::Min(Best, RectDistance(P, S.U0, S.U1, S.V0, S.V1));
    }
    for (const FTeaShopRect& W : Walls)
    {
        Best = FMath::Min(Best, RectDistance(P, W.U0, W.U1, W.V0, W.V1));
    }
    return Best;
}
} // namespace PWTeaShopTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWTeaShopSelectionTest, "PerfectWorld.TeaShop.Selection",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPWTeaShopSelectionTest::RunTest(const FString& Parameters)
{
    using namespace PWLane;
    // The generator values the building is chosen by (BuildFirstRows: side +1, Index 3).
    const uint32 H = LaneHash(1 * 131 + 7, 3, 11);
    TestTrue(TEXT("the hash of side +1, index 3 is the documented seed"), H == TeaShopGeneratorSeed);
    TestTrue(TEXT("width 600 + 100 * (H % 6) = 700"), 600.0 + 100.0 * static_cast<double>(H % 6u) == 700.0);
    TestTrue(TEXT("depth 700 + 100 * ((H >> 16) % 3) = 900"), 700.0 + 100.0 * static_cast<double>((H >> 16) % 3u) == 900.0);
    TestTrue(TEXT("setback kind (H >> 12) % 4 = 0 gives a 120 cm setback"), ((H >> 12) % 4u) == 0u);
    TestTrue(TEXT("3 + (H >> 8) % 3 = 3 storeys"), 3 + static_cast<int32>((H >> 8) % 3u) == 3);

    // The real generator still places a first-row +Y building at X 600 to 1300 with its front at |Y| 520.
    FLaneScene Lane;
    BuildLaneRows(Lane);
    bool bFound = false;
    for (const FLaneFront& Front : Lane.Fronts[1])
    {
        bFound = bFound || (FMath::IsNearlyEqual(Front.X0, 600.0, 0.5) && FMath::IsNearlyEqual(Front.X1, 1300.0, 0.5) &&
                            FMath::IsNearlyEqual(Front.AbsY, 520.0, 0.5));
    }
    TestTrue(TEXT("the generated +Y row has a building at X 600 to 1300 with its front at Y 520"), bFound);

    FLaneBuilding B = PWTeaShopTest::MakeBuilding();
    TestTrue(TEXT("the reference building is the tea shop"), IsTeaShopBuilding(B));
    FLaneBuilding Other = B;
    Other.Side = -1;
    TestFalse(TEXT("the -Y side is not"), IsTeaShopBuilding(Other));
    Other = B;
    Other.Index = 2;
    TestFalse(TEXT("index 2 is not"), IsTeaShopBuilding(Other));
    Other = B;
    Other.Width = 600.0;
    TestFalse(TEXT("another width is not"), IsTeaShopBuilding(Other));
    Other = B;
    Other.Setback = 60.0;
    TestFalse(TEXT("another setback is not"), IsTeaShopBuilding(Other));
    Other = B;
    Other.Frame = MakeLaneFrame(1400.0, 520.0, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0));
    TestFalse(TEXT("another position is not"), IsTeaShopBuilding(Other));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWTeaShopLayoutTest, "PerfectWorld.TeaShop.Layout",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPWTeaShopLayoutTest::RunTest(const FString& Parameters)
{
    using namespace PWLane;
    using namespace TeaShopLayout;
    const TConstArrayView<FTeaShopPiece> Pieces = GetTeaShopPieces();
    TestTrue(TEXT("there is furniture"), Pieces.Num() > 40);

    // Rooms sit inside the walls and meet at the counter line and the partition.
    TestTrue(TEXT("customer room meets the counter"), CustomerRoom.V1 == ServiceCounter.V0);
    TestTrue(TEXT("prep area starts at the counter's back"), PrepArea.V0 == ServiceCounter.V1);
    TestTrue(TEXT("prep area ends at the partition"), PrepArea.V1 == PartitionV0);
    TestTrue(TEXT("back room starts after the partition"), BackRoom.V0 == PartitionV1);
    TestTrue(TEXT("rooms inside the 25 cm walls"), CustomerRoom.U0 >= Interior.U0 && BackRoom.U1 <= Interior.U1 && BackRoom.V1 <= Interior.V1);
    TestTrue(TEXT("ceiling leaves 274 cm clear"), CeilingW - FloorW == 274.0);

    // Every piece is inside a room or on the footpath seating strip, and no solid stands in the entrance swing zone.
    for (const FTeaShopPiece& P : Pieces)
    {
        const bool bOutdoor = P.V0 < 0.0;
        const bool bInside = P.U0 >= Interior.U0 && P.U1 <= Interior.U1 && P.V0 >= Interior.V0 && P.V1 <= Interior.V1;
        const bool bOnFootpath = bOutdoor && P.V1 <= 0.0 && P.V0 >= OutdoorSeating.V0 - 1.0 && P.U0 >= OutdoorSeating.U0 - 1.0 && P.U1 <= OutdoorSeating.U1;
        if (!bInside && !bOnFootpath)
        {
            AddError(FString::Printf(TEXT("a piece at U %.0f-%.0f V %.0f-%.0f is outside the rooms and the seating strip"), P.U0, P.U1, P.V0, P.V1));
        }
        if (P.bSolid && P.U1 > ShutterHingeU - 90.0 && P.U0 < ShutterHingeU && P.V1 > 0.0 && P.V0 < 105.0)
        {
            AddError(TEXT("a solid piece stands in the shutter leaf's swing zone"));
        }
        if (P.bSolid && P.U1 > DoorCentreU - 60.0 && P.U0 < DoorCentreU + 60.0 && P.V0 > -100.0 && P.V1 < 110.0 && P.H0 < 165.0)
        {
            AddError(TEXT("a solid piece blocks the entrance approach"));
        }
    }

    // Solids do not interpenetrate (stacking is fine), and nothing standing on the floor can be stepped onto (the player steps up 45).
    const TArray<FTeaShopPiece> Solids = PWTeaShopTest::WalkingSolids();
    for (int32 I = 0; I < Solids.Num(); ++I)
    {
        const FTeaShopPiece& A = Solids[I];
        double ColumnTop = A.H1;
        for (int32 J = 0; J < Solids.Num(); ++J)
        {
            const FTeaShopPiece& B = Solids[J];
            const bool bFootprint = A.U0 < B.U1 && B.U0 < A.U1 && A.V0 < B.V1 && B.V0 < A.V1;
            if (bFootprint)
            {
                ColumnTop = FMath::Max(ColumnTop, B.H1);
                if (J > I && FMath::Min(A.H1, B.H1) - FMath::Max(A.H0, B.H0) > 0.01)
                {
                    AddError(FString::Printf(TEXT("solids overlap near U %.0f V %.0f"), A.U0, A.V0));
                }
            }
        }
        if (A.H0 == 0.0 && ColumnTop <= 46.0)
        {
            AddError(FString::Printf(TEXT("a solid at U %.0f V %.0f is low enough to be stepped onto"), A.U0, A.V0));
        }
    }

    // The service counter: 90 high, the whole customer side of the staff area from the west wall to the staff access, which is 100 clear.
    bool bCounter = false;
    for (const FTeaShopPiece& P : Solids)
    {
        bCounter = bCounter || (P.U0 == ServiceCounter.U0 && P.U1 == ServiceCounter.U1 && P.V0 == ServiceCounter.V0 &&
                                P.V1 == ServiceCounter.V1 && P.H1 == CounterTopH);
        if (P.U1 > StaffGap.U0 && P.U0 < StaffGap.U1 && P.V1 > StaffGap.V0 && P.V0 < StaffGap.V1)
        {
            AddError(TEXT("a solid stands in the staff access"));
        }
    }
    TestTrue(TEXT("a solid counter with its top 90 above the floor spans the staff boundary"), bCounter);
    TestTrue(TEXT("the counter reaches the west wall"), ServiceCounter.U0 == Interior.U0);
    TestTrue(TEXT("the staff access starts where the counter ends and reaches the east wall"),
             StaffGap.U0 == ServiceCounter.U1 && StaffGap.U1 == Interior.U1);
    TestTrue(TEXT("the staff access is at least 90 clear"), StaffGap.U1 - StaffGap.U0 >= 90.0);
    TestTrue(TEXT("the partition doorway is at least 90 clear"), 2.0 * PartitionDoorHalfClear >= 90.0);
    TestTrue(TEXT("the entrance is 94 clear"), 2.0 * DoorHalfClear == 94.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWTeaShopAnchorsTest, "PerfectWorld.TeaShop.Anchors",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPWTeaShopAnchorsTest::RunTest(const FString& Parameters)
{
    using namespace PWLane;
    using namespace TeaShopLayout;
    const FLaneBuilding B = PWTeaShopTest::MakeBuilding();
    TArray<FTeaShopAnchor> Anchors;
    GetTeaShopAnchors(B, Anchors);
    TestTrue(TEXT("seven anchors"), Anchors.Num() == 7);

    const TArray<FTeaShopPiece> Solids = PWTeaShopTest::WalkingSolids();
    const TArray<FTeaShopRect> Walls = PWTeaShopTest::WallRects();
    struct FExpect
    {
        const TCHAR* Id;
        const FTeaShopRect* Room;
    };
    const FExpect Expected[] = {{TeaShopAnchorCounter, &PrepArea}, {TeaShopAnchorStove, &PrepArea}, {TeaShopAnchorWash, &PrepArea},
                                {TeaShopAnchorStorage, &BackRoom}, {TeaShopAnchorBenchWipe, &CustomerRoom}, {TeaShopAnchorRest, &BackRoom}};
    for (const FExpect& E : Expected)
    {
        const FTeaShopAnchor* Found = Anchors.FindByPredicate([&E](const FTeaShopAnchor& A) { return A.Id == FName(E.Id); });
        if (Found == nullptr)
        {
            AddError(FString::Printf(TEXT("missing anchor %s"), E.Id));
            continue;
        }
        TestTrue(FString::Printf(TEXT("%s stands in its room"), E.Id), E.Room->Contains(Found->Local.X, Found->Local.Y));
        TestTrue(FString::Printf(TEXT("%s keeps the capsule clear of furniture and walls"), E.Id),
                 PWTeaShopTest::Clearance(Found->Local, Solids, Walls) >= PWTeaShopTest::CapsuleRadius);
    }
    const FTeaShopAnchor* Door = Anchors.FindByPredicate([](const FTeaShopAnchor& A) { return A.Id == FName(TeaShopAnchorDoor); });
    if (Door != nullptr)
    {
        TestTrue(TEXT("the door anchor is outside the entrance, on the footpath"), Door->Local.Y < 0.0 && FMath::IsNearlyEqual(Door->Local.X, DoorCentreU));
    }
    for (const FTeaShopAnchor& A : Anchors)
    {
        const FVector Expect = B.Frame.Pt(A.Local.X, A.Local.Y, FloorW);
        TestTrue(TEXT("world position follows the building frame"), A.World.Equals(Expect, 0.01));
        TestTrue(TEXT("world Z is the finished floor"), FMath::IsNearlyEqual(A.World.Z, GroundZ + FootpathHeight));
        TestTrue(TEXT("facing is a unit vector"), FMath::IsNearlyEqual(A.Facing.Size(), 1.0, 0.001));
        const FVector Dir = B.Frame.U * A.Facing.X + B.Frame.V * A.Facing.Y;
        const double YawRadians = FMath::DegreesToRadians(A.WorldYawDegrees);
        TestTrue(TEXT("yaw points along the facing"), FMath::IsNearlyEqual(Dir.X, FMath::Cos(YawRadians), 0.001) && FMath::IsNearlyEqual(Dir.Y, FMath::Sin(YawRadians), 0.001));
    }
    // The counter anchor serves customers across the counter; the stove anchor faces the stove platform; wash faces the basin.
    const FTeaShopAnchor* Counter = Anchors.FindByPredicate([](const FTeaShopAnchor& A) { return A.Id == FName(TeaShopAnchorCounter); });
    if (Counter != nullptr)
    {
        TestTrue(TEXT("counter anchor is within 100 cm behind the counter and faces it"),
                 Counter->Local.Y - ServiceCounter.V1 < 100.0 && Counter->Facing.Y < -0.99);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWTeaShopRoutesTest, "PerfectWorld.TeaShop.Routes",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPWTeaShopRoutesTest::RunTest(const FString& Parameters)
{
    using namespace PWLane;
    using namespace TeaShopLayout;
    const FLaneBuilding B = PWTeaShopTest::MakeBuilding();
    TArray<FTeaShopRoute> Routes;
    GetTeaShopRoutes(B, Routes);
    TestTrue(TEXT("seven routes"), Routes.Num() == 7);

    const TArray<FTeaShopPiece> Solids = PWTeaShopTest::WalkingSolids();
    const TArray<FTeaShopRect> Walls = PWTeaShopTest::WallRects();
    TArray<FTeaShopAnchor> Anchors;
    GetTeaShopAnchors(B, Anchors);
    for (const FTeaShopRoute& Route : Routes)
    {
        const FString Name = Route.Id.ToString();
        TestTrue(Name + TEXT(" has points in both frames"), Route.Local.Num() >= 2 && Route.Local.Num() == Route.World.Num());
        for (int32 I = 0; I + 1 < Route.Local.Num(); ++I)
        {
            const FVector2D A = Route.Local[I];
            const FVector2D Z = Route.Local[I + 1];
            const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(A, Z) / 2.0));
            for (int32 S = 0; S <= Steps; ++S)
            {
                const FVector2D P = A + (Z - A) * (static_cast<double>(S) / static_cast<double>(Steps));
                const double Clear = PWTeaShopTest::Clearance(P, Solids, Walls);
                if (Clear < PWTeaShopTest::CapsuleRadius - 0.01)
                {
                    AddError(FString::Printf(TEXT("%s passes within %.1f cm of an obstacle at U %.0f V %.0f"), *Name, Clear, P.X, P.Y));
                    break;
                }
            }
        }
        for (int32 I = 0; I < Route.Local.Num(); ++I)
        {
            TestTrue(Name + TEXT(" world points follow the frame"), Route.World[I].Equals(B.Frame.Pt(Route.Local[I].X, Route.Local[I].Y, FloorW), 0.01));
        }
    }

    // Endpoints: the counter routes start at the counter anchor and finish at their anchor; the door route runs out to the footpath line.
    const auto AnchorLocal = [&Anchors](const TCHAR* Id)
    {
        const FTeaShopAnchor* Found = Anchors.FindByPredicate([Id](const FTeaShopAnchor& A) { return A.Id == FName(Id); });
        return Found != nullptr ? Found->Local : FVector2D(-1.0e6, -1.0e6);
    };
    const struct
    {
        const TCHAR* Route;
        const TCHAR* End;
    } Ends[] = {{TEXT("route.shanti_chai.counter_to_stove"), TeaShopAnchorStove},
                {TEXT("route.shanti_chai.counter_to_wash"), TeaShopAnchorWash},
                {TEXT("route.shanti_chai.counter_to_storage"), TeaShopAnchorStorage},
                {TEXT("route.shanti_chai.counter_to_rest"), TeaShopAnchorRest},
                {TEXT("route.shanti_chai.counter_to_bench_wipe"), TeaShopAnchorBenchWipe}};
    for (const auto& E : Ends)
    {
        const FTeaShopRoute* Found = Routes.FindByPredicate([&E](const FTeaShopRoute& R) { return R.Id == FName(E.Route); });
        if (Found == nullptr)
        {
            AddError(FString::Printf(TEXT("missing route %s"), E.Route));
            continue;
        }
        TestTrue(FString(E.Route) + TEXT(" starts at the counter anchor"), Found->Local[0].Equals(AnchorLocal(TeaShopAnchorCounter), 0.01));
        TestTrue(FString(E.Route) + TEXT(" ends at its anchor"), Found->Local.Last().Equals(AnchorLocal(E.End), 0.01));
    }
    const FTeaShopRoute* Out = Routes.FindByPredicate([](const FTeaShopRoute& R) { return R.Id == FName(TEXT("route.shanti_chai.door_to_footpath")); });
    if (Out != nullptr)
    {
        TestTrue(TEXT("the door route passes the door anchor"), Out->Local.ContainsByPredicate([&AnchorLocal](const FVector2D& P) { return P.Equals(AnchorLocal(TeaShopAnchorDoor), 0.01); }));
        TestTrue(TEXT("the door route ends on the footpath walking line"), FMath::IsNearlyEqual(Out->Local.Last().Y, FootpathRouteV));
        TestTrue(TEXT("the footpath line is world Y 340"), FMath::IsNearlyEqual(Out->World.Last().Y, 340.0, 0.01));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWTeaShopBuildTest, "PerfectWorld.TeaShop.Build",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPWTeaShopBuildTest::RunTest(const FString& Parameters)
{
    using namespace PWLane;
    using namespace TeaShopLayout;
    const FLaneBuilding B = PWTeaShopTest::MakeBuilding();
    FLaneScene Scene;
    BuildTeaShop(Scene, B);
    TestTrue(TEXT("the shutter door is registered"), HasTeaShop(Scene));
    TestTrue(TEXT("exactly one door spec"), Scene.Doors.Num() == 1);
    if (Scene.Doors.Num() == 1)
    {
        const FLaneDoorSpec& Door = Scene.Doors[0];
        TestTrue(TEXT("the shutter id is door.nabapara.shanti_chai_shutter"), Door.Id == FName(TEXT("door.nabapara.shanti_chai_shutter")));
        TestTrue(TEXT("the hinge is at the east jamb, mid-wall, on the floor"), Door.Hinge.Equals(FVector(995.0, 532.5, GroundZ + FootpathHeight), 0.01));
        TestTrue(TEXT("the closed leaf runs toward -X across the doorway"), FMath::IsNearlyEqual(FMath::Abs(Door.ClosedYaw), 180.0, 0.01));
        const double Yaw = FMath::DegreesToRadians(Door.ClosedYaw);
        const FVector LocalY(-FMath::Sin(Yaw), FMath::Cos(Yaw), 0.0);
        TestTrue(TEXT("the preferred swing is into the shop (world +Y)"), (LocalY * Door.PreferredSwing).Y > 0.5);
        TestTrue(TEXT("it opens both ways so a customer can leave"), Door.bTwoWay);
    }

    // Lights: warm room lights inside the building, under the ceiling, in every working room.
    TestTrue(TEXT("room lights"), Scene.RoomLights.Num() >= 5);
    for (const FLaneRoomLight& L : Scene.RoomLights)
    {
        const FVector Local = L.Position - B.Frame.Origin;
        TestTrue(TEXT("a light is inside the footprint"), Local.X > 25.0 && Local.X < 675.0 && Local.Y > 25.0 && Local.Y < 875.0);
        TestTrue(TEXT("a light hangs under the ceiling"), L.Position.Z > GroundZ + 250.0 && L.Position.Z < GroundZ + CeilingW);
        TestTrue(TEXT("a light's reach is short"), L.Radius <= 300.0f && L.Lumens <= 1000.0f);
    }
    TestTrue(TEXT("at most one stall-style bulb"), Scene.LightSpots.Num() <= 1);

#if !UE_BUILD_SHIPPING
    // The footpath stays clear: nothing blocking within 120 cm of the kerb (Y 250), and the build stays inside its footprint.
    const double WalkLow = GroundZ + FootpathHeight + 1.0;
    const double WalkHigh = GroundZ + FootpathHeight + PWTeaShopTest::WalkingHeight;
    const double KerbY = 250.0;
    const TArray<FBox>& Boxes = Scene.Blocks.BlockingBoxes();
    // The perimeter walls block through their World-colour bodies, which are not recorded. What is recorded is every
    // PropCollision box: one per solid furniture piece, the floor slab, the partition either side of and above its doorway
    // (three), and the glass in the two front hatches and the storage window (three).
    int32 SolidPieces = 0;
    for (const FTeaShopPiece& Piece : GetTeaShopPieces())
    {
        SolidPieces += Piece.bSolid ? 1 : 0;
    }
    const int32 ExpectedBoxes = SolidPieces + 1 + 3 + 3;
    TestEqual(TEXT("blocking boxes were recorded: every solid piece, the floor slab, the partition and the window glass"),
              Boxes.Num(), ExpectedBoxes);
    double NearestToKerb = 1.0e9;
    for (const FBox& Box : Boxes)
    {
        const bool bWalking = Box.Min.Z < WalkHigh && Box.Max.Z > WalkLow;
        if (bWalking)
        {
            NearestToKerb = FMath::Min(NearestToKerb, Box.Min.Y - KerbY);
        }
        if (Box.Min.X < 599.5 || Box.Max.X > 1300.5 || Box.Max.Y > 1420.5 || Box.Min.Y < 430.0)
        {
            AddError(TEXT("a blocking box lies outside the tea shop's footprint and seating strip"));
            break;
        }
        // The entrance corridor (X 937-963) from the door anchor to inside, and the partition doorway corridor, stay open.
        if (bWalking && Box.Min.X < 963.0 && Box.Max.X > 937.0 && Box.Min.Y < 640.0 && Box.Max.Y > 475.0)
        {
            AddError(TEXT("a blocking box closes the entrance corridor"));
            break;
        }
    }
    TestTrue(TEXT("at least 120 cm of footpath stays clear beside the kerb"), NearestToKerb >= 120.0);
#endif
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
