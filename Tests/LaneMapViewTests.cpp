// F2: the map view arithmetic (LaneMapView.h): world to widget space and back, the camera yaw turning the map, culling, the minimap's
// 25 m scale and layout, fit at the four window sizes (and at 150% text, which is the same logical window), pan and zoom about the pointer,
// label placement, and the rules for when the minimap is hidden and when the map may open.

#include "LaneMap.h"
#include "LaneMapView.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace Lm = PerfectWorld::LaneMap;

namespace
{
int MpAssertions = 0;
void MpExpect(bool condition, const std::string& message)
{
    ++MpAssertions;
    if (condition)
    {
        return;
    }
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

bool MpNear(double a, double b, double eps = 1.0e-6)
{
    return std::abs(a - b) <= eps;
}

bool MpNear(Lm::Point a, Lm::Point b, double eps = 1.0e-6)
{
    return MpNear(a.X, b.X, eps) && MpNear(a.Y, b.Y, eps);
}

const Lm::Size MpSizes[] = {{853.0, 480.0}, {1280.0, 720.0}, {1920.0, 1080.0}, {1920.0 / 1.5, 1080.0 / 1.5}, {853.0 / 1.5, 480.0 / 1.5}};

void MpTestProjection()
{
    const Lm::Size widget{400.0, 300.0};
    Lm::View view;
    view.CentreCm = {1000.0, 200.0};
    view.PixelsPerCm = 0.5;

    // Up heading -90 (north-up): +X is right, -Y is up.
    view.UpHeadingDegrees = Lm::NorthUpHeadingDegrees;
    MpExpect(MpNear(Lm::WorldToWidget(view, widget, view.CentreCm), {200.0, 150.0}), "the centre is the widget's centre");
    MpExpect(MpNear(Lm::WorldToWidget(view, widget, {1100.0, 200.0}), {250.0, 150.0}), "+X runs to the right when north-up");
    MpExpect(MpNear(Lm::WorldToWidget(view, widget, {1000.0, 100.0}), {200.0, 100.0}), "-Y is up when north-up");
    MpExpect(MpNear(Lm::WorldToWidget(view, widget, {1000.0, 300.0}), {200.0, 200.0}), "+Y is down when north-up");

    // Up heading 0: +X is up, +Y is right (Unreal's own top-down reading).
    view.UpHeadingDegrees = 0.0;
    MpExpect(MpNear(Lm::WorldToWidget(view, widget, {1100.0, 200.0}), {200.0, 100.0}), "+X is up at heading 0");
    MpExpect(MpNear(Lm::WorldToWidget(view, widget, {1000.0, 300.0}), {250.0, 150.0}), "+Y is right at heading 0");

    // Heading 90 (facing +Y): +Y is up, +X is left.
    view.UpHeadingDegrees = 90.0;
    MpExpect(MpNear(Lm::WorldToWidget(view, widget, {1000.0, 300.0}), {200.0, 100.0}), "+Y is up at heading 90");
    MpExpect(MpNear(Lm::WorldToWidget(view, widget, {1100.0, 200.0}), {150.0, 150.0}), "+X is left at heading 90");

    // Round trip at a spread of headings and scales.
    for (const double heading : {-180.0, -90.0, -37.5, 0.0, 45.0, 90.0, 133.0, 270.0, 359.0})
    {
        for (const double scale : {0.02, 0.1, 1.0})
        {
            Lm::View v;
            v.CentreCm = {-400.0, 650.0};
            v.UpHeadingDegrees = heading;
            v.PixelsPerCm = scale;
            for (const Lm::Point p : {Lm::Point{0.0, 0.0}, Lm::Point{-3300.0, -1500.0}, Lm::Point{7300.0, 1500.0}, Lm::Point{123.0, -456.0}})
            {
                const Lm::Point back = Lm::WidgetToWorld(v, widget, Lm::WorldToWidget(v, widget, p));
                MpExpect(MpNear(back, p, 1.0e-6 / scale), "world to widget and back is the identity");
            }
        }
    }

    // Distances scale uniformly whatever the heading.
    for (const double heading : {0.0, 33.0, 90.0, 200.0})
    {
        Lm::View v;
        v.UpHeadingDegrees = heading;
        v.PixelsPerCm = 0.25;
        const Lm::Point a = Lm::WorldToWidget(v, widget, {0.0, 0.0});
        const Lm::Point b = Lm::WorldToWidget(v, widget, {300.0, 400.0});
        MpExpect(MpNear(std::hypot(b.X - a.X, b.Y - a.Y), 500.0 * 0.25, 1.0e-9), "rotation preserves distance");
    }
}

void MpTestRotation()
{
    // The arrow: facing the up heading points up; a quarter turn to the right points right.
    Lm::View v;
    v.UpHeadingDegrees = 30.0;
    MpExpect(MpNear(Lm::ScreenAngleOfYaw(v, 30.0), 0.0), "facing the up heading points up");
    MpExpect(MpNear(Lm::ScreenAngleOfYaw(v, 120.0), 90.0), "a quarter turn clockwise points right");
    MpExpect(MpNear(Lm::ScreenAngleOfYaw(v, -60.0), 270.0), "a quarter turn the other way points left");
    MpExpect(MpNear(Lm::ScreenAngleOfYaw(v, 210.0), 180.0), "facing away points down");
    MpExpect(MpNear(Lm::ScreenDirectionOfYaw(v, 30.0), {0.0, -1.0}), "up is (0, -1)");
    MpExpect(MpNear(Lm::ScreenDirectionOfYaw(v, 120.0), {1.0, 0.0}), "right is (1, 0)");

    // North-up with the lane left to right: facing +X points right.
    v.UpHeadingDegrees = Lm::NorthUpHeadingDegrees;
    MpExpect(MpNear(Lm::ScreenAngleOfYaw(v, 0.0), 90.0), "facing +X points right on the north-up map");
    MpExpect(MpNear(Lm::ScreenAngleOfYaw(v, 180.0), 270.0), "facing -X points left");

    // The arrow's direction agrees with where the world ahead of the player lands.
    for (const double yaw : {-170.0, -90.0, 0.0, 15.0, 90.0, 181.0})
    {
        for (const auto orientation : {Lm::MinimapOrientation::RotateWithCamera, Lm::MinimapOrientation::NorthUp})
        {
            const Lm::Point player{500.0, -300.0};
            const double side = 160.0;
            const Lm::View view = Lm::MinimapView(player, yaw, side, orientation);
            const double rad = yaw * 3.14159265358979323846 / 180.0;
            const Lm::Point ahead{player.X + 1000.0 * std::cos(rad), player.Y + 1000.0 * std::sin(rad)};
            const Lm::Point screenAhead = Lm::WorldToWidget(view, {side, side}, ahead);
            const Lm::Point dir = Lm::ScreenDirectionOfYaw(view, yaw);
            const Lm::Point toAhead{screenAhead.X - side * 0.5, screenAhead.Y - side * 0.5};
            const double len = std::hypot(toAhead.X, toAhead.Y);
            MpExpect(MpNear(toAhead.X / len, dir.X, 1.0e-9) && MpNear(toAhead.Y / len, dir.Y, 1.0e-9), "the world ahead lies where the arrow points");
            if (orientation == Lm::MinimapOrientation::RotateWithCamera)
            {
                MpExpect(MpNear(Lm::ScreenAngleOfYaw(view, yaw), 0.0, 1.0e-9) || MpNear(Lm::ScreenAngleOfYaw(view, yaw), 360.0, 1.0e-9),
                         "a rotating minimap keeps the arrow pointing up");
            }
        }
    }
}

void MpTestCulling()
{
    const Lm::Size widget{200.0, 200.0};
    const Lm::Rect clip{0.0, 0.0, 200.0, 200.0};
    Lm::View view;
    view.CentreCm = {0.0, 0.0};
    view.UpHeadingDegrees = 0.0;
    view.PixelsPerCm = 0.1; // the widget shows 2000 cm square

    const Lm::Polygon inside{{-100.0, -100.0}, {100.0, -100.0}, {100.0, 100.0}, {-100.0, 100.0}};
    const Lm::Polygon straddling{{900.0, -100.0}, {1300.0, -100.0}, {1300.0, 100.0}, {900.0, 100.0}};
    const Lm::Polygon outside{{1100.0, -100.0}, {1500.0, -100.0}, {1500.0, 100.0}, {1100.0, 100.0}};
    MpExpect(Lm::PolygonVisible(view, widget, inside, clip), "a polygon in view is visible");
    MpExpect(Lm::PolygonVisible(view, widget, straddling, clip), "a polygon crossing the edge is visible");
    MpExpect(!Lm::PolygonVisible(view, widget, outside, clip), "a polygon beyond the edge is culled");
    MpExpect(!Lm::PolygonVisible(view, widget, {}, clip), "an empty polygon is culled");

    // Culling follows the rotation: at heading 90 the world's +X points left, so the same polygon leaves by the other side.
    view.UpHeadingDegrees = 90.0;
    const Lm::Polygon ahead{{900.0, -100.0}, {1300.0, -100.0}, {1300.0, 100.0}, {900.0, 100.0}};
    MpExpect(Lm::PolygonVisible(view, widget, ahead, clip), "the polygon straddles the left edge after a quarter turn");
    MpExpect(!Lm::PolygonVisible(view, widget, {{1100.0, -100.0}, {1500.0, -100.0}, {1500.0, 100.0}, {1100.0, 100.0}}, clip), "and the far one stays culled");
    MpExpect(Lm::PolygonVisible(view, widget, {{-100.0, 900.0}, {100.0, 900.0}, {100.0, 1300.0}, {-100.0, 1300.0}}, clip), "+Y is now up: that one is in view");

    MpExpect(Lm::PointVisible(view, widget, {0.0, 0.0}, clip, 4.0), "the centre point is visible");
    MpExpect(!Lm::PointVisible(view, widget, {0.0, 1000.0}, clip, 4.0), "a point on the edge is culled by the inset");
    MpExpect(Lm::PointVisible(view, widget, {0.0, 1000.0}, clip, 0.0), "and shown without it");
    MpExpect(!Lm::PointVisible(view, widget, {0.0, 1400.0}, clip, 0.0), "a point beyond the edge is culled");

    // The real lane at the minimap's scale: far buildings drop out, near ones stay.
    const Lm::Map map = Lm::BuildLaneMap();
    const double side = 160.0;
    const Lm::View mini = Lm::MinimapView({0.0, 0.0}, 0.0, side, Lm::MinimapOrientation::NorthUp);
    const Lm::Rect miniClip{0.0, 0.0, side, side};
    int shown = 0;
    int culled = 0;
    for (const Lm::Building& b : map.Buildings)
    {
        (Lm::PolygonVisible(mini, {side, side}, b.Footprint, miniClip) ? shown : culled)++;
    }
    MpExpect(shown >= 3 && shown <= 12, "a 25 m window on the lane shows a handful of buildings");
    MpExpect(culled >= 8, "and culls the rest");
    const Lm::Building* far = Lm::FindBuilding(map, "building.nabapara.end_shop");
    MpExpect(far != nullptr && !Lm::PolygonVisible(mini, {side, side}, far->Footprint, miniClip), "the shop at the far end is out of the starting window");
}

void MpTestMinimap()
{
    // 25 m from the player reaches the middle of every edge, at any yaw, in either orientation.
    for (const double yaw : {0.0, 33.0, 90.0, -135.0})
    {
        for (const auto orientation : {Lm::MinimapOrientation::RotateWithCamera, Lm::MinimapOrientation::NorthUp})
        {
            const double side = 200.0;
            const Lm::Point player{1234.0, -321.0};
            const Lm::View view = Lm::MinimapView(player, yaw, side, orientation);
            MpExpect(MpNear(view.PixelsPerCm, 100.0 / 2500.0, 1.0e-12), "the scale is side/2 per 25 m");
            MpExpect(MpNear(Lm::WorldToWidget(view, {side, side}, player), {100.0, 100.0}), "the player is at the centre");
            const double rad = view.UpHeadingDegrees * 3.14159265358979323846 / 180.0;
            const Lm::Point up{player.X + 2500.0 * std::cos(rad), player.Y + 2500.0 * std::sin(rad)};
            MpExpect(MpNear(Lm::WorldToWidget(view, {side, side}, up), {100.0, 0.0}, 1.0e-6), "2500 cm up the map reaches the top edge");
        }
    }
    MpExpect(Lm::MinimapView({0, 0}, 77.0, 100.0, Lm::MinimapOrientation::RotateWithCamera).UpHeadingDegrees == 77.0, "rotating: up is the camera yaw");
    MpExpect(Lm::MinimapView({0, 0}, 77.0, 100.0, Lm::MinimapOrientation::NorthUp).UpHeadingDegrees == Lm::NorthUpHeadingDegrees, "north-up: the lane stays put");
    MpExpect(Lm::EffectiveOrientation(Lm::MinimapOrientation::RotateWithCamera, true) == Lm::MinimapOrientation::NorthUp, "reduced motion never turns the map");
    MpExpect(Lm::EffectiveOrientation(Lm::MinimapOrientation::RotateWithCamera, false) == Lm::MinimapOrientation::RotateWithCamera, "otherwise the request stands");
    MpExpect(Lm::EffectiveOrientation(Lm::MinimapOrientation::NorthUp, false) == Lm::MinimapOrientation::NorthUp, "north-up is honoured");

    // Layout at every size: top-right, below the clock, clear of the prompt (bottom centre) and the conversation panel (the lower half).
    double previousSide = 0.0;
    for (const Lm::Size size : {Lm::Size{853.0, 480.0}, Lm::Size{1280.0, 720.0}, Lm::Size{1920.0, 1080.0}})
    {
        const Lm::MinimapLayout layout = Lm::LayoutMinimap(size);
        MpExpect(layout.SidePx >= 96.0 && layout.SidePx <= 220.0, "the side stays within 96..220");
        MpExpect(layout.SidePx >= previousSide, "a larger window never gets a smaller minimap");
        previousSide = layout.SidePx;
        MpExpect(MpNear(layout.Bounds.Width(), layout.SidePx) && MpNear(layout.Bounds.Height(), layout.SidePx), "the bounds are square");
        MpExpect(MpNear(layout.Bounds.X1, size.W - Lm::MinimapMarginPx) && layout.Bounds.Y0 == Lm::MinimapTopInsetPx, "right margin and clock inset");
        MpExpect(layout.Bounds.X0 > size.W * 0.5 && layout.Bounds.Y1 <= size.H * 0.5, "the minimap stays in the upper-right quarter");
        const Lm::Rect prompt{size.W * 0.25, size.H - 160.0, size.W * 0.75, size.H};
        MpExpect(!layout.Bounds.Intersects(prompt), "it never covers the interaction prompt or notice");
        const Lm::Rect clock{size.W - 240.0, 0.0, size.W, 56.0};
        MpExpect(!layout.Bounds.Intersects(clock), "it never covers the clock readout");
    }
    // At 150% text the logical window shrinks; the minimap still fits and leaves the lower half free.
    for (const Lm::Size size : {Lm::Size{853.0 / 1.5, 480.0 / 1.5}, Lm::Size{1280.0 / 1.5, 720.0 / 1.5}, Lm::Size{1920.0 / 1.5, 1080.0 / 1.5}})
    {
        const Lm::MinimapLayout layout = Lm::LayoutMinimap(size);
        MpExpect(layout.Bounds.X0 > size.W * 0.5, "150% text: still on the right half");
        MpExpect(layout.Bounds.Y1 <= size.H * 0.6, "150% text: still in the upper part");
        MpExpect(!layout.Bounds.Intersects({size.W * 0.25, size.H - 160.0, size.W * 0.75, size.H}), "150% text: clear of the prompt");
    }
}

void MpTestVisibility()
{
    Lm::HudState state;
    MpExpect(Lm::ResolveMinimapVisibility(state) == Lm::MinimapHidden::NoPlayer, "no local player: hidden");
    state.bHasLocalPlayer = true;
    MpExpect(Lm::ResolveMinimapVisibility(state) == Lm::MinimapHidden::Visible, "playing: visible");
    MpExpect(Lm::CanOpenWorldMap(state), "playing: the map may open");

    Lm::HudState s = state;
    s.bMenuOpen = true;
    MpExpect(Lm::ResolveMinimapVisibility(s) == Lm::MinimapHidden::Menu && !Lm::CanOpenWorldMap(s), "title, creation or pause: hidden, and the map cannot open");
    s = state;
    s.bConversationOpen = true;
    MpExpect(Lm::ResolveMinimapVisibility(s) == Lm::MinimapHidden::Conversation && !Lm::CanOpenWorldMap(s), "a conversation: hidden, no map");
    s = state;
    s.bReadingOpen = true;
    MpExpect(Lm::ResolveMinimapVisibility(s) == Lm::MinimapHidden::Reading && !Lm::CanOpenWorldMap(s), "a reading: hidden, no map");
    s = state;
    s.bWorldMapOpen = true;
    MpExpect(Lm::ResolveMinimapVisibility(s) == Lm::MinimapHidden::WorldMap && !Lm::CanOpenWorldMap(s), "the full map covers the minimap and cannot open twice");
    s = state;
    s.bMinimapEnabled = false;
    MpExpect(Lm::ResolveMinimapVisibility(s) == Lm::MinimapHidden::Disabled && Lm::CanOpenWorldMap(s), "switched off: hidden, but the full map still opens");
    s.bMenuOpen = true;
    MpExpect(Lm::ResolveMinimapVisibility(s) == Lm::MinimapHidden::Menu, "a menu outranks the setting");

    // Every combination: visible only when nothing at all is in front.
    for (int bits = 0; bits < 64; ++bits)
    {
        Lm::HudState c;
        c.bHasLocalPlayer = (bits & 1) != 0;
        c.bMenuOpen = (bits & 2) != 0;
        c.bConversationOpen = (bits & 4) != 0;
        c.bReadingOpen = (bits & 8) != 0;
        c.bWorldMapOpen = (bits & 16) != 0;
        c.bMinimapEnabled = (bits & 32) == 0;
        const bool clear = c.bHasLocalPlayer && !c.bMenuOpen && !c.bConversationOpen && !c.bReadingOpen && !c.bWorldMapOpen && c.bMinimapEnabled;
        MpExpect((Lm::ResolveMinimapVisibility(c) == Lm::MinimapHidden::Visible) == clear, "visible exactly when nothing is in front");
        MpExpect(Lm::CanOpenWorldMap(c) == (c.bHasLocalPlayer && !c.bMenuOpen && !c.bConversationOpen && !c.bReadingOpen && !c.bWorldMapOpen), "the map opens exactly when nothing is in front");
    }
}

void MpTestFit()
{
    const Lm::Map map = Lm::BuildLaneMap();
    const double padding = 24.0;
    for (const Lm::Size size : MpSizes)
    {
        for (const double heading : {Lm::NorthUpHeadingDegrees, 0.0, 90.0, 37.0})
        {
            const Lm::View view = Lm::FitView(map.BoundsMin, map.BoundsMax, size, padding, heading);
            MpExpect(view.PixelsPerCm > 0.0, "a fitted view has a scale");
            MpExpect(MpNear(view.CentreCm, {(map.BoundsMin.X + map.BoundsMax.X) * 0.5, (map.BoundsMin.Y + map.BoundsMax.Y) * 0.5}), "the fit is centred on the map");
            const Lm::Rect inner{padding - 1.0e-6, padding - 1.0e-6, size.W - padding + 1.0e-6, size.H - padding + 1.0e-6};
            bool touches = false;
            for (const Lm::Point corner : {map.BoundsMin, map.BoundsMax, Lm::Point{map.BoundsMin.X, map.BoundsMax.Y}, Lm::Point{map.BoundsMax.X, map.BoundsMin.Y}})
            {
                const Lm::Point p = Lm::WorldToWidget(view, size, corner);
                MpExpect(inner.Contains(p), "every corner of the map lies inside the padded window");
                touches = touches || MpNear(p.X, padding, 1.0e-6) || MpNear(p.X, size.W - padding, 1.0e-6) || MpNear(p.Y, padding, 1.0e-6) || MpNear(p.Y, size.H - padding, 1.0e-6);
            }
            MpExpect(touches, "the fit is as large as it can be");
            for (const Lm::Building& b : map.Buildings)
            {
                const Lm::Rect box = Lm::ProjectedBounds(view, size, b.Footprint);
                MpExpect(inner.Contains({box.X0, box.Y0}) && inner.Contains({box.X1, box.Y1}), "every building is inside the window when fitted: " + b.Id);
            }
            const Lm::ZoomLimits limits = Lm::ZoomLimitsFor(view);
            MpExpect(limits.MinPixelsPerCm < view.PixelsPerCm && limits.MaxPixelsPerCm > view.PixelsPerCm, "the fit sits inside its zoom range");
            MpExpect(limits.MaxPixelsPerCm >= 0.6, "street detail is reachable");
        }
        // The lane fitted left to right is at a legible scale: at 853 x 480 a 5 m road is at least 8 px across.
        const Lm::View lane = Lm::FitView(map.BoundsMin, map.BoundsMax, size, padding, Lm::NorthUpHeadingDegrees);
        MpExpect(lane.PixelsPerCm * 500.0 >= 8.0 || size.W < 600.0, "the road is legible when the whole lane is fitted");
    }
    // A degenerate window or bounds never produces a zero or negative scale.
    MpExpect(Lm::FitView({0, 0}, {0, 0}, {0.0, 0.0}, 10.0, 0.0).PixelsPerCm > 0.0, "a degenerate fit still has a positive scale");
}

void MpTestPanZoom()
{
    const Lm::Map map = Lm::BuildLaneMap();
    const Lm::Size size{1280.0, 720.0};
    Lm::View view = Lm::FitView(map.BoundsMin, map.BoundsMax, size, 24.0, Lm::NorthUpHeadingDegrees);
    const Lm::ZoomLimits limits = Lm::ZoomLimitsFor(view);

    // Zooming keeps the world under the pointer where it is.
    for (const Lm::Point anchor : {Lm::Point{640.0, 360.0}, Lm::Point{100.0, 80.0}, Lm::Point{1200.0, 650.0}})
    {
        Lm::View v = view;
        const Lm::Point under = Lm::WidgetToWorld(v, size, anchor);
        Lm::ZoomAbout(v, size, anchor, 2.0, limits);
        MpExpect(MpNear(v.PixelsPerCm, view.PixelsPerCm * 2.0, 1.0e-12), "zooming in doubles the scale");
        MpExpect(MpNear(Lm::WorldToWidget(v, size, under), anchor, 1.0e-6), "the point under the pointer stays under it");
    }

    // Limits hold.
    Lm::View v = view;
    for (int i = 0; i < 40; ++i) Lm::ZoomAbout(v, size, {640.0, 360.0}, 1.5, limits);
    MpExpect(MpNear(v.PixelsPerCm, limits.MaxPixelsPerCm, 1.0e-12), "zoom stops at the maximum");
    for (int i = 0; i < 80; ++i) Lm::ZoomAbout(v, size, {640.0, 360.0}, 0.5, limits);
    MpExpect(MpNear(v.PixelsPerCm, limits.MinPixelsPerCm, 1.0e-12), "zoom stops at the minimum");
    const Lm::View before = v;
    Lm::ZoomAbout(v, size, {10.0, 10.0}, 0.0, limits);
    Lm::ZoomAbout(v, size, {10.0, 10.0}, -3.0, limits);
    MpExpect(MpNear(v.PixelsPerCm, before.PixelsPerCm) && MpNear(v.CentreCm, before.CentreCm), "a nonsense factor changes nothing");

    // Dragging moves the map under the pointer, at any heading.
    for (const double heading : {Lm::NorthUpHeadingDegrees, 0.0, 61.0})
    {
        Lm::View d = view;
        d.UpHeadingDegrees = heading;
        const Lm::Point grabbed = Lm::WidgetToWorld(d, size, {500.0, 300.0});
        Lm::PanByPixels(d, {80.0, -40.0});
        MpExpect(MpNear(Lm::WorldToWidget(d, size, grabbed), {580.0, 260.0}, 1.0e-6), "the grabbed point follows the pointer");
    }

    // Pan clamping keeps the centre within the map and a margin.
    Lm::View c = view;
    c.CentreCm = {100000.0, -100000.0};
    Lm::ClampCentre(c, map.BoundsMin, map.BoundsMax, 500.0);
    MpExpect(c.CentreCm.X == map.BoundsMax.X + 500.0 && c.CentreCm.Y == map.BoundsMin.Y - 500.0, "the centre is clamped to the bounds and margin");
    c.CentreCm = {0.0, 0.0};
    Lm::ClampCentre(c, map.BoundsMin, map.BoundsMax, 500.0);
    MpExpect(c.CentreCm == Lm::Point{0.0, 0.0}, "a centre inside is left alone");
}

void MpTestLabels()
{
    const Lm::Rect clip{0.0, 0.0, 400.0, 300.0};
    const Lm::LabelCandidate labels[] = {
        {{100.0, 100.0}, 80.0, 16.0, 1},  // 0: clashes with 1, lower priority
        {{130.0, 104.0}, 80.0, 16.0, 5},  // 1: wins
        {{300.0, 100.0}, 80.0, 16.0, 1},  // 2: clear
        {{395.0, 100.0}, 80.0, 16.0, 9},  // 3: off the right edge
        {{300.0, 116.0}, 80.0, 16.0, 1},  // 4: touches 2 within the gap
        {{300.0, 150.0}, 80.0, 16.0, 1},  // 5: clear
        {{300.0, 150.0}, 80.0, 16.0, 1},  // 6: identical to 5: the earlier wins
    };
    const std::vector<bool> placed = Lm::PlaceLabels(labels, clip, 2.0);
    MpExpect(placed.size() == 7, "one answer per candidate");
    MpExpect(!placed[0] && placed[1], "the higher priority wins a clash");
    MpExpect(placed[2], "an isolated label is placed");
    MpExpect(!placed[3], "a label that leaves the window is dropped");
    MpExpect(!placed[4], "labels closer than the gap clash");
    MpExpect(placed[5] && !placed[6], "ties go to the earlier candidate");
    MpExpect(Lm::PlaceLabels({}, clip, 2.0).empty(), "no candidates, no answers");

    // Placement is deterministic.
    MpExpect(Lm::PlaceLabels(labels, clip, 2.0) == placed, "placement is deterministic");

    // On the real map at the full-screen fit, every named building competes for a label and none overlap.
    const Lm::Map map = Lm::BuildLaneMap();
    for (const Lm::Size size : MpSizes)
    {
        const Lm::View view = Lm::FitView(map.BoundsMin, map.BoundsMax, size, 24.0, Lm::NorthUpHeadingDegrees);
        std::vector<Lm::LabelCandidate> candidates;
        for (const Lm::Building& b : map.Buildings)
        {
            if (b.Label.empty()) continue;
            Lm::LabelCandidate c;
            c.CentrePx = Lm::WorldToWidget(view, size, b.Centre);
            c.WidthPx = 7.0 * static_cast<double>(b.Label.size());
            c.HeightPx = 16.0;
            candidates.push_back(c);
        }
        const std::vector<bool> result = Lm::PlaceLabels(candidates, {0.0, 0.0, size.W, size.H}, 3.0);
        for (std::size_t i = 0; i < candidates.size(); ++i)
        {
            for (std::size_t j = i + 1; j < candidates.size(); ++j)
            {
                if (!result[i] || !result[j]) continue;
                const Lm::Rect a{candidates[i].CentrePx.X - candidates[i].WidthPx / 2, candidates[i].CentrePx.Y - candidates[i].HeightPx / 2,
                                 candidates[i].CentrePx.X + candidates[i].WidthPx / 2, candidates[i].CentrePx.Y + candidates[i].HeightPx / 2};
                const Lm::Rect b{candidates[j].CentrePx.X - candidates[j].WidthPx / 2, candidates[j].CentrePx.Y - candidates[j].HeightPx / 2,
                                 candidates[j].CentrePx.X + candidates[j].WidthPx / 2, candidates[j].CentrePx.Y + candidates[j].HeightPx / 2};
                MpExpect(!a.Intersects(b), "accepted labels never overlap");
            }
        }
        std::size_t shown = 0;
        for (const bool r : result) shown += r;
        MpExpect(shown >= 4, "at least a few labels are readable at every window size");
    }
}
} // namespace

int main()
{
    MpTestProjection();
    MpTestRotation();
    MpTestCulling();
    MpTestMinimap();
    MpTestVisibility();
    MpTestFit();
    MpTestPanZoom();
    MpTestLabels();
    std::cout << "LaneMapViewTests: " << MpAssertions << " assertions passed\n";
    return 0;
}
