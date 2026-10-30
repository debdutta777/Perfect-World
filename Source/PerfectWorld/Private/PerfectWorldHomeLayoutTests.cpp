#include "PerfectWorldHomeLayoutTestSupport.h"

// Layout rules of the opening home (PerfectWorld.Home.*): door clear widths, routes, swing zones, the household adult's
// spot, the seat and readable anchors, the reserved stair, ceiling fans and room lights. The home is built exactly as the
// lane builds it (BuildOpeningHome plus the furniture footprints), so these tests read the real collision boxes.
// Run in the editor under "PerfectWorld.Home".

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Doors.h"
#include "PerfectWorldDoorActor.h"


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWHomeDoorWidthsTest, "PerfectWorld.Home.DoorClearWidths",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWHomeDoorWidthsTest::RunTest(const FString& Parameters)
{
    using namespace PWHomeLayoutTest;
    FHome Home;
    BuildHome(Home);
    if (!TestTrue(TEXT("the opening home builds"), Home.bBuilt))
    {
        return false;
    }
    struct FProbe
    {
        const TCHAR* Id;
        bool bAlongU;
        double Line;
        double Centre;
    };
    const FProbe Probes[] = {
        {TEXT("door.nabapara.home_front"), true, 12.5, HomeFrontDoorU},
        {LivingDoorId, false, HomeLivingPartitionU, HomeLivingDoorV},
        {TEXT("door.nabapara.home_bedroom"), true, HomeAcrossPartitionV, HomeBedroomDoorU},
        {TEXT("door.nabapara.home_kitchen"), true, HomeAcrossPartitionV, HomeKitchenDoorU},
        {TEXT("door.nabapara.home_washroom"), true, HomeAcrossPartitionV, HomeWashroomDoorU},
    };
    for (const FProbe& Probe : Probes)
    {
        double Clear = 0.0;
        double Headroom = 0.0;
        if (!MeasureDoorGap(Home, Probe.bAlongU, Probe.Line, Probe.Centre, Clear, Headroom))
        {
            AddError(FString::Printf(TEXT("%s: the doorway could not be measured (solid in the opening, or no jambs)"), Probe.Id));
            continue;
        }
        if (Clear < 90.0)
        {
            AddError(FString::Printf(TEXT("%s: clear width %.1f cm, needs at least 90"), Probe.Id, Clear));
        }
        if (Headroom < 210.0)
        {
            AddError(FString::Printf(TEXT("%s: clear height %.1f cm, needs at least 210"), Probe.Id, Headroom));
        }
        if (FindDoor(Home, Probe.Id) == nullptr)
        {
            AddError(FString::Printf(TEXT("%s: no door spec in Scene.Doors"), Probe.Id));
        }
    }
    TestEqual(TEXT("five door specs, ids unchanged"), Home.Scene.Doors.Num(), 5);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWHomeSwingZonesTest, "PerfectWorld.Home.SwingZonesClear",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWHomeSwingZonesTest::RunTest(const FString& Parameters)
{
    using namespace PWHomeLayoutTest;
    FHome Home;
    BuildHome(Home);
    if (!TestTrue(TEXT("the opening home builds"), Home.bBuilt))
    {
        return false;
    }
    const FHomeStairSpec Stair = HomeStairSpec();
    const FRect StairRect{Stair.U0, Stair.U1, Stair.V0, Stair.V1};
    for (const FNamedZone& Zone : AllSwingZones(Home))
    {
        for (const FHomeSolid& Solid : Home.Solids)
        {
            if (Overlaps(Zone.Zone, RectOf(Solid)))
            {
                AddError(FString::Printf(TEXT("%s: furniture U %.0f-%.0f V %.0f-%.0f stands in the swing zone U %.0f-%.0f V %.0f-%.0f"),
                                         *Zone.Door.ToString(), Solid.U0, Solid.U1, Solid.V0, Solid.V1, Zone.Zone.U0, Zone.Zone.U1,
                                         Zone.Zone.V0, Zone.Zone.V1));
            }
        }
        if (Overlaps(Zone.Zone, StairRect))
        {
            AddError(FString::Printf(TEXT("%s: the reserved stair overlaps its swing zone"), *Zone.Door.ToString()));
        }
    }
    // The living door opens into the entry only: its zone must lie east of the partition, and no part in the living room.
    const FLaneDoorSpec* Living = FindDoor(Home, LivingDoorId);
    if (TestTrue(TEXT("the living door exists"), Living != nullptr))
    {
        TestFalse(TEXT("the living door is one-way"), Living->bTwoWay);
        const FRect Zone = SwingZone(Home.Building.Frame, *Living, Living->PreferredSwing);
        TestTrue(TEXT("the living door's swing is on the entry side only"), Zone.U0 >= HomeLivingPartitionU - 10.0);
        FRect Leaf;
        if (TestTrue(TEXT("open leaf computed"), OpenLeafRect(Home, Leaf)))
        {
            // The open leaf lies against the back partition (face at V 431): within 25 cm of it, never across the room.
            TestTrue(TEXT("the open living leaf lies against the back partition"), Leaf.V1 >= 431.0 - 25.0 && Leaf.U0 >= HomeLivingPartitionU - 4.0);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWHomeRoutesTest, "PerfectWorld.Home.RouteWidths",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWHomeRoutesTest::RunTest(const FString& Parameters)
{
    using namespace PWHomeLayoutTest;
    FHome Home;
    BuildHome(Home);
    if (!TestTrue(TEXT("the opening home builds"), Home.bBuilt))
    {
        return false;
    }
    TArray<FRect> Obstacles = WalkObstacles(Home);
    const FRect Floor{25.0, 975.0, 25.0, 875.0};
    const double SeedU = 730.0; // just inside the front door
    const double SeedV = 85.0;

    // 90 cm routes (a 45 cm disc) from the front door to every part of the home, every door and every anchor.
    struct FTarget
    {
        const TCHAR* Name;
        double U;
        double V;
    };
    TArray<FTarget> Targets = {
        {TEXT("front door, inside"), SeedU, SeedV},
        {TEXT("in front of the living doorway (entry)"), 700.0, 345.0},
        {TEXT("in front of the washroom door (entry)"), 760.0, 365.0},
        {TEXT("living doorway"), HomeLivingPartitionU, HomeLivingDoorV},
        {TEXT("living room, middle"), 400.0, 300.0},
        {TEXT("bedroom door"), HomeBedroomDoorU, 435.0},
        {TEXT("kitchen door"), HomeKitchenDoorU, 435.0},
        {TEXT("washroom door"), HomeWashroomDoorU, 435.0},
        {TEXT("sleeping room, middle"), 200.0, 650.0},
        {TEXT("sleeping room, bed side"), 170.0, 700.0},
        {TEXT("sleeping room, behind the desk chair"), 295.0, 700.0},
        {TEXT("kitchen aisle"), 560.0, 600.0},
        {TEXT("washroom"), 860.0, 640.0},
    };
    const FHomeAdultSpot Adult = HomeAdultSpot();
    Targets.Add(FTarget{TEXT("adult spot"), Adult.U, Adult.V});
    for (const FHomeSeatSpec& Seat : HomeSeatSpecs())
    {
        Targets.Add(FTarget{TEXT("seat, stand-up point"), Seat.StandU, Seat.StandV});
    }
    const FHomeReadableSpec Paper = HomeReadableSpec();
    Targets.Add(FTarget{TEXT("newspaper, reading spot"), Paper.StandU, Paper.StandV});

    const TArray<uint8> Reach90 = FloodRoutes(Obstacles, 45.0, Floor, SeedU, SeedV);
    for (const FTarget& Target : Targets)
    {
        if (!IsReached(Reach90, Target.U, Target.V))
        {
            AddError(FString::Printf(TEXT("no 90 cm route from the front door to: %s (U %.0f, V %.0f)"), Target.Name, Target.U, Target.V));
        }
    }

    // With the adult standing at their spot (a 34 cm capsule), the living room and its doors stay reachable.
    TArray<FRect> WithAdult = Obstacles;
    WithAdult.Add(FRect{Adult.U - HomePersonRadiusCm, Adult.U + HomePersonRadiusCm, Adult.V - HomePersonRadiusCm, Adult.V + HomePersonRadiusCm});
    const TArray<uint8> ReachAdult = FloodRoutes(WithAdult, 45.0, Floor, SeedU, SeedV);
    for (const FTarget& Target : Targets)
    {
        if (Target.U == Adult.U && Target.V == Adult.V)
        {
            continue;
        }
        if (!IsReached(ReachAdult, Target.U, Target.V))
        {
            AddError(FString::Printf(TEXT("with the adult in place there is no 90 cm route to: %s"), Target.Name));
        }
    }

    // 120 cm routes in the entry: from the front door to the living doorway's approach and the washroom door's approach.
    const FRect Entry{626.0, 975.0, 25.0, 431.0};
    const TArray<uint8> Reach120 = FloodRoutes(Obstacles, 60.0, Entry, SeedU, SeedV);
    TestTrue(TEXT("a 120 cm route from the front door to the living doorway's approach"), IsReached(Reach120, 700.0, 345.0));
    TestTrue(TEXT("a 120 cm route from the front door to the washroom door's approach"), IsReached(Reach120, 760.0, 365.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWHomeAdultSpotTest, "PerfectWorld.Home.AdultSpot",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWHomeAdultSpotTest::RunTest(const FString& Parameters)
{
    using namespace PWHomeLayoutTest;
    FHome Home;
    BuildHome(Home);
    if (!TestTrue(TEXT("the opening home builds"), Home.bBuilt))
    {
        return false;
    }
    const FHomeAdultSpot Spot = HomeAdultSpot();
    const double ToThreshold = FMath::Sqrt(FMath::Square(Spot.FaceU - Spot.U) + FMath::Square(Spot.FaceV - Spot.V));
    TestTrue(TEXT("the spot is at most 150 cm from the living doorway's threshold"), ToThreshold <= 150.0);
    TestTrue(TEXT("the threshold is the living doorway's centre"),
             FMath::IsNearlyEqual(Spot.FaceU, HomeLivingPartitionU) && FMath::IsNearlyEqual(Spot.FaceV, HomeLivingDoorV));
    TestTrue(TEXT("the spot is in the living room"), Spot.U > 25.0 && Spot.U < HomeLivingPartitionU - 6.0 && Spot.V > 25.0 && Spot.V < 431.0);
    TestTrue(TEXT("45 cm clear of every piece of furniture"), MinDistToSolids(Home, Spot.U, Spot.V) >= 45.0);
    TestTrue(TEXT("45 cm clear of every wall, jamb and furniture box"), MinDistToBodyBlocks(Home, Spot.U, Spot.V) >= 45.0);
    TestTrue(TEXT("45 cm outside every door swing"), MinDistToZones(Home, Spot.U, Spot.V) >= 45.0);
    FRect Leaf;
    if (TestTrue(TEXT("open leaf computed"), OpenLeafRect(Home, Leaf)))
    {
        TestTrue(TEXT("45 cm clear of the open living leaf"), DistToRect(Spot.U, Spot.V, Leaf) >= 45.0);
    }

    // A player stepping through the living doorway (facing west, -U) is within the 2 m reach and the 50 degree half-cone.
    const FLaneFrame& F = Home.Building.Frame;
    for (const double U : {HomeLivingPartitionU + 30.0, HomeLivingPartitionU})
    {
        for (double V = HomeLivingDoorV - 35.0; V <= HomeLivingDoorV + 35.0; V += 10.0)
        {
            const double DU = Spot.U - U;
            const double DV = Spot.V - V;
            const double Distance = FMath::Sqrt(DU * DU + DV * DV);
            const double Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(-DU / Distance, -1.0, 1.0)));
            if (Distance > 200.0 || Angle > 50.0)
            {
                AddError(FString::Printf(TEXT("from the doorway at U %.0f V %.0f the adult is %.0f cm away at %.0f degrees"), U, V, Distance, Angle));
            }
        }
    }

    // The actor's forward (yaw) points at the threshold.
    const double Yaw = FMath::DegreesToRadians(HomeAdultSpotYaw(F));
    const FVector Forward(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.0);
    const FVector Wanted = (HomeWorldPoint(F, Spot.FaceU, Spot.FaceV) - HomeAdultSpotWorld(F)).GetSafeNormal2D();
    TestTrue(TEXT("the adult's yaw faces the doorway"), FVector::DotProduct(Forward, Wanted) > 0.999);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWHomeAnchorsTest, "PerfectWorld.Home.SeatAndReadableAnchors",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWHomeAnchorsTest::RunTest(const FString& Parameters)
{
    using namespace PWHomeLayoutTest;
    FHome Home;
    BuildHome(Home);
    if (!TestTrue(TEXT("the opening home builds"), Home.bBuilt))
    {
        return false;
    }
    const TArray<FHomeSeatSpec> Seats = HomeSeatSpecs();
    TestTrue(TEXT("at least one seat"), Seats.Num() >= 1);
    for (const FHomeSeatSpec& Seat : Seats)
    {
        const FString Name = Seat.Id.ToString();
        bool bOnSolid = false;
        for (const FHomeSolid& Solid : Home.Solids)
        {
            bOnSolid |= Seat.SeatU >= Solid.U0 && Seat.SeatU <= Solid.U1 && Seat.SeatV >= Solid.V0 && Seat.SeatV <= Solid.V1 &&
                        FMath::Abs(Solid.H1 - Seat.SeatHeightCm) <= 1.0;
        }
        if (!bOnSolid)
        {
            AddError(FString::Printf(TEXT("%s: the seat point is not on top of a solid piece at the stated seat height"), *Name));
        }
        if (!FMath::IsNearlyEqual(FMath::Sqrt(Seat.FaceU * Seat.FaceU + Seat.FaceV * Seat.FaceV), 1.0, 1.0e-6))
        {
            AddError(FString::Printf(TEXT("%s: the facing is not a unit vector"), *Name));
        }
        if (MinDistToBodyBlocks(Home, Seat.StandU, Seat.StandV) < Seat.StandClearCm || Seat.StandClearCm < 45.0)
        {
            AddError(FString::Printf(TEXT("%s: the stand-up point lacks 45 cm of clear floor"), *Name));
        }
        if (MinDistToZones(Home, Seat.StandU, Seat.StandV) < 45.0)
        {
            AddError(FString::Printf(TEXT("%s: the stand-up point is in or near a door swing"), *Name));
        }
        const double Gap = FMath::Sqrt(FMath::Square(Seat.StandU - Seat.SeatU) + FMath::Square(Seat.StandV - Seat.SeatV));
        if (Gap > 120.0)
        {
            AddError(FString::Printf(TEXT("%s: the stand-up point is %.0f cm from the seat (limit 120)"), *Name, Gap));
        }
    }

    const FHomeReadableSpec Paper = HomeReadableSpec();
    bool bOnTable = false;
    for (const FHomeSolid& Solid : Home.Solids)
    {
        bOnTable |= Paper.U >= Solid.U0 && Paper.U <= Solid.U1 && Paper.V >= Solid.V0 && Paper.V <= Solid.V1 &&
                    Paper.HeightCm > Solid.H1 && Paper.HeightCm - Solid.H1 <= 3.0;
    }
    TestTrue(TEXT("the newspaper lies on a solid surface"), bOnTable);
    TestTrue(TEXT("the reading spot has 45 cm of clear floor"), MinDistToBodyBlocks(Home, Paper.StandU, Paper.StandV) >= 45.0 && Paper.StandClearCm >= 45.0);
    TestTrue(TEXT("the reading spot is outside every door swing"), MinDistToZones(Home, Paper.StandU, Paper.StandV) >= 45.0);
    TestTrue(TEXT("the reading spot is within 120 cm of the sheet"),
             FMath::Sqrt(FMath::Square(Paper.StandU - Paper.U) + FMath::Square(Paper.StandV - Paper.V)) <= 120.0);
    TestTrue(TEXT("the readable ids are set"), !Paper.Id.IsNone() && !Paper.ContentId.IsNone());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWHomeStairTest, "PerfectWorld.Home.StairReservation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWHomeStairTest::RunTest(const FString& Parameters)
{
    using namespace PWHomeLayoutTest;
    FHome Home;
    BuildHome(Home);
    if (!TestTrue(TEXT("the opening home builds"), Home.bBuilt))
    {
        return false;
    }
    const FHomeStairSpec Stair = HomeStairSpec();
    const FRect Footprint{Stair.U0, Stair.U1, Stair.V0, Stair.V1};
    TestTrue(TEXT("the stair lies in the entry"), Stair.U0 >= 626.0 && Stair.U1 <= 975.0 && Stair.V0 >= 25.0 && Stair.V1 <= 431.0);
    TestTrue(TEXT("two lanes of at least 90 cm"), Stair.LaneWidthCm >= 90.0 && Stair.U1 - Stair.U0 >= 2.0 * Stair.LaneWidthCm);
    TestEqual(TEXT("17 risers"), Stair.Risers, 17);
    TestTrue(TEXT("equal risers reach the upper floor exactly"), FMath::IsNearlyEqual(Stair.RiserCm * Stair.Risers, Stair.FloorToFloorCm, 0.01));
    TestTrue(TEXT("riser near the 17.5 reference"), FMath::Abs(Stair.RiserCm - 17.5) <= 0.5);
    TestTrue(TEXT("tread 25"), FMath::IsNearlyEqual(Stair.TreadCm, 25.0));
    // Lower flight: 9 risers, 8 treads, from the foot line to the landing; upper flight: 8 risers, 7 treads, to the upper edge.
    TestTrue(TEXT("lower flight meets the landing"), FMath::IsNearlyEqual(Stair.FootV - 8.0 * Stair.TreadCm, Stair.LandingV1, 0.01));
    TestTrue(TEXT("landing is at least 90 deep"), Stair.LandingV1 - Stair.LandingV0 >= 90.0);
    TestTrue(TEXT("upper flight meets the upper floor"), FMath::IsNearlyEqual(Stair.LandingV1 + 7.0 * Stair.TreadCm, Stair.UpperEdgeV, 0.01));
    TestTrue(TEXT("the flights stay inside the footprint"), Stair.FootV <= Stair.V1 && Stair.LandingV0 >= Stair.V0 && Stair.UpperEdgeV <= Stair.V1);
    for (const FBlock& Block : Home.Blocks)
    {
        if (IsBodyBlock(Block) && Overlaps(Block.R, Footprint))
        {
            AddError(FString::Printf(TEXT("a collision box (U %.0f-%.0f V %.0f-%.0f) stands in the reserved stair footprint"), Block.R.U0,
                                     Block.R.U1, Block.R.V0, Block.R.V1));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWHomeLightingTest, "PerfectWorld.Home.FansAndLights",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWHomeLightingTest::RunTest(const FString& Parameters)
{
    using namespace PWHomeLayoutTest;
    FHome Home;
    BuildHome(Home);
    if (!TestTrue(TEXT("the opening home builds"), Home.bBuilt))
    {
        return false;
    }
    const FLaneFrame& F = Home.Building.Frame;
    const double FloorZ = GroundZ + HomeFloorW;
    const TArray<FHomeLightSpec> Specs = HomeLightSpecs();
    const TArray<FHomeFanSpec> Fans = HomeFanSpecs();
    TestTrue(TEXT("a modest light count"), Specs.Num() >= 5 && Specs.Num() <= 8);
    if (!TestEqual(TEXT("Scene.RoomLights matches the light specs"), Home.Scene.RoomLights.Num(), Specs.Num()))
    {
        return false;
    }
    for (int32 I = 0; I < Specs.Num(); ++I)
    {
        const FHomeLightSpec& Spec = Specs[I];
        const FLaneRoomLight& Light = Home.Scene.RoomLights[I];
        double U = 0.0;
        double V = 0.0;
        ToLocal(F, Light.Position, U, V);
        const FString Name = FString::Printf(TEXT("%s light %d"), *Spec.Room.ToString(), I);
        TestTrue(*(Name + TEXT(" is published at its spec position")), FMath::IsNearlyEqual(U, Spec.U, 0.01) && FMath::IsNearlyEqual(V, Spec.V, 0.01));
        TestTrue(*(Name + TEXT(" publishes the spec's radius and lumens")), FMath::IsNearlyEqual(Light.Radius, Spec.AttenuationCm) && FMath::IsNearlyEqual(Light.Lumens, Spec.Lumens));
        TestTrue(*(Name + TEXT(" casts shadows")), Spec.bCastShadows);
        TestTrue(*(Name + TEXT(" hangs below the ceiling")), Light.Position.Z > FloorZ + 200.0 && Light.Position.Z < GroundZ + HomeCeilingW);
        TestTrue(*(Name + TEXT(" is 20 cm or more inside its room")),
                 U >= Spec.RoomU0 + 20.0 && U <= Spec.RoomU1 - 20.0 && V >= Spec.RoomV0 + 20.0 && V <= Spec.RoomV1 - 20.0);
        const double ShorterSide = FMath::Min(Spec.RoomU1 - Spec.RoomU0, Spec.RoomV1 - Spec.RoomV0);
        TestTrue(*(Name + TEXT(" reaches no farther than its room's shorter side")), Spec.AttenuationCm <= ShorterSide + Tolerance);
        for (const FHomeFanSpec& Fan : Fans)
        {
            TestTrue(*(Name + TEXT(" clears every fan's blades")),
                     FMath::Sqrt(FMath::Square(U - Fan.U) + FMath::Square(V - Fan.V)) >= Fan.RadiusCm + 10.0);
        }
    }

    for (const FHomeFanSpec& Fan : Fans)
    {
        TestTrue(TEXT("fan underside at least 240 cm above the floor"), Fan.UndersideCm >= 240.0);
        const FString Where = FString::Printf(TEXT("fan at U %.0f V %.0f"), Fan.U, Fan.V);
        bool bInRoom = false;
        for (const FHomeLightSpec& Room : Specs)
        {
            bInRoom |= Fan.U - Fan.RadiusCm >= Room.RoomU0 && Fan.U + Fan.RadiusCm <= Room.RoomU1 && Fan.V - Fan.RadiusCm >= Room.RoomV0 &&
                       Fan.V + Fan.RadiusCm <= Room.RoomV1;
        }
        TestTrue(*(Where + TEXT(" turns clear of the walls")), bInRoom);
        for (const FHomeSolid& Solid : Home.Solids)
        {
            if (DistToRect(Fan.U, Fan.V, RectOf(Solid)) < Fan.RadiusCm && Solid.H1 > Fan.UndersideCm - 55.0)
            {
                AddError(FString::Printf(TEXT("%s: furniture up to %.0f cm high is under the blades"), *Where, Solid.H1));
            }
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR89HomeKitchenRouteTest, "PerfectWorld.Home.RestoredKitchenLeafClearsChairRoute",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FR89HomeKitchenRouteTest::RunTest(const FString& Parameters)
{
    using namespace PWHomeLayoutTest;
    FHome R89Home;
    BuildHome(R89Home);
    const FLaneDoorSpec* R89Door = FindDoor(R89Home, TEXT("door.nabapara.home_kitchen"));
    const auto* R89Def = PerfectWorld::FindDoor("door.nabapara.home_kitchen");
    if (!TestTrue(TEXT("authored kitchen door and core definition exist"), R89Door != nullptr && R89Def != nullptr)) return false;
    TestTrue(TEXT("core starts the kitchen door open"), R89Def->bInitiallyOpen);
    // RegisterDoor/StartLife restore the core side, which may differ from the layout preference.
    const double R89Yaw = FMath::DegreesToRadians(R89Door->ClosedYaw + R89Def->PreferredSwingSide * PWDoor::OpenAngleDegrees);
    const FVector R89Tip = R89Door->Hinge + FVector(FMath::Cos(R89Yaw), FMath::Sin(R89Yaw), 0) * PWDoor::LeafWidth;
    double R89HingeU = 0, R89HingeV = 0, R89TipU = 0, R89TipV = 0;
    ToLocal(R89Home.Building.Frame, R89Door->Hinge, R89HingeU, R89HingeV);
    ToLocal(R89Home.Building.Frame, R89Tip, R89TipU, R89TipV);
    TestTrue(TEXT("restored leaf opens into kitchen"), R89TipV > R89HingeV);
    const FRect R89Leaf{FMath::Min(R89HingeU, R89TipU) - 2, FMath::Max(R89HingeU, R89TipU) + 2,
        FMath::Min(R89HingeV, R89TipV) - 2, FMath::Max(R89HingeV, R89TipV) + 2};
    TestTrue(TEXT("reported stuck position is clear of restored leaf"), DistToRect(485, 366, R89Leaf) > HomePersonRadiusCm);
    // Exercise the unchanged chair route against the real shell/furniture and the core-restored leaf, without a map.
    const FVector2D R89Points[] = {{630, 365}, {560, 375}, {440, 370}, {405, 265}, {393, 215}};
    const auto R89Adult = HomeAdultSpot();
    for (int32 R89Segment = 1; R89Segment < UE_ARRAY_COUNT(R89Points); ++R89Segment)
    {
        for (int32 R89Sample = 0; R89Sample <= 100; ++R89Sample)
        {
            const FVector2D R89At = R89Points[R89Segment - 1] +
                (R89Points[R89Segment] - R89Points[R89Segment - 1]) * (R89Sample / 100.0);
            if (MinDistToBodyBlocks(R89Home, R89At.X, R89At.Y) <= HomePersonRadiusCm ||
                DistToRect(R89At.X, R89At.Y, R89Leaf) <= HomePersonRadiusCm ||
                FVector2D::Distance(R89At, FVector2D(R89Adult.U, R89Adult.V)) <= 2 * HomePersonRadiusCm)
            {
                AddError(FString::Printf(TEXT("chair route blocked at U %.1f V %.1f"), R89At.X, R89At.Y));
                return false;
            }
        }
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
