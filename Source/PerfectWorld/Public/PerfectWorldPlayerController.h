#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PerfectWorldPlayerController.generated.h"

/**
 * The player controller for the walking player (ROADMAP M1-13).
 *
 * Its only job today is to make sure the controller reads input through Enhanced Input whatever the project's input
 * settings say. The project's Config/DefaultInput.ini does not name the Enhanced classes (and must not be edited by
 * agents), so without this the Enhanced Input subsystem would find a plain UPlayerInput and ignore every mapping.
 * Menus drawn by the front end HUD use ordinary Slate and legacy key bindings, which the Enhanced player input still
 * processes, so nothing else changes.
 */
UCLASS()
class PERFECTWORLD_API APerfectWorldPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    APerfectWorldPlayerController();
};
