#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "UObject/WeakObjectPtrTemplates.h"

class SWidget;
class UPerfectWorldCharacterCreatorSubsystem;

/**
 * Builds the character creation panel: household picker, trait rows, budget
 * display, Confirm Origin / New Draft, per-slot Save buttons, and a Back to
 * Title button. Split out of PerfectWorldMenuGameMode.cpp to keep both files
 * under the project's line-count limit; the game mode owns only screen
 * switching and hands this function the creator subsystem and a callback for
 * returning to the title screen.
 */
TSharedRef<SWidget> BuildCreatorPanel(TWeakObjectPtr<UPerfectWorldCharacterCreatorSubsystem> Creator,
                                       FSimpleDelegate OnBackToTitle);
