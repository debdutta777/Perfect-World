#include "PerfectWorldDoorActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr float HandleX = 78.0f;                 // handle position along the leaf from the hinge
constexpr float HandleZ = 105.0f;                // a door handle about 105 cm above the floor
// The swept test volume: the leaf from 14 to 86 cm out from the hinge, 10 cm thick, 6 to 206 cm above the floor. It stays
// clear of the jambs, the lintel and the floor so a closed door never reports its own frame as an obstruction.
// It is sampled at poses at most PWDoor::MaxSubStepDegrees apart (a tip travel of at most 5 cm at 90 cm, 4.8 cm at the
// volume's 86 cm end), which is less than its 10 cm thickness, so consecutive sample boxes overlap and no gap is left.
constexpr float SweepCentreX = 50.0f;
constexpr double SweepHalfLength = 36.0;
constexpr double SweepHalfThickness = 5.0;
constexpr double SweepHalfHeight = 100.0;
constexpr float SweepCentreZ = 106.0f;
} // namespace

APWDoorActor::APWDoorActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.TickGroup = TG_PostPhysics; // after the pawns have moved this frame, so the swing check sees where they are

    HingeRoot = CreateDefaultSubobject<USceneComponent>(TEXT("HingeRoot"));
    HingeRoot->SetMobility(EComponentMobility::Movable);
    SetRootComponent(HingeRoot);

    Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
    Pivot->SetMobility(EComponentMobility::Movable);
    Pivot->SetupAttachment(HingeRoot);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Paint(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (Paint.Succeeded())
    {
        BaseMaterial = Paint.Object;
    }

    Leaf = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Leaf"));
    Leaf->SetMobility(EComponentMobility::Movable);
    Leaf->SetupAttachment(Pivot);
    Leaf->SetRelativeLocation(FVector(0.5 * PWDoor::LeafWidth, 0.0, PWDoor::LeafLift + 0.5 * PWDoor::LeafHeight));
    Leaf->SetRelativeScale3D(FVector(PWDoor::LeafWidth / 100.0, PWDoor::LeafThickness / 100.0, PWDoor::LeafHeight / 100.0));
    Leaf->SetCanEverAffectNavigation(false);
    Leaf->CanCharacterStepUpOn = ECB_No; // nobody is carried by the leaf
    // The leaf blocks the walking player, the camera boom and traces, and nothing else. Queries only; nothing simulates.
    Leaf->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Leaf->SetCollisionObjectType(ECC_WorldDynamic);
    Leaf->SetCollisionResponseToAllChannels(ECR_Ignore);
    Leaf->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    Leaf->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
    Leaf->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    for (int32 Side = 0; Side < 2; ++Side)
    {
        UStaticMeshComponent* Handle =
            CreateDefaultSubobject<UStaticMeshComponent>(Side == 0 ? TEXT("HandleInside") : TEXT("HandleOutside"));
        Handle->SetMobility(EComponentMobility::Movable);
        Handle->SetupAttachment(Pivot);
        const float Sign = Side == 0 ? 1.0f : -1.0f;
        Handle->SetRelativeLocation(FVector(HandleX, Sign * (0.5f * PWDoor::LeafThickness + 2.0f), HandleZ));
        Handle->SetRelativeScale3D(FVector(0.14, 0.03, 0.03)); // a 14 x 3 x 3 cm lever
        Handle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Handle->SetCanEverAffectNavigation(false);
        Handle->SetCastShadow(false);
        (Side == 0 ? HandleInside : HandleOutside) = Handle;
    }

    if (Cube.Succeeded())
    {
        Leaf->SetStaticMesh(Cube.Object);
        HandleInside->SetStaticMesh(Cube.Object);
        HandleOutside->SetStaticMesh(Cube.Object);
    }
}

void APWDoorActor::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    if (BaseMaterial != nullptr)
    {
        // BasicShapeMaterial exposes its base colour as the vector parameter "Color" (the same as the lane blocks).
        LeafPaint = UMaterialInstanceDynamic::Create(BaseMaterial, this);
        LeafPaint->SetVectorParameterValue(TEXT("Color"), FLinearColor::FromSRGBColor(FColor(92, 56, 38)));
        LeafPaint->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor::FromSRGBColor(FColor(92, 56, 38)));
        Leaf->SetMaterial(0, LeafPaint);
        HandlePaint = UMaterialInstanceDynamic::Create(BaseMaterial, this);
        HandlePaint->SetVectorParameterValue(TEXT("Color"), FLinearColor::FromSRGBColor(FColor(172, 174, 178)));
        HandlePaint->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor::FromSRGBColor(FColor(172, 174, 178)));
        HandleInside->SetMaterial(0, HandlePaint);
        HandleOutside->SetMaterial(0, HandlePaint);
    }
}

void APWDoorActor::Configure(FName InDoorId, float InPreferredSwing, bool bInTwoWay)
{
    DoorId = InDoorId;
    PreferredSwing = InPreferredSwing >= 0.0f ? 1.0f : -1.0f;
    bTwoWay = bInTwoWay;
    OpenSign = PreferredSwing;
}

void APWDoorActor::ApplyAngle()
{
    Pivot->SetRelativeRotation(FRotator(0.0, static_cast<double>(CurrentAngle), 0.0));
}

void APWDoorActor::SetOpenImmediate(bool bNewOpen, int8 SwingSide)
{
    bOpen = bNewOpen;
    bMoving = false;
    SetActorTickEnabled(false);
    if (bNewOpen)
    {
        OpenSign = (bTwoWay && SwingSide != 0) ? (SwingSide > 0 ? 1.0f : -1.0f) : PreferredSwing;
    }
    CurrentAngle = bNewOpen ? OpenSign * PWDoor::OpenAngleDegrees : 0.0f;
    StartAngle = CurrentAngle;
    TargetAngle = CurrentAngle;
    ApplyAngle();
}

void APWDoorActor::BeginMove(float NewTargetAngle)
{
    StartAngle = CurrentAngle;
    TargetAngle = NewTargetAngle;
    Elapsed = 0.0f;
    // A full swing takes OpenSeconds; a shorter move takes proportionally less, but never a blink.
    Duration = FMath::Max(0.12f, PWDoor::OpenSeconds * FMath::Abs(TargetAngle - StartAngle) / PWDoor::OpenAngleDegrees);
    bMoving = true;
    SetActorTickEnabled(true);
}

void APWDoorActor::Settle(const bool bReachedTarget)
{
    bMoving = false;
    SetActorTickEnabled(false);
    // The leaf's real angle is the truth: a leaf stopped part-way is open, whichever way it was going.
    const int8 Side = PWDoor::SwingSideOf(CurrentAngle);
    bOpen = Side != 0;
    if (Side != 0)
    {
        OpenSign = static_cast<float>(Side);
    }
    StartAngle = CurrentAngle;
    TargetAngle = CurrentAngle;
    if (!bReachedTarget)
    {
        UE_LOG(LogTemp, Display, TEXT("Door %s: something stepped into the swing; it stopped at %.1f degrees and counts as %s."),
               *DoorId.ToString(), CurrentAngle, bOpen ? TEXT("open") : TEXT("closed"));
    }
    OnSettled.Broadcast(DoorId, bReachedTarget, bOpen, Side);
}

void APWDoorActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bMoving)
    {
        SetActorTickEnabled(false);
        return;
    }
    Elapsed += DeltaSeconds;
    const float T = FMath::Clamp(Elapsed / Duration, 0.0f, 1.0f);
    const float Eased = T * T * (3.0f - 2.0f * T); // smoothstep: slow start, slow finish
    const float Wanted = T >= 1.0f ? TargetAngle : FMath::Lerp(StartAngle, TargetAngle, Eased);

    // Never apply a step that has not been checked, however long the frame was: the whole interval from where the leaf is
    // to where it wants to be is swept in sub-steps, and the leaf stops at the last clear pose.
    bool bBlocked = false;
    const float Safe = FindFurthestSafeAngle(CurrentAngle, Wanted, true, bBlocked);
    CurrentAngle = Safe;
    ApplyAngle();
    if (bBlocked)
    {
        Settle(false);
    }
    else if (T >= 1.0f)
    {
        CurrentAngle = TargetAngle;
        ApplyAngle();
        Settle(true);
    }
}

void APWDoorActor::BeginPlay()
{
    Super::BeginPlay();
    // The core owns whether this door is open; take its start state (and have an unknown id reported).
    if (UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(this))
    {
        Life->RegisterDoor(*this);
    }
}

FText APWDoorActor::GetInteractionVerb(const APawn* By) const
{
    if (bMoving)
    {
        return FText::GetEmpty();
    }
    // The verb follows the core's state; the actor's own flag is only the fallback when no subsystem exists.
    bool bCoreOpen = bOpen;
    if (const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(this))
    {
        bCoreOpen = Life->IsDoorOpen(DoorId, bOpen);
    }
    return bCoreOpen ? NSLOCTEXT("PerfectWorldDoor", "Close", "Close") : NSLOCTEXT("PerfectWorldDoor", "Open", "Open");
}

FVector APWDoorActor::GetInteractionPoint() const
{
    return Pivot->GetComponentTransform().TransformPosition(FVector(HandleX, 0.0, HandleZ));
}

bool APWDoorActor::IsPoseBlocked(const float Angle) const
{
    const UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return false;
    }
    FCollisionQueryParams Params(TEXT("PWDoorSweep"), false, this); // ignores every component of this actor, the leaf included
    const FCollisionShape Box = FCollisionShape::MakeBox(FVector(SweepHalfLength, SweepHalfThickness, SweepHalfHeight));
    const FCollisionObjectQueryParams Pawns(ECC_Pawn);
    const FTransform Hinge = GetActorTransform();
    const FQuat Swing(FVector::UpVector, FMath::DegreesToRadians(Angle));
    const FVector Centre = Hinge.TransformPosition(Swing.RotateVector(FVector(SweepCentreX, 0.0, SweepCentreZ)));
    const FQuat Rotation = Hinge.GetRotation() * Swing;
    // Anything that blocks the Pawn channel (walls, furniture, a standing capsule) and any pawn at all.
    return World->OverlapBlockingTestByChannel(Centre, Rotation, ECC_Pawn, Box, Params) ||
           World->OverlapAnyTestByObjectType(Centre, Rotation, Pawns, Box, Params);
}

float APWDoorActor::FindFurthestSafeAngle(const float FromAngle, const float ToAngle, const bool bSkipStartPose, bool& bOutBlocked) const
{
    bOutBlocked = false;
    if (FMath::IsNearlyEqual(FromAngle, ToAngle, 0.01f))
    {
        return ToAngle; // no movement to test
    }
    const int32 Steps = PWDoor::SubStepCount(FromAngle, ToAngle);
    float LastSafe = FromAngle;
    for (int32 I = bSkipStartPose ? 1 : 0; I <= Steps; ++I)
    {
        const float Angle = FMath::Lerp(FromAngle, ToAngle, static_cast<float>(I) / static_cast<float>(Steps));
        if (IsPoseBlocked(Angle))
        {
            bOutBlocked = true;
            return LastSafe;
        }
        LastSafe = Angle;
    }
    return ToAngle;
}

bool APWDoorActor::IsSweepClear(const float FromAngle, const float ToAngle) const
{
    bool bBlocked = false;
    FindFurthestSafeAngle(FromAngle, ToAngle, false, bBlocked);
    return !bBlocked;
}

bool APWDoorActor::TryOpen(const APawn* By)
{
    float Candidates[2] = {PreferredSwing, 0.0f};
    int32 Count = 1;
    if (bTwoWay)
    {
        // Swing away from whoever is using the door; fall back to the other side.
        float Away = -PreferredSwing;
        if (By != nullptr)
        {
            const FVector Local = GetActorTransform().InverseTransformPosition(By->GetActorLocation());
            Away = Local.Y >= 0.0 ? -1.0f : 1.0f;
        }
        Candidates[0] = Away;
        Candidates[1] = -Away;
        Count = 2;
    }
    for (int32 I = 0; I < Count; ++I)
    {
        const float Angle = Candidates[I] * PWDoor::OpenAngleDegrees;
        if (IsSweepClear(0.0f, Angle))
        {
            OpenSign = Candidates[I];
            bOpen = true;
            BeginMove(Angle);
            return true;
        }
    }
    return false;
}

bool APWDoorActor::TryClose()
{
    if (!IsSweepClear(CurrentAngle, 0.0f))
    {
        return false; // someone or something is in the way: stay open
    }
    bOpen = false;
    BeginMove(0.0f);
    return true;
}

bool APWDoorActor::TryMoveTo(const bool bWantOpen, const APawn* By)
{
    if (bMoving || bWantOpen == bOpen)
    {
        return false;
    }
    const bool bDone = bWantOpen ? TryOpen(By) : TryClose();
    if (!bDone)
    {
        UE_LOG(LogTemp, Display, TEXT("Door %s: something is in the way; it stays %s."), *DoorId.ToString(), bOpen ? TEXT("open") : TEXT("closed"));
    }
    return bDone;
}

void APWDoorActor::Interact(APawn* By)
{
    if (bMoving)
    {
        return;
    }
    // The portable core decides; this actor only swings when it is told the move is allowed.
    UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(this);
    if (Life == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("Door %s: no lane-life subsystem in this world; the door does nothing."), *DoorId.ToString());
        return;
    }
    Life->RequestDoorToggle(*this, By);
}
