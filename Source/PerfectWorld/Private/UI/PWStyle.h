#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

/**
 * The look of the front end: a dark navy and charcoal palette with a saffron
 * accent, all built in code so the style is diffable and needs no binary assets.
 * Brushes are owned by one function-local singleton because Slate widgets keep
 * pointers to the brushes and styles they are given.
 */
namespace PWUI
{
/** sRGB bytes to the linear colour Slate expects. */
FLinearColor Srgb(uint8 R, uint8 G, uint8 B, uint8 A = 255);

/** The engine's default font at a size. Bold uses the Bold face. */
FSlateFontInfo Font(int32 Size, bool bBold = false);

struct FStyle
{
    // Colours.
    FLinearColor Scrim;
    FLinearColor SideShade;
    FLinearColor TextPrimary;
    FLinearColor TextSecondary;
    FLinearColor TextMuted;
    FLinearColor Accent;
    FLinearColor AccentHot;
    FLinearColor Good;
    FLinearColor Warn;
    FLinearColor Danger;
    FLinearColor OnAccent;

    // Panels and cards.
    FSlateBrush BrushPanel;
    FSlateBrush BrushInset;
    FSlateBrush BrushCard;
    FSlateBrush BrushCardHover;
    FSlateBrush BrushCardSelected;
    FSlateBrush BrushCardSelectedHover;
    FSlateBrush BrushFocusRing;
    FSlateBrush BrushCardDisabled;

    // Buttons.
    FSlateBrush BrushPrimary;
    FSlateBrush BrushPrimaryHover;
    FSlateBrush BrushPrimaryPressed;
    FSlateBrush BrushSecondary;
    FSlateBrush BrushSecondaryHover;
    FSlateBrush BrushSecondaryPressed;
    FSlateBrush BrushDisabled;

    // Bars and rules.
    FSlateBrush BrushTrack;
    FSlateBrush BrushFillAccent;
    FSlateBrush BrushFillGood;
    FSlateBrush BrushFillDanger;
    FSlateBrush BrushPipOff;
    FSlateBrush BrushPipOn;
    FSlateBrush BrushRule;
    FSlateBrush BrushAccentRule;
    FSlateBrush BrushScrim;
    FSlateBrush BrushSideShade;
    FSlateBrush BrushToast;

    // Widget styles.
    FButtonStyle NoButton;
    FSliderStyle Slider;
    FEditableTextBoxStyle TextBox;
    FScrollBarStyle ScrollBar;

    static const FStyle& Get();

private:
    FStyle();
};
} // namespace PWUI
