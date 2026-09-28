#include "RealmCoordinate.h"

#include <cmath>
#include <limits>

namespace PerfectWorld
{
bool IsValidRealmFloor(std::int32_t floor)
{
    return floor >= 1 && floor <= RealmFloorCount;
}

std::int64_t RealmFloorSectorsPerEdge(std::int32_t floor)
{
    if (!IsValidRealmFloor(floor))
    {
        return 0;
    }
    const std::int64_t sectors = RealmFloor1SectorsPerEdge >> (floor - 1);
    return sectors < 1 ? 1 : sectors;
}

std::int64_t RealmFloorChunksPerEdge(std::int32_t floor)
{
    return RealmFloorSectorsPerEdge(floor) * RealmChunksPerSectorEdge;
}

std::int64_t RealmFloorEdgeMillimetres(std::int32_t floor)
{
    return RealmFloorChunksPerEdge(floor) * RealmChunkEdgeMillimetres;
}

double RealmFloorAreaSquareKilometres(std::int32_t floor)
{
    // Edge in km is exact for every floor (a whole number of 32.768 km sectors).
    const double edgeKilometres = static_cast<double>(RealmFloorEdgeMillimetres(floor)) / 1'000'000.0;
    return edgeKilometres * edgeKilometres;
}

bool IsValidRealmChunk(const RealmChunk& chunk)
{
    const std::int64_t edge = RealmFloorChunksPerEdge(chunk.Floor);
    return edge > 0 && chunk.X >= 0 && chunk.X < edge && chunk.Y >= 0 && chunk.Y < edge;
}

bool IsValidRealmPosition(const RealmPosition& position)
{
    return IsValidRealmChunk(position.Chunk) && position.LocalXMillimetres >= 0 &&
           position.LocalXMillimetres < RealmChunkEdgeMillimetres && position.LocalYMillimetres >= 0 &&
           position.LocalYMillimetres < RealmChunkEdgeMillimetres;
}

std::optional<RealmPosition> MakeRealmPosition(
    std::int32_t floor, std::int64_t xMillimetres, std::int64_t yMillimetres, std::int32_t elevationMillimetres)
{
    const std::int64_t edge = RealmFloorEdgeMillimetres(floor);
    if (edge == 0 || xMillimetres < 0 || yMillimetres < 0 || xMillimetres >= edge || yMillimetres >= edge)
    {
        return std::nullopt;
    }
    RealmPosition position;
    position.Chunk.Floor = floor;
    position.Chunk.X = xMillimetres / RealmChunkEdgeMillimetres;
    position.Chunk.Y = yMillimetres / RealmChunkEdgeMillimetres;
    position.LocalXMillimetres = static_cast<std::int32_t>(xMillimetres % RealmChunkEdgeMillimetres);
    position.LocalYMillimetres = static_cast<std::int32_t>(yMillimetres % RealmChunkEdgeMillimetres);
    position.ElevationMillimetres = elevationMillimetres;
    return position;
}

std::int64_t RealmAbsoluteXMillimetres(const RealmPosition& position)
{
    return position.Chunk.X * RealmChunkEdgeMillimetres + position.LocalXMillimetres;
}

std::int64_t RealmAbsoluteYMillimetres(const RealmPosition& position)
{
    return position.Chunk.Y * RealmChunkEdgeMillimetres + position.LocalYMillimetres;
}

std::optional<RealmPosition> OffsetRealmPosition(
    const RealmPosition& position, std::int64_t dxMillimetres, std::int64_t dyMillimetres, std::int64_t dzMillimetres)
{
    if (!IsValidRealmPosition(position))
    {
        return std::nullopt;
    }
    // Any step longer than the floor's edge leaves the floor, and refusing it
    // here also keeps the additions below far from int64 overflow.
    const std::int64_t edge = RealmFloorEdgeMillimetres(position.Chunk.Floor);
    if (dxMillimetres <= -edge || dxMillimetres >= edge || dyMillimetres <= -edge || dyMillimetres >= edge)
    {
        return std::nullopt;
    }
    const std::int64_t elevation = static_cast<std::int64_t>(position.ElevationMillimetres) + dzMillimetres;
    if (dzMillimetres < -(std::int64_t{1} << 40) || dzMillimetres > (std::int64_t{1} << 40) ||
        elevation < std::numeric_limits<std::int32_t>::min() || elevation > std::numeric_limits<std::int32_t>::max())
    {
        return std::nullopt;
    }
    return MakeRealmPosition(position.Chunk.Floor, RealmAbsoluteXMillimetres(position) + dxMillimetres,
                             RealmAbsoluteYMillimetres(position) + dyMillimetres,
                             static_cast<std::int32_t>(elevation));
}

RealmSector RealmSectorOf(const RealmChunk& chunk)
{
    return RealmSector{chunk.Floor, chunk.X / RealmChunksPerSectorEdge, chunk.Y / RealmChunksPerSectorEdge};
}

RealmSector RealmSectorOf(const RealmPosition& position)
{
    return RealmSectorOf(position.Chunk);
}

std::optional<RealmDelta> RealmDeltaBetween(const RealmPosition& from, const RealmPosition& to)
{
    if (from.Chunk.Floor != to.Chunk.Floor || !IsValidRealmPosition(from) || !IsValidRealmPosition(to))
    {
        return std::nullopt;
    }
    return RealmDelta{RealmAbsoluteXMillimetres(to) - RealmAbsoluteXMillimetres(from),
                      RealmAbsoluteYMillimetres(to) - RealmAbsoluteYMillimetres(from),
                      static_cast<std::int64_t>(to.ElevationMillimetres) - from.ElevationMillimetres};
}

std::optional<double> RealmHorizontalDistanceMetres(const RealmPosition& a, const RealmPosition& b)
{
    const std::optional<RealmDelta> delta = RealmDeltaBetween(a, b);
    if (!delta)
    {
        return std::nullopt;
    }
    // Each axis difference is an exact integer below 2^53, so it converts to
    // double exactly; squaring the magnitudes makes the result independent of
    // direction, and IEEE sqrt is correctly rounded.
    const double dx = static_cast<double>(delta->XMillimetres);
    const double dy = static_cast<double>(delta->YMillimetres);
    return std::sqrt(dx * dx + dy * dy) / 1000.0;
}

std::optional<RealmDelta> RealmOffsetFromSectorOrigin(const RealmSector& sector, const RealmPosition& position)
{
    if (sector.Floor != position.Chunk.Floor || !IsValidRealmPosition(position))
    {
        return std::nullopt;
    }
    return RealmDelta{RealmAbsoluteXMillimetres(position) - sector.X * RealmSectorEdgeMillimetres,
                      RealmAbsoluteYMillimetres(position) - sector.Y * RealmSectorEdgeMillimetres,
                      position.ElevationMillimetres};
}
} // namespace PerfectWorld
