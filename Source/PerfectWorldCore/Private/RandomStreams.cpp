#include "RandomStreams.h"

namespace PerfectWorld
{
namespace
{
// The stable text IDs, indexed by RandomStreamId. These strings are hashed into
// each stream's seed and appear in saves, so they never change.
constexpr std::array<std::string_view, RandomStreamCount> RandomStreamsNames = {
    "opportunity", "tower", "ambience", "crystallisation"};

constexpr std::uint64_t RandomStreamsGamma = 0x9E3779B97F4A7C15ULL;
// Domain tag so keyed draws can never coincide with a stream's own sequence.
constexpr std::uint64_t RandomStreamsKeyedTag = 0x4B45594544524E47ULL; // "KEYEDRNG"

constexpr std::uint64_t RandomStreamsRotl(std::uint64_t value, int count)
{
    return (value << count) | (value >> (64 - count));
}

// SplitMix64's output function: a bijective, well-avalanched 64-bit mixer. Used
// on its own to combine seeds and keys.
constexpr std::uint64_t RandomStreamsMix(std::uint64_t z)
{
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

// One SplitMix64 step: advance the state by the golden gamma, then mix it.
constexpr std::uint64_t RandomStreamsSplitMixNext(std::uint64_t& state)
{
    state += RandomStreamsGamma;
    return RandomStreamsMix(state);
}

// FNV-1a, 64 bit, over the bytes of a text ID. Hand-written rather than
// std::hash because std::hash differs between standard libraries and runs.
constexpr std::uint64_t RandomStreamsFnv1a(std::string_view text)
{
    std::uint64_t hash = 0xCBF29CE484222325ULL;
    for (const char character : text)
    {
        hash ^= static_cast<std::uint8_t>(character);
        hash *= 0x100000001B3ULL;
    }
    return hash;
}

// Folds one 64-bit value into a running hash. Adding the gamma first keeps a
// zero value and a zero hash from being fixed points.
constexpr std::uint64_t RandomStreamsAbsorb(std::uint64_t hash, std::uint64_t value)
{
    return RandomStreamsMix((hash ^ value) + RandomStreamsGamma);
}

// 64 x 64 -> 128-bit product from 32-bit limbs, so no __int128 (MSVC has none).
void RandomStreamsMultiply(std::uint64_t a, std::uint64_t b, std::uint64_t& high, std::uint64_t& low)
{
    const std::uint64_t aLow = a & 0xFFFFFFFFULL;
    const std::uint64_t aHigh = a >> 32;
    const std::uint64_t bLow = b & 0xFFFFFFFFULL;
    const std::uint64_t bHigh = b >> 32;

    const std::uint64_t lowLow = aLow * bLow;
    const std::uint64_t highLow = aHigh * bLow;
    const std::uint64_t lowHigh = aLow * bHigh;
    const std::uint64_t highHigh = aHigh * bHigh;

    // The middle column cannot overflow 64 bits: each term is below 2^32 and
    // there are three of them.
    const std::uint64_t middle = (lowLow >> 32) + (highLow & 0xFFFFFFFFULL) + lowHigh;
    low = (middle << 32) | (lowLow & 0xFFFFFFFFULL);
    high = highHigh + (highLow >> 32) + (middle >> 32);
}

RandomStreamState RandomStreamsStateFromSeed(std::uint64_t seed)
{
    RandomStreamState state;
    std::uint64_t splitMix = seed;
    for (std::uint64_t& word : state.Words)
    {
        word = RandomStreamsSplitMixNext(splitMix);
    }
    // Four consecutive SplitMix64 outputs are distinct (the mixer is a
    // bijection of distinct states), so all-zero cannot occur; guard anyway
    // because an all-zero state would silently freeze the stream.
    if (!IsValidRandomStreamState(state))
    {
        state.Words[0] = 1;
    }
    return state;
}
} // namespace

std::string_view RandomStreamName(RandomStreamId id)
{
    const std::size_t index = static_cast<std::size_t>(id);
    return index < RandomStreamCount ? RandomStreamsNames[index] : std::string_view{};
}

std::optional<RandomStreamId> ParseRandomStreamId(std::string_view name)
{
    for (std::size_t index = 0; index < RandomStreamCount; ++index)
    {
        if (RandomStreamsNames[index] == name)
        {
            return static_cast<RandomStreamId>(index);
        }
    }
    return std::nullopt;
}

std::uint64_t DeriveRandomStreamSeed(std::uint64_t lifeSeed, RandomStreamId id)
{
    // Mix the seed first so related life seeds (1, 2, 3) do not give related
    // stream seeds, then fold in the name's hash. Only the name enters, never
    // the stream's index, which is what keeps added streams from moving old ones.
    return RandomStreamsAbsorb(RandomStreamsMix(lifeSeed + RandomStreamsGamma), RandomStreamsFnv1a(RandomStreamName(id)));
}

bool IsValidRandomStreamState(const RandomStreamState& state)
{
    return (state.Words[0] | state.Words[1] | state.Words[2] | state.Words[3]) != 0;
}

RandomStream::RandomStream(std::uint64_t seed) : State(RandomStreamsStateFromSeed(seed))
{
}

std::uint64_t RandomStream::NextU64()
{
    // xoshiro256** 1.0, transcribed from the authors' public-domain reference.
    std::array<std::uint64_t, 4>& s = State.Words;
    const std::uint64_t result = RandomStreamsRotl(s[1] * 5, 7) * 9;
    const std::uint64_t t = s[1] << 17;
    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = RandomStreamsRotl(s[3], 45);
    ++State.Draws;
    return result;
}

std::uint64_t RandomStream::NextBelow(std::uint64_t bound)
{
    if (bound <= 1)
    {
        return 0;
    }
    // Lemire: the high half of x * bound is uniform in [0, bound) except that
    // 2^64 mod bound of the low-half values over-represent some results; those
    // are rejected. The division is only paid for in the rare case lo < bound.
    std::uint64_t high = 0;
    std::uint64_t low = 0;
    RandomStreamsMultiply(NextU64(), bound, high, low);
    if (low < bound)
    {
        const std::uint64_t threshold = (0 - bound) % bound; // 2^64 mod bound
        while (low < threshold)
        {
            RandomStreamsMultiply(NextU64(), bound, high, low);
        }
    }
    return high;
}

std::int64_t RandomStream::NextInRange(std::int64_t low, std::int64_t high)
{
    if (high <= low)
    {
        return low;
    }
    // Unsigned subtraction is exact modulo 2^64, so the span is right even for
    // ranges wider than INT64_MAX. A span of 2^64 wraps to 0: take raw bits.
    const std::uint64_t span = static_cast<std::uint64_t>(high) - static_cast<std::uint64_t>(low) + 1;
    const std::uint64_t offset = span == 0 ? NextU64() : NextBelow(span);
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(low) + offset);
}

bool RandomStream::Chance(std::uint64_t numerator, std::uint64_t denominator)
{
    if (denominator == 0 || numerator == 0)
    {
        return false;
    }
    if (numerator >= denominator)
    {
        return true;
    }
    return NextBelow(denominator) < numerator;
}

RandomStreamState RandomStream::Capture() const
{
    return State;
}

bool RandomStream::Restore(const RandomStreamState& state)
{
    if (!IsValidRandomStreamState(state))
    {
        return false;
    }
    State = state;
    return true;
}

RandomStreams::RandomStreams(std::uint64_t lifeSeed)
{
    Seed(lifeSeed);
}

RandomStream& RandomStreams::Stream(RandomStreamId id)
{
    return Streams[static_cast<std::size_t>(id)];
}

const RandomStream& RandomStreams::Stream(RandomStreamId id) const
{
    return Streams[static_cast<std::size_t>(id)];
}

void RandomStreams::Seed(std::uint64_t lifeSeed)
{
    for (std::size_t index = 0; index < RandomStreamCount; ++index)
    {
        Streams[index] = RandomStream(DeriveRandomStreamSeed(lifeSeed, static_cast<RandomStreamId>(index)));
    }
}

RandomStreams::Snapshot RandomStreams::Capture() const
{
    Snapshot snapshot;
    for (std::size_t index = 0; index < RandomStreamCount; ++index)
    {
        snapshot[index] = Streams[index].Capture();
    }
    return snapshot;
}

bool RandomStreams::Restore(const Snapshot& snapshot)
{
    // Validate everything before touching anything, so a corrupt save cannot
    // leave some streams restored and others not.
    for (const RandomStreamState& state : snapshot)
    {
        if (!IsValidRandomStreamState(state))
        {
            return false;
        }
    }
    for (std::size_t index = 0; index < RandomStreamCount; ++index)
    {
        Streams[index].Restore(snapshot[index]);
    }
    return true;
}

RandomStream KeyedRandomStream(std::uint64_t worldSeed, RandomStreamId id, std::span<const std::int64_t> key, std::string_view text)
{
    std::uint64_t hash = RandomStreamsAbsorb(DeriveRandomStreamSeed(worldSeed, id), RandomStreamsKeyedTag);
    // The lengths go in first so {1} + "" cannot collide with {} + a text
    // whose hash happens to match, and so key order and arity both matter.
    hash = RandomStreamsAbsorb(hash, key.size());
    for (const std::int64_t part : key)
    {
        hash = RandomStreamsAbsorb(hash, static_cast<std::uint64_t>(part));
    }
    hash = RandomStreamsAbsorb(hash, text.size());
    hash = RandomStreamsAbsorb(hash, RandomStreamsFnv1a(text));
    return RandomStream(hash);
}

std::uint64_t KeyedRandomU64(std::uint64_t worldSeed, RandomStreamId id, std::span<const std::int64_t> key, std::string_view text)
{
    return KeyedRandomStream(worldSeed, id, key, text).NextU64();
}

std::uint64_t KeyedRandomBelow(
    std::uint64_t worldSeed, RandomStreamId id, std::uint64_t bound, std::span<const std::int64_t> key, std::string_view text)
{
    return KeyedRandomStream(worldSeed, id, key, text).NextBelow(bound);
}
} // namespace PerfectWorld
