#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "UObject/WeakObjectPtrTemplates.h"

class SWidget;
class UPerfectWorldCharacterCreatorSubsystem;

/**
 * Builds the appearance editor screen: discrete pickers for face shape, hair
 * style, build and uniform variant, RGB sliders for skin/hair/eye colour, a
 * live text preview, and Back / Continue. Split out of
 * PerfectWorldMenuGameMode.cpp the same way PerfectWorldCreatorPanel.cpp is,
 * to keep both files under the project's line-count limit; the game mode
 * owns only screen switching and hands this function the creator subsystem
 * plus callbacks for moving to the next or previous screen.
 */
TSharedRef<SWidget> BuildAppearancePanel(TWeakObjectPtr<UPerfectWorldCharacterCreatorSubsystem> Creator,
                                          FSimpleDelegate OnContinue, FSimpleDelegate OnBack);
