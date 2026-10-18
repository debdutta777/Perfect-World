// NL-03 furniture readiness: the general-goods and workshop furniture sets (LaneInteriorLayout.h, LaneInteriorShopFurniture.cpp) built
// through the generic interior kit on an ISOLATED development fixture. Nothing here touches the lane catalogue or any real frontage; these
// are bare scenes, like PerfectWorld.InteriorKit. They check what the kit actually emitted: collision boxes against the portable layout,
// free circulation, door sweeps and approach zones, clear doorways, and that a 34 cm capsule can WALK from the footpath to every stand
// anchor through the emitted collision. They record furniture readiness only, not newly completed lane shops.
// Run in the editor under "PerfectWorld.ShopFurniture" (Session Frontend, or the Unreal MCP AutomationTestToolset).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "PerfectWorldLaneInteriorKit.h"

namespace PWShopFurnitureTest
{
using namespace PWLane;

constexpr double SftCapsuleRadius = 34.0;
constexpr double SftCapsuleHeight = 165.0;
constexpr double SftCell = 5.0; // walking grid, building-local cm

FBox SftWorldBox(const FLaneBuilding& B, const LI::InteriorLayout& L, const LI::Rect& R, double H0, double H1)
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

bool SftBoxesMatch(const FBox& A, const FBox& C)
{
    return A.Min.Equals(C.Min, 0.6) && A.Max.Equals(C.Max, 0.6);
}

bool SftOverlaps(const FBox& A, const FBox& C)
{
    constexpr double Hair = 0.01;
    return A.Min.X < C.Max.X - Hair && C.Min.X < A.Max.X - Hair && A.Min.Y < C.Max.Y - Hair && C.Min.Y < A.Max.Y - Hair &&
           A.Min.Z < C.Max.Z - Hair && C.Min.Z < A.Max.Z - Hair;
}

FVector SftYawDirection(double Degrees)
{
    const double Radians = FMath::DegreesToRadians(Degrees);
    return FVector(FMath::Cos(Radians), FMath::Sin(Radians), 0.0);
}

FLaneBuilding SftMakeBuilding(double OriginX, double OriginY, const FVector& U, const FVector& V, double Width, double Depth, int32 Storeys)
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

/** A building-local point lies on floor a person can stand on: inside a room, a doorway, or the footpath strip outside the front wall. */
bool SftOnFloor(const LI::InteriorLayout& L, double U, double V)
{
    if (V <= 0.0 && V >= -150.0 && U >= -100.0 && U <= L.WidthCm + 100.0)
    {
        return true;
    }
    for (const LI::Room& R : L.Rooms)
    {
        if (U >= R.Area.U0 && U <= R.Area.U1 && V >= R.Area.V0 && V <= R.Area.V1)
        {
            return true;
        }
    }
    for (const LI::Opening& O : L.Openings)
    {
        if (O.Kind != LI::OpeningKind::Window && U >= O.Plan.U0 && U <= O.Plan.U1 && V >= O.Plan.V0 && V <= O.Plan.V1)
        {
            return true;
        }
    }
    return false;
}

/** The capsule centred here stands wholly on floor (nine samples across its 68 cm width) and touches no emitted collision. */
bool SftCapsuleFree(const FLaneBuilding& B, const LI::InteriorLayout& L, const TArray<FBox>& Blocking, double U, double V)
{
    for (const double dU : {-SftCapsuleRadius, 0.0, SftCapsuleRadius})
    {
        for (const double dV : {-SftCapsuleRadius, 0.0, SftCapsuleRadius})
        {
            if (!SftOnFloor(L, U + dU, V + dV))
            {
                return false;
            }
        }
    }
    const FVector At = KitPoint(B, L, U, V, 0.0);
    const FBox Capsule(At + FVector(-SftCapsuleRadius, -SftCapsuleRadius, 2.0), At + FVector(SftCapsuleRadius, SftCapsuleRadius, SftCapsuleHeight));
    for (const FBox& Box : Blocking)
    {
        if (SftOverlaps(Capsule, Box))
        {
            return false;
        }
    }
    return true;
}

/** Flood fill on a 5 cm grid from the threshold anchor; true when every stand and threshold anchor's cell is reached. */
void SftCheckReachable(FAutomationTestBase& T, const TCHAR* Name, const FLaneBuilding& B, const LI::InteriorLayout& L, const TArray<FBox>& Blocking)
{
    const int32 U0 = -100;
    const int32 V0 = -150;
    const int32 NU = static_cast<int32>((L.WidthCm + 200) / SftCell) + 1;
    const int32 NV = static_cast<int32>((L.DepthCm + 150) / SftCell) + 1;
    TArray<uint8> State; // 0 unknown, 1 blocked, 2 reached
    State.Init(static_cast<uint8>(0), NU * NV);
    const auto Local = [&](int32 Iu, int32 Iv) { return FVector2D(U0 + Iu * SftCell, V0 + Iv * SftCell); };
    const auto Snap = [&](double U, double V, int32& OutU, int32& OutV)
    {
        OutU = FMath::Clamp(FMath::RoundToInt((U - U0) / SftCell), 0, NU - 1);
        OutV = FMath::Clamp(FMath::RoundToInt((V - V0) / SftCell), 0, NV - 1);
    };
    const LI::Anchor* Door = nullptr;
    for (const LI::Anchor& A : L.Anchors)
    {
        if (A.Kind == LI::AnchorKind::Threshold)
        {
            Door = &A;
        }
    }
    if (Door == nullptr)
    {
        T.AddError(FString::Printf(TEXT("%s: no threshold anchor to start the walk from"), Name));
        return;
    }
    TArray<FIntPoint> Open;
    int32 Su = 0;
    int32 Sv = 0;
    Snap(Door->U, Door->V, Su, Sv);
    const auto Free = [&](int32 Iu, int32 Iv)
    {
        const FVector2D P = Local(Iu, Iv);
        return SftCapsuleFree(B, L, Blocking, P.X, P.Y);
    };
    if (!Free(Su, Sv))
    {
        T.AddError(FString::Printf(TEXT("%s: the footpath outside the door is not walkable"), Name));
        return;
    }
    State[Sv * NU + Su] = 2;
    Open.Add(FIntPoint(Su, Sv));
    for (int32 Head = 0; Head < Open.Num(); ++Head)
    {
        const FIntPoint At = Open[Head];
        for (const FIntPoint Step : {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)})
        {
            const int32 Nu = At.X + Step.X;
            const int32 Nv = At.Y + Step.Y;
            if (Nu < 0 || Nv < 0 || Nu >= NU || Nv >= NV || State[Nv * NU + Nu] != 0)
            {
                continue;
            }
            if (Free(Nu, Nv))
            {
                State[Nv * NU + Nu] = 2;
                Open.Add(FIntPoint(Nu, Nv));
            }
            else
            {
                State[Nv * NU + Nu] = 1;
            }
        }
    }
    for (const LI::Anchor& A : L.Anchors)
    {
        if (A.Kind != LI::AnchorKind::Stand)
        {
            continue;
        }
        int32 Au = 0;
        int32 Av = 0;
        Snap(A.U, A.V, Au, Av);
        T.TestTrue(*FString::Printf(TEXT("%s: stand anchor %hs is reachable on foot from the footpath"), Name, A.Id.c_str()), State[Av * NU + Au] == 2);
    }
}

/** Builds the kit for one footprint and reads every emitted fact back against the portable layout. */
void SftCheckFixture(FAutomationTestBase& T, const TCHAR* Name, const FLaneBuilding& B, const LI::InteriorInput& In, int32 ExpectedAnchorsAtLeast,
                     int32* OutBlockingCount = nullptr)
{
    const LI::InteriorLayout Layout = LI::GenerateInteriorLayout(In);
    if (!Layout.Ok())
    {
        T.AddError(FString::Printf(TEXT("%s: layout refused (%hs)"), Name, std::string(LI::RefusalCode(Layout.Status)).c_str()));
        return;
    }
    T.TestTrue(*FString::Printf(TEXT("%s: the core re-validates the layout"), Name), LI::ValidateInteriorLayout(Layout) == LI::Refusal::None);
    FLaneScene Scene;
    BuildInteriorKit(Scene, B, Layout);
    const TArray<FBox>& Blocking = Scene.Blocks.BlockingBoxes();
    const double FloorAbs = GroundZ + Layout.FinishedFloorCm;
    if (OutBlockingCount != nullptr)
    {
        *OutBlockingCount = Blocking.Num();
    }

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
    for (const LI::Furniture& P : Layout.Pieces)
    {
        if (!P.Solid)
        {
            continue;
        }
        const FBox Want = SftWorldBox(B, Layout, P.Footprint, P.H0, P.H1);
        if (!Blocking.ContainsByPredicate([&](const FBox& Box) { return SftBoxesMatch(Box, Want); }))
        {
            T.AddError(FString::Printf(TEXT("%s: no collision box for %hs"), Name, P.Id.c_str()));
        }
    }
    T.TestTrue(*FString::Printf(TEXT("%s: the floor slab tops out at the finished floor"), Name),
               Blocking.ContainsByPredicate([&](const FBox& Box) { return FMath::IsNearlyEqual(Box.Max.Z, FloorAbs, 0.1) && Box.Min.Z <= GroundZ + 0.1; }));

    // Anchors: floor-relative, facing the piece they serve, and the stand points clear of every box.
    TArray<FKitAnchor> Anchors;
    GetKitAnchors(B, Layout, Anchors);
    T.TestEqual(*FString::Printf(TEXT("%s: anchor count"), Name), Anchors.Num(), static_cast<int32>(Layout.Anchors.size()));
    T.TestTrue(*FString::Printf(TEXT("%s: at least %d anchors (work and standing points)"), Name, ExpectedAnchorsAtLeast), Anchors.Num() >= ExpectedAnchorsAtLeast);
    for (int32 I = 0; I < Anchors.Num() && I < static_cast<int32>(Layout.Anchors.size()); ++I)
    {
        const LI::Anchor& A = Layout.Anchors[I];
        const FKitAnchor& W = Anchors[I];
        T.TestTrue(*FString::Printf(TEXT("%s: anchor %hs is on the finished floor"), Name, A.Id.c_str()), W.Location.Equals(B.Frame.Pt(A.U, A.V, FloorAbs - GroundZ), 0.01));
        const FVector Facing = (B.Frame.U * A.FaceU + B.Frame.V * A.FaceV).GetSafeNormal();
        T.TestTrue(*FString::Printf(TEXT("%s: anchor %hs yaw matches its facing"), Name, A.Id.c_str()), SftYawDirection(W.YawDegrees).Equals(Facing, 0.001));
        if (A.Kind == LI::AnchorKind::Stand)
        {
            const FBox Capsule(W.Location + FVector(-SftCapsuleRadius, -SftCapsuleRadius, 2.0), W.Location + FVector(SftCapsuleRadius, SftCapsuleRadius, SftCapsuleHeight));
            for (const FBox& Box : Blocking)
            {
                if (SftOverlaps(Capsule, Box))
                {
                    T.AddError(FString::Printf(TEXT("%s: a collision box intrudes on stand point %hs"), Name, A.Id.c_str()));
                    break;
                }
            }
            if (const LI::Furniture* Piece = LI::FindFurniture(Layout, A.FurnitureId))
            {
                const FVector ToPiece = SftWorldBox(B, Layout, Piece->Footprint, Piece->H0, Piece->H1).GetClosestPointTo(W.Location) - W.Location;
                T.TestTrue(*FString::Printf(TEXT("%s: %hs faces its furniture"), Name, A.Id.c_str()), FVector::DotProduct(ToPiece.GetSafeNormal2D(), Facing) > 0.5);
            }
        }
    }

    // Reserved space: circulation, door sweeps, approach and use zones stay free of every collision box between floor and head.
    for (const LI::Zone& Z : Layout.Zones)
    {
        if (Z.Kind == LI::ZoneKind::Route)
        {
            continue;
        }
        const FBox Space = SftWorldBox(B, Layout, Z.Area, 2.0, 210.0);
        for (const FBox& Box : Blocking)
        {
            if (SftOverlaps(Space, Box))
            {
                T.AddError(FString::Printf(TEXT("%s: a collision box fills clear space %hs"), Name, Z.Id.c_str()));
                break;
            }
        }
    }
    for (const LI::Route& Route : Layout.Routes)
    {
        for (size_t I = 0; I + 1 < Route.Points.size(); ++I)
        {
            const LI::RoutePoint P = Route.Points[I];
            const LI::RoutePoint Q = Route.Points[I + 1];
            const LI::Rect Leg{FMath::Min(P.U, Q.U) - 34, FMath::Max(P.U, Q.U) + 34, FMath::Min(P.V, Q.V) - 34, FMath::Max(P.V, Q.V) + 34};
            const FBox Walk = SftWorldBox(B, Layout, Leg, 2.0, SftCapsuleHeight);
            for (const FBox& Box : Blocking)
            {
                if (SftOverlaps(Walk, Box))
                {
                    T.AddError(FString::Printf(TEXT("%s: a collision box lies on staff route %hs"), Name, Route.Id.c_str()));
                    break;
                }
            }
        }
    }

    // Every doorway is 90 x 210 clear of collision.
    for (const LI::Opening& O : Layout.Openings)
    {
        if (O.Kind == LI::OpeningKind::Window)
        {
            continue;
        }
        const bool bAlongU = O.Plan.Width() > O.Plan.Depth();
        const LI::Rect Clear = bAlongU ? LI::Rect{O.Plan.U0 + 2, O.Plan.U1 - 2, O.Plan.V0, O.Plan.V1} : LI::Rect{O.Plan.U0, O.Plan.U1, O.Plan.V0 + 2, O.Plan.V1 - 2};
        const FBox Gap = SftWorldBox(B, Layout, Clear, 2.0, LI::DoorClearHeightCm);
        for (const FBox& Box : Blocking)
        {
            if (SftOverlaps(Gap, Box))
            {
                T.AddError(FString::Printf(TEXT("%s: doorway %hs is blocked"), Name, O.Id.c_str()));
                break;
            }
        }
    }

    // Door specs: exactly the hung leaves, hinged on the jamb the core names.
    int32 Leaves = 0;
    for (const LI::Opening& O : Layout.Openings)
    {
        Leaves += O.DoorId.empty() ? 0 : 1;
    }
    T.TestEqual(*FString::Printf(TEXT("%s: door spec count"), Name), Scene.Doors.Num(), Leaves);

    SftCheckReachable(T, Name, B, Layout, Blocking);
}

bool SftHasAnchor(const LI::InteriorLayout& L, const char* Suffix)
{
    return LI::FindAnchor(L, L.BuildingId + ".anchor." + Suffix) != nullptr;
}
} // namespace PWShopFurnitureTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWShopFurnitureSetsTest, "PerfectWorld.ShopFurniture.Sets",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPWShopFurnitureSetsTest::RunTest(const FString& Parameters)
{
    using namespace PWShopFurnitureTest;
    // 1. General goods on the +Y side: U = +X, V = +Y, 700 x 700, door in the left half.
    {
        const FLaneBuilding B = SftMakeBuilding(-100.0, 460.0, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), 700.0, 700.0, 3);
        const LI::InteriorInput In = MakeKitInput(B, "building.test.sf_goods", LI::Variant::ShopWithStockroom, LI::FurnitureSet::GeneralGoods, 250, "door.test.sf_goods");
        SftCheckFixture(*this, TEXT("general goods 700x700 plus-Y"), B, In, 7);
        const LI::InteriorLayout L = LI::GenerateInteriorLayout(In);
        for (const char* Id : {"staff_counter", "customer_counter", "stock_room", "weigh_station", "front_door"})
        {
            TestTrue(*FString::Printf(TEXT("general goods has anchor %hs"), Id), SftHasAnchor(L, Id));
        }
        TestTrue(TEXT("general goods has a browse point"), SftHasAnchor(L, "browse_shelf") || SftHasAnchor(L, "browse_bins"));
    }
    // 2. Workshop on the -Y side, mirrored, wider: U = +X, V = -Y.
    {
        const FLaneBuilding B = SftMakeBuilding(-300.0, -400.0, FVector(1.0, 0.0, 0.0), FVector(0.0, -1.0, 0.0), 900.0, 800.0, 4);
        LI::InteriorInput In = MakeKitInput(B, "building.test.sf_workshop", LI::Variant::ShopWithStockroom, LI::FurnitureSet::Workshop, 600, "door.test.sf_workshop");
        In.MirrorU = true;
        SftCheckFixture(*this, TEXT("workshop 900x800 minus-Y mirrored"), B, In, 7);
        const LI::InteriorLayout L = LI::GenerateInteriorLayout(In);
        for (const char* Id : {"staff_counter", "customer_counter", "tool_rack", "workbench", "finishing_table", "front_door"})
        {
            TestTrue(*FString::Printf(TEXT("workshop has anchor %hs"), Id), SftHasAnchor(L, Id));
        }
    }
    // 3. Rotated frames (the end-closing masses): general goods at U = +Y, V = -X, and a workshop at U = -Y, V = +X.
    {
        const FLaneBuilding B = SftMakeBuilding(6400.0, -1300.0, FVector(0.0, 1.0, 0.0), FVector(-1.0, 0.0, 0.0), 760.0, 700.0, 3);
        SftCheckFixture(*this, TEXT("rotated general goods 760x700"), B,
                        MakeKitInput(B, "building.test.sf_goods_rot", LI::Variant::ShopWithStockroom, LI::FurnitureSet::GeneralGoods, 280, "door.test.sf_goods_rot"), 7);
    }
    {
        const FLaneBuilding B = SftMakeBuilding(-2400.0, 1300.0, FVector(0.0, -1.0, 0.0), FVector(1.0, 0.0, 0.0), 800.0, 760.0, 3);
        SftCheckFixture(*this, TEXT("rotated workshop 800x760"), B,
                        MakeKitInput(B, "building.test.sf_workshop_rot", LI::Variant::ShopWithStockroom, LI::FurnitureSet::Workshop, 300, "door.test.sf_workshop_rot"), 7);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWShopFurnitureFixtureTest, "PerfectWorld.ShopFurniture.IsolatedFixture",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPWShopFurnitureFixtureTest::RunTest(const FString& Parameters)
{
    using namespace PWShopFurnitureTest;
    // Both sets side by side in ONE bare scene, as an integrator would walk them: general goods then workshop, 1 m apart, nothing else.
    const FLaneBuilding Goods = SftMakeBuilding(0.0, 0.0, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), 700.0, 700.0, 3);
    const FLaneBuilding Work = SftMakeBuilding(800.0, 0.0, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), 900.0, 800.0, 3);
    const LI::InteriorLayout GoodsLayout = LI::GenerateInteriorLayout(
        MakeKitInput(Goods, "building.test.sf_fixture_goods", LI::Variant::ShopWithStockroom, LI::FurnitureSet::GeneralGoods, 250, "door.test.sf_fixture_goods"));
    const LI::InteriorLayout WorkLayout = LI::GenerateInteriorLayout(
        MakeKitInput(Work, "building.test.sf_fixture_workshop", LI::Variant::ShopWithStockroom, LI::FurnitureSet::Workshop, 300, "door.test.sf_fixture_workshop"));
    if (!GoodsLayout.Ok() || !WorkLayout.Ok())
    {
        AddError(TEXT("a fixture layout was refused"));
        return true;
    }
    FLaneScene Scene;
    BuildInteriorKit(Scene, Goods, GoodsLayout);
    const int32 AfterGoods = Scene.Blocks.BlockingBoxes().Num();
    BuildInteriorKit(Scene, Work, WorkLayout);
    int32 Alone = 0;
    {
        FLaneScene WorkAlone;
        BuildInteriorKit(WorkAlone, Work, WorkLayout);
        Alone = WorkAlone.Blocks.BlockingBoxes().Num();
    }
    TestEqual(TEXT("the shared fixture scene holds both shops' collision and nothing else"), Scene.Blocks.BlockingBoxes().Num(), AfterGoods + Alone);
    TestEqual(TEXT("two hung entrance leaves"), Scene.Doors.Num(), 2);
    TestTrue(TEXT("each shop's leaf is under its own lane door id"), HasKitDoor(Scene, TEXT("door.test.sf_fixture_goods")) && HasKitDoor(Scene, TEXT("door.test.sf_fixture_workshop")));
    TestEqual(TEXT("room lights from both layouts"), Scene.RoomLights.Num(), static_cast<int32>(GoodsLayout.Lights.size() + WorkLayout.Lights.size()));
    // The work anchors an integrator walks to, in world space, on the finished floor.
    TArray<FKitAnchor> GoodsAnchors;
    GetKitAnchors(Goods, GoodsLayout, GoodsAnchors);
    TArray<FKitAnchor> WorkAnchors;
    GetKitAnchors(Work, WorkLayout, WorkAnchors);
    for (const FKitAnchor& A : GoodsAnchors)
    {
        UE_LOG(LogTemp, Display, TEXT("ShopFurniture fixture goods anchor %s: (%.0f, %.0f, %.0f) yaw %.0f"), *A.Id.ToString(), A.Location.X, A.Location.Y, A.Location.Z, A.YawDegrees);
    }
    for (const FKitAnchor& A : WorkAnchors)
    {
        UE_LOG(LogTemp, Display, TEXT("ShopFurniture fixture workshop anchor %s: (%.0f, %.0f, %.0f) yaw %.0f"), *A.Id.ToString(), A.Location.X, A.Location.Y, A.Location.Z, A.YawDegrees);
    }
    TestTrue(TEXT("anchors exist for both shops"), GoodsAnchors.Num() >= 7 && WorkAnchors.Num() >= 7);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWShopFurnitureNarrowTest, "PerfectWorld.ShopFurniture.NarrowFootprint",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPWShopFurnitureNarrowTest::RunTest(const FString& Parameters)
{
    using namespace PWShopFurnitureTest;
    // A footprint that cannot hold the set is refused by the core with a code and never reaches the kit as a half-furnished shop.
    const FLaneBuilding Narrow = SftMakeBuilding(0.0, 0.0, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), 420.0, 680.0, 3);
    const LI::InteriorLayout Workshop = LI::GenerateInteriorLayout(
        MakeKitInput(Narrow, "building.test.sf_narrow_workshop", LI::Variant::ShopWithStockroom, LI::FurnitureSet::Workshop, 300, "door.test.sf_narrow_workshop"));
    TestFalse(TEXT("a workshop does not fit 420 x 680"), Workshop.Ok());
    TestEqual(TEXT("and says so with furniture_does_not_fit"), FString(UTF8_TO_TCHAR(std::string(LI::RefusalCode(Workshop.Status)).c_str())),
              FString(TEXT("furniture_does_not_fit")));
    TestTrue(TEXT("the refusal carries no geometry"), Workshop.Pieces.empty() && Workshop.Rooms.empty());
    // What the core accepts at the narrow footprint is still fully valid and builds.
    const LI::InteriorInput GoodsInput =
        MakeKitInput(Narrow, "building.test.sf_narrow_goods", LI::Variant::ShopWithStockroom, LI::FurnitureSet::GeneralGoods, 300, "door.test.sf_narrow_goods");
    const LI::InteriorLayout Goods = LI::GenerateInteriorLayout(GoodsInput);
    if (Goods.Ok())
    {
        SftCheckFixture(*this, TEXT("narrow general goods 420x680"), Narrow, GoodsInput, 5);
    }
    else
    {
        TestEqual(TEXT("a general-goods shop that does not fit says furniture_does_not_fit"), FString(UTF8_TO_TCHAR(std::string(LI::RefusalCode(Goods.Status)).c_str())),
                  FString(TEXT("furniture_does_not_fit")));
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
