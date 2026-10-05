#pragma once

// The independent reference encoder for the world hash, and the pinned worlds and goldens.
// Included by WorldHashTests.cpp only; it relies on that file's Check().

#include "WorldHash.h"

#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

void Check(bool condition, const char* message);

namespace
{
// ---- An independent reference encoder: builds the bytes first, then hashes them. ----

std::uint64_t RefFnv1a(const std::string& bytes)
{
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (const char c : bytes)
    {
        h ^= static_cast<std::uint64_t>(static_cast<unsigned char>(c));
        h *= 0x100000001b3ULL;
    }
    return h;
}

struct RefBytes
{
    std::string Data;
    void Le64(std::uint64_t v)
    {
        for (int i = 0; i < 8; ++i)
            Data.push_back(static_cast<char>((v >> (8 * i)) & 0xff));
    }
    void U(std::uint64_t v) { Data.push_back('u'); Le64(v); }
    void I(std::int64_t v) { Data.push_back('i'); Le64(static_cast<std::uint64_t>(v)); }
    void B(bool v) { Data.push_back('b'); Data.push_back(v ? '\x01' : '\x00'); }
    void S(const std::string& v) { Data.push_back('s'); Le64(v.size()); Data += v; }
    void N(std::size_t v) { Data.push_back('n'); Le64(v); }
};

std::vector<std::pair<std::string, std::uint64_t>> RefSections(const WorldState& w)
{
    RefBytes meta;
    meta.U(w.WorldSeed);
    meta.U(w.Seq);
    const GameClockSave save = w.Clock.CaptureSave();
    RefBytes clock;
    clock.I(save.Now);
    clock.I(save.NextHandle);
    clock.N(save.Pending.size());
    for (const ScheduledEvent& e : save.Pending)
        clock.I(e.Handle), clock.I(e.When), clock.S(e.Id);
    RefBytes flags;
    flags.N(w.Flags.size());
    for (const auto& kv : w.Flags)
        flags.S(kv.first), flags.B(kv.second);
    RefBytes gates;
    gates.N(w.Gates.size());
    for (const auto& [id, g] : w.Gates)
        gates.S(id), gates.U(static_cast<std::uint64_t>(g.Rank)), gates.U(static_cast<std::uint64_t>(g.Status)), gates.I(g.StatusSince),
            gates.I(g.OnsetCount);
    // rng: a count of streams, then per stream (in RandomStreamId order) its four xoshiro words and its draw counter.
    RefBytes rng;
    rng.N(w.Rng.size());
    for (const RandomStreamState& s : w.Rng)
    {
        for (const std::uint64_t word : s.Words)
            rng.U(word);
        rng.U(s.Draws);
    }
    return {{"meta", RefFnv1a(meta.Data)},   {"clock", RefFnv1a(clock.Data)}, {"flags", RefFnv1a(flags.Data)},
            {"gates", RefFnv1a(gates.Data)}, {"rng", RefFnv1a(rng.Data)}};
}

std::uint64_t RefWorld(const WorldState& w)
{
    RefBytes all;
    all.U(WorldHashVersion);
    for (const auto& s : RefSections(w))
    {
        all.S(s.first);
        all.U(s.second);
    }
    return RefFnv1a(all.Data);
}

/** The library's hashes must equal the reference encoder's, section by section and overall. */
void CheckAgainstReference(const WorldState& w, const char* what)
{
    const std::vector<WorldSectionHash> got = HashWorldSections(w);
    const auto want = RefSections(w);
    Check(got.size() == want.size(), what);
    for (std::size_t i = 0; i < got.size() && i < want.size(); ++i)
        Check(got[i].Name == want[i].first && got[i].Hash == want[i].second, what);
    Check(HashWorldState(w) == RefWorld(w), what);
}

// ---- Worlds ----

WorldState MakeWorld(std::uint64_t seed, std::uint64_t seq, const GameClockSave& clock,
                     const std::vector<std::pair<std::string, bool>>& flags)
{
    WorldState w;
    w.WorldSeed = seed;
    w.Seq = seq;
    Check(w.Clock.RestoreSave(clock), "a test clock save is consistent");
    for (const auto& f : flags)
        w.Flags[f.first] = f.second;
    return w;
}

WorldState HandBuiltWorld()
{
    GameClockSave clock;
    clock.Now = 1234;
    clock.NextHandle = 5;
    clock.Pending = {{2, 1300, "test/kind/a"}, {4, 1300, "test/kind/b"}, {3, 2000, "sys/x/y"}};
    WorldState w = MakeWorld(2042, 7, clock, {{"door.open", true}, {"gate.cordoned", false}});
    // Literal stream states, not drawn from the generator, so the golden does not depend on the RNG implementation.
    for (std::size_t i = 0; i < RandomStreamCount; ++i)
    {
        const std::uint64_t base = 10 * (i + 1);
        w.Rng[i].Words = {base + 1, base + 2, base + 3, base + 4};
        w.Rng[i].Draws = 7 * i + 1;
    }
    return w;
}

WorldState EdgeWorld()
{
    constexpr std::int64_t Max = std::numeric_limits<std::int64_t>::max();
    GameClockSave clock;
    clock.Now = std::numeric_limits<std::int64_t>::min();
    clock.NextHandle = Max;
    clock.Pending = {{Max - 1, Max, std::string("nul\0inside/\xff\x80/key", 18)}, {1, std::numeric_limits<std::int64_t>::min(), ""}};
    WorldState w = MakeWorld(std::numeric_limits<std::uint64_t>::max(), std::numeric_limits<std::uint64_t>::max() - 1, clock,
                             {{"", false}, {std::string("a\0b", 3), true}, {"\xc3\xa9.unicode", true}});
    constexpr std::uint64_t Top = std::numeric_limits<std::uint64_t>::max();
    for (std::size_t i = 0; i < RandomStreamCount; ++i)
    {
        w.Rng[i].Words = {Top - i, 0x8000000000000000ULL, 1, 0x0123456789abcdefULL};
        w.Rng[i].Draws = Top - 3 * i;
    }
    return w;
}

// Pinned on 2026-10-05 for WorldHashVersion 3 (rng section added). Derived from the independent
// reference encoder above (PW_PRINT_GOLDEN=1 prints RefWorld, after the library has been checked
// equal to it) and cross-checked by a separate Python implementation of the byte layout. A change
// here means the encoding changed.
constexpr std::uint64_t GoldenHandBuilt = 0x094455d520abf770ULL;
constexpr std::uint64_t GoldenEdge = 0xf0312d8420e90da4ULL;
} // namespace
