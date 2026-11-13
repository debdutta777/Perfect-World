#include "LaneMapView.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace PerfectWorld::LaneMap
{
namespace
{
constexpr double MpPi = 3.14159265358979323846;
constexpr double MpMinimapSideFraction = 0.22;
constexpr double MpMinimapSideMinPx = 96.0;
constexpr double MpMinimapSideMaxPx = 220.0;
constexpr double MpMinScale = 1.0e-6;

double MpRadians(double degrees) noexcept
{
    return degrees * MpPi / 180.0;
}

/** The unit vectors of a view in world space: `up` is the heading pointing up on the widget, `right` is a quarter turn clockwise from it. */
struct MpAxes
{
    Point Up;
    Point Right;
};

MpAxes MpAxesOf(const View& view) noexcept
{
    const double h = MpRadians(view.UpHeadingDegrees);
    return {{std::cos(h), std::sin(h)}, {-std::sin(h), std::cos(h)}};
}

double MpScaleOf(const View& view) noexcept
{
    return view.PixelsPerCm > MpMinScale ? view.PixelsPerCm : MpMinScale;
}
} // namespace

Point WorldToWidget(const View& view, Size widget, Point world) noexcept
{
    const MpAxes axes = MpAxesOf(view);
    const double s = MpScaleOf(view);
    const double dx = world.X - view.CentreCm.X;
    const double dy = world.Y - view.CentreCm.Y;
    return {widget.W * 0.5 + (dx * axes.Right.X + dy * axes.Right.Y) * s, widget.H * 0.5 - (dx * axes.Up.X + dy * axes.Up.Y) * s};
}

Point WidgetToWorld(const View& view, Size widget, Point pixel) noexcept
{
    const MpAxes axes = MpAxesOf(view);
    const double s = MpScaleOf(view);
    const double along = (pixel.X - widget.W * 0.5) / s;  // distance along `Right`
    const double above = -(pixel.Y - widget.H * 0.5) / s; // distance along `Up`
    return {view.CentreCm.X + along * axes.Right.X + above * axes.Up.X, view.CentreCm.Y + along * axes.Right.Y + above * axes.Up.Y};
}

Point ScreenDirectionOfYaw(const View& view, double yawDegrees) noexcept
{
    const double theta = MpRadians(yawDegrees - view.UpHeadingDegrees);
    return {std::sin(theta), -std::cos(theta)};
}

double ScreenAngleOfYaw(const View& view, double yawDegrees) noexcept
{
    double angle = std::fmod(yawDegrees - view.UpHeadingDegrees, 360.0);
    if (angle < 0.0)
    {
        angle += 360.0;
    }
    return angle;
}

Rect ProjectedBounds(const View& view, Size widget, const Polygon& world) noexcept
{
    if (world.empty())
    {
        return {};
    }
    Rect box;
    bool first = true;
    for (const Point& p : world)
    {
        const Point q = WorldToWidget(view, widget, p);
        if (first)
        {
            box = {q.X, q.Y, q.X, q.Y};
            first = false;
            continue;
        }
        box.X0 = std::min(box.X0, q.X);
        box.Y0 = std::min(box.Y0, q.Y);
        box.X1 = std::max(box.X1, q.X);
        box.Y1 = std::max(box.Y1, q.Y);
    }
    return box;
}

bool PolygonVisible(const View& view, Size widget, const Polygon& world, const Rect& clip) noexcept
{
    if (world.empty())
    {
        return false;
    }
    const Rect box = ProjectedBounds(view, widget, world);
    return box.X0 <= clip.X1 && clip.X0 <= box.X1 && box.Y0 <= clip.Y1 && clip.Y0 <= box.Y1;
}

bool PointVisible(const View& view, Size widget, Point world, const Rect& clip, double insetPx) noexcept
{
    const Point q = WorldToWidget(view, widget, world);
    return q.X >= clip.X0 + insetPx && q.X <= clip.X1 - insetPx && q.Y >= clip.Y0 + insetPx && q.Y <= clip.Y1 - insetPx;
}

View FitView(Point boundsMin, Point boundsMax, Size widget, double paddingPx, double upHeadingDegrees) noexcept
{
    View view;
    view.UpHeadingDegrees = upHeadingDegrees;
    view.CentreCm = {(boundsMin.X + boundsMax.X) * 0.5, (boundsMin.Y + boundsMax.Y) * 0.5};
    view.PixelsPerCm = 1.0;
    // The bounds' four corners at unit scale: their spread on the widget is the size the map needs.
    const Polygon corners{{boundsMin.X, boundsMin.Y}, {boundsMax.X, boundsMin.Y}, {boundsMax.X, boundsMax.Y}, {boundsMin.X, boundsMax.Y}};
    const Rect needed = ProjectedBounds(view, widget, corners);
    const double availW = std::max(1.0, widget.W - 2.0 * paddingPx);
    const double availH = std::max(1.0, widget.H - 2.0 * paddingPx);
    const double w = std::max(needed.Width(), 1.0e-9);
    const double h = std::max(needed.Height(), 1.0e-9);
    view.PixelsPerCm = std::max(MpMinScale, std::min(availW / w, availH / h));
    return view;
}

ZoomLimits ZoomLimitsFor(const View& fitted) noexcept
{
    ZoomLimits limits;
    limits.MinPixelsPerCm = fitted.PixelsPerCm * 0.75;
    limits.MaxPixelsPerCm = std::max(fitted.PixelsPerCm * 6.0, 0.6);
    return limits;
}

void ZoomAbout(View& view, Size widget, Point anchorPixel, double factor, const ZoomLimits& limits) noexcept
{
    if (!(factor > 0.0))
    {
        return;
    }
    const Point anchored = WidgetToWorld(view, widget, anchorPixel);
    view.PixelsPerCm = std::clamp(MpScaleOf(view) * factor, limits.MinPixelsPerCm, limits.MaxPixelsPerCm);
    const Point moved = WidgetToWorld(view, widget, anchorPixel);
    view.CentreCm.X += anchored.X - moved.X;
    view.CentreCm.Y += anchored.Y - moved.Y;
}

void PanByPixels(View& view, Point deltaPixels) noexcept
{
    const MpAxes axes = MpAxesOf(view);
    const double s = MpScaleOf(view);
    const double along = deltaPixels.X / s;
    const double above = -deltaPixels.Y / s;
    view.CentreCm.X -= along * axes.Right.X + above * axes.Up.X;
    view.CentreCm.Y -= along * axes.Right.Y + above * axes.Up.Y;
}

void ClampCentre(View& view, Point boundsMin, Point boundsMax, double marginCm) noexcept
{
    view.CentreCm.X = std::clamp(view.CentreCm.X, boundsMin.X - marginCm, boundsMax.X + marginCm);
    view.CentreCm.Y = std::clamp(view.CentreCm.Y, boundsMin.Y - marginCm, boundsMax.Y + marginCm);
}

MinimapOrientation EffectiveOrientation(MinimapOrientation requested, bool reducedMotion) noexcept
{
    return reducedMotion ? MinimapOrientation::NorthUp : requested;
}

double MinimapSidePx(Size viewport) noexcept
{
    return std::clamp(viewport.H * MpMinimapSideFraction, MpMinimapSideMinPx, MpMinimapSideMaxPx);
}

MinimapLayout LayoutMinimap(Size viewport) noexcept
{
    MinimapLayout layout;
    layout.SidePx = MinimapSidePx(viewport);
    layout.Bounds.X1 = viewport.W - MinimapMarginPx;
    layout.Bounds.X0 = layout.Bounds.X1 - layout.SidePx;
    layout.Bounds.Y0 = MinimapTopInsetPx;
    layout.Bounds.Y1 = layout.Bounds.Y0 + layout.SidePx;
    return layout;
}

View MinimapView(Point playerWorld, double playerYawDegrees, double sidePx, MinimapOrientation orientation) noexcept
{
    View view;
    view.CentreCm = playerWorld;
    view.UpHeadingDegrees = orientation == MinimapOrientation::RotateWithCamera ? playerYawDegrees : NorthUpHeadingDegrees;
    view.PixelsPerCm = (sidePx * 0.5) / MinimapRadiusCm;
    return view;
}

MinimapHidden ResolveMinimapVisibility(const HudState& state) noexcept
{
    if (!state.bHasLocalPlayer) return MinimapHidden::NoPlayer;
    if (state.bMenuOpen) return MinimapHidden::Menu;
    if (state.bWorldMapOpen) return MinimapHidden::WorldMap;
    if (state.bConversationOpen) return MinimapHidden::Conversation;
    if (state.bReadingOpen) return MinimapHidden::Reading;
    return state.bMinimapEnabled ? MinimapHidden::Visible : MinimapHidden::Disabled;
}

bool CanOpenWorldMap(const HudState& state) noexcept
{
    return state.bHasLocalPlayer && !state.bMenuOpen && !state.bConversationOpen && !state.bReadingOpen && !state.bWorldMapOpen;
}

std::vector<bool> PlaceLabels(std::span<const LabelCandidate> candidates, const Rect& clip, double gapPx)
{
    std::vector<bool> accepted(candidates.size(), false);
    std::vector<std::size_t> order(candidates.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(), [&candidates](std::size_t a, std::size_t b) { return candidates[a].Priority > candidates[b].Priority; });

    std::vector<Rect> placed;
    for (const std::size_t index : order)
    {
        const LabelCandidate& c = candidates[index];
        const Rect box{c.CentrePx.X - c.WidthPx * 0.5, c.CentrePx.Y - c.HeightPx * 0.5, c.CentrePx.X + c.WidthPx * 0.5, c.CentrePx.Y + c.HeightPx * 0.5};
        if (box.X0 < clip.X0 || box.Y0 < clip.Y0 || box.X1 > clip.X1 || box.Y1 > clip.Y1)
        {
            continue;
        }
        const Rect padded{box.X0 - gapPx, box.Y0 - gapPx, box.X1 + gapPx, box.Y1 + gapPx};
        const bool clash = std::any_of(placed.begin(), placed.end(), [&padded](const Rect& r) { return padded.Intersects(r); });
        if (clash)
        {
            continue;
        }
        accepted[index] = true;
        placed.push_back(box);
    }
    return accepted;
}
} // namespace PerfectWorld::LaneMap
