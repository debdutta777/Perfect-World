// Needed before any include so MSVC does not warn about std::getenv.
#define _CRT_SECURE_NO_WARNINGS

#include "RandomStreams.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
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

namespace
{
constexpr std::uint64_t RandomTestLifeSeed = 0x5EED000120260929ULL;

constexpr std::array<RandomStreamId, RandomStreamCount> RandomTestAllStreams = {
    RandomStreamId::Opportunity, RandomStreamId::Tower, RandomStreamId::Ambience, RandomStreamId::Crystallisation};

// Pinned first four raw draws of each stream for RandomTestLifeSeed. They were
// first produced by an independent Python transcription of SplitMix64 +
// xoshiro256** (so the C++ is checked against a second implementation, not
// against itself), and confirmed by running this binary with PW_PRINT_GOLDEN=1,
// which prints the C++ values in this table's format. A change here means every
// saved life would roll differently: it is a save-breaking change.
constexpr std::array<std::array<std::uint64_t, 4>, RandomStreamCount> RandomTestGolden = {{
    {0x1890FB60E9B0F9F1ULL, 0x1E3FF3B84563D9BDULL, 0xC460150F67B856DBULL, 0xA003356F339F5D60ULL},
    {0xDF9784108F9476D8ULL, 0x29B3EC7B6E4AD781ULL, 0xDE198E91CA155CBFULL, 0x9D8C633AA37FCEB8ULL},
    {0x03266C2803657C99ULL, 0xF92B9A639CDA8763ULL, 0x1A8A090A00633C97ULL, 0x1AC4C7D330DC8A5CULL},
    {0x079132C83E5CBFF7ULL, 0x9BBD6638C9EACBDEULL, 0x8AF47AA8B25F2A1FULL, 0x248F66ED9D28175CULL},
}};
// Keyed draw: world seed RandomTestLifeSeed, tower, key {3, -7}, text "chest".
constexpr std::uint64_t RandomTestKeyedGolden = 0x31EF998E2DA9D631ULL;

void RandomTestPrintGolden()
{
    std::cout << std::hex << std::uppercase;
    for (const RandomStreamId id : RandomTestAllStreams)
    {
        RandomStreams streams(RandomTestLifeSeed);
        std::cout << "    {";
        for (int i = 0; i < 4; ++i)
        {
            std::cout << (i ? ", " : "") << "0x" << streams.Stream(id).NextU64() << "ULL";
        }
        std::cout << "},  // " << RandomStreamName(id) << '\n';
    }
    const std::array<std::int64_t, 2> key = {3, -7};
    std::cout << "keyed: 0x" << KeyedRandomU64(RandomTestLifeSeed, RandomStreamId::Tower, key, "chest") << "ULL\n";
    std::cout << std::dec << std::nouppercase;
}

// Every stream's next few draws, for comparing "did anything move?".
std::array<std::uint64_t, 4> RandomTestPeek(RandomStream copy)
{
    return {copy.NextU64(), copy.NextU64(), copy.NextU64(), copy.NextU64()};
}
} // namespace

int main()
{
    if (std::getenv("PW_PRINT_GOLDEN") != nullptr)
    {
        RandomTestPrintGolden();
    }

    // The two building blocks against their published reference outputs.
    {
        // SplitMix64 from seed 0 (Vigna's reference): the seeding expansion.
        const RandomStreamState seeded = RandomStream(0).Capture();
        Check(seeded.Words[0] == 0xE220A8397B1DCDAFULL && seeded.Words[1] == 0x6E789E6AA1B965F4ULL &&
                  seeded.Words[2] == 0x06C45D188009454FULL && seeded.Words[3] == 0xF88BB8A8724C81ECULL,
              "SplitMix64 seed 0 matches the reference outputs");

        // xoshiro256** from state {1, 2, 3, 4}: 11520 = (rotl(2 * 5, 7)) * 9 first.
        RandomStream raw;
        Check(raw.Restore(RandomStreamState{{1, 2, 3, 4}, 0}), "a plain nonzero state restores");
        const std::array<std::uint64_t, 4> expected = {11520ULL, 0ULL, 1509978240ULL, 1215971899390074240ULL};
        for (const std::uint64_t value : expected)
        {
            Check(raw.NextU64() == value, "xoshiro256** from {1,2,3,4} matches the reference sequence");
        }
        Check(raw.Capture().Draws == 4, "four draws are counted");
    }

    // Stable text IDs and their round trip.
    {
        Check(RandomStreamName(RandomStreamId::Opportunity) == "opportunity", "opportunity ID");
        Check(RandomStreamName(RandomStreamId::Tower) == "tower", "tower ID");
        Check(RandomStreamName(RandomStreamId::Ambience) == "ambience", "ambience ID");
        Check(RandomStreamName(RandomStreamId::Crystallisation) == "crystallisation", "crystallisation ID");
        Check(RandomStreamName(static_cast<RandomStreamId>(200)).empty(), "a bad stream value has no name");
        for (const RandomStreamId id : RandomTestAllStreams)
        {
            Check(ParseRandomStreamId(RandomStreamName(id)) == id, "every stream ID parses back");
        }
        Check(!ParseRandomStreamId("Tower").has_value() && !ParseRandomStreamId("").has_value(), "IDs are exact");
    }

    // Golden values: a fixed life seed gives these exact sequences forever.
    {
        for (std::size_t index = 0; index < RandomStreamCount; ++index)
        {
            RandomStreams streams(RandomTestLifeSeed);
            RandomStream& stream = streams.Stream(RandomTestAllStreams[index]);
            for (const std::uint64_t value : RandomTestGolden[index])
            {
                Check(stream.NextU64() == value, "golden first draws of each stream");
            }
        }
        const std::array<std::int64_t, 2> key = {3, -7};
        Check(KeyedRandomU64(RandomTestLifeSeed, RandomStreamId::Tower, key, "chest") == RandomTestKeyedGolden,
              "golden keyed draw");

        // Different life seeds give different streams, and the streams differ from each other.
        const RandomStreams other(RandomTestLifeSeed + 1);
        Check(RandomTestPeek(other.Stream(RandomStreamId::Tower)) != RandomTestGolden[1], "another life seed rolls differently");
        Check(RandomTestGolden[0][0] != RandomTestGolden[1][0] && RandomTestGolden[1][0] != RandomTestGolden[2][0] &&
                  RandomTestGolden[2][0] != RandomTestGolden[3][0],
              "the four streams start differently");
        // A stream's seed depends on the life seed and its name only, not on
        // the stream's position, so a stream can be rebuilt on its own.
        Check(RandomStream(DeriveRandomStreamSeed(RandomTestLifeSeed, RandomStreamId::Ambience)).NextU64() ==
                  RandomTestGolden[2][0],
              "a stream is reproducible from the derived seed alone");
    }

    // Isolation: a million draws on one stream leave the others exactly as fresh.
    {
        RandomStreams streams(RandomTestLifeSeed);
        const RandomStreams fresh(RandomTestLifeSeed);
        RandomStream& tower = streams.Stream(RandomStreamId::Tower);
        std::uint64_t sink = 0;
        for (int i = 0; i < 1'000'000; ++i)
        {
            sink ^= tower.NextU64();
        }
        Check(sink != 0, "the million draws happened");
        Check(tower.Capture().Draws == 1'000'000, "the tower stream counted a million draws");
        for (const RandomStreamId id : RandomTestAllStreams)
        {
            if (id == RandomStreamId::Tower)
            {
                continue;
            }
            Check(streams.Stream(id).Capture() == fresh.Stream(id).Capture(), "other streams are untouched");
            Check(streams.Stream(id).Capture().Draws == 0, "other streams drew nothing");
        }
        Check(streams.Stream(RandomStreamId::Opportunity).NextU64() == RandomTestGolden[0][0],
              "the opportunity stream still starts at its golden value");
    }

    // Capture mid-sequence, draw, restore, draw: identical. Also through a
    // brand-new object, which is what loading a save does.
    {
        RandomStreams streams(RandomTestLifeSeed);
        for (int i = 0; i < 137; ++i)
        {
            streams.Stream(RandomStreamId::Opportunity).NextU64();
            streams.Stream(RandomStreamId::Crystallisation).NextBelow(1000);
        }
        const RandomStreams::Snapshot saved = streams.Capture();
        std::vector<std::uint64_t> first;
        for (int i = 0; i < 500; ++i)
        {
            for (const RandomStreamId id : RandomTestAllStreams)
            {
                first.push_back(streams.Stream(id).NextBelow(1'000'003));
            }
        }
        Check(streams.Restore(saved), "a captured snapshot restores");
        Check(streams.Capture() == saved, "the restored state equals the snapshot, draw counts included");
        RandomStreams reloaded(0xDEADBEEFULL); // a different seed: restore must replace everything
        Check(reloaded.Restore(saved), "a snapshot restores into a new object");
        std::size_t at = 0;
        for (int i = 0; i < 500; ++i)
        {
            for (const RandomStreamId id : RandomTestAllStreams)
            {
                Check(streams.Stream(id).NextBelow(1'000'003) == first[at], "draws after restore repeat exactly");
                Check(reloaded.Stream(id).NextBelow(1'000'003) == first[at], "draws after a reload repeat exactly");
                ++at;
            }
        }
        Check(streams.Capture() == reloaded.Capture(), "both continuations end in the same state");
    }

    // Restore refuses an all-zero state and changes nothing.
    {
        RandomStream stream(7);
        stream.NextU64();
        const RandomStreamState before = stream.Capture();
        Check(!IsValidRandomStreamState(RandomStreamState{}), "all-zero is invalid");
        Check(!stream.Restore(RandomStreamState{{0, 0, 0, 0}, 99}), "a stream refuses an all-zero state");
        Check(stream.Capture() == before, "a refused restore leaves the stream untouched");

        RandomStreams streams(RandomTestLifeSeed);
        streams.Stream(RandomStreamId::Tower).NextU64();
        const RandomStreams::Snapshot good = streams.Capture();
        RandomStreams::Snapshot corrupt = good;
        corrupt[3] = RandomStreamState{};
        RandomStreams other(RandomTestLifeSeed + 5);
        const RandomStreams::Snapshot otherBefore = other.Capture();
        Check(!other.Restore(corrupt), "a snapshot with one bad stream is refused");
        Check(other.Capture() == otherBefore, "a refused snapshot changes no stream, not even the valid ones");

        // Even seed 0 produces a valid, live generator.
        Check(IsValidRandomStreamState(RandomStream(0).Capture()), "seed 0 expands to a valid state");
    }

    // Bounded draws: always in range, edge cases exact, coarse uniformity.
    {
        RandomStream stream(RandomTestLifeSeed);
        const std::uint64_t before = stream.Capture().Draws;
        Check(stream.NextBelow(0) == 0 && stream.NextBelow(1) == 0, "bounds 0 and 1 answer 0");
        Check(stream.NextInRange(5, 5) == 5 && stream.NextInRange(9, 2) == 9, "an empty or inverted range answers low");
        Check(!stream.Chance(0, 10) && !stream.Chance(5, 0) && stream.Chance(10, 10) && stream.Chance(11, 10),
              "certain and impossible chances are exact");
        Check(stream.Capture().Draws == before, "those edge cases consumed nothing");

        constexpr int Buckets = 10;
        constexpr int Samples = 100'000;
        std::array<int, Buckets> counts{};
        for (int i = 0; i < Samples; ++i)
        {
            const std::uint64_t value = stream.NextBelow(Buckets);
            Check(value < Buckets, "NextBelow stays below its bound");
            ++counts[static_cast<std::size_t>(value)];
        }
        // Expected 10,000 each, sigma 95; +-1,000 is over ten sigma, so this
        // only fails for a real bias, and the seed is fixed so it cannot flake.
        for (const int count : counts)
        {
            Check(count > 9'000 && count < 11'000, "NextBelow(10) is roughly uniform");
        }

        // A bound just over half the range is where modulo bias is worst
        // (low values twice as likely). Lemire must keep the halves even.
        const std::uint64_t large = (std::uint64_t{1} << 63) + (std::uint64_t{1} << 61);
        int lowerHalf = 0;
        for (int i = 0; i < Samples; ++i)
        {
            const std::uint64_t value = stream.NextBelow(large);
            Check(value < large, "a huge bound is respected");
            lowerHalf += value < large / 2 ? 1 : 0;
        }
        Check(lowerHalf > 49'000 && lowerHalf < 51'000, "a huge bound has no modulo bias");

        // Inclusive ranges reach both ends, including negative and extreme ones.
        bool sawLow = false;
        bool sawHigh = false;
        for (int i = 0; i < 2'000; ++i)
        {
            const std::int64_t value = stream.NextInRange(-3, 4);
            Check(value >= -3 && value <= 4, "NextInRange stays inside");
            sawLow = sawLow || value == -3;
            sawHigh = sawHigh || value == 4;
        }
        Check(sawLow && sawHigh, "NextInRange reaches both inclusive ends");
        for (int i = 0; i < 200; ++i)
        {
            const std::int64_t value = stream.NextInRange(INT64_MIN, INT64_MAX - 1);
            Check(value < INT64_MAX, "a near-full range stays inside");
            const std::int64_t wide = stream.NextInRange(INT64_MIN, INT64_MAX);
            (void)wide; // the full range has no out-of-range value; it must simply not fault
        }
        Check(stream.NextInRange(INT64_MAX - 1, INT64_MAX) >= INT64_MAX - 1, "a range at the top end works");

        // Chance(1, 4) is about a quarter; Chance(3, 4) about three quarters.
        int quarter = 0;
        int threeQuarters = 0;
        for (int i = 0; i < Samples; ++i)
        {
            quarter += stream.Chance(1, 4) ? 1 : 0;
            threeQuarters += stream.Chance(3, 4) ? 1 : 0;
        }
        Check(quarter > 24'000 && quarter < 26'000, "Chance(1, 4) is about 25%");
        Check(threeQuarters > 74'000 && threeQuarters < 76'000, "Chance(3, 4) is about 75%");
    }

    // Keyed draws: stateless, so order and repetition cannot matter, and every
    // part of the key counts.
    {
        const std::array<std::int64_t, 2> a = {3, -7};
        const std::array<std::int64_t, 2> swapped = {-7, 3};
        const std::array<std::int64_t, 3> longer = {3, -7, 0};
        const std::array<std::int64_t, 1> single = {0};
        const std::span<const std::int64_t> none;

        const std::uint64_t reference = KeyedRandomU64(RandomTestLifeSeed, RandomStreamId::Tower, a, "chest");
        // Ask a lot of unrelated questions in between, and ask again.
        std::uint64_t sink = 0;
        for (std::int64_t i = 0; i < 1'000; ++i)
        {
            const std::array<std::int64_t, 1> other = {i};
            sink ^= KeyedRandomU64(RandomTestLifeSeed, RandomStreamId::Ambience, other, "x");
        }
        Check(sink != 0, "unrelated keyed questions were asked");
        Check(KeyedRandomU64(RandomTestLifeSeed, RandomStreamId::Tower, a, "chest") == reference, "asking again gives the same answer");

        // Reverse order of a set of questions gives the same answers.
        std::vector<std::uint64_t> forward;
        for (std::int64_t i = 0; i < 100; ++i)
        {
            const std::array<std::int64_t, 1> k = {i};
            forward.push_back(KeyedRandomBelow(RandomTestLifeSeed, RandomStreamId::Opportunity, 1'000'000, k));
        }
        for (std::int64_t i = 99; i >= 0; --i)
        {
            const std::array<std::int64_t, 1> k = {i};
            Check(KeyedRandomBelow(RandomTestLifeSeed, RandomStreamId::Opportunity, 1'000'000, k) ==
                      forward[static_cast<std::size_t>(i)],
                  "keyed answers do not depend on the order of asking");
        }

        // Drawing from the sequential streams does not change keyed answers.
        RandomStreams streams(RandomTestLifeSeed);
        for (int i = 0; i < 1'000; ++i)
        {
            streams.Stream(RandomStreamId::Tower).NextU64();
        }
        Check(KeyedRandomU64(RandomTestLifeSeed, RandomStreamId::Tower, a, "chest") == reference,
              "keyed draws ignore the sequential stream state");

        // Key sensitivity: each change moves the answer.
        Check(KeyedRandomU64(RandomTestLifeSeed, RandomStreamId::Tower, swapped, "chest") != reference, "key order matters");
        Check(KeyedRandomU64(RandomTestLifeSeed, RandomStreamId::Tower, longer, "chest") != reference, "key length matters");
        Check(KeyedRandomU64(RandomTestLifeSeed, RandomStreamId::Tower, a, "chests") != reference, "text matters");
        Check(KeyedRandomU64(RandomTestLifeSeed, RandomStreamId::Tower, a, "") != reference, "no text differs from text");
        Check(KeyedRandomU64(RandomTestLifeSeed, RandomStreamId::Ambience, a, "chest") != reference, "the stream matters");
        Check(KeyedRandomU64(RandomTestLifeSeed + 1, RandomStreamId::Tower, a, "chest") != reference, "the world seed matters");
        Check(KeyedRandomU64(RandomTestLifeSeed, RandomStreamId::Tower, none) !=
                  KeyedRandomU64(RandomTestLifeSeed, RandomStreamId::Tower, single),
              "an empty key differs from {0}");
        Check(KeyedRandomU64(RandomTestLifeSeed, RandomStreamId::Tower, none) != RandomTestGolden[1][0],
              "a keyed draw never equals the stream's own first draw");

        // The keyed stream is a real generator: its later draws are reproducible too.
        RandomStream k1 = KeyedRandomStream(RandomTestLifeSeed, RandomStreamId::Tower, a, "chest");
        RandomStream k2 = KeyedRandomStream(RandomTestLifeSeed, RandomStreamId::Tower, a, "chest");
        Check(k1.NextU64() == reference, "the keyed stream's first draw is KeyedRandomU64");
        k2.NextU64();
        Check(k1.NextU64() == k2.NextU64(), "keyed streams continue identically");

        // Coarse spread over nearby keys: consecutive integer keys must not correlate.
        std::array<int, 8> counts{};
        for (std::int64_t i = 0; i < 8'000; ++i)
        {
            const std::array<std::int64_t, 1> k = {i};
            ++counts[KeyedRandomBelow(RandomTestLifeSeed, RandomStreamId::Opportunity, 8, k)];
        }
        for (const int count : counts)
        {
            Check(count > 850 && count < 1'150, "consecutive keys spread evenly");
        }
    }

    std::cout << "PASS: " << Checks << " random stream checks\n";
    return 0;
}
