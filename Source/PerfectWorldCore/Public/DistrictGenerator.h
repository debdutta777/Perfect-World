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
 * All twelve archetypes have their own layout. Water appears only in riverfront
 * and port, Rail only in rail_yard; a bridge, mole or level crossing is a run of
 * Street cells, so the one street network still crosses them. Storeys per
 * archetype: old_dense_core 2-5, apartment_belt 6-14, market 1-3, riverfront 2-6,
 * rail_yard 1-3, port 1-5, campus 2-6, industrial 1-4, suburb 1-2,
 * government_quarter 3-7, informal_settlement 1-3, park 1-2.
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

/** True when the archetype has its own layout: all twelve do now. */
PERFECTWORLDCORE_API bool IsDistrictLayoutImplemented(DistrictArchetype archetype);
/**
 * The layout that builds an archetype: itself for all twelve. The fallback table is kept for any
 * archetype added to the enum before its layout is written: that one would borrow an implemented
 * layout, with a storey cap that only lowers storeys and never changes the street plan.
 */
PERFECTWORLDCORE_API DistrictArchetype DistrictFallbackLayout(DistrictArchetype archetype);
/** The storey cap of the fallback, or 255 when there is none. */
PERFECTWORLDCORE_API std::uint8_t DistrictFallbackStoreyCap(DistrictArchetype archetype);

PERFECTWORLDCORE_API GeneratedDistrict GenerateDistrict(DistrictArchetype archetype, std::uint64_t seed);

PERFECTWORLDCORE_API std::int32_t CountDistrictCells(const GeneratedDistrict& district, DistrictCellKind kind);
} // namespace PerfectWorld
