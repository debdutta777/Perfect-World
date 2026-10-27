#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PerfectWorldPlayerCharacter.generated.h"

class UCameraComponent;
class UEnhancedInputComponent;
class UInputAction;
class UInputComponent;
class UInputMappingContext;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPWInteractionComponent;
class USpringArmComponent;
class UStaticMeshComponent;
struct FCollisionQueryParams;
struct FInputActionValue;

/** The two ways the player sees the world. Switching never moves or turns the character. */
enum class EPWCameraMode : uint8
{
    FirstPerson,
    ThirdPerson
};

/** Fixed numbers for the walking player, in centimetres and seconds. The 16-year-old is 165 cm until the appearance record has a height. */
namespace PWPlayer
{
constexpr float CapsuleRadius = 34.0f;           // ASSET_INTEGRATION.md: capsule sweeps use 34 cm
constexpr float CapsuleHalfHeight = 82.5f;       // a 165 cm tall standing body
constexpr float EyeHeightAboveFloor = 155.0f;    // first-person camera height above the floor
constexpr float WalkSpeed = 140.0f;              // 1.4 m/s
constexpr float RunSpeed = 400.0f;               // 4 m/s, a brisk jog
constexpr float MaxStepHeight = 45.0f;           // clears the 15 cm kerb and 17.5 cm stairs
constexpr float JumpSpeed = 420.0f;              // about 90 cm of height
constexpr float BoomLengthDefault = 300.0f;      // third person: boom length
constexpr float BoomLengthMin = 120.0f;
constexpr float BoomLengthMax = 600.0f;
constexpr float BoomProbeSize = 12.0f;           // ASSET_PIPELINE.md: 12 cm probe, pulls in without clipping
} // namespace PWPlayer

/** The seated state of the walking player. Entering and Leaving are the two 0.4 s glides. */
enum class EPWSeatState : uint8
{
    Standing,
    Entering,
    Seated,
    Leaving
};

/** Why a request to sit was not taken up. A notice is shown to the player for every refusal except Busy. */
enum class EPWSitResult : uint8
{
    Started,    // the glide to the seat has begun
    Busy,       // already seated, or already gliding
    NotReady,   // a menu or conversation is open, or the player is not standing on the ground
    WrongLevel, // the seat is on another floor level than the player's feet
    Occupied,   // another person is at the seat
    Obstructed  // something other than the furniture itself is in the way
};

/**
 * Seated numbers, centimetres and seconds. The seated capsule is a 24 cm radius, 82 cm tall capsule whose BOTTOM rests on the
 * top of the seat, so it never reaches into the furniture. The first-person eye sits 70 cm above the seat top (a 165 cm person's
 * sitting eye height): 115 cm above the floor for the 45 cm table chair, 112 for the 42 cm divan. The camera is 8 cm in front of the
 * capsule axis, as when standing; relative to the seated capsule centre it is 29 cm up.
 */
namespace PWSeat
{
constexpr float CapsuleRadius = 24.0f;
constexpr float CapsuleHalfHeight = 41.0f;
constexpr float EyeAboveSeatCm = 70.0f;
constexpr float EyeRelativeZ = EyeAboveSeatCm - CapsuleHalfHeight; // 29
constexpr float GlideSeconds = 0.4f;
constexpr float LookYawLimitDegrees = 105.0f;   // either side of the way the seat faces
constexpr float LookUpLimitDegrees = 55.0f;
constexpr float LookDownLimitDegrees = 60.0f;
constexpr float BodyRadius = 16.0f;             // the torso volume swept to and from the seat
constexpr float BodyHalfHeight = 26.0f;
constexpr float BodyAboveSeatCm = 2.0f;
constexpr float FootprintCm = 50.0f;            // furniture within this of the seat axis is the seat itself, not an obstruction
constexpr float MaxFloorDifferenceCm = 40.0f;
constexpr float MoveInputThreshold = 0.3f;

inline float SmoothStep(float T)
{
    const float C = FMath::Clamp(T, 0.0f, 1.0f);
    return C * C * (3.0f - 2.0f * C);
}
/** The look yaw kept within LookYawLimitDegrees of the facing; both in degrees, any range. */
inline float ClampLookYaw(float YawDegrees, float FacingYawDegrees)
{
    const float Delta = FMath::Clamp(FMath::FindDeltaAngleDegrees(FacingYawDegrees, YawDegrees), -LookYawLimitDegrees, LookYawLimitDegrees);
    return FRotator::NormalizeAxis(FacingYawDegrees + Delta);
}
inline float ClampLookPitch(float PitchDegrees)
{
    return FMath::Clamp(FRotator::NormalizeAxis(PitchDegrees), -LookDownLimitDegrees, LookUpLimitDegrees);
}
/** World height of the first-person eye above the floor when seated on a seat of this height. */
inline float SeatedEyeAboveFloor(float SeatHeightCm)
{
    return SeatHeightCm + EyeAboveSeatCm;
}
} // namespace PWSeat

/** Where a seat is, in the world: the seat actor fills this in. */
struct FPWSeatPlacement
{
    FVector SeatTop = FVector::ZeroVector;    // the centre of the seating surface (Z is the top of the cushion)
    FVector StandPoint = FVector::ZeroVector; // a floor point to stand up onto
    FVector Facing = FVector(1.0, 0.0, 0.0);  // horizontal: the way a seated person faces
    float SeatHeightCm = 45.0f;               // SeatTop.Z minus the floor
    float StandClearCm = 45.0f;
    float FootprintCm = PWSeat::FootprintCm;
};

/**
 * The player in the world (ROADMAP M1-13): a walking, running, jumping block character with a first-person camera
 * (default) and a third-person orbit camera, switched with one key and never changing position or facing.
 *
 * Camera: the first-person camera is attached to the capsule at eye height, never to a body part, and nothing moves
 * it but the controller's look rotation (no bob, no sway). The third-person camera sits on a spring arm that orbits
 * the same point, tests collision against the world and can be shortened or lengthened within limits. The body turns
 * with the camera yaw in both modes, so a toggle changes the view only. The block body is hidden from its owner in
 * first person (it still casts a shadow).
 *
 * Input: Enhanced Input, built in code (no input assets and no config edits). Exact bindings, all on the keyboard and
 * mouse or on a gamepad:
 *   Move      W A S D           | left stick
 *   Look      mouse             | right stick
 *   Run       hold Left Shift   | click the left stick (L3), held
 *   Jump      Space             | A / cross (bottom face button)
 *   Camera    V                 | Y / triangle (top face button)
 *   Zoom      mouse wheel       | D-pad up and down (third person only)
 *   Interact  E                 | X / square (left face button)
 *   Next target  Tab            | right shoulder button (RB / R1)
 * Interact and next-target are ignored while a menu or a conversation is open (UPWInteractionComponent::IsWorldInputBlocked),
 * and a held key does not repeat them. Leaving a conversation is Escape or B, handled by the conversation panel.
 * Pause stays on Escape and the controller menu button (the front end HUD owns it). If the project's input classes
 * are not the Enhanced ones, APerfectWorldPlayerController and this class's input component override them; if that
 * ever fails, equivalent legacy bindings are created at runtime and the log says which path ran.
 */
UCLASS()
class PERFECTWORLD_API APerfectWorldPlayerCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    APerfectWorldPlayerCharacter();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void NotifyControllerChanged() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    EPWCameraMode GetCameraMode() const { return CameraMode; }
    bool IsFirstPerson() const { return CameraMode == EPWCameraMode::FirstPerson; }

    /** Switches camera. Only the active camera and the visibility of the body change. */
    void SetCameraMode(EPWCameraMode NewMode);
    void ToggleCameraMode();

    /** Third person boom length in centimetres, clamped to PWPlayer::BoomLengthMin..Max. */
    void SetThirdPersonDistance(float Centimetres);
    float GetThirdPersonDistance() const;

    /**
     * Sitting (FurnitureActor.h calls this). Sweeps a torso capsule from here to the seat and refuses, with a notice, when
     * another person is at the seat or anything but the furniture is in the way; otherwise glides there in 0.4 s, turns to
     * the seat's facing, lowers the eye to PWSeat::EyeAboveSeatCm over the seat, blocks walking and limits looking to
     * +/-105 degrees. Third person shows a sitting block pose. StandUp (E on the seat again, any movement input once the
     * movement keys have been released, or Jump) puts the player on the stand point or, if that is blocked, the nearest
     * clear spot around it; with none, a notice and the player stays seated.
     */
    EPWSitResult TrySit(AActor* Seat, const FPWSeatPlacement& Placement);
    bool StandUp();
    EPWSeatState GetSeatState() const { return SeatState; }
    bool IsSeated() const { return SeatState != EPWSeatState::Standing; }
    bool IsSeatedOn(const AActor* Seat) const { return SeatState != EPWSeatState::Standing && SeatActor.Get() == Seat; }
    bool CanStandUp() const { return SeatState == EPWSeatState::Seated; }
    /** Shows a short message above the interaction prompt. */
    void ShowNotice(const FText& Message);
    /** World position of the first-person camera (the eye), whichever camera is active. */
    FVector GetEyeLocation() const;

    virtual void Jump() override;

    /** Look and zoom rates. M1-14 exposes these as settings. Mouse: a multiplier on the engine's mouse delta (1 = as delivered). */
    float MouseSensitivity = 1.0f;
    float StickLookDegreesPerSecond = 140.0f;
    float ZoomCentimetresPerWheelNotch = 40.0f;
    float ZoomCentimetresPerSecond = 220.0f;

protected:
    UPROPERTY(VisibleAnywhere, Category = "Camera")
    TObjectPtr<UCameraComponent> FirstPersonCamera;

    UPROPERTY(VisibleAnywhere, Category = "Camera")
    TObjectPtr<USpringArmComponent> BoomArm;

    UPROPERTY(VisibleAnywhere, Category = "Camera")
    TObjectPtr<UCameraComponent> ThirdPersonCamera;

    /** Finds what can be acted on (doors, people), draws the prompt, and acts on it. */
    UPROPERTY(VisibleAnywhere, Category = "Interaction")
    TObjectPtr<UPWInteractionComponent> Interaction;

    /** The block body parts, in the order of the table in PerfectWorldPlayerCharacter.cpp. */
    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> BodyParts;

    UPROPERTY()
    TArray<TObjectPtr<UMaterialInstanceDynamic>> BodyPaints;

    /** The engine's basic shape material, painted per body part through dynamic instances ("Color" parameter). */
    UPROPERTY()
    TObjectPtr<UMaterialInterface> BodyBaseMaterial;

    UPROPERTY()
    TObjectPtr<UInputMappingContext> WalkingContext;
    UPROPERTY()
    TObjectPtr<UInputAction> MoveAction;
    UPROPERTY()
    TObjectPtr<UInputAction> LookAction;
    UPROPERTY()
    TObjectPtr<UInputAction> LookStickAction;
    UPROPERTY()
    TObjectPtr<UInputAction> RunAction;
    UPROPERTY()
    TObjectPtr<UInputAction> JumpAction;
    UPROPERTY()
    TObjectPtr<UInputAction> ToggleCameraAction;
    UPROPERTY()
    TObjectPtr<UInputAction> ZoomWheelAction;
    UPROPERTY()
    TObjectPtr<UInputAction> ZoomHeldAction;
    UPROPERTY()
    TObjectPtr<UInputAction> InteractAction;
    UPROPERTY()
    TObjectPtr<UInputAction> CycleTargetAction;
    /** F3: opens the quest window (J and D-pad Right by default; the key bindings store owns the keys). */
    UPROPERTY()
    TObjectPtr<UInputAction> QuestsAction;
    UPROPERTY()
    TObjectPtr<UInputAction> InventoryAction; // R88
    /** F2: opens and closes the full world map (M and D-pad Left by default; the key bindings store owns the keys). */
    UPROPERTY()
    TObjectPtr<UInputAction> MapAction;

private:
    // Movement and look, shared by both input paths.
    void ApplyMove(float Right, float Forward);
    void ApplyLookDegrees(float YawDegrees, float UpDegrees);
    void ApplyZoomCentimetres(float Centimetres);
    bool IsInputBlocked() const;

    // Enhanced Input path (PerfectWorldPlayerCharacterInput.cpp).
    void BuildInputAssets();
    void BindEnhancedInput(UEnhancedInputComponent& Input);
    void OnMove(const FInputActionValue& Value);
    void OnLook(const FInputActionValue& Value);
    void OnLookStick(const FInputActionValue& Value);
    void OnRunHeld();
    void OnJumpStarted();
    void OnJumpCompleted();
    void OnToggleCamera();
    void OnZoomWheel(const FInputActionValue& Value);
    void OnZoomHeld(const FInputActionValue& Value);
    void OnInteractStarted();
    void OnCycleTargetStarted();
    void OnToggleMapStarted();
    void OnInventoryStarted(); // R88
    /** Tells the world map which keys close it from inside Slate: the player's own map keys. */
    void PushMapCloseKeys() const;
    bool WantsEnhancedInput() const;
    void AddWalkingContext();
    void RemoveWalkingContext(AController* FromController);

    // Legacy fallback path (PerfectWorldPlayerCharacterInput.cpp).
    void BindLegacyInput(UInputComponent& Input);
    void OnLegacyAxis(float Value, int32 Which);
    void OnLegacyRunPressed();
    void OnLegacyRunReleased();
    void OnLegacyZoomIn();
    void OnLegacyZoomOut();

    // Body (PerfectWorldPlayerCharacter.cpp).
    void BuildBody();
    void PaintBody();
    void ApplyCameraMode();

    // Seating (PerfectWorldPlayerCharacter.cpp).
    void TickSeat(float DeltaSeconds);
    void ApplySeatPose(float Blend);
    void SetEyeWorldHeight(float EyeZ);
    void ClampSeatedLook();
    void FinishStandUp();
    bool FindStandLocation(const FCollisionQueryParams& Params, FVector& OutCentre) const;

    EPWSeatState SeatState = EPWSeatState::Standing;
    TWeakObjectPtr<AActor> SeatActor;
    FPWSeatPlacement SeatPlacement;
    float SeatElapsed = 0.0f;
    FVector GlideFrom = FVector::ZeroVector; // actor locations
    FVector GlideTo = FVector::ZeroVector;
    float EyeFromZ = 0.0f; // world heights of the first-person eye
    float EyeToZ = 0.0f;
    float YawFrom = 0.0f;
    float FacingYaw = 0.0f;
    bool bMoveInputThisFrame = false;
    bool bMoveReleasedSinceSeated = false;

    EPWCameraMode CameraMode = EPWCameraMode::FirstPerson;
    bool bRunHeldThisFrame = false;
    bool bLegacyRunKeyDown = false;
    FVector HomeLocation = FVector::ZeroVector;
    FRotator HomeRotation = FRotator::ZeroRotator;
};
