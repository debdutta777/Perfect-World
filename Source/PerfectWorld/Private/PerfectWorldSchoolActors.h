#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PerfectWorldInteractable.h"

#include "PerfectWorldSchoolActors.generated.h"

class USceneComponent;

/**
 * S03: the player's desk in Nabapara Vidyalaya's classroom, "Take your seat". Interact opens the school-day panel through UPWSchoolPresenter
 * (whether a lesson can open is the core's: the panel explains every refusal). The actor decides nothing. Priority 15, as the gate's register.
 */
UCLASS()
class APWSchoolSeatActor : public AActor, public IPWInteractable
{
    GENERATED_BODY()

public:
    APWSchoolSeatActor();
    void Configure(const FVector& PointWorld);
    int32 GetOpenCount() const { return OpenCount; }
    /** True when the last Interact opened the day panel. */
    bool DidOpenPanel() const { return bOpenedPanel; }

    virtual FText GetInteractionVerb(const APawn* By) const override;
    virtual FVector GetInteractionPoint() const override { return GetActorLocation(); }
    virtual int32 GetInteractionPriority() const override { return 15; }
    virtual void Interact(APawn* By) override;

private:
    UPROPERTY()
    TObjectPtr<USceneComponent> Root;
    int32 OpenCount = 0;
    bool bOpenedPanel = false;
};

/** S03: the study desk at home (the opening home's sleeping and study room), "Study at the desk". Interact opens the study desk panel. */
UCLASS()
class APWStudyDeskActor : public AActor, public IPWInteractable
{
    GENERATED_BODY()

public:
    APWStudyDeskActor();
    void Configure(const FVector& PointWorld);
    bool DidOpenPanel() const { return bOpenedPanel; }

    virtual FText GetInteractionVerb(const APawn* By) const override;
    virtual FVector GetInteractionPoint() const override { return GetActorLocation(); }
    virtual int32 GetInteractionPriority() const override { return 12; }
    virtual void Interact(APawn* By) override;

private:
    UPROPERTY()
    TObjectPtr<USceneComponent> Root;
    bool bOpenedPanel = false;
};
