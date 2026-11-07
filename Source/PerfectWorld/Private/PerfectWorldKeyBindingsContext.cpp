#include "PerfectWorldKeyBindingsContext.h"

#include "EnhancedActionKeyMapping.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "PerfectWorldKeyBindings.h"

namespace
{
/** Adds key mappings to one context, with the modifiers the walking player's actions have always used. */
struct FKbMapper
{
    UInputMappingContext& Context;
    UObject& Outer;

    void Map(const UInputAction* Action, const FKey& Key, bool bSwizzle = false, bool bNegate = false, bool bDeadZone = false) const
    {
        if (Action == nullptr || !Key.IsValid())
        {
            return;
        }
        FEnhancedActionKeyMapping& Mapping = Context.MapKey(Action, Key);
        if (bSwizzle)
        {
            Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(&Outer)); // X into Y
        }
        if (bNegate)
        {
            Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(&Outer));
        }
        if (bDeadZone)
        {
            UInputModifierDeadZone* Zone = NewObject<UInputModifierDeadZone>(&Outer);
            Zone->LowerThreshold = 0.2f;
            Zone->UpperThreshold = 1.0f;
            Zone->Type = EDeadZoneType::Radial;
            Mapping.Modifiers.Add(Zone);
        }
    }
};

/** One direction of zoom on the keyboard side: wheel keys feed the wheel action, any other key feeds the held action. */
void KbMapZoomKey(const FKbMapper& Mapper, const FKbContextActions& Actions, const FKey& Key, bool bZoomOut)
{
    if (KbIsWheelKey(Key))
    {
        Mapper.Map(Actions.ZoomWheel, Key, false, bZoomOut);
    }
    else
    {
        Mapper.Map(Actions.ZoomHeld, Key, false, bZoomOut);
    }
}
} // namespace

void KbPopulateWalkingContext(UInputMappingContext& Context, const FKbContextActions& Actions, UObject& ModifierOuter,
                              const FKbBindingSet& Bindings)
{
    Context.UnmapAll();
    const FKbMapper Mapper{Context, ModifierOuter};
    const auto Keyboard = [&Bindings](EKbAction Action) { return Bindings.Get(Action, EKbDevice::Keyboard); };
    const auto Pad = [&Bindings](EKbAction Action) { return Bindings.Get(Action, EKbDevice::Gamepad); };

    // Move: a key gives 1 on X (right) or Y (forward); swizzle moves X into Y, negate flips the sign. The left stick is fixed.
    Mapper.Map(Actions.Move, Keyboard(EKbAction::MoveForward), true);
    Mapper.Map(Actions.Move, Keyboard(EKbAction::MoveBack), true, true);
    Mapper.Map(Actions.Move, Keyboard(EKbAction::MoveLeft), false, true);
    Mapper.Map(Actions.Move, Keyboard(EKbAction::MoveRight));
    Mapper.Map(Actions.Move, EKeys::Gamepad_Left2D, false, false, true);

    // Look: the mouse delta, and the right stick as a rate. Not rebindable.
    Mapper.Map(Actions.Look, EKeys::Mouse2D);
    Mapper.Map(Actions.LookStick, EKeys::Gamepad_Right2D, false, false, true);

    for (int32 Slot = 0; Slot < KbDeviceCount; ++Slot)
    {
        const EKbDevice Device = static_cast<EKbDevice>(Slot);
        const auto Key = [&Bindings, Device](EKbAction Action) { return Bindings.Get(Action, Device); };
        Mapper.Map(Actions.Run, Key(EKbAction::Run));
        Mapper.Map(Actions.Jump, Key(EKbAction::Jump));
        Mapper.Map(Actions.ToggleCamera, Key(EKbAction::ToggleCamera));
        Mapper.Map(Actions.Interact, Key(EKbAction::Interact));
        Mapper.Map(Actions.CycleTarget, Key(EKbAction::CycleTarget));
        Mapper.Map(Actions.Map, Key(EKbAction::Map));
        Mapper.Map(Actions.Quests, Key(EKbAction::Quests));
        Mapper.Map(Actions.Inventory, Key(EKbAction::Inventory)); // R88
    }

    // Zoom. The usual wheel (up closer, down further) is the wheel axis itself, which can deliver several notches in a frame; a wheel
    // pair swapped round is the same axis negated. Anything else is mapped key by key: a wheel key counts one notch per scroll event,
    // a held key moves the camera at a steady rate.
    const FKey ZoomInKey = Keyboard(EKbAction::ZoomIn);
    const FKey ZoomOutKey = Keyboard(EKbAction::ZoomOut);
    if (ZoomInKey == EKeys::MouseScrollUp && ZoomOutKey == EKeys::MouseScrollDown)
    {
        Mapper.Map(Actions.ZoomWheel, EKeys::MouseWheelAxis);
    }
    else if (ZoomInKey == EKeys::MouseScrollDown && ZoomOutKey == EKeys::MouseScrollUp)
    {
        Mapper.Map(Actions.ZoomWheel, EKeys::MouseWheelAxis, false, true);
    }
    else
    {
        KbMapZoomKey(Mapper, Actions, ZoomInKey, false);
        KbMapZoomKey(Mapper, Actions, ZoomOutKey, true);
    }
    Mapper.Map(Actions.ZoomHeld, Pad(EKbAction::ZoomIn));
    Mapper.Map(Actions.ZoomHeld, Pad(EKbAction::ZoomOut), false, true);
}
