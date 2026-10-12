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
class USpringArmComponent;
class UStaticMeshComponent;
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

    EPWCameraMode CameraMode = EPWCameraMode::FirstPerson;
    bool bRunHeldThisFrame = false;
    bool bLegacyRunKeyDown = false;
    FVector HomeLocation = FVector::ZeroVector;
    FRotator HomeRotation = FRotator::ZeroRotator;
};
