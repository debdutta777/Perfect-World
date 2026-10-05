#pragma once

// Internal to the portable core: what DistrictGenerator.cpp shares with the per-archetype
// layouts (DistrictLayouts.cpp, DistrictLayoutsLand.cpp). Not part of the public API.

#include "DistrictGenerator.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace PerfectWorld::DistrictInternal
{
constexpr std::int32_t DistrictN = DistrictGridEdgeCells;
using DistrictCells = std::array<DistrictCell, DistrictCellCount>;

/** Straight-ish lanes across the whole grid; neighbouring lanes are MinGap..MaxGap cells apart. */
struct DistrictLanes
{
    std::int32_t MinGap;
    std::int32_t MaxGap;
    /** How far a lane may wander sideways from its base line, one cell per row at most. */
    std::int32_t Drift;
    /** Every n-th lane is two cells wide (an avenue); 0 for none. */
    std::int32_t WideEvery;
};

std::uint64_t DistrictHash(std::uint64_t key, std::uint64_t salt, std::int64_t a, std::int64_t b);
std::int32_t DistrictRoll(std::uint64_t key, std::uint64_t salt, std::int64_t a, std::int64_t b, std::int32_t modulus);
std::size_t DistrictIndex(std::int32_t x, std::int32_t y);
bool DistrictInside(std::int32_t x, std::int32_t y);
bool DistrictIs(const DistrictCells& c, std::int32_t x, std::int32_t y, DistrictCellKind kind);
void DistrictSet(DistrictCells& c, std::int32_t x, std::int32_t y, DistrictCellKind kind);
bool DistrictTouchesStreet(const DistrictCells& c, std::int32_t x, std::int32_t y);

std::int32_t DistrictLaneStart(std::uint64_t key, bool vertical, const DistrictLanes& lanes);
void DistrictDrawLanes(DistrictCells& c, std::uint64_t key, bool vertical, const DistrictLanes& lanes);
/** A lot that touches no street gets an alley straight through its block. */
void DistrictCarveAlleys(DistrictCells& c, std::uint64_t key);
/** Keep only the largest street network, then turn every lot that touches no street into open ground. */
void DistrictFinishPlan(DistrictCells& c);

/** Turn the whole plan a quarter turn clockwise, `turns` times (storeys travel with their cells). */
void DistrictRotate(DistrictCells& c, std::int32_t turns);

/** Give every building a storey count from storeys(x, y), which must return 1 or more. */
template <typename StoreyFn>
void DistrictAssignStoreys(DistrictCells& c, StoreyFn storeys)
{
    for (std::int32_t y = 0; y < DistrictN; ++y)
    {
        for (std::int32_t x = 0; x < DistrictN; ++x)
        {
            DistrictCell& cell = c[DistrictIndex(x, y)];
            if (cell.Kind == DistrictCellKind::Building)
            {
                cell.Storeys = static_cast<std::uint8_t>(storeys(x, y));
            }
        }
    }
}

/** The common ending of the new layouts: connect the streets, assign storeys, then orient the plan. */
template <typename StoreyFn>
void DistrictFinishLayout(DistrictCells& c, StoreyFn storeys, std::int32_t turns)
{
    DistrictFinishPlan(c);
    DistrictAssignStoreys(c, storeys);
    DistrictRotate(c, turns);
}

/**
 * Builds the layouts that have their own file: riverfront, port, rail_yard (DistrictLayouts.cpp) and campus,
 * industrial, suburb, government_quarter, informal_settlement, park (DistrictLayoutsLand.cpp). `c` arrives
 * filled with Building cells; on return it is complete, storeys included. False, with `c` untouched, for
 * old_dense_core, apartment_belt and market, which DistrictGenerator.cpp builds itself.
 */
bool DistrictBuildLayout(DistrictArchetype layout, std::uint64_t key, DistrictCells& c);
bool DistrictBuildWaterRailLayout(DistrictArchetype layout, std::uint64_t key, DistrictCells& c);
bool DistrictBuildLandLayout(DistrictArchetype layout, std::uint64_t key, DistrictCells& c);
} // namespace PerfectWorld::DistrictInternal
