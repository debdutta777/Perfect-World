#pragma once

#include "CoreMinimal.h"

class FKbBindingSet;
class UInputAction;
class UInputMappingContext;
class UObject;

/**
 * The input actions the walking player's mapping context is made of. Look and LookStick are fixed (mouse and right stick); every
 * other action's keys come from the binding set. An action left null is simply not mapped, so a new action (Map, Quests) is added by
 * creating it, setting its field here, and binding its handler; its keys are already in the binding set.
 */
struct FKbContextActions
{
    const UInputAction* Move = nullptr;       // Axis2D: X right, Y forward
    const UInputAction* Look = nullptr;       // Axis2D: the mouse
    const UInputAction* LookStick = nullptr;  // Axis2D: the right stick
    const UInputAction* Run = nullptr;
    const UInputAction* Jump = nullptr;
    const UInputAction* ToggleCamera = nullptr;
    const UInputAction* ZoomWheel = nullptr;  // Axis1D, +1 zooms in; the wheel and any wheel key
    const UInputAction* ZoomHeld = nullptr;   // Axis1D, +1 zooms in; held keys
    const UInputAction* Interact = nullptr;
    const UInputAction* CycleTarget = nullptr;
    const UInputAction* Map = nullptr;
    const UInputAction* Quests = nullptr;
    const UInputAction* Inventory = nullptr; // R88
};

/**
 * Replaces everything in Context with the mappings the binding set describes. Safe to call on a context that is already added to a
 * player: change it, then ask the Enhanced Input subsystem to rebuild its control mappings. Modifier objects are created under ModifierOuter.
 */
void KbPopulateWalkingContext(UInputMappingContext& Context, const FKbContextActions& Actions, UObject& ModifierOuter,
                              const FKbBindingSet& Bindings);
