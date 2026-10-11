#include "UI/PWStyle.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/CompositeFont.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"

namespace PWUI
{
namespace
{
FSlateBrush Box(const FLinearColor& Fill, float Radius, const FLinearColor& Edge = FLinearColor::Transparent,
                float EdgeWidth = 0.0f)
{
    FSlateBrush Brush = FSlateRoundedBoxBrush(Fill, Radius);
    if (EdgeWidth > 0.0f)
        Brush.OutlineSettings = FSlateBrushOutlineSettings(Radius, FSlateColor(Edge), EdgeWidth);
    return Brush;
}

const TCHAR* WeightName(EFontWeight Weight)
{
    switch (Weight)
    {
    case EFontWeight::Medium: return TEXT("Medium");
    case EFontWeight::SemiBold: return TEXT("SemiBold");
    case EFontWeight::Bold: return TEXT("Bold");
    default: return TEXT("Regular");
    }
}

TSharedPtr<const FCompositeFont> MakeHindSiliguriFont()
{
    const FString Directory = FPaths::ProjectContentDir() / TEXT("UIResources/Fonts/HindSiliguri");
    const TCHAR* Weights[] = {TEXT("Regular"), TEXT("Medium"), TEXT("SemiBold"), TEXT("Bold")};
    TArray<FTypefaceEntry> Entries;
    for (const TCHAR* Weight : Weights)
    {
        const FString Path = Directory / FString::Printf(TEXT("HindSiliguri-%s.ttf"), Weight);
        if (!FPaths::FileExists(Path))
        {
            UE_LOG(LogTemp, Warning, TEXT("PWUI: Hind Siliguri missing at %s; using engine fallback."), *Path);
            return nullptr;
        }
        Entries.Emplace(FName(Weight), Path, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
    }
    TSharedPtr<FCompositeFont> Composite = MakeShared<FCompositeFont>();
    Composite->DefaultTypeface.Fonts = MoveTemp(Entries);
    return Composite;
}
} // namespace

FLinearColor Srgb(uint8 R, uint8 G, uint8 B, uint8 A)
{
    return FLinearColor::FromSRGBColor(FColor(R, G, B, A));
}

FSlateFontInfo Font(int32 Size, EFontWeight Weight)
{
    if (const TSharedPtr<const FCompositeFont> Family = FStyle::Get().CompositeFont)
        return FSlateFontInfo(Family, Size, FName(WeightName(Weight)));
    const bool bHeavy = Weight == EFontWeight::SemiBold || Weight == EFontWeight::Bold;
    return FCoreStyle::GetDefaultFontStyle(bHeavy ? "Bold" : "Regular", Size);
}

FSlateFontInfo Font(int32 Size, bool bBold)
{
    return Font(Size, bBold ? EFontWeight::Bold : EFontWeight::Regular);
}

FSlateFontInfo Font(ETextRole Role)
{
    switch (Role)
    {
    case ETextRole::Title: return Font(60, EFontWeight::SemiBold);
    case ETextRole::PageHeading: return Font(30, EFontWeight::SemiBold);
    case ETextRole::SectionHeading: return Font(22, EFontWeight::SemiBold);
    case ETextRole::Control: return Font(18, EFontWeight::Medium);
    case ETextRole::Meta: return Font(15, EFontWeight::Medium);
    default: return Font(18, EFontWeight::Regular);
    }
}

const FStyle& FStyle::Get()
{
    static const FStyle Instance;
    return Instance;
}

FStyle::FStyle()
{
    CompositeFont = MakeHindSiliguriFont();
    Background = Srgb(0x10, 0x1A, 0x1D); Panel = Srgb(0x18, 0x26, 0x2A);
    Raised = Srgb(0x22, 0x34, 0x39); Divider = Srgb(0x41, 0x53, 0x59);
    TextPrimary = Srgb(0xF4, 0xEE, 0xDF); TextSecondary = Srgb(0xBB, 0xC8, 0xC7);
    Accent = Srgb(0xE9, 0xB5, 0x68); OnAccent = Srgb(0x17, 0x1D, 0x1E);
    Focus = Srgb(0x9C, 0xDC, 0xDD); // independent of selection (amber) and text (ivory), so focus always reads
    Success = Srgb(0x91, 0xC6, 0xAC); Error = Srgb(0xF1, 0x9A, 0x8E);
    Good = Success; Warn = Accent; Danger = Error;
    TextMuted = TextSecondary.CopyWithNewOpacity(0.62f);

    BrushPanel = Box(Panel, 10.0f, Divider, 1.0f); BrushInset = Box(Background, 10.0f, Divider, 1.0f);
    BrushDialog = Box(Panel, 14.0f, Divider, 1.0f);
    BrushDialogShadow = Box(Background.CopyWithNewOpacity(0.18f), 20.0f);
    BrushCard = Box(Raised, 10.0f, Divider, 1.0f); BrushCardHover = Box(Srgb(0x2B, 0x41, 0x46), 10.0f, Accent, 1.0f);
    BrushCardPressed = Box(Srgb(0x1D, 0x2E, 0x32), 10.0f, Accent, 1.5f);
    BrushCardSelected = Box(Raised, 10.0f, Accent, 2.0f);
    BrushCardSelectedHover = Box(Srgb(0x2B, 0x41, 0x46), 10.0f, Accent, 2.0f);
    BrushCardDisabled = Box(Panel, 10.0f, Divider.CopyWithNewOpacity(0.5f), 1.0f);
    BrushTextRow = Box(FLinearColor::Transparent, 6.0f); BrushTextRowHover = Box(Raised, 6.0f);
    BrushTextRowPressed = Box(Panel, 6.0f, Accent, 1.0f); BrushFocusRing = Box(FLinearColor::Transparent, 10.0f, Focus, 2.0f);
    BrushPrimary = Box(Accent, 6.0f); BrushPrimaryHover = Box(Srgb(0xF2, 0xC7, 0x87), 6.0f);
    BrushPrimaryPressed = Box(Srgb(0xCF, 0x98, 0x4C), 6.0f);
    BrushSecondary = Box(Raised, 6.0f, Divider, 1.0f); BrushSecondaryHover = Box(Srgb(0x2B, 0x41, 0x46), 6.0f, Accent, 1.0f);
    BrushSecondaryPressed = Box(Panel, 6.0f, Accent, 1.5f); BrushDisabled = Box(Panel, 6.0f, Divider.CopyWithNewOpacity(0.5f), 1.0f);
    BrushTrack = Box(Background, 6.0f, Divider, 1.0f); BrushFillAccent = Box(Accent, 6.0f);
    BrushFillGood = Box(Success, 6.0f); BrushFillDanger = Box(Error, 6.0f); BrushPipOff = Box(Divider, 3.0f); BrushPipOn = Box(Accent, 3.0f);
    BrushRule = FSlateColorBrush(Divider); BrushAccentRule = FSlateColorBrush(Accent);
    BrushScrim = FSlateColorBrush(Background.CopyWithNewOpacity(0.78f)); BrushSideShade = FSlateColorBrush(Background.CopyWithNewOpacity(0.92f));
    BrushToast = Box(Raised, 10.0f, Divider, 1.0f); // neutral: most notices are good news (saved, life begun)

    NoButton = FButtonStyle().SetNormal(FSlateNoResource()).SetHovered(FSlateNoResource()).SetPressed(FSlateNoResource())
        .SetDisabled(FSlateNoResource()).SetNormalPadding(FMargin(0.0f)).SetPressedPadding(FMargin(0.0f));
    const FSlateBrush Thumb = FSlateRoundedBoxBrush(Accent, 10.0f, FVector2f(20.0f, 20.0f));
    const FSlateBrush ThumbHot = FSlateRoundedBoxBrush(Srgb(0xF2, 0xC7, 0x87), 10.0f, FVector2f(22.0f, 22.0f));
    const FSlateBrush ThumbOff = FSlateRoundedBoxBrush(Divider, 10.0f, FVector2f(20.0f, 20.0f));
    Slider = FSliderStyle().SetNormalBarImage(BrushTrack).SetHoveredBarImage(BrushTrack).SetDisabledBarImage(BrushTrack)
        .SetNormalThumbImage(Thumb).SetHoveredThumbImage(ThumbHot).SetDisabledThumbImage(ThumbOff).SetBarThickness(8.0f);

    TextBox = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
    TextBox.SetBackgroundImageNormal(Box(Background, 6.0f, Divider, 1.0f)).SetBackgroundImageHovered(Box(Background, 6.0f, Accent, 1.0f))
        .SetBackgroundImageFocused(Box(Background, 6.0f, Focus, 2.0f)).SetBackgroundImageReadOnly(Box(Panel, 6.0f, Divider, 1.0f))
        .SetPadding(FMargin(14.0f, 10.0f)).SetForegroundColor(FSlateColor(TextPrimary)).SetFocusedForegroundColor(FSlateColor(TextPrimary))
        .SetReadOnlyForegroundColor(FSlateColor(TextSecondary)).SetBackgroundColor(FSlateColor(FLinearColor::White))
        .SetFont(CompositeFont.IsValid() ? FSlateFontInfo(CompositeFont, 18, FName(TEXT("Regular")))
                                         : FCoreStyle::GetDefaultFontStyle("Regular", 18));
    ScrollBar = FCoreStyle::Get().GetWidgetStyle<FScrollBarStyle>("Scrollbar");
    ScrollBar.SetNormalThumbImage(Box(Divider, 4.0f)).SetHoveredThumbImage(Box(Accent, 4.0f)).SetDraggedThumbImage(Box(Accent, 4.0f))
        .SetHorizontalBackgroundImage(Box(Background, 4.0f)).SetVerticalBackgroundImage(Box(Background, 4.0f)).SetThickness(8.0f);
}
} // namespace PWUI
