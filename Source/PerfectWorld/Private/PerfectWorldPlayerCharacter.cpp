#include "PerfectWorldPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
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
    // Thighs: folded into the hips and hidden while standing; they carry the sitting pose.
    {TEXT("BodyThighLeft"), FVector(0.0, -9.0, 80.0), FVector(14.0, 14.0, 14.0), TintTrousers},
    {TEXT("BodyThighRight"), FVector(0.0, 9.0, 80.0), FVector(14.0, 14.0, 14.0), TintTrousers},
};
constexpr int32 FirstThighPart = 6;

/**
 * The sitting pose of one block part, relative to the seated capsule's centre (its bottom is the seat top, 41 cm down):
 * calves hang from the knee (36 cm forward) to the floor, thighs lie on the seat, the torso rises from the seat.
 */
void SeatedBlockPart(const int32 Index, const float SeatHeightCm, FVector& OutCentre, FVector& OutSize)
{
    const float Top = -PWSeat::CapsuleHalfHeight;
    const float Side = (Index % 2 == 0) ? -9.0f : 9.0f; // even parts are the left ones (legs 0-1, thighs 6-7)
    switch (Index)
    {
    case 0:
    case 1: OutCentre = FVector(36.0, Side, Top - SeatHeightCm * 0.5f); OutSize = FVector(14.0, 14.0, SeatHeightCm); break;
    case 2: OutCentre = FVector(-2.0, 0.0, Top + 30.0f); OutSize = FVector(20.0, 40.0, 60.0); break;
    case 3:
    case 4: OutCentre = FVector(0.0, Index == 3 ? -25.0 : 25.0, Top + 28.0f); OutSize = FVector(10.0, 10.0, 56.0); break;
    case 5: OutCentre = FVector(0.0, 0.0, Top + 72.5f); OutSize = FVector(20.0, 20.0, 25.0); break;
    default: OutCentre = FVector(18.0, Side, Top + 7.0f); OutSize = FVector(40.0, 14.0, 14.0); break;
    }
}

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
        Part->SetHiddenInGame(BodyParts.Num() >= FirstThighPart); // the thighs only show while sitting
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
    TickSeat(DeltaSeconds);

    // Run is "held this frame": the input handlers set the flag, and a missed release can never leave the player running.
    const bool bRun = (bRunHeldThisFrame || bLegacyRunKeyDown) && !IsInputBlocked();
    bRunHeldThisFrame = false;
    GetCharacterMovement()->MaxWalkSpeed = bRun ? PWPlayer::RunSpeed : PWPlayer::WalkSpeed;

    if (SeatState == EPWSeatState::Standing && GetActorLocation().Z < HomeLocation.Z - FallRecoveryDepth)
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
    if (SeatState != EPWSeatState::Standing)
    {
        // Seated: walking is blocked; a deliberate movement input stands the player up, but only after the movement keys have
        // been let go once since sitting, so a key still held from walking up to the seat does not undo the sit.
        if (FMath::Max(FMath::Abs(Right), FMath::Abs(Forward)) >= PWSeat::MoveInputThreshold)
        {
            bMoveInputThisFrame = true;
            if (SeatState == EPWSeatState::Seated && bMoveReleasedSinceSeated)
            {
                StandUp();
            }
        }
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
    if (SeatState == EPWSeatState::Entering)
    {
        return; // the glide turns the view to the seat's facing
    }
    AddControllerYawInput(YawDegrees);
    AddControllerPitchInput(-UpDegrees); // the engine's positive pitch input looks down; the default is not inverted
    ClampSeatedLook();
}

void APerfectWorldPlayerCharacter::ApplyZoomCentimetres(float Centimetres)
{
    if (!IsFirstPerson() && !IsInputBlocked())
    {
        SetThirdPersonDistance(GetThirdPersonDistance() + Centimetres);
    }
}

// ---- Seating. The portable core does not know about sitting: it is presentation and safe movement only. ----

void APerfectWorldPlayerCharacter::ApplySeatPose(float Blend)
{
    const float SeatHeight = SeatPlacement.SeatHeightCm;
    for (int32 Index = 0; Index < BodyParts.Num() && Index < static_cast<int32>(UE_ARRAY_COUNT(BodySpecs)); ++Index)
    {
        UStaticMeshComponent* Part = BodyParts[Index];
        if (Part == nullptr)
        {
            continue;
        }
        FVector SeatedCentre;
        FVector SeatedSize;
        SeatedBlockPart(Index, SeatHeight, SeatedCentre, SeatedSize);
        const FBodyPartSpec& Spec = BodySpecs[Index];
        const FVector StandingCentre(Spec.Centre.X, Spec.Centre.Y, Spec.Centre.Z - PWPlayer::CapsuleHalfHeight);
        Part->SetRelativeLocation(FMath::Lerp(StandingCentre, SeatedCentre, Blend));
        Part->SetRelativeScale3D(FMath::Lerp(Spec.Size, SeatedSize, Blend) / 100.0);
        if (Index >= FirstThighPart)
        {
            Part->SetHiddenInGame(Blend <= 0.0f);
        }
    }
}

void APerfectWorldPlayerCharacter::SetEyeWorldHeight(float EyeZ)
{
    // Both cameras hang off the capsule; only their height above its centre changes (the first person one stays 8 cm forward).
    const double Relative = static_cast<double>(EyeZ) - GetActorLocation().Z;
    FirstPersonCamera->SetRelativeLocation(FVector(8.0, 0.0, Relative));
    BoomArm->SetRelativeLocation(FVector(0.0, 0.0, Relative));
}
