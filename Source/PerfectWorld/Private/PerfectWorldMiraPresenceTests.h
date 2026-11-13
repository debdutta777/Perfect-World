#pragma once

#include "PerfectWorldNpcPresenceTests.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PerfectWorldLaneStationeryShop.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldNpcMovementComponent.h"

namespace UaMiraPresence
{
// The passing NpcRoute harness, translated into the production provider's building frame.
// Shared with the roster probe so an earlier shop arrival actually walks clear of the threshold.
struct FUaShopPhysics
{
    static constexpr double UaFrameSeconds = 1.0 / 120.0;
    double UaSinceTick = 0.0;

    void UaBuild(UWorld& UaWorld, const PWLane::FLaneBuilding& UaBuilding) const
    {
        const double UaFloor = PWLane::GetStationeryLayout().FinishedFloorCm;
        const auto UaBox = [&](const FVector& UaMin, const FVector& UaMax)
        {
            AActor* UaSolid = UaWorld.SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
            if (UaSolid == nullptr) { return; }
            UBoxComponent* UaShape = NewObject<UBoxComponent>(UaSolid, TEXT("UaSolid"));
            UaShape->SetMobility(EComponentMobility::Movable);
            UaShape->SetBoxExtent((UaMax - UaMin) * 0.5);
            UaShape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            UaShape->SetCollisionObjectType(ECC_WorldStatic);
            UaShape->SetCollisionResponseToAllChannels(ECR_Block);
            UaSolid->SetRootComponent(UaShape);
            UaShape->RegisterComponent();
            const FVector UaCentre = (UaMin + UaMax) * 0.5;
            UaSolid->SetActorLocation(UaBuilding.Frame.Pt(UaCentre.X, UaCentre.Y, UaFloor + UaCentre.Z));
        };
        UaBox(FVector(-100.0, -150.0, -20.0), FVector(800.0, 800.0, 0.0));
        UaBox(FVector(25.0, 435.0, 0.0), FVector(568.0, 447.0, 274.0));
        UaBox(FVector(662.0, 435.0, 0.0), FVector(675.0, 447.0, 274.0));
        UaBox(FVector(568.0, 435.0, 212.0), FVector(662.0, 447.0, 274.0));
        UaBox(FVector(395.0, 447.0, 0.0), FVector(555.0, 507.0, 78.0));
        UaBox(FVector(25.0, 225.0, 0.0), FVector(555.0, 285.0, 90.0));
        UaBox(FVector(675.0, 25.0, 0.0), FVector(700.0, 675.0, 274.0));
        UaBox(FVector(0.0, 675.0, 0.0), FVector(700.0, 700.0, 274.0));
    }

    void UaBegin(APWNpcCharacter& UaActor)
    {
        UaSinceTick = 0.0;
        UaActor.GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }

    void UaFrame(UWorld& UaWorld, APWNpcCharacter& UaActor)
    {
        UaWorld.TimeSeconds += UaFrameSeconds;
        UaActor.GetCapsuleComponent()->SetLastRenderTime(static_cast<float>(UaWorld.TimeSeconds));
        UaSinceTick += UaFrameSeconds;
        UPWNpcMovementComponent* UaMove = UaActor.GetNpcMovement();
        const double UaInterval = UaMove->GetComponentTickInterval();
        if (UaInterval <= 0.0 || UaSinceTick >= UaInterval - 1e-6)
        {
            UaMove->TickPresentation(static_cast<float>(UaSinceTick));
            UaSinceTick = 0.0;
        }
        UCharacterMovementComponent* UaBody = UaActor.GetCharacterMovement();
        UaBody->TickComponent(static_cast<float>(UaFrameSeconds), LEVELTICK_All, &UaBody->PrimaryComponentTick);
    }
};
}
#endif
