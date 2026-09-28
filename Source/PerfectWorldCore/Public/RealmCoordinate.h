#pragma once

#include <cstdint>
#include <optional>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * Positions in the Deep Realm (WORLD_SCALE.md §4).
 *
 * The realm is a stack of square floors. Floor 1 alone is at least ten times
 * Earth's surface, about 71,400 km on a side, so no single float can hold a
 * position on it to the centimetre. A realm position is therefore all
 * integers: a floor, a chunk index on each axis, and an offset in millimetres
 * inside that chunk. Every sum, difference and comparison is exact and gives
 * the same answer on every machine.
 *
 * Unreal never sees these numbers directly. It renders a local window around
 * the player, measured from the origin of the player's current sector, and
 * re-origins when the player crosses into another sector.
 */

/** A chunk is the unit of generation and saving: 32 m by 32 m of 1 m blocks. */
constexpr std::int64_t RealmChunkEdgeMillimetres = 32'000;
/** A sector is 1,024 by 1,024 chunks, 32.768 km a side: the re-origin cell. */
constexpr std::int64_t RealmChunksPerSectorEdge = 1'024;
constexpr std::int64_t RealmSectorEdgeMillimetres = RealmChunkEdgeMillimetres * RealmChunksPerSectorEdge;

/** Floors are numbered from 1 (the surface) down to RealmFloorCount. */
constexpr std::int32_t RealmFloorCount = 12;
/**
 * Floor 1 is 2,180 sectors a side: 71,434.24 km, an area of about 5.103
 * billion km², just over ten times Earth's 510.1 million km².
 */
constexpr std::int64_t RealmFloor1SectorsPerEdge = 2'180;
/** Earth's surface in km², the yardstick for the owner's "ten times Earth". */
constexpr double EarthSurfaceSquareKilometres = 510'072'000.0;

PERFECTWORLDCORE_API bool IsValidRealmFloor(std::int32_t floor);
/** Each floor down halves the edge (a quarter of the area). 0 for an invalid floor. */
PERFECTWORLDCORE_API std::int64_t RealmFloorSectorsPerEdge(std::int32_t floor);
PERFECTWORLDCORE_API std::int64_t RealmFloorChunksPerEdge(std::int32_t floor);
PERFECTWORLDCORE_API std::int64_t RealmFloorEdgeMillimetres(std::int32_t floor);
PERFECTWORLDCORE_API double RealmFloorAreaSquareKilometres(std::int32_t floor);

struct RealmChunk
{
    std::int32_t Floor = 1;
    std::int64_t X = 0;
    std::int64_t Y = 0;

    friend bool operator==(const RealmChunk&, const RealmChunk&) = default;
};

struct RealmSector
{
    std::int32_t Floor = 1;
    std::int64_t X = 0;
    std::int64_t Y = 0;

    friend bool operator==(const RealmSector&, const RealmSector&) = default;
};

/**
 * A normalised position: the chunk lies on the floor and both local offsets
 * are in [0, RealmChunkEdgeMillimetres). Elevation is height above the
 * floor's datum in millimetres (about ±2,147 km), which no floor approaches.
 */
struct RealmPosition
{
    RealmChunk Chunk;
    std::int32_t LocalXMillimetres = 0;
    std::int32_t LocalYMillimetres = 0;
    std::int32_t ElevationMillimetres = 0;

    friend bool operator==(const RealmPosition&, const RealmPosition&) = default;
};

/** Exact per-axis difference between two positions on the same floor. */
struct RealmDelta
{
    std::int64_t XMillimetres = 0;
    std::int64_t YMillimetres = 0;
    std::int64_t ZMillimetres = 0;
};

PERFECTWORLDCORE_API bool IsValidRealmChunk(const RealmChunk& chunk);
PERFECTWORLDCORE_API bool IsValidRealmPosition(const RealmPosition& position);

/**
 * Builds a position from millimetres measured from the floor's (0, 0) corner.
 * Refuses a bad floor or any point off the floor rather than clamping it,
 * because a clamped position would silently put the player somewhere else.
 */
PERFECTWORLDCORE_API std::optional<RealmPosition> MakeRealmPosition(
    std::int32_t floor, std::int64_t xMillimetres, std::int64_t yMillimetres, std::int32_t elevationMillimetres = 0);

PERFECTWORLDCORE_API std::int64_t RealmAbsoluteXMillimetres(const RealmPosition& position);
PERFECTWORLDCORE_API std::int64_t RealmAbsoluteYMillimetres(const RealmPosition& position);

/** Moves a position on its floor. Refuses a move that would leave the floor. */
PERFECTWORLDCORE_API std::optional<RealmPosition> OffsetRealmPosition(
    const RealmPosition& position, std::int64_t dxMillimetres, std::int64_t dyMillimetres, std::int64_t dzMillimetres = 0);

PERFECTWORLDCORE_API RealmSector RealmSectorOf(const RealmChunk& chunk);
PERFECTWORLDCORE_API RealmSector RealmSectorOf(const RealmPosition& position);

/** Only positions on the same floor have a delta or a distance. */
PERFECTWORLDCORE_API std::optional<RealmDelta> RealmDeltaBetween(const RealmPosition& from, const RealmPosition& to);
/**
 * Straight-line distance across the floor in metres, from exact integer
 * deltas. Symmetric by construction and correctly rounded, so identical on
 * every machine.
 */
PERFECTWORLDCORE_API std::optional<double> RealmHorizontalDistanceMetres(const RealmPosition& a, const RealmPosition& b);

/**
 * The position relative to the origin corner of a sector, in millimetres: what
 * Unreal renders once it has re-origined on that sector. Nullopt when the
 * sector is on another floor.
 */
PERFECTWORLDCORE_API std::optional<RealmDelta> RealmOffsetFromSectorOrigin(const RealmSector& sector, const RealmPosition& position);
} // namespace PerfectWorld
