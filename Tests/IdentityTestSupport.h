// Shared helpers for the M2-05 identity and event-log tests.
#pragma once

#include "EventLog.h"
#include "IdentityRegistry.h"
#include "RandomStreams.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

using namespace PerfectWorld;

// Defined in IdentityRegistryTests.cpp, which also owns main().
extern int Checks;
void Check(bool condition, const char* message);

constexpr std::uint64_t IdTestSeed = 0x5EED000120261005ULL;

inline IdentityOrigin IdTestOrigin(const std::string& cohort, std::int64_t slot)
{
    IdentityOrigin origin;
    origin.Cohort = cohort;
    origin.Slot = slot;
    return origin;
}

// The first reference is a pure function of the member, so every ordering of the
// same references builds the same registry.
inline IdentityFirstRef IdTestRefFor(std::int64_t slot)
{
    IdentityFirstRef ref;
    ref.Kind = static_cast<IdentityReferenceKind>(slot % static_cast<std::int64_t>(IdentityReferenceKindCount));
    ref.At = 1000 + slot * 7;
    ref.Ref = (slot % 3 == 0) ? std::string() : "record_" + std::to_string(slot);
    return ref;
}

inline const std::array<const char*, 3> IdTestCohorts = {
    "nation_bra.chess.age_16_20.y2026", "nation_jpn.maths.age_21_25.y2026", "nation_ind.football.age_16_20.y2026"};

// Every strict prefix of the text must be refused, including the one that only lacks the final newline.
template <typename ParseFn>
void IdTestEveryTruncationRefused(const std::string& text, ParseFn parse, const char* message)
{
    for (std::size_t length = 0; length < text.size(); ++length)
        Check(!parse(std::string_view(text).substr(0, length)), message);
}

// Every single-byte substitution must be refused (the end hash covers the content).
template <typename ParseFn>
void IdTestEveryCorruptionRefused(const std::string& text, ParseFn parse, const char* message)
{
    const std::array<char, 4> replacements = {'0', '7', 'z', ' '};
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        for (const char replacement : replacements)
        {
            if (text[i] == replacement || text[i] == '\n')
                continue;
            std::string copy = text;
            copy[i] = replacement;
            Check(!parse(copy), message);
        }
    }
}

inline std::string IdTestReplace(std::string text, const std::string& from, const std::string& to)
{
    const std::size_t at = text.find(from);
    Check(at != std::string::npos, "the text being edited contains the target");
    text.replace(at, from.size(), to);
    return text;
}
