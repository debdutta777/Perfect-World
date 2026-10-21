#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PerfectWorldInteractable.h"
#include "PerfectWorldDoorActor.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMeshComponent;
class UPWDoorGuardComponent;

/** Fixed numbers for a hinged door leaf, centimetres and seconds (ASSET_PIPELINE.md: doors 90 x 210). */
namespace PWDoor
{
constexpr float LeafWidth = 90.0f;
constexpr float LeafHeight = 210.0f;
constexpr float LeafThickness = 4.0f;
constexpr float LeafLift = 1.0f;             // the leaf hangs 1 cm clear of the floor
constexpr float OpenAngleDegrees = 95.0f;
constexpr float OpenSeconds = 0.45f;

/** The leaf tip never moves more than this between two obstruction checks (centimetres). */
constexpr float MaxTipStepCm = 5.0f;
/** The largest angular sub-step that keeps the tip (LeafWidth from the hinge) within MaxTipStepCm: 5 / 90 rad = 3.183 degrees. */
constexpr float MaxSubStepDegrees = MaxTipStepCm / LeafWidth * 57.2957795f;
/** Below this many degrees from closed the leaf counts as closed. Anything more counts as open for collision and for the core. */
constexpr float ClosedToleranceDegrees = 0.25f;

/** Number of obstruction checks needed to cover the swing between two signed angles (always at least 1). */
inline int32 SubStepCount(float FromAngle, float ToAngle)
{
    return FMath::Max(1, FMath::CeilToInt(FMath::Abs(ToAngle - FromAngle) / MaxSubStepDegrees));
}

/** -1, 0 (closed) or +1: the side of the hinge line the leaf is on at a signed angle. */
inline int8 SwingSideOf(float Angle)
{
    return FMath::Abs(Angle) <= ClosedToleranceDegrees ? static_cast<int8>(0) : (Angle > 0.0f ? static_cast<int8>(1) : static_cast<int8>(-1));
}
} // namespace PWDoor

/**
 * Raised once when a door's motion ends, whether it reached its target or was stopped by an obstruction.
 * DoorId; bReachedTarget (false: stopped short because someone or something was in the swing); bIsOpenNow (true for any
 * angle beyond PWDoor::ClosedToleranceDegrees, so a leaf stopped part-way counts as open); SwingSide (-1 or +1 while open, 0 closed).
 * Not raised by SetOpenImmediate or when TryMoveTo returns false (nothing moved).
 */
DECLARE_MULTICAST_DELEGATE_FourParams(FPWDoorSettled, FName /*DoorId*/, bool /*bReachedTarget*/, bool /*bIsOpenNow*/, int8 /*SwingSide*/);

/**
 * A hinged block door. The actor's location is the hinge axis at floor level and its yaw points along the closed leaf
 * (+X); the leaf is a 90 x 210 x 4 cube with its own collision (Pawn, Camera, Visibility), swung about the hinge in
 * about 0.45 s with an eased curve to 95 degrees.
 *
 * It never pushes anyone. Before it starts, the whole swing is tested for anything that blocks the Pawn channel or any
 * pawn (people, furniture, walls); if something is in the way it stays where it is. While it moves, every rotation step
 * (however large the frame delta) is first checked in sub-steps that keep the leaf tip within PWDoor::MaxTipStepCm per
 * check; if a person steps into the swing, the leaf stops at the last safe angle and reports it through OnSettled with
 * bReachedTarget false. A leaf stopped part-way counts as open. A two-way door opens away from the person using it; a
 * one-way door (a front door) only swings to its preferred side.
 *
 * Gameplay authority belongs to the portable core. Interact() asks UPWLaneLifeSubsystem, which runs the core's door rule
 * (Doors.h); only when the core allows it does the subsystem call TryMoveTo, and it records the new state only if the leaf
 * could really move. The animation is presentation only. At BeginPlay the door registers with the subsystem, which sets its
 * start state from the core. When motion ends, OnSettled tells the subsystem the real outcome so the core can be reconciled.
 *
 * F5, the calling bell: a private dwelling's door hangs a bell beside it at BeginPlay (APWDoorBellActor), and carries a threshold
 * guard (UPWDoorGuardComponent). The door itself never rings: the interact key on a leaf that is not open to this visitor says
 * "Ring the bell first." and does not open it.
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

    /** +1 or -1: the side (local +Y or -Y) the leaf swings to by default; a dwelling leaf swings into the dwelling. */
    float GetPreferredSwing() const { return PreferredSwing; }

    /** The calling bell hung beside this door (F5, PerfectWorldDoorBell.h); null for a door that has none. */
    AActor* GetBellActor() const { return BellActor.Get(); }
    void SetBellActor(AActor* InBell) { BellActor = InBell; }

    /** The state being moved to (or held): true if open. */
    bool IsOpen() const { return bOpen; }

    /**
     * Sets the state at once with no animation, no obstruction test and no OnSettled (loading a save, placing at start).
     * SwingSide (-1 or +1) restores which way a two-way door was open; 0 means the preferred side. A one-way door always uses
     * its preferred side.
     */
    void SetOpenImmediate(bool bNewOpen, int8 SwingSide = 0);

    /** 0 closed .. 1 fully open, from the leaf's actual angle (so a leaf stopped part-way reads between). */
    float GetOpenFraction() const { return FMath::Clamp(FMath::Abs(CurrentAngle) / PWDoor::OpenAngleDegrees, 0.0f, 1.0f); }

    /** -1 or +1: the side the leaf is on while it is open at all; 0 when closed. */
    int8 GetSwingSide() const { return PWDoor::SwingSideOf(CurrentAngle); }

    /** The leaf's current signed angle in degrees (0 closed). */
    float GetCurrentAngleDegrees() const { return CurrentAngle; }

    /** Raised when motion ends (see FPWDoorSettled). The lane-life subsystem binds this and reconciles the core. */
    FPWDoorSettled OnSettled;

    /** True while the leaf is swinging. */
    bool IsMoving() const { return bMoving; }

    /**
     * Presentation only: swings the leaf to open (true) or closed (false), if the volume it sweeps is clear of anything
     * that blocks the Pawn channel. Returns false, and changes nothing, when something is in the way or the leaf is
     * already moving or already in that state (a leaf stopped part-way counts as open, so only "close" can start from there).
     * It does not ask the core; callers (UPWLaneLifeSubsystem) have already been allowed. When it returns true, OnSettled is
     * raised exactly once later, from Tick, with the real outcome.
     */
    bool TryMoveTo(bool bWantOpen, const APawn* By);

    // IPWInteractable
    virtual FText GetInteractionVerb(const APawn* By) const override;
    virtual FVector GetInteractionPoint() const override;
    virtual int32 GetInteractionPriority() const override { return 10; }
    virtual void Interact(APawn* By) override;

    virtual void PostInitializeComponents() override;
    virtual void BeginPlay() override;
    virtual void Destroyed() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    /** Starts moving to a signed angle (0 closed). */
    void BeginMove(float NewTargetAngle);
    void ApplyAngle();
    /** True if nothing that blocks the Pawn channel stands in the volume the leaf sweeps between the two signed angles. */
    bool IsSweepClear(float FromAngle, float ToAngle) const;
    /** True if the leaf posed at one signed angle overlaps anything that blocks the Pawn channel or any pawn. */
    bool IsPoseBlocked(float Angle) const;
    /**
     * Walks FromAngle -> ToAngle in sub-steps of at most PWDoor::MaxSubStepDegrees and returns the last angle whose pose is
     * clear (FromAngle itself when the very first step is blocked). bOutBlocked reports whether ToAngle was not reached.
     * bSkipStartPose accepts the current pose without testing it (it is where the leaf already is).
     */
    float FindFurthestSafeAngle(float FromAngle, float ToAngle, bool bSkipStartPose, bool& bOutBlocked) const;
    /** Ends the motion at CurrentAngle, reconciles the target flag with the real angle and raises OnSettled. */
    void Settle(bool bReachedTarget);
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
    /** F5: stops an un-admitted player crossing a private threshold (an open leaf alone never grants entry). */
    UPROPERTY()
    TObjectPtr<UPWDoorGuardComponent> Guard;
    /** F5: the bell beside a private dwelling's door, spawned at BeginPlay. */
    TWeakObjectPtr<AActor> BellActor;

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
