#include "PerfectWorldAwakeningActors.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PerfectWorldAwakeningBlocks.h"
#include "PerfectWorldAwakeningCatalogue.h"

using namespace PWAwakening;

APWAwakeningMoverActor::APWAwakeningMoverActor()
{
    PrimaryActorTick.bCanEverTick = true;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Root->SetMobility(EComponentMobility::Movable);
    SetRootComponent(Root);
    Tags.AddUnique(FName(TEXT("awakening.floor.mover")));
}

void APWAwakeningMoverActor::Configure(const FAwMoverSpec& InSpec, UStaticMesh* Cube, UMaterialInterface* Base)
{
    Spec = InSpec;
    CubeMesh = Cube;
    BaseMaterial = Base;
    Tags.AddUnique(Spec.Id);
    if (Block == nullptr)
    {
        Block = NewObject<UStaticMeshComponent>(this, TEXT("Block"));
        Block->SetMobility(EComponentMobility::Movable);
        Block->SetupAttachment(Root);
        Block->SetStaticMesh(Cube);
        Block->SetRelativeScale3D(Spec.Size / 100.0);
        Block->SetCanEverAffectNavigation(false);
        Block->CanCharacterStepUpOn = ECB_Yes;
        if (Spec.bSolid)
        {
            // Blocks the walking player, the camera boom and traces; queries only, nothing simulates.
            Block->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Block->SetCollisionObjectType(ECC_WorldDynamic);
            Block->SetCollisionResponseToAllChannels(ECR_Ignore);
            Block->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
            Block->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
            Block->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        }
        else
        {
            Block->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        Block->RegisterComponent();
    }
    Pose = FMath::Clamp(Spec.StartPose, 0, FMath::Max(0, Spec.Poses.Num() - 1));
    TargetPose = Pose;
    PendingPose = INDEX_NONE;
    bGliding = false;
    bFlickering = Spec.bFlicker;
    if (Spec.Poses.IsValidIndex(Pose))
    {
        SetActorLocation(Spec.Poses[Pose].Centre, false, nullptr, ETeleportType::TeleportPhysics);
        ShowColour(Spec.Poses[Pose].Colour);
    }
}

bool APWAwakeningMoverActor::IsReduced() const
{
    return AwIsReducedMotion(ReducedOverride);
}

void APWAwakeningMoverActor::ShowColour(const EAwColour Colour)
{
    ShownColour = Colour;
    if (Block == nullptr || BaseMaterial == nullptr)
    {
        return;
    }
    if (Paint == nullptr)
    {
        Paint = UMaterialInstanceDynamic::Create(BaseMaterial, this);
        Block->SetMaterial(0, Paint);
    }
    const FLinearColor Linear = FLinearColor::FromSRGBColor(AwColourSrgb(Colour));
    Paint->SetVectorParameterValue(TEXT("Color"), Linear);
    Paint->SetVectorParameterValue(TEXT("BaseColor"), Linear);
}

void APWAwakeningMoverActor::SnapTo(const int32 NewPose)
{
    if (!Spec.Poses.IsValidIndex(NewPose))
    {
        return;
    }
    Pose = NewPose;
    TargetPose = NewPose;
    PendingPose = INDEX_NONE;
    bGliding = false;
    SetActorLocation(Spec.Poses[NewPose].Centre, false, nullptr, ETeleportType::TeleportPhysics);
    ShowColour(Spec.Poses[NewPose].Colour);
    CycleTimer = 0.0f;
}

void APWAwakeningMoverActor::GoTo(const int32 NewPose)
{
    if (!Spec.Poses.IsValidIndex(NewPose) || (NewPose == TargetPose && !IsMoving()))
    {
        return;
    }
    TargetPose = NewPose;
    CycleTimer = 0.0f;
    if (IsReduced())
    {
        // Announced, then one discrete jump: no glide, no in-between position.
        bGliding = false;
        PendingPose = NewPose;
        PendingDelay = AwReducedStepAnnounceSeconds;
        return;
    }
    PendingPose = INDEX_NONE;
    bGliding = true;
    GlideT = 0.0f;
    GlideFrom = GetActorLocation();
}

void APWAwakeningMoverActor::StepNext()
{
    const int32 Cycle = FMath::Min(2, Spec.Poses.Num());
    if (Cycle >= 2)
    {
        GoTo((TargetPose + 1) % Cycle);
    }
}

void APWAwakeningMoverActor::SetHold(const bool bHold)
{
    bHeld = bHold;
    CycleTimer = 0.0f;
}

void APWAwakeningMoverActor::SetFlickering(const bool bOn)
{
    bFlickering = bOn;
    bFlickerOff = false;
    if (!bOn && Spec.Poses.IsValidIndex(Pose))
    {
        ShowColour(Spec.Poses[Pose].Colour);
    }
}

void APWAwakeningMoverActor::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    AdvanceTime(DeltaSeconds);
}

void APWAwakeningMoverActor::AdvanceTime(const float DeltaSeconds)
{
    const bool bReduced = IsReduced();
    if (PendingPose != INDEX_NONE)
    {
        PendingDelay -= DeltaSeconds;
        if (PendingDelay <= 0.0f)
        {
            SnapTo(PendingPose);
        }
    }
    else if (bGliding)
    {
        if (bReduced)
        {
            SnapTo(TargetPose); // the setting changed mid-glide: finish at once, never half way
        }
        else
        {
            GlideT = FMath::Min(1.0f, GlideT + DeltaSeconds / AwMoverGlideSeconds);
            const float Eased = GlideT * GlideT * (3.0f - 2.0f * GlideT);
            SetActorLocation(FMath::Lerp(GlideFrom, Spec.Poses[TargetPose].Centre, Eased), false, nullptr, ETeleportType::TeleportPhysics);
            if (GlideT >= 1.0f)
            {
                SnapTo(TargetPose);
            }
        }
    }
    else if (Spec.bAutoCycle && !bHeld && !bReduced && Spec.Poses.Num() >= 2)
    {
        CycleTimer += DeltaSeconds;
        if (CycleTimer >= AwMoverCycleSeconds)
        {
            CycleTimer = 0.0f;
            GoTo(Pose == 0 ? 1 : 0);
        }
    }

    if (bFlickering && Spec.Poses.IsValidIndex(Pose))
    {
        if (bReduced)
        {
            ShowColour(Spec.Poses[Pose].Colour); // steady fault colour, no flicker
        }
        else
        {
            FlickerTimer += DeltaSeconds;
            if (FlickerTimer >= 0.5f)
            {
                FlickerTimer = 0.0f;
                bFlickerOff = !bFlickerOff;
                ShowColour(bFlickerOff ? EAwColour::Ink : Spec.Poses[Pose].Colour);
            }
        }
    }
}
