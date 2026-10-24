// Key remapping (order F4): the defaults equal the keys the walking player was hard-coded with, the rules (conflicts, swaps, the
// protected pause keys, which keys fit which slot), the save file and its round trip, loading bad files, and that the mapping context is
// rebuilt from the bindings. Run in the editor under "PerfectWorld.KeyBindings". These use the real binding rules, the real save code on a
// temporary file, and a real Enhanced Input mapping context; no world or player is needed. That the walking player rebuilds its own live
// context when the store changes is the listener in PerfectWorldPlayerCharacterInput.cpp, proved by playing, not here.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "HAL/FileManager.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "PerfectWorldKeyBindings.h"
#include "PerfectWorldKeyBindingsContext.h"
#include "UObject/Package.h"

namespace
{
/** A temporary folder for one test's save file, removed afterwards. */
struct FKbtTempFile
{
    FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("KbTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
    FString Path = FPaths::Combine(Dir, TEXT("KeyBindings.txt"));
    ~FKbtTempFile() { IFileManager::Get().DeleteDirectory(*Dir, false, true); }
};

FString KbtKey(const FKbBindingSet& Set, EKbAction Action, EKbDevice Device)
{
    return Set.Get(Action, Device).ToString();
}

/** One mapping as "Action|Key|Modifiers", the modifiers as S (swizzle), N (negate), D (dead zone) in order. */
TSet<FString> KbtRead(const UInputMappingContext& Context)
{
    TSet<FString> Found;
    for (const FEnhancedActionKeyMapping& Mapping : Context.GetMappings())
    {
        FString Mods;
        for (const TObjectPtr<UInputModifier>& Modifier : Mapping.Modifiers)
        {
            Mods += Cast<UInputModifierSwizzleAxis>(Modifier.Get()) != nullptr ? TEXT("S")
                  : Cast<UInputModifierNegate>(Modifier.Get()) != nullptr      ? TEXT("N")
                  : Cast<UInputModifierDeadZone>(Modifier.Get()) != nullptr    ? TEXT("D") : TEXT("?");
        }
        Found.Add(FString::Printf(TEXT("%s|%s|%s"), *Mapping.Action->GetName(), *Mapping.Key.ToString(), *Mods));
    }
    return Found;
}

/** The actions of the walking player, named as the character names them, on a context built from a set. */
struct FKbtContext
{
    UInputMappingContext* Context = NewObject<UInputMappingContext>(GetTransientPackage());
    FKbContextActions Actions;

    FKbtContext(bool bWithMenus)
    {
        const auto Make = [](const TCHAR* Name, EInputActionValueType Type) -> const UInputAction*
        {
            UInputAction* Action = NewObject<UInputAction>(GetTransientPackage(), FName(Name));
            Action->ValueType = Type;
            return Action;
        };
        Actions.Move = Make(TEXT("IA_Move"), EInputActionValueType::Axis2D);
        Actions.Look = Make(TEXT("IA_Look"), EInputActionValueType::Axis2D);
        Actions.LookStick = Make(TEXT("IA_LookStick"), EInputActionValueType::Axis2D);
        Actions.Run = Make(TEXT("IA_Run"), EInputActionValueType::Boolean);
        Actions.Jump = Make(TEXT("IA_Jump"), EInputActionValueType::Boolean);
        Actions.ToggleCamera = Make(TEXT("IA_ToggleCamera"), EInputActionValueType::Boolean);
        Actions.ZoomWheel = Make(TEXT("IA_ZoomWheel"), EInputActionValueType::Axis1D);
        Actions.ZoomHeld = Make(TEXT("IA_ZoomHeld"), EInputActionValueType::Axis1D);
        Actions.Interact = Make(TEXT("IA_Interact"), EInputActionValueType::Boolean);
        Actions.CycleTarget = Make(TEXT("IA_CycleTarget"), EInputActionValueType::Boolean);
        if (bWithMenus)
        {
            Actions.Map = Make(TEXT("IA_Map"), EInputActionValueType::Boolean);
            Actions.Quests = Make(TEXT("IA_Quests"), EInputActionValueType::Boolean);
        }
    }
    TSet<FString> Build(const FKbBindingSet& Set) const
    {
        KbPopulateWalkingContext(*Context, Actions, *Context, Set);
        return KbtRead(*Context);
    }
};

/** What the walking player mapped before remapping existed, written out by hand from the old PerfectWorldPlayerCharacterInput.cpp. */
TSet<FString> KbtOldMappings()
{
    return TSet<FString>({
        TEXT("IA_Move|W|S"), TEXT("IA_Move|S|SN"), TEXT("IA_Move|A|N"), TEXT("IA_Move|D|"), TEXT("IA_Move|Gamepad_Left2D|D"),
        TEXT("IA_Look|Mouse2D|"), TEXT("IA_LookStick|Gamepad_Right2D|D"),
        TEXT("IA_Run|LeftShift|"), TEXT("IA_Run|Gamepad_LeftThumbstick|"),
        TEXT("IA_Jump|SpaceBar|"), TEXT("IA_Jump|Gamepad_FaceButton_Bottom|"),
        TEXT("IA_ToggleCamera|V|"), TEXT("IA_ToggleCamera|Gamepad_FaceButton_Top|"),
        TEXT("IA_ZoomWheel|MouseWheelAxis|"), TEXT("IA_ZoomHeld|Gamepad_DPad_Up|"), TEXT("IA_ZoomHeld|Gamepad_DPad_Down|N"),
        TEXT("IA_Interact|E|"), TEXT("IA_Interact|Gamepad_FaceButton_Left|"),
        TEXT("IA_CycleTarget|Tab|"), TEXT("IA_CycleTarget|Gamepad_RightShoulder|")});
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKbtDefaultsTest, "PerfectWorld.KeyBindings.DefaultsEqualTheOldKeys",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FKbtDefaultsTest::RunTest(const FString& Parameters)
{
    const FKbBindingSet Defaults;
    struct FOld { EKbAction Action; const TCHAR* Keyboard; const TCHAR* Gamepad; };
    const FOld Old[] = {
        {EKbAction::MoveForward, TEXT("W"), TEXT("Gamepad_Left2D")}, {EKbAction::MoveBack, TEXT("S"), TEXT("Gamepad_Left2D")},
        {EKbAction::MoveLeft, TEXT("A"), TEXT("Gamepad_Left2D")}, {EKbAction::MoveRight, TEXT("D"), TEXT("Gamepad_Left2D")},
        {EKbAction::Run, TEXT("LeftShift"), TEXT("Gamepad_LeftThumbstick")}, {EKbAction::Jump, TEXT("SpaceBar"), TEXT("Gamepad_FaceButton_Bottom")},
        {EKbAction::Interact, TEXT("E"), TEXT("Gamepad_FaceButton_Left")}, {EKbAction::CycleTarget, TEXT("Tab"), TEXT("Gamepad_RightShoulder")},
        {EKbAction::ToggleCamera, TEXT("V"), TEXT("Gamepad_FaceButton_Top")}, {EKbAction::ZoomIn, TEXT("MouseScrollUp"), TEXT("Gamepad_DPad_Up")},
        {EKbAction::ZoomOut, TEXT("MouseScrollDown"), TEXT("Gamepad_DPad_Down")},
        {EKbAction::Map, TEXT("M"), TEXT("Gamepad_DPad_Left")}, {EKbAction::Quests, TEXT("J"), TEXT("Gamepad_DPad_Right")},
        {EKbAction::Pause, TEXT("Escape"), TEXT("Gamepad_Special_Right")}};
    for (const FOld& Row : Old)
    {
        const FString Name = KbInfo(Row.Action).Id;
        TestEqual(*(Name + TEXT(": keyboard default")), KbtKey(Defaults, Row.Action, EKbDevice::Keyboard), FString(Row.Keyboard));
        TestEqual(*(Name + TEXT(": gamepad default")), KbtKey(Defaults, Row.Action, EKbDevice::Gamepad), FString(Row.Gamepad));
        TestTrue(*(Name + TEXT(": is the default")), Defaults.IsDefault(Row.Action));
    }
    TestTrue(TEXT("a new set is every default"), Defaults.IsEveryDefault());

    // The mapping context the defaults build is the old hand-written one, mapping for mapping, modifier for modifier.
    const FKbtContext Walking(false);
    const TSet<FString> Built = Walking.Build(Defaults);
    const TSet<FString> Expected = KbtOldMappings();
    TestEqual(TEXT("twenty mappings, as before"), Built.Num(), 20);
    for (const FString& Line : Expected) TestTrue(*(Line + TEXT(" is mapped")), Built.Contains(Line));
    for (const FString& Line : Built) TestTrue(*(Line + TEXT(" was mapped before")), Expected.Contains(Line));

    // The two new actions, when they exist, take M and J on the keyboard.
    const FKbtContext WithMenus(true);
    const TSet<FString> More = WithMenus.Build(Defaults);
    TestEqual(TEXT("four more mappings for Map and Quests"), More.Num(), 24);
    TestTrue(TEXT("M opens the map"), More.Contains(TEXT("IA_Map|M|")));
    TestTrue(TEXT("J opens the quests"), More.Contains(TEXT("IA_Quests|J|")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKbtTableTest, "PerfectWorld.KeyBindings.ActionTableIsConsistent",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FKbtTableTest::RunTest(const FString& Parameters)
{
    TSet<FString> Ids;
    for (int32 Index = 0; Index < KbActionCount; ++Index)
    {
        const EKbAction Action = static_cast<EKbAction>(Index);
        const FKbActionInfo& Info = KbInfo(Action);
        TestTrue(TEXT("the table is in enum order"), Info.Action == Action);
        TestFalse(*(FString(Info.Id) + TEXT(": ids are unique")), Ids.Contains(FString(Info.Id)));
        Ids.Add(FString(Info.Id));
        TestFalse(*(FString(Info.Id) + TEXT(": has a label")), KbActionLabel(Action).IsEmpty());
        EKbAction Found = EKbAction::Count;
        TestTrue(*(FString(Info.Id) + TEXT(": found by id")), KbFindAction(Info.Id, Found) && Found == Action);
        TestTrue(*(FString(Info.Id) + TEXT(": default keyboard key is a real key")), Info.DefaultKeyboard.IsValid());
        TestTrue(*(FString(Info.Id) + TEXT(": default gamepad key is a real key")), Info.DefaultGamepad.IsValid());
    }
    EKbAction Out = EKbAction::Count;
    TestTrue(TEXT("Map is a known action, on M"), KbFindAction(TEXT("map"), Out) && Out == EKbAction::Map && KbInfo(Out).DefaultKeyboard == EKeys::M);
    TestTrue(TEXT("Quests is a known action, on J"), KbFindAction(TEXT("quests"), Out) && Out == EKbAction::Quests && KbInfo(Out).DefaultKeyboard == EKeys::J);
    TestFalse(TEXT("an unknown id is not found"), KbFindAction(TEXT("fly"), Out));
    TestFalse(TEXT("ids are case sensitive"), KbFindAction(TEXT("JUMP"), Out));
    TestFalse(TEXT("Pause is locked on the keyboard"), KbInfo(EKbAction::Pause).bKeyboardRebindable);
    TestFalse(TEXT("Pause is locked on the gamepad"), KbInfo(EKbAction::Pause).bGamepadRebindable);
    TestFalse(TEXT("the move stick is fixed"), KbInfo(EKbAction::MoveForward).bGamepadRebindable);
    TestTrue(TEXT("move keys are rebindable"), KbInfo(EKbAction::MoveForward).bKeyboardRebindable);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKbtSaveLoadTest, "PerfectWorld.KeyBindings.SaveAndLoadRoundTrip",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FKbtSaveLoadTest::RunTest(const FString& Parameters)
{
    FKbtTempFile Temp;
    {
        FKbBindingStore Store(Temp.Path);
        TestTrue(TEXT("a store with no file reads the defaults"), Store.Current().IsEveryDefault());
        int32 Changes = 0;
        Store.OnChanged.AddLambda([&Changes]() { ++Changes; });

        TestTrue(TEXT("Jump moves to Y"), Store.Rebind(EKbAction::Jump, EKbDevice::Keyboard, EKeys::Y, false) == EKbSetResult::Applied);
        TestTrue(TEXT("Interact on the pad moves to B"), Store.Rebind(EKbAction::Interact, EKbDevice::Gamepad, EKeys::Gamepad_FaceButton_Right, false) == EKbSetResult::Applied);
        TestTrue(TEXT("Interact on the keyboard takes a mouse button"), Store.Rebind(EKbAction::Interact, EKbDevice::Keyboard, EKeys::ThumbMouseButton, false) == EKbSetResult::Applied);
        TestTrue(TEXT("Map is unbound on the pad"), Store.Unbind(EKbAction::Map, EKbDevice::Gamepad) == EKbSetResult::Applied);
        TestTrue(TEXT("Zoom in takes the other wheel direction by swapping"),
                 Store.Rebind(EKbAction::ZoomIn, EKbDevice::Keyboard, EKeys::MouseScrollDown, true) == EKbSetResult::Swapped);
        TestEqual(TEXT("each change is announced once"), Changes, 5);
        TestTrue(TEXT("the file exists"), IFileManager::Get().FileExists(*Temp.Path));

        FString Text;
        FFileHelper::LoadFileToString(Text, *Temp.Path);
        TestTrue(TEXT("it starts with the header"), Text.StartsWith(TEXT("PWKB1")));
        TestTrue(TEXT("it lists what changed"), Text.Contains(TEXT("jump.kb=Y")) && Text.Contains(TEXT("map.pad=none")));
        TestFalse(TEXT("it does not list what did not"), Text.Contains(TEXT("run.")) || Text.Contains(TEXT("pause.")));
        TestFalse(TEXT("it is not under Config"), Temp.Path.Contains(TEXT("/Config/")) || Temp.Path.Contains(TEXT("\\Config\\")));
        TestFalse(TEXT("the real store path is not under Config"), FKbBindingStore::DefaultPath().Contains(TEXT("/Config/")));
        TestTrue(TEXT("it is under Saved"), FKbBindingStore::DefaultPath().Contains(TEXT("/Saved/")));
    }
    {
        FKbBindingStore Again(Temp.Path);
        const FKbBindingSet& Set = Again.Current();
        TestEqual(TEXT("Jump kept Y"), KbtKey(Set, EKbAction::Jump, EKbDevice::Keyboard), FString(TEXT("Y")));
        TestEqual(TEXT("Jump's pad key is untouched"), KbtKey(Set, EKbAction::Jump, EKbDevice::Gamepad), FString(TEXT("Gamepad_FaceButton_Bottom")));
        TestEqual(TEXT("Interact kept B on the pad"), KbtKey(Set, EKbAction::Interact, EKbDevice::Gamepad), FString(TEXT("Gamepad_FaceButton_Right")));
        TestEqual(TEXT("Interact kept the mouse button"), KbtKey(Set, EKbAction::Interact, EKbDevice::Keyboard), FString(TEXT("ThumbMouseButton")));
        TestFalse(TEXT("Map stayed unbound on the pad"), Set.Get(EKbAction::Map, EKbDevice::Gamepad).IsValid());
        TestEqual(TEXT("Zoom in kept the swapped wheel"), KbtKey(Set, EKbAction::ZoomIn, EKbDevice::Keyboard), FString(TEXT("MouseScrollDown")));
        TestEqual(TEXT("and zoom out the other"), KbtKey(Set, EKbAction::ZoomOut, EKbDevice::Keyboard), FString(TEXT("MouseScrollUp")));
        TestEqual(TEXT("Pause is the default"), KbtKey(Set, EKbAction::Pause, EKbDevice::Keyboard), FString(TEXT("Escape")));

        FKbBindingSet Reread;
        TestTrue(TEXT("the set reads its own text"), Reread.FromText(Set.ToText()));
        TestTrue(TEXT("and gets the same set"), Reread == Set);

        // A reset is a real reset: back to the defaults, and no file left behind.
        Again.ResetAction(EKbAction::Jump);
        TestTrue(TEXT("Jump is reset"), Again.Current().IsDefault(EKbAction::Jump));
        TestFalse(TEXT("the others are not"), Again.Current().IsEveryDefault());
        Again.ResetAll();
        TestTrue(TEXT("Reset all gives the defaults"), Again.Current().IsEveryDefault());
        TestFalse(TEXT("and removes the file"), IFileManager::Get().FileExists(*Temp.Path));
    }
    FKbBindingStore Last(Temp.Path);
    TestTrue(TEXT("a reloaded store after reset is the defaults"), Last.Current().IsEveryDefault());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKbtConflictTest, "PerfectWorld.KeyBindings.ConflictsAreFoundAndSwapped",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FKbtConflictTest::RunTest(const FString& Parameters)
{
    FKbBindingSet Set;
    EKbAction Other = EKbAction::Count;
    TestTrue(TEXT("E is held by Interact"), Set.FindHolder(EKbAction::Jump, EKbDevice::Keyboard, EKeys::E, Other) && Other == EKbAction::Interact);
    TestFalse(TEXT("Interact itself is not its own conflict"), Set.FindHolder(EKbAction::Interact, EKbDevice::Keyboard, EKeys::E, Other));
    TestFalse(TEXT("a key on the other device is no conflict"), Set.FindHolder(EKbAction::Jump, EKbDevice::Gamepad, EKeys::E, Other));

    TestTrue(TEXT("binding a held key is a conflict"), Set.Set(EKbAction::Jump, EKbDevice::Keyboard, EKeys::E, false, &Other) == EKbSetResult::Conflict);
    TestTrue(TEXT("naming the holder"), Other == EKbAction::Interact);
    TestTrue(TEXT("and changing nothing"), Set.IsEveryDefault());

    TestTrue(TEXT("with a swap it goes through"), Set.Set(EKbAction::Jump, EKbDevice::Keyboard, EKeys::E, true) == EKbSetResult::Swapped);
    TestEqual(TEXT("Jump has E"), KbtKey(Set, EKbAction::Jump, EKbDevice::Keyboard), FString(TEXT("E")));
    TestEqual(TEXT("Interact has Jump's old key"), KbtKey(Set, EKbAction::Interact, EKbDevice::Keyboard), FString(TEXT("SpaceBar")));
    TestTrue(TEXT("the swap leaves the pad alone"), Set.IsDefault(EKbAction::Jump, EKbDevice::Gamepad) && Set.IsDefault(EKbAction::Interact, EKbDevice::Gamepad));
    TestTrue(TEXT("binding the key it already has changes nothing"), Set.Set(EKbAction::Jump, EKbDevice::Keyboard, EKeys::E, false) == EKbSetResult::Unchanged);

    // The pad is a separate set of keys.
    TestTrue(TEXT("a pad conflict is found too"), Set.Set(EKbAction::Jump, EKbDevice::Gamepad, EKeys::Gamepad_FaceButton_Left, false, &Other) == EKbSetResult::Conflict
             && Other == EKbAction::Interact);

    // Swapping into an unbound slot leaves the other action unbound.
    FKbBindingSet Fresh;
    Fresh.Clear(EKbAction::Run, EKbDevice::Keyboard);
    TestTrue(TEXT("Run is unbound"), !Fresh.Get(EKbAction::Run, EKbDevice::Keyboard).IsValid());
    TestTrue(TEXT("Run takes V by swap"), Fresh.Set(EKbAction::Run, EKbDevice::Keyboard, EKeys::V, true) == EKbSetResult::Swapped);
    TestFalse(TEXT("so the camera key is now unbound"), Fresh.Get(EKbAction::ToggleCamera, EKbDevice::Keyboard).IsValid());

    // A swap the other action cannot take fails whole: a wheel key cannot go to Jump.
    FKbBindingSet Wheel;
    TestTrue(TEXT("Zoom in cannot swap onto Space"), Wheel.Set(EKbAction::ZoomIn, EKbDevice::Keyboard, EKeys::SpaceBar, true) == EKbSetResult::NotAllowed);
    TestTrue(TEXT("and nothing changed"), Wheel.IsEveryDefault());

    // Reset per slot puts the key back and reports the collision it causes.
    FKbBindingSet Moved;
    Moved.Set(EKbAction::Jump, EKbDevice::Keyboard, EKeys::Y, false);
    Moved.Set(EKbAction::Interact, EKbDevice::Keyboard, EKeys::SpaceBar, false);
    TestTrue(TEXT("Jump's reset finds Space taken"), Moved.ResetSlot(EKbAction::Jump, EKbDevice::Keyboard, false, &Other) == EKbSetResult::Conflict && Other == EKbAction::Interact);
    TestTrue(TEXT("and swaps when asked"), Moved.ResetSlot(EKbAction::Jump, EKbDevice::Keyboard, true) == EKbSetResult::Swapped);
    TestEqual(TEXT("Jump is Space again"), KbtKey(Moved, EKbAction::Jump, EKbDevice::Keyboard), FString(TEXT("SpaceBar")));
    TestEqual(TEXT("Interact took Y"), KbtKey(Moved, EKbAction::Interact, EKbDevice::Keyboard), FString(TEXT("Y")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKbtProtectedTest, "PerfectWorld.KeyBindings.PauseKeysAreProtectedAndKeysFitTheirSlot",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FKbtProtectedTest::RunTest(const FString& Parameters)
{
    FKbtTempFile Temp;
    FKbBindingStore Store(Temp.Path);
    int32 Changes = 0;
    Store.OnChanged.AddLambda([&Changes]() { ++Changes; });

    for (const EKbDevice Device : {EKbDevice::Keyboard, EKbDevice::Gamepad})
    {
        TestTrue(TEXT("Pause cannot be unbound"), Store.Unbind(EKbAction::Pause, Device) == EKbSetResult::NotRebindable);
        TestTrue(TEXT("Pause cannot be rebound"), Store.Rebind(EKbAction::Pause, Device, Device == EKbDevice::Keyboard ? EKeys::P : EKeys::Gamepad_FaceButton_Top, false) == EKbSetResult::NotRebindable);
    }
    TestTrue(TEXT("Escape cannot go to Jump"), Store.Rebind(EKbAction::Jump, EKbDevice::Keyboard, EKeys::Escape, false) == EKbSetResult::Reserved);
    TestTrue(TEXT("even with a swap"), Store.Rebind(EKbAction::Jump, EKbDevice::Keyboard, EKeys::Escape, true) == EKbSetResult::Reserved);
    TestTrue(TEXT("the menu button cannot go to Interact"), Store.Rebind(EKbAction::Interact, EKbDevice::Gamepad, EKeys::Gamepad_Special_Right, true) == EKbSetResult::Reserved);
    TestTrue(TEXT("Pause cannot be reset away from its keys"), Store.Current().IsDefault(EKbAction::Pause));
    TestEqual(TEXT("none of that changed or announced anything"), Changes, 0);
    TestFalse(TEXT("or wrote a file"), IFileManager::Get().FileExists(*Temp.Path));

    // Which key fits which slot.
    const auto Judge = [](EKbAction Action, EKbDevice Device, const FKey& Key) { return FKbBindingSet::Judge(Action, Device, Key); };
    for (const FKey& Key : {EKeys::F5, EKeys::Q, EKeys::RightMouseButton, EKeys::MiddleMouseButton, EKeys::ThumbMouseButton, EKeys::ThumbMouseButton2,
                            EKeys::LeftControl, EKeys::Enter, EKeys::Up})
    {
        TestTrue(*(Key.ToString() + TEXT(" fits a keyboard slot")), Judge(EKbAction::Jump, EKbDevice::Keyboard, Key) == EKbSetResult::Applied);
    }
    for (const FKey& Key : {EKeys::Gamepad_LeftShoulder, EKeys::Gamepad_LeftTrigger, EKeys::Gamepad_RightTrigger, EKeys::Gamepad_DPad_Left, EKeys::Gamepad_RightThumbstick,
                            EKeys::Gamepad_Special_Left, EKeys::Gamepad_FaceButton_Right})
    {
        TestTrue(*(Key.ToString() + TEXT(" fits a gamepad slot")), Judge(EKbAction::Jump, EKbDevice::Gamepad, Key) == EKbSetResult::Applied);
    }
    for (const FKey& Key : {EKeys::Gamepad_FaceButton_Bottom, EKeys::Mouse2D, EKeys::MouseX, EKeys::MouseWheelAxis, EKeys::LeftMouseButton, EKeys::Tilde, FKey()})
    {
        TestTrue(*(Key.ToString() + TEXT(" does not fit a keyboard slot")), Judge(EKbAction::Jump, EKbDevice::Keyboard, Key) == EKbSetResult::NotAllowed);
    }
    for (const FKey& Key : {EKeys::E, EKeys::LeftMouseButton, EKeys::Gamepad_Left2D, EKeys::Gamepad_LeftX, EKeys::Gamepad_LeftTriggerAxis, EKeys::Gamepad_LeftStick_Up,
                            EKeys::Gamepad_RightStick_Left, FKey()})
    {
        TestTrue(*(Key.ToString() + TEXT(" does not fit a gamepad slot")), Judge(EKbAction::Jump, EKbDevice::Gamepad, Key) == EKbSetResult::NotAllowed);
    }
    TestTrue(TEXT("the wheel fits zoom"), Judge(EKbAction::ZoomIn, EKbDevice::Keyboard, EKeys::MouseScrollDown) == EKbSetResult::Applied);
    TestTrue(TEXT("but not jump"), Judge(EKbAction::Jump, EKbDevice::Keyboard, EKeys::MouseScrollUp) == EKbSetResult::NotAllowed);
    TestTrue(TEXT("the move stick cannot be rebound"), Judge(EKbAction::MoveForward, EKbDevice::Gamepad, EKeys::Gamepad_FaceButton_Top) == EKbSetResult::NotRebindable);
    TestTrue(TEXT("a movement key can"), Judge(EKbAction::MoveForward, EKbDevice::Keyboard, EKeys::Up) == EKbSetResult::Applied);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKbtBadFileTest, "PerfectWorld.KeyBindings.LoadingDropsBadAndConflictingEntries",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FKbtBadFileTest::RunTest(const FString& Parameters)
{
    FKbBindingSet Set;
    TestFalse(TEXT("text without the header is not a bindings file"), Set.FromText(TEXT("jump.kb=Y\n")));
    TestTrue(TEXT("and leaves the defaults"), Set.IsEveryDefault());
    TestFalse(TEXT("an empty file is not either"), Set.FromText(TEXT("")));

    const FString Messy = TEXT("PWKB1\n# a comment\n\nfly.kb=Y\nrun.keyboard=R\njump.kb=NoSuchKey\npause.kb=P\npause.pad=none\n"
                               "jump.kb=MouseScrollUp\ninteract.kb=Escape\nmap.pad=none\nquests.kb=F9\nzoom_in.kb=SpaceBar\ncycle_target.kb=E\ntoggle_camera.pad=Gamepad_LeftStick_Up\n"
                               "move_forward.pad=Gamepad_FaceButton_Top\nrun.kb=R");
    TestTrue(TEXT("a messy file is still a bindings file"), Set.FromText(Messy));
    TestEqual(TEXT("an unknown key name and a wheel key on Jump are ignored"), KbtKey(Set, EKbAction::Jump, EKbDevice::Keyboard), FString(TEXT("SpaceBar")));
    TestEqual(TEXT("an unknown device word is ignored, a later good line is kept"), KbtKey(Set, EKbAction::Run, EKbDevice::Keyboard), FString(TEXT("R")));
    TestEqual(TEXT("Pause keeps Escape"), KbtKey(Set, EKbAction::Pause, EKbDevice::Keyboard), FString(TEXT("Escape")));
    TestEqual(TEXT("and its pad key"), KbtKey(Set, EKbAction::Pause, EKbDevice::Gamepad), FString(TEXT("Gamepad_Special_Right")));
    TestEqual(TEXT("Escape on Interact is refused"), KbtKey(Set, EKbAction::Interact, EKbDevice::Keyboard), FString(TEXT("E")));
    TestFalse(TEXT("none unbinds a slot"), Set.Get(EKbAction::Map, EKbDevice::Gamepad).IsValid());
    TestEqual(TEXT("a good rebinding is kept"), KbtKey(Set, EKbAction::Quests, EKbDevice::Keyboard), FString(TEXT("F9")));
    TestEqual(TEXT("Zoom in on Space gives way to Jump, the default"), KbtKey(Set, EKbAction::ZoomIn, EKbDevice::Keyboard), FString(TEXT("MouseScrollUp")));
    TestEqual(TEXT("and so does Next target on E"), KbtKey(Set, EKbAction::CycleTarget, EKbDevice::Keyboard), FString(TEXT("Tab")));
    TestEqual(TEXT("a stick direction on a button is dropped"), KbtKey(Set, EKbAction::ToggleCamera, EKbDevice::Gamepad), FString(TEXT("Gamepad_FaceButton_Top")));
    TestEqual(TEXT("a fixed slot cannot be changed by a file"), KbtKey(Set, EKbAction::MoveForward, EKbDevice::Gamepad), FString(TEXT("Gamepad_Left2D")));

    // Two overrides on one key: the later action gives way.
    FKbBindingSet Clash;
    TestTrue(TEXT("two overrides on one key load"), Clash.FromText(TEXT("PWKB1\nrun.kb=Y\ninteract.kb=Y\n")));
    TestEqual(TEXT("the earlier keeps it"), KbtKey(Clash, EKbAction::Run, EKbDevice::Keyboard), FString(TEXT("Y")));
    TestEqual(TEXT("the later goes back to its default"), KbtKey(Clash, EKbAction::Interact, EKbDevice::Keyboard), FString(TEXT("E")));

    // A missing or unreadable file is the defaults.
    FKbtTempFile Temp;
    FKbBindingStore Missing(Temp.Path);
    TestTrue(TEXT("no file means the defaults"), Missing.Current().IsEveryDefault());
    FFileHelper::SaveStringToFile(TEXT("not a bindings file"), *Temp.Path);
    FKbBindingStore Garbage(Temp.Path);
    TestTrue(TEXT("a garbage file means the defaults"), Garbage.Current().IsEveryDefault());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKbtRebuildTest, "PerfectWorld.KeyBindings.TheMappingContextIsRebuiltFromOverrides",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FKbtRebuildTest::RunTest(const FString& Parameters)
{
    const FKbtContext Walking(true);
    FKbBindingSet Set;
    const TSet<FString> Before = Walking.Build(Set);
    TestTrue(TEXT("Space jumps at first"), Before.Contains(TEXT("IA_Jump|SpaceBar|")));

    // The same context object, rebuilt after a change, as the live player does.
    Set.Set(EKbAction::Jump, EKbDevice::Keyboard, EKeys::Y, false);
    Set.Set(EKbAction::Jump, EKbDevice::Gamepad, EKeys::Gamepad_LeftShoulder, false);
    Set.Set(EKbAction::MoveForward, EKbDevice::Keyboard, EKeys::Up, false);
    Set.Set(EKbAction::Map, EKbDevice::Keyboard, EKeys::N, false);
    Set.Clear(EKbAction::Quests, EKbDevice::Keyboard);
    const TSet<FString> After = Walking.Build(Set);
    TestTrue(TEXT("Y jumps"), After.Contains(TEXT("IA_Jump|Y|")));
    TestTrue(TEXT("so does the new pad key"), After.Contains(TEXT("IA_Jump|Gamepad_LeftShoulder|")));
    TestFalse(TEXT("Space no longer does"), After.Contains(TEXT("IA_Jump|SpaceBar|")));
    TestFalse(TEXT("nor the old pad key"), After.Contains(TEXT("IA_Jump|Gamepad_FaceButton_Bottom|")));
    TestTrue(TEXT("Up walks forward, with the forward modifiers"), After.Contains(TEXT("IA_Move|Up|S")));
    TestFalse(TEXT("W no longer does"), After.Contains(TEXT("IA_Move|W|S")));
    TestTrue(TEXT("the stick still walks"), After.Contains(TEXT("IA_Move|Gamepad_Left2D|D")));
    TestTrue(TEXT("the map moved to N"), After.Contains(TEXT("IA_Map|N|")) && !After.Contains(TEXT("IA_Map|M|")));
    TestFalse(TEXT("an unbound quests key maps nothing"), After.Contains(TEXT("IA_Quests|J|")));
    TestTrue(TEXT("its pad key still does"), After.Contains(TEXT("IA_Quests|Gamepad_DPad_Right|")));
    TestTrue(TEXT("looking is untouched"), After.Contains(TEXT("IA_Look|Mouse2D|")) && After.Contains(TEXT("IA_LookStick|Gamepad_Right2D|D")));
    TestEqual(TEXT("the old mappings are gone, not added to"), After.Num(), Before.Num() - 1);

    // And back: a reset rebuilds the original mapping.
    Set.ResetAll();
    const TSet<FString> Reset = Walking.Build(Set);
    TestTrue(TEXT("a reset context equals the first"), Reset.Includes(Before) && Before.Includes(Reset));

    // Zoom: the wheel pair swapped is the wheel axis negated; other keys are mapped one by one.
    FKbBindingSet Zoom;
    Zoom.Set(EKbAction::ZoomIn, EKbDevice::Keyboard, EKeys::MouseScrollDown, true);
    TSet<FString> Swapped = Walking.Build(Zoom);
    TestTrue(TEXT("a swapped wheel is the axis, negated"), Swapped.Contains(TEXT("IA_ZoomWheel|MouseWheelAxis|N")) && !Swapped.Contains(TEXT("IA_ZoomWheel|MouseWheelAxis|")));
    FKbBindingSet Keys;
    Keys.Set(EKbAction::ZoomIn, EKbDevice::Keyboard, EKeys::PageUp, false);
    Keys.Set(EKbAction::ZoomOut, EKbDevice::Keyboard, EKeys::PageDown, false);
    TSet<FString> Held = Walking.Build(Keys);
    TestTrue(TEXT("zoom keys feed the held action"), Held.Contains(TEXT("IA_ZoomHeld|PageUp|")) && Held.Contains(TEXT("IA_ZoomHeld|PageDown|N")));
    TestFalse(TEXT("and the wheel no longer zooms"), Held.Contains(TEXT("IA_ZoomWheel|MouseWheelAxis|")));
    FKbBindingSet Half;
    Half.Set(EKbAction::ZoomOut, EKbDevice::Keyboard, EKeys::PageDown, false);
    TSet<FString> Mixed = Walking.Build(Half);
    TestTrue(TEXT("one wheel direction left maps its wheel key"), Mixed.Contains(TEXT("IA_ZoomWheel|MouseScrollUp|")) && Mixed.Contains(TEXT("IA_ZoomHeld|PageDown|N")));

    // The store announces changes so the player can rebuild; refused changes announce nothing.
    FKbtTempFile Temp;
    FKbBindingStore Store(Temp.Path);
    int32 Changes = 0;
    Store.OnChanged.AddLambda([&Changes]() { ++Changes; });
    Store.Rebind(EKbAction::Jump, EKbDevice::Keyboard, EKeys::Y, false);
    TestEqual(TEXT("a rebind is announced"), Changes, 1);
    Store.Rebind(EKbAction::Jump, EKbDevice::Keyboard, EKeys::Y, false);
    TestEqual(TEXT("the same rebind is not"), Changes, 1);
    Store.Rebind(EKbAction::Run, EKbDevice::Keyboard, EKeys::E, false);
    TestEqual(TEXT("a conflict is not"), Changes, 1);
    Store.ResetAll();
    TestEqual(TEXT("a reset is"), Changes, 2);
    Store.ResetAll();
    TestEqual(TEXT("a reset of nothing is not"), Changes, 2);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
