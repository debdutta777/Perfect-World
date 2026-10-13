#include "PerfectWorldPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"
#include "PerfectWorldInteractionComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
/** Colours of the placeholder block body (sRGB). Real art replaces the whole body at M9; gameplay never reads these. */
const FColor BodyTints[] = {FColor(198, 146, 104), FColor(70, 104, 150), FColor(54, 58, 70)};
constexpr int32 TintSkin = 0;
constexpr int32 TintShirt = 1;
constexpr int32 TintTrousers = 2;

struct FBodyPartSpec
{
    const TCHAR* Name;
    FVector Centre; // X forward, Y right, Z height above the floor, in cm
    FVector Size;   // full extent in cm
    int32 Tint;
};

// A 165 cm blocky figure. Shoulders and head are well inside the 34 cm capsule radius.
const FBodyPartSpec BodySpecs[] = {
    {TEXT("BodyLegLeft"), FVector(0.0, -9.0, 40.0), FVector(14.0, 14.0, 80.0), TintTrousers},
    {TEXT("BodyLegRight"), FVector(0.0, 9.0, 40.0), FVector(14.0, 14.0, 80.0), TintTrousers},
    {TEXT("BodyTorso"), FVector(0.0, 0.0, 110.0), FVector(20.0, 40.0, 60.0), TintShirt},
    {TEXT("BodyArmLeft"), FVector(0.0, -25.0, 108.0), FVector(10.0, 10.0, 56.0), TintSkin},
    {TEXT("BodyArmRight"), FVector(0.0, 25.0, 108.0), FVector(10.0, 10.0, 56.0), TintSkin},
    {TEXT("BodyHead"), FVector(0.0, 0.0, 152.5), FVector(20.0, 20.0, 25.0), TintSkin},
};

/** If the character ever ends up this far under its starting height it has fallen out of the world: put it back. */
constexpr double FallRecoveryDepth = 3000.0;
} // namespace

APerfectWorldPlayerCharacter::APerfectWorldPlayerCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    UCapsuleComponent* Capsule = GetCapsuleComponent();
    Capsule->InitCapsuleSize(PWPlayer::CapsuleRadius, PWPlayer::CapsuleHalfHeight);
    BaseEyeHeight = PWPlayer::EyeHeightAboveFloor - PWPlayer::CapsuleHalfHeight;

    // The body turns with the camera's yaw in both camera modes, so switching camera never changes facing.
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = true;
    bUseControllerRotationRoll = false;

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->bOrientRotationToMovement = false;
    Movement->bUseControllerDesiredRotation = false;
    Movement->MaxWalkSpeed = PWPlayer::WalkSpeed;
    Movement->MaxStepHeight = PWPlayer::MaxStepHeight;
    Movement->JumpZVelocity = PWPlayer::JumpSpeed;
    Movement->AirControl = 0.25f;
    Movement->MaxAcceleration = 1600.0f;
    Movement->BrakingDecelerationWalking = 1800.0f;
    Movement->GravityScale = 1.0f;

    // The ACharacter skeletal mesh stays empty; the block body below is what is seen.
    OverrideInputComponentClass = UEnhancedInputComponent::StaticClass();

    // First person: on the capsule at eye height, turned by the controller and by nothing else.
    FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCamera->SetupAttachment(Capsule);
    FirstPersonCamera->SetRelativeLocation(FVector(8.0, 0.0, PWPlayer::EyeHeightAboveFloor - PWPlayer::CapsuleHalfHeight));
    FirstPersonCamera->bUsePawnControlRotation = true;
    FirstPersonCamera->bAutoActivate = true;

    // Third person: a boom that orbits the same eye point with the controller's rotation and pulls in on collision.
    BoomArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("BoomArm"));
    BoomArm->SetupAttachment(Capsule);
    BoomArm->SetRelativeLocation(FVector(0.0, 0.0, PWPlayer::EyeHeightAboveFloor - PWPlayer::CapsuleHalfHeight));
    BoomArm->TargetArmLength = PWPlayer::BoomLengthDefault;
    BoomArm->bUsePawnControlRotation = true;
    BoomArm->bDoCollisionTest = true;
    BoomArm->ProbeSize = PWPlayer::BoomProbeSize;
    BoomArm->ProbeChannel = ECC_Camera;
    BoomArm->bEnableCameraLag = false;
    BoomArm->bEnableCameraRotationLag = false;

    ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
    ThirdPersonCamera->SetupAttachment(BoomArm, USpringArmComponent::SocketName);
    ThirdPersonCamera->bUsePawnControlRotation = false; // the boom already carries the controller rotation
    ThirdPersonCamera->bAutoActivate = false;

    // Targeting measures from the capsule, so it is the same in both cameras; see PWInteraction in its header.
    Interaction = CreateDefaultSubobject<UPWInteractionComponent>(TEXT("Interaction"));

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Paint(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (Paint.Succeeded())
    {
        BodyBaseMaterial = Paint.Object;
    }
    BuildBody();
}

void APerfectWorldPlayerCharacter::BuildBody()
{
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    for (const FBodyPartSpec& Spec : BodySpecs)
    {
        UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Spec.Name);
        Part->SetupAttachment(GetCapsuleComponent());
        if (Cube.Succeeded())
        {
            Part->SetStaticMesh(Cube.Object);
        }
        // The engine cube is 100 cm on a side, centred on its pivot; the capsule's origin is its centre.
        Part->SetRelativeLocation(FVector(Spec.Centre.X, Spec.Centre.Y, Spec.Centre.Z - PWPlayer::CapsuleHalfHeight));
        Part->SetRelativeScale3D(Spec.Size / 100.0);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision); // the capsule is the body for collision
        Part->SetCanEverAffectNavigation(false);
        Part->SetOwnerNoSee(true); // first person is the default
        BodyParts.Add(Part);
    }
}

void APerfectWorldPlayerCharacter::PaintBody()
{
    if (BodyBaseMaterial == nullptr)
    {
        return;
    }
    BodyPaints.Reset();
    for (int32 Index = 0; Index < BodyParts.Num() && Index < static_cast<int32>(UE_ARRAY_COUNT(BodySpecs)); ++Index)
    {
        UStaticMeshComponent* Part = BodyParts[Index];
        if (Part == nullptr)
        {
            continue;
        }
        UMaterialInstanceDynamic* Paint = UMaterialInstanceDynamic::Create(BodyBaseMaterial, this);
        const FLinearColor Linear = FLinearColor::FromSRGBColor(BodyTints[BodySpecs[Index].Tint]);
        Paint->SetVectorParameterValue(TEXT("Color"), Linear);
        Paint->SetVectorParameterValue(TEXT("BaseColor"), Linear);
        Part->SetMaterial(0, Paint);
        BodyPaints.Add(Paint);
    }
}

void APerfectWorldPlayerCharacter::BeginPlay()
{
    Super::BeginPlay();
    HomeLocation = GetActorLocation();
    HomeRotation = GetActorRotation();
    PaintBody();
    ApplyCameraMode();
}

void APerfectWorldPlayerCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // Run is "held this frame": the input handlers set the flag, and a missed release can never leave the player running.
    const bool bRun = (bRunHeldThisFrame || bLegacyRunKeyDown) && !IsInputBlocked();
    bRunHeldThisFrame = false;
    GetCharacterMovement()->MaxWalkSpeed = bRun ? PWPlayer::RunSpeed : PWPlayer::WalkSpeed;

    if (GetActorLocation().Z < HomeLocation.Z - FallRecoveryDepth)
    {
        TeleportTo(HomeLocation, HomeRotation);
        GetCharacterMovement()->Velocity = FVector::ZeroVector;
    }
}

void APerfectWorldPlayerCharacter::SetCameraMode(EPWCameraMode NewMode)
{
    CameraMode = NewMode;
    ApplyCameraMode();
}

void APerfectWorldPlayerCharacter::ToggleCameraMode()
{
    SetCameraMode(IsFirstPerson() ? EPWCameraMode::ThirdPerson : EPWCameraMode::FirstPerson);
}

void APerfectWorldPlayerCharacter::ApplyCameraMode()
{
    // Only these change: which camera is active, and whether the owner sees the body. Position and facing are untouched.
    const bool bFirst = IsFirstPerson();
    FirstPersonCamera->SetActive(bFirst);
    ThirdPersonCamera->SetActive(!bFirst);
    for (UStaticMeshComponent* Part : BodyParts)
    {
        if (Part != nullptr)
        {
            Part->SetOwnerNoSee(bFirst);
        }
    }
}

void APerfectWorldPlayerCharacter::SetThirdPersonDistance(float Centimetres)
{
    BoomArm->TargetArmLength = FMath::Clamp(Centimetres, PWPlayer::BoomLengthMin, PWPlayer::BoomLengthMax);
}

float APerfectWorldPlayerCharacter::GetThirdPersonDistance() const
{
    return BoomArm->TargetArmLength;
}

bool APerfectWorldPlayerCharacter::IsInputBlocked() const
{
    // While the title, creation flow, pause menu or a conversation is up the world does not take play input.
    return UPWInteractionComponent::IsWorldInputBlocked(this);
}

void APerfectWorldPlayerCharacter::ApplyMove(float Right, float Forward)
{
    if (Controller == nullptr || IsInputBlocked())
    {
        return;
    }
    // Move relative to where the camera looks (yaw only), so looking up or down never slows walking.
    const FRotator YawOnly(0.0, Controller->GetControlRotation().Yaw, 0.0);
    const FRotationMatrix Basis(YawOnly);
    AddMovementInput(Basis.GetUnitAxis(EAxis::X), Forward);
    AddMovementInput(Basis.GetUnitAxis(EAxis::Y), Right);
}

void APerfectWorldPlayerCharacter::ApplyLookDegrees(float YawDegrees, float UpDegrees)
{
    if (Controller == nullptr || IsInputBlocked())
    {
        return;
    }
    AddControllerYawInput(YawDegrees);
    AddControllerPitchInput(-UpDegrees); // the engine's positive pitch input looks down; the default is not inverted
}

void APerfectWorldPlayerCharacter::ApplyZoomCentimetres(float Centimetres)
{
    if (!IsFirstPerson() && !IsInputBlocked())
    {
        SetThirdPersonDistance(GetThirdPersonDistance() + Centimetres);
    }
}
