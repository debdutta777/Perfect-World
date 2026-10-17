// Checks for the generic kit-built frontages (PerfectWorldLaneKitFrontages.h): every batch frontage is recognised by its pinned
// footprint and refused for a perturbed one, its emitted collision leaves clear 90 x 210 doorways and 120 cm circulation, and no
// frontage overlaps the tea shop, the stationery shop, the children's home or the opening home. They run the real builders on bare
// scenes and read back the collision boxes; no world is needed.
// Run in the editor under "PerfectWorld.KitFrontages" (Session Frontend, or the Unreal MCP AutomationTestToolset).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "PerfectWorldLaneCareInterior.h"
#include "PerfectWorldLaneInteriors.h"
#include "PerfectWorldLaneKitFrontages.h"
#include "PerfectWorldLaneProps.h"
#include "PerfectWorldLaneStationeryShop.h"
#include "PerfectWorldLaneTeaShop.h"

namespace PWKitFrontTest
{
using namespace PWLane;

constexpr double KfCapsuleRadius = 34.0;
constexpr double KfCapsuleHeight = 165.0;

/** A first-row building exactly as BuildFirstRows makes it. */
FLaneBuilding KfMakeBuilding(int32 Side, int32 Index, double X, double Width, double Depth, double Setback, int32 Storeys, uint32 Seed)
{
    FLaneBuilding B;
    B.Width = Width;
    B.Depth = Depth;
    B.Setback = Setback;
    B.Storeys = Storeys;
    B.Side = Side;
    B.Index = Index;
    B.Seed = Seed;
    B.Detail = ELaneDetail::Full;
    B.Wall = ELaneColour::WallCream;
    B.Frame = MakeLaneFrame(X, Side * (FootpathOuter + Setback), FVector(1.0, 0.0, 0.0), FVector(0.0, static_cast<double>(Side), 0.0));
    return B;
}

FLaneBuilding KfFromRow(const LI::FrontageEntry& Row)
{
    return KfMakeBuilding(Row.Side, Row.RowIndex, Row.OriginXCm, Row.WidthCm, Row.DepthCm, Row.SetbackCm, Row.Storeys, Row.Seed);
}

/** The four ground floors built by their own code, as the generator makes them (seeds from LaneHash(Side * 131 + 7, Index, 11)). */
struct FKfBuilt
{
    const TCHAR* Name;
    FLaneBuilding Building;
};

TArray<FKfBuilt> KfBuiltBuildings()
{
    TArray<FKfBuilt> Built;
    Built.Add({TEXT("children's home"), KfMakeBuilding(-1, 1, -1400.0, 1100.0, 900.0, 0.0, 3, 408894755u)});
    Built.Add({TEXT("opening home"), KfMakeBuilding(-1, 2, -300.0, 1000.0, 900.0, 0.0, 5, 1655793196u)});
    Built.Add({TEXT("stationery shop"), KfMakeBuilding(1, 2, -100.0, 700.0, 700.0, 60.0, 3, StationeryGeneratorSeed)});
    Built.Add({TEXT("tea shop"), KfMakeBuilding(1, 3, 600.0, 700.0, 900.0, 120.0, 3, 10160749u)});
    return Built;
}

bool KfBuiltPredicate(const FKfBuilt& Built)
{
    const FLaneBuilding& B = Built.Building;
    return IsCareHomeBuilding(B) || IsOpeningHome(B) || IsStationeryBuilding(B) || IsTeaShopBuilding(B);
}

FBox KfWorldBox(const FLaneBuilding& B, const LI::InteriorLayout& L, const LI::Rect& R, double H0, double H1)
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

bool KfOverlaps(const FBox& A, const FBox& B)
{
    constexpr double Hair = 0.01;
    return A.Min.X < B.Max.X - Hair && B.Min.X < A.Max.X - Hair && A.Min.Y < B.Max.Y - Hair && B.Min.Y < A.Max.Y - Hair &&
           A.Min.Z < B.Max.Z - Hair && B.Min.Z < A.Max.Z - Hair;
}

/** The building's footprint in plan, in world space. */
FBox KfFootprint(const FLaneBuilding& B)
{
    FBox Box(ForceInit);
    Box += B.Frame.Pt(0.0, 0.0, 0.0);
    Box += B.Frame.Pt(B.Width, B.Depth, 1.0);
    return Box;
}

bool KfAnyBlocking(const TArray<FBox>& Blocking, const FBox& Space)
{
    return Blocking.ContainsByPredicate([&](const FBox& Box) { return KfOverlaps(Space, Box); });
}
} // namespace PWKitFrontTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWKitFrontagesRecognitionTest, "PerfectWorld.KitFrontages.Recognition",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPWKitFrontagesRecognitionTest::RunTest(const FString& Parameters)
{
    using namespace PWKitFrontTest;
    const TArray<FKfBuilt> Built = KfBuiltBuildings();

    // The real generator still lays out every pinned footprint, and the four hand-built ones where this test places them.
    FLaneScene Lane;
    BuildLaneRows(Lane);
    const auto GeneratedHas = [&](int32 Side, double X0, double X1, double AbsY)
    {
        return Lane.Fronts[Side > 0 ? 1 : 0].ContainsByPredicate([&](const FLaneFront& F)
                                                                  { return FMath::IsNearlyEqual(F.X0, X0, 0.5) && FMath::IsNearlyEqual(F.X1, X1, 0.5) && FMath::IsNearlyEqual(F.AbsY, AbsY, 0.5); });
    };
    TestEqual(TEXT("the generator records nineteen side frontages"), Lane.Fronts[0].Num() + Lane.Fronts[1].Num(), 19);
    for (const FKfBuilt& Item : Built)
    {
        const FLaneBuilding& B = Item.Building;
        TestTrue(*FString::Printf(TEXT("the %s is built by its own code"), Item.Name), KfBuiltPredicate(Item));
        TestTrue(*FString::Printf(TEXT("the generator has the %s frontage"), Item.Name),
                 GeneratedHas(B.Side, B.Frame.Origin.X, B.Frame.Origin.X + B.Width, FMath::Abs(B.Frame.Origin.Y)));
        FLaneKitFrontage Out;
        TestFalse(*FString::Printf(TEXT("the %s is not a kit frontage"), Item.Name), IsKitFrontage(B, Out));
    }

    TestEqual(TEXT("batch 1 has three frontages"), static_cast<int32>(LI::KitFrontageEntries().size()), 3);
    int32 SideCount[2] = {0, 0};
    for (const LI::FrontageEntry& Row : LI::KitFrontageEntries())
    {
        const FString Name = UTF8_TO_TCHAR(std::string(Row.BuildingId).c_str());
        SideCount[Row.Side > 0 ? 1 : 0] += 1;
        TestTrue(*FString::Printf(TEXT("%s: the generator has this frontage"), *Name),
                 GeneratedHas(Row.Side, Row.OriginXCm, Row.OriginXCm + Row.WidthCm, Row.FrontAbsYCm));

        const FLaneBuilding B = KfFromRow(Row);
        FLaneKitFrontage Out;
        if (!IsKitFrontage(B, Out))
        {
            AddError(FString::Printf(TEXT("%s: the exact footprint is not recognised"), *Name));
            continue;
        }
        TestEqual(*FString::Printf(TEXT("%s: building id"), *Name), Out.BuildingId.ToString(), Name);
        TestTrue(*FString::Printf(TEXT("%s: variant, furniture set, entrance and mirror come from the row"), *Name),
                 Out.Kind == Row.Kind && Out.Furniture == Row.Furniture && Out.EntranceCentreU == Row.EntranceCentreU && Out.bMirrorU == Row.MirrorU);
        TestTrue(*FString::Printf(TEXT("%s: no lane door id is hung and the planned one is only named"), *Name),
                 Out.DoorId.IsNone() && !Out.PlannedDoorId.IsNone());
        for (const LI::FrontageEntry& Other : LI::KitFrontageEntries())
        {
            if (&Other != &Row)
            {
                FLaneKitFrontage OtherOut;
                TestTrue(*FString::Printf(TEXT("%s: another row's building is a different id"), *Name),
                         IsKitFrontage(KfFromRow(Other), OtherOut) && OtherOut.BuildingId != Out.BuildingId);
            }
        }

        // Every perturbation of the footprint is refused.
        const auto Refused = [&](const TCHAR* What, const FLaneBuilding& Perturbed)
        {
            FLaneKitFrontage Scratch;
            TestFalse(*FString::Printf(TEXT("%s: %s is refused"), *Name, What), IsKitFrontage(Perturbed, Scratch));
        };
        FLaneBuilding P = B;
        P.Width += 100.0;
        Refused(TEXT("another width"), P);
        P = B;
        P.Depth += 100.0;
        Refused(TEXT("another depth"), P);
        P = B;
        P.Setback = Row.SetbackCm == 0 ? 60.0 : 0.0;
        Refused(TEXT("another setback"), P);
        P = B;
        P.Storeys += 1;
        Refused(TEXT("another storey count"), P);
        P = B;
        P.Seed ^= 1u;
        Refused(TEXT("another seed"), P);
        P = B;
        P.Index += 1;
        FLaneKitFrontage Scratch;
        TestTrue(*FString::Printf(TEXT("%s: the row index is only a label, the footprint decides"), *Name), IsKitFrontage(P, Scratch));
        P = B;
        P.Detail = ELaneDetail::Medium;
        Refused(TEXT("another detail level"), P);
        P = B;
        P.Side = 0;
        Refused(TEXT("a closing mass (side 0)"), P);
        P = B;
        P.StoreyHeightCm = OlderStoreyHeight;
        Refused(TEXT("another storey height"), P);
        P = B;
        P.Frame.Origin.X += 100.0;
        Refused(TEXT("another position along the lane"), P);
        P = B;
        P.Frame.Origin.X += 0.4;
        Refused(TEXT("a fractional position"), P);
        P = B;
        P.Frame.Origin.Y += Row.Side * 60.0;
        Refused(TEXT("another frontage line"), P);
        P = B;
        P.Frame.U = FVector(-1.0, 0.0, 0.0);
        Refused(TEXT("a reversed frontage axis"), P);
        P = B;
        P.Frame.V = FVector(0.0, -Row.Side, 0.0);
        Refused(TEXT("a reversed depth axis"), P);
        P = KfMakeBuilding(-Row.Side, Row.RowIndex, Row.OriginXCm, Row.WidthCm, Row.DepthCm, Row.SetbackCm, Row.Storeys, Row.Seed);
        Refused(TEXT("the other side of the lane"), P);
    }
    TestTrue(TEXT("at least one frontage on each side of the lane"), SideCount[0] >= 1 && SideCount[1] >= 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWKitFrontagesOverlapTest, "PerfectWorld.KitFrontages.NoOverlap",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPWKitFrontagesOverlapTest::RunTest(const FString& Parameters)
{
    using namespace PWKitFrontTest;
    const TArray<FKfBuilt> Built = KfBuiltBuildings();
    TArray<FBox> Taken;
    for (const FKfBuilt& Item : Built)
    {
        Taken.Add(KfFootprint(Item.Building));
    }
    for (const LI::FrontageEntry& Row : LI::KitFrontageEntries())
    {
        const FString Name = UTF8_TO_TCHAR(std::string(Row.BuildingId).c_str());
        const FBox Mine = KfFootprint(KfFromRow(Row));
        for (int32 I = 0; I < Taken.Num(); ++I)
        {
            TestFalse(*FString::Printf(TEXT("%s does not overlap the %s"), *Name, Built[I].Name), KfOverlaps(Mine, Taken[I]));
        }
        for (const LI::FrontageEntry& Other : LI::KitFrontageEntries())
        {
            if (&Other != &Row)
            {
                TestFalse(*FString::Printf(TEXT("%s does not overlap another kit frontage"), *Name), KfOverlaps(Mine, KfFootprint(KfFromRow(Other))));
            }
        }
        // The side alleys stay open: the footprint lies wholly inside the lane and clear of the alley band on its side.
        const double AlleyA = Row.Side < 0 ? AlleyMinusA : AlleyPlusA;
        const double AlleyB = Row.Side < 0 ? AlleyMinusB : AlleyPlusB;
        TestTrue(*FString::Printf(TEXT("%s is inside the lane's ends"), *Name), Mine.Min.X >= LaneX0 && Mine.Max.X <= LaneX1);
        TestTrue(*FString::Printf(TEXT("%s is clear of its side alley"), *Name), Mine.Max.X <= AlleyA + 0.5 || Mine.Min.X >= AlleyB - 0.5);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWKitFrontagesCollisionTest, "PerfectWorld.KitFrontages.Collision",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPWKitFrontagesCollisionTest::RunTest(const FString& Parameters)
{
    using namespace PWKitFrontTest;
    FLaneScene Props; // the kerb-edge poles, trees and vehicles of the real lane
    BuildLaneProps(Props);
    const TArray<FBox>& PropBoxes = Props.Blocks.BlockingBoxes();

    for (const LI::FrontageEntry& Row : LI::KitFrontageEntries())
    {
        const FString Name = UTF8_TO_TCHAR(std::string(Row.BuildingId).c_str());
        const FLaneBuilding B = KfFromRow(Row);
        FLaneKitFrontage Frontage;
        if (!IsKitFrontage(B, Frontage))
        {
            AddError(FString::Printf(TEXT("%s: not recognised"), *Name));
            continue;
        }
        const LI::InteriorLayout Layout = MakeKitFrontageLayout(B, Frontage);
        if (!Layout.Ok())
        {
            AddError(FString::Printf(TEXT("%s: layout refused (%hs)"), *Name, std::string(LI::RefusalCode(Layout.Status)).c_str()));
            continue;
        }
        TestTrue(*FString::Printf(TEXT("%s: the core re-validates the layout"), *Name), LI::ValidateInteriorLayout(Layout) == LI::Refusal::None);
        TestTrue(*FString::Printf(TEXT("%s: mirror flag"), *Name), Layout.Mirrored == Row.MirrorU);

        FLaneScene Scene;
        BuildKitFrontage(Scene, B, Frontage);
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
        TestEqual(*FString::Printf(TEXT("%s: collision box count (the street face adds none)"), *Name), Blocking.Num(), Expected);
        TestEqual(*FString::Printf(TEXT("%s: open doorways only, so no door spec"), *Name), Scene.Doors.Num(), 0);
        TestEqual(*FString::Printf(TEXT("%s: one room light per layout light"), *Name), Scene.RoomLights.Num(), static_cast<int32>(Layout.Lights.size()));
        TestTrue(*FString::Printf(TEXT("%s: the floor slab tops out at the finished floor"), *Name),
                 Blocking.ContainsByPredicate([&](const FBox& Box) { return FMath::IsNearlyEqual(Box.Max.Z, FloorAbs, 0.1) && Box.Min.Z <= GroundZ + 0.1; }));

        // Every solid piece has a collision box of exactly its size, floor-relative.
        for (const LI::Furniture& P : Layout.Pieces)
        {
            if (!P.Solid)
            {
                continue;
            }
            const FBox Want = KfWorldBox(B, Layout, P.Footprint, P.H0, P.H1);
            TestTrue(*FString::Printf(TEXT("%s: collision box for %hs"), *Name, P.Id.c_str()),
                     Blocking.ContainsByPredicate([&](const FBox& Box) { return Box.Min.Equals(Want.Min, 0.6) && Box.Max.Equals(Want.Max, 0.6); }));
        }

        // Doorways: 90 x 210 clear of every collision box, front doorway and internal ones alike.
        int32 Doorways = 0;
        for (const LI::Opening& O : Layout.Openings)
        {
            if (O.Kind == LI::OpeningKind::Window)
            {
                continue;
            }
            ++Doorways;
            TestTrue(*FString::Printf(TEXT("%s: %hs is at least 90 wide and 210 high"), *Name, O.Id.c_str()),
                     O.ClearWidthCm >= LI::DoorClearWidthCm && O.W0 == 0 && O.W1 - O.W0 >= LI::DoorClearHeightCm);
            const bool bAlongU = O.Plan.Width() > O.Plan.Depth();
            const LI::Rect Clear = bAlongU ? LI::Rect{O.Plan.U0 + 2, O.Plan.U1 - 2, O.Plan.V0, O.Plan.V1}
                                           : LI::Rect{O.Plan.U0, O.Plan.U1, O.Plan.V0 + 2, O.Plan.V1 - 2};
            TestFalse(*FString::Printf(TEXT("%s: doorway %hs is clear of collision"), *Name, O.Id.c_str()),
                      KfAnyBlocking(Blocking, KfWorldBox(B, Layout, Clear, 2.0, LI::DoorClearHeightCm)));
        }
        TestTrue(*FString::Printf(TEXT("%s: at least the front doorway"), *Name), Doorways >= 1 && KitFrontEntrance(Layout) != nullptr);

        // Circulation (120 wide), door approaches, standing and seat spaces stay clear of collision.
        int32 Corridors = 0;
        for (const LI::Zone& Z : Layout.Zones)
        {
            if (Z.Kind == LI::ZoneKind::Route)
            {
                continue;
            }
            if (Z.Kind == LI::ZoneKind::Circulation)
            {
                ++Corridors;
                TestTrue(*FString::Printf(TEXT("%s: corridor %hs is at least 120 wide"), *Name, Z.Id.c_str()),
                         FMath::Min(Z.Area.Width(), Z.Area.Depth()) >= LI::CirculationCm);
            }
            TestFalse(*FString::Printf(TEXT("%s: %hs is clear of collision"), *Name, Z.Id.c_str()),
                      KfAnyBlocking(Blocking, KfWorldBox(B, Layout, Z.Area, 2.0, 210.0)));
        }
        TestTrue(*FString::Printf(TEXT("%s: at least one circulation corridor"), *Name), Corridors >= 1);

        TArray<FKitAnchor> Anchors;
        GetKitAnchors(B, Layout, Anchors);
        for (int32 I = 0; I < Anchors.Num(); ++I)
        {
            if (Anchors[I].Kind != LI::AnchorKind::Stand)
            {
                continue;
            }
            const FVector At = Anchors[I].Location;
            const FBox Capsule(At + FVector(-KfCapsuleRadius, -KfCapsuleRadius, 2.0), At + FVector(KfCapsuleRadius, KfCapsuleRadius, KfCapsuleHeight));
            TestFalse(*FString::Printf(TEXT("%s: stand point %hs holds a capsule"), *Name, Layout.Anchors[I].Id.c_str()), KfAnyBlocking(Blocking, Capsule));
        }

        // On the footpath: the 120 cm in front of the front doorway is free of the lane's kerb props and of this frontage's own collision.
        const LI::Opening* Entrance = KitFrontEntrance(Layout);
        if (Entrance != nullptr)
        {
            const LI::Rect Approach{Entrance->Plan.U0, Entrance->Plan.U1, -LI::CirculationCm, 0};
            const FBox Space = KfWorldBox(B, Layout, Approach, 2.0, LI::DoorClearHeightCm);
            TestFalse(*FString::Printf(TEXT("%s: the footpath in front of the door is clear of lane props"), *Name), KfAnyBlocking(PropBoxes, Space));
            TestFalse(*FString::Printf(TEXT("%s: the footpath in front of the door is clear of the frontage's own collision"), *Name), KfAnyBlocking(Blocking, Space));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWKitFrontagesRefusedTest, "PerfectWorld.KitFrontages.RefusedLayoutStaysSolid",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPWKitFrontagesRefusedTest::RunTest(const FString& Parameters)
{
    using namespace PWKitFrontTest;
    // A frontage whose row is missing never builds an interior: with no catalogue row the layout is empty and the building is a solid block.
    FLaneBuilding B = KfMakeBuilding(1, 9, 100.0, 700.0, 700.0, 60.0, 3, 1u);
    FLaneKitFrontage Missing;
    TestFalse(TEXT("an unknown footprint is not a kit frontage"), IsKitFrontage(B, Missing));
    const LI::InteriorLayout Empty = MakeKitFrontageLayout(B, Missing);
    TestFalse(TEXT("no row, a refused layout"), Empty.Ok());
    AddExpectedError(TEXT("stays a solid block"), EAutomationExpectedErrorFlags::Contains, 1);
    FLaneScene Scene;
    BuildKitFrontage(Scene, B, Missing);
    TestEqual(TEXT("a refused frontage emits no door, no light and no interior collision"), Scene.Doors.Num() + Scene.RoomLights.Num() + Scene.Blocks.BlockingBoxes().Num(), 0);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
