#include "PerfectWorldNeighbourhoodStreets.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectGlobals.h"

/*
 * HOOK for the integrator (PerfectWorldKolkataLane.cpp is integrator-owned and was not edited by R7-02).
 *
 * 1. In BuildKolkataLane, after ReportFootpathRoutes(Scene) and before or after Scene.Blocks.Commit (the overlay has its own
 *    blocks, so order does not matter), add:
 *
 *        SpawnNeighbourhoodStreets(World, Scene);
 *
 *    and include "PerfectWorldNeighbourhoodStreets.h". It is in namespace PWLane, like BuildKolkataLane.
 *
 * 2. In AddGroundAndRoad, remove (or replace with the junction cuts the overlay draws itself) only the original north-alley box
 *    K.Box(ELaneColour::Asphalt, FVector(AlleyPlusA, RoadHalfWidth, GZ - 20.0), FVector(AlleyPlusB, 1500.0, GZ));
 *    The overlay lays the 360 cm cross-street carriageway and two 120 cm footpaths there and covers the lane's +Y drain strip
 *    across the mouth with a 3 cm asphalt lip, so the game looks right even if the old box stays (it is just redundant asphalt).
 *    Keep the -Y alley box, the lane asphalt, ground and every frontage threshold.
 *
 * The other two R7-01 masks live in files this order owns and are already applied: the north-alley stopping barrier in
 * PerfectWorldLaneBarriers.cpp (SealGap) and the decorative back rows/skyline in PerfectWorldLaneBuildings.cpp (BuildStrip).
 */

namespace PWLane
{
int32 SpawnNeighbourhoodStreets(UWorld& World, const FLaneScene& Scene, UStaticMesh& Cube, UMaterialInterface* BaseMaterial)
{
    PWNs::FNsContext Ctx;
    Ctx.Plan = PerfectWorld::BuildNeighbourhoodPlan(0);
    Ctx.Lane = &Scene;
    const std::vector<std::string> Problems = PerfectWorld::ValidateNeighbourhoodPlan(Ctx.Plan);
    if (!Problems.empty())
    {
        UE_LOG(LogTemp, Error, TEXT("Neighbourhood streets: the plan does not validate (%d problems, first: %hs); building from it anyway."),
               static_cast<int32>(Problems.size()), Problems.front().c_str());
    }
    FLaneBlocks Blocks;
    PWNs::FNsSink Sink(Blocks);
    PWNs::NsBuildAll(Sink, Ctx);
    int32 Instances = 0;
    const int32 Components = Blocks.Commit(World, Cube, BaseMaterial, Instances);
    UE_LOG(LogTemp, Display, TEXT("Neighbourhood streets: %d block instances in %d components; %d frontage buildings, %d walls, %d lettered signs."),
           Instances, Components, Ctx.Lots.Num(), Ctx.Walls.Num(), Ctx.SignCount);
    return Instances;
}

int32 SpawnNeighbourhoodStreets(UWorld& World, const FLaneScene& Scene)
{
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UMaterialInterface* Paint = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (Cube == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("Neighbourhood streets: the engine cube was not found; nothing built."));
        return 0;
    }
    if (Paint == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("Neighbourhood streets: BasicShapeMaterial not found; the overlay will be unpainted."));
    }
    return SpawnNeighbourhoodStreets(World, Scene, *Cube, Paint);
}
} // namespace PWLane
