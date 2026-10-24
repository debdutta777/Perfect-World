#pragma once

#include "LaneMap.h"

#include <cstdint>
#include <span>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld::LaneMap
{
/**
 * The map view (order F2): the portable arithmetic behind the minimap and the full-screen map. World points go to widget pixels and back,
 * the camera's yaw can turn the map, shapes are culled, the map is fitted to a window, panned and zoomed, labels are placed without
 * overlapping, and the rules for when the minimap is shown and when the map may open live here. Slate only draws what this returns.
 *
 * Widget space: pixels, origin at the widget's top-left corner, x to the right, y down. The world point `CentreCm` sits at the widget's
 * centre. `UpHeadingDegrees` is the world yaw (0 faces +X, 90 faces +Y, as in Unreal) that points up on the widget; the widget's right
 * is that heading turned a quarter clockwise as seen from above. With an up heading of -90 the lane runs left to right and the
 * -Y buildings (the homes) are at the top; with the player's own yaw the map turns with the camera. Pure double arithmetic.
 */
struct Size
{
    double W = 0.0;
    double H = 0.0;
};

struct Rect
{
    double X0 = 0.0;
    double Y0 = 0.0;
    double X1 = 0.0;
    double Y1 = 0.0;
    [[nodiscard]] double Width() const noexcept { return X1 - X0; }
    [[nodiscard]] double Height() const noexcept { return Y1 - Y0; }
    [[nodiscard]] bool Contains(Point p) const noexcept { return p.X >= X0 && p.X <= X1 && p.Y >= Y0 && p.Y <= Y1; }
    [[nodiscard]] bool Intersects(const Rect& o) const noexcept { return X0 < o.X1 && o.X0 < X1 && Y0 < o.Y1 && o.Y0 < Y1; }
};

/** The map's up heading when it does not turn with the camera: the lane runs left to right. */
inline constexpr double NorthUpHeadingDegrees = -90.0;

struct View
{
    Point CentreCm;
    double UpHeadingDegrees = NorthUpHeadingDegrees;
    double PixelsPerCm = 0.1;
};

/** World to widget pixels. */
[[nodiscard]] PERFECTWORLDCORE_API Point WorldToWidget(const View& view, Size widget, Point world) noexcept;
/** Widget pixels to world; the exact inverse of WorldToWidget while PixelsPerCm > 0. */
[[nodiscard]] PERFECTWORLDCORE_API Point WidgetToWorld(const View& view, Size widget, Point pixel) noexcept;
/** The screen direction (unit, y down) of a world yaw: (0, -1) is up. */
[[nodiscard]] PERFECTWORLDCORE_API Point ScreenDirectionOfYaw(const View& view, double yawDegrees) noexcept;
/** The clockwise angle from up, in degrees (0..360), at which something facing this world yaw points on the widget. */
[[nodiscard]] PERFECTWORLDCORE_API double ScreenAngleOfYaw(const View& view, double yawDegrees) noexcept;

/** The pixel bounding box of a world polygon (empty polygon: a zero rect at the origin). */
[[nodiscard]] PERFECTWORLDCORE_API Rect ProjectedBounds(const View& view, Size widget, const Polygon& world) noexcept;
/** True when the polygon's projected bounds touch the clip rectangle. Conservative: the box, not the exact outline. */
[[nodiscard]] PERFECTWORLDCORE_API bool PolygonVisible(const View& view, Size widget, const Polygon& world, const Rect& clip) noexcept;
/** True when the world point projects inside the clip rectangle shrunk by `insetPx` on every side. */
[[nodiscard]] PERFECTWORLDCORE_API bool PointVisible(const View& view, Size widget, Point world, const Rect& clip, double insetPx) noexcept;

// ---- Fit, pan and zoom ----

struct ZoomLimits
{
    double MinPixelsPerCm = 0.01;
    double MaxPixelsPerCm = 1.0;
};

/** The view that shows the whole rectangle (world bounds) inside the widget with `paddingPx` free on every side. */
[[nodiscard]] PERFECTWORLDCORE_API View FitView(Point boundsMin, Point boundsMax, Size widget, double paddingPx, double upHeadingDegrees) noexcept;
/** Zoom range around a fitted view: a little out, and in to street detail (at least 0.6 px/cm, six times the fit). */
[[nodiscard]] PERFECTWORLDCORE_API ZoomLimits ZoomLimitsFor(const View& fitted) noexcept;
/** Multiplies the scale by `factor` (clamped to the limits) keeping the world point under `anchorPixel` where it is. */
PERFECTWORLDCORE_API void ZoomAbout(View& view, Size widget, Point anchorPixel, double factor, const ZoomLimits& limits) noexcept;
/** Moves the map under the pointer by `deltaPixels` (dragging right moves the map right). */
PERFECTWORLDCORE_API void PanByPixels(View& view, Point deltaPixels) noexcept;
/** Keeps the view's centre within the world bounds grown by `marginCm`. */
PERFECTWORLDCORE_API void ClampCentre(View& view, Point boundsMin, Point boundsMax, double marginCm) noexcept;

// ---- The minimap ----

/** Metres of world the minimap shows from its centre to the middle of each edge. */
inline constexpr double MinimapRadiusCm = 2500.0;

enum class MinimapOrientation : std::uint8_t
{
    /** The map turns so the camera's facing is always up; the player arrow stays still. */
    RotateWithCamera,
    /** The lane stays put (left to right) and the player arrow turns. */
    NorthUp
};

/** A setting-ready flag: reduced motion never turns the map under the player, whatever was asked. */
[[nodiscard]] PERFECTWORLDCORE_API MinimapOrientation EffectiveOrientation(MinimapOrientation requested, bool reducedMotion) noexcept;

/** The side of the square minimap in widget units: 22% of the height, never under 96 or over 220. */
[[nodiscard]] PERFECTWORLDCORE_API double MinimapSidePx(Size viewport) noexcept;

struct MinimapLayout
{
    double SidePx = 0.0;
    /** The minimap's rectangle in viewport units (top-right, below the clock). */
    Rect Bounds;
};

inline constexpr double MinimapMarginPx = 24.0;
/** Distance from the top edge to the minimap's top: the clock readout sits above it. */
inline constexpr double MinimapTopInsetPx = 64.0;
[[nodiscard]] PERFECTWORLDCORE_API MinimapLayout LayoutMinimap(Size viewport) noexcept;

/** The minimap's view: the player at the centre, 25 m to the middle of each edge, turned by the orientation. */
[[nodiscard]] PERFECTWORLDCORE_API View MinimapView(Point playerWorld, double playerYawDegrees, double sidePx, MinimapOrientation orientation) noexcept;

/** Why the minimap is not shown, in the order the rules apply. */
enum class MinimapHidden : std::uint8_t
{
    Visible, NoPlayer, Menu, Conversation, Reading, WorldMap, Disabled
};

struct HudState
{
    bool bHasLocalPlayer = false;
    /** The title screen, creation flow or pause menu (the front end's IsMenuOpen). */
    bool bMenuOpen = false;
    bool bConversationOpen = false;
    bool bReadingOpen = false;
    bool bWorldMapOpen = false;
    /** The player's own switch (a settings entry); true by default. */
    bool bMinimapEnabled = true;
};

[[nodiscard]] PERFECTWORLDCORE_API MinimapHidden ResolveMinimapVisibility(const HudState& state) noexcept;
/** The full map opens only in the world with nothing else in front: a player, no menu, no conversation, no reading, and not already open. */
[[nodiscard]] PERFECTWORLDCORE_API bool CanOpenWorldMap(const HudState& state) noexcept;

// ---- Labels ----

struct LabelCandidate
{
    /** The label's centre in widget pixels. */
    Point CentrePx;
    double WidthPx = 0.0;
    double HeightPx = 0.0;
    /** Higher wins a clash; ties go to the earlier candidate. */
    int Priority = 0;
};

/** One flag per candidate: true when it fits inside `clip` and overlaps no better accepted label (with `gapPx` between boxes). */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<bool> PlaceLabels(std::span<const LabelCandidate> candidates, const Rect& clip, double gapPx);
} // namespace PerfectWorld::LaneMap
