#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_OneParam(FPWOnKeyCaptured, const FKey&);

/**
 * "Press a key" (order F4): a small prompt that takes the keyboard focus and reports the next key, mouse button or wheel notch the
 * player gives it. Escape and the controller's menu button cancel; so does anything that takes focus away (clicking elsewhere).
 * It reports every key it is given and leaves the judging to the binding rules; it only drops auto-repeats and the stick
 * directions Slate makes up for menu navigation. While it has focus it handles every key, so nothing else in the menu reacts.
 */
class SPWKeyCapture : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWKeyCapture) {}
        SLATE_ATTRIBUTE(FText, Prompt)
        SLATE_ATTRIBUTE(FText, Hint)
        SLATE_EVENT(FPWOnKeyCaptured, OnCaptured)
        SLATE_EVENT(FSimpleDelegate, OnCancelled)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
    virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;

private:
    FPWOnKeyCaptured OnCaptured;
    FSimpleDelegate OnCancelled;
};
