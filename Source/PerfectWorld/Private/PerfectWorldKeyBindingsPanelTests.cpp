// The Controls tab of Settings (order F4): the tabs, the list of every action with its bindings, the capture flow (a key is bound, Escape
// and the menu button cancel, a conflict asks before swapping, a key for the wrong device or a reserved key is refused), Reset and Reset all,
// the locked rows, the keys, and the layout at every window size and UI scale. Run in the editor under "PerfectWorld.KeyBindingsPanel".
// The widgets are the real ones, built without a window: SlatePrepass evaluates bound text and visibility, buttons are clicked with
// SButton::SimulateClick, and keys are delivered straight to the widgets that handle them. The store is a private one on a temporary file.
// What needs a real window (focus moving to the prompt and back, a real mouse click) is for a play session.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "HAL/FileManager.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "PerfectWorldKeyBindings.h"
#include "UI/SPWControlsSection.h"
#include "UI/SPWKeyCapture.h"
#include "UI/SPWSettingsPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
struct FKbpTempFile
{
    FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("KbPanelTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
    FString Path = FPaths::Combine(Dir, TEXT("KeyBindings.txt"));
    ~FKbpTempFile() { IFileManager::Get().DeleteDirectory(*Dir, false, true); }
};

/** Visits every reachable widget, telling it whether it and everything above it is shown. */
void KbpWalk(const TSharedRef<SWidget>& Widget, const bool bShownAbove, const TFunctionRef<void(const TSharedRef<SWidget>&, bool)>& Visit)
{
    const bool bShown = bShownAbove && Widget->GetVisibility().IsVisible();
    Visit(Widget, bShown);
    FChildren* Children = Widget->GetChildren();
    for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
    {
        KbpWalk(Children->GetChildAt(Index), bShown, Visit);
    }
}

TArray<FString> KbpTexts(const TSharedRef<SWidget>& Root)
{
    Root->SlatePrepass(1.0f);
    TArray<FString> Texts;
    KbpWalk(Root, true, [&Texts](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (bShown && Widget->GetTypeAsString() == TEXT("STextBlock"))
        {
            Texts.Add(StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString());
        }
    });
    return Texts;
}

bool KbpHasText(const TSharedRef<SWidget>& Root, const FString& Exact) { return KbpTexts(Root).Contains(Exact); }

bool KbpHasTextPart(const TSharedRef<SWidget>& Root, const FString& Part)
{
    for (const FString& Text : KbpTexts(Root))
    {
        if (Text.Contains(Part)) return true;
    }
    return false;
}

/** The first shown widget with this tag, or null. */
TSharedPtr<SWidget> KbpFindTag(const TSharedRef<SWidget>& Root, const FName Tag)
{
    Root->SlatePrepass(1.0f);
    TSharedPtr<SWidget> Found;
    KbpWalk(Root, true, [&Found, Tag](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (!Found.IsValid() && bShown && Widget->GetTag() == Tag) Found = Widget;
    });
    return Found;
}

int32 KbpCountTag(const TSharedRef<SWidget>& Root, const FName Tag)
{
    Root->SlatePrepass(1.0f);
    int32 Count = 0;
    KbpWalk(Root, true, [&Count, Tag](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (bShown && Widget->GetTag() == Tag) ++Count;
    });
    return Count;
}

TSharedPtr<SButton> KbpButtonUnder(const TSharedRef<SWidget>& Root)
{
    TSharedPtr<SButton> Found;
    KbpWalk(Root, true, [&Found](const TSharedRef<SWidget>& Widget, bool)
    {
        if (!Found.IsValid() && Widget->GetTypeAsString() == TEXT("SButton")) Found = StaticCastSharedRef<SButton>(Widget);
    });
    return Found;
}

/** Clicks the button inside the first shown widget with this tag. */
bool KbpClickTag(const TSharedRef<SWidget>& Root, const FName Tag)
{
    const TSharedPtr<SWidget> Holder = KbpFindTag(Root, Tag);
    if (!Holder.IsValid()) return false;
    const TSharedPtr<SButton> Button = KbpButtonUnder(Holder.ToSharedRef());
    if (!Button.IsValid() || !Button->IsEnabled()) return false;
    Button->SimulateClick();
    Root->SlatePrepass(1.0f);
    return true;
}

/** Clicks the first shown button whose text is exactly this. */
bool KbpClickLabel(const TSharedRef<SWidget>& Root, const FString& Label)
{
    Root->SlatePrepass(1.0f);
    TSharedPtr<SButton> Target;
    KbpWalk(Root, true, [&Target, &Label](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (!Target.IsValid() && bShown && Widget->GetTypeAsString() == TEXT("SButton") && KbpTexts(Widget).Contains(Label))
        {
            Target = StaticCastSharedRef<SButton>(Widget);
        }
    });
    if (!Target.IsValid()) return false;
    Target->SimulateClick();
    Root->SlatePrepass(1.0f);
    return true;
}

TSharedPtr<SPWKeyCapture> KbpShownCapture(const TSharedRef<SWidget>& Root)
{
    Root->SlatePrepass(1.0f);
    TSharedPtr<SPWKeyCapture> Found;
    KbpWalk(Root, true, [&Found](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (!Found.IsValid() && bShown && Widget->GetTypeAsString() == TEXT("SPWKeyCapture")) Found = StaticCastSharedRef<SPWKeyCapture>(Widget);
    });
    return Found;
}

TSharedPtr<SPWControlsSection> KbpSection(const TSharedRef<SWidget>& Root)
{
    TSharedPtr<SPWControlsSection> Found;
    KbpWalk(Root, true, [&Found](const TSharedRef<SWidget>& Widget, bool)
    {
        if (!Found.IsValid() && Widget->GetTypeAsString() == TEXT("SPWControlsSection")) Found = StaticCastSharedRef<SPWControlsSection>(Widget);
    });
    return Found;
}

FKeyEvent KbpKey(const FKey Key, const bool bRepeat = false)
{
    return FKeyEvent(Key, FModifierKeysState(), 0, bRepeat, 0, 0);
}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKbpTabsTest, "PerfectWorld.KeyBindingsPanel.TabsAndTheListOfControls",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FKbpTabsTest::RunTest(const FString& Parameters)
{
    FKbpTempFile Temp;
    FKbBindingStore Store(Temp.Path);
    int32 Backs = 0;
    const TSharedRef<SPWSettingsPanel> Panel = SNew(SPWSettingsPanel)
        .BindingStore(&Store)
        .OnBack(FSimpleDelegate::CreateLambda([&Backs]() { ++Backs; }));

    TestEqual(TEXT("Settings opens on General"), Panel->GetTab(), 0);
    TestTrue(TEXT("General shows the display and mouse settings"), KbpHasText(Panel, TEXT("DISPLAY")) && KbpHasText(Panel, TEXT("MOUSE SENSITIVITY")));
    TestEqual(TEXT("the General page is shown"), KbpCountTag(Panel, PWSettingsUi::GeneralPageTag), 1);
    TestEqual(TEXT("the Controls page is not"), KbpCountTag(Panel, PWSettingsUi::ControlsPageTag), 0);
    TestFalse(TEXT("so no controls are listed"), KbpHasText(Panel, TEXT("Reset all to defaults")));
    TestEqual(TEXT("both tabs are offered"), KbpCountTag(Panel, PWSettingsUi::GeneralTabTag) + KbpCountTag(Panel, PWSettingsUi::ControlsTabTag), 2);

    if (!TestTrue(TEXT("the Controls tab can be clicked"), KbpClickTag(Panel, PWSettingsUi::ControlsTabTag))) return false;
    TestEqual(TEXT("Controls is open"), Panel->GetTab(), 1);
    TestEqual(TEXT("the Controls page is shown"), KbpCountTag(Panel, PWSettingsUi::ControlsPageTag), 1);
    TestFalse(TEXT("the General page is not"), KbpHasText(Panel, TEXT("DISPLAY")));
    TestTrue(TEXT("Reset all is offered"), KbpHasText(Panel, TEXT("Reset all to defaults")));
    TestTrue(TEXT("Back stays"), KbpHasText(Panel, TEXT("Back")));

    // Every action is listed, with a binding button for each slot, showing today's key.
    const FKbBindingSet Defaults;
    for (int32 Index = 0; Index < KbActionCount; ++Index)
    {
        const EKbAction Action = static_cast<EKbAction>(Index);
        const FString Name = KbInfo(Action).Id;
        TestTrue(*(Name + TEXT(": the action is named")), KbpHasText(Panel, KbActionLabel(Action).ToString()));
        for (const EKbDevice Device : {EKbDevice::Keyboard, EKbDevice::Gamepad})
        {
            TestEqual(*(Name + TEXT(": one binding button per slot")), KbpCountTag(Panel, PWKbUi::SlotTag(Action, Device)), 1);
            TestTrue(*(Name + TEXT(": the binding shows its key")), KbpHasText(Panel, KbKeyLabel(Defaults.Get(Action, Device)).ToString()));
        }
    }
    TestTrue(TEXT("Map and Quests are listed"), KbpHasText(Panel, TEXT("Map")) && KbpHasText(Panel, TEXT("Quests")));
    TestEqual(TEXT("no prompt is open"), KbpCountTag(Panel, PWKbUi::CaptureTag) + KbpCountTag(Panel, PWKbUi::ConfirmTag), 0);

    // The shoulder buttons step between the tabs; Escape and B leave.
    Panel->OnKeyDown(FGeometry(), KbpKey(EKeys::Gamepad_LeftShoulder));
    TestEqual(TEXT("LB goes to General"), Panel->GetTab(), 0);
    Panel->OnKeyDown(FGeometry(), KbpKey(EKeys::Gamepad_RightShoulder));
    TestEqual(TEXT("RB goes to Controls"), Panel->GetTab(), 1);
    Panel->OnKeyDown(FGeometry(), KbpKey(EKeys::Escape));
    TestEqual(TEXT("Escape leaves"), Backs, 1);
    Panel->OnKeyDown(FGeometry(), KbpKey(EKeys::Gamepad_FaceButton_Right));
    TestEqual(TEXT("B leaves"), Backs, 2);
    TestTrue(TEXT("the General tab can be clicked back"), KbpClickTag(Panel, PWSettingsUi::GeneralTabTag));
    TestEqual(TEXT("and General is open"), Panel->GetTab(), 0);

    // A prompt left open is closed by changing tab.
    const TSharedRef<SPWSettingsPanel> Opened = SNew(SPWSettingsPanel).InitialTab(1).BindingStore(&Store);
    const TSharedPtr<SPWControlsSection> Section = KbpSection(Opened);
    if (!TestTrue(TEXT("the Controls section exists"), Section.IsValid())) return false;
    Section->BeginCapture(EKbAction::Jump, EKbDevice::Keyboard);
    TestEqual(TEXT("a prompt is open"), static_cast<int32>(Section->GetPhase()), static_cast<int32>(SPWControlsSection::EPhase::Capturing));
    Opened->SetTab(0);
    TestEqual(TEXT("leaving the tab closes it"), static_cast<int32>(Section->GetPhase()), static_cast<int32>(SPWControlsSection::EPhase::Idle));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKbpCaptureTest, "PerfectWorld.KeyBindingsPanel.CaptureConflictAndCancelFlow",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FKbpCaptureTest::RunTest(const FString& Parameters)
{
    using EPhase = SPWControlsSection::EPhase;
    FKbpTempFile Temp;
    FKbBindingStore Store(Temp.Path);
    const TSharedRef<SPWSettingsPanel> Panel = SNew(SPWSettingsPanel).InitialTab(1).BindingStore(&Store);
    const TSharedPtr<SPWControlsSection> Section = KbpSection(Panel);
    if (!TestTrue(TEXT("the Controls section exists"), Section.IsValid())) return false;
    const auto Phase = [&Section]() { return Section->GetPhase(); };
    const auto Key = [&Store](EKbAction Action, EKbDevice Device) { return Store.Current().Get(Action, Device).ToString(); };

    // Select a binding, press a key.
    if (!TestTrue(TEXT("Jump's keyboard binding can be clicked"), KbpClickTag(Panel, PWKbUi::SlotTag(EKbAction::Jump, EKbDevice::Keyboard)))) return false;
    TestTrue(TEXT("it waits for a key"), Phase() == EPhase::Capturing && Section->GetActiveAction() == EKbAction::Jump && Section->GetActiveDevice() == EKbDevice::Keyboard);
    TestEqual(TEXT("one prompt is shown"), KbpCountTag(Panel, PWKbUi::CaptureTag), 1);
    TestTrue(TEXT("it names the action"), KbpHasTextPart(Panel, TEXT("Press a key for Jump")));
    TestTrue(TEXT("and says how to cancel"), KbpHasTextPart(Panel, TEXT("Escape cancels")));
    TestTrue(TEXT("the binding itself says it is waiting"), KbpHasText(Panel, TEXT("Press a key...")));
    TSharedPtr<SPWKeyCapture> Capture = KbpShownCapture(Panel);
    if (!TestTrue(TEXT("the prompt is the key capture"), Capture.IsValid())) return false;
    TestTrue(TEXT("a held key is not a choice"), Capture->OnKeyDown(FGeometry(), KbpKey(EKeys::Y, true)).IsEventHandled() && Phase() == EPhase::Capturing);
    TestTrue(TEXT("the capture takes the key"), Capture->OnKeyDown(FGeometry(), KbpKey(EKeys::Y)).IsEventHandled());
    TestEqual(TEXT("Jump is Y"), Key(EKbAction::Jump, EKbDevice::Keyboard), FString(TEXT("Y")));
    TestTrue(TEXT("the prompt closes"), Phase() == EPhase::Idle && KbpCountTag(Panel, PWKbUi::CaptureTag) == 0);
    TestTrue(TEXT("and says what happened"), KbpHasTextPart(Panel, TEXT("Jump now uses Y")));
    TestTrue(TEXT("the binding shows the new key"), KbpHasText(Panel, TEXT("Y")));
    TestTrue(TEXT("it was saved"), IFileManager::Get().FileExists(*Temp.Path));

    // Escape cancels and changes nothing; so does the menu button, and so does focus leaving.
    Section->BeginCapture(EKbAction::Jump, EKbDevice::Keyboard);
    Capture = KbpShownCapture(Panel);
    Capture->OnKeyDown(FGeometry(), KbpKey(EKeys::Escape));
    TestTrue(TEXT("Escape closes the prompt"), Phase() == EPhase::Idle);
    TestEqual(TEXT("and leaves Jump as it was"), Key(EKbAction::Jump, EKbDevice::Keyboard), FString(TEXT("Y")));
    Section->BeginCapture(EKbAction::Jump, EKbDevice::Gamepad);
    KbpShownCapture(Panel)->OnKeyDown(FGeometry(), KbpKey(EKeys::Gamepad_Special_Right));
    TestTrue(TEXT("the menu button closes a gamepad prompt"), Phase() == EPhase::Idle);
    TestEqual(TEXT("the pad key is untouched"), Key(EKbAction::Jump, EKbDevice::Gamepad), FString(TEXT("Gamepad_FaceButton_Bottom")));
    Section->BeginCapture(EKbAction::Run, EKbDevice::Keyboard);
    KbpShownCapture(Panel)->OnFocusLost(FFocusEvent());
    TestTrue(TEXT("losing focus closes a prompt"), Phase() == EPhase::Idle);
    Section->BeginCapture(EKbAction::Run, EKbDevice::Keyboard);
    TestTrue(TEXT("Escape reaching the section closes the prompt and is handled"), Section->OnKeyDown(FGeometry(), KbpKey(EKeys::Escape)).IsEventHandled() && Phase() == EPhase::Idle);
    TestFalse(TEXT("with no prompt open, Escape is left for the Settings screen"), Section->OnKeyDown(FGeometry(), KbpKey(EKeys::Escape)).IsEventHandled());
    Section->BeginCapture(EKbAction::Run, EKbDevice::Keyboard);
    Section->CancelPrompt();
    Section->CancelPrompt();
    TestTrue(TEXT("cancelling twice is harmless"), Phase() == EPhase::Idle);

    // Refusals leave the prompt open for another try.
    Section->BeginCapture(EKbAction::Run, EKbDevice::Keyboard);
    Section->SubmitKey(EKeys::Gamepad_FaceButton_Top);
    TestTrue(TEXT("a controller button for a keyboard binding is refused"), Phase() == EPhase::Capturing && KbpHasTextPart(Panel, TEXT("controller button")));
    Section->SubmitKey(EKeys::MouseScrollUp);
    TestTrue(TEXT("the wheel is not for running"), Phase() == EPhase::Capturing);
    Section->SubmitKey(EKeys::LeftMouseButton);
    TestTrue(TEXT("the left mouse button is not either"), Phase() == EPhase::Capturing);
    Section->SubmitKey(EKeys::Escape);
    TestTrue(TEXT("a reserved key is refused with a reason"), Phase() == EPhase::Capturing && KbpHasTextPart(Panel, TEXT("pause menu")));
    Section->SubmitKey(EKeys::MiddleMouseButton);
    TestEqual(TEXT("a mouse button works"), Key(EKbAction::Run, EKbDevice::Keyboard), FString(TEXT("MiddleMouseButton")));
    Section->BeginCapture(EKbAction::ZoomIn, EKbDevice::Keyboard);
    Section->SubmitKey(EKeys::MouseScrollDown);
    TestTrue(TEXT("the wheel works for zoom, by swapping with the other direction"), Phase() == EPhase::Confirming);
    Section->ConfirmSwap();
    TestTrue(TEXT("zoom in is the wheel down now"), Key(EKbAction::ZoomIn, EKbDevice::Keyboard) == TEXT("MouseScrollDown") && Key(EKbAction::ZoomOut, EKbDevice::Keyboard) == TEXT("MouseScrollUp"));
    Section->BeginCapture(EKbAction::Interact, EKbDevice::Gamepad);
    Section->SubmitKey(EKeys::E);
    TestTrue(TEXT("a keyboard key for a controller binding is refused"), Phase() == EPhase::Capturing && KbpHasTextPart(Panel, TEXT("not a controller button")));
    Section->SubmitKey(EKeys::Gamepad_LeftTrigger);
    TestEqual(TEXT("a trigger works"), Key(EKbAction::Interact, EKbDevice::Gamepad), FString(TEXT("Gamepad_LeftTrigger")));

    // A conflict asks first.
    Section->BeginCapture(EKbAction::Jump, EKbDevice::Keyboard);
    KbpShownCapture(Panel)->OnKeyDown(FGeometry(), KbpKey(EKeys::E));
    TestTrue(TEXT("a held key is a conflict"), Phase() == EPhase::Confirming);
    TestEqual(TEXT("the confirmation is shown"), KbpCountTag(Panel, PWKbUi::ConfirmTag), 1);
    TestEqual(TEXT("in place of the prompt"), KbpCountTag(Panel, PWKbUi::CaptureTag), 0);
    TestTrue(TEXT("it names the key and the other action"), KbpHasTextPart(Panel, TEXT("E is already used by Interact")));
    TestTrue(TEXT("and what the other would get"), KbpHasTextPart(Panel, TEXT("Interact would use Y")));
    TestEqual(TEXT("nothing has changed yet"), Key(EKbAction::Jump, EKbDevice::Keyboard), FString(TEXT("Y")));
    TestEqual(TEXT("Interact still has E"), Key(EKbAction::Interact, EKbDevice::Keyboard), FString(TEXT("E")));
    TestTrue(TEXT("Escape on the confirmation cancels it"), Section->OnKeyDown(FGeometry(), KbpKey(EKeys::Escape)).IsEventHandled() && Phase() == EPhase::Idle);
    TestEqual(TEXT("still nothing changed"), Key(EKbAction::Jump, EKbDevice::Keyboard), FString(TEXT("Y")));

    Section->BeginCapture(EKbAction::Jump, EKbDevice::Keyboard);
    Section->SubmitKey(EKeys::E);
    if (!TestTrue(TEXT("Cancel on the confirmation"), KbpClickLabel(Panel, TEXT("Cancel")))) return false;
    TestTrue(TEXT("closes it"), Phase() == EPhase::Idle && KbpCountTag(Panel, PWKbUi::ConfirmTag) == 0);
    TestEqual(TEXT("and changes nothing"), Key(EKbAction::Jump, EKbDevice::Keyboard), FString(TEXT("Y")));

    Section->BeginCapture(EKbAction::Jump, EKbDevice::Keyboard);
    Section->SubmitKey(EKeys::E);
    if (!TestTrue(TEXT("Swap on the confirmation"), KbpClickLabel(Panel, TEXT("Swap")))) return false;
    TestTrue(TEXT("closes it"), Phase() == EPhase::Idle);
    TestEqual(TEXT("Jump has E"), Key(EKbAction::Jump, EKbDevice::Keyboard), FString(TEXT("E")));
    TestEqual(TEXT("Interact has Jump's old key"), Key(EKbAction::Interact, EKbDevice::Keyboard), FString(TEXT("Y")));
    TestTrue(TEXT("the result is said"), KbpHasTextPart(Panel, TEXT("Jump now uses E")) && KbpHasTextPart(Panel, TEXT("Interact now uses Y")));

    // Pressing the key a slot already has is not a conflict with itself.
    Section->BeginCapture(EKbAction::Jump, EKbDevice::Keyboard);
    Section->SubmitKey(EKeys::E);
    TestTrue(TEXT("the same key is accepted as it is"), Phase() == EPhase::Idle && KbpHasTextPart(Panel, TEXT("still uses")));

    // Locked rows and fixed slots do not start a prompt.
    Section->BeginCapture(EKbAction::Pause, EKbDevice::Keyboard);
    Section->BeginCapture(EKbAction::Pause, EKbDevice::Gamepad);
    Section->BeginCapture(EKbAction::MoveForward, EKbDevice::Gamepad);
    TestTrue(TEXT("Pause and the move stick cannot be chosen"), Phase() == EPhase::Idle);
    TestTrue(TEXT("Pause says why"), KbpHasTextPart(Panel, TEXT("always open the pause menu")));
    TestEqual(TEXT("Pause has no Reset"), KbpCountTag(Panel, PWKbUi::ResetTag(EKbAction::Pause)), 0);
    const TSharedPtr<SWidget> PauseSlot = KbpFindTag(Panel, PWKbUi::SlotTag(EKbAction::Pause, EKbDevice::Keyboard));
    const TSharedPtr<SButton> PauseButton = PauseSlot.IsValid() ? KbpButtonUnder(PauseSlot.ToSharedRef()) : nullptr;
    TestTrue(TEXT("its binding button is disabled"), PauseButton.IsValid() && !PauseButton->IsEnabled());
    TestTrue(TEXT("the move stick is shown as fixed"), KbpHasText(Panel, TEXT("Left stick")));
    TestEqual(TEXT("Pause keeps Escape"), Key(EKbAction::Pause, EKbDevice::Keyboard), FString(TEXT("Escape")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKbpResetTest, "PerfectWorld.KeyBindingsPanel.ResetPerActionAndAll",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FKbpResetTest::RunTest(const FString& Parameters)
{
    FKbpTempFile Temp;
    FKbBindingStore Store(Temp.Path);
    const TSharedRef<SPWSettingsPanel> Panel = SNew(SPWSettingsPanel).InitialTab(1).BindingStore(&Store);
    const TSharedPtr<SPWControlsSection> Section = KbpSection(Panel);
    if (!TestTrue(TEXT("the Controls section exists"), Section.IsValid())) return false;

    Store.Rebind(EKbAction::Jump, EKbDevice::Keyboard, EKeys::Y, false);
    Store.Rebind(EKbAction::Jump, EKbDevice::Gamepad, EKeys::Gamepad_LeftShoulder, false);
    Store.Rebind(EKbAction::Run, EKbDevice::Keyboard, EKeys::R, false);
    TestTrue(TEXT("the list shows the new key at once"), KbpHasText(Panel, TEXT("Y")) && KbpHasText(Panel, TEXT("LB / L1")));

    if (!TestTrue(TEXT("Jump's Reset can be clicked"), KbpClickTag(Panel, PWKbUi::ResetTag(EKbAction::Jump)))) return false;
    TestTrue(TEXT("both of Jump's slots are back"), Store.Current().IsDefault(EKbAction::Jump));
    TestFalse(TEXT("Run is not"), Store.Current().IsDefault(EKbAction::Run));
    TestTrue(TEXT("the result is said"), KbpHasTextPart(Panel, TEXT("Jump is back to its default keys")));
    TestTrue(TEXT("the list shows the key again"), KbpHasText(Panel, KbKeyLabel(EKeys::SpaceBar).ToString()));
    KbpClickTag(Panel, PWKbUi::ResetTag(EKbAction::Jump));
    TestTrue(TEXT("resetting what is already default says so"), KbpHasTextPart(Panel, TEXT("Jump already uses its default keys")));

    // A reset that finds its key taken swaps it back, so nothing is ever bound twice.
    Store.Rebind(EKbAction::Jump, EKbDevice::Keyboard, EKeys::Y, false);
    Store.Rebind(EKbAction::Interact, EKbDevice::Keyboard, EKeys::SpaceBar, false);
    KbpClickTag(Panel, PWKbUi::ResetTag(EKbAction::Jump));
    TestEqual(TEXT("Jump is Space again"), Store.Current().Get(EKbAction::Jump, EKbDevice::Keyboard).ToString(), FString(TEXT("SpaceBar")));
    TestEqual(TEXT("and Interact took Y, not a duplicate"), Store.Current().Get(EKbAction::Interact, EKbDevice::Keyboard).ToString(), FString(TEXT("Y")));
    KbpClickTag(Panel, PWKbUi::ResetTag(EKbAction::Interact));
    TestTrue(TEXT("Interact reset puts it right"), Store.Current().IsDefault(EKbAction::Interact) && Store.Current().IsDefault(EKbAction::Jump));

    // An open prompt is closed by a reset.
    Section->BeginCapture(EKbAction::Run, EKbDevice::Keyboard);
    TestTrue(TEXT("Reset all can be clicked"), KbpClickTag(Panel, PWKbUi::ResetAllTag));
    TestTrue(TEXT("every control is the default"), Store.Current().IsEveryDefault());
    TestTrue(TEXT("the prompt is gone"), Section->GetPhase() == SPWControlsSection::EPhase::Idle && KbpCountTag(Panel, PWKbUi::CaptureTag) == 0);
    TestFalse(TEXT("and no file remains"), IFileManager::Get().FileExists(*Temp.Path));
    TestTrue(TEXT("the result is said"), KbpHasTextPart(Panel, TEXT("All controls are back to their defaults")));
    KbpClickTag(Panel, PWKbUi::ResetAllTag);
    TestTrue(TEXT("a second Reset all says there is nothing to do"), KbpHasTextPart(Panel, TEXT("already uses its default")));
    return true;
}

namespace
{
/** How far past Limit any shown widget under Widget is laid out, by arranging the widget tree in the given space. */
float KbpOverflow(const TSharedRef<SWidget>& Widget, const FGeometry& Geometry, const float Limit)
{
    float Worst = static_cast<float>(Geometry.GetAbsolutePosition().X) + static_cast<float>(Geometry.GetAbsoluteSize().X) - Limit;
    FArrangedChildren Arranged(EVisibility::All);
    Widget->ArrangeChildren(Geometry, Arranged);
    for (int32 Index = 0; Index < Arranged.Num(); ++Index)
    {
        const FArrangedWidget& Child = Arranged[Index];
        Worst = FMath::Max(Worst, KbpOverflow(Child.Widget, Child.Geometry, Limit));
    }
    return Worst;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKbpFitTest, "PerfectWorld.KeyBindingsPanel.FitsEveryWindowAndScale",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FKbpFitTest::RunTest(const FString& Parameters)
{
    // Logical sizes: 853x480 and 1280x720 are 1280x720 and 1920x1080 windows at 150% UI scale, and the 569x320 is an 853x480 window at 150%.
    // The settings panel is the parent's width less 48, at most 760, with 36 of padding each side.
    const FVector2D Sizes[] = {FVector2D(569.0, 320.0), FVector2D(853.0, 480.0), FVector2D(1280.0, 720.0), FVector2D(1920.0, 1080.0)};
    FKbpTempFile Temp;
    for (const FVector2D& Size : Sizes)
    {
        const FString Name = FString::Printf(TEXT("%.0fx%.0f"), Size.X, Size.Y);
        const float Width = FMath::Max(1.0f, FMath::Min(760.0f, static_cast<float>(Size.X) - 48.0f) - 72.0f);
        FKbBindingStore Store(Temp.Path);
        const TSharedRef<SPWControlsSection> Section = SNew(SPWControlsSection).Store(&Store);
        for (int32 Stage = 0; Stage < 3; ++Stage)
        {
            if (Stage == 1) Section->BeginCapture(EKbAction::Run, EKbDevice::Gamepad);
            if (Stage == 2)
            {
                Section->BeginCapture(EKbAction::Jump, EKbDevice::Keyboard);
                Section->SubmitKey(EKeys::E);
            }
            Section->SlatePrepass(1.0f);
            const FGeometry Root = FGeometry::MakeRoot(FVector2D(Width, 100000.0), FSlateLayoutTransform());
            const float Over = KbpOverflow(Section, Root, Width);
            AddInfo(FString::Printf(TEXT("%s stage %d: content %.0f wide, widest overflow %.1f"), *Name, Stage, Width, Over));
            TestTrue(*FString::Printf(TEXT("%s stage %d: nothing is laid out past the panel's edge"), *Name, Stage), Over <= 0.5f);
        }
        TestTrue(*(Name + TEXT(": every action is still listed")), KbpHasText(Section, KbActionLabel(EKbAction::Quests).ToString()) && KbpHasText(Section, TEXT("Reset all to defaults")));
        TestTrue(*(Name + TEXT(": the prompt's buttons are shown")), KbpHasText(Section, TEXT("Swap")) && KbpHasText(Section, TEXT("Cancel")));
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
