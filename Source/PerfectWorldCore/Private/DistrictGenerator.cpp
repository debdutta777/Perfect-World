#include "DistrictGenerator.h"

#include <algorithm>
#include <vector>

namespace PerfectWorld
{
namespace
{
constexpr std::int32_t DistrictN = DistrictGridEdgeCells;
using DistrictCells = std::array<DistrictCell, DistrictCellCount>;

// Salts keep each purpose's hash stream independent.
constexpr std::uint64_t DistrictSaltKey = 0x44495354'4B455900ULL;
constexpr std::uint64_t DistrictSaltLane = 0x44495354'4C414E00ULL;
constexpr std::uint64_t DistrictSaltDrift = 0x44495354'44524600ULL;
constexpr std::uint64_t DistrictSaltGap = 0x44495354'47415000ULL;
constexpr std::uint64_t DistrictSaltPlaza = 0x44495354'504C5A00ULL;
constexpr std::uint64_t DistrictSaltCarve = 0x44495354'43525600ULL;
constexpr std::uint64_t DistrictSaltStorey = 0x44495354'53544F00ULL;

std::uint64_t DistrictMix(std::uint64_t z)
{
    z += 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

std::uint64_t DistrictHash(std::uint64_t key, std::uint64_t salt, std::int64_t a, std::int64_t b)
{
    std::uint64_t h = DistrictMix(key ^ salt);
    h = DistrictMix(h ^ static_cast<std::uint64_t>(a));
    return DistrictMix(h ^ static_cast<std::uint64_t>(b));
}

std::int32_t DistrictRoll(std::uint64_t key, std::uint64_t salt, std::int64_t a, std::int64_t b, std::int32_t modulus)
{
    return static_cast<std::int32_t>(DistrictHash(key, salt, a, b) % static_cast<std::uint64_t>(modulus));
}

std::size_t DistrictIndex(std::int32_t x, std::int32_t y)
{
    return static_cast<std::size_t>(y * DistrictN + x);
}

bool DistrictInside(std::int32_t x, std::int32_t y)
{
    return x >= 0 && y >= 0 && x < DistrictN && y < DistrictN;
}

bool DistrictIs(const DistrictCells& c, std::int32_t x, std::int32_t y, DistrictCellKind kind)
{
    return DistrictInside(x, y) && c[DistrictIndex(x, y)].Kind == kind;
}

void DistrictSet(DistrictCells& c, std::int32_t x, std::int32_t y, DistrictCellKind kind)
{
    c[DistrictIndex(x, y)] = DistrictCell{kind, 0};
}

bool DistrictTouchesStreet(const DistrictCells& c, std::int32_t x, std::int32_t y)
{
    return DistrictIs(c, x - 1, y, DistrictCellKind::Street) || DistrictIs(c, x + 1, y, DistrictCellKind::Street) ||
           DistrictIs(c, x, y - 1, DistrictCellKind::Street) || DistrictIs(c, x, y + 1, DistrictCellKind::Street);
}

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

std::int32_t DistrictLaneStart(std::uint64_t key, bool vertical, const DistrictLanes& lanes)
{
    return DistrictRoll(key, DistrictSaltLane, vertical ? 1 : 2, -1, lanes.MinGap);
}

void DistrictDrawLanes(DistrictCells& c, std::uint64_t key, bool vertical, const DistrictLanes& lanes)
{
    const std::int64_t axis = vertical ? 1 : 2;
    const auto put = [&](std::int32_t across, std::int32_t along)
    {
        if (vertical)
        {
            DistrictSet(c, across, along, DistrictCellKind::Street);
        }
        else
        {
            DistrictSet(c, along, across, DistrictCellKind::Street);
        }
    };

    std::int32_t base = DistrictLaneStart(key, vertical, lanes);
    for (std::int32_t lane = 0; base < DistrictN; ++lane)
    {
        std::int32_t offset = 0;
        std::int32_t previous = base;
        for (std::int32_t along = 0; along < DistrictN; ++along)
        {
            if (lanes.Drift > 0)
            {
                const std::int32_t step = DistrictRoll(key, DistrictSaltDrift, axis * 1000 + lane, along, 8);
                if (step == 0)
                {
                    offset = std::max(offset - 1, -lanes.Drift);
                }
                else if (step == 1)
                {
                    offset = std::min(offset + 1, lanes.Drift);
                }
            }
            const std::int32_t current = std::clamp(base + offset, 0, DistrictN - 1);
            put(current, along);
            if (current != previous)
            {
                put(previous, along); // the jog, so the lane stays 4-connected
            }
            previous = current;
            if (lanes.WideEvery > 0 && lane % lanes.WideEvery == 0)
            {
                put(std::min(current + 1, DistrictN - 1), along);
            }
        }
        base += lanes.MinGap + DistrictRoll(key, DistrictSaltGap, axis, lane, lanes.MaxGap - lanes.MinGap + 1);
    }
}

/** A square of open ground in the middle, ringed by a street, with the lanes cut off inside it. */
void DistrictMakePlaza(DistrictCells& c, std::uint64_t key)
{
    const std::int32_t half = 5 + DistrictRoll(key, DistrictSaltPlaza, 0, 0, 4);
    const std::int32_t cx = DistrictN / 2 + DistrictRoll(key, DistrictSaltPlaza, 1, 0, 5) - 2;
    const std::int32_t cy = DistrictN / 2 + DistrictRoll(key, DistrictSaltPlaza, 2, 0, 5) - 2;
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
}

/**
 * A lot that touches no street gets an alley straight through the block to the
 * streets at both ends, so dense blocks stay all lots and the alley is not a dead end.
 */
void DistrictCarveAlleys(DistrictCells& c, std::uint64_t key)
{
    for (std::int32_t y = 0; y < DistrictN; ++y)
    {
        for (std::int32_t x = 0; x < DistrictN; ++x)
        {
            if (!DistrictIs(c, x, y, DistrictCellKind::Building) || DistrictTouchesStreet(c, x, y))
            {
                continue;
            }
            const bool horizontal = (DistrictHash(key, DistrictSaltCarve, x, y) & 1) == 0;
            const std::int32_t dx = horizontal ? 1 : 0;
            const std::int32_t dy = horizontal ? 0 : 1;
            DistrictSet(c, x, y, DistrictCellKind::Street);
            for (const std::int32_t sign : {1, -1})
            {
                for (std::int32_t px = x + sign * dx, py = y + sign * dy; DistrictIs(c, px, py, DistrictCellKind::Building);
                     px += sign * dx, py += sign * dy)
                {
                    DistrictSet(c, px, py, DistrictCellKind::Street);
                }
            }
        }
    }
}

/** Safety net: keep only the largest street network, so no street cell is stranded. */
void DistrictKeepLargestNetwork(DistrictCells& c)
{
    std::array<std::int32_t, DistrictCellCount> label{};
    std::int32_t labels = 0;
    std::int32_t best = 0;
    std::int32_t bestSize = 0;
    std::vector<std::int32_t> stack;
    for (std::int32_t start = 0; start < DistrictCellCount; ++start)
    {
        if (c[static_cast<std::size_t>(start)].Kind != DistrictCellKind::Street || label[static_cast<std::size_t>(start)] != 0)
        {
            continue;
        }
        ++labels;
        std::int32_t size = 0;
        stack.assign(1, start);
        label[static_cast<std::size_t>(start)] = labels;
        while (!stack.empty())
        {
            const std::int32_t at = stack.back();
            stack.pop_back();
            ++size;
            const std::int32_t x = at % DistrictN;
            const std::int32_t y = at / DistrictN;
            const std::int32_t nx[4] = {x - 1, x + 1, x, x};
            const std::int32_t ny[4] = {y, y, y - 1, y + 1};
            for (std::int32_t k = 0; k < 4; ++k)
            {
                if (DistrictIs(c, nx[k], ny[k], DistrictCellKind::Street) && label[DistrictIndex(nx[k], ny[k])] == 0)
                {
                    label[DistrictIndex(nx[k], ny[k])] = labels;
                    stack.push_back(static_cast<std::int32_t>(DistrictIndex(nx[k], ny[k])));
                }
            }
        }
        if (size > bestSize)
        {
            bestSize = size;
            best = labels;
        }
    }
    for (std::size_t i = 0; i < c.size(); ++i)
    {
        if (c[i].Kind == DistrictCellKind::Street && label[i] != best)
        {
            c[i] = DistrictCell{DistrictCellKind::Open, 0};
        }
    }
}

std::uint8_t DistrictStoreys(DistrictArchetype layout, std::uint64_t key, std::int32_t x, std::int32_t y, std::int32_t startX,
                             std::int32_t startY)
{
    switch (layout)
    {
    case DistrictArchetype::ApartmentBelt:
    {
        // A tower block shares a base height, with a little variation per tower: 6..14.
        const std::int32_t blockX = (x + 6 - startX) / 6;
        const std::int32_t blockY = (y + 6 - startY) / 6;
        const std::int32_t base = 6 + DistrictRoll(key, DistrictSaltStorey, blockX, blockY + 1000, 7);
        return static_cast<std::uint8_t>(base + DistrictRoll(key, DistrictSaltStorey + 1, x, y, 3));
    }
    case DistrictArchetype::Market:
        return static_cast<std::uint8_t>(1 + DistrictRoll(key, DistrictSaltStorey, x, y, 3));
    default:
        // Old dense core: neighbouring lots share a height, 2..5.
        return static_cast<std::uint8_t>(2 + DistrictRoll(key, DistrictSaltStorey, x >> 1, y >> 1, 4));
    }
}

constexpr std::array<std::string_view, DistrictArchetypeCount> DistrictNames = {
    "old_dense_core", "riverfront", "market",   "apartment_belt",      "rail_yard", "government_quarter",
    "campus",         "port",       "informal_settlement", "industrial", "suburb",  "park"};
} // namespace

std::string_view DistrictArchetypeName(DistrictArchetype archetype)
{
    return DistrictNames[static_cast<std::size_t>(archetype)];
}

std::optional<DistrictArchetype> ParseDistrictArchetype(std::string_view name)
{
    for (std::size_t i = 0; i < DistrictNames.size(); ++i)
    {
        if (DistrictNames[i] == name)
        {
            return static_cast<DistrictArchetype>(i);
        }
    }
    return std::nullopt;
}

bool IsDistrictLayoutImplemented(DistrictArchetype archetype)
{
    return DistrictFallbackLayout(archetype) == archetype;
}

DistrictArchetype DistrictFallbackLayout(DistrictArchetype archetype)
{
    using A = DistrictArchetype;
    switch (archetype)
    {
    case A::OldDenseCore:
    case A::Riverfront:
    case A::InformalSettlement:
        return A::OldDenseCore;
    case A::ApartmentBelt:
    case A::RailYard:
    case A::GovernmentQuarter:
    case A::Industrial:
        return A::ApartmentBelt;
    default:
        return A::Market; // market, campus, port, suburb, park
    }
}

std::uint8_t DistrictFallbackStoreyCap(DistrictArchetype archetype)
{
    using A = DistrictArchetype;
    switch (archetype)
    {
    case A::RailYard:
    case A::InformalSettlement:
    case A::Suburb:
        return 2;
    case A::Industrial:
        return 3;
    case A::Campus:
        return 4;
    case A::Port:
        return 5;
    case A::GovernmentQuarter:
        return 6;
    case A::Park:
        return 1;
    default:
        return 255;
    }
}

GeneratedDistrict GenerateDistrict(DistrictArchetype archetype, std::uint64_t seed)
{
    GeneratedDistrict district;
    district.Archetype = archetype;
    district.Layout = DistrictFallbackLayout(archetype);
    district.Seed = seed;

    // The requested archetype is part of the key, so a fallback does not copy the layout it borrows.
    const std::uint64_t key = DistrictHash(seed, DistrictSaltKey, static_cast<std::int64_t>(archetype), 0);
    DistrictCells& c = district.Cells;
    for (DistrictCell& cell : c)
    {
        cell = DistrictCell{DistrictCellKind::Building, 0};
    }

    DistrictLanes lanes{6, 9, 1, 0};
    bool carve = true;
    switch (district.Layout)
    {
    case DistrictArchetype::ApartmentBelt:
        lanes = DistrictLanes{6, 6, 0, 3}; // a regular grid with an avenue every third street
        carve = false;                     // lots deep inside a block become podium gardens
        break;
    case DistrictArchetype::Market:
        lanes = DistrictLanes{4, 6, 0, 0};
        break;
    default:
        break;
    }

    DistrictDrawLanes(c, key, true, lanes);
    DistrictDrawLanes(c, key, false, lanes);
    if (district.Layout == DistrictArchetype::Market)
    {
        DistrictMakePlaza(c, key);
    }
    if (carve)
    {
        DistrictCarveAlleys(c, key);
    }
    DistrictKeepLargestNetwork(c);

    const std::int32_t startX = DistrictLaneStart(key, true, lanes);
    const std::int32_t startY = DistrictLaneStart(key, false, lanes);
    const std::uint8_t cap = DistrictFallbackStoreyCap(archetype);
    for (std::int32_t y = 0; y < DistrictN; ++y)
    {
        for (std::int32_t x = 0; x < DistrictN; ++x)
        {
            DistrictCell& cell = c[DistrictIndex(x, y)];
            if (cell.Kind != DistrictCellKind::Building)
            {
                continue;
            }
            if (!DistrictTouchesStreet(c, x, y))
            {
                cell = DistrictCell{DistrictCellKind::Open, 0};
                continue;
            }
            cell.Storeys = std::min(DistrictStoreys(district.Layout, key, x, y, startX, startY), cap);
        }
    }
    return district;
}

std::int32_t CountDistrictCells(const GeneratedDistrict& district, DistrictCellKind kind)
{
    std::int32_t count = 0;
    for (const DistrictCell& cell : district.Cells)
    {
        count += cell.Kind == kind ? 1 : 0;
    }
    return count;
}
} // namespace PerfectWorld
