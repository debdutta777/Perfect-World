#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "PerfectWorldInteractable.h"
#include "PerfectWorldDoorBell.generated.h"

class APWDoorActor;
class USceneComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * The calling bell (order F5). Fixed numbers, centimetres and seconds. The rules are the portable core's (DoorBell.h over Doors.h);
 * everything here is presentation and the two small geometry questions the presentation needs.
 */
namespace PWDoorBell
{
/** Where the button sits: beside the frame on the latch side, at about chest height, on the OUTSIDE wall face. */
constexpr float LatchSideAcrossCm = 111.0f;   // clear opening 94 + trim 5 + 12 cm of wall beyond the jamb, from the hinge
constexpr float HingeSideAcrossCm = -17.0f;   // the other side of the frame, used when the latch side has no clear wall
constexpr float HeightCm = 120.0f;
constexpr float FaceFromHingeLineCm = 12.5f;  // a 25 cm wall: its face is this far from the hinge line (refined by a trace)
constexpr float FreeFrontCm = 15.0f;          // the wall face must have this much clear air in front of the button
constexpr float PlateWidthCm = 11.0f;
constexpr float PlateHeightCm = 16.0f;
constexpr float PlateDepthCm = 2.5f;
constexpr float ButtonSizeCm = 5.0f;
constexpr int32 InteractionPriority = 12;     // above doors (10), below seats (15) and people (20)
constexpr float PulseSeconds = 0.35f;
constexpr float PulseDepthCm = 1.2f;

/**
 * The threshold guard: the player's position in a door's CLOSED-frame coordinates. Across runs along the closed leaf from the
 * hinge, Depth is signed distance from the hinge line, positive on the dwelling (inside) side, Up is height above the door's floor.
 */
constexpr float GuardNearAcrossMinCm = -45.0f;
constexpr float GuardNearAcrossMaxCm = 150.0f;
constexpr float GuardNearDepthCm = 220.0f;
constexpr float GuardNearUpMinCm = -30.0f;
constexpr float GuardNearUpMaxCm = 260.0f;
/** A sample whose centre is nearer the hinge line than this (still on the outside) already counts as "in the doorway". */
constexpr float GuardCrossMarginCm = 8.0f;
/** Across span of the doorway itself, with a little slack for the jamb trim: only a crossing inside it is a doorway crossing. */
constexpr float GuardDoorwayAcrossMinCm = -10.0f;
constexpr float GuardDoorwayAcrossMaxCm = 105.0f;
/** A move longer than this between two samples is a teleport (a load, a placement), never a walk through the doorway. */
constexpr float GuardMaxWalkStepCm = 150.0f;
constexpr float GuardNoticeCooldownSeconds = 1.5f;

struct FThresholdProbe
{
    float Across = 0.0f;
    float Depth = 0.0f;
    float Up = 0.0f;
    bool bNear = false;
};

/** A world point in a door's frame. InsideSide is the authored inside half-plane: +1 toward the actor's local +Y, -1 toward -Y. */
PERFECTWORLD_API FThresholdProbe ProbeThreshold(const FTransform& DoorTransform, double InsideSide, const FVector& WorldPoint);

/** True when the walk from Previous to Now goes from the outside to the inside through the doorway (not a teleport). */
PERFECTWORLD_API bool IsInwardCrossing(const FThresholdProbe& Previous, const FThresholdProbe& Now, float StepCm);

/** World-space unit vector pointing away from the dwelling at this door (where a visitor stands). */
PERFECTWORLD_API FVector OutsideDirection(const FTransform& DoorTransform, double InsideSide);

/** The nominal button position on the outside wall face, `AcrossCm` along the closed leaf from the hinge. */
PERFECTWORLD_API FVector NominalBellPoint(const FTransform& DoorTransform, double InsideSide, float AcrossCm);

/** The bell's facing: the yaw of OutsideDirection, in degrees (the bell's local +X points at the visitor). */
PERFECTWORLD_API double BellYawDegrees(const FVector& Outside);

/** Spawns the bell for a Private dwelling entrance (no-op for any other door, or when one already hangs). Returns it, or null. */
PERFECTWORLD_API APWDoorBellActor* SpawnBellFor(APWDoorActor& Door);
} // namespace PWDoorBell

/**
 * The bell button beside a dwelling's front door: a small plate and a push button with a "BELL" label, block visuals only. The
 * interact key on it rings the bell through UPWLaneLifeSubsystem::RingDoorBell, which asks the core's own knock; it performs no rule of its own.
 * There is no sound system in the project yet, so the ring is a visible pulse (the button sinks and warms for a third of a second;
 * with reduced motion it only warms) and the notice "You ring the bell.".
 */
UCLASS()
class PERFECTWORLD_API APWDoorBellActor : public AActor, public IPWInteractable
{
    GENERATED_BODY()

public:
    APWDoorBellActor();

    /** The door this bell belongs to (a stable lane door id). */
    void Configure(FName InDoorId);
    FName GetDoorId() const { return DoorId; }

    /** The button animation; harmless to call again while it plays. */
    void Pulse();
    bool IsPulsing() const { return PulseRemaining > 0.0f; }

    // IPWInteractable
    virtual FText GetInteractionVerb(const APawn* By) const override;
    virtual FVector GetInteractionPoint() const override;
    virtual int32 GetInteractionPriority() const override { return PWDoorBell::InteractionPriority; }
    virtual void Interact(APawn* By) override;

    virtual void Tick(float DeltaSeconds) override;

private:
    void ApplyPulse(float Fraction);

    UPROPERTY()
    TObjectPtr<USceneComponent> Root;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Plate;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Button;
    UPROPERTY()
    TObjectPtr<UTextRenderComponent> Label;
    UPROPERTY()
    TObjectPtr<UMaterialInterface> BaseMaterial;
    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> PlatePaint;
    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> ButtonPaint;

    FName DoorId;
    float PulseRemaining = 0.0f;
};

/**
 * The passage trigger at a private door's threshold (order F5). An open leaf never grants permission, and nothing closes a leaf
 * behind a visitor, so a walk through an open doorway has to be stopped by the doorway itself. Every sample the component asks
 * where the player is in the door's closed-frame coordinates; when the player crosses from outside to inside it asks the lane-life
 * subsystem, which asks the core (EvaluateDoorPassage, direction Entry). A refusal puts the player back where they stood outside and
 * shows the polite notice. Going out is never checked. Only the player is guarded (people are not walked by this component) and
 * only doors the core calls Private.
 */
UCLASS(ClassGroup = (PerfectWorld))
class PERFECTWORLD_API UPWDoorGuardComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPWDoorGuardComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** How many times this guard has put the player back (acceptance and tests read it). */
    int32 GetBounceCount() const { return BounceCount; }

private:
    PWDoorBell::FThresholdProbe Previous;
    FVector PreviousLocation = FVector::ZeroVector;
    FVector LastOutside = FVector::ZeroVector;
    bool bHavePrevious = false;
    bool bHaveOutside = false;
    /** 0 unknown, 1 guarded (a Private door), -1 not guarded. */
    int8 GuardState = 0;
    double NextNoticeSeconds = 0.0;
    int32 BounceCount = 0;
};
