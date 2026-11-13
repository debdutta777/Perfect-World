#pragma once

#include "CoreMinimal.h"
#include "Animation/CurveSequence.h"
#include "Framework/SlateDelegates.h"
#include "Input/Reply.h"
#include "Layout/Margin.h"
#include "Styling/SlateColor.h"
#include "Types/SlateEnums.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "UI/PWStyle.h"

class STextBlock;
class SWidget;

/** Small builders that keep every screen's widgets looking and behaving the same. */
namespace PWUI
{
/** One presentation policy for every decorative motion consumer. */
bool IsReducedMotion();
float MotionLerp(float AnimatedLerp);
void PlayMotion(FCurveSequence& Sequence, const TSharedRef<SWidget>& Owner);

enum class EButtonKind : uint8
{
    /** The one saffron call to action on a screen. */
    Primary,
    /** An ordinary button. */
    Secondary,
    /** A large selectable card that holds arbitrary content. */
    Card,
    /** A compact, borderless navigation or list row. */
    TextRow,
    /** A small toggle; shows the selected look when selected. */
    Chip
};

TSharedRef<STextBlock> MakeText(const TAttribute<FText>& Text, int32 Size, const FLinearColor& Color, bool bBold = false,
                                bool bWrap = false);
TSharedRef<STextBlock> MakeText(const TAttribute<FText>& Text, ETextRole Role, const FLinearColor& Color,
                                bool bWrap = false);

/** A text button. It is a real SButton, so keyboard and controller focus work. */
TSharedRef<SWidget> MakeButton(const TAttribute<FText>& Label, EButtonKind Kind, FOnClicked OnClicked,
                               TAttribute<bool> bEnabled = true, int32 FontSize = 20, float MinWidth = 0.0f,
                               TAttribute<bool> bSelected = false);

/** A button around any content, with hover, focus, selected and disabled looks. */
TSharedRef<SWidget> MakeContentButton(TSharedRef<SWidget> Content, EButtonKind Kind, FOnClicked OnClicked,
                                      TAttribute<bool> bSelected = false, TAttribute<bool> bEnabled = true,
                                      const FMargin& Padding = FMargin(16.0f));

/** A rounded dark panel. */
TSharedRef<SWidget> MakePanel(TSharedRef<SWidget> Content, const FMargin& Padding = FMargin(24.0f));
/** Sparse brackets inside a reading surface's padding, beneath its text and controls. */
TSharedRef<SWidget> MakeBracketPanel(TSharedRef<SWidget> Content, const FMargin& Padding = FMargin(24.0f));

/** A radius-14 modal surface with the visual system's only soft shadow. */
TSharedRef<SWidget> MakeDialog(TSharedRef<SWidget> Content, const FMargin& Padding = FMargin(24.0f));

/** A one-pixel horizontal line in the panel edge colour. */
TSharedRef<SWidget> MakeRule();

/** The first widget under Root that can take keyboard focus, or null. */
TSharedPtr<SWidget> FindFirstFocusable(const TSharedRef<SWidget>& Root);

/** Moves keyboard focus to the first focusable widget under Root. The widgets must already be on screen. */
void FocusFirstFocusable(const TSharedRef<SWidget>& Root);

/**
 * Focuses the first focusable widget under Target once the layout can accept it.
 * Newly built widgets cannot take focus until they have been arranged once, so
 * this retries on Host's active timer for about a second, then gives up quietly.
 */
void FocusWhenReady(const TSharedRef<SWidget>& Host, const TWeakPtr<SWidget>& Target);

/** Cancels delayed focus work before a screen or modal changes. */
void CancelPendingFocus();
void CancelPendingFocus(const TSharedRef<SWidget>& Host);
} // namespace PWUI
