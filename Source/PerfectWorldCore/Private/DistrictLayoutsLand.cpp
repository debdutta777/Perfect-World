#include "DistrictLayouts.h"

#include <algorithm>

// The dry-land layouts: campus, industrial, suburb, government_quarter, informal_settlement and park
// (WORLD_SCALE.md §1). None places Water or Rail. Industrial and government_quarter are drawn one way
// and turned a random quarter-turn count at the end; the rest are drawn as they stand.

namespace PerfectWorld::DistrictInternal
{
namespace
{
constexpr std::uint64_t DistrictLayoutSaltQuad = 0x4C414E44'51554144ULL;
constexpr std::uint64_t DistrictLayoutSaltHall = 0x4C414E44'48414C4CULL;
constexpr std::uint64_t DistrictLayoutSaltSpur = 0x4C414E44'53505552ULL;
constexpr std::uint64_t DistrictLayoutSaltHouse = 0x4C414E44'484F5553ULL;
constexpr std::uint64_t DistrictLayoutSaltYard = 0x4C414E44'59415244ULL;
constexpr std::uint64_t DistrictLayoutSaltGovernment = 0x4C414E44'474F5645ULL;
constexpr std::uint64_t DistrictLayoutSaltPavilion = 0x4C414E44'50415649ULL;
constexpr std::uint64_t DistrictLayoutSaltStorey = 0x4C414E44'53544F52ULL;
constexpr std::uint64_t DistrictLayoutSaltTurn = 0x4C414E44'5455524EULL;

/**
 * Dead-end spurs: from a straight stretch of street with lots on both sides, a 1-wide street runs into
 * the block for minLength..maxLength cells and stops, leaving a dead-end tip with lots along it. A spur
 * never touches another street, so its tip is always a true dead end. About one candidate in `oneIn` is
 * taken. With bothAxes the spurs can leave vertical stretches as well (and so branch off spurs).
 */
void DistrictLayoutSpurs(DistrictCells& c, std::uint64_t key, std::int32_t oneIn, std::int32_t minLength, std::int32_t maxLength,
                         bool bothAxes)
{
    for (std::int32_t y = 1; y < DistrictN - 1; ++y)
    {
        for (std::int32_t x = 1; x < DistrictN - 1; ++x)
        {
            if (!DistrictIs(c, x, y, DistrictCellKind::Street))
            {
                continue;
            }
            for (std::int32_t axis = 0; axis < (bothAxes ? 2 : 1); ++axis)
            {
                const std::int32_t ax = axis == 0 ? 1 : 0; // along the street
                const std::int32_t ay = 1 - ax;
                const std::int32_t dir = DistrictRoll(key, DistrictLayoutSaltSpur + 1, x, y, 2) == 0 ? 1 : -1;
                const std::int32_t px = ay * dir; // across it, into the block
                const std::int32_t py = ax * dir;
                const bool straight = DistrictIs(c, x - ax, y - ay, DistrictCellKind::Street) &&
                                      DistrictIs(c, x + ax, y + ay, DistrictCellKind::Street) &&
                                      DistrictIs(c, x + px, y + py, DistrictCellKind::Building) &&
                                      DistrictIs(c, x - px, y - py, DistrictCellKind::Building);
                if (!straight || DistrictRoll(key, DistrictLayoutSaltSpur, x, y * 2 + axis, oneIn) != 0)
                {
                    continue;
                }
                const std::int32_t length = minLength + DistrictRoll(key, DistrictLayoutSaltSpur + 2, x, y, maxLength - minLength + 1);
                for (std::int32_t k = 1; k <= length; ++k)
                {
                    const std::int32_t qx = x + px * k;
                    const std::int32_t qy = y + py * k;
                    if (!DistrictIs(c, qx, qy, DistrictCellKind::Building) || DistrictIs(c, qx + px, qy + py, DistrictCellKind::Street) ||
                        DistrictIs(c, qx - ax, qy - ay, DistrictCellKind::Street) ||
                        DistrictIs(c, qx + ax, qy + ay, DistrictCellKind::Street))
                    {
                        break;
                    }
                    DistrictSet(c, qx, qy, DistrictCellKind::Street);
                }
            }
        }
    }
}

/** Replace each Building cell by open ground unless keep(x, y) says it stays. */
template <typename KeepFn>
void DistrictLayoutThin(DistrictCells& c, KeepFn keep)
{
    for (std::int32_t y = 0; y < DistrictN; ++y)
    {
        for (std::int32_t x = 0; x < DistrictN; ++x)
        {
            if (DistrictIs(c, x, y, DistrictCellKind::Building) && !keep(x, y))
            {
                DistrictSet(c, x, y, DistrictCellKind::Open);
            }
        }
    }
}

/**
 * Campus: winding paths 8 to 11 cells apart, one in four 4-by-4 chunks of lot turned to lawn, and a great
 * quad of 16 to 20 cells a side (256 to 400 cells of open ground) ringed by a street at the middle. Halls
 * are 2 to 4 storeys with an occasional library or laboratory tower, up to 6.
 */
void DistrictLayoutCampus(DistrictCells& c, std::uint64_t key)
{
    const DistrictLanes lanes{8, 11, 1, 0};
    DistrictDrawLanes(c, key, true, lanes);
    DistrictDrawLanes(c, key, false, lanes);

    const std::int32_t half = 8 + DistrictRoll(key, DistrictLayoutSaltQuad, 0, 0, 3);
    const std::int32_t cx = DistrictN / 2 + DistrictRoll(key, DistrictLayoutSaltQuad, 1, 0, 5) - 2;
    const std::int32_t cy = DistrictN / 2 + DistrictRoll(key, DistrictLayoutSaltQuad, 2, 0, 5) - 2;
    for (std::int32_t y = cy - half; y < cy + half; ++y)
    {
        for (std::int32_t x = cx - half; x < cx + half; ++x)
        {
            DistrictSet(c, x, y, DistrictCellKind::Open);
        }
    }
    for (std::int32_t i = -1; i <= 2 * half; ++i)
    {
        DistrictSet(c, cx - half + i, cy - half - 1, DistrictCellKind::Street);
        DistrictSet(c, cx - half + i, cy + half, DistrictCellKind::Street);
        DistrictSet(c, cx - half - 1, cy - half + i, DistrictCellKind::Street);
        DistrictSet(c, cx + half, cy - half + i, DistrictCellKind::Street);
    }
    DistrictLayoutThin(c, [&](std::int32_t x, std::int32_t y) { return DistrictRoll(key, DistrictLayoutSaltHall, x >> 2, y >> 2, 4) != 0; });

    DistrictFinishLayout(
        c,
        [&](std::int32_t x, std::int32_t y)
        {
            const std::int32_t hall = 2 + DistrictRoll(key, DistrictLayoutSaltStorey, x >> 2, y >> 2, 3);
            return hall + (DistrictRoll(key, DistrictLayoutSaltStorey + 1, x >> 2, y >> 2, 6) == 0 ? 2 : 0);
        },
        0);
}

/**
 * Industrial: long sheds. Streets run 6 to 8 rows apart one way and 14 to 20 columns apart the other (every
 * third of those a 2-wide avenue), so each block is a ring of sheds 13 to 19 cells long round an open yard.
 * Sheds are 1 to 2 storeys, with an occasional boiler house or stack of 4.
 */
void DistrictLayoutIndustrial(DistrictCells& c, std::uint64_t key)
{
    DistrictDrawLanes(c, key, true, DistrictLanes{14, 20, 0, 3});
    DistrictDrawLanes(c, key, false, DistrictLanes{6, 8, 0, 0});

    DistrictFinishLayout(
        c,
        [&](std::int32_t x, std::int32_t y)
        {
            const std::int32_t shed = 1 + DistrictRoll(key, DistrictLayoutSaltStorey, x >> 3, y >> 3, 2);
            return DistrictRoll(key, DistrictLayoutSaltStorey + 1, x >> 3, y >> 3, 10) == 0 ? 4 : shed;
        },
        DistrictRoll(key, DistrictLayoutSaltTurn, 0, 0, 4));
}

/**
 * Suburb: roads 12 to 16 cells apart with gentle bends, cul-de-sac spurs of 3 to 6 cells off the
 * east-west roads, and detached houses (about 55 percent of the lots along a street) in gardens, so most
 * of the district is open ground. Houses are 1 storey, a few 2.
 */
void DistrictLayoutSuburb(DistrictCells& c, std::uint64_t key)
{
    const DistrictLanes lanes{12, 16, 1, 0};
    DistrictDrawLanes(c, key, true, lanes);
    DistrictDrawLanes(c, key, false, lanes);
    DistrictLayoutSpurs(c, key, 12, 3, 6, false);
    DistrictLayoutThin(c, [&](std::int32_t x, std::int32_t y) { return DistrictRoll(key, DistrictLayoutSaltHouse, x, y, 100) < 55; });

    DistrictFinishLayout(
        c, [&](std::int32_t x, std::int32_t y) { return 1 + (DistrictRoll(key, DistrictLayoutSaltStorey, x, y, 3) == 0 ? 1 : 0); }, 0);
}

/**
 * Government quarter: a formal grid of streets 7 apart, a 3-wide boulevard straight down the middle
 * with a long lawn (9 cells wide, 27 to 34 long) on each side of it, and ministries of 3 to 5 storeys
 * round courtyards, rising to 5 to 7 where they frame the two ends of the lawns.
 */
void DistrictLayoutGovernment(DistrictCells& c, std::uint64_t key)
{
    const DistrictLanes lanes{7, 7, 0, 0};
    DistrictDrawLanes(c, key, true, lanes);
    DistrictDrawLanes(c, key, false, lanes);

    const std::int32_t boulevard = 30 + DistrictRoll(key, DistrictLayoutSaltGovernment, 0, 0, 3);
    const std::int32_t lawnStart = 14 + DistrictRoll(key, DistrictLayoutSaltGovernment, 1, 0, 4);
    const std::int32_t lawnEnd = 48 - DistrictRoll(key, DistrictLayoutSaltGovernment, 2, 0, 4);
    for (std::int32_t y = lawnStart; y < lawnEnd; ++y)
    {
        for (std::int32_t x = boulevard - 9; x < boulevard; ++x)
        {
            DistrictSet(c, x, y, DistrictCellKind::Open);
        }
        for (std::int32_t x = boulevard + 3; x < boulevard + 12; ++x)
        {
            DistrictSet(c, x, y, DistrictCellKind::Open);
        }
    }
    for (std::int32_t y = 0; y < DistrictN; ++y)
    {
        for (std::int32_t x = boulevard; x < boulevard + 3; ++x)
        {
            DistrictSet(c, x, y, DistrictCellKind::Street);
        }
    }

    DistrictFinishLayout(
        c,
        [&](std::int32_t x, std::int32_t y)
        {
            const std::int32_t ministry = 3 + DistrictRoll(key, DistrictLayoutSaltStorey, x >> 2, y >> 2, 3);
            const bool framing = x >= boulevard - 12 && x <= boulevard + 14 && ((y >= lawnStart - 4 && y < lawnStart) || (y >= lawnEnd && y < lawnEnd + 4));
            return ministry + (framing ? 2 : 0);
        },
        DistrictRoll(key, DistrictLayoutSaltTurn, 0, 0, 4));
}

/**
 * Informal settlement: lanes only 5 to 7 apart that wander two cells either way, every lot reached by an
 * alley, plenty of 1 to 2 cell dead-end nubs and small open yards (about one lot in thirty), and nothing
 * taller than 3 storeys. No avenues.
 */
void DistrictLayoutInformal(DistrictCells& c, std::uint64_t key)
{
    const DistrictLanes lanes{5, 7, 2, 0};
    DistrictDrawLanes(c, key, true, lanes);
    DistrictDrawLanes(c, key, false, lanes);
    DistrictCarveAlleys(c, key);
    DistrictLayoutSpurs(c, key, 6, 1, 2, true);
    DistrictLayoutThin(c, [&](std::int32_t x, std::int32_t y) { return DistrictRoll(key, DistrictLayoutSaltYard, x, y, 30) != 0; });

    DistrictFinishLayout(
        c, [&](std::int32_t x, std::int32_t y) { return 1 + DistrictRoll(key, DistrictLayoutSaltStorey, x >> 1, y >> 1, 3); }, 0);
}

/**
 * Park: winding paths 10 to 14 cells apart, nearly all of it open ground, with a pavilion or kiosk on
 * about one lot in sixteen along the paths. Buildings are 1 storey, a few 2.
 */
void DistrictLayoutPark(DistrictCells& c, std::uint64_t key)
{
    const DistrictLanes lanes{10, 14, 1, 0};
    DistrictDrawLanes(c, key, true, lanes);
    DistrictDrawLanes(c, key, false, lanes);
    DistrictLayoutThin(c, [&](std::int32_t x, std::int32_t y) { return DistrictRoll(key, DistrictLayoutSaltPavilion, x, y, 16) == 0; });

    DistrictFinishLayout(
        c, [&](std::int32_t x, std::int32_t y) { return 1 + (DistrictRoll(key, DistrictLayoutSaltStorey, x, y, 4) == 0 ? 1 : 0); }, 0);
}
} // namespace

bool DistrictBuildLandLayout(DistrictArchetype layout, std::uint64_t key, DistrictCells& c)
{
    switch (layout)
    {
    case DistrictArchetype::Campus:
        DistrictLayoutCampus(c, key);
        return true;
    case DistrictArchetype::Industrial:
        DistrictLayoutIndustrial(c, key);
        return true;
    case DistrictArchetype::Suburb:
        DistrictLayoutSuburb(c, key);
        return true;
    case DistrictArchetype::GovernmentQuarter:
        DistrictLayoutGovernment(c, key);
        return true;
    case DistrictArchetype::InformalSettlement:
        DistrictLayoutInformal(c, key);
        return true;
    case DistrictArchetype::Park:
        DistrictLayoutPark(c, key);
        return true;
    default:
        return false;
    }
}
} // namespace PerfectWorld::DistrictInternal
