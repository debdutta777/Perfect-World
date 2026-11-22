#pragma once

#include "LaneInteriorLayout.h"

#include <cstdint>
#include <span>
#include <string_view>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld::LaneInterior
{
/**
 * The catalogue of near frontages that the interior kit builds (LANE_LIFE_UPGRADE_PLAN LL-03, docs/LANE_BUILDING_INVENTORY.md).
 * Portable and data only: each row pins one generated footprint of the lane's first rows to a stable building id, a kit
 * variant, an entrance and a furniture set, so Rung 1 can prove every row generates a valid layout without Unreal.
 *
 * Identity. A building is its BuildingId ("building.nabapara.<name>"), never its row index. RowIndex and the footprint
 * numbers are only the PIN: the lane generator (BuildFirstRows) must still produce exactly this footprint, hash seed
 * included, or the row is not recognised and the frontage keeps its generic ground floor.
 *
 * Frame. Side is -1 or +1 (the sign of Y). The building frame has U along +X and V = Side * (+Y), origin at
 * (OriginXCm, Side * FrontAbsYCm). EntranceCentreU and MirrorU are in that frame, exactly as InteriorInput expects.
 */
struct FrontageEntry
{
    std::string_view BuildingId;
    int Side = 0;
    int RowIndex = 0;       // pin only: BuildFirstRows position in its row
    int OriginXCm = 0;      // frame origin X (the frontage's low-X corner)
    int FrontAbsYCm = 0;    // |Y| of the frontage line: 400 + setback
    int WidthCm = 0;
    int DepthCm = 0;
    int SetbackCm = 0;
    int Storeys = 0;
    std::uint32_t Seed = 0; // FLaneBuilding::Seed, LaneHash(Side * 131 + 7, RowIndex, 11)
    Variant Kind = Variant::CompactResidence;
    FurnitureSet Furniture = FurnitureSet::None;
    int EntranceCentreU = 0;
    bool MirrorU = false;
    /** Lane door id hung on the front doorway now; empty builds an open doorway. Must be an authored door when set. */
    std::string_view EntranceDoorId;
    /** The lane door id planned for a later door migration (TODO); empty when none is planned. Never built today. */
    std::string_view PlannedDoorId;
};

/** A footprint that is already built by its own code (tea shop, stationery shop, children's home, opening home): reserved, never a kit row. */
struct BuiltFootprint
{
    std::string_view BuildingId;
    int Side = 0;
    int RowIndex = 0;
    int X0Cm = 0;
    int X1Cm = 0;
};

/** Every kit-built frontage, in a stable order. */
[[nodiscard]] PERFECTWORLDCORE_API std::span<const FrontageEntry> KitFrontageEntries() noexcept;

/** The four ground floors built by their own code; no kit row may overlap or equal one. */
[[nodiscard]] PERFECTWORLDCORE_API std::span<const BuiltFootprint> BuiltFootprints() noexcept;

/** The row for this exact generated footprint (all numbers and the seed must match), or nullptr. A perturbed footprint is never matched. */
[[nodiscard]] PERFECTWORLDCORE_API const FrontageEntry* FindFrontageByFootprint(int side, int originXCm, int frontAbsYCm, int widthCm,
                                                                                  int depthCm, int setbackCm, int storeys,
                                                                                  std::uint32_t seed) noexcept;

[[nodiscard]] PERFECTWORLDCORE_API const FrontageEntry* FindFrontageById(std::string_view buildingId) noexcept;

/** The kit input for a row, with storey height and finished floor passed by the caller (the lane's 300 and 15). */
[[nodiscard]] PERFECTWORLDCORE_API InteriorInput MakeFrontageInput(const FrontageEntry& entry, int finishedFloorCm, int storeyHeightCm);
} // namespace PerfectWorld::LaneInterior
