#include "UI/SPWMinimap.h"

#include "Layout/Clipping.h"
#include "Rendering/DrawElements.h"
#include "UI/PWMapData.h"
#include "UI/PWMapPainter.h"
#include "UI/PWMapPlan.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SLeafWidget.h"

const TCHAR* const SPWMinimap::SquareTag = TEXT("PWMinimapSquare");

namespace
{
constexpr float MpInsetPx = 4.0f;
constexpr float MpBackdropOpacity = 0.9f;
constexpr float MpDoorMarkPx = 5.0f;
constexpr float MpPersonMarkPx = 5.0f;

/** The painted square. It reads the live snapshot through a shared pointer and draws a plan; it holds no rules. */
class SMpMinimapCanvas final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SMpMinimapCanvas) {}
        SLATE_ARGUMENT(TSharedPtr<FPWMapLive>, Live)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        Live = InArgs._Live;
        // Nothing here is clickable or focusable: the minimap can never take input from the world behind it.
        SetVisibility(EVisibility::HitTestInvisible);
    }

    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                          int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
    {
        const PWUI::FStyle& Style = PWUI::FStyle::Get();
        const FVector2f Size = AllottedGeometry.GetLocalSize();
        const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
        FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), &Style.BrushInset, ESlateDrawEffect::None,
                                   FLinearColor(1.0f, 1.0f, 1.0f, MpBackdropOpacity) * Tint);
        if (!Live.IsValid() || Size.X <= 2.0f * MpInsetPx || Size.Y <= 2.0f * MpInsetPx)
        {
            return LayerId;
        }

        // The map is clipped inside the rounded border, inset far enough that no corner pokes through the radius.
        const FGeometry Inner = AllottedGeometry.MakeChild(Size - FVector2f(2.0f * MpInsetPx, 2.0f * MpInsetPx), FSlateLayoutTransform(FVector2f(MpInsetPx, MpInsetPx)));
        OutDrawElements.PushClip(FSlateClippingZone(Inner));

        const FPWMapSnapshot& Snapshot = Live->GetSnapshot();
        const PerfectWorld::LaneMap::View View = PWMap::MakeMinimapView(Snapshot, Live->GetPreferences(), Size.X, PWUI::IsReducedMotion());
        const PerfectWorld::LaneMap::Rect Clip{MpInsetPx, MpInsetPx, Size.X - MpInsetPx, Size.Y - MpInsetPx};
        PWMap::FPlanOptions Options;
        Options.DotInsetPx = MpInsetPx;
        const PWMap::FMapPlan Plan = PWMap::BuildPlan(Live->GetScene(), Snapshot, View, Size, Clip, Options);

        int32 Layer = LayerId + 1;
        Layer = PWMap::PaintQuads(Plan, AllottedGeometry, OutDrawElements, Layer, Tint, true) + 1;
        Layer = PWMap::PaintDots(Plan, AllottedGeometry, OutDrawElements, Layer, Tint, MpDoorMarkPx, MpPersonMarkPx) + 1;
        Layer = PWMap::PaintPlayer(Plan.Player, 1.0f, -1.0f, AllottedGeometry, OutDrawElements, Layer, Tint);
        OutDrawElements.PopClip();
        return Layer + 1;
    }

private:
    TSharedPtr<FPWMapLive> Live;
};
} // namespace

void SPWMinimap::Construct(const FArguments& InArgs)
{
    Live = InArgs._Live;
    WorldMapOpen = InArgs._WorldMapOpen;
    Layout = PerfectWorld::LaneMap::LayoutMinimap({ViewportSize.X, ViewportSize.Y});

    // The widget fills the viewport and takes no input at all; only the square inside it paints.
    SetVisibility(EVisibility::HitTestInvisible);

    ChildSlot
    [
        SNew(SBox)
        .HAlign(HAlign_Right)
        .VAlign(VAlign_Top)
        .Padding(TAttribute<FMargin>::CreateLambda([this]()
        {
            return FMargin(0.0f, static_cast<float>(Layout.Bounds.Y0), static_cast<float>(PerfectWorld::LaneMap::MinimapMarginPx), 0.0f);
        }))
        [
            SAssignNew(Square, SBox)
            .Tag(FName(SquareTag))
            .WidthOverride(TAttribute<FOptionalSize>::CreateLambda([this]() { return FOptionalSize(static_cast<float>(Layout.SidePx)); }))
            .HeightOverride(TAttribute<FOptionalSize>::CreateLambda([this]() { return FOptionalSize(static_cast<float>(Layout.SidePx)); }))
            .Visibility(TAttribute<EVisibility>::CreateLambda([this]()
            {
                return IsMinimapShown() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
            }))
            [
                SNew(SMpMinimapCanvas).Live(Live)
            ]
        ]
    ];
}

void SPWMinimap::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    const FVector2f Size = AllottedGeometry.GetLocalSize();
    if (Size.X > 1.0f && Size.Y > 1.0f)
    {
        ViewportSize = Size;
    }
    Layout = PerfectWorld::LaneMap::LayoutMinimap({ViewportSize.X, ViewportSize.Y});
    if (!Live.IsValid())
    {
        Hidden = PerfectWorld::LaneMap::MinimapHidden::NoPlayer;
        return;
    }
    Live->Refresh();
    Hidden = PerfectWorld::LaneMap::ResolveMinimapVisibility(Live->ReadHudState(WorldMapOpen.Get(false)));
}
