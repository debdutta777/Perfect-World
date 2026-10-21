#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "PerfectWorldKeyBindings.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

namespace PWKbUi
{
/** Tags the widgets tests and focus handling look for. */
inline const FName CaptureTag(TEXT("PWKb.Capture"));
inline const FName ConfirmTag(TEXT("PWKb.Confirm"));
inline const FName ResetAllTag(TEXT("PWKb.ResetAll"));
inline FName SlotTag(EKbAction Action, EKbDevice Device)
{
    return FName(*FString::Printf(TEXT("PWKb.Slot.%s.%s"), KbInfo(Action).Id, Device == EKbDevice::Keyboard ? TEXT("kb") : TEXT("pad")));
}
inline FName ResetTag(EKbAction Action)
{
    return FName(*FString::Printf(TEXT("PWKb.Reset.%s"), KbInfo(Action).Id));
}
} // namespace PWKbUi

/**
 * The Controls half of the Settings screen (order F4): every rebindable action with its keyboard and gamepad binding. Select a binding,
 * press the key or button you want (Escape or the menu button cancels), and if another action already uses it, choose to swap the two.
 * Each action has Reset, and the whole list has Reset all. Pause is shown but locked. Changes go straight to the key binding store, which
 * saves them and rebuilds the walking player's mapping context. All of it works from the keyboard and a controller: the bindings are
 * ordinary buttons, the prompts take focus when they open and give it back when they close.
 */
class SPWControlsSection : public SCompoundWidget
{
public:
    enum class EPhase : uint8
    {
        Idle,
        Capturing,  // waiting for a key for ActiveAction / ActiveDevice
        Confirming  // that key belongs to another action; waiting for swap or cancel
    };

    SLATE_BEGIN_ARGS(SPWControlsSection) : _Store(nullptr) {}
        /** The store to edit; the process-wide one if left null. Tests pass their own. */
        SLATE_ARGUMENT(FKbBindingStore*, Store)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

    EPhase GetPhase() const { return Phase; }
    EKbAction GetActiveAction() const { return ActiveAction; }
    EKbDevice GetActiveDevice() const { return ActiveDevice; }
    const FText& GetStatus() const { return Status; }

    /** Starts waiting for a key for one slot. Does nothing for a fixed slot. */
    void BeginCapture(EKbAction Action, EKbDevice Device);
    /** A key, button or wheel notch arrived while capturing. */
    void SubmitKey(const FKey& Key);
    /** Accepts the pending swap. */
    void ConfirmSwap();
    /** Abandons a capture or a pending swap, changing nothing. */
    void CancelPrompt();
    void ResetAction(EKbAction Action);
    void ResetEverything();

private:
    TSharedRef<SWidget> MakeGroup(EKbGroup Group);
    TSharedRef<SWidget> MakeActionBlock(EKbAction Action);
    TSharedRef<SWidget> MakeBindingButton(EKbAction Action, EKbDevice Device);
    TSharedRef<SWidget> MakeConfirmBox(EKbAction Action);
    bool IsActive(EKbAction Action, EPhase InPhase) const;
    void Finish(const FText& Message, EKbAction FocusAction, EKbDevice FocusDevice);
    void FocusSlot(EKbAction Action, EKbDevice Device);
    FKbBindingStore& Store() const;
    FText NotAllowedMessage(const FKey& Key) const;

    FKbBindingStore* StorePtr = nullptr;
    EPhase Phase = EPhase::Idle;
    EKbAction ActiveAction = EKbAction::Count;
    EKbDevice ActiveDevice = EKbDevice::Keyboard;
    FKey PendingKey;
    EKbAction ConflictWith = EKbAction::Count;
    FText Status;

    TWeakPtr<SWidget> SlotButtons[KbActionCount][KbDeviceCount];
    TWeakPtr<SWidget> CaptureBoxes[KbActionCount];
    TWeakPtr<SWidget> ConfirmBoxes[KbActionCount];
};
