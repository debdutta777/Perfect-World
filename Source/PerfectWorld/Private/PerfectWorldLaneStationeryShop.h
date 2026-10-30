#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldLaneInteriorKit.h"

struct FNpBindingProvider;

namespace PWLane
{
/**
 * The stationery shop (LANE_LIFE_PLAN stage 4, "Stationery shop"): the interior kit's public variant. Anindita Sen's shop, the
 * lane's second public interior. Ground floor only; the upper storeys stay a solid block so the street facade is unchanged.
 *
 * Which building: the first-row building on the +Y side with Index 2. BuildFirstRows gives side +1 the hash
 * LaneHash(138, 2, 11) = 2673104671 (Side * 131 + 7 = 138), which yields X -100 to 600 cm (Frame origin (-100, 460), U = +X,
 * V = +Y), Width 700 (600 + 100 * (2673104671 % 6 = 1)), Depth 700 (700 + 100 * ((H >> 16) % 3 = 0)), Setback 60
 * ((H >> 12) % 4 = 1), 3 storeys of 300 cm, and wall colour WallButter. It sits between two others the generator authors for
 * people: the tea shop (Index 3, X 600 to 1300) is its east neighbour. The generic ground floor would give it open stalls or
 * roller shutters; the integrator calls BuildStationeryShop INSTEAD of BuildLaneGroundFloor and AddWallVolume.
 * IsStationeryBuilding checks the whole footprint, so a generator change makes the lane fall back to the old building.
 *
 * The shell, furniture, anchors, clearances and lights come from the portable kit (LaneInteriorLayout.h, variant
 * ShopWithStockroom, furniture set Stationery); this file adds only what is particular to the shop: the sign, the awning, the plinth,
 * the exercise books, pens, parcels and cartons as coloured blocks, and the public anchor ids Anindita's schedule uses.
 */
inline constexpr const TCHAR* StationeryDoorId = TEXT("door.nabapara.stationery_front");
inline constexpr const TCHAR* StationeryAnchorCounter = TEXT("anchor.stationery.counter");
inline constexpr const TCHAR* StationeryAnchorStockRoom = TEXT("anchor.stationery.stock_room");
inline constexpr const TCHAR* StationeryAnchorPackingDesk = TEXT("anchor.stationery.packing_desk");
inline constexpr const TCHAR* StationeryAnchorFrontDoor = TEXT("anchor.stationery.front_door");
inline constexpr const TCHAR* StationeryAnchorCustomer = TEXT("anchor.stationery.customer"); // where the player stands to talk across the counter
inline constexpr uint32 StationeryGeneratorSeed = 2673104671u; // FLaneBuilding::Seed of the building (LaneHash(138, 2, 11))
inline constexpr int32 StationeryEntranceU = 250;              // door centre along the frontage (X 150): clear of the staff gap at the east end

/** One of the shop's anchors in world space, under its public id (the layout id is kept for tests). */
struct FStationeryAnchor
{
    FName Id;
    FName LayoutId;
    LI::AnchorKind Kind = LI::AnchorKind::Stand;
    FVector Local = FVector::ZeroVector; // building-local U, V and Z = 0 (floor)
    FVector World = FVector::ZeroVector;
    double YawDegrees = 0.0;
};

/** True for the one building that gets the stationery shop (side +1, Index 2, X -100 to 600, the footprint above). */
bool IsStationeryBuilding(const FLaneBuilding& Building);

/** The portable layout of the shop (built once, identical for the build, the tests and the schedule planner). */
const LI::InteriorLayout& GetStationeryLayout();

/**
 * Replaces the building's solid ground-floor volume: the kit's walls with real openings (front door, two display windows,
 * the doorway to the stock room), floors, ceiling, counter with side access, shelves, stock room, packing desk, ceiling lights
 * and the front door spec (StationeryDoorId), plus the shop's sign, awning and dressing. Adds to Scene.Doors and
 * Scene.RoomLights. Call INSTEAD of BuildLaneGroundFloor and AddWallVolume.
 */
void BuildStationeryShop(FLaneScene& Scene, const FLaneBuilding& Building);

/** True once BuildStationeryShop has added the front door spec to the scene. */
bool HasStationeryShop(const FLaneScene& Scene);

/** The shop's five anchors (counter, stock_room, packing_desk, front_door, customer) transformed by the building's frame. */
void GetStationeryAnchors(const FLaneBuilding& Building, TArray<FStationeryAnchor>& OutAnchors);

/** Presenter bindings from the built layout, including Sen home off lane and both doorway exclusion zones. */
FNpBindingProvider NpStationeryBindings(const FLaneBuilding& Building);
} // namespace PWLane
