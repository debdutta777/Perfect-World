#pragma once

#include "CoreMinimal.h"
#include "LaneMapView.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FPWMapLive;
class SWidget;

/** Layout rules of the full map, portable enough to test without a window. */
namespace PWMap
{
/** The same density switch as the pause menu and the journal: short or narrow windows are compact. */
bool WorldMapCompact(const FVector2f& Space);
/** Space kept free round the fitted lane, in widget units. */
float WorldMapPadding(const FVector2f& Space);

/** Widget tags the tests and the acceptance scenarios search for. */
inline const TCHAR* const WorldMapTag = TEXT("PWWorldMap");
inline const TCHAR* const WorldMapTopBarTag = TEXT("PWWorldMapTopBar");
inline const TCHAR* const WorldMapLegendTag = TEXT("PWWorldMapLegend");
inline const TCHAR* const WorldMapCloseTag = TEXT("PWWorldMapClose");
} // namespace PWMap

/**
 * The full-screen map (order F2). It shows the same portable lane map as the minimap, large: roads, footpaths, buildings coloured by kind with
 * their names, front doors with whether they are open, private, closed for now or locked, the named people standing in the lane, and a
 * "you are here" marker. A hover line says what is under the pointer. A legend explains every mark.
 *
 * Keys and controller (all of it works with either alone):
 *   Pan        drag with the mouse (left, middle or right button)   | left stick | arrow keys or W A S D
 *   Zoom       mouse wheel (about the pointer)                      | triggers (right in, left out) | + and -
 *   Centre     Home or C, or the Centre on me button
 *   Legend     L, or the Legend button
 *   Close      M, Esc, Backspace, the controller's View/Back button, B, any key given as ExtraCloseKeys (the player's Map binding), or the Close button (a held key never repeats it)
 * Tab and the D-pad move between the buttons; every button is a real focusable button.
 *
 * Motion: the zoom eases and the "you are here" ring pulses, both only while reduced motion is off; with it on the zoom snaps and the ring is
 * a still circle. The widget holds no rules and mirrors none: geometry, fit, zoom and label placement come from LaneMapView.h and what is
 * drawn from PWMapPlan.h. It does not pause the game itself; UPWMapSubsystem does that exactly as the pause menu does.
 */
class SPWWorldMap : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWWorldMap) {}
        SLATE_ARGUMENT(TSharedPtr<FPWMapLive>, Live)
        SLATE_EVENT(FSimpleDelegate, OnClose)
        /** Keys that close the map besides M, Esc, B, Back and Menu: the player's own binding(s) of the Map action (PerfectWorldKeyBindings.h). */
        SLATE_ARGUMENT(TArray<FKey>, ExtraCloseKeys)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;
    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                          int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
    virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
    virtual FReply OnAnalogValueChanged(const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogInputEvent) override;
    virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
    virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
    virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;

    // ---- State, for the buttons, the tests and the acceptance scenarios ----
    const PerfectWorld::LaneMap::View& GetView() const { return View; }
    const PerfectWorld::LaneMap::View& GetFittedView() const { return Fitted; }
    FVector2f GetSpace() const { return Space; }
    bool IsLegendShown() const { return bLegendShown; }
    FText GetHoverText() const { return HoverText; }
    bool IsDragging() const { return bDragging; }

    /** Puts the map back as opened: the whole lane in view. */
    void ResetView();
    /** Centres on the player, zooming in to street scale when the lane is fitted. A no-op with no player. */
    void CentreOnPlayer();
    /** Zooms about the middle of the window (the buttons and keys); the wheel zooms about the pointer. */
    void ZoomBy(double Factor, bool bImmediate = false);
    /** Moves the view as a drag of this many widget units would. */
    void PanBy(const FVector2f& DeltaPixels);
    void SetLegendShown(bool bShown);
    void RequestClose();

private:
    void Refit(const FVector2f& NewSpace);
    void ApplyZoom(double Factor, const FVector2f& AnchorPixel, bool bImmediate);
    void UpdateHover();
    FReply HandleKey(const FKey& Key, bool bRepeat);
    TSharedRef<SWidget> BuildTopBar();
    TSharedRef<SWidget> BuildLegend();

    TSharedPtr<FPWMapLive> Live;
    FSimpleDelegate OnClose;
    TArray<FKey> ExtraCloseKeys;
    TSharedPtr<SWidget> TopBar;
    TSharedPtr<SWidget> LegendPanel;

    PerfectWorld::LaneMap::View View;
    PerfectWorld::LaneMap::View Fitted;
    PerfectWorld::LaneMap::ZoomLimits Limits;
    FVector2f Space = FVector2f(1280.0f, 720.0f);
    FVector2f LastBars = FVector2f::ZeroVector;
    bool bHasLayout = false;
    bool bUserMoved = false;

    bool bLegendShown = true;
    bool bDragging = false;
    FVector2f LastDragPixel = FVector2f::ZeroVector;
    FVector2f CursorPixel = FVector2f::ZeroVector;
    bool bCursorInside = false;
    FText HoverText;
    int32 HoverBuilding = INDEX_NONE;

    // Easing zoom: the log of the factor still to apply, and where it zooms about.
    double PendingZoomLog = 0.0;
    FVector2f PendingZoomAnchor = FVector2f::ZeroVector;
    // Held controller axes and keys, in -1..1.
    FVector2f StickPan = FVector2f::ZeroVector;
    FVector2f KeyPan = FVector2f::ZeroVector;
    float TriggerIn = 0.0f;
    float TriggerOut = 0.0f;
    double Now = 0.0;
};
