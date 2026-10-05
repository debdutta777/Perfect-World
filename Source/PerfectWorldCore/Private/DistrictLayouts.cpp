#include "DistrictLayouts.h"

#include <algorithm>

// The layouts that put water or rail in a district: riverfront, port and rail_yard (WORLD_SCALE.md §1).
// Each is drawn facing one canonical way (water to the west, rails running east-west) and turned a
// random quarter-turn count at the end, so the same archetype meets the sea or the tracks on any side.
//
// Water and Rail are not street, so they never carry the street network. Where a street has to cross
// them (a bridge over the river, a mole out to the sea, a level crossing over the tracks) the crossing is
// a run of Street cells, which keeps the district's one 4-connected network and its four-edge reach.

namespace PerfectWorld::DistrictInternal
{
namespace
{
constexpr std::uint64_t DistrictLayoutSaltBank = 0x4C41594F'42414E4BULL;
constexpr std::uint64_t DistrictLayoutSaltGhat = 0x4C41594F'47484154ULL;
constexpr std::uint64_t DistrictLayoutSaltBridge = 0x4C41594F'42524447ULL;
constexpr std::uint64_t DistrictLayoutSaltPier = 0x4C41594F'50494552ULL;
constexpr std::uint64_t DistrictLayoutSaltBand = 0x4C41594F'42414E44ULL;
constexpr std::uint64_t DistrictLayoutSaltStorey = 0x4C41594F'53544F52ULL;
constexpr std::uint64_t DistrictLayoutSaltTurn = 0x4C41594F'5455524EULL;

/** Bank[y] is the first land column of row y; water fills every column left of it. */
using DistrictLayoutBank = std::array<std::int32_t, DistrictN>;

DistrictLayoutBank DistrictLayoutMakeBank(std::uint64_t key, std::int32_t base, std::int32_t wobble)
{
    DistrictLayoutBank bank{};
    std::int32_t offset = 0;
    for (std::int32_t y = 0; y < DistrictN; ++y)
    {
        if (wobble > 0)
        {
            const std::int32_t step = DistrictRoll(key, DistrictLayoutSaltBank, 1, y, 6);
            if (step == 0)
            {
                offset = std::max(offset - 1, -wobble);
            }
            else if (step == 1)
            {
                offset = std::min(offset + 1, wobble);
            }
        }
        bank[static_cast<std::size_t>(y)] = base + offset;
    }
    return bank;
}

void DistrictLayoutFillWater(DistrictCells& c, const DistrictLayoutBank& bank)
{
    for (std::int32_t y = 0; y < DistrictN; ++y)
    {
        for (std::int32_t x = 0; x < bank[static_cast<std::size_t>(y)]; ++x)
        {
            DistrictSet(c, x, y, DistrictCellKind::Water);
        }
    }
}

/** A street along the bank (the promenade or quay), widened where the bank steps so it stays 4-connected. */
void DistrictLayoutBankStreet(DistrictCells& c, const DistrictLayoutBank& bank)
{
    for (std::int32_t y = 0; y < DistrictN; ++y)
    {
        const std::int32_t here = bank[static_cast<std::size_t>(y)];
        const std::int32_t before = y > 0 ? bank[static_cast<std::size_t>(y - 1)] : here;
        for (std::int32_t x = std::min(here, before); x <= std::max(here, before); ++x)
        {
            DistrictSet(c, x, y, DistrictCellKind::Street);
        }
    }
}

/**
 * Riverfront (the Hooghly ghats): a river band 6 to 11 cells wide along one edge, a ragged bank with a
 * promenade street on it, flights of ghat steps (Open cells) going down into the water, one or two
 * bridges across to the edge, and an old city of lots 2 to 6 storeys behind.
 */
void DistrictLayoutRiverfront(DistrictCells& c, std::uint64_t key)
{
    const DistrictLanes lanes{5, 7, 0, 0};
    DistrictDrawLanes(c, key, true, lanes);
    DistrictDrawLanes(c, key, false, lanes);

    const DistrictLayoutBank bank = DistrictLayoutMakeBank(key, 7 + DistrictRoll(key, DistrictLayoutSaltBank, 0, 0, 4), 1);
    DistrictLayoutFillWater(c, bank);

    for (std::int32_t y = DistrictRoll(key, DistrictLayoutSaltGhat, 0, 0, 6); y < DistrictN;)
    {
        const std::int32_t length = 3 + DistrictRoll(key, DistrictLayoutSaltGhat, 1, y, 3);
        for (std::int32_t row = y; row < std::min(y + length, DistrictN); ++row)
        {
            for (std::int32_t depth = 1; depth <= 2; ++depth)
            {
                DistrictSet(c, bank[static_cast<std::size_t>(row)] - depth, row, DistrictCellKind::Open);
            }
        }
        y += length + 4 + DistrictRoll(key, DistrictLayoutSaltGhat, 2, y, 6);
    }
    DistrictLayoutBankStreet(c, bank);

    const std::int32_t bridges = 1 + DistrictRoll(key, DistrictLayoutSaltBridge, 0, 0, 2);
    for (std::int32_t i = 0; i < bridges; ++i)
    {
        const std::int32_t row = i == 0 ? 8 + DistrictRoll(key, DistrictLayoutSaltBridge, 1, i, 16)
                                        : 36 + DistrictRoll(key, DistrictLayoutSaltBridge, 1, i, 20);
        for (std::int32_t x = 0; x <= bank[static_cast<std::size_t>(row)]; ++x)
        {
            DistrictSet(c, x, row, DistrictCellKind::Street);
        }
    }
    DistrictCarveAlleys(c, key);

    DistrictFinishLayout(
        c, [&](std::int32_t x, std::int32_t y) { return 2 + DistrictRoll(key, DistrictLayoutSaltStorey, x >> 1, y >> 1, 5); },
        DistrictRoll(key, DistrictLayoutSaltTurn, 0, 0, 4));
}

/**
 * Port: open sea 14 to 20 cells wide along one edge, a straight quay street, 1-wide piers every 7 to 10
 * rows with dead-end tips, one long mole out to the edge, big blocks of warehouses and container yards
 * behind, 1 to 5 storeys (sheds, with an occasional harbour office or crane tower).
 */
void DistrictLayoutPort(DistrictCells& c, std::uint64_t key)
{
    DistrictDrawLanes(c, key, true, DistrictLanes{8, 11, 0, 0});
    DistrictDrawLanes(c, key, false, DistrictLanes{8, 10, 0, 0});

    const std::int32_t width = 14 + DistrictRoll(key, DistrictLayoutSaltBank, 0, 1, 7);
    const DistrictLayoutBank bank = DistrictLayoutMakeBank(key, width, 0);
    DistrictLayoutFillWater(c, bank);
    DistrictLayoutBankStreet(c, bank);

    std::int32_t y = 3 + DistrictRoll(key, DistrictLayoutSaltPier, 0, 0, 5);
    for (std::int32_t pier = 0; y < DistrictN - 1; ++pier)
    {
        const std::int32_t length = pier == 0 ? width : 4 + DistrictRoll(key, DistrictLayoutSaltPier, 1, y, 5);
        for (std::int32_t k = 0; k < length; ++k)
        {
            DistrictSet(c, width - 1 - k, y, DistrictCellKind::Street);
        }
        y += 7 + DistrictRoll(key, DistrictLayoutSaltPier, 2, y, 4);
    }

    DistrictFinishLayout(
        c,
        [&](std::int32_t x, std::int32_t y2)
        {
            const std::int32_t shed = 1 + DistrictRoll(key, DistrictLayoutSaltStorey, x >> 2, y2 >> 2, 3);
            return shed + (DistrictRoll(key, DistrictLayoutSaltStorey + 1, x >> 2, y2 >> 2, 9) == 0 ? 2 : 0);
        },
        DistrictRoll(key, DistrictLayoutSaltTurn, 0, 0, 4));
}

/**
 * Rail yard: a band of tracks 8 to 11 cells deep right across the district, every third row a platform
 * (Open), with level crossings (runs of Street cells) at least 14 cells apart, and goods sheds of 1 to 3
 * storeys on a tight street grid either side. Rail cells exist only here.
 */
void DistrictLayoutRailYard(DistrictCells& c, std::uint64_t key)
{
    const DistrictLanes lanes{5, 7, 0, 0};
    DistrictDrawLanes(c, key, true, lanes);
    DistrictDrawLanes(c, key, false, lanes);

    const std::int32_t top = 18 + DistrictRoll(key, DistrictLayoutSaltBand, 0, 0, 22);
    const std::int32_t depth = 8 + DistrictRoll(key, DistrictLayoutSaltBand, 1, 0, 4);

    // A crossing sits on a straight lane that is street on both sides of the band; the first one found is
    // always taken, so the two halves of the district are always joined.
    std::array<bool, DistrictN> crossing{};
    std::int32_t last = -100;
    for (std::int32_t x = 0; x < DistrictN; ++x)
    {
        if (DistrictIs(c, x, top - 1, DistrictCellKind::Street) && DistrictIs(c, x, top + depth, DistrictCellKind::Street) &&
            x - last >= 14)
        {
            crossing[static_cast<std::size_t>(x)] = true;
            last = x;
        }
    }
    for (std::int32_t y = top; y < top + depth; ++y)
    {
        for (std::int32_t x = 0; x < DistrictN; ++x)
        {
            DistrictSet(c, x, y,
                        crossing[static_cast<std::size_t>(x)]
                            ? DistrictCellKind::Street
                            : ((y - top) % 3 == 2 ? DistrictCellKind::Open : DistrictCellKind::Rail));
        }
    }
    DistrictCarveAlleys(c, key);

    DistrictFinishLayout(
        c, [&](std::int32_t x, std::int32_t y) { return 1 + DistrictRoll(key, DistrictLayoutSaltStorey, x >> 1, y >> 1, 3); },
        DistrictRoll(key, DistrictLayoutSaltTurn, 0, 0, 4));
}
} // namespace

void DistrictRotate(DistrictCells& c, std::int32_t turns)
{
    for (std::int32_t t = 0; t < turns; ++t)
    {
        const DistrictCells before = c;
        for (std::int32_t y = 0; y < DistrictN; ++y)
        {
            for (std::int32_t x = 0; x < DistrictN; ++x)
            {
                c[DistrictIndex(DistrictN - 1 - y, x)] = before[DistrictIndex(x, y)];
            }
        }
    }
}

bool DistrictBuildWaterRailLayout(DistrictArchetype layout, std::uint64_t key, DistrictCells& c)
{
    switch (layout)
    {
    case DistrictArchetype::Riverfront:
        DistrictLayoutRiverfront(c, key);
        return true;
    case DistrictArchetype::Port:
        DistrictLayoutPort(c, key);
        return true;
    case DistrictArchetype::RailYard:
        DistrictLayoutRailYard(c, key);
        return true;
    default:
        return false;
    }
}

bool DistrictBuildLayout(DistrictArchetype layout, std::uint64_t key, DistrictCells& c)
{
    return DistrictBuildWaterRailLayout(layout, key, c) || DistrictBuildLandLayout(layout, key, c);
}
} // namespace PerfectWorld::DistrictInternal
