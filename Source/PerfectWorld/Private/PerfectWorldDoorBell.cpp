#include "PerfectWorldDoorBell.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DoorBell.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldPresentationSettings.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const FColor DbPlateColour(228, 220, 198);
const FColor DbButtonColour(214, 140, 46);
const FColor DbButtonLitColour(255, 222, 120);
const FColor DbLabelColour(44, 38, 32);
/** The bell is only asked about its own verb when the player is within this of it; farther away the prompt cannot reach it anyway. */
constexpr double DbVerbAskDistanceCm = 400.0;

void DbPaint(UMaterialInstanceDynamic* Paint, const FLinearColor& Colour)
{
    if (Paint != nullptr)
    {
        Paint->SetVectorParameterValue(TEXT("Color"), Colour);
        Paint->SetVectorParameterValue(TEXT("BaseColor"), Colour);
    }
}
} // namespace

// ---- Geometry ----

namespace PWDoorBell
{
FThresholdProbe ProbeThreshold(const FTransform& DoorTransform, const double InsideSide, const FVector& WorldPoint)
{
    const FVector Local = DoorTransform.InverseTransformPositionNoScale(WorldPoint);
    FThresholdProbe Probe;
    Probe.Across = static_cast<float>(Local.X);
    Probe.Depth = static_cast<float>(Local.Y * (InsideSide >= 0.0 ? 1.0 : -1.0));
    Probe.Up = static_cast<float>(Local.Z);
    Probe.bNear = Probe.Across >= GuardNearAcrossMinCm && Probe.Across <= GuardNearAcrossMaxCm && FMath::Abs(Probe.Depth) <= GuardNearDepthCm &&
                  Probe.Up >= GuardNearUpMinCm && Probe.Up <= GuardNearUpMaxCm;
    return Probe;
}

bool IsInwardCrossing(const FThresholdProbe& Previous, const FThresholdProbe& Now, const float StepCm)
{
    return Previous.bNear && Now.bNear && StepCm <= GuardMaxWalkStepCm && Previous.Depth < -GuardCrossMarginCm && Now.Depth >= -GuardCrossMarginCm &&
           Now.Across >= GuardDoorwayAcrossMinCm && Now.Across <= GuardDoorwayAcrossMaxCm;
}

FVector OutsideDirection(const FTransform& DoorTransform, const double InsideSide)
{
    return DoorTransform.TransformVectorNoScale(FVector(0.0, InsideSide >= 0.0 ? -1.0 : 1.0, 0.0)).GetSafeNormal();
}

FVector NominalBellPoint(const FTransform& DoorTransform, const double InsideSide, const float AcrossCm)
{
    const double Face = (InsideSide >= 0.0 ? -1.0 : 1.0) * static_cast<double>(FaceFromHingeLineCm);
    return DoorTransform.TransformPositionNoScale(FVector(static_cast<double>(AcrossCm), Face, static_cast<double>(HeightCm)));
}

double BellYawDegrees(const FVector& Outside)
{
    return FMath::RadiansToDegrees(FMath::Atan2(Outside.Y, Outside.X));
}

APWDoorBellActor* SpawnBellFor(APWDoorActor& Door)
{
    UWorld* World = Door.GetWorld();
    const FName Id = Door.GetDoorId();
    if (World == nullptr || Id.IsNone())
    {
        return nullptr;
    }
    const PerfectWorld::DoorDef* Def = PerfectWorld::FindDoor(PWLaneLife::ToStd(Id));
    if (Def == nullptr || !PerfectWorld::DoorHasBell(*Def))
    {
        return nullptr;
    }
    if (AActor* Existing = Door.GetBellActor())
    {
        return Cast<APWDoorBellActor>(Existing);
    }

    const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(&Door);
    double Side = Life != nullptr ? Life->GetDoorInsideSide(Id) : 0.0;
    if (Side == 0.0)
    {
        Side = Door.GetPreferredSwing() >= 0.0f ? 1.0 : -1.0; // a dwelling leaf swings into the dwelling
    }
    const FTransform Frame = Door.GetActorTransform();
    const FVector Out = OutsideDirection(Frame, Side);

    // The button goes on the real wall face beside the frame: the latch side first, then the hinge side, then a little further each
    // way. A candidate is good when a trace from the street side meets a wall facing the visitor with clear air in front of it.
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PWDoorBellPlace), false, &Door);
    FVector Surface = FVector::ZeroVector;
    bool bOnWall = false;
    for (const float Across : {LatchSideAcrossCm, HingeSideAcrossCm, LatchSideAcrossCm + 17.0f, HingeSideAcrossCm - 17.0f})
    {
        const FVector Nominal = NominalBellPoint(Frame, Side, Across);
        FHitResult Hit;
        if (World->LineTraceSingleByChannel(Hit, Nominal + Out * (FreeFrontCm + 10.0f), Nominal - Out * 10.0f, ECC_Visibility, Params) &&
            !Hit.bStartPenetrating && Hit.Distance >= FreeFrontCm && FVector::DotProduct(Hit.ImpactNormal, Out) > 0.9)
        {
            Surface = Hit.ImpactPoint;
            bOnWall = true;
            break;
        }
    }
    if (!bOnWall)
    {
        // No wall was found (a bare test world, or a layout this does not understand): the nominal point still gives the visitor a bell.
        Surface = NominalBellPoint(Frame, Side, LatchSideAcrossCm);
        UE_LOG(LogTemp, Warning, TEXT("Door %s: no clear wall face found for its bell; it hangs at the nominal point."), *Id.ToString());
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    APWDoorBellActor* Bell = World->SpawnActor<APWDoorBellActor>(Surface, FRotator(0.0, BellYawDegrees(Out), 0.0), SpawnParams);
    if (Bell != nullptr)
    {
        Bell->Configure(Id);
        Door.SetBellActor(Bell);
    }
    return Bell;
}
} // namespace PWDoorBell

// ---- The bell actor ----

APWDoorBellActor::APWDoorBellActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Root->SetMobility(EComponentMobility::Movable);
    SetRootComponent(Root);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Paint(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (Paint.Succeeded())
    {
        BaseMaterial = Paint.Object;
    }

    Plate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Plate"));
    Plate->SetMobility(EComponentMobility::Movable);
    Plate->SetupAttachment(Root);
    // The plate's back sits on the wall face (the actor's origin); it stands PlateDepth proud, towards +X (the visitor).
    Plate->SetRelativeLocation(FVector(0.5f * PWDoorBell::PlateDepthCm, 0.0f, 0.0f));
    Plate->SetRelativeScale3D(FVector(PWDoorBell::PlateDepthCm / 100.0f, PWDoorBell::PlateWidthCm / 100.0f, PWDoorBell::PlateHeightCm / 100.0f));

    Button = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Button"));
    Button->SetMobility(EComponentMobility::Movable);
    Button->SetupAttachment(Root);
    Button->SetRelativeLocation(FVector(PWDoorBell::PlateDepthCm + 0.8f, 0.0f, -2.6f));
    Button->SetRelativeScale3D(FVector(1.6f / 100.0f, PWDoorBell::ButtonSizeCm / 100.0f, PWDoorBell::ButtonSizeCm / 100.0f));

    for (UStaticMeshComponent* Part : {Plate.Get(), Button.Get()})
    {
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCanEverAffectNavigation(false);
        Part->SetCastShadow(false);
        if (Cube.Succeeded())
        {
            Part->SetStaticMesh(Cube.Object);
        }
    }

    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
    Label->SetMobility(EComponentMobility::Movable);
    Label->SetupAttachment(Root);
    Label->SetRelativeLocation(FVector(PWDoorBell::PlateDepthCm + 0.15f, 0.0f, 4.6f)); // on the plate's face, above the button
    Label->SetText(NSLOCTEXT("PerfectWorldDoorBell", "BellLabel", "BELL"));
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetVerticalAlignment(EVRTA_TextCenter);
    Label->SetTextRenderColor(DbLabelColour);
    Label->SetWorldSize(2.6f);
    Label->SetCastShadow(false);
    Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APWDoorBellActor::Configure(const FName InDoorId)
{
    DoorId = InDoorId;
    if (BaseMaterial != nullptr && PlatePaint == nullptr)
    {
        PlatePaint = UMaterialInstanceDynamic::Create(BaseMaterial, this);
        ButtonPaint = UMaterialInstanceDynamic::Create(BaseMaterial, this);
        Plate->SetMaterial(0, PlatePaint);
        Button->SetMaterial(0, ButtonPaint);
    }
    DbPaint(PlatePaint, FLinearColor::FromSRGBColor(DbPlateColour));
    DbPaint(ButtonPaint, FLinearColor::FromSRGBColor(DbButtonColour));
}

FText APWDoorBellActor::GetInteractionVerb(const APawn* By) const
{
    const FText Ring = NSLOCTEXT("PerfectWorldDoorBell", "Ring", "Ring the bell");
    if (By == nullptr || FVector::DistSquared(By->GetActorLocation(), GetActorLocation()) > DbVerbAskDistanceCm * DbVerbAskDistanceCm)
    {
        return Ring; // out of reach anyway; the prompt never shows it
    }
    const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(this);
    // Not while an earlier ring is still being answered, and not for a resident or someone already let in: no bell for their own door.
    if (Life != nullptr && (Life->IsRingPending(DoorId) || Life->IsBellUnneeded(DoorId)))
    {
        return FText::GetEmpty();
    }
    return Ring;
}

FVector APWDoorBellActor::GetInteractionPoint() const
{
    return GetActorTransform().TransformPositionNoScale(FVector(PWDoorBell::PlateDepthCm + 1.6f, 0.0, -2.6));
}

void APWDoorBellActor::Interact(APawn* By)
{
    UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(this);
    if (Life == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("Bell of %s: no lane-life subsystem in this world; the bell does nothing."), *DoorId.ToString());
        return;
    }
    if (Life->IsRingPending(DoorId))
    {
        return;
    }
    Pulse();
    Life->RingDoorBell(DoorId, By);
}

void APWDoorBellActor::Pulse()
{
    PulseRemaining = PWDoorBell::PulseSeconds;
    SetActorTickEnabled(true);
    ApplyPulse(0.0f);
}

void APWDoorBellActor::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (PulseRemaining <= 0.0f)
    {
        SetActorTickEnabled(false);
        return;
    }
    PulseRemaining = FMath::Max(0.0f, PulseRemaining - DeltaSeconds);
    ApplyPulse(1.0f - PulseRemaining / PWDoorBell::PulseSeconds);
    if (PulseRemaining <= 0.0f)
    {
        SetActorTickEnabled(false);
    }
}

void APWDoorBellActor::ApplyPulse(const float Fraction)
{
    // A rise and fall over the pulse: warm and (unless reduced motion is on) pressed in by a little over a millimetre.
    const float Strength = PulseRemaining > 0.0f ? FMath::Sin(PI * FMath::Clamp(Fraction, 0.0f, 1.0f)) : 0.0f;
    const FLinearColor Resting = FLinearColor::FromSRGBColor(DbButtonColour);
    const FLinearColor Lit = FLinearColor::FromSRGBColor(DbButtonLitColour);
    DbPaint(ButtonPaint, FMath::Lerp(Resting, Lit, Strength));
    const bool bReduced = UPerfectWorldPresentationSettings::Get().IsReducedMotion();
    const float Press = bReduced ? 0.0f : PWDoorBell::PulseDepthCm * Strength;
    Button->SetRelativeLocation(FVector(PWDoorBell::PlateDepthCm + 0.8f - Press, 0.0f, -2.6f));
}

// ---- The threshold guard ----

UPWDoorGuardComponent::UPWDoorGuardComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    // Straight after the player has moved this frame: the sample sees where the pawn really is.
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UPWDoorGuardComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    const APWDoorActor* Door = Cast<APWDoorActor>(GetOwner());
    const UWorld* World = GetWorld();
    UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(this);
    if (Door == nullptr || World == nullptr || Life == nullptr || !World->IsGameWorld() || Door->GetDoorId().IsNone())
    {
        bHavePrevious = false;
        return;
    }
    if (GuardState == 0)
    {
        const PerfectWorld::DoorDef* Def = PerfectWorld::FindDoor(PWLaneLife::ToStd(Door->GetDoorId()));
        GuardState = Def != nullptr && Def->Access == PerfectWorld::DoorAccess::Private ? 1 : -1;
    }
    if (GuardState < 0)
    {
        SetComponentTickEnabled(false); // a public or business door: nothing for this guard to decide
        return;
    }
    APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
    const double Side = Life->GetDoorInsideSide(Door->GetDoorId());
    if (Pawn == nullptr || Side == 0.0)
    {
        bHavePrevious = false; // no authored inside side: direction cannot be told, and an exit must never be stopped
        return;
    }

    const FVector Location = Pawn->GetActorLocation();
    const PWDoorBell::FThresholdProbe Now = PWDoorBell::ProbeThreshold(Door->GetActorTransform(), Side, Location);
    if (!Now.bNear)
    {
        bHavePrevious = false;
        return;
    }
    if (bHavePrevious && PWDoorBell::IsInwardCrossing(Previous, Now, static_cast<float>(FVector::Dist(PreviousLocation, Location))))
    {
        FText Why;
        if (!Life->MayEnterThroughDoor(Door->GetDoorId(), Why))
        {
            if (bHaveOutside)
            {
                Pawn->SetActorLocation(LastOutside, false, nullptr, ETeleportType::TeleportPhysics);
            }
            if (const ACharacter* Character = Cast<ACharacter>(Pawn))
            {
                if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
                {
                    Movement->StopMovementImmediately();
                }
            }
            ++BounceCount;
            if (World->GetTimeSeconds() >= NextNoticeSeconds)
            {
                NextNoticeSeconds = World->GetTimeSeconds() + PWDoorBell::GuardNoticeCooldownSeconds;
                Life->ShowPlayerNotice(Why, Pawn);
            }
            bHavePrevious = false;
            return;
        }
    }
    if (Now.Depth < -PWDoorBell::GuardCrossMarginCm)
    {
        LastOutside = Location;
        bHaveOutside = true;
    }
    Previous = Now;
    PreviousLocation = Location;
    bHavePrevious = true;
}
