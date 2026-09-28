#pragma once

#include "RealmCoordinate.h"

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
 * The Deep Realm generator (WORLD_SCALE.md §4.3, ROADMAP M1-26).
 *
 * Everything here is a pure function of the realm seed and a coordinate:
 * counter-based hashing, no sequential random stream, no state. Any chunk can
 * be generated alone, in any order, on any thread, and gives the same result
 * on every machine, because the arithmetic is integer only. It never touches
 * the life's RNG streams (SYSTEMS.md), which it has no access to.
 *
 * Terrain height is a function of the global block column, so neighbouring
 * chunks always meet without a seam. Region theme, havens and danger live on
 * coarser grids above the terrain.
 */
constexpr std::int32_t RealmBlocksPerChunkEdge = 32;
constexpr std::int32_t RealmBlockEdgeMillimetres = 1'000;
static_assert(RealmBlocksPerChunkEdge * RealmBlockEdgeMillimetres == RealmChunkEdgeMillimetres, "a chunk is 32 one-metre blocks");

/** A region is 64 by 64 sectors, about 2,097 km a side: one theme, possibly a haven. */
constexpr std::int64_t RealmRegionEdgeSectors = 64;

/** Hazard classification, the same F-to-S scale as Gates (AWAKENED_GRADES.md §5). */
enum class HazardRank : std::uint8_t
{
    F = 0,
    E,
    D,
    C,
    B,
    A,
    S
};

enum class RealmTheme : std::uint8_t
{
    VerdantWilds = 0,
    AshWastes,
    DrownedMarsh,
    CrystalBarrens,
    SkySpires,
    HollowForest,
    BoneDesert,
    StormSteppe
};
constexpr std::int32_t RealmThemeCount = 8;

enum class RealmFeatureKind : std::uint8_t
{
    Ruin = 0,
    Camp,
    Lair,
    Tower
};

struct RealmFeature
{
    RealmFeatureKind Kind = RealmFeatureKind::Ruin;
    std::int32_t LocalBlockX = 0;
    std::int32_t LocalBlockY = 0;
};

struct RealmChunkTerrain
{
    RealmChunk Chunk;
    /** Surface height above the floor datum for each block column, row-major (y * 32 + x). */
    std::array<std::int32_t, RealmBlocksPerChunkEdge * RealmBlocksPerChunkEdge> SurfaceHeightMillimetres{};
    RealmTheme Theme = RealmTheme::VerdantWilds;
    HazardRank Danger = HazardRank::F;
    std::optional<RealmFeature> Feature;
};

PERFECTWORLDCORE_API std::string_view RealmThemeName(RealmTheme theme);
PERFECTWORLDCORE_API std::string_view HazardRankName(HazardRank rank);

/** Surface height of one block column, in millimetres. Block coordinates are metres from the floor's corner. */
PERFECTWORLDCORE_API std::int32_t RealmSurfaceHeightMillimetres(std::uint64_t realmSeed, std::int32_t floor, std::int64_t blockX,
                                                                std::int64_t blockY);

PERFECTWORLDCORE_API RealmTheme RealmRegionTheme(std::uint64_t realmSeed, std::int32_t floor, std::int64_t regionX, std::int64_t regionY);
/** A haven region is safe ground where nations' Gates can anchor. Region (0, 0) of every floor is always one. */
PERFECTWORLDCORE_API bool IsRealmHavenRegion(std::uint64_t realmSeed, std::int32_t floor, std::int64_t regionX, std::int64_t regionY);
/**
 * Danger at a chunk: F within 50 km of a haven's centre, one rank more for
 * each doubling of that distance, plus up to six ranks for depth (none on
 * floors 1 and 2, enough on floor 12 that all of it is S).
 */
PERFECTWORLDCORE_API HazardRank RealmChunkDanger(std::uint64_t realmSeed, const RealmChunk& chunk);

/** The whole chunk. Nullopt for a chunk that is not on its floor. */
PERFECTWORLDCORE_API std::optional<RealmChunkTerrain> GenerateRealmChunk(std::uint64_t realmSeed, const RealmChunk& chunk);
} // namespace PerfectWorld
