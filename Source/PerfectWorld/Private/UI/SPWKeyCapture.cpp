#include "UI/SPWKeyCapture.h"

#include "Input/Events.h"
#include "PerfectWorldKeyBindings.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldKeyCapture"

void SPWKeyCapture::Construct(const FArguments& InArgs)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    OnCaptured = InArgs._OnCaptured;
    OnCancelled = InArgs._OnCancelled;

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(&Style.BrushInset)
        .Padding(PWUI::Spacing::Medium)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                PWUI::MakeText(InArgs._Prompt, 18, Style.Accent, true, true)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, PWUI::Spacing::XSmall, 0.0f, 0.0f)
            [
                PWUI::MakeText(InArgs._Hint, 15, Style.TextSecondary, false, true)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, PWUI::Spacing::Compact, 0.0f, 0.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [
                    PWUI::MakeButton(LOCTEXT("Cancel", "Cancel"), PWUI::EButtonKind::Secondary,
                                     FOnClicked::CreateLambda([this]()
                                     {
                                         OnCancelled.ExecuteIfBound();
                                         return FReply::Handled();
                                     }),
                                     true, 16, 100.0f)
                ]
            ]
        ]
    ];
}

FReply SPWKeyCapture::OnKeyDown(const FGeometry&, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right)
    {
        OnCancelled.ExecuteIfBound();
        return FReply::Handled();
    }
    // A held key repeats; the stick directions are menu navigation Slate makes from the analog sticks. Neither is a choice.
    if (InKeyEvent.IsRepeat() || !Key.IsValid() || KbIsStickDirection(Key))
    {
        return FReply::Handled();
    }
    OnCaptured.ExecuteIfBound(Key);
    return FReply::Handled();
}

FReply SPWKeyCapture::OnMouseButtonDown(const FGeometry&, const FPointerEvent& MouseEvent)
{
    const FKey Button = MouseEvent.GetEffectingButton();
    // The left button is how menus are clicked, so a click on the prompt only keeps the focus here.
    if (Button != EKeys::LeftMouseButton)
    {
        OnCaptured.ExecuteIfBound(Button);
    }
    return FReply::Handled();
}

FReply SPWKeyCapture::OnMouseWheel(const FGeometry&, const FPointerEvent& MouseEvent)
{
    const float Delta = MouseEvent.GetWheelDelta();
    if (Delta != 0.0f)
    {
        OnCaptured.ExecuteIfBound(Delta > 0.0f ? EKeys::MouseScrollUp : EKeys::MouseScrollDown);
    }
    return FReply::Handled();
}

void SPWKeyCapture::OnFocusLost(const FFocusEvent& InFocusEvent)
{
    SCompoundWidget::OnFocusLost(InFocusEvent);
    OnCancelled.ExecuteIfBound();
}

#undef LOCTEXT_NAMESPACE
