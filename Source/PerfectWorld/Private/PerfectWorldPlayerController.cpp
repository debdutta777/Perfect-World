#include "PerfectWorldPlayerController.h"

#include "EnhancedPlayerInput.h"

APerfectWorldPlayerController::APerfectWorldPlayerController()
{
    // Used by InitInputSystem instead of the Input Settings' DefaultPlayerInputClass.
    OverridePlayerInputClass = UEnhancedPlayerInput::StaticClass();
}
