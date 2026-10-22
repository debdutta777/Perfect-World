#pragma once

#include "CoreMinimal.h"
#include "NeighbourhoodPlan.h"
#include "PerfectWorldNeighbourhoodSink.h"

class UMaterialInterface;
class UStaticMesh;
class UWorld;

/**
 * R7-02: the Unreal presentation of the R7-01 neighbourhood overlay (NeighbourhoodPlan.h): carriageways, footpaths, kerbs,
 * markings, a tram strip, building frontages with Kolkata character, the new shop lots, a bus stop, a rickshaw/auto stand,
 * street furniture and the club/pandal open space. Everything is the engine cube in the lane's colour buckets, placed from the
 * plan's features; nothing is hand-placed off-plan. Collision follows the lane's roles: walls and surfaces block everything,
 * solid props carry invisible PropBlock boxes, and the neighbourhood's open ends are closed by Pawn-only barriers.
 *
 * Collision-affecting decisions, in one place:
 *  - Footpaths and aprons are World boxes with tops at NsPath (-185); carriageways at NsGround (-200); crossings cut the footpath
 *    and replace the cut with a 15 cm-in-100 ramp (never a bare kerb) so the walk from the lane to the school is level or ramped.
 *  - Frontage buildings and hoardings are World boxes starting at their plan-derived face line; none touches a plan footprint,
 *    street, crossing or the lane's reserved massing.
 *  - Lamp posts, poles, trees, shelters, boards and benches stand on a kerb edge or a building line and leave at least 120 cm
 *    clear of every footpath (the plan routes run on footpath centres).
 */
namespace PWNs
{
/** A generated frontage building: where it stands and which way it faces. */
struct FNsLot
{
    FNsRect R;
    ENsFace Face = ENsFace::PlusY;
    int32 Storeys = 3;
    uint32 Seed = 0;
    ELaneColour Wall = ELaneColour::WallCream;
    bool bFull = true; // full-detail frontage, or a medium-detail second-row block
};

/** One generated hoarding or compound wall that seals an unbuilt stretch of a frontage line. */
struct FNsWall
{
    FNsRect R;
    double Height = 240.0;
};

/** Everything the streets builders share: the owned plan, what has been claimed, and what the tests read back. */
struct FNsContext
{
    PerfectWorld::NeighbourhoodPlan Plan;
    const PWLane::FLaneScene* Lane = nullptr;
    TArray<FNsLot> Lots;      // generated frontage buildings, first and second row
    TArray<FNsWall> Walls;    // generated hoardings and compound walls
    TArray<FNsRect> Surfaces; // emitted footpath and apron pieces, after crossing cuts
    TArray<FNsRect> Roads;    // emitted carriageway pieces
    TArray<FNsRect> Ramps;    // the ground footprint of each crossing ramp
    int32 SignCount = 0;      // block-lettered signboards emitted

    const PerfectWorld::NeighbourhoodFeature* Feature(const char* Id) const;
    /** XY bounds of a plan feature; an empty rectangle for an unknown id. */
    FNsRect Bounds(const char* Id) const;
    /** Every plan rectangle a building must stay off: footprints, sites, carriageways, footpaths, crossings and the stand-alone reservations. */
    TArray<FNsRect> PlanSolids() const;
    /** True when the rectangle overlaps a plan footprint, site, street, crossing or the lane's/campus's/club's reserved massing. */
    bool BlockedByPlan(const FNsRect& R) const;
    /** True when the rectangle overlaps a plan footpath or carriageway (used to find the side of a building that shows on a street). */
    bool TouchesStreet(const FNsRect& R) const;
    /** True when the rectangle overlaps an earlier generated lot or wall. */
    bool Claimed(const FNsRect& R) const;
};

/** Builds every part of the neighbourhood overlay into the sink. The order is fixed so that claims are deterministic. */
void NsBuildAll(FNsSink& K, FNsContext& Ctx);

// The pieces, each in its own translation unit; NsBuildAll calls them in this order.
void NsBuildRoads(FNsSink& K, FNsContext& Ctx);
void NsBuildFrontages(FNsSink& K, FNsContext& Ctx);
void NsBuildShopLots(FNsSink& K, FNsContext& Ctx);
void NsBuildStalls(FNsSink& K, FNsContext& Ctx); // the tea and vendor pitches and the rickshaw/auto stand (called by NsBuildShopLots)
void NsBuildClubAndPandal(FNsSink& K, FNsContext& Ctx);
void NsBuildStreetFurniture(FNsSink& K, FNsContext& Ctx);
void NsBuildBoundaries(FNsSink& K, FNsContext& Ctx);
} // namespace PWNs

namespace PWLane
{
/**
 * Builds the R7-01 neighbourhood overlay into the world. Call it once after BuildLaneBarriers inside BuildKolkataLane, with the
 * finished lane scene (see the hook in PerfectWorldNeighbourhoodStreetsSpawn.cpp). It loads the engine cube and basic-shape
 * material itself; the overload takes them when the caller already has them. Returns the number of block instances placed.
 */
int32 SpawnNeighbourhoodStreets(UWorld& World, const FLaneScene& Scene);
int32 SpawnNeighbourhoodStreets(UWorld& World, const FLaneScene& Scene, UStaticMesh& Cube, UMaterialInterface* BaseMaterial);
} // namespace PWLane
