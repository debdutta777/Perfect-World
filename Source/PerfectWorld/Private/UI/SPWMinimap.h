#pragma once

#include "CoreMinimal.h"
#include "LaneMapView.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FPWMapLive;
class SBox;

/**
 * The minimap (order F2): a rounded square in the upper-right corner of the window, under the clock readout, showing roughly 25 m of the
 * lane around the player. It draws what the portable lane map says (roads, footpaths, buildings coloured by kind, front doors with their
 * current state) and who is standing in the lane now (named people the presenter has actually spawned), with the player's arrow at the
 * centre. By default the map turns with the camera so the way you face is always up; with reduced motion, or when the setting is off,
 * the lane stays still and the arrow turns. The orientation and an on/off switch live in FPWMapPreferences (settings-ready).
 *
 * It never takes input: the whole widget is hit-test invisible and sits at the top right, clear of the interaction prompt (lower centre),
 * the conversation panel and the clock. It is collapsed under the title, creation and pause menus, during a conversation or a reading,
 * and while the full map is open (LaneMapView.h ResolveMinimapVisibility). All geometry and visibility rules are portable; this widget
 * only paints a plan (PWMapPlan.h).
 *
 * The widget fills the viewport (it is held by a viewport slot) and places the square inside it; it must stay unfocusable.
 */
class SPWMinimap : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWMinimap) {}
        SLATE_ARGUMENT(TSharedPtr<FPWMapLive>, Live)
        /** True while the full-screen map is open; the minimap steps aside. */
        SLATE_ATTRIBUTE(bool, WorldMapOpen)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;

    /** Why the minimap is hidden now, or Visible. Evaluated every Tick. */
    PerfectWorld::LaneMap::MinimapHidden GetHidden() const { return Hidden; }
    bool IsMinimapShown() const { return Hidden == PerfectWorld::LaneMap::MinimapHidden::Visible; }

    /** The square the minimap occupies in viewport units, from the viewport size last seen by Tick. */
    PerfectWorld::LaneMap::MinimapLayout GetLayout() const { return Layout; }

    /** Tag the tests and the acceptance scenarios search for. */
    static const TCHAR* const SquareTag;

private:
    TSharedPtr<FPWMapLive> Live;
    TAttribute<bool> WorldMapOpen;
    TSharedPtr<SBox> Square;
    PerfectWorld::LaneMap::MinimapHidden Hidden = PerfectWorld::LaneMap::MinimapHidden::NoPlayer;
    PerfectWorld::LaneMap::MinimapLayout Layout;
    FVector2f ViewportSize = FVector2f(1280.0f, 720.0f);
};
