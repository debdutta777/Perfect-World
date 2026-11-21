// The minimap and the full-screen map (order F2): what the scene carries from the portable lane map, what the plan projects to widget space
// (rotation, culling, people, doors), when the minimap hides, how it sits at the four window sizes, and how the full map fits, zooms, centres,
// describes what is under the pointer and answers its keys. Run in the editor under "PerfectWorld.Minimap". The widgets are the real ones,
// built without a window and given a fixed, world-free source (FPWMapLive::MakeFixed); what needs a real window (painting, focus, the
// pause and unpause around the full map, the live people and doors) is proved by running the game, not here.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Layout/Geometry.h"
#include "UI/PWMapData.h"
#include "UI/PWMapPlan.h"
#include "UI/SPWMinimap.h"
#include "UI/SPWWorldMap.h"

namespace MpLm = PerfectWorld::LaneMap;

namespace
{
const FVector2f MpSizes[] = {FVector2f(853.0f, 480.0f), FVector2f(1280.0f, 720.0f), FVector2f(1920.0f / 1.5f, 1080.0f / 1.5f), FVector2f(1920.0f, 1080.0f)};

FKeyEvent MpKey(const FKey Key, const bool bRepeat = false)
{
    return FKeyEvent(Key, FModifierKeysState(), 0, bRepeat, 0, 0);
}

/** A snapshot at the lane's start: the player at the origin facing +X, one named person nearby and every door read at this minute. */
FPWMapSnapshot MpSnapshot(const FPWMapScene& Scene, const int32 Minute, const bool bHasPlayer = true)
{
    FPWMapSnapshot Snapshot;
    Snapshot.bHasPlayer = bHasPlayer;
    Snapshot.PlayerCm = {0.0, 0.0};
    Snapshot.PlayerYawDegrees = 0.0;
    Snapshot.MinuteOfDay = Minute;
    FPWMapMarker& Person = Snapshot.People.AddDefaulted_GetRef();
    Person.PersonId = FName(TEXT("person.test"));
    Person.Name = FText::FromString(TEXT("Farida"));
    Person.WorldCm = {300.0, 0.0};
    FPWMapMarker& Far = Snapshot.People.AddDefaulted_GetRef();
    Far.PersonId = FName(TEXT("person.far"));
    Far.Name = FText::FromString(TEXT("Far away"));
    Far.WorldCm = {5000.0, 0.0};
    for (const MpLm::MapDoor& Door : Scene.Map.Doors)
    {
        Snapshot.Doors.Add(MpLm::QueryDoorStatus(Door, nullptr, Minute));
    }
    return Snapshot;
}

TSharedRef<FPWMapLive> MpLive(const int32 Minute = 600, const bool bHasPlayer = true)
{
    const TSharedRef<FPWMapScene> Scene = FPWMapScene::Build();
    return FPWMapLive::MakeFixed(Scene, MpSnapshot(*Scene, Minute, bHasPlayer));
}

const PWMap::FPlanDot* MpFindDot(const PWMap::FMapPlan& Plan, const PWMap::EDotRole Role)
{
    return Plan.Dots.FindByPredicate([Role](const PWMap::FPlanDot& Dot) { return Dot.Role == Role; });
}

void MpTick(const TSharedRef<SWidget>& Widget, const FVector2f& Size, const float Dt = 0.016f)
{
    Widget->SlatePrepass(1.0f);
    const FGeometry Root = FGeometry::MakeRoot(Size, FSlateLayoutTransform());
    Widget->Tick(Root, 0.0, Dt);
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMpSceneTest, "PerfectWorld.Minimap.SceneCarriesThePortableMap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMpSceneTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FPWMapScene> Scene = FPWMapScene::Build();
    TestTrue(TEXT("the portable map is valid"), MpLm::IsMapValid(Scene->Map));
    TestEqual(TEXT("a translated label for every building"), Scene->BuildingLabels.Num(), static_cast<int32>(Scene->Map.Buildings.size()));
    TestEqual(TEXT("and for every point of interest"), Scene->PoiLabels.Num(), static_cast<int32>(Scene->Map.Pois.size()));
    int32 Named = 0;
    for (int32 Index = 0; Index < Scene->BuildingLabels.Num(); ++Index)
    {
        const bool bNamed = !Scene->BuildingLabels[Index].IsEmpty();
        Named += bNamed ? 1 : 0;
        TestEqual(*FString::Printf(TEXT("named exactly when the portable map names it (%d)"), Index), bNamed, !Scene->Map.Buildings[static_cast<size_t>(Index)].Label.empty());
    }
    TestEqual(TEXT("sixteen named buildings"), Named, 16);
    TestEqual(TEXT("the stationery shop reads in English"), PWMap::LabelText("map.building.stationery", "x").ToString(), FString(TEXT("Stationery shop")));
    TestEqual(TEXT("an unknown key falls back to the portable label"), PWMap::LabelText("map.building.unknown", "Tea stall").ToString(), FString(TEXT("Tea stall")));
    TestTrue(TEXT("no label at all stays empty"), PWMap::LabelText("", "").IsEmpty());
    TestEqual(TEXT("clock text"), PWMap::ClockText(15 * 60 + 5).ToString(), FString(TEXT("15:05")));
    TestEqual(TEXT("short duration"), PWMap::DurationText(40).ToString(), FString(TEXT("40 min")));
    TestEqual(TEXT("long duration"), PWMap::DurationText(100).ToString(), FString(TEXT("1 h 40 min")));
    TestEqual(TEXT("whole hours"), PWMap::DurationText(120).ToString(), FString(TEXT("2 h")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMpPlanProjectionTest, "PerfectWorld.Minimap.PlanProjectsToWidgetSpace", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMpPlanProjectionTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FPWMapScene> Scene = FPWMapScene::Build();
    const FPWMapSnapshot Snapshot = MpSnapshot(*Scene, 600);
    const float Side = 160.0f;
    const MpLm::Rect Clip{4.0, 4.0, Side - 4.0, Side - 4.0};

    FPWMapPreferences Still;
    Still.bMinimapRotates = false;
    FPWMapPreferences Turning;
    Turning.bMinimapRotates = true;

    // North-up: the lane runs left to right, the arrow (facing +X) points right, and 25 m is half the side.
    const MpLm::View NorthUp = PWMap::MakeMinimapView(Snapshot, Still, Side, false);
    const PWMap::FMapPlan Plan = PWMap::BuildPlan(*Scene, Snapshot, NorthUp, FVector2f(Side, Side), Clip);
    TestTrue(TEXT("the player is on the plan"), Plan.Player.bVisible);
    TestTrue(TEXT("at the centre"), Plan.Player.Position.Equals(FVector2f(80.0f, 80.0f), 0.01f));
    TestTrue(TEXT("pointing right when facing +X on the still map"), FMath::IsNearlyEqual(Plan.Player.AngleDegrees, 90.0f, 0.01f));
    const PWMap::FPlanQuad* Road = Plan.Quads.FindByPredicate([](const PWMap::FPlanQuad& Quad) { return Quad.Role == PWMap::EQuadRole::Road; });
    if (TestNotNull(TEXT("the road is on the plan"), Road))
    {
        TestTrue(TEXT("the road is a long horizontal band"), Road->Size.X > 250.0f && FMath::IsNearlyEqual(Road->Size.Y, 500.0f * (Side * 0.5f / 2500.0f), 0.01f));
        TestTrue(TEXT("on the middle row"), FMath::IsNearlyEqual(Road->Centre.Y, 80.0f, 0.01f));
        TestTrue(TEXT("and it is not turned"), FMath::IsNearlyZero(FMath::Sin(Road->AngleRadians), 0.001f));
    }

    // Rotating: the arrow always points up, and the lane turns under it.
    FPWMapSnapshot FacingY = Snapshot;
    FacingY.PlayerYawDegrees = 90.0;
    const MpLm::View Rotating = PWMap::MakeMinimapView(FacingY, Turning, Side, false);
    const PWMap::FMapPlan Turned = PWMap::BuildPlan(*Scene, FacingY, Rotating, FVector2f(Side, Side), Clip);
    TestTrue(TEXT("the rotating arrow points up"), FMath::IsNearlyZero(FMath::Fmod(Turned.Player.AngleDegrees + 360.0f, 360.0f), 0.01f));
    const PWMap::FPlanQuad* TurnedRoad = Turned.Quads.FindByPredicate([](const PWMap::FPlanQuad& Quad) { return Quad.Role == PWMap::EQuadRole::Road; });
    if (TestNotNull(TEXT("the road is still on the plan"), TurnedRoad))
    {
        TestTrue(TEXT("a quarter turn makes the road a vertical band"), FMath::IsNearlyEqual(FMath::Abs(FMath::Sin(TurnedRoad->AngleRadians)), 1.0f, 0.001f));
    }
    // Reduced motion never turns the map, whatever the preference says.
    const MpLm::View Reduced = PWMap::MakeMinimapView(FacingY, Turning, Side, true);
    TestTrue(TEXT("reduced motion keeps the lane still"), Reduced.UpHeadingDegrees == MpLm::NorthUpHeadingDegrees);

    // Culling: the far end of the lane is outside a 25 m window.
    TestTrue(TEXT("some rectangles are culled"), Plan.CulledQuads > 0);
    TestEqual(TEXT("every rectangle is either planned or counted as culled"), Plan.Quads.Num() + Plan.CulledQuads,
              static_cast<int32>(Scene->Map.Surfaces.size() + Scene->Map.Buildings.size()));
    const int32 EndShop = static_cast<int32>(MpLm::FindBuilding(Scene->Map, "building.nabapara.end_shop") - Scene->Map.Buildings.data());
    TestFalse(TEXT("the shop at the far end is culled"),
              Plan.Quads.ContainsByPredicate([EndShop](const PWMap::FPlanQuad& Quad) { return Quad.bBuilding && Quad.SourceIndex == EndShop; }));
    const int32 Stationery = static_cast<int32>(MpLm::FindBuilding(Scene->Map, "building.nabapara.stationery") - Scene->Map.Buildings.data());
    TestTrue(TEXT("the stationery shop beside the start is in view"),
             Plan.Quads.ContainsByPredicate([Stationery](const PWMap::FPlanQuad& Quad) { return Quad.bBuilding && Quad.SourceIndex == Stationery; }));

    // People and doors: the near person is a dot at the right pixel, the far one is dropped, an open door is a disc mark.
    int32 People = 0;
    for (const PWMap::FPlanDot& Dot : Plan.Dots)
    {
        if (Dot.Role == PWMap::EDotRole::Person)
        {
            ++People;
            TestEqual(TEXT("it is the near person"), Dot.SourceIndex, 0);
            TestTrue(TEXT("300 cm east is 9.6 px right of centre"), Dot.Position.Equals(FVector2f(80.0f + 300.0f * Side * 0.5f / 2500.0f, 80.0f), 0.01f));
        }
    }
    TestEqual(TEXT("only the person within 25 m is shown"), People, 1);
    const PWMap::FPlanDot* OpenDoor = MpFindDot(Plan, PWMap::EDotRole::DoorOpen);
    TestNotNull(TEXT("a door open at 10:00 is on the plan"), OpenDoor);
    const PWMap::FMapPlan NoPeople = PWMap::BuildPlan(*Scene, Snapshot, NorthUp, FVector2f(Side, Side), Clip, [](){ PWMap::FPlanOptions O; O.bPeople = false; O.bDoors = false; return O; }());
    TestEqual(TEXT("options remove people and doors"), NoPeople.Dots.Num(), 0);

    // At 13:20 the stationery shop's door is closed for now: its mark is a square, not a disc.
    const FPWMapSnapshot Noon = MpSnapshot(*Scene, 800);
    const PWMap::FMapPlan NoonPlan = PWMap::BuildPlan(*Scene, Noon, NorthUp, FVector2f(Side, Side), Clip);
    TestNotNull(TEXT("a closed door is on the plan at 13:20"), MpFindDot(NoonPlan, PWMap::EDotRole::DoorClosed));

    // No player: no arrow, and the view falls back to the lane's start.
    FPWMapSnapshot Nobody = Snapshot;
    Nobody.bHasPlayer = false;
    TestFalse(TEXT("no arrow without a player"), PWMap::BuildPlan(*Scene, Nobody, NorthUp, FVector2f(Side, Side), Clip).Player.bVisible);
    TestTrue(TEXT("rotating preference reads as rotation"), Turning.Orientation(false) == MpLm::MinimapOrientation::RotateWithCamera);
    TestTrue(TEXT("and as still when reduced"), Turning.Orientation(true) == MpLm::MinimapOrientation::NorthUp);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMpHiddenStatesTest, "PerfectWorld.Minimap.HiddenStatesFollowTheRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMpHiddenStatesTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FPWMapLive> Live = MpLive();
    const TSharedRef<SPWMinimap> Minimap = SNew(SPWMinimap).Live(Live);
    MpTick(Minimap, FVector2f(1280.0f, 720.0f));
    TestTrue(TEXT("shown with a player and nothing in front"), Minimap->IsMinimapShown());

    bool bWorldMapOpen = true;
    const TSharedRef<SPWMinimap> Covered = SNew(SPWMinimap).Live(Live).WorldMapOpen(TAttribute<bool>::CreateLambda([&bWorldMapOpen]() { return bWorldMapOpen; }));
    MpTick(Covered, FVector2f(1280.0f, 720.0f));
    TestTrue(TEXT("hidden while the full map is open"), Covered->GetHidden() == MpLm::MinimapHidden::WorldMap);
    bWorldMapOpen = false;
    MpTick(Covered, FVector2f(1280.0f, 720.0f));
    TestTrue(TEXT("and back when it closes"), Covered->IsMinimapShown());

    const TSharedRef<FPWMapLive> NoPlayer = MpLive(600, false);
    const TSharedRef<SPWMinimap> Empty = SNew(SPWMinimap).Live(NoPlayer);
    MpTick(Empty, FVector2f(1280.0f, 720.0f));
    TestTrue(TEXT("hidden with no player (title, creation)"), Empty->GetHidden() == MpLm::MinimapHidden::NoPlayer);

    FPWMapPreferences Off;
    Off.bMinimapEnabled = false;
    const TSharedRef<FPWMapLive> Disabled = MpLive();
    Disabled->SetPreferences(Off);
    const TSharedRef<SPWMinimap> Switched = SNew(SPWMinimap).Live(Disabled);
    MpTick(Switched, FVector2f(1280.0f, 720.0f));
    TestTrue(TEXT("hidden when the player turns it off"), Switched->GetHidden() == MpLm::MinimapHidden::Disabled);

    // The remaining states (menu, conversation, reading) are the portable rule table, proved in the Rung 1 view tests; the live reads of
    // the HUD, the conversation presenter and the reading presenter are in FPWMapLive::ReadHudState and need a world.
    MpLm::HudState State;
    State.bHasLocalPlayer = true;
    State.bMenuOpen = true;
    TestTrue(TEXT("a menu hides it"), MpLm::ResolveMinimapVisibility(State) == MpLm::MinimapHidden::Menu);
    TestFalse(TEXT("and the map cannot open over it"), MpLm::CanOpenWorldMap(State));
    State.bMenuOpen = false;
    State.bConversationOpen = true;
    TestTrue(TEXT("a conversation hides it"), MpLm::ResolveMinimapVisibility(State) == MpLm::MinimapHidden::Conversation);
    State.bConversationOpen = false;
    State.bReadingOpen = true;
    TestTrue(TEXT("a reading hides it"), MpLm::ResolveMinimapVisibility(State) == MpLm::MinimapHidden::Reading);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMpMinimapFitTest, "PerfectWorld.Minimap.SitsClearAtEveryWindowSize", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMpMinimapFitTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FPWMapLive> Live = MpLive();
    for (const FVector2f& Size : MpSizes)
    {
        const FString Name = FString::Printf(TEXT("%.0fx%.0f"), Size.X, Size.Y);
        const TSharedRef<SPWMinimap> Minimap = SNew(SPWMinimap).Live(Live);
        MpTick(Minimap, Size);
        const MpLm::MinimapLayout Layout = Minimap->GetLayout();
        TestTrue(*(Name + TEXT(": inside the window")), Layout.Bounds.X0 >= 0.0 && Layout.Bounds.X1 <= Size.X && Layout.Bounds.Y1 <= Size.Y);
        TestTrue(*(Name + TEXT(": in the upper-right quarter")), Layout.Bounds.X0 > Size.X * 0.5 && Layout.Bounds.Y1 <= Size.Y * 0.5);
        TestTrue(*(Name + TEXT(": below the clock readout")), Layout.Bounds.Y0 >= MpLm::MinimapTopInsetPx);
        TestFalse(*(Name + TEXT(": clear of the interaction prompt")), Layout.Bounds.Intersects(MpLm::Rect{Size.X * 0.25, Size.Y - 160.0, Size.X * 0.75, static_cast<double>(Size.Y)}));
        TestTrue(*(Name + TEXT(": 25 m radius at the middle of an edge")), FMath::IsNearlyEqual(Layout.SidePx * 0.5 / MpLm::MinimapRadiusCm, MpLm::MinimapView({0, 0}, 0.0, Layout.SidePx, MpLm::MinimapOrientation::NorthUp).PixelsPerCm, 1.0e-9));
        Minimap->SlatePrepass(1.0f);
        TestTrue(*(Name + TEXT(": it takes no input")), Minimap->GetVisibility() == EVisibility::HitTestInvisible);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMpWorldMapFitTest, "PerfectWorld.Minimap.WorldMapFitsAtEveryWindowSize", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMpWorldMapFitTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FPWMapLive> Live = MpLive();
    for (const FVector2f& Size : MpSizes)
    {
        const FString Name = FString::Printf(TEXT("%.0fx%.0f"), Size.X, Size.Y);
        const TSharedRef<SPWWorldMap> Map = SNew(SPWWorldMap).Live(Live);
        MpTick(Map, Size);
        const FPWMapScene& Scene = Live->GetScene();
        const MpLm::Size Widget{Size.X, Size.Y};
        bool bAllInside = true;
        for (const MpLm::Building& Building : Scene.Map.Buildings)
        {
            const MpLm::Rect Box = MpLm::ProjectedBounds(Map->GetView(), Widget, Building.Footprint);
            bAllInside = bAllInside && Box.X0 >= -0.5 && Box.Y0 >= -0.5 && Box.X1 <= Size.X + 0.5 && Box.Y1 <= Size.Y + 0.5;
        }
        TestTrue(*(Name + TEXT(": the whole lane is in the window when opened")), bAllInside);
        TestTrue(*(Name + TEXT(": opened at the fitted view")), FMath::IsNearlyEqual(Map->GetView().PixelsPerCm, Map->GetFittedView().PixelsPerCm, 1.0e-12));
        TestEqual(*(Name + TEXT(": compact below 760")), PWMap::WorldMapCompact(Size), Size.X < 760.0f || Size.Y < 760.0f);

        Map->ZoomBy(2.0, true);
        TestTrue(*(Name + TEXT(": zooming in doubles the scale")), FMath::IsNearlyEqual(Map->GetView().PixelsPerCm, Map->GetFittedView().PixelsPerCm * 2.0, 1.0e-9));
        Map->ResetView();
        TestTrue(*(Name + TEXT(": reset puts the lane back")), FMath::IsNearlyEqual(Map->GetView().PixelsPerCm, Map->GetFittedView().PixelsPerCm, 1.0e-12));
        for (int32 Step = 0; Step < 60; ++Step)
        {
            Map->ZoomBy(1.5, true);
        }
        TestTrue(*(Name + TEXT(": zoom stops at street detail")), Map->GetView().PixelsPerCm <= MpLm::ZoomLimitsFor(Map->GetFittedView()).MaxPixelsPerCm + 1.0e-9);

        Map->ResetView();
        Map->CentreOnPlayer();
        const MpLm::Point OnScreen = MpLm::WorldToWidget(Map->GetView(), Widget, Live->GetSnapshot().PlayerCm);
        TestTrue(*(Name + TEXT(": centring puts the player in the middle")), FMath::IsNearlyEqual(OnScreen.X, Size.X * 0.5, 0.01) && FMath::IsNearlyEqual(OnScreen.Y, Size.Y * 0.5, 0.01));
        TestTrue(*(Name + TEXT(": and steps in")), Map->GetView().PixelsPerCm > Map->GetFittedView().PixelsPerCm);

        Map->ResetView();
        const MpLm::Point Before = Map->GetView().CentreCm;
        Map->PanBy(FVector2f(100.0f, 0.0f));
        TestTrue(*(Name + TEXT(": dragging right moves the view left in the world")), Map->GetView().CentreCm.X < Before.X);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMpWorldMapKeysTest, "PerfectWorld.Minimap.WorldMapKeysCloseOnceAndMove", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMpWorldMapKeysTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FPWMapLive> Live = MpLive();
    int32 Closed = 0;
    TArray<FKey> Extra;
    Extra.Add(EKeys::Gamepad_DPad_Left);
    const TSharedRef<SPWWorldMap> Map = SNew(SPWWorldMap).Live(Live).ExtraCloseKeys(Extra)
        .OnClose(FSimpleDelegate::CreateLambda([&Closed]() { ++Closed; }));
    MpTick(Map, FVector2f(1280.0f, 720.0f));
    const FGeometry Geometry;

    TestTrue(TEXT("the map takes keyboard focus"), Map->SupportsKeyboardFocus());
    int32 Expected = 0;
    for (const FKey& Key : {EKeys::Escape, EKeys::M, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_Special_Left, EKeys::Gamepad_Special_Right, EKeys::Gamepad_DPad_Left})
    {
        TestTrue(*(Key.ToString() + TEXT(" is handled")), Map->OnKeyDown(Geometry, MpKey(Key)).IsEventHandled());
        ++Expected;
        TestEqual(*(Key.ToString() + TEXT(" closes")), Closed, Expected);
        Map->OnKeyDown(Geometry, MpKey(Key, true));
        TestEqual(*(Key.ToString() + TEXT(" held does not close again")), Closed, Expected);
    }

    const bool bLegend = Map->IsLegendShown();
    Map->OnKeyDown(Geometry, MpKey(EKeys::L));
    TestEqual(TEXT("L toggles the legend"), Map->IsLegendShown(), !bLegend);
    Map->OnKeyDown(Geometry, MpKey(EKeys::L, true));
    TestEqual(TEXT("a held L does not toggle it again"), Map->IsLegendShown(), !bLegend);

    Map->ResetView();
    Map->OnKeyDown(Geometry, MpKey(EKeys::Home));
    TestTrue(TEXT("Home centres on the player"), Map->GetView().CentreCm == MpLm::Point{0.0, 0.0});
    Map->OnKeyDown(Geometry, MpKey(EKeys::R));
    TestTrue(TEXT("R puts the whole lane back"), FMath::IsNearlyEqual(Map->GetView().PixelsPerCm, Map->GetFittedView().PixelsPerCm, 1.0e-12));

    // A held arrow pans for as long as it is down, and stops when released.
    const MpLm::Point Start = Map->GetView().CentreCm;
    TestTrue(TEXT("Right is handled"), Map->OnKeyDown(Geometry, MpKey(EKeys::Right)).IsEventHandled());
    MpTick(Map, FVector2f(1280.0f, 720.0f), 0.25f);
    const MpLm::Point Moved = Map->GetView().CentreCm;
    TestTrue(TEXT("the view moved east"), Moved.X > Start.X);
    Map->OnKeyUp(Geometry, MpKey(EKeys::Right));
    MpTick(Map, FVector2f(1280.0f, 720.0f), 0.25f);
    TestTrue(TEXT("and stopped on release"), Map->GetView().CentreCm == Moved);

    // Zoom keys ease unless reduced motion is on; give the ease time to finish.
    Map->ResetView();
    Map->OnKeyDown(Geometry, MpKey(EKeys::Equals));
    for (int32 Frame = 0; Frame < 120; ++Frame)
    {
        MpTick(Map, FVector2f(1280.0f, 720.0f), 0.05f);
    }
    TestTrue(TEXT("plus zooms in"), Map->GetView().PixelsPerCm > Map->GetFittedView().PixelsPerCm * 1.2);
    TestFalse(TEXT("an unrelated key is not swallowed"), Map->OnKeyDown(Geometry, MpKey(EKeys::F9)).IsEventHandled());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMpDescribeTest, "PerfectWorld.Minimap.HoverDescribesBuildingsAndDoors", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMpDescribeTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FPWMapScene> Scene = FPWMapScene::Build();
    const int32 Stationery = static_cast<int32>(MpLm::FindBuilding(Scene->Map, "building.nabapara.stationery") - Scene->Map.Buildings.data());
    const int32 Care = static_cast<int32>(MpLm::FindBuilding(Scene->Map, "building.nabapara.care_home") - Scene->Map.Buildings.data());
    const int32 West = static_cast<int32>(MpLm::FindBuilding(Scene->Map, "building.nabapara.west_end_block") - Scene->Map.Buildings.data());

    const FString Open = PWMap::DescribeBuilding(*Scene, MpSnapshot(*Scene, 600), Stationery).ToString();
    TestTrue(TEXT("the stationery shop names itself and is open at 10:00 until 13:00"), Open.Contains(TEXT("Stationery shop")) && Open.Contains(TEXT("Open now, until 13:00")));
    const FString Closed = PWMap::DescribeBuilding(*Scene, MpSnapshot(*Scene, 800), Stationery).ToString();
    TestTrue(TEXT("at 13:20 it says when it opens"), Closed.Contains(TEXT("Closed. Opens in 1 h 40 min, at 15:00")));
    const FString Private = PWMap::DescribeBuilding(*Scene, MpSnapshot(*Scene, 600), Care).ToString();
    TestTrue(TEXT("a dwelling is private"), Private.Contains(TEXT("Children's home")) && Private.Contains(TEXT("knock")));
    FPWMapSnapshot Mine = MpSnapshot(*Scene, 600);
    Mine.PlayerHomeBuilding = Care;
    TestTrue(TEXT("the player's own home says so"), PWMap::DescribeBuilding(*Scene, Mine, Care).ToString().StartsWith(TEXT("Your home.")));
    // R6-05: the end masses are open halls; they have no door, so they are named and typed but carry no door line.
    const FString Hall = PWMap::DescribeBuilding(*Scene, MpSnapshot(*Scene, 600), West).ToString();
    TestTrue(TEXT("the west end is the community hall, described without a door"), Hall.Contains(TEXT("Community hall")) && Hall.Contains(TEXT("Public hall")) && !Hall.Contains(TEXT("Shuttered")) && !Hall.Contains(TEXT("Open")));
    TestTrue(TEXT("an invalid index describes nothing"), PWMap::DescribeBuilding(*Scene, MpSnapshot(*Scene, 600), 9999).IsEmpty());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
