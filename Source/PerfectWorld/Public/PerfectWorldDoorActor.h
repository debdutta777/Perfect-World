#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PerfectWorldInteractable.h"
#include "PerfectWorldDoorActor.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMeshComponent;

/** Fixed numbers for a hinged door leaf, centimetres and seconds (ASSET_PIPELINE.md: doors 90 x 210). */
namespace PWDoor
{
constexpr float LeafWidth = 90.0f;
constexpr float LeafHeight = 210.0f;
constexpr float LeafThickness = 4.0f;
constexpr float LeafLift = 1.0f;             // the leaf hangs 1 cm clear of the floor
constexpr float OpenAngleDegrees = 95.0f;
constexpr float OpenSeconds = 0.45f;
} // namespace PWDoor

/**
 * A hinged block door. The actor's location is the hinge axis at floor level and its yaw points along the closed leaf
 * (+X); the leaf is a 90 x 210 x 4 cube with its own collision (Pawn, Camera, Visibility), swung about the hinge in
 * about 0.45 s with an eased curve to 95 degrees.
 *
 * It never pushes anyone: before it moves, the volume the leaf will sweep is tested for anything that blocks the Pawn
 * channel (people, furniture, walls). If something is in the way it stays where it is. A two-way door opens away from
 * the person using it; a one-way door (a front door) only swings to its preferred side.
 *
 * Gameplay authority belongs to the portable core. Interact() asks UPWLaneLifeSubsystem, which runs the core's door rule
 * (Doors.h); only when the core allows it does the subsystem call TryMoveTo, and it records the new state only if the leaf
 * could really move. The animation is presentation only. At BeginPlay the door registers with the subsystem, which sets its
 * start state from the core.
 */
UCLASS()
class PERFECTWORLD_API APWDoorActor : public AActor, public IPWInteractable
{
    GENERATED_BODY()

public:
    APWDoorActor();

    /** Stable id, e.g. door.nabapara.home_front. PreferredSwing is +1 or -1: the side (local +Y or -Y) the leaf swings to by default. */
    void Configure(FName InDoorId, float InPreferredSwing, bool bInTwoWay);

    FName GetDoorId() const { return DoorId; }

    /** The state being moved to (or held): true if open. */
    bool IsOpen() const { return bOpen; }

    /** Sets the state at once with no animation and no obstruction test (loading a save, placing at start). */
    void SetOpenImmediate(bool bNewOpen);

    /** True while the leaf is swinging. */
    bool IsMoving() const { return bMoving; }

    /**
     * Presentation only: swings the leaf to open (true) or closed (false), if the volume it sweeps is clear of anything
     * that blocks the Pawn channel. Returns false, and changes nothing, when something is in the way. It does not ask the
     * core; callers (UPWLaneLifeSubsystem) have already been allowed.
     */
    bool TryMoveTo(bool bWantOpen, const APawn* By);

    // IPWInteractable
    virtual FText GetInteractionVerb(const APawn* By) const override;
    virtual FVector GetInteractionPoint() const override;
    virtual int32 GetInteractionPriority() const override { return 10; }
    virtual void Interact(APawn* By) override;

    virtual void PostInitializeComponents() override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    /** Starts moving to a signed angle (0 closed). */
    void BeginMove(float NewTargetAngle);
    void ApplyAngle();
    /** True if nothing that blocks the Pawn channel stands in the volume the leaf sweeps between the two signed angles. */
    bool IsSweepClear(float FromAngle, float ToAngle) const;
    bool TryOpen(const APawn* By);
    bool TryClose();

    UPROPERTY()
    TObjectPtr<USceneComponent> HingeRoot;
    UPROPERTY()
    TObjectPtr<USceneComponent> Pivot;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Leaf;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> HandleInside;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> HandleOutside;
    UPROPERTY()
    TObjectPtr<UMaterialInterface> BaseMaterial;
    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> LeafPaint;
    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> HandlePaint;

    FName DoorId;
    float PreferredSwing = 1.0f;
    bool bTwoWay = true;
    bool bOpen = false;          // target state
    bool bMoving = false;
    float OpenSign = 1.0f;       // side the leaf is on (or moving to) while open
    float CurrentAngle = 0.0f;   // signed degrees
    float StartAngle = 0.0f;
    float TargetAngle = 0.0f;
    float Elapsed = 0.0f;
    float Duration = PWDoor::OpenSeconds;
};
