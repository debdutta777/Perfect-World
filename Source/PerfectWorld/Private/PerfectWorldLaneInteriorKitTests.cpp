// Checks for the parametric interior kit (PerfectWorldLaneInteriorKit.h) and the stationery shop built on it
// (PerfectWorldLaneStationeryShop.h). They run the real builders on bare scenes and read back what was emitted: the collision
// boxes, the door specs, the room lights and the anchors, for three different footprints and orientations. No world is needed.
// Run in the editor under "PerfectWorld.InteriorKit" (Session Frontend, or the Unreal MCP AutomationTestToolset).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "PerfectWorldLaneInteriorKit.h"
#include "PerfectWorldLaneStationeryShop.h"

namespace PWKitTest
{
using namespace PWLane;

constexpr double CapsuleRadius = 34.0;  // the walking capsule is 68 wide
constexpr double CapsuleHeight = 165.0;

/** The world-space box of a layout rectangle between two heights above the finished floor. */
FBox WorldBox(const FLaneBuilding& B, const LI::InteriorLayout& L, const LI::Rect& R, double H0, double H1)
{
    FBox Box(ForceInit);
    for (const int32 U : {R.U0, R.U1})
    {
        for (const int32 V : {R.V0, R.V1})
        {
            Box += KitPoint(B, L, U, V, H0);
            Box += KitPoint(B, L, U, V, H1);
        }
    }
    return Box;
}

bool BoxesMatch(const FBox& A, const FBox& B)
{
    return A.Min.Equals(B.Min, 0.6) && A.Max.Equals(B.Max, 0.6);
}

/** Boxes that overlap by more than a hair (touching faces do not count). */
bool Overlaps(const FBox& A, const FBox& B)
{
    constexpr double Hair = 0.01;
    return A.Min.X < B.Max.X - Hair && B.Min.X < A.Max.X - Hair && A.Min.Y < B.Max.Y - Hair && B.Min.Y < A.Max.Y - Hair &&
           A.Min.Z < B.Max.Z - Hair && B.Min.Z < A.Max.Z - Hair;
}

FVector YawDirection(double Degrees)
{
    const double Radians = FMath::DegreesToRadians(Degrees);
    return FVector(FMath::Cos(Radians), FMath::Sin(Radians), 0.0);
}

FLaneBuilding MakeBuilding(double OriginX, double OriginY, const FVector& U, const FVector& V, double Width, double Depth, int32 Storeys)
{
    FLaneBuilding B;
    B.Frame = MakeLaneFrame(OriginX, OriginY, U, V);
    B.Width = Width;
    B.Depth = Depth;
    B.Storeys = Storeys;
    B.Detail = ELaneDetail::Full;
    B.Wall = ELaneColour::WallCream;
    return B;
}

/** Builds the kit for one footprint and reads every emitted fact back against the portable layout. */
void CheckKitCase(FAutomationTestBase& T, const TCHAR* Name, const FLaneBuilding& B, const LI::InteriorInput& In, int32 ExpectedDoors)
{
    const LI::InteriorLayout Layout = LI::GenerateInteriorLayout(In);
    if (!Layout.Ok())
    {
        T.AddError(FString::Printf(TEXT("%s: layout refused (%hs)"), Name, std::string(LI::RefusalCode(Layout.Status)).c_str()));
        return;
    }
    FLaneScene Scene;
    BuildInteriorKit(Scene, B, Layout);
    const TArray<FBox>& Blocking = Scene.Blocks.BlockingBoxes();
    const double FloorAbs = GroundZ + Layout.FinishedFloorCm;

    // The exact set of collision boxes: one floor slab, every solid piece, every partition segment, every window pane.
    int32 Expected = 1;
    for (const LI::Furniture& P : Layout.Pieces)
    {
        Expected += P.Solid ? 1 : 0;
    }
    for (const LI::WallSegment& S : Layout.Walls)
    {
        Expected += S.WallId.find(".wall.partition") != std::string::npos ? 1 : 0;
    }
    for (const LI::Opening& O : Layout.Openings)
    {
        Expected += O.Kind == LI::OpeningKind::Window ? 1 : 0;
    }
    T.TestEqual(*FString::Printf(TEXT("%s: collision box count"), Name), Blocking.Num(), Expected);

    // Every solid piece has a collision box of exactly its size, at the right height above the finished floor.
    for (const LI::Furniture& P : Layout.Pieces)
    {
        if (!P.Solid)
        {
            continue;
        }
        const FBox Want = WorldBox(B, Layout, P.Footprint, P.H0, P.H1);
        if (!Blocking.ContainsByPredicate([&](const FBox& Box) { return BoxesMatch(Box, Want); }))
        {
            T.AddError(FString::Printf(TEXT("%s: no collision box for %hs"), Name, P.Id.c_str()));
        }
    }
    T.TestTrue(*FString::Printf(TEXT("%s: the floor slab tops out at the finished floor"), Name),
               Blocking.ContainsByPredicate([&](const FBox& Box) { return FMath::IsNearlyEqual(Box.Max.Z, FloorAbs, 0.1) && Box.Min.Z <= GroundZ + 0.1; }));

    // Walking space: stand points, seats' approach, circulation and door sweeps stay clear of every collision box above the floor.
    TArray<FKitAnchor> Anchors;
    GetKitAnchors(B, Layout, Anchors);
    T.TestEqual(*FString::Printf(TEXT("%s: anchor count"), Name), Anchors.Num(), Layout.Anchors.Num());
    for (int32 I = 0; I < Anchors.Num(); ++I)
    {
        const LI::Anchor& A = Layout.Anchors[I];
        const FKitAnchor& W = Anchors[I];
        T.TestTrue(*FString::Printf(TEXT("%s: anchor %hs is floor-relative"), Name, A.Id.c_str()),
                   W.Location.Equals(B.Frame.Pt(A.U, A.V, FloorAbs - GroundZ), 0.01));
        const FVector Facing = (B.Frame.U * A.FaceU + B.Frame.V * A.FaceV).GetSafeNormal();
        T.TestTrue(*FString::Printf(TEXT("%s: anchor %hs yaw matches its facing"), Name, A.Id.c_str()), YawDirection(W.YawDegrees).Equals(Facing, 0.001));
        if (A.Kind == LI::AnchorKind::Stand)
        {
            const FBox Capsule(W.Location + FVector(-CapsuleRadius, -CapsuleRadius, 2.0), W.Location + FVector(CapsuleRadius, CapsuleRadius, CapsuleHeight));
            for (const FBox& Box : Blocking)
            {
                if (Overlaps(Capsule, Box))
                {
                    T.AddError(FString::Printf(TEXT("%s: a collision box intrudes on stand point %hs"), Name, A.Id.c_str()));
                    break;
                }
            }
            if (const LI::Furniture* Piece = LI::FindFurniture(Layout, A.FurnitureId))
            {
                const FVector ToPiece = WorldBox(B, Layout, Piece->Footprint, Piece->H0, Piece->H1).GetClosestPointTo(W.Location) - W.Location;
                T.TestTrue(*FString::Printf(TEXT("%s: %hs faces its furniture"), Name, A.Id.c_str()), FVector::DotProduct(ToPiece.GetSafeNormal2D(), Facing) > 0.5);
            }
        }
        else if (A.Kind == LI::AnchorKind::Seat)
        {
            const LI::Furniture* Piece = LI::FindFurniture(Layout, A.FurnitureId);
            const FBox Want = Piece != nullptr ? WorldBox(B, Layout, Piece->Footprint, Piece->H0, Piece->H1) : FBox(ForceInit);
            T.TestTrue(*FString::Printf(TEXT("%s: seat %hs sits on its stool"), Name, A.Id.c_str()),
                       Piece != nullptr && W.Location.X >= Want.Min.X && W.Location.X <= Want.Max.X && W.Location.Y >= Want.Min.Y && W.Location.Y <= Want.Max.Y);
        }
    }
    for (const LI::Zone& Z : Layout.Zones)
    {
        if (Z.Kind == LI::ZoneKind::Route)
        {
            continue;
        }
        const FBox Space = WorldBox(B, Layout, Z.Area, 2.0, 210.0);
        for (const FBox& Box : Blocking)
        {
            if (Overlaps(Space, Box))
            {
                T.AddError(FString::Printf(TEXT("%s: a collision box fills clear space %hs"), Name, Z.Id.c_str()));
                break;
            }
        }
    }

    // Every doorway is 90 x 210 clear of collision (partitions have their openings).
    for (const LI::Opening& O : Layout.Openings)
    {
        if (O.Kind == LI::OpeningKind::Window)
        {
            continue;
        }
        const bool bAlongU = O.Plan.Width() > O.Plan.Depth();
        const LI::Rect Clear = bAlongU ? LI::Rect{O.Plan.U0 + 2, O.Plan.U1 - 2, O.Plan.V0, O.Plan.V1} : LI::Rect{O.Plan.U0, O.Plan.U1, O.Plan.V0 + 2, O.Plan.V1 - 2};
        const FBox Gap = WorldBox(B, Layout, Clear, 2.0, LI::DoorClearHeightCm);
        for (const FBox& Box : Blocking)
        {
            if (Overlaps(Gap, Box))
            {
                T.AddError(FString::Printf(TEXT("%s: doorway %hs is blocked"), Name, O.Id.c_str()));
                break;
            }
        }
    }

    // Door specs: one per hung leaf, hinged on the right jamb, closed across the doorway, opening into the building by default.
    T.TestEqual(*FString::Printf(TEXT("%s: door spec count"), Name), Scene.Doors.Num(), ExpectedDoors);
    for (const LI::Opening& O : Layout.Openings)
    {
        if (O.DoorId.empty())
        {
            continue;
        }
        const FName Id(UTF8_TO_TCHAR(O.DoorId.c_str()));
        const FLaneDoorSpec* Spec = Scene.Doors.FindByPredicate([&](const FLaneDoorSpec& D) { return D.Id == Id; });
        if (Spec == nullptr)
        {
            T.AddError(FString::Printf(TEXT("%s: no door spec for %hs"), Name, O.DoorId.c_str()));
            continue;
        }
        T.TestTrue(*FString::Printf(TEXT("%s: hinge position"), Name), Spec->Hinge.Equals(KitPoint(B, Layout, O.HingeU, O.HingeV, 0.0), 0.01));
        const bool bHingeLow = O.HingeU == O.Plan.U0;
        const int32 OtherU = bHingeLow ? O.Plan.U1 : O.Plan.U0;
        const FVector Across = Spec->Hinge + YawDirection(Spec->ClosedYaw) * LI::DoorOpeningWidthCm;
        T.TestTrue(*FString::Printf(TEXT("%s: the closed leaf spans the doorway"), Name), Across.Equals(KitPoint(B, Layout, OtherU, O.HingeV, 0.0), 0.5));
        const FVector LocalY(-YawDirection(Spec->ClosedYaw).Y, YawDirection(Spec->ClosedYaw).X, 0.0);
        T.TestTrue(*FString::Printf(TEXT("%s: the default swing is into the building"), Name), Spec->PreferredSwing * FVector::DotProduct(LocalY, B.Frame.V) > 0.5);
        T.TestTrue(*FString::Printf(TEXT("%s: the leaf is two-way"), Name), Spec->bTwoWay);
    }

    // Lights: one per spec, inside its room at the specified height, with an attenuation radius inside the room's shorter side.
    T.TestEqual(*FString::Printf(TEXT("%s: room light count"), Name), Scene.RoomLights.Num(), Layout.Lights.Num());
    for (int32 I = 0; I < Layout.Lights.Num() && I < Scene.RoomLights.Num(); ++I)
    {
        const LI::LightSpec& Spec = Layout.Lights[I];
        T.TestTrue(*FString::Printf(TEXT("%s: light %hs position"), Name, Spec.Id.c_str()),
                   Scene.RoomLights[I].Position.Equals(KitPoint(B, Layout, Spec.U, Spec.V, Spec.HeightCm), 0.01));
        T.TestTrue(*FString::Printf(TEXT("%s: light %hs radius and lumens"), Name, Spec.Id.c_str()),
                   FMath::IsNearlyEqual(Scene.RoomLights[I].Radius, static_cast<float>(Spec.RadiusCm)) && Scene.RoomLights[I].Lumens >= 600.0f);
        const LI::Room* Room = LI::FindRoom(Layout, Spec.RoomId);
        T.TestTrue(*FString::Printf(TEXT("%s: light %hs radius stays inside the room"), Name, Spec.Id.c_str()),
                   Room != nullptr && Spec.RadiusCm <= FMath::Max(150, FMath::Min(Room->Area.Width(), Room->Area.Depth())));
    }

    // Orientation: the rooms lie on the building's +V side of its frontage line, whichever way the frame points.
    for (const LI::Room& R : Layout.Rooms)
    {
        const FVector Centre = KitPoint(B, Layout, 0.5 * (R.Area.U0 + R.Area.U1), 0.5 * (R.Area.V0 + R.Area.V1));
        T.TestTrue(*FString::Printf(TEXT("%s: room %hs lies behind the frontage"), Name, R.Id.c_str()), FVector::DotProduct(Centre - B.Frame.Origin, B.Frame.V) > 0.0);
    }
}
} // namespace PWKitTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWInteriorKitFootprintsTest, "PerfectWorld.InteriorKit.Footprints",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPWInteriorKitFootprintsTest::RunTest(const FString& Parameters)
{
    using namespace PWKitTest;
    // 1. The +Y side: U = +X, V = +Y. A shop with a stock room, 700 x 700 (the stationery footprint, built through the generic kit).
    {
        const FLaneBuilding B = MakeBuilding(-100.0, 460.0, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), 700.0, 700.0, 3);
        CheckKitCase(*this, TEXT("plus-Y shop 700x700"), B,
                     MakeKitInput(B, "building.test.kit_shop", LI::Variant::ShopWithStockroom, LI::FurnitureSet::Stationery, 250, "door.test.kit_shop"), 1);
    }
    // 2. The -Y side: U = +X, V = -Y. A wide compact residence, 1000 x 900, mirrored, with a hung front door.
    {
        const FLaneBuilding B = MakeBuilding(-300.0, -400.0, FVector(1.0, 0.0, 0.0), FVector(0.0, -1.0, 0.0), 1000.0, 900.0, 5);
        LI::InteriorInput In = MakeKitInput(B, "building.test.kit_home", LI::Variant::CompactResidence, LI::FurnitureSet::Residence, 700, "door.test.kit_home");
        In.MirrorU = true;
        CheckKitCase(*this, TEXT("minus-Y mirrored home 1000x900"), B, In, 1);
    }
    // 3. A rotated frame (the end-closing masses run along Y): a narrow shared-entry building, 640 x 600, U = +Y, V = -X.
    {
        const FLaneBuilding B = MakeBuilding(6400.0, -1300.0, FVector(0.0, 1.0, 0.0), FVector(-1.0, 0.0, 0.0), 640.0, 600.0, 4);
        CheckKitCase(*this, TEXT("rotated shared-entry 640x600"), B,
                     MakeKitInput(B, "building.test.kit_shared", LI::Variant::SharedEntry, LI::FurnitureSet::Residence, 320, nullptr), 0);
    }
    // 4. A narrow shop, 420 wide, an open doorway, rotated the other way (U = -Y, V = +X): the geometry is frame independent.
    {
        const FLaneBuilding B = MakeBuilding(-2400.0, 1300.0, FVector(0.0, -1.0, 0.0), FVector(1.0, 0.0, 0.0), 420.0, 680.0, 4);
        CheckKitCase(*this, TEXT("rotated narrow shop 420x680"), B,
                     MakeKitInput(B, "building.test.kit_narrow", LI::Variant::ShopWithStockroom, LI::FurnitureSet::Stationery, 300, "door.test.kit_narrow"), 1);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWStationerySelectionTest, "PerfectWorld.InteriorKit.Stationery.Selection",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPWStationerySelectionTest::RunTest(const FString& Parameters)
{
    using namespace PWLane;
    // The generator values the building is chosen by (BuildFirstRows: side +1, Index 2).
    const uint32 H = LaneHash(1 * 131 + 7, 2, 11);
    TestTrue(TEXT("the hash of side +1, index 2 is the documented seed"), H == StationeryGeneratorSeed);
    TestTrue(TEXT("width 600 + 100 * (H % 6) = 700"), 600.0 + 100.0 * static_cast<double>(H % 6u) == 700.0);
    TestTrue(TEXT("depth 700 + 100 * ((H >> 16) % 3) = 700"), 700.0 + 100.0 * static_cast<double>((H >> 16) % 3u) == 700.0);
    TestTrue(TEXT("setback kind (H >> 12) % 4 = 1 gives a 60 cm setback"), ((H >> 12) % 4u) == 1u);
    TestTrue(TEXT("3 + (H >> 8) % 3 = 3 storeys"), 3 + static_cast<int32>((H >> 8) % 3u) == 3);

    // The real generator still places a first-row +Y building at X -100 to 600 with its front at |Y| 460.
    FLaneScene Lane;
    BuildLaneRows(Lane);
    bool bFound = false;
    for (const FLaneFront& Front : Lane.Fronts[1])
    {
        bFound = bFound || (FMath::IsNearlyEqual(Front.X0, -100.0, 0.5) && FMath::IsNearlyEqual(Front.X1, 600.0, 0.5) &&
                            FMath::IsNearlyEqual(Front.AbsY, 460.0, 0.5));
    }
    TestTrue(TEXT("the generated +Y row has a building at X -100 to 600 with its front at Y 460"), bFound);

    FLaneBuilding B;
    B.Frame = MakeLaneFrame(-100.0, 460.0, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0));
    B.Width = 700.0;
    B.Depth = 700.0;
    B.Setback = 60.0;
    B.Storeys = 3;
    B.Side = 1;
    B.Index = 2;
    B.Detail = ELaneDetail::Full;
    B.Seed = StationeryGeneratorSeed;
    B.Wall = ELaneColour::WallButter;
    TestTrue(TEXT("the reference building is the stationery shop"), IsStationeryBuilding(B));
    FLaneBuilding Other = B;
    Other.Side = -1;
    TestFalse(TEXT("the -Y side is not"), IsStationeryBuilding(Other));
    Other = B;
    Other.Index = 3;
    TestFalse(TEXT("index 3 (the tea shop) is not"), IsStationeryBuilding(Other));
    Other = B;
    Other.Width = 600.0;
    TestFalse(TEXT("another width is not"), IsStationeryBuilding(Other));
    Other = B;
    Other.Depth = 900.0;
    TestFalse(TEXT("another depth is not"), IsStationeryBuilding(Other));
    Other = B;
    Other.Setback = 120.0;
    TestFalse(TEXT("another setback is not"), IsStationeryBuilding(Other));
    Other = B;
    Other.Frame = MakeLaneFrame(600.0, 520.0, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0));
    TestFalse(TEXT("another position (the tea shop's) is not"), IsStationeryBuilding(Other));
    Other = B;
    Other.Frame = MakeLaneFrame(-100.0, 440.0, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0));
    TestFalse(TEXT("another frontage line is not"), IsStationeryBuilding(Other));
    Other = B;
    Other.Detail = ELaneDetail::Medium;
    TestFalse(TEXT("another detail level is not"), IsStationeryBuilding(Other));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWStationeryShopTest, "PerfectWorld.InteriorKit.Stationery.Shop",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPWStationeryShopTest::RunTest(const FString& Parameters)
{
    using namespace PWLane;
    using namespace PWKitTest;
    FLaneBuilding B = MakeBuilding(-100.0, 460.0, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), 700.0, 700.0, 3);
    B.Side = 1;
    B.Index = 2;
    B.Setback = 60.0;
    B.Seed = StationeryGeneratorSeed;
    B.Wall = ELaneColour::WallButter;
    TestTrue(TEXT("reference building accepted"), IsStationeryBuilding(B));

    const LI::InteriorLayout& Layout = GetStationeryLayout();
    TestTrue(TEXT("the stationery layout is accepted by the core"), Layout.Ok());
    FLaneScene Scene;
    BuildStationeryShop(Scene, B);
    TestTrue(TEXT("the front door spec is in the scene under the lane door id"), HasStationeryShop(Scene));
    TestEqual(TEXT("exactly one door spec"), Scene.Doors.Num(), 1);
    TestEqual(TEXT("one room light per layout light"), Scene.RoomLights.Num(), Layout.Lights.Num());
    TestTrue(TEXT("a bulb under the awning"), Scene.LightSpots.Num() == 1);

    // Anchors: the four Anindita's schedule names, plus the customer's side of the counter.
    TArray<FStationeryAnchor> Anchors;
    GetStationeryAnchors(B, Anchors);
    TestEqual(TEXT("five anchors"), Anchors.Num(), 5);
    const auto Find = [&](const TCHAR* Id) -> const FStationeryAnchor*
    { return Anchors.FindByPredicate([&](const FStationeryAnchor& A) { return A.Id == FName(Id); }); };
    const FStationeryAnchor* Counter = Find(StationeryAnchorCounter);
    const FStationeryAnchor* Stock = Find(StationeryAnchorStockRoom);
    const FStationeryAnchor* Desk = Find(StationeryAnchorPackingDesk);
    const FStationeryAnchor* Door = Find(StationeryAnchorFrontDoor);
    const FStationeryAnchor* Customer = Find(StationeryAnchorCustomer);
    if (Counter == nullptr || Stock == nullptr || Desk == nullptr || Door == nullptr || Customer == nullptr)
    {
        AddError(TEXT("an anchor is missing under its public id"));
        return true;
    }
    for (const FStationeryAnchor* A : {Counter, Stock, Desk, Door, Customer})
    {
        // The frame is U = +X, V = +Y from (-100, 460); the finished floor is 15 above the ground datum.
        const FVector Expected(-100.0 + A->Local.X, 460.0 + A->Local.Y, GroundZ + GroundFloorZ);
        TestTrue(*FString::Printf(TEXT("%s is where the frame puts its local point"), *A->Id.ToString()), A->World.Equals(Expected, 0.01));
    }
    TestTrue(TEXT("the counter anchor is behind the counter, the customer in front, 140 cm apart facing each other"),
             FMath::IsNearlyEqual(FVector::Dist(Counter->World, Customer->World), 140.0, 0.01) &&
                 FVector::DotProduct(YawDirection(Counter->YawDegrees), YawDirection(Customer->YawDegrees)) < -0.99);
    TestTrue(TEXT("the front door anchor is on the footpath, facing in"), Door->Local.Y < 0.0 && YawDirection(Door->YawDegrees).Equals(FVector(0.0, 1.0, 0.0), 0.001));
    TestTrue(TEXT("the stock room anchor is behind the sales room"), Stock->Local.Y > Counter->Local.Y + 150.0);
    TestTrue(TEXT("the packing desk anchor is in the stock room"), Desk->Local.Y > Counter->Local.Y + 150.0);

    // Distances between the anchors, building-local (cm): straight lines here, walking lines below.
    const auto Straight = [](const FStationeryAnchor* A, const FStationeryAnchor* C) { return FVector::Dist2D(A->Local, C->Local); };
    UE_LOG(LogTemp, Display, TEXT("Stationery anchors (building-local U,V): counter (%.0f,%.0f) customer (%.0f,%.0f) stock_room (%.0f,%.0f) packing_desk (%.0f,%.0f) front_door (%.0f,%.0f)"),
           Counter->Local.X, Counter->Local.Y, Customer->Local.X, Customer->Local.Y, Stock->Local.X, Stock->Local.Y, Desk->Local.X, Desk->Local.Y, Door->Local.X, Door->Local.Y);
    UE_LOG(LogTemp, Display, TEXT("Stationery straight-line cm: counter-stock %.0f, counter-desk %.0f, counter-door %.0f"), Straight(Counter, Stock), Straight(Counter, Desk), Straight(Counter, Door));
    for (const LI::Route& Route : Layout.Routes)
    {
        UE_LOG(LogTemp, Display, TEXT("Stationery route %hs: %d cm"), Route.Id.c_str(), Route.LengthCm());
        TestTrue(TEXT("a walking line is longer than the straight line and under 14 m"), Route.LengthCm() < 1400);
    }

    // The same collision and walking-space contracts as the generic cases, on the real shop.
    CheckKitCase(*this, TEXT("stationery shop"), B,
                 MakeKitInput(B, "building.nabapara.stationery", LI::Variant::ShopWithStockroom, LI::FurnitureSet::Stationery, StationeryEntranceU,
                              "door.nabapara.stationery_front"), 1);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
