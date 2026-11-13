#include "UI/SPWControlsSection.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "UI/SPWKeyCapture.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldControls"

namespace
{
/** The groups in the order they are listed. */
const EKbGroup KbUiGroups[] = {EKbGroup::Movement, EKbGroup::Actions, EKbGroup::Camera, EKbGroup::Interface};

bool KbUiIsCancelKey(const FKey& Key)
{
    return Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right;
}

bool KbUiRebindable(EKbAction Action, EKbDevice Device)
{
    const FKbActionInfo& Info = KbInfo(Action);
    return Device == EKbDevice::Keyboard ? Info.bKeyboardRebindable : Info.bGamepadRebindable;
}
} // namespace

FKbBindingStore& SPWControlsSection::Store() const
{
    return StorePtr != nullptr ? *StorePtr : FKbBindingStore::Get();
}

void SPWControlsSection::Construct(const FArguments& InArgs)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    StorePtr = InArgs._Store;

    const TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
    List->AddSlot().AutoHeight()
    [
        PWUI::MakeText(LOCTEXT("Intro",
            "Choose a binding, then press the key or button you want. Escape cancels; on a controller the menu button cancels. "
            "Changes are saved and work at once."),
            15, Style.TextSecondary, false, true)
    ];
    const TSharedRef<STextBlock> StatusText =
        PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return Status; }), 16, Style.Accent, true, true);
    StatusText->SetVisibility(TAttribute<EVisibility>::CreateLambda(
        [this]() { return Status.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; }));
    List->AddSlot().AutoHeight().Padding(0.0f, PWUI::Spacing::Small, 0.0f, 0.0f)[StatusText];
    for (const EKbGroup Group : KbUiGroups)
    {
        List->AddSlot().AutoHeight().Padding(0.0f, PWUI::Spacing::Large, 0.0f, 0.0f)[MakeGroup(Group)];
    }
    List->AddSlot().AutoHeight().Padding(0.0f, PWUI::Spacing::XLarge, 0.0f, 0.0f)
    [
        SNew(SBox).HAlign(HAlign_Left).Tag(PWKbUi::ResetAllTag)
        [
            PWUI::MakeButton(LOCTEXT("ResetAll", "Reset all to defaults"), PWUI::EButtonKind::Secondary,
                             FOnClicked::CreateLambda([this]()
                             {
                                 ResetEverything();
                                 return FReply::Handled();
                             }),
                             true, 18)
        ]
    ];
    ChildSlot[List];
}

TSharedRef<SWidget> SPWControlsSection::MakeGroup(EKbGroup Group)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
    Box->AddSlot().AutoHeight()[PWUI::MakeText(KbGroupLabel(Group), 13, Style.Accent, true)];
    Box->AddSlot().AutoHeight().Padding(0.0f, PWUI::Spacing::XSmall, 0.0f, 0.0f)[PWUI::MakeRule()];
    for (int32 Index = 0; Index < KbActionCount; ++Index)
    {
        const EKbAction Action = static_cast<EKbAction>(Index);
        if (KbInfo(Action).Group == Group)
        {
            Box->AddSlot().AutoHeight()[MakeActionBlock(Action)];
        }
    }
    return Box;
}

TSharedRef<SWidget> SPWControlsSection::MakeActionBlock(EKbAction Action)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const bool bLocked = !KbUiRebindable(Action, EKbDevice::Keyboard) && !KbUiRebindable(Action, EKbDevice::Gamepad);

    const TSharedRef<SWidget> Capture =
        SNew(SBox).Tag(PWKbUi::CaptureTag)
        .Visibility_Lambda([this, Action]() { return IsActive(Action, EPhase::Capturing) ? EVisibility::Visible : EVisibility::Collapsed; })
        [
            SNew(SPWKeyCapture)
            .Prompt_Lambda([this]()
            {
                return ActiveDevice == EKbDevice::Keyboard
                    ? FText::Format(LOCTEXT("PressKeyboard", "Press a key for {0}."), KbActionLabel(ActiveAction))
                    : FText::Format(LOCTEXT("PressGamepad", "Press a controller button for {0}."), KbActionLabel(ActiveAction));
            })
            .Hint_Lambda([this]()
            {
                return ActiveDevice == EKbDevice::Keyboard
                    ? LOCTEXT("HintKeyboard", "Escape cancels. A mouse button or the wheel also counts when clicked or scrolled here.")
                    : LOCTEXT("HintGamepad", "The menu button cancels. Sticks cannot be bound.");
            })
            .OnCaptured(FPWOnKeyCaptured::CreateLambda([this](const FKey& Key) { SubmitKey(Key); }))
            .OnCancelled(FSimpleDelegate::CreateLambda([this]()
            {
                // Focus leaving a closing prompt calls this too, so only a prompt that is still open is cancelled.
                if (Phase == EPhase::Capturing)
                {
                    CancelPrompt();
                }
            }))
        ];
    CaptureBoxes[static_cast<int32>(Action)] = Capture;

    const TSharedRef<SVerticalBox> Block = SNew(SVerticalBox);
    Block->AddSlot().AutoHeight().Padding(0.0f, PWUI::Spacing::Compact, 0.0f, 0.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
        [
            PWUI::MakeText(KbActionLabel(Action), 18, Style.TextPrimary, true, true)
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(PWUI::Spacing::Small, 0.0f, 0.0f, 0.0f)
        [
            SNew(SBox).Tag(PWKbUi::ResetTag(Action))
            .Visibility(bLocked ? EVisibility::Collapsed : EVisibility::Visible)
            [
                PWUI::MakeButton(LOCTEXT("Reset", "Reset"), PWUI::EButtonKind::Secondary,
                                 FOnClicked::CreateLambda([this, Action]()
                                 {
                                     ResetAction(Action);
                                     return FReply::Handled();
                                 }),
                                 true, 16)
            ]
        ]
    ];
    Block->AddSlot().AutoHeight().Padding(0.0f, PWUI::Spacing::XSmall, 0.0f, 0.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f)[MakeBindingButton(Action, EKbDevice::Keyboard)]
        + SHorizontalBox::Slot().FillWidth(1.0f).Padding(PWUI::Spacing::Small, 0.0f, 0.0f, 0.0f)[MakeBindingButton(Action, EKbDevice::Gamepad)]
    ];
    if (bLocked)
    {
        Block->AddSlot().AutoHeight().Padding(0.0f, PWUI::Spacing::XSmall, 0.0f, 0.0f)
        [
            PWUI::MakeText(LOCTEXT("PauseLocked", "These always open the pause menu, so they cannot be changed or used for anything else."),
                           14, Style.TextMuted, false, true)
        ];
    }
    Block->AddSlot().AutoHeight().Padding(0.0f, PWUI::Spacing::Small, 0.0f, 0.0f)[Capture];
    Block->AddSlot().AutoHeight().Padding(0.0f, PWUI::Spacing::Small, 0.0f, 0.0f)[MakeConfirmBox(Action)];
    return Block;
}

TSharedRef<SWidget> SPWControlsSection::MakeBindingButton(EKbAction Action, EKbDevice Device)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SWidget> Content =
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            PWUI::MakeText(KbDeviceLabel(Device), 12, Style.TextMuted, true, true)
        ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(STextBlock)
            .Text_Lambda([this, Action, Device]()
            {
                if (IsActive(Action, EPhase::Capturing) && ActiveDevice == Device)
                {
                    return LOCTEXT("Waiting", "Press a key...");
                }
                return KbKeyLabel(Store().Current().Get(Action, Device));
            })
            .Font(PWUI::Font(20, PWUI::EFontWeight::Medium))
            .ColorAndOpacity(FSlateColor::UseForeground())
            .TextShapingMethod(ETextShapingMethod::FullShaping)
            .AutoWrapText(true)
        ];
    const TSharedRef<SWidget> Button = PWUI::MakeContentButton(
        Content, PWUI::EButtonKind::Secondary,
        FOnClicked::CreateLambda([this, Action, Device]()
        {
            BeginCapture(Action, Device);
            return FReply::Handled();
        }),
        false, KbUiRebindable(Action, Device), FMargin(PWUI::Spacing::Compact, PWUI::Spacing::Small));
    const TSharedRef<SWidget> Tagged = SNew(SBox).Tag(PWKbUi::SlotTag(Action, Device))[Button];
    SlotButtons[static_cast<int32>(Action)][static_cast<int32>(Device)] = Button;
    return Tagged;
}

TSharedRef<SWidget> SPWControlsSection::MakeConfirmBox(EKbAction Action)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SWidget> Box =
        SNew(SBox).Tag(PWKbUi::ConfirmTag)
        .Visibility_Lambda([this, Action]() { return IsActive(Action, EPhase::Confirming) ? EVisibility::Visible : EVisibility::Collapsed; })
        [
            SNew(SBorder).BorderImage(&Style.BrushInset).Padding(PWUI::Spacing::Medium)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    PWUI::MakeText(TAttribute<FText>::CreateLambda([this]()
                    {
                        const FKbBindingSet& Bindings = Store().Current();
                        const FKey Freed = Bindings.Get(ActiveAction, ActiveDevice);
                        if (!Freed.IsValid())
                        {
                            return FText::Format(LOCTEXT("ConflictFreed", "{0} is already used by {1}. Swap them? {1} would be left unbound."),
                                                 KbKeyLabel(PendingKey), KbActionLabel(ConflictWith));
                        }
                        return FText::Format(LOCTEXT("ConflictSwap", "{0} is already used by {1}. Swap them? {1} would use {2}."),
                                             KbKeyLabel(PendingKey), KbActionLabel(ConflictWith), KbKeyLabel(Freed));
                    }), 18, Style.Accent, true, true)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, PWUI::Spacing::Compact, 0.0f, 0.0f)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        PWUI::MakeButton(LOCTEXT("Swap", "Swap"), PWUI::EButtonKind::Primary,
                                         FOnClicked::CreateLambda([this]()
                                         {
                                             ConfirmSwap();
                                             return FReply::Handled();
                                         }),
                                         true, 18, 110.0f)
                    ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(PWUI::Spacing::Small, 0.0f, 0.0f, 0.0f)
                    [
                        PWUI::MakeButton(LOCTEXT("KeepOld", "Cancel"), PWUI::EButtonKind::Secondary,
                                         FOnClicked::CreateLambda([this]()
                                         {
                                             CancelPrompt();
                                             return FReply::Handled();
                                         }),
                                         true, 18, 110.0f)
                    ]
                ]
            ]
        ];
    ConfirmBoxes[static_cast<int32>(Action)] = Box;
    return Box;
}

bool SPWControlsSection::IsActive(EKbAction Action, EPhase InPhase) const
{
    return Phase == InPhase && ActiveAction == Action;
}

void SPWControlsSection::FocusSlot(EKbAction Action, EKbDevice Device)
{
    if (!KbUiRebindable(Action, Device))
    {
        return;
    }
    PWUI::FocusWhenReady(AsShared(), SlotButtons[static_cast<int32>(Action)][static_cast<int32>(Device)]);
}

void SPWControlsSection::Finish(const FText& Message, EKbAction FocusAction, EKbDevice FocusDevice)
{
    Phase = EPhase::Idle;
    ConflictWith = EKbAction::Count;
    PendingKey = FKey();
    Status = Message;
    FocusSlot(FocusAction, FocusDevice);
}

void SPWControlsSection::BeginCapture(EKbAction Action, EKbDevice Device)
{
    if (!KbUiRebindable(Action, Device))
    {
        return;
    }
    Phase = EPhase::Capturing;
    ActiveAction = Action;
    ActiveDevice = Device;
    ConflictWith = EKbAction::Count;
    PendingKey = FKey();
    Status = FText::GetEmpty();
    PWUI::FocusWhenReady(AsShared(), CaptureBoxes[static_cast<int32>(Action)]);
}

FText SPWControlsSection::NotAllowedMessage(const FKey& Key) const
{
    if (ActiveDevice == EKbDevice::Keyboard && Key.IsGamepadKey())
    {
        return LOCTEXT("WrongPad", "That is a controller button. This binding is for the keyboard and mouse; press a key or mouse button.");
    }
    if (ActiveDevice == EKbDevice::Gamepad && !Key.IsGamepadKey())
    {
        return LOCTEXT("WrongKeyboard", "That is not a controller button. This binding is for the controller; press one of its buttons.");
    }
    return LOCTEXT("NotAllowed", "That key cannot be used for this. Try another.");
}

void SPWControlsSection::SubmitKey(const FKey& Key)
{
    if (Phase != EPhase::Capturing)
    {
        return;
    }
    EKbAction Other = EKbAction::Count;
    const EKbSetResult Result = Store().Rebind(ActiveAction, ActiveDevice, Key, false, &Other);
    const FText Name = KbActionLabel(ActiveAction);
    switch (Result)
    {
    case EKbSetResult::Applied:
    case EKbSetResult::Swapped:
        Finish(FText::Format(LOCTEXT("NowUses", "{0} now uses {1}."), Name, KbKeyLabel(Key)), ActiveAction, ActiveDevice);
        break;
    case EKbSetResult::Unchanged:
        Finish(FText::Format(LOCTEXT("StillUses", "{0} still uses {1}."), Name, KbKeyLabel(Key)), ActiveAction, ActiveDevice);
        break;
    case EKbSetResult::Conflict:
        Phase = EPhase::Confirming;
        PendingKey = Key;
        ConflictWith = Other;
        PWUI::FocusWhenReady(AsShared(), ConfirmBoxes[static_cast<int32>(ActiveAction)]);
        break;
    case EKbSetResult::Reserved:
        Status = LOCTEXT("Reserved", "Escape and the menu button open the pause menu, so nothing else can use them.");
        break;
    default:
        Status = NotAllowedMessage(Key);
        break;
    }
}

void SPWControlsSection::ConfirmSwap()
{
    if (Phase != EPhase::Confirming)
    {
        return;
    }
    const FText Name = KbActionLabel(ActiveAction);
    const FText OtherName = KbActionLabel(ConflictWith);
    const FKey Key = PendingKey;
    const EKbSetResult Result = Store().Rebind(ActiveAction, ActiveDevice, Key, true);
    if (Result == EKbSetResult::Swapped || Result == EKbSetResult::Applied)
    {
        const FKey Other = Store().Current().Get(ConflictWith, ActiveDevice);
        Finish(Other.IsValid() ? FText::Format(LOCTEXT("Swapped", "{0} now uses {1}. {2} now uses {3}."), Name, KbKeyLabel(Key), OtherName, KbKeyLabel(Other))
                               : FText::Format(LOCTEXT("SwappedFree", "{0} now uses {1}. {2} is unbound."), Name, KbKeyLabel(Key), OtherName),
               ActiveAction, ActiveDevice);
        return;
    }
    Finish(FText::Format(LOCTEXT("SwapRefused", "{0} could not take {1}; nothing changed."), Name, KbKeyLabel(Key)), ActiveAction, ActiveDevice);
}

void SPWControlsSection::CancelPrompt()
{
    if (Phase == EPhase::Idle)
    {
        return;
    }
    const EKbAction Action = ActiveAction;
    const EKbDevice Device = ActiveDevice;
    Finish(FText::GetEmpty(), Action, Device);
}

void SPWControlsSection::ResetAction(EKbAction Action)
{
    if (!KbUiRebindable(Action, EKbDevice::Keyboard) && !KbUiRebindable(Action, EKbDevice::Gamepad))
    {
        return;
    }
    const FText Name = KbActionLabel(Action);
    const bool bWasDefault = Store().Current().IsDefault(Action);
    if (!bWasDefault)
    {
        Store().ResetAction(Action);
    }
    Finish(bWasDefault ? FText::Format(LOCTEXT("AlreadyDefault", "{0} already uses its default keys."), Name)
                       : FText::Format(LOCTEXT("WasReset", "{0} is back to its default keys."), Name),
           Action, KbUiRebindable(Action, EKbDevice::Keyboard) ? EKbDevice::Keyboard : EKbDevice::Gamepad);
}

void SPWControlsSection::ResetEverything()
{
    const bool bWasDefault = Store().Current().IsEveryDefault();
    if (!bWasDefault)
    {
        Store().ResetAll();
    }
    Phase = EPhase::Idle;
    ConflictWith = EKbAction::Count;
    PendingKey = FKey();
    Status = bWasDefault ? LOCTEXT("AllAlready", "Every control already uses its default.") : LOCTEXT("AllReset", "All controls are back to their defaults.");
}

FReply SPWControlsSection::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    // While a prompt is open Escape (or B, or the menu button) closes it instead of leaving Settings.
    if (Phase != EPhase::Idle && KbUiIsCancelKey(InKeyEvent.GetKey()))
    {
        if (!InKeyEvent.IsRepeat())
        {
            CancelPrompt();
        }
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
