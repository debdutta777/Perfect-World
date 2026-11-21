#pragma once

#include "CoreMinimal.h"
class SWidget;
DECLARE_DELEGATE_OneParam(FPWWidgetDelegate, TSharedPtr<SWidget>);

/** The step numbers of the creation flow, in order. */
namespace PWUI
{
enum class ECreationStep : int32
{
    Household = 0,
    Traits = 1,
    Identity = 2,
    Stream = 3,
    Review = 4
};
constexpr int32 CreationStepCount = 5;
} // namespace PWUI

/** An action that can be refused: returns empty text on success, otherwise the message to show the player. */
DECLARE_DELEGATE_RetVal(FText, FPWTryDelegate);

/** A request to go to one step of the creation flow. */
DECLARE_DELEGATE_OneParam(FPWStepDelegate, int32);

/** A request that names a save slot and can be refused: empty text on success, otherwise the message. */
DECLARE_DELEGATE_RetVal_OneParam(FText, FPWSlotDelegate, int32);
