#include "UI/PWStyle.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"

namespace PWUI
{
namespace
{
/** A rounded box with an optional inner outline. */
FSlateBrush StyleBox(const FLinearColor& Fill, float Radius, const FLinearColor& Edge = FLinearColor::Transparent,
                float EdgeWidth = 0.0f)
{
    FSlateBrush Brush = FSlateRoundedBoxBrush(Fill, Radius);
    if (EdgeWidth > 0.0f)
    {
        Brush.OutlineSettings = FSlateBrushOutlineSettings(Radius, FSlateColor(Edge), EdgeWidth);
    }
    return Brush;
}

FSlateBrush StyleSolid(const FLinearColor& Fill)
{
    return FSlateColorBrush(Fill);
}
} // namespace

FLinearColor Srgb(uint8 R, uint8 G, uint8 B, uint8 A)
{
    return FLinearColor::FromSRGBColor(FColor(R, G, B, A));
}

FSlateFontInfo Font(int32 Size, bool bBold)
{
    return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
}

const FStyle& FStyle::Get()
{
    static const FStyle Instance;
    return Instance;
}

FStyle::FStyle()
{
    Scrim = Srgb(6, 8, 16, 150);
    SideShade = Srgb(8, 11, 22, 214);
    TextPrimary = Srgb(240, 233, 220);
    TextSecondary = Srgb(176, 182, 200);
    TextMuted = Srgb(122, 130, 152);
    Accent = Srgb(244, 162, 48);
    AccentHot = Srgb(255, 190, 92);
    Good = Srgb(122, 204, 136);
    Warn = Srgb(244, 162, 48);
    Danger = Srgb(240, 106, 88);
    OnAccent = Srgb(30, 20, 8);

    const FLinearColor PanelFill = Srgb(15, 20, 36, 240);
    const FLinearColor PanelEdge = Srgb(58, 66, 94);
    const FLinearColor CardFill = Srgb(26, 33, 56);
    const FLinearColor CardEdge = Srgb(54, 62, 90);
    const FLinearColor CardHoverFill = Srgb(36, 45, 74);
    const FLinearColor SelectedFill = Srgb(52, 40, 28);

    BrushPanel = StyleBox(PanelFill, 14.0f, PanelEdge, 1.5f);
    BrushInset = StyleBox(Srgb(10, 14, 27, 235), 10.0f, Srgb(40, 47, 70), 1.0f);
    BrushCard = StyleBox(CardFill, 10.0f, CardEdge, 1.5f);
    BrushCardHover = StyleBox(CardHoverFill, 10.0f, Srgb(96, 108, 148), 1.5f);
    BrushCardSelected = StyleBox(SelectedFill, 10.0f, Accent, 2.5f);
    BrushCardSelectedHover = StyleBox(Srgb(66, 50, 32), 10.0f, AccentHot, 2.5f);
    BrushFocusRing = StyleBox(CardHoverFill, 10.0f, TextPrimary, 2.5f);
    BrushCardDisabled = StyleBox(Srgb(20, 25, 42), 10.0f, Srgb(38, 44, 64), 1.5f);

    BrushPrimary = StyleBox(Accent, 10.0f, Srgb(255, 214, 140), 1.5f);
    BrushPrimaryHover = StyleBox(AccentHot, 10.0f, Srgb(255, 232, 186), 2.0f);
    BrushPrimaryPressed = StyleBox(Srgb(214, 132, 28), 10.0f, Srgb(255, 190, 92), 1.5f);
    BrushSecondary = StyleBox(Srgb(30, 38, 64), 10.0f, Srgb(70, 80, 112), 1.5f);
    BrushSecondaryHover = StyleBox(Srgb(44, 54, 88), 10.0f, Accent, 1.5f);
    BrushSecondaryPressed = StyleBox(Srgb(22, 28, 48), 10.0f, Accent, 1.5f);
    BrushDisabled = StyleBox(Srgb(24, 29, 46), 10.0f, Srgb(40, 46, 66), 1.5f);

    BrushTrack = StyleBox(Srgb(8, 11, 22), 6.0f, Srgb(46, 54, 80), 1.0f);
    BrushFillAccent = StyleBox(Accent, 6.0f);
    BrushFillGood = StyleBox(Good, 6.0f);
    BrushFillDanger = StyleBox(Danger, 6.0f);
    BrushPipOff = StyleBox(Srgb(34, 42, 68), 3.0f);
    BrushPipOn = StyleBox(Accent, 3.0f);
    BrushRule = StyleSolid(Srgb(56, 64, 92));
    BrushAccentRule = StyleSolid(Accent);
    BrushScrim = StyleSolid(Scrim);
    BrushSideShade = StyleSolid(SideShade);
    BrushToast = StyleBox(Srgb(34, 24, 18, 245), 10.0f, Danger, 1.5f);

    NoButton = FButtonStyle()
                   .SetNormal(FSlateNoResource())
                   .SetHovered(FSlateNoResource())
                   .SetPressed(FSlateNoResource())
                   .SetDisabled(FSlateNoResource())
                   .SetNormalPadding(FMargin(0.0f))
                   .SetPressedPadding(FMargin(0.0f));

    FSlateBrush Thumb = FSlateRoundedBoxBrush(Accent, 10.0f, FVector2f(20.0f, 20.0f));
    FSlateBrush ThumbHot = FSlateRoundedBoxBrush(AccentHot, 10.0f, FVector2f(22.0f, 22.0f));
    FSlateBrush ThumbOff = FSlateRoundedBoxBrush(Srgb(70, 78, 104), 10.0f, FVector2f(20.0f, 20.0f));
    Slider = FSliderStyle()
                 .SetNormalBarImage(BrushTrack)
                 .SetHoveredBarImage(BrushTrack)
                 .SetDisabledBarImage(BrushTrack)
                 .SetNormalThumbImage(Thumb)
                 .SetHoveredThumbImage(ThumbHot)
                 .SetDisabledThumbImage(ThumbOff)
                 .SetBarThickness(8.0f);

    TextBox = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
    TextBox.SetBackgroundImageNormal(StyleBox(Srgb(10, 14, 27), 8.0f, Srgb(70, 80, 112), 1.5f))
        .SetBackgroundImageHovered(StyleBox(Srgb(14, 19, 36), 8.0f, Srgb(110, 122, 160), 1.5f))
        .SetBackgroundImageFocused(StyleBox(Srgb(14, 19, 36), 8.0f, Accent, 2.0f))
        .SetBackgroundImageReadOnly(StyleBox(Srgb(18, 23, 40), 8.0f, Srgb(46, 54, 80), 1.5f))
        .SetPadding(FMargin(14.0f, 10.0f))
        .SetForegroundColor(FSlateColor(TextPrimary))
        .SetFocusedForegroundColor(FSlateColor(TextPrimary))
        .SetReadOnlyForegroundColor(FSlateColor(TextSecondary))
        .SetBackgroundColor(FSlateColor(FLinearColor::White))
        .SetFont(Font(18));

    ScrollBar = FCoreStyle::Get().GetWidgetStyle<FScrollBarStyle>("Scrollbar");
    ScrollBar.SetNormalThumbImage(StyleBox(Srgb(86, 96, 130), 4.0f))
        .SetHoveredThumbImage(StyleBox(Accent, 4.0f))
        .SetDraggedThumbImage(StyleBox(AccentHot, 4.0f))
        .SetHorizontalBackgroundImage(StyleBox(Srgb(8, 11, 22, 160), 4.0f))
        .SetVerticalBackgroundImage(StyleBox(Srgb(8, 11, 22, 160), 4.0f))
        .SetThickness(8.0f);
}
} // namespace PWUI
