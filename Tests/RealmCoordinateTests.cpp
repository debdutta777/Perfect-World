#include "RealmCoordinate.h"

#include <cstdlib>
#include <iostream>
#include <random>

using namespace PerfectWorld;

int Checks = 0;
void Check(bool condition, const char* message)
{
    ++Checks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

namespace
{
RealmPosition At(std::int32_t floor, std::int64_t x, std::int64_t y, std::int32_t z = 0)
{
    const std::optional<RealmPosition> position = MakeRealmPosition(floor, x, y, z);
    Check(position.has_value(), "test position is on its floor");
    return *position;
}
} // namespace

int main()
{
    // The owner's requirement: the dungeon is at least ten times Earth's total
    // surface. Floor 1 alone must meet it.
    Check(RealmFloorAreaSquareKilometres(1) >= 10.0 * EarthSurfaceSquareKilometres,
          "floor 1 is at least ten times Earth's surface");
    Check(RealmFloorAreaSquareKilometres(1) >= 5.101e9, "floor 1 is at least 5.101 billion km^2");
    Check(RealmFloorEdgeMillimetres(1) == 71'434'240'000, "floor 1 is 71,434.24 km a side");
    Check(RealmSectorEdgeMillimetres == 32'768'000, "a sector is 32.768 km");

    // Deeper floors are smaller, every floor has at least one sector, and
    // floors outside 1..RealmFloorCount do not exist.
    for (std::int32_t floor = 2; floor <= RealmFloorCount; ++floor)
    {
        Check(RealmFloorSectorsPerEdge(floor) >= 1, "every floor has at least one sector");
        Check(RealmFloorSectorsPerEdge(floor) <= RealmFloorSectorsPerEdge(floor - 1), "no floor is larger than the one above");
    }
    Check(RealmFloorSectorsPerEdge(2) < RealmFloorSectorsPerEdge(1), "floor 2 is smaller than floor 1");
    for (const std::int32_t bad : {0, -1, RealmFloorCount + 1})
    {
        Check(!IsValidRealmFloor(bad), "floor outside the stack is invalid");
        Check(RealmFloorSectorsPerEdge(bad) == 0, "an invalid floor has no extent");
        Check(!MakeRealmPosition(bad, 0, 0).has_value(), "no position on an invalid floor");
    }

    // Every corner of every floor round trips exactly, and one millimetre past
    // any edge is refused rather than clamped.
    for (std::int32_t floor = 1; floor <= RealmFloorCount; ++floor)
    {
        const std::int64_t last = RealmFloorEdgeMillimetres(floor) - 1;
        for (const std::int64_t x : {std::int64_t{0}, last})
        {
            for (const std::int64_t y : {std::int64_t{0}, last})
            {
                const RealmPosition p = At(floor, x, y, -123);
                Check(IsValidRealmPosition(p), "corner position is valid");
                Check(RealmAbsoluteXMillimetres(p) == x && RealmAbsoluteYMillimetres(p) == y, "corner round trips");
                Check(p.ElevationMillimetres == -123, "elevation is kept");
            }
        }
        Check(!MakeRealmPosition(floor, -1, 0).has_value(), "one millimetre west of the floor is refused");
        Check(!MakeRealmPosition(floor, 0, last + 1).has_value(), "one millimetre north of the floor is refused");
        Check(!OffsetRealmPosition(At(floor, last, last), 1, 0).has_value(), "stepping off the edge is refused");
        Check(!OffsetRealmPosition(At(floor, 0, 0), 0, -1).has_value(), "stepping off the origin is refused");
    }

    // Chunk and sector arithmetic at the boundaries.
    {
        const RealmPosition p = At(1, RealmChunkEdgeMillimetres - 1, RealmChunkEdgeMillimetres);
        Check(p.Chunk.X == 0 && p.LocalXMillimetres == RealmChunkEdgeMillimetres - 1, "last millimetre of chunk 0");
        Check(p.Chunk.Y == 1 && p.LocalYMillimetres == 0, "first millimetre of chunk 1");
        Check(RealmSectorOf(RealmChunk{1, 1023, 1024}) == RealmSector{1, 0, 1}, "chunk 1023 is sector 0, chunk 1024 sector 1");
        const RealmPosition carried = *OffsetRealmPosition(p, 1, -1);
        Check(carried.Chunk.X == 1 && carried.LocalXMillimetres == 0, "a positive step carries into the next chunk");
        Check(carried.Chunk.Y == 0 && carried.LocalYMillimetres == RealmChunkEdgeMillimetres - 1,
              "a negative step borrows from the previous chunk");
    }

    // Distances are exact and symmetric, including across sector boundaries.
    {
        const std::int64_t boundary = 7 * RealmSectorEdgeMillimetres;
        const RealmPosition west = At(1, boundary - 1, 5'000);
        const RealmPosition east = At(1, boundary + 1, 5'000);
        Check(RealmSectorOf(west).X == 6 && RealmSectorOf(east).X == 7, "the two points straddle a sector boundary");
        Check(*RealmHorizontalDistanceMetres(west, east) == 2.0 / 1000.0, "two millimetres across the boundary");
        Check(*RealmHorizontalDistanceMetres(east, west) == *RealmHorizontalDistanceMetres(west, east), "distance is symmetric");

        const RealmPosition origin = At(1, 1'000'000'000, 2'000'000'000);
        const RealmPosition triangle = *OffsetRealmPosition(origin, 3'000'000, -4'000'000);
        Check(*RealmHorizontalDistanceMetres(origin, triangle) == 5'000.0, "a 3-4-5 km triangle is exactly 5 km");

        const std::int64_t last = RealmFloorEdgeMillimetres(1) - 1;
        const double diagonal = *RealmHorizontalDistanceMetres(At(1, 0, 0), At(1, last, last));
        Check(diagonal > 1.0e8 && diagonal < 1.02e8, "floor 1's diagonal is about 101,000 km");

        Check(!RealmHorizontalDistanceMetres(At(1, 0, 0), At(2, 0, 0)).has_value(), "no distance between floors");
        Check(!RealmDeltaBetween(At(1, 0, 0), At(2, 0, 0)).has_value(), "no delta between floors");
    }

    // Sector-relative offsets are what Unreal renders after re-origin: always
    // small, and exact.
    {
        const RealmPosition far = At(1, RealmFloorEdgeMillimetres(1) - 7, RealmFloorEdgeMillimetres(1) - 9, 1'500);
        const RealmSector sector = RealmSectorOf(far);
        const RealmDelta local = *RealmOffsetFromSectorOrigin(sector, far);
        Check(local.XMillimetres == RealmSectorEdgeMillimetres - 7 && local.YMillimetres == RealmSectorEdgeMillimetres - 9,
              "the far corner is 7 and 9 mm inside its sector's far edge");
        Check(local.ZMillimetres == 1'500, "elevation passes through");
        Check(!RealmOffsetFromSectorOrigin(RealmSector{2, 0, 0}, far).has_value(), "a sector on another floor is refused");
    }

    // Random walks: every accepted step lands exactly where integer arithmetic
    // says it should, and every refused step would have left the floor.
    {
        std::mt19937_64 random(0x5EA1'2026'0928ULL);
        for (std::int32_t floor = 1; floor <= RealmFloorCount; ++floor)
        {
            const std::int64_t edge = RealmFloorEdgeMillimetres(floor);
            std::uniform_int_distribution<std::int64_t> anywhere(0, edge - 1);
            std::uniform_int_distribution<std::int64_t> step(-3 * RealmSectorEdgeMillimetres, 3 * RealmSectorEdgeMillimetres);
            RealmPosition p = At(floor, anywhere(random), anywhere(random));
            for (int i = 0; i < 20'000; ++i)
            {
                const std::int64_t dx = step(random);
                const std::int64_t dy = step(random);
                const std::int64_t x = RealmAbsoluteXMillimetres(p) + dx;
                const std::int64_t y = RealmAbsoluteYMillimetres(p) + dy;
                const std::optional<RealmPosition> next = OffsetRealmPosition(p, dx, dy);
                const bool inside = x >= 0 && y >= 0 && x < edge && y < edge;
                Check(next.has_value() == inside, "a step is accepted exactly when it stays on the floor");
                if (next)
                {
                    Check(IsValidRealmPosition(*next), "an accepted step is normalised");
                    Check(RealmAbsoluteXMillimetres(*next) == x && RealmAbsoluteYMillimetres(*next) == y,
                          "an accepted step lands exactly");
                    const RealmDelta delta = *RealmDeltaBetween(p, *next);
                    Check(delta.XMillimetres == dx && delta.YMillimetres == dy, "the delta is the step");
                    p = *next;
                }
            }
        }
    }

    // Offsets that could overflow are refused, not wrapped.
    Check(!OffsetRealmPosition(At(1, 0, 0), INT64_MAX, 0).has_value(), "a huge step is refused");
    Check(!OffsetRealmPosition(At(1, 0, 0, 2'000'000'000), 0, 0, 500'000'000).has_value(), "elevation overflow is refused");

    std::cout << "PASS: " << Checks << " realm coordinate checks\n";
    return 0;
}
