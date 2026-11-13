#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PerfectWorldInteractable.generated.h"

class APawn;

UINTERFACE(MinimalAPI)
class UPWInteractable : public UInterface { GENERATED_BODY() };

/** Something the player can act on with the interact key: a door, a person. C++ only (no Blueprint events). */
class PERFECTWORLD_API IPWInteractable
{
    GENERATED_BODY()
public:
    /** Verb shown in the prompt, e.g. "Open", "Close", "Talk to Farida". Empty means not currently interactable. */
    virtual FText GetInteractionVerb(const APawn* By) const = 0;
    /** World point the targeting cone and the 2 m reach are measured to. */
    virtual FVector GetInteractionPoint() const = 0;
    /** Higher wins when several targets overlap (doors 10, people 20). */
    virtual int32 GetInteractionPriority() const = 0;
    /** Perform the action. */
    virtual void Interact(APawn* By) = 0;
};
