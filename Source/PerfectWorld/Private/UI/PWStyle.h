#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

namespace PWUI
{
namespace Spacing
{
inline constexpr float XSmall = 4.0f;
inline constexpr float Small = 8.0f;
inline constexpr float Compact = 12.0f;
inline constexpr float Medium = 16.0f;
inline constexpr float Large = 24.0f;
inline constexpr float XLarge = 32.0f;
inline constexpr float Huge = 48.0f;
inline constexpr float Massive = 64.0f;
}
FLinearColor Srgb(uint8 R, uint8 G, uint8 B, uint8 A = 255);

enum class EFontWeight : uint8 { Regular, Medium, SemiBold, Bold };
enum class ETextRole : uint8 { Title, PageHeading, SectionHeading, Section = SectionHeading, Body, Control, Meta };

FSlateFontInfo Font(int32 Size, EFontWeight Weight = EFontWeight::Regular);
FSlateFontInfo Font(int32 Size, bool bBold);
FSlateFontInfo Font(ETextRole Role);

struct FStyle
{
    FLinearColor Background, Panel, Raised, Divider;
    FLinearColor TextPrimary, TextSecondary, Accent, OnAccent, Focus, Success, Error, TextMuted;
    FLinearColor Good, Warn, Danger;

    FSlateBrush BrushPanel, BrushInset, BrushDialog, BrushDialogShadow;
    FSlateBrush BrushCard, BrushCardHover, BrushCardPressed, BrushCardSelected, BrushCardSelectedHover, BrushCardDisabled;
    FSlateBrush BrushTextRow, BrushTextRowHover, BrushTextRowPressed, BrushFocusRing;
    FSlateBrush BrushPrimary, BrushPrimaryHover, BrushPrimaryPressed;
    FSlateBrush BrushSecondary, BrushSecondaryHover, BrushSecondaryPressed, BrushDisabled;
    FSlateBrush BrushTrack, BrushFillAccent, BrushFillGood, BrushFillDanger, BrushPipOff, BrushPipOn;
    FSlateBrush BrushRule, BrushAccentRule, BrushScrim, BrushSideShade, BrushToast;

    FButtonStyle NoButton;
    FSliderStyle Slider;
    FEditableTextBoxStyle TextBox;
    FScrollBarStyle ScrollBar;
    TSharedPtr<const FCompositeFont> CompositeFont;

    static const FStyle& Get();
private:
    FStyle();
};
} // namespace PWUI
