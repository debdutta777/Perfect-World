#include "PerfectWorldVendorCart.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LaneAmbient.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldNpcPresenceSubsystem.h"

namespace
{
FVector AmCartVector(const PerfectWorld::AmbientPoint& P) { return FVector(P.X, P.Y, P.Height); }
}
namespace PWLane
{
void AmbientMoveIgnoreParams(const AActor& Owner, FCollisionQueryParams& Params)
{
    if (const auto* Root = Cast<UPrimitiveComponent>(Owner.GetRootComponent()))
        for (const AActor* Ignored : Root->GetMoveIgnoreActors()) { Params.AddIgnoredActor(Ignored); }
}
bool AmbientAllowsSnap(const AActor& Owner)
{
    return !Owner.Tags.Contains(FName(TEXT("npc.presentation.no_snap")));
}
}
APWVendorCart::APWVendorCart()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("AmCartBounds"));
    SetRootComponent(Bounds);
    Bounds->SetBoxExtent(AmCartVector(PerfectWorld::AmbientCartHalfExtent));
    Bounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Bounds->SetCollisionObjectType(ECC_WorldDynamic);
    Bounds->SetCollisionResponseToAllChannels(ECR_Block);
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    const auto Part = [this, Cube](const TCHAR* Name, FVector Centre, FVector Size)
    {
        auto* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Mesh->SetupAttachment(Bounds);
        Mesh->SetStaticMesh(Cube);
        Mesh->SetRelativeLocation(Centre);
        Mesh->SetRelativeScale3D(Size / 100.0);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetCastShadow(false);
    };
    Part(TEXT("AmCartTray"), FVector(0, 0, 20), FVector(90, 64, 12));
    Part(TEXT("AmCartCrate"), FVector(0, 0, 32), FVector(70, 50, 20));
    Part(TEXT("AmCartAxle"), FVector(0, 0, -25), FVector(12, 70, 12));
    Part(TEXT("AmCartWheelL"), FVector(0, -28, -28), FVector(28, 8, 28));
    Part(TEXT("AmCartWheelR"), FVector(0, 28, -28), FVector(28, 8, 28));
    Part(TEXT("AmCartHandle"), FVector(0, 47, 12), FVector(70, 30, 8));
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
}
APWVendorCart* APWVendorCart::GetOrSpawn(UWorld& World)
{
    for (TActorIterator<APWVendorCart> It(&World); It; ++It) { return *It; }
    return World.SpawnActor<APWVendorCart>();
}
void APWVendorCart::Tick(float Dt)
{
    Super::Tick(Dt);
    const auto* Presence = GetWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
    APWNpcCharacter* Person = Presence ? Presence->FindPerson(FName(TEXT("person.purnima_das"))) : nullptr;
    if (Person == nullptr)
    {
        Vendor.Reset();
        bPlaced = false;
        bRouteBlocked = false;
        SetActorHiddenInGame(true);
        SetActorEnableCollision(false);
        return;
    }
    if (Vendor.Get() != Person)
    {
        Vendor = Person;
        bPlaced = false;
        LastVendorCentre = Person->GetActorLocation();
        // Integrator seam: TrySnap must decline this tag; SweepBlocked/TrySnap must
        // copy the root primitive's move-ignore actors into their query params.
        // The latter lets a solid cart block pedestrians without blocking its owner.
        Person->Tags.AddUnique(FName(TEXT("npc.presentation.no_snap")));
        // The cart must not obstruct its own vendor's capsule/avoidance sweeps.
        Person->GetCapsuleComponent()->IgnoreActorWhenMoving(this, true);
        Bounds->IgnoreActorWhenMoving(Person, true);
    }
    const FVector Centre = Person->GetActorLocation();
    const FVector Feet = Centre - FVector(0, 0, Person->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0);
    const FVector Desired = Feet + AmCartVector(PerfectWorld::AmbientCartOffset);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(AmCartClearance), false, this);
    Params.AddIgnoredActor(Person);
    const FCollisionShape Shape = FCollisionShape::MakeBox(Bounds->GetUnscaledBoxExtent());
    FHitResult Hit;
    const bool Occupied = GetWorld()->OverlapBlockingTestByChannel(Desired, FQuat::Identity, ECC_Pawn, Shape, Params);
    const bool Swept = bPlaced && GetWorld()->SweepSingleByChannel(Hit, GetActorLocation(), Desired, FQuat::Identity, ECC_Pawn, Shape, Params);
    // Refuse a presenter's offscreen catch-up snap as well: a paired cart never jumps over a blocker.
    const bool TooFar = bPlaced && FVector::Dist(Centre, LastVendorCentre) > FMath::Max(60.0, 240.0 * Dt);
    bRouteBlocked = Occupied || Swept || TooFar;
    if (bRouteBlocked)
    {
        ++RefusedMoves;
        Person->GetCharacterMovement()->StopMovementImmediately();
        Person->ConsumeMovementInputVector();
        // Transactional rollback of this frame's move to the last collision-free pair.
        Person->SetActorLocation(LastVendorCentre, false);
        Person->ApplyPose(PWNpcGesture::Yielding());
        return;
    }
    SetActorLocation(Desired, false);
    LastVendorCentre = Centre;
    bPlaced = true;
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
    if (Person->GetNpcMovement()->GetPresentedState() == EPWNpcState::Chat)
    {
        Person->GetCharacterMovement()->StopMovementImmediately();
    }
}
