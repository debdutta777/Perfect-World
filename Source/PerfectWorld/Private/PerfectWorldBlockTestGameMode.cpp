#include "PerfectWorldBlockTestGameMode.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
// The field is 81 x 81 one-metre blocks. The player spawns at the origin,
// two metres above the block tops, looking along +X.
constexpr int32 BlockTestHalfExtent = 40;
constexpr float BlockTestBlockCm = 100.0f;
constexpr float BlockTestGroundTopCm = -200.0f;

/** A small integer hash so the towers land in the same places every run. */
uint32 BlockTestHash(int32 X, int32 Y)
{
    uint32 H = static_cast<uint32>(X) * 0x9E3779B1u ^ static_cast<uint32>(Y) * 0x85EBCA77u;
    H ^= H >> 15;
    H *= 0xC2B2AE3Du;
    H ^= H >> 13;
    return H;
}
} // namespace

APerfectWorldBlockTestGameMode::APerfectWorldBlockTestGameMode()
{
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded())
    {
        BlockMesh = Cube.Object;
    }
    DefaultPawnClass = ADefaultPawn::StaticClass();
}

void APerfectWorldBlockTestGameMode::StartPlay()
{
    BuildSkyAndSun();
    BuildBlockField();
    Super::StartPlay();
}

void APerfectWorldBlockTestGameMode::BuildBlockField()
{
    UWorld* World = GetWorld();
    if (World == nullptr || BlockMesh == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("Block test scene: no world or no cube mesh; nothing built."));
        return;
    }

    AActor* Field = World->SpawnActor<AActor>();
    UInstancedStaticMeshComponent* Blocks = NewObject<UInstancedStaticMeshComponent>(Field, TEXT("Blocks"));
    Blocks->SetStaticMesh(BlockMesh);
    Blocks->SetMobility(EComponentMobility::Static);
    Field->SetRootComponent(Blocks);
    Blocks->RegisterComponent();

    const float GroundCentreZ = BlockTestGroundTopCm - BlockTestBlockCm * 0.5f;
    int32 Placed = 0;
    for (int32 Y = -BlockTestHalfExtent; Y <= BlockTestHalfExtent; ++Y)
    {
        for (int32 X = -BlockTestHalfExtent; X <= BlockTestHalfExtent; ++X)
        {
            Blocks->AddInstance(FTransform(FVector(X * BlockTestBlockCm, Y * BlockTestBlockCm, GroundCentreZ)));
            ++Placed;

            // Towers of 3 to 12 blocks, never within eight metres of the spawn.
            const uint32 H = BlockTestHash(X, Y);
            const bool bNearSpawn = FMath::Abs(X) < 8 && FMath::Abs(Y) < 8;
            if (!bNearSpawn && H % 37u == 0u)
            {
                const int32 Height = 3 + static_cast<int32>((H >> 8) % 10u);
                for (int32 Z = 1; Z <= Height; ++Z)
                {
                    Blocks->AddInstance(FTransform(FVector(X * BlockTestBlockCm, Y * BlockTestBlockCm, GroundCentreZ + Z * BlockTestBlockCm)));
                    ++Placed;
                }
            }
        }
    }
    UE_LOG(LogTemp, Display, TEXT("Block test scene: %d one-metre blocks placed."), Placed);
}

void APerfectWorldBlockTestGameMode::BuildSkyAndSun()
{
    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return;
    }

    // The engine Entry map is empty and unlit: add an atmosphere, a sun that
    // drives it, and a sky light that captures it in real time.
    World->SpawnActor<ASkyAtmosphere>();

    ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-40.0f, 30.0f, 0.0f));
    if (Sun != nullptr)
    {
        if (UDirectionalLightComponent* SunLight = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
        {
            SunLight->SetMobility(EComponentMobility::Movable);
            SunLight->SetAtmosphereSunLight(true);
        }
    }

    ASkyLight* Sky = World->SpawnActor<ASkyLight>();
    if (Sky != nullptr)
    {
        if (USkyLightComponent* SkyLight = Sky->GetLightComponent())
        {
            SkyLight->SetMobility(EComponentMobility::Movable);
            SkyLight->bRealTimeCapture = true;
            SkyLight->MarkRenderStateDirty();
            SkyLight->RecaptureSky();
        }
    }
}
