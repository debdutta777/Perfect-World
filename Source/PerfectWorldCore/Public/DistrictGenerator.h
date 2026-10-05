#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * The generated-district builder (WORLD_SCALE.md §1-2, ROADMAP M1-29).
 *
 * A generated district is a pure function of (archetype, seed): a square grid
 * of 64 by 64 cells, each 8 m, so 512 m a side. The visual is one-metre blocks
 * at metric scale, built from the grid later; this is only the plan: which
 * cells are street, which are building lots (and how many storeys), which are
 * open ground, water or rail. Hashing is counter based and integer only, so
 * the same input gives the same grid on every machine and in any order.
 *
 * Guarantees for every archetype (the CTest checks them over hundreds of seeds):
 *   - All street cells form one 4-connected network: no isolated or stranded
 *     street cell.
 *   - Every building lot has a 4-neighbour street cell.
 *   - The network touches all four edges of the grid, so districts join up.
 *
 * Implemented layouts: old_dense_core, apartment_belt and market. The other
 * nine archetypes fall back to one of those (see DistrictFallbackLayout), with
 * a different hash stream and a storey cap, until their own layouts are written.
 * Water and Rail cells exist in the format but no layout places them yet.
 */
constexpr std::int32_t DistrictGridEdgeCells = 64;
constexpr std::int32_t DistrictCellEdgeMetres = 8;
constexpr std::int32_t DistrictCellCount = DistrictGridEdgeCells * DistrictGridEdgeCells;
static_assert(DistrictGridEdgeCells * DistrictCellEdgeMetres == 512, "a district is 512 m a side");

/** The archetype names are fixed by the content validator (ContentSchemas.cpp). */
enum class DistrictArchetype : std::uint8_t
{
    OldDenseCore = 0,
    Riverfront,
    Market,
    ApartmentBelt,
    RailYard,
    GovernmentQuarter,
    Campus,
    Port,
    InformalSettlement,
    Industrial,
    Suburb,
    Park
};
constexpr std::int32_t DistrictArchetypeCount = 12;

enum class DistrictCellKind : std::uint8_t
{
    Street = 0,
    Building,
    Open,
    Water,
    Rail
};

struct DistrictCell
{
    DistrictCellKind Kind = DistrictCellKind::Open;
    /** 1 or more for a Building, 0 for every other kind. */
    std::uint8_t Storeys = 0;
};

struct GeneratedDistrict
{
    /** The archetype asked for. */
    DistrictArchetype Archetype = DistrictArchetype::OldDenseCore;
    /** The layout that actually built the grid: the same as Archetype when that one is implemented. */
    DistrictArchetype Layout = DistrictArchetype::OldDenseCore;
    std::uint64_t Seed = 0;
    /** Row-major (y * 64 + x). */
    std::array<DistrictCell, DistrictCellCount> Cells{};

    const DistrictCell& At(std::int32_t x, std::int32_t y) const
    {
        return Cells[static_cast<std::size_t>(y * DistrictGridEdgeCells + x)];
    }
};

/** The stable content name, for example "old_dense_core". */
PERFECTWORLDCORE_API std::string_view DistrictArchetypeName(DistrictArchetype archetype);
/** Nullopt for a name that is not one of the twelve. */
PERFECTWORLDCORE_API std::optional<DistrictArchetype> ParseDistrictArchetype(std::string_view name);

/** True for the three archetypes with their own layout. */
PERFECTWORLDCORE_API bool IsDistrictLayoutImplemented(DistrictArchetype archetype);
/**
 * The layout that builds an archetype: itself when implemented, otherwise
 *   riverfront, informal_settlement -> old_dense_core (storey cap 2 for informal_settlement)
 *   industrial, rail_yard, government_quarter -> apartment_belt (storey caps 3, 2, 6)
 *   suburb -> market (storey cap 2); campus, port, park -> market (caps 4, 5, 1)
 * A cap only lowers storeys; it never changes the street plan.
 */
PERFECTWORLDCORE_API DistrictArchetype DistrictFallbackLayout(DistrictArchetype archetype);
/** The storey cap of the fallback, or 255 when there is none. */
PERFECTWORLDCORE_API std::uint8_t DistrictFallbackStoreyCap(DistrictArchetype archetype);

PERFECTWORLDCORE_API GeneratedDistrict GenerateDistrict(DistrictArchetype archetype, std::uint64_t seed);

PERFECTWORLDCORE_API std::int32_t CountDistrictCells(const GeneratedDistrict& district, DistrictCellKind kind);
} // namespace PerfectWorld
