#include "PerfectWorldNpcCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldPresentationSettings.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "PerfectWorldNpcCharacter"

namespace
{
/** One block of the placeholder body. Centre is X forward, Y right, Z above the floor; Size is the full extent; all in cm. */
struct FPartSpec
{
    FVector Centre;
    FVector Size;
    FColor Tint;
};

void Put(TArray<FPartSpec>& Out, const FVector& Centre, const FVector& Size, const FColor& Tint)
{
    FPartSpec Spec;
    Spec.Centre = Centre;
    Spec.Size = Size;
    Spec.Tint = Tint;
    Out.Add(Spec);
}

double HeightFor(const EPWNpcOutfit Outfit)
{
    return Outfit == EPWNpcOutfit::HouseholdAdult ? 162.0 : 168.0;
}

/**
 * The blocks for an outfit, proportioned from the standing height H. Two people differ in at least two silhouette
 * attributes (LANE_LIFE_PLAN section B): the household adult has a hip-length overshirt and a key pouch; the parent has
 * a belted plain shirt and fuller dark hair.
 */
void AppendParts(const EPWNpcOutfit Outfit, const double H, TArray<FPartSpec>& Out)
{
    const double LegHeight = 0.48 * H;
    const double TorsoHeight = 0.36 * H;
    const double TorsoCentre = 0.66 * H;
    const double SleeveHeight = 0.20 * H;
    const double ForearmHeight = 0.14 * H;
    const double HeadHeight = 0.152 * H;

    if (Outfit == EPWNpcOutfit::HouseholdAdult)
    {
        const FColor Skin(176, 128, 92), Trousers(70, 74, 84), Cream(214, 206, 186), Sage(128, 150, 118);
        const FColor Silver(192, 192, 198), Leather(112, 80, 54);
        Put(Out, FVector(0.0, -9.0, LegHeight * 0.5), FVector(14.0, 14.0, LegHeight), Trousers);
        Put(Out, FVector(0.0, 9.0, LegHeight * 0.5), FVector(14.0, 14.0, LegHeight), Trousers);
        Put(Out, FVector(0.0, 0.0, TorsoCentre), FVector(20.0, 40.0, TorsoHeight), Cream);
        // The sage overshirt, hip length, with the cream shirt showing down the open front.
        Put(Out, FVector(0.0, 0.0, 0.62 * H), FVector(24.0, 44.0, 0.44 * H), Sage);
        Put(Out, FVector(12.5, 0.0, TorsoCentre), FVector(2.0, 9.0, TorsoHeight), Cream);
        for (const double Side : {-1.0, 1.0})
        {
            Put(Out, FVector(0.0, Side * 26.0, 0.84 * H - SleeveHeight * 0.5), FVector(12.0, 12.0, SleeveHeight), Sage);
            Put(Out, FVector(0.0, Side * 26.0, 0.84 * H - SleeveHeight - ForearmHeight * 0.5), FVector(9.0, 9.0, ForearmHeight), Skin);
        }
        Put(Out, FVector(0.0, 0.0, H - HeadHeight * 0.5), FVector(20.0, 20.0, HeadHeight), Skin);
        Put(Out, FVector(0.0, 0.0, H - 3.5), FVector(22.0, 22.0, 7.0), Silver);      // silver-streaked hair
        Put(Out, FVector(3.0, 25.0, 0.47 * H), FVector(8.0, 11.0, 12.0), Leather);    // the key pouch
    }
    else
    {
        const FColor Skin(206, 160, 122), Trousers(58, 60, 66), Shirt(216, 210, 194), Belt(34, 32, 34), Hair(44, 36, 34);
        Put(Out, FVector(0.0, -9.5, LegHeight * 0.5), FVector(15.0, 15.0, LegHeight), Trousers);
        Put(Out, FVector(0.0, 9.5, LegHeight * 0.5), FVector(15.0, 15.0, LegHeight), Trousers);
        Put(Out, FVector(0.0, 0.0, TorsoCentre), FVector(20.0, 42.0, TorsoHeight), Shirt);
        Put(Out, FVector(0.0, 0.0, LegHeight + 1.5), FVector(22.0, 43.0, 3.0), Belt);
        for (const double Side : {-1.0, 1.0})
        {
            Put(Out, FVector(0.0, Side * 27.0, 0.84 * H - SleeveHeight * 0.5), FVector(12.0, 12.0, SleeveHeight), Shirt);
            Put(Out, FVector(0.0, Side * 27.0, 0.84 * H - SleeveHeight - ForearmHeight * 0.5), FVector(9.0, 9.0, ForearmHeight), Skin);
        }
        Put(Out, FVector(0.0, 0.0, H - HeadHeight * 0.5), FVector(20.0, 20.0, HeadHeight), Skin);
        Put(Out, FVector(0.0, 0.0, H - 4.5), FVector(22.0, 22.0, 9.0), Hair);          // fuller, dark hair
        Put(Out, FVector(-8.0, 0.0, H - 0.1 * H), FVector(6.0, 21.0, 0.1 * H), Hair);
    }

    // A minimal face on the +X side (the actor's forward), so a player can read which way a person faces: the first
    // packaged walk-in (2026-10-14) could not tell a symmetric block head's front from its back.
    const FColor Eye(30, 26, 26), Brow(52, 40, 36), Nose(150, 104, 78);
    const double EyeZ = H - 0.075 * H;
    for (const double Side : {-1.0, 1.0})
    {
        Put(Out, FVector(10.6, Side * 4.5, EyeZ), FVector(1.2, 3.0, 2.4), Eye);
        Put(Out, FVector(10.6, Side * 4.5, EyeZ + 3.2), FVector(1.2, 4.2, 1.0), Brow);
    }
    Put(Out, FVector(11.2, 0.0, EyeZ - 4.0), FVector(2.4, 2.4, 3.6), Nose);
}
} // namespace

APWNpcCharacter::APWNpcCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    GetCapsuleComponent()->InitCapsuleSize(PWNpc::CapsuleRadius, static_cast<float>(HeightFor(Outfit) * 0.5));

    // A standing person: no AI controller, no wandering. Physics still runs without a controller so they settle on the floor.
    AutoPossessAI = EAutoPossessAI::Disabled;
    AIControllerClass = nullptr;
    bUseControllerRotationYaw = false;
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->bRunPhysicsWithNoController = true;
    Movement->bOrientRotationToMovement = false;
    Movement->MaxWalkSpeed = 0.0f;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Paint(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (Cube.Succeeded())
    {
        CubeMesh = Cube.Object;
    }
    if (Paint.Succeeded())
    {
        BodyBaseMaterial = Paint.Object;
    }
}

APWNpcCharacter* APWNpcCharacter::SpawnAt(UWorld& World, const FTransform& Transform, const FName PersonId,
                                          const FText& DisplayName, const EPWNpcOutfit Outfit)
{
    const double Height = HeightFor(Outfit);
    FTransform At = Transform;
    At.AddToTranslation(FVector(0.0, 0.0, Height * 0.5)); // the transform is where the feet stand; the capsule is centred
    APWNpcCharacter* Npc = World.SpawnActorDeferred<APWNpcCharacter>(
        StaticClass(), At, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
    if (Npc == nullptr)
    {
        return nullptr;
    }
    Npc->PersonId = PersonId;
    Npc->DisplayName = DisplayName;
    Npc->Outfit = Outfit;
    Npc->GetCapsuleComponent()->InitCapsuleSize(PWNpc::CapsuleRadius, static_cast<float>(Height * 0.5));
    Npc->BaseEyeHeight = static_cast<float>(Height * 0.94 - Height * 0.5);
    Npc->FinishSpawning(At);
    return Npc;
}

void APWNpcCharacter::BeginPlay()
{
    Super::BeginPlay();
    HomeYaw = GetActorRotation().Yaw; // the way they face when no one is near: as placed
    BuildBody();
}

void APWNpcCharacter::BuildBody()
{
    if (CubeMesh == nullptr || !BodyParts.IsEmpty())
    {
        return;
    }
    const double Height = HeightFor(Outfit);
    TArray<FPartSpec> Specs;
    AppendParts(Outfit, Height, Specs);

    UCapsuleComponent* Capsule = GetCapsuleComponent();
    const double HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    for (int32 Index = 0; Index < Specs.Num(); ++Index)
    {
        const FPartSpec& Spec = Specs[Index];
        UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("NpcBodyPart%d"), Index));
        Part->SetStaticMesh(CubeMesh);
        Part->SetupAttachment(Capsule);
        // The engine cube is 100 cm on a side, centred on its pivot; the capsule's origin is its centre.
        Part->SetRelativeLocation(FVector(Spec.Centre.X, Spec.Centre.Y, Spec.Centre.Z - HalfHeight));
        Part->SetRelativeScale3D(Spec.Size / 100.0);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision); // the capsule is the body for collision
        Part->SetCanEverAffectNavigation(false);
        Part->RegisterComponent();
        if (BodyBaseMaterial != nullptr)
        {
            UMaterialInstanceDynamic* Paint = UMaterialInstanceDynamic::Create(BodyBaseMaterial, this);
            const FLinearColor Linear = FLinearColor::FromSRGBColor(Spec.Tint);
            Paint->SetVectorParameterValue(TEXT("Color"), Linear);
            Paint->SetVectorParameterValue(TEXT("BaseColor"), Linear);
            Part->SetMaterial(0, Paint);
            BodyPaints.Add(Paint);
        }
        BodyParts.Add(Part);
    }
}

void APWNpcCharacter::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // Presentation only. With reduced motion a person stands as placed and faces the player only when spoken to.
    if (UPerfectWorldPresentationSettings::Get().IsReducedMotion())
    {
        return;
    }
    const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    double TargetYaw = HomeYaw;
    if (Player != nullptr && FVector::DistSquared2D(Player->GetActorLocation(), GetActorLocation()) <= FMath::Square(static_cast<double>(PWNpc::LookAtRangeCm)))
    {
        const FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
        TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(ToPlayer.Y, ToPlayer.X));
    }
    FRotator Rotation = GetActorRotation();
    const double Step = static_cast<double>(PWNpc::TurnDegreesPerSecond) * static_cast<double>(DeltaSeconds);
    const double Delta = FMath::FindDeltaAngleDegrees(Rotation.Yaw, TargetYaw);
    if (FMath::Abs(Delta) > 0.01)
    {
        Rotation.Yaw += FMath::Clamp(Delta, -Step, Step);
        SetActorRotation(Rotation);
    }
}

void APWNpcCharacter::FacePlayerNow(const APawn& Player)
{
    const FVector ToPlayer = Player.GetActorLocation() - GetActorLocation();
    FRotator Rotation = GetActorRotation();
    Rotation.Yaw = FMath::RadiansToDegrees(FMath::Atan2(ToPlayer.Y, ToPlayer.X));
    SetActorRotation(Rotation);
}

FText APWNpcCharacter::GetInteractionVerb(const APawn* By) const
{
    const UPWConversationPresenter* Presenter = UPWConversationPresenter::Get(this);
    if (Presenter != nullptr && Presenter->IsOpen())
    {
        return FText::GetEmpty(); // someone is already being talked to
    }
    if (DisplayName.IsEmpty())
    {
        return LOCTEXT("TalkUnnamed", "Talk");
    }
    return FText::Format(LOCTEXT("TalkTo", "Talk to {0}"), DisplayName);
}

FVector APWNpcCharacter::GetInteractionPoint() const
{
    return GetActorLocation() + FVector(0.0, 0.0, 25.0); // chest height for either outfit
}

int32 APWNpcCharacter::GetInteractionPriority() const
{
    return PWNpc::InteractionPriority;
}

void APWNpcCharacter::Interact(APawn* By)
{
    UPWConversationPresenter* Presenter = UPWConversationPresenter::Get(this);
    if (Presenter == nullptr || Presenter->IsOpen())
    {
        return;
    }
    if (Presenter->BeginConversationWith(PersonId, DisplayName) && By != nullptr
        && UPerfectWorldPresentationSettings::Get().IsReducedMotion())
    {
        FacePlayerNow(*By);
    }
}

#undef LOCTEXT_NAMESPACE
