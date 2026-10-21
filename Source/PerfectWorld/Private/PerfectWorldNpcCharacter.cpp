#include "PerfectWorldNpcCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldNpcLooks.h"
#include "PerfectWorldNpcPilotBridge.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "PerfectWorldNpcCharacter"

namespace PWNpc
{
FVector SpawnCapsuleCentre(const FVector& Feet, const FName PersonId, const EPWNpcOutfit Outfit)
{
    return Feet + FVector(0.0, 0.0, CapsuleHalfHeightCm(PersonId, Outfit) + 2.0);
}

bool HasSpawnClearance(const UWorld& World, const FVector& Feet, const FName PersonId, const EPWNpcOutfit Outfit)
{
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(CapsuleRadius, static_cast<float>(CapsuleHalfHeightCm(PersonId, Outfit)));
    return !World.OverlapBlockingTestByChannel(SpawnCapsuleCentre(Feet, PersonId, Outfit), FQuat::Identity, ECC_Pawn, Shape);
}
} // namespace PWNpc

APWNpcCharacter::APWNpcCharacter()
{
    PrimaryActorTick.bCanEverTick = false; // the movement component ticks, at 10 to 30 Hz

    GetCapsuleComponent()->InitCapsuleSize(PWNpc::CapsuleRadius, static_cast<float>(StandingHeight * 0.5)); // SpawnAt resizes it per person

    // A standing person: no AI controller, no wandering. Physics still runs without a controller so they settle on the floor.
    AutoPossessAI = EAutoPossessAI::Disabled;
    AIControllerClass = nullptr;
    bUseControllerRotationYaw = false;
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->bRunPhysicsWithNoController = true;
    Movement->bOrientRotationToMovement = false;
    Movement->MaxWalkSpeed = 0.0f;            // the movement component sets the pace when it walks them
    Movement->MaxAcceleration = 1400.0f;      // snappy: speed follows the commanded pace, so avoidance stops cleanly
    Movement->BrakingDecelerationWalking = 1800.0f;
    NpcMovement = CreateDefaultSubobject<UPWNpcMovementComponent>(TEXT("NpcMovement"));

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
    const double Height = PWNpc::StandingHeightCm(PersonId, Outfit);
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
    Npc->StandingHeight = Height;
    Npc->GetCapsuleComponent()->InitCapsuleSize(PWNpc::CapsuleRadius, static_cast<float>(Height * 0.5));
    Npc->BaseEyeHeight = static_cast<float>(Height * 0.94 - Height * 0.5);
    Npc->FinishSpawning(At);
    return Npc;
}

void APWNpcCharacter::BeginPlay()
{
    Super::BeginPlay();
    BuildBody();
    NpcMovement->OnPose.AddUObject(this, &APWNpcCharacter::ApplyPose);
}

void APWNpcCharacter::BuildBody()
{
    if (CubeMesh == nullptr || !BodyParts.IsEmpty())
    {
        return;
    }
    const double Height = StandingHeight;
    TArray<FPWNpcBlock> Specs;
    PWNpcLooks::BuildBlocks(PersonId, Outfit, Height, Specs); // keyed by person id, the outfit as the fallback

    UCapsuleComponent* Capsule = GetCapsuleComponent();
    const double HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    for (int32 Index = 0; Index < Specs.Num(); ++Index)
    {
        const FPWNpcBlock& Spec = Specs[Index];
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
        FPWNpcPartRest Rest;
        Rest.Group = Spec.Group;
        Rest.Rest = Spec.Centre;
        Rest.Pivot = Spec.Pivot;
        PartRests.Add(Rest);
    }
}

void APWNpcCharacter::ApplyPose(const FPWNpcPose& Pose)
{
    if (BodyParts.Num() != PartRests.Num())
    {
        return;
    }
    const double Height = StandingHeight;
    const double HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    for (int32 Index = 0; Index < BodyParts.Num(); ++Index)
    {
        FVector P = FVector::ZeroVector;
        FQuat Q = FQuat::Identity;
        PWNpcLooks::PoseBlock(PartRests[Index], Pose, Height, P, Q); // the math is shared with the tests
        BodyParts[Index]->SetRelativeLocationAndRotation(FVector(P.X, P.Y, P.Z - HalfHeight), Q);
    }
}

FText APWNpcCharacter::GetInteractionVerb(const APawn* By) const
{
    // The verb does not depend on bTalkable: a sleeping or busy person is still targeted, and Interact declines politely.
    const UPWConversationPresenter* Presenter = UPWConversationPresenter::Get(this);
    if (Presenter != nullptr && Presenter->IsOpen())
    {
        return FText::GetEmpty(); // someone is already being talked to
    }
    const auto* R55Presence = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UPWNpcPresenceSubsystem>() : nullptr;
    if (const auto* Presence = R55Presence)
    {
        const auto* Decision = Presence->FindPilotDecision(PersonId);
        if (Decision != nullptr && Decision->DeferredUntil)
            return R55NpcReasonText(FName(UTF8_TO_TCHAR(std::string(PerfectWorld::NpcDecisionReasonKey(Decision->Reason)).c_str())));
    }
    if (DisplayName.IsEmpty())
    {
        return LOCTEXT("TalkUnnamed", "Talk");
    }
    return FText::Format(LOCTEXT("TalkTo", "Talk to {0}"), DisplayName);
}

FVector APWNpcCharacter::GetInteractionPoint() const
{
    return GetActorLocation() + FVector(0.0, 0.0, 0.15 * StandingHeight); // chest height: 23 cm above the centre at 154, 27 at 181
}

int32 APWNpcCharacter::GetInteractionPriority() const
{
    return PWNpc::InteractionPriority;
}

FText APWNpcCharacter::GetDeclineText() const
{
    const auto* R55Presence = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UPWNpcPresenceSubsystem>() : nullptr;
    if (const auto* Presence = R55Presence)
    {
        if (const auto* Decision = Presence->FindPilotDecision(PersonId))
            return R55NpcReasonText(FName(UTF8_TO_TCHAR(std::string(PerfectWorld::NpcDecisionReasonKey(Decision->Reason)).c_str())));
    }
    const FName Key = NpcMovement->GetDeclineReasonKey();
    if (Key.IsNone() && NpcMovement->IsTalkable())
    {
        return FText::GetEmpty(); // nothing declines
    }
    const FText Name = DisplayName.IsEmpty() ? LOCTEXT("SomeoneName", "Someone") : DisplayName;
    if (Key == FName(TEXT("npc.reason.asleep")))
    {
        return FText::Format(LOCTEXT("DeclineAsleep", "{0} is asleep."), Name);
    }
    if (Key == FName(TEXT("npc.reason.walking")))
    {
        return FText::Format(LOCTEXT("DeclineWalking", "{0} is on the way somewhere."), Name);
    }
    if (Key == FName(TEXT("npc.reason.working")))
    {
        return FText::Format(LOCTEXT("DeclineWorking", "{0} is in the middle of work."), Name);
    }
    if (Key == FName(TEXT("npc.reason.blocked")))
    {
        return FText::Format(LOCTEXT("DeclineBlocked", "{0} is waiting for the way to clear."), Name);
    }
    return FText::Format(LOCTEXT("DeclineOther", "{0} cannot talk right now."), Name);
}

void APWNpcCharacter::Interact(APawn* By)
{
    auto* R55Presence = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UPWNpcPresenceSubsystem>() : nullptr;
    if (auto* Presence = R55Presence)
    {
        if (Presence->RequestPilotTalk(PersonId, By)) { return; }
    }
    if (!NpcMovement->IsTalkable())
    {
        // Sleeping, occupied or off duty: no conversation opens; a polite notice says why.
        UPWInteractionComponent* Interaction = By != nullptr ? By->FindComponentByClass<UPWInteractionComponent>() : nullptr;
        if (Interaction != nullptr)
        {
            Interaction->ShowNotice(GetDeclineText());
        }
        return;
    }
    UPWConversationPresenter* Presenter = UPWConversationPresenter::Get(this);
    if (Presenter == nullptr || Presenter->IsOpen())
    {
        return;
    }
    if (Presenter->BeginConversationWith(PersonId, DisplayName))
    {
        // Stop, finish the stroke in hand, face the player and hold until the panel closes (reduced motion: face at once).
        NpcMovement->SetLocalConversationActive(true);
    }
}

#undef LOCTEXT_NAMESPACE
