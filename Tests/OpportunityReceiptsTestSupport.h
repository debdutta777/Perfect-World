// Shared helpers for the ROADMAP M2-11 opportunity receipt tests.
#pragma once

#include "OpportunityReceipts.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace ReceiptTestSupport
{
using namespace PerfectWorld;

inline int Checks = 0;

inline void Check(bool condition, const char* message)
{
    ++Checks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

// A small deterministic generator for the tests (xorshift64*).
struct Rng
{
    std::uint64_t S;
    std::uint64_t Next()
    {
        S ^= S >> 12;
        S ^= S << 25;
        S ^= S >> 27;
        return (S * 2685821657736338717ULL) >> 16;
    }
    std::int64_t Pick(std::int64_t low, std::int64_t high) { return low + static_cast<std::int64_t>(Next() % static_cast<std::uint64_t>(high - low + 1)); }
};

// The instant midnight opens on day `n` after the first full day of the campaign.
inline GameInstant Mid(int n)
{
    return GameClock::StartOfDay(0) + (n + 1) * MinutesPerDay;
}

inline bool IsMondayDay(std::int64_t day)
{
    const GameInstant offset = GameClock::DayNumber(0) * MinutesPerDay - GameClock::StartOfDay(0);
    return GameClock::ToCalendar(day * MinutesPerDay - offset).DayOfWeek == Weekday::Monday;
}

inline ReceiptCommitment Commit(const char* id, GameInstant decided, GameInstant start, std::int32_t minutes, std::int32_t learning = 0,
                         std::int32_t condition = 0)
{
    ReceiptCommitment c;
    c.ActivityId = id;
    c.DecidedAt = decided;
    c.Start = start;
    c.DurationMinutes = minutes;
    c.Forecast = {learning, condition};
    return c;
}

inline ReceiptCandidate Cand(const char* id, GameInstant known, GameInstant start, std::int32_t minutes, bool available = true,
                      std::int32_t learning = 0, const char* source = "source.board")
{
    ReceiptCandidate c;
    c.Id = id;
    c.Source = source;
    c.KnownAt = known;
    c.Start = start;
    c.DurationMinutes = minutes;
    c.Available = available;
    c.Forecast = {learning, learning / 2};
    return c;
}

inline std::vector<std::string> Ids(const std::vector<ReceiptDisplaced>& lines)
{
    std::vector<std::string> ids;
    for (const ReceiptDisplaced& line : lines)
        ids.push_back(line.Id);
    return ids;
}
} // namespace ReceiptTestSupport

using namespace ReceiptTestSupport;
