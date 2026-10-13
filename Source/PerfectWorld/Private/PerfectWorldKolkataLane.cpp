#include "PerfectWorldKolkataLane.h"

#include "Components/PointLightComponent.h"
#include "Engine/PointLight.h"
#include "Engine/World.h"
#include "PerfectWorldLaneBarriers.h"
#include "PerfectWorldLaneBlocks.h"
#include "PerfectWorldLaneBuildings.h"
#include "PerfectWorldLaneProps.h"
#include "PerfectWorldLaneRoutes.h"

namespace PWLane
{
namespace
{
constexpr int32 WarmLightCount = 4;
constexpr double WarmLightFocusX = 900.0; // lights go to the stalls nearest this point on the lane

/** The ground, the road, side alleys, drains, patches and manhole covers. The footpaths belong to the buildings. */
void AddGroundAndRoad(FLaneBlocks& K)
{
    constexpr double GZ = GroundZ;

    // Flat packed earth well past the skyline; its top sits 5 cm under the road so the two never fight.
    K.Box(ELaneColour::Ground, FVector(-30000.0, -30000.0, GZ - 400.0), FVector(40000.0, 30000.0, GZ - 5.0));

    K.Box(ELaneColour::Asphalt, FVector(LaneX0, -RoadHalfWidth, GZ - 20.0), FVector(LaneX1, RoadHalfWidth, GZ));
    K.Box(ELaneColour::Asphalt, FVector(AlleyPlusA, RoadHalfWidth, GZ - 20.0), FVector(AlleyPlusB, 1500.0, GZ));
    K.Box(ELaneColour::Asphalt, FVector(AlleyMinusA, -1500.0, GZ - 20.0), FVector(AlleyMinusB, -RoadHalfWidth, GZ));

    // Open drains along both road edges.
    K.Box(ELaneColour::Drain, FVector(LaneX0, -RoadHalfWidth, GZ), FVector(LaneX1, -RoadHalfWidth + 25.0, GZ + 2.0));
    K.Box(ELaneColour::Drain, FVector(LaneX0, RoadHalfWidth - 25.0, GZ), FVector(LaneX1, RoadHalfWidth, GZ + 2.0));

    // Patched tarmac and worn spots, scattered by the hash.
    for (int32 I = 0; I < 8; ++I)
    {
        const uint32 R = LaneHash(55, I);
        const double X = LaneX0 + 300.0 + LaneUnit(R) * (LaneX1 - LaneX0 - 600.0);
        const double Y = (LaneUnit(R >> 8) - 0.5) * 360.0;
        const double Sx = 160.0 + LaneUnit(R >> 14) * 260.0;
        const double Sy = 80.0 + LaneUnit(R >> 18) * 120.0;
        K.Box(ELaneColour::RoadPatch, FVector(X - Sx * 0.5, Y - Sy * 0.5, GZ), FVector(X + Sx * 0.5, Y + Sy * 0.5, GZ + 1.5));
    }
    for (const double X : {1300.0, 3600.0, 5200.0})
    {
        const double Y = (X > 3000.0) ? -40.0 : 30.0;
        K.Box(ELaneColour::Drain, FVector(X - 35.0, Y - 35.0, GZ), FVector(X + 35.0, Y + 35.0, GZ + 2.0));
    }
}

/** A handful of warm point lights at the stalls nearest the starting view; no shadows, so they stay cheap. */
void SpawnWarmLights(UWorld& World, const TArray<FVector>& Spots)
{
    TArray<FVector> Sorted = Spots;
    Sorted.Sort([](const FVector& A, const FVector& B)
                { return FMath::Abs(A.X - WarmLightFocusX) < FMath::Abs(B.X - WarmLightFocusX); });
    const int32 Count = FMath::Min(WarmLightCount, Sorted.Num());
    for (int32 I = 0; I < Count; ++I)
    {
        APointLight* Light = World.SpawnActor<APointLight>(Sorted[I], FRotator::ZeroRotator);
        UPointLightComponent* Component = Light != nullptr ? Cast<UPointLightComponent>(Light->GetLightComponent()) : nullptr;
        if (Component == nullptr)
        {
            continue;
        }
        Component->SetMobility(EComponentMobility::Movable);
        Component->IntensityUnits = ELightUnits::Lumens;
        Component->SetLightColor(FLinearColor(1.0f, 0.72f, 0.42f));
        Component->SetIntensity(600.0f);
        Component->SetAttenuationRadius(650.0f);
        Component->SetCastShadows(false);
    }
}
} // namespace

void BuildKolkataLane(UWorld& World, UStaticMesh& Cube, UMaterialInterface* BaseMaterial)
{
    FLaneScene Scene;
    AddGroundAndRoad(Scene.Blocks);
    BuildLaneRows(Scene);
    BuildLaneProps(Scene);
    BuildLaneBarriers(Scene);
    ReportFootpathRoutes(Scene); // development builds only: logs the narrowest clear footpath width

    int32 Instances = 0;
    const int32 Components = Scene.Blocks.Commit(World, Cube, BaseMaterial, Instances);
    SpawnWarmLights(World, Scene.LightSpots);

    UE_LOG(LogTemp, Display, TEXT("Kolkata lane: %d block instances in %d components, %d stall lights placed."), Instances, Components,
           FMath::Min(WarmLightCount, Scene.LightSpots.Num()));
}
} // namespace PWLane
