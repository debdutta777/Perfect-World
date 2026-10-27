#include "PerfectWorldKeyBindings.h"

namespace
{
/** The action table, built on first use so every default key is a registered key by then. Indexed by EKbAction. */
const TArray<FKbActionInfo>& KbTable()
{
    static const TArray<FKbActionInfo> Table = []()
    {
        TArray<FKbActionInfo> Built;
        const auto Add = [&Built](EKbAction Action, const TCHAR* Id, EKbGroup Group, const FKey& Keyboard, const FKey& Gamepad,
                                  bool bKeyboardRebindable, bool bGamepadRebindable)
        {
            FKbActionInfo& Info = Built.AddDefaulted_GetRef();
            Info.Action = Action;
            Info.Id = Id;
            Info.Group = Group;
            Info.DefaultKeyboard = Keyboard;
            Info.DefaultGamepad = Gamepad;
            Info.bKeyboardRebindable = bKeyboardRebindable;
            Info.bGamepadRebindable = bGamepadRebindable;
        };
        // The keyboard and gamepad defaults are the keys PerfectWorldPlayerCharacterInput.cpp was hard-coded with. Movement on the
        // gamepad is the left stick, which is an axis and stays fixed. Map and Quests are new and have no older keys to match.
        Add(EKbAction::MoveForward, TEXT("move_forward"), EKbGroup::Movement, EKeys::W, EKeys::Gamepad_Left2D, true, false);
        Add(EKbAction::MoveBack, TEXT("move_back"), EKbGroup::Movement, EKeys::S, EKeys::Gamepad_Left2D, true, false);
        Add(EKbAction::MoveLeft, TEXT("move_left"), EKbGroup::Movement, EKeys::A, EKeys::Gamepad_Left2D, true, false);
        Add(EKbAction::MoveRight, TEXT("move_right"), EKbGroup::Movement, EKeys::D, EKeys::Gamepad_Left2D, true, false);
        Add(EKbAction::Run, TEXT("run"), EKbGroup::Movement, EKeys::LeftShift, EKeys::Gamepad_LeftThumbstick, true, true);
        Add(EKbAction::Jump, TEXT("jump"), EKbGroup::Movement, EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom, true, true);
        Add(EKbAction::Interact, TEXT("interact"), EKbGroup::Actions, EKeys::E, EKeys::Gamepad_FaceButton_Left, true, true);
        Add(EKbAction::CycleTarget, TEXT("cycle_target"), EKbGroup::Actions, EKeys::Tab, EKeys::Gamepad_RightShoulder, true, true);
        Add(EKbAction::ToggleCamera, TEXT("toggle_camera"), EKbGroup::Camera, EKeys::V, EKeys::Gamepad_FaceButton_Top, true, true);
        Add(EKbAction::ZoomIn, TEXT("zoom_in"), EKbGroup::Camera, EKeys::MouseScrollUp, EKeys::Gamepad_DPad_Up, true, true);
        Add(EKbAction::ZoomOut, TEXT("zoom_out"), EKbGroup::Camera, EKeys::MouseScrollDown, EKeys::Gamepad_LeftShoulder, true, true); // R88: Down opens Inventory
        Add(EKbAction::Map, TEXT("map"), EKbGroup::Interface, EKeys::M, EKeys::Gamepad_DPad_Left, true, true);
        Add(EKbAction::Quests, TEXT("quests"), EKbGroup::Interface, EKeys::J, EKeys::Gamepad_DPad_Right, true, true);
        Add(EKbAction::Pause, TEXT("pause"), EKbGroup::Interface, EKeys::Escape, EKeys::Gamepad_Special_Right, false, false);
        Add(EKbAction::Inventory, TEXT("inventory"), EKbGroup::Interface, EKeys::I, EKeys::Gamepad_DPad_Down, true, true); // R88
        return Built;
    }();
    return Table;
}

bool KbIsPauseKey(const FKey& Key)
{
    return Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right;
}

bool KbIsZoom(EKbAction Action)
{
    return Action == EKbAction::ZoomIn || Action == EKbAction::ZoomOut;
}
} // namespace

const FKbActionInfo& KbInfo(EKbAction Action)
{
    const TArray<FKbActionInfo>& Table = KbTable();
    const int32 Index = static_cast<int32>(Action);
    return Table[Table.IsValidIndex(Index) ? Index : 0];
}

bool KbFindAction(const FString& Id, EKbAction& OutAction)
{
    for (const FKbActionInfo& Info : KbTable())
    {
        if (Id.Equals(Info.Id, ESearchCase::CaseSensitive))
        {
            OutAction = Info.Action;
            return true;
        }
    }
    return false;
}

bool KbIsStickDirection(const FKey& Key)
{
    return Key == EKeys::Gamepad_LeftStick_Up || Key == EKeys::Gamepad_LeftStick_Down || Key == EKeys::Gamepad_LeftStick_Left
        || Key == EKeys::Gamepad_LeftStick_Right || Key == EKeys::Gamepad_RightStick_Up || Key == EKeys::Gamepad_RightStick_Down
        || Key == EKeys::Gamepad_RightStick_Left || Key == EKeys::Gamepad_RightStick_Right;
}

bool KbIsWheelKey(const FKey& Key)
{
    return Key == EKeys::MouseScrollUp || Key == EKeys::MouseScrollDown;
}

FKbBindingSet::FKbBindingSet()
{
    for (int32 Action = 0; Action < KbActionCount; ++Action)
    {
        for (int32 Device = 0; Device < KbDeviceCount; ++Device)
        {
            Slots[Action][Device] = DefaultKey(static_cast<EKbAction>(Action), static_cast<EKbDevice>(Device));
        }
    }
}

FKey FKbBindingSet::Get(EKbAction Action, EKbDevice Device) const
{
    const int32 A = static_cast<int32>(Action);
    const int32 D = static_cast<int32>(Device);
    return A >= 0 && A < KbActionCount && D >= 0 && D < KbDeviceCount ? Slots[A][D] : FKey();
}

FKey FKbBindingSet::DefaultKey(EKbAction Action, EKbDevice Device)
{
    const FKbActionInfo& Info = KbInfo(Action);
    return Device == EKbDevice::Keyboard ? Info.DefaultKeyboard : Info.DefaultGamepad;
}

bool FKbBindingSet::IsDefault(EKbAction Action, EKbDevice Device) const
{
    return Get(Action, Device) == DefaultKey(Action, Device);
}

bool FKbBindingSet::IsDefault(EKbAction Action) const
{
    return IsDefault(Action, EKbDevice::Keyboard) && IsDefault(Action, EKbDevice::Gamepad);
}

bool FKbBindingSet::IsEveryDefault() const
{
    for (int32 Action = 0; Action < KbActionCount; ++Action)
    {
        if (!IsDefault(static_cast<EKbAction>(Action)))
        {
            return false;
        }
    }
    return true;
}

EKbSetResult FKbBindingSet::Judge(EKbAction Action, EKbDevice Device, const FKey& Key)
{
    const FKbActionInfo& Info = KbInfo(Action);
    if (!(Device == EKbDevice::Keyboard ? Info.bKeyboardRebindable : Info.bGamepadRebindable))
    {
        return EKbSetResult::NotRebindable;
    }
    if (!Key.IsValid())
    {
        return EKbSetResult::NotAllowed;
    }
    if (KbIsPauseKey(Key))
    {
        return EKbSetResult::Reserved;
    }
    if (Device == EKbDevice::Keyboard)
    {
        // The left mouse button is how the menus are clicked, the tilde key opens the console, and axes are not buttons.
        if (Key.IsGamepadKey() || Key.IsTouch() || Key.IsGesture() || Key.IsAnalog() || Key == EKeys::LeftMouseButton
            || Key == EKeys::Tilde || Key == EKeys::AnyKey)
        {
            return EKbSetResult::NotAllowed;
        }
        if (KbIsWheelKey(Key) && !KbIsZoom(Action))
        {
            return EKbSetResult::NotAllowed;
        }
        return EKbSetResult::Applied;
    }
    if (!Key.IsGamepadKey() || Key.IsAnalog() || KbIsStickDirection(Key))
    {
        return EKbSetResult::NotAllowed;
    }
    return EKbSetResult::Applied;
}

bool FKbBindingSet::FindHolder(EKbAction Except, EKbDevice Device, const FKey& Key, EKbAction& OutHolder) const
{
    if (!Key.IsValid())
    {
        return false;
    }
    for (int32 Index = 0; Index < KbActionCount; ++Index)
    {
        const EKbAction Other = static_cast<EKbAction>(Index);
        if (Other != Except && Get(Other, Device) == Key)
        {
            OutHolder = Other;
            return true;
        }
    }
    return false;
}

EKbSetResult FKbBindingSet::Set(EKbAction Action, EKbDevice Device, const FKey& Key, bool bSwap, EKbAction* OutOther)
{
    const EKbSetResult Verdict = Judge(Action, Device, Key);
    if (Verdict != EKbSetResult::Applied)
    {
        return Verdict;
    }
    const FKey Current = Get(Action, Device);
    if (Current == Key)
    {
        return EKbSetResult::Unchanged;
    }
    EKbAction Holder = EKbAction::Count;
    if (!FindHolder(Action, Device, Key, Holder))
    {
        SetSlot(Action, Device, Key);
        return EKbSetResult::Applied;
    }
    if (OutOther != nullptr)
    {
        *OutOther = Holder;
    }
    if (!bSwap)
    {
        return EKbSetResult::Conflict;
    }
    // The holder takes this slot's old key, which may be nothing. If it cannot take it (a wheel key on a non-zoom action) the swap fails.
    if (Current.IsValid() && Judge(Holder, Device, Current) != EKbSetResult::Applied)
    {
        return EKbSetResult::NotAllowed;
    }
    SetSlot(Holder, Device, Current);
    SetSlot(Action, Device, Key);
    return EKbSetResult::Swapped;
}

EKbSetResult FKbBindingSet::Clear(EKbAction Action, EKbDevice Device)
{
    const FKbActionInfo& Info = KbInfo(Action);
    if (!(Device == EKbDevice::Keyboard ? Info.bKeyboardRebindable : Info.bGamepadRebindable))
    {
        return EKbSetResult::NotRebindable;
    }
    if (!Get(Action, Device).IsValid())
    {
        return EKbSetResult::Unchanged;
    }
    SetSlot(Action, Device, FKey());
    return EKbSetResult::Applied;
}

EKbSetResult FKbBindingSet::ResetSlot(EKbAction Action, EKbDevice Device, bool bSwap, EKbAction* OutOther)
{
    const FKbActionInfo& Info = KbInfo(Action);
    if (!(Device == EKbDevice::Keyboard ? Info.bKeyboardRebindable : Info.bGamepadRebindable))
    {
        return EKbSetResult::NotRebindable;
    }
    const FKey Default = DefaultKey(Action, Device);
    if (Get(Action, Device) == Default)
    {
        return EKbSetResult::Unchanged;
    }
    return Default.IsValid() ? Set(Action, Device, Default, bSwap, OutOther) : Clear(Action, Device);
}

void FKbBindingSet::ResetAll()
{
    *this = FKbBindingSet();
}

bool FKbBindingSet::operator==(const FKbBindingSet& Other) const
{
    for (int32 Action = 0; Action < KbActionCount; ++Action)
    {
        for (int32 Device = 0; Device < KbDeviceCount; ++Device)
        {
            if (Slots[Action][Device] != Other.Slots[Action][Device])
            {
                return false;
            }
        }
    }
    return true;
}

void FKbBindingSet::Normalize()
{
    // Each pass fixes one problem by putting one slot back to its default, so the loop ends; the defaults never collide.
    for (int32 Pass = 0; Pass < KbActionCount * KbDeviceCount * 2; ++Pass)
    {
        bool bFixed = false;
        for (int32 Device = 0; Device < KbDeviceCount && !bFixed; ++Device)
        {
            const EKbDevice Dev = static_cast<EKbDevice>(Device);
            for (int32 Index = 0; Index < KbActionCount && !bFixed; ++Index)
            {
                const EKbAction Action = static_cast<EKbAction>(Index);
                const FKey Key = Get(Action, Dev);
                if (!Key.IsValid() || IsDefault(Action, Dev))
                {
                    continue;
                }
                EKbAction Holder = EKbAction::Count;
                const bool bBad = Judge(Action, Dev, Key) != EKbSetResult::Applied;
                // Of two actions on one key the later override gives way; an override always gives way to a default.
                const bool bClash = FindHolder(Action, Dev, Key, Holder) && (IsDefault(Holder, Dev) || Holder < Action);
                if (bBad || bClash)
                {
                    SetSlot(Action, Dev, DefaultKey(Action, Dev));
                    bFixed = true;
                }
            }
        }
        if (!bFixed)
        {
            return;
        }
    }
}
