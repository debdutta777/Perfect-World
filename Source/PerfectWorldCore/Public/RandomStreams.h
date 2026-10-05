#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * Deterministic random streams (SYSTEMS.md rule 6, ROADMAP M1-02).
 *
 * Everything chance-driven in the simulation draws from here, never from
 * std::rand, std distributions, clocks or addresses, because those differ
 * between the CTest build and the Unreal build and between runs. The rules:
 *
 *  - ONE fixed algorithm: xoshiro256** (Blackman and Vigna, "Scrambled Linear
 *    Pseudorandom Number Generators", 2018, https://prng.di.unimi.it/),
 *    seeded by SplitMix64 (Steele, Lea and Flood, "Fast Splittable Pseudorandom
 *    Number Generators", 2014; Vigna's public-domain reference). Both are
 *    hand-implemented over uint64_t, so they are bit-identical everywhere.
 *    Changing either is a save-breaking change.
 *  - Named streams, each seeded from the per-life seed AND its stable text ID.
 *    A stream's seed does not depend on how many streams exist, so adding a
 *    stream later never moves the others. Draining one stream never moves
 *    another: reloading, or a new feature drawing more often, cannot reshuffle
 *    the tower because the opportunity stream was used.
 *  - Streams have persisted state (Capture/Restore), so a reload continues the
 *    sequence instead of re-rolling it.
 *  - Keyed draws are stateless: a pure function of the persisted world seed, a
 *    stream ID and a caller key. They suit "the same question must always get
 *    the same answer" (what is in this chest, who is at this stall today),
 *    regardless of the order questions are asked in.
 *
 * Integer arithmetic only. The unbiased bound uses Lemire's multiply-and-reject
 * method (Lemire, "Fast Random Integer Generation in an Interval", 2019), not
 * modulo, so no value in a range is even slightly favoured.
 */

/**
 * Adding a stream appends to this enum and to the name table in the .cpp;
 * existing values and names never change (they are in saves).
 */
enum class RandomStreamId : std::uint8_t
{
    Opportunity,
    Tower,
    Ambience,
    Crystallisation,
};

constexpr std::size_t RandomStreamCount = 4;

/** The stable text ID of a stream ("opportunity", "tower", ...). Empty for a bad value. */
PERFECTWORLDCORE_API std::string_view RandomStreamName(RandomStreamId id);
PERFECTWORLDCORE_API std::optional<RandomStreamId> ParseRandomStreamId(std::string_view name);

/** The seed of one stream: a pure function of the per-life seed and the stream's text ID. */
PERFECTWORLDCORE_API std::uint64_t DeriveRandomStreamSeed(std::uint64_t lifeSeed, RandomStreamId id);

/** Everything needed to continue one stream exactly where it was. */
struct RandomStreamState
{
    std::array<std::uint64_t, 4> Words{};
    /** Raw 64-bit outputs consumed so far; bookkeeping for tests and the debug overlay. */
    std::uint64_t Draws = 0;

    friend bool operator==(const RandomStreamState&, const RandomStreamState&) = default;
};

/** An all-zero xoshiro state is a fixed point that only ever outputs zero, so it is never valid. */
PERFECTWORLDCORE_API bool IsValidRandomStreamState(const RandomStreamState& state);

/** One generator. Copyable by value: a copy continues independently from the same point. */
class PERFECTWORLDCORE_API RandomStream
{
public:
    /** Expands any 64-bit seed (including 0) into a valid state with SplitMix64. */
    explicit RandomStream(std::uint64_t seed = 0);

    /** Raw 64 bits. Every other draw is built from this, so Draws counts all consumption. */
    std::uint64_t NextU64();

    /**
     * Uniform in [0, bound), without modulo bias. A bound of 0 or 1 has only
     * one possible answer (0) and consumes nothing.
     */
    std::uint64_t NextBelow(std::uint64_t bound);

    /**
     * Uniform in [low, high], both inclusive, across the whole int64 range. If
     * high <= low the answer is low and nothing is consumed (a caller bug,
     * but never an out-of-range value).
     */
    std::int64_t NextInRange(std::int64_t low, std::int64_t high);

    /**
     * True with probability numerator/denominator. A denominator of 0 or a
     * numerator of 0 is never; numerator >= denominator is always. The
     * certain and impossible cases consume nothing, so changing a chance
     * between 0%, 100% and anything else does not read as a reroll elsewhere.
     */
    bool Chance(std::uint64_t numerator, std::uint64_t denominator);

    RandomStreamState Capture() const;
    /** Refuses an invalid state and leaves the stream untouched. */
    bool Restore(const RandomStreamState& state);

private:
    RandomStreamState State;
};

/** The four named streams of one life. */
class PERFECTWORLDCORE_API RandomStreams
{
public:
    explicit RandomStreams(std::uint64_t lifeSeed = 0);

    RandomStream& Stream(RandomStreamId id);
    const RandomStream& Stream(RandomStreamId id) const;

    /** Reseeds every stream from a life seed, as a new life does. */
    void Seed(std::uint64_t lifeSeed);

    using Snapshot = std::array<RandomStreamState, RandomStreamCount>;
    /** The persisted form: indexed by RandomStreamId. */
    Snapshot Capture() const;
    /** All or nothing: if any stream's state is invalid, none is changed. */
    bool Restore(const Snapshot& snapshot);

private:
    std::array<RandomStream, RandomStreamCount> Streams;
};

/**
 * A fresh generator that is a pure function of (world seed, stream, integer
 * key, text key). Nothing is stored, so asking in any order, any number of
 * times, in any session, gives the same answer. The key is a sequence:
 * {1, 2} and {2, 1} are different keys, and so are {} and {0}. Keyed draws
 * never equal the same stream's sequential draws.
 */
PERFECTWORLDCORE_API RandomStream KeyedRandomStream(
    std::uint64_t worldSeed, RandomStreamId id, std::span<const std::int64_t> key, std::string_view text = {});

/** Convenience: the first raw draw of the keyed stream. */
PERFECTWORLDCORE_API std::uint64_t KeyedRandomU64(
    std::uint64_t worldSeed, RandomStreamId id, std::span<const std::int64_t> key, std::string_view text = {});
/** Convenience: the first unbiased draw below bound of the keyed stream. */
PERFECTWORLDCORE_API std::uint64_t KeyedRandomBelow(
    std::uint64_t worldSeed, RandomStreamId id, std::uint64_t bound, std::span<const std::int64_t> key, std::string_view text = {});
} // namespace PerfectWorld
