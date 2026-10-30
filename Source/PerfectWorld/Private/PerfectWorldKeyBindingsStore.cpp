#include "PerfectWorldKeyBindings.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "PerfectWorldKeyBindings"

namespace
{
const TCHAR* const KbFileHeader = TEXT("PWKB1");
const TCHAR* const KbKeyboardTag = TEXT("kb");
const TCHAR* const KbGamepadTag = TEXT("pad");
const TCHAR* const KbNoneValue = TEXT("none");
} // namespace

FText KbActionLabel(EKbAction Action)
{
    switch (Action)
    {
    case EKbAction::MoveForward: return LOCTEXT("MoveForward", "Move forward");
    case EKbAction::MoveBack: return LOCTEXT("MoveBack", "Move back");
    case EKbAction::MoveLeft: return LOCTEXT("MoveLeft", "Move left");
    case EKbAction::MoveRight: return LOCTEXT("MoveRight", "Move right");
    case EKbAction::Run: return LOCTEXT("Run", "Run (hold)");
    case EKbAction::Jump: return LOCTEXT("Jump", "Jump");
    case EKbAction::Interact: return LOCTEXT("Interact", "Interact");
    case EKbAction::CycleTarget: return LOCTEXT("CycleTarget", "Next target");
    case EKbAction::ToggleCamera: return LOCTEXT("ToggleCamera", "Switch camera");
    case EKbAction::ZoomIn: return LOCTEXT("ZoomIn", "Zoom in");
    case EKbAction::ZoomOut: return LOCTEXT("ZoomOut", "Zoom out");
    case EKbAction::Map: return LOCTEXT("Map", "Map");
    case EKbAction::Quests: return LOCTEXT("Quests", "Quests");
    case EKbAction::Inventory: return LOCTEXT("Inventory", "Inventory"); // R88
    case EKbAction::Pause: return LOCTEXT("Pause", "Pause menu");
    default: return FText::GetEmpty();
    }
}

FText KbGroupLabel(EKbGroup Group)
{
    switch (Group)
    {
    case EKbGroup::Movement: return LOCTEXT("GroupMovement", "MOVEMENT");
    case EKbGroup::Actions: return LOCTEXT("GroupActions", "ACTIONS");
    case EKbGroup::Camera: return LOCTEXT("GroupCamera", "CAMERA");
    case EKbGroup::Interface: return LOCTEXT("GroupInterface", "MENUS AND SCREENS");
    default: return FText::GetEmpty();
    }
}

FText KbDeviceLabel(EKbDevice Device)
{
    return Device == EKbDevice::Keyboard ? LOCTEXT("DeviceKeyboard", "KEYBOARD AND MOUSE") : LOCTEXT("DeviceGamepad", "GAMEPAD");
}

FText KbKeyLabel(const FKey& Key)
{
    if (!Key.IsValid()) return LOCTEXT("Unbound", "Unbound");
    if (Key == EKeys::Gamepad_Left2D) return LOCTEXT("LeftStick", "Left stick");
    if (Key == EKeys::MouseScrollUp) return LOCTEXT("WheelUp", "Mouse Wheel Up");
    if (Key == EKeys::MouseScrollDown) return LOCTEXT("WheelDown", "Mouse Wheel Down");
    if (Key == EKeys::Gamepad_FaceButton_Bottom) return LOCTEXT("PadSouth", "A / Cross");
    if (Key == EKeys::Gamepad_FaceButton_Right) return LOCTEXT("PadEast", "B / Circle");
    if (Key == EKeys::Gamepad_FaceButton_Left) return LOCTEXT("PadWest", "X / Square");
    if (Key == EKeys::Gamepad_FaceButton_Top) return LOCTEXT("PadNorth", "Y / Triangle");
    if (Key == EKeys::Gamepad_LeftShoulder) return LOCTEXT("PadLB", "LB / L1");
    if (Key == EKeys::Gamepad_RightShoulder) return LOCTEXT("PadRB", "RB / R1");
    if (Key == EKeys::Gamepad_LeftTrigger) return LOCTEXT("PadLT", "LT / L2");
    if (Key == EKeys::Gamepad_RightTrigger) return LOCTEXT("PadRT", "RT / R2");
    if (Key == EKeys::Gamepad_LeftThumbstick) return LOCTEXT("PadL3", "Left stick click (L3)");
    if (Key == EKeys::Gamepad_RightThumbstick) return LOCTEXT("PadR3", "Right stick click (R3)");
    if (Key == EKeys::Gamepad_Special_Left) return LOCTEXT("PadView", "View button");
    if (Key == EKeys::Gamepad_Special_Right) return LOCTEXT("PadMenu", "Menu button");
    if (Key == EKeys::Gamepad_DPad_Up) return LOCTEXT("DPadUp", "D-pad Up");
    if (Key == EKeys::Gamepad_DPad_Down) return LOCTEXT("DPadDown", "D-pad Down");
    if (Key == EKeys::Gamepad_DPad_Left) return LOCTEXT("DPadLeft", "D-pad Left");
    if (Key == EKeys::Gamepad_DPad_Right) return LOCTEXT("DPadRight", "D-pad Right");
    return Key.GetDisplayName();
}

FString FKbBindingSet::ToText() const
{
    FString Text = FString(KbFileHeader) + TEXT("\n");
    for (int32 Index = 0; Index < KbActionCount; ++Index)
    {
        const EKbAction Action = static_cast<EKbAction>(Index);
        for (int32 Slot = 0; Slot < KbDeviceCount; ++Slot)
        {
            const EKbDevice Device = static_cast<EKbDevice>(Slot);
            if (IsDefault(Action, Device))
            {
                continue;
            }
            const FKey Key = Get(Action, Device);
            Text += FString::Printf(TEXT("%s.%s=%s\n"), KbInfo(Action).Id, Device == EKbDevice::Keyboard ? KbKeyboardTag : KbGamepadTag,
                                    Key.IsValid() ? *Key.ToString() : KbNoneValue);
        }
    }
    return Text;
}

bool FKbBindingSet::FromText(const FString& Text)
{
    *this = FKbBindingSet();
    TArray<FString> Lines;
    Text.ParseIntoArrayLines(Lines);
    bool bHeader = false;
    for (FString Line : Lines)
    {
        Line.TrimStartAndEndInline();
        if (Line.IsEmpty() || Line.StartsWith(TEXT("#")))
        {
            continue;
        }
        if (!bHeader)
        {
            if (Line != KbFileHeader)
            {
                return false;
            }
            bHeader = true;
            continue;
        }
        int32 Equals = INDEX_NONE;
        int32 Dot = INDEX_NONE;
        if (!Line.FindChar(TEXT('='), Equals) || !Line.Left(Equals).FindLastChar(TEXT('.'), Dot))
        {
            continue;
        }
        const FString Name = Line.Left(Equals).TrimEnd();
        const FString Value = Line.Mid(Equals + 1).TrimStart();
        const FString DeviceTag = Name.Mid(Dot + 1);
        EKbAction Action = EKbAction::Count;
        if (!KbFindAction(Name.Left(Dot), Action) || (DeviceTag != KbKeyboardTag && DeviceTag != KbGamepadTag))
        {
            continue;
        }
        const EKbDevice Device = DeviceTag == KbKeyboardTag ? EKbDevice::Keyboard : EKbDevice::Gamepad;
        if (Value == KbNoneValue)
        {
            Clear(Action, Device);
            continue;
        }
        const FKey Key{FName(*Value)};
        if (Judge(Action, Device, Key) == EKbSetResult::Applied)
        {
            SetSlot(Action, Device, Key);
        }
    }
    if (!bHeader)
    {
        return false;
    }
    Normalize();
    return true;
}

FKbBindingStore::FKbBindingStore(const FString& InPath)
    : Path(InPath)
{
}

FString FKbBindingStore::DefaultPath()
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PerfectWorld"), TEXT("KeyBindings.txt")));
}

FKbBindingStore& FKbBindingStore::Get()
{
    static FKbBindingStore Instance(DefaultPath());
    return Instance;
}

const FKbBindingSet& FKbBindingStore::Current()
{
    if (!bLoaded)
    {
        Load();
    }
    return Bindings;
}

bool FKbBindingStore::Load()
{
    bLoaded = true;
    Bindings = FKbBindingSet();
    FString Text;
    return FFileHelper::LoadFileToString(Text, *Path) && Bindings.FromText(Text);
}

bool FKbBindingStore::Save() const
{
    IFileManager& Files = IFileManager::Get();
    if (Bindings.IsEveryDefault())
    {
        // Nothing to remember: leave no file behind, so a reset really is a reset.
        return !Files.FileExists(*Path) || Files.Delete(*Path, false, true, true);
    }
    Files.MakeDirectory(*FPaths::GetPath(Path), true);
    return FFileHelper::SaveStringToFile(Bindings.ToText(), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void FKbBindingStore::Commit(const FKbBindingSet& Updated)
{
    Current();
    if (Updated == Bindings)
    {
        return;
    }
    Bindings = Updated;
    Save();
    OnChanged.Broadcast();
}

EKbSetResult FKbBindingStore::Rebind(EKbAction Action, EKbDevice Device, const FKey& Key, bool bSwap, EKbAction* OutOther)
{
    FKbBindingSet Updated = Current();
    const EKbSetResult Result = Updated.Set(Action, Device, Key, bSwap, OutOther);
    if (Result == EKbSetResult::Applied || Result == EKbSetResult::Swapped)
    {
        Commit(Updated);
    }
    return Result;
}

EKbSetResult FKbBindingStore::Unbind(EKbAction Action, EKbDevice Device)
{
    FKbBindingSet Updated = Current();
    const EKbSetResult Result = Updated.Clear(Action, Device);
    if (Result == EKbSetResult::Applied)
    {
        Commit(Updated);
    }
    return Result;
}

void FKbBindingStore::ResetAction(EKbAction Action)
{
    FKbBindingSet Updated = Current();
    for (int32 Slot = 0; Slot < KbDeviceCount; ++Slot)
    {
        const EKbDevice Device = static_cast<EKbDevice>(Slot);
        EKbAction Other = EKbAction::Count;
        if (Updated.ResetSlot(Action, Device, true, &Other) == EKbSetResult::NotAllowed)
        {
            // The action that took the default key cannot take this one's current key, so it loses the key instead.
            if (Updated.FindHolder(Action, Device, FKbBindingSet::DefaultKey(Action, Device), Other))
            {
                Updated.Clear(Other, Device);
                Updated.ResetSlot(Action, Device, false);
            }
        }
    }
    Commit(Updated);
}

void FKbBindingStore::ResetAll()
{
    FKbBindingSet Updated = Current();
    Updated.ResetAll();
    Commit(Updated);
}

#undef LOCTEXT_NAMESPACE
