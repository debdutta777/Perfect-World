#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PerfectWorldInteractable.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldFurnitureActor.generated.h"

class USceneComponent;

namespace PWLane
{
struct FHomeSeatSpec;
struct FHomeReadableSpec;
struct FLaneFrame;
} // namespace PWLane

/** Numbers shared by the furniture actors, centimetres. */
namespace PWFurniture
{
constexpr int32 InteractionPriority = 15;   // above doors (10), below people (20)
constexpr int32 StandUpPriority = 25;       // the seat the player is sitting on: E again stands up, ahead of people (20)
constexpr float SeatPromptAboveSeatCm = 55.0f;  // the prompt point is this far over the cushion, above a chair back, so it can be seen
constexpr float ReadablePromptAboveCm = 12.0f;  // ... and this far over the sheet
} // namespace PWFurniture

/**
 * A place to sit. The actor stands at the seat centre ON THE FLOOR and its yaw points the way a seated person faces; it has
 * no mesh and no collision (the furniture is lane geometry). Interact sits the player down through
 * APerfectWorldPlayerCharacter::TrySit, or stands them up when they are already on it. The verb is the spec's ("Sit on the
 * divan", "Sit at the table"), "Stand up" while the viewer is seated here, empty while they are seated elsewhere or gliding.
 * Priority 15; 25 while the viewer is seated here.
 */
UCLASS()
class PERFECTWORLD_API APWSeatActor : public AActor, public IPWInteractable
{
    GENERATED_BODY()

public:
    APWSeatActor();

    /** All world values: SeatFloor is the seat centre at floor level, StandFloor a floor point to stand up onto, Facing horizontal. */
    void Configure(FName InSeatId, const FText& InVerb, const FVector& SeatFloor, const FVector& Facing, float InSeatHeightCm,
                   const FVector& StandFloor, float InStandClearCm);
    /** From a building-local seat spec (PerfectWorldLaneHomeFurniture.h) and the building's frame. */
    void ConfigureFromSpec(const PWLane::FHomeSeatSpec& Spec, const PWLane::FLaneFrame& Frame);

    FName GetSeatId() const { return SeatId; }
    float GetSeatHeightCm() const { return SeatHeightCm; }
    /** What the player character is given. */
    FPWSeatPlacement MakePlacement() const;

    // IPWInteractable
    virtual FText GetInteractionVerb(const APawn* By) const override;
    virtual FVector GetInteractionPoint() const override;
    virtual int32 GetInteractionPriority() const override;
    virtual void Interact(APawn* By) override;

private:
    UPROPERTY()
    TObjectPtr<USceneComponent> Root;

    FName SeatId;
    FText Verb;
    float SeatHeightCm = 45.0f;
    FVector StandFloor = FVector::ZeroVector;
    float StandClearCm = 45.0f;
    TWeakObjectPtr<const APawn> Occupant; // the player who last sat here; only meaningful while that player is seated on this actor
};

/** Raised by an APWReadableActor when the player acts on it: the readable's id, the id of the text to show, and its title. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FPWReadRequested, FName /*ReadableId*/, FName /*ContentId*/, FText /*Title*/);

/**
 * Something to read (the newspaper on the table). The actor stands at the centre of the sheet. Interact does not show the
 * text: it raises OnReadRequested (and then the world-wide GlobalReadRequested()) for the reading presenter, which owns the
 * modal. When neither is bound it only logs. Priority 15.
 */
UCLASS()
class PERFECTWORLD_API APWReadableActor : public AActor, public IPWInteractable
{
    GENERATED_BODY()

public:
    APWReadableActor();

    void Configure(FName InReadableId, FName InContentId, const FText& InVerb, const FText& InTitle, const FVector& SheetWorld,
                   const FVector& InStandFloor);
    /** Title empty: a generic one. */
    void ConfigureFromSpec(const PWLane::FHomeReadableSpec& Spec, const PWLane::FLaneFrame& Frame, const FText& InTitle = FText::GetEmpty());

    FName GetReadableId() const { return ReadableId; }
    FName GetContentId() const { return ContentId; }
    FText GetTitle() const { return Title; }
    /** The floor point the layout suggests reading from. */
    FVector GetStandPoint() const { return StandFloor; }

    /** Per-actor delegate. */
    FPWReadRequested OnReadRequested;
    /** Raised after the per-actor one, for a presenter that does not hold the actors. */
    static FPWReadRequested& GlobalReadRequested();

    // IPWInteractable
    virtual FText GetInteractionVerb(const APawn* By) const override;
    virtual FVector GetInteractionPoint() const override;
    virtual int32 GetInteractionPriority() const override { return PWFurniture::InteractionPriority; }
    virtual void Interact(APawn* By) override;

private:
    UPROPERTY()
    TObjectPtr<USceneComponent> Root;

    FName ReadableId;
    FName ContentId;
    FText Verb;
    FText Title;
    FVector StandFloor = FVector::ZeroVector;
};
