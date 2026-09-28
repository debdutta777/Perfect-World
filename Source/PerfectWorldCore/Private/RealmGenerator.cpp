#include "RealmGenerator.h"

#include <algorithm>

namespace PerfectWorld
{
namespace
{
// Salts keep each purpose's hash stream independent.
constexpr std::uint64_t SaltHeight = 0x48454947'48540000ULL;
constexpr std::uint64_t SaltTheme = 0x5448454D'45000000ULL;
constexpr std::uint64_t SaltHaven = 0x48415645'4E000000ULL;
constexpr std::uint64_t SaltFeature = 0x46454154'55524500ULL;

std::uint64_t Mix(std::uint64_t z)
{
    z += 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

std::uint64_t Hash(std::uint64_t seed, std::uint64_t salt, std::int64_t a, std::int64_t b, std::int64_t c)
{
    std::uint64_t h = Mix(seed ^ salt);
    h = Mix(h ^ static_cast<std::uint64_t>(a));
    h = Mix(h ^ static_cast<std::uint64_t>(b));
    return Mix(h ^ static_cast<std::uint64_t>(c));
}

struct Octave
{
    std::int64_t SpacingMetres;
    std::int64_t AmplitudeMillimetres;
};
// Rolling hills of 4 km down to 16 m bumps; at most about ±560 m in total.
constexpr std::array<Octave, 5> Octaves = {{{4096, 400'000}, {1024, 120'000}, {256, 30'000}, {64, 8'000}, {16, 2'000}}};

constexpr std::int64_t FixedOne = 1 << 16;

std::int64_t Fade(std::int64_t t)
{
    // 3t^2 - 2t^3 on [0, 1] in 16.16 fixed point.
    const std::int64_t t2 = t * t / FixedOne;
    const std::int64_t t3 = t2 * t / FixedOne;
    return 3 * t2 - 2 * t3;
}

std::int64_t Lerp(std::int64_t a, std::int64_t b, std::int64_t t)
{
    return a + (b - a) * t / FixedOne;
}

std::int64_t LatticeValue(std::uint64_t seed, std::int32_t floor, std::size_t octave, std::int64_t ix, std::int64_t iy)
{
    const std::int64_t amplitude = Octaves[octave].AmplitudeMillimetres;
    const std::uint64_t h = Hash(seed, SaltHeight + octave, floor, ix, iy);
    return static_cast<std::int64_t>(h % static_cast<std::uint64_t>(2 * amplitude + 1)) - amplitude;
}

std::int64_t RegionEdgeMetres()
{
    return RealmRegionEdgeSectors * RealmSectorEdgeMillimetres / 1'000;
}

std::int64_t RegionsPerFloorEdge(std::int32_t floor)
{
    return (RealmFloorSectorsPerEdge(floor) + RealmRegionEdgeSectors - 1) / RealmRegionEdgeSectors;
}

/** Centre of a region in metres, kept on the floor when the region overhangs its edge. */
std::int64_t RegionCentre(std::int32_t floor, std::int64_t region)
{
    const std::int64_t start = region * RegionEdgeMetres();
    const std::int64_t end = std::min(start + RegionEdgeMetres(), RealmFloorEdgeMillimetres(floor) / 1'000);
    return (start + end) / 2;
}
} // namespace

std::string_view RealmThemeName(RealmTheme theme)
{
    constexpr std::array<std::string_view, RealmThemeCount> Names = {
        "verdant_wilds", "ash_wastes", "drowned_marsh", "crystal_barrens", "sky_spires", "hollow_forest", "bone_desert", "storm_steppe"};
    return Names[static_cast<std::size_t>(theme)];
}

std::string_view HazardRankName(HazardRank rank)
{
    constexpr std::array<std::string_view, 7> Names = {"F", "E", "D", "C", "B", "A", "S"};
    return Names[static_cast<std::size_t>(rank)];
}

std::int32_t RealmSurfaceHeightMillimetres(std::uint64_t realmSeed, std::int32_t floor, std::int64_t blockX, std::int64_t blockY)
{
    std::int64_t total = 0;
    for (std::size_t o = 0; o < Octaves.size(); ++o)
    {
        const std::int64_t spacing = Octaves[o].SpacingMetres;
        const std::int64_t ix = blockX / spacing;
        const std::int64_t iy = blockY / spacing;
        const std::int64_t tx = Fade((blockX % spacing) * FixedOne / spacing);
        const std::int64_t ty = Fade((blockY % spacing) * FixedOne / spacing);
        const std::int64_t top = Lerp(LatticeValue(realmSeed, floor, o, ix, iy), LatticeValue(realmSeed, floor, o, ix + 1, iy), tx);
        const std::int64_t bottom =
            Lerp(LatticeValue(realmSeed, floor, o, ix, iy + 1), LatticeValue(realmSeed, floor, o, ix + 1, iy + 1), tx);
        total += Lerp(top, bottom, ty);
    }
    return static_cast<std::int32_t>(total);
}

RealmTheme RealmRegionTheme(std::uint64_t realmSeed, std::int32_t floor, std::int64_t regionX, std::int64_t regionY)
{
    return static_cast<RealmTheme>(Hash(realmSeed, SaltTheme, floor, regionX, regionY) % RealmThemeCount);
}

bool IsRealmHavenRegion(std::uint64_t realmSeed, std::int32_t floor, std::int64_t regionX, std::int64_t regionY)
{
    return (regionX == 0 && regionY == 0) || Hash(realmSeed, SaltHaven, floor, regionX, regionY) % 8 == 0;
}

HazardRank RealmChunkDanger(std::uint64_t realmSeed, const RealmChunk& chunk)
{
    const std::int64_t chunkMetres = RealmChunkEdgeMillimetres / 1'000;
    const std::int64_t cx = chunk.X * chunkMetres + chunkMetres / 2;
    const std::int64_t cy = chunk.Y * chunkMetres + chunkMetres / 2;
    const std::int64_t rx = cx / RegionEdgeMetres();
    const std::int64_t ry = cy / RegionEdgeMetres();
    const std::int64_t regions = RegionsPerFloorEdge(chunk.Floor);

    // Nearest haven centre among the regions within three of this one;
    // anything farther is S territory anyway.
    std::int64_t best = INT64_MAX;
    for (std::int64_t y = std::max<std::int64_t>(0, ry - 3); y <= std::min(regions - 1, ry + 3); ++y)
    {
        for (std::int64_t x = std::max<std::int64_t>(0, rx - 3); x <= std::min(regions - 1, rx + 3); ++x)
        {
            if (IsRealmHavenRegion(realmSeed, chunk.Floor, x, y))
            {
                const std::int64_t dx = cx - RegionCentre(chunk.Floor, x);
                const std::int64_t dy = cy - RegionCentre(chunk.Floor, y);
                best = std::min(best, dx * dx + dy * dy);
            }
        }
    }
    std::int64_t rank = 0;
    for (const std::int64_t thresholdMetres : {50'000LL, 100'000LL, 200'000LL, 400'000LL, 800'000LL, 1'600'000LL})
    {
        rank += best >= thresholdMetres * thresholdMetres ? 1 : 0;
    }
    // Depth adds up to six ranks: nothing on floors 1-2, S everywhere on the deepest.
    rank += (chunk.Floor - 1) * 6 / (RealmFloorCount - 1);
    return static_cast<HazardRank>(std::min<std::int64_t>(rank, 6));
}

std::optional<RealmChunkTerrain> GenerateRealmChunk(std::uint64_t realmSeed, const RealmChunk& chunk)
{
    if (!IsValidRealmChunk(chunk))
    {
        return std::nullopt;
    }
    RealmChunkTerrain terrain;
    terrain.Chunk = chunk;
    const std::int64_t baseX = chunk.X * RealmBlocksPerChunkEdge;
    const std::int64_t baseY = chunk.Y * RealmBlocksPerChunkEdge;
    for (std::int32_t y = 0; y < RealmBlocksPerChunkEdge; ++y)
    {
        for (std::int32_t x = 0; x < RealmBlocksPerChunkEdge; ++x)
        {
            terrain.SurfaceHeightMillimetres[static_cast<std::size_t>(y * RealmBlocksPerChunkEdge + x)] =
                RealmSurfaceHeightMillimetres(realmSeed, chunk.Floor, baseX + x, baseY + y);
        }
    }
    const std::int64_t regionChunks = RealmRegionEdgeSectors * RealmChunksPerSectorEdge;
    terrain.Theme = RealmRegionTheme(realmSeed, chunk.Floor, chunk.X / regionChunks, chunk.Y / regionChunks);
    terrain.Danger = RealmChunkDanger(realmSeed, chunk);

    // At most one landmark per 8 x 8 chunk cell, placed before anyone visits.
    const std::uint64_t h = Hash(realmSeed, SaltFeature, chunk.Floor, chunk.X / 8, chunk.Y / 8);
    if (h % 16 == 0 && static_cast<std::int64_t>((h >> 8) % 8) == chunk.X % 8 && static_cast<std::int64_t>((h >> 16) % 8) == chunk.Y % 8)
    {
        RealmFeature feature;
        feature.Kind = static_cast<RealmFeatureKind>((h >> 40) % 4);
        if (feature.Kind == RealmFeatureKind::Tower && terrain.Danger < HazardRank::C)
        {
            feature.Kind = RealmFeatureKind::Ruin; // towers only stand in dangerous country
        }
        feature.LocalBlockX = static_cast<std::int32_t>((h >> 24) % RealmBlocksPerChunkEdge);
        feature.LocalBlockY = static_cast<std::int32_t>((h >> 32) % RealmBlocksPerChunkEdge);
        terrain.Feature = feature;
    }
    return terrain;
}
} // namespace PerfectWorld
