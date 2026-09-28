#include "RealmGenerator.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>
#include <set>
#include <vector>

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

// Pinned from the first run of realm generator version 1 (2026-09-28). Set
// PW_PRINT_GOLDEN=1 to print the current values after a deliberate change.
#define GOLDEN_HEIGHT_0_0 (-185'530)
#define GOLDEN_HEIGHT_1000_2000 86'204
#define GOLDEN_FINGERPRINT 0x1c7660d9a8f60d2fULL

namespace
{
constexpr std::uint64_t Seed = 0x0DEE'9EA1'2042'0602ULL;

/** FNV-1a over everything a chunk contains: a fingerprint for golden checks. */
std::uint64_t Fingerprint(const RealmChunkTerrain& t)
{
    std::uint64_t h = 1469598103934665603ULL;
    auto add = [&h](std::uint64_t v) {
        for (int i = 0; i < 8; ++i)
        {
            h ^= (v >> (8 * i)) & 0xFF;
            h *= 1099511628211ULL;
        }
    };
    for (const std::int32_t height : t.SurfaceHeightMillimetres)
        add(static_cast<std::uint64_t>(static_cast<std::int64_t>(height)));
    add(static_cast<std::uint64_t>(t.Theme));
    add(static_cast<std::uint64_t>(t.Danger));
    add(t.Feature ? 1 + static_cast<std::uint64_t>(t.Feature->Kind) * 1024 + t.Feature->LocalBlockX * 32 + t.Feature->LocalBlockY : 0);
    return h;
}

std::uint64_t SampleFingerprint()
{
    std::uint64_t combined = 0;
    const std::int64_t edge = RealmFloorChunksPerEdge(1);
    for (const RealmChunk chunk : {RealmChunk{1, 0, 0}, RealmChunk{1, 1, 0}, RealmChunk{1, 12345, 67890},
                                   RealmChunk{1, edge - 1, edge - 1}, RealmChunk{3, 500, 500}, RealmChunk{12, 7, 9}})
    {
        combined = combined * 31 + Fingerprint(*GenerateRealmChunk(Seed, chunk));
    }
    return combined;
}
} // namespace

int main()
{
    if (std::getenv("PW_PRINT_GOLDEN") != nullptr)
    {
        std::cout << "height(0,0)=" << RealmSurfaceHeightMillimetres(Seed, 1, 0, 0)
                  << " height(1000,2000)=" << RealmSurfaceHeightMillimetres(Seed, 1, 1000, 2000)
                  << " fingerprint=0x" << std::hex << SampleFingerprint() << std::dec << '\n';
        return 0;
    }

    // Golden values: if the generator changes, every saved realm changes, so
    // any change here must be deliberate and bump the realm generator version.
    Check(RealmSurfaceHeightMillimetres(Seed, 1, 0, 0) == GOLDEN_HEIGHT_0_0, "golden height at the origin");
    Check(RealmSurfaceHeightMillimetres(Seed, 1, 1000, 2000) == GOLDEN_HEIGHT_1000_2000, "golden height at (1000, 2000)");
    Check(SampleFingerprint() == GOLDEN_FINGERPRINT, "golden fingerprint over six chunks on three floors");

    // Order independence and statelessness: generating in a random order, or
    // twice, gives identical chunks.
    {
        std::vector<RealmChunk> chunks;
        for (std::int64_t y = 1000; y < 1010; ++y)
            for (std::int64_t x = 2000; x < 2010; ++x)
                chunks.push_back({1, x, y});
        std::vector<std::uint64_t> forward;
        for (const RealmChunk& c : chunks)
            forward.push_back(Fingerprint(*GenerateRealmChunk(Seed, c)));
        std::vector<std::size_t> order(chunks.size());
        for (std::size_t i = 0; i < order.size(); ++i)
            order[i] = i;
        std::shuffle(order.begin(), order.end(), std::mt19937_64(7));
        for (const std::size_t i : order)
            Check(Fingerprint(*GenerateRealmChunk(Seed, chunks[i])) == forward[i], "any order gives the same chunk");
        Check(Fingerprint(*GenerateRealmChunk(Seed + 1, chunks[0])) != forward[0], "a different seed gives a different realm");
    }

    // No seams: a chunk's edge columns are exactly the global function, and
    // the step across a chunk boundary is no larger than steps inside chunks.
    {
        const RealmChunkTerrain a = *GenerateRealmChunk(Seed, {1, 3000, 3000});
        const RealmChunkTerrain b = *GenerateRealmChunk(Seed, {1, 3001, 3000});
        std::int64_t worstInside = 0;
        std::int64_t worstAcross = 0;
        for (std::int32_t y = 0; y < RealmBlocksPerChunkEdge; ++y)
        {
            const std::int32_t lastOfA = a.SurfaceHeightMillimetres[static_cast<std::size_t>(y * 32 + 31)];
            const std::int32_t firstOfB = b.SurfaceHeightMillimetres[static_cast<std::size_t>(y * 32)];
            Check(lastOfA == RealmSurfaceHeightMillimetres(Seed, 1, 3000 * 32 + 31, 3000 * 32 + y), "edge column matches the global field");
            Check(firstOfB == RealmSurfaceHeightMillimetres(Seed, 1, 3001 * 32, 3000 * 32 + y), "neighbour's edge matches too");
            worstAcross = std::max<std::int64_t>(worstAcross, std::abs(firstOfB - lastOfA));
            for (std::int32_t x = 0; x < 31; ++x)
            {
                const std::size_t i = static_cast<std::size_t>(y * 32 + x);
                worstInside = std::max<std::int64_t>(worstInside, std::abs(a.SurfaceHeightMillimetres[i + 1] - a.SurfaceHeightMillimetres[i]));
            }
        }
        Check(worstAcross <= worstInside + 2'000, "no cliff at the chunk boundary");
        Check(worstInside < 2'000, "one-metre blocks never jump two metres in a step");
    }

    // Heights stay within the octave amplitudes, everywhere including far corners.
    {
        const std::int64_t last = RealmFloorEdgeMillimetres(1) / 1'000 - 1;
        std::mt19937_64 random(11);
        std::uniform_int_distribution<std::int64_t> anywhere(0, last);
        for (int i = 0; i < 20'000; ++i)
        {
            const std::int32_t h = RealmSurfaceHeightMillimetres(Seed, 1 + i % RealmFloorCount, anywhere(random) % (last + 1), anywhere(random));
            Check(h >= -560'000 && h <= 560'000, "height within ±560 m");
        }
        Check(RealmSurfaceHeightMillimetres(Seed, 1, last, last) >= -560'000, "the far corner generates");
    }

    // Danger: safe at the origin haven, rising with distance and with depth.
    {
        Check(IsRealmHavenRegion(Seed, 1, 0, 0) && IsRealmHavenRegion(Seed, 12, 0, 0), "region (0, 0) is always a haven");
        const std::int64_t centreChunk = RealmRegionEdgeSectors * RealmChunksPerSectorEdge / 2;
        Check(RealmChunkDanger(Seed, {1, centreChunk, centreChunk}) == HazardRank::F, "a haven's centre is F");
        Check(RealmChunkDanger(Seed, {3, centreChunk, centreChunk}) == HazardRank::E, "two floors down adds a rank");
        Check(RealmChunkDanger(Seed, {12, 0, 0}) == HazardRank::S, "the deepest floor is S");
        std::set<HazardRank> seen;
        const std::int64_t edge = RealmFloorChunksPerEdge(1);
        for (std::int64_t i = 0; i < 2'000; ++i)
            seen.insert(RealmChunkDanger(Seed, {1, (i * 7919) % edge, (i * 104729) % edge}));
        Check(seen.size() >= 4, "floor 1 spans many danger ranks");
        std::set<RealmTheme> themes;
        for (std::int64_t r = 0; r < 34; ++r)
            themes.insert(RealmRegionTheme(Seed, 1, r, 33 - r));
        Check(themes.size() >= 5, "regions vary in theme");
    }

    // Landmarks exist before anyone visits, about one per 8 x 8 cell in sixteen.
    {
        int features = 0;
        for (std::int64_t y = 0; y < 256; ++y)
            for (std::int64_t x = 0; x < 256; ++x)
            {
                const RealmChunkTerrain t = *GenerateRealmChunk(Seed, {1, 40'000 + x, 40'000 + y});
                if (t.Feature)
                {
                    ++features;
                    Check(t.Feature->Kind != RealmFeatureKind::Tower || t.Danger >= HazardRank::C, "towers stand only in dangerous country");
                }
            }
        Check(features >= 30 && features <= 110, "about 64 landmarks in 1,024 cells");
    }

    Check(!GenerateRealmChunk(Seed, {1, -1, 0}).has_value(), "a chunk off the floor is refused");
    Check(!GenerateRealmChunk(Seed, {13, 0, 0}).has_value(), "a chunk on a missing floor is refused");

    // Throughput: 10,000 chunks must generate within 5 seconds in a Release build.
    {
        const auto start = std::chrono::steady_clock::now();
        std::uint64_t sink = 0;
        for (std::int64_t i = 0; i < 10'000; ++i)
            sink ^= GenerateRealmChunk(Seed, {1, 100'000 + i % 100, 200'000 + i / 100})->SurfaceHeightMillimetres[i % 1024];
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << "10,000 chunks in " << seconds << " s (sink " << sink % 10 << ")\n";
        Check(seconds < 5.0, "10,000 chunks within 5 seconds");
    }

    std::cout << "PASS: " << Checks << " realm generator checks\n";
    return 0;
}
