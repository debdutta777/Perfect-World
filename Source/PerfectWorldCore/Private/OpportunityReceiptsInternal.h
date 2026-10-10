#pragma once

#include "OpportunityReceipts.h"

#include <algorithm>
#include <cstdint>
#include <string_view>

// Helpers shared by the opportunity receipt translation units (decision and
// recording, state validation, text). Not part of the public surface.
namespace PerfectWorld::ReceiptInternal
{
/** Minutes shared by [startA, startA + durationA) and [startB, startB + durationB); 0 when disjoint. */
inline std::int64_t ReceiptOverlapMinutes(GameInstant startA, std::int64_t durationA, GameInstant startB,
                                          std::int64_t durationB)
{
    const std::int64_t begin = std::max(startA, startB);
    const std::int64_t end = std::min(startA + durationA, startB + durationB);
    return end > begin ? end - begin : 0;
}

inline bool ReceiptForecastValid(const ReceiptForecast& forecast)
{
    return forecast.LearningMilliXp >= 0 && forecast.LearningMilliXp <= ReceiptMaxForecastMilli &&
           forecast.ConditionCostMilli >= 0 && forecast.ConditionCostMilli <= ReceiptMaxForecastMilli;
}

inline bool ReceiptWindowValid(GameInstant start, std::int32_t duration)
{
    return start >= 0 && start <= ReceiptMaxInstant && duration >= 1 && duration <= ReceiptMaxDurationMinutes;
}

/**
 * The total rank order of displaced alternatives: forecast learning descending,
 * overlap descending, then ID and every remaining field ascending.
 */
inline bool ReceiptRankLess(const ReceiptDisplaced& a, const ReceiptDisplaced& b)
{
    if (a.Forecast.LearningMilliXp != b.Forecast.LearningMilliXp)
        return a.Forecast.LearningMilliXp > b.Forecast.LearningMilliXp;
    if (a.OverlapMinutes != b.OverlapMinutes)
        return a.OverlapMinutes > b.OverlapMinutes;
    if (a.Id != b.Id)
        return a.Id < b.Id;
    if (a.Start != b.Start)
        return a.Start < b.Start;
    if (a.DurationMinutes != b.DurationMinutes)
        return a.DurationMinutes < b.DurationMinutes;
    if (a.KnownAt != b.KnownAt)
        return a.KnownAt < b.KnownAt;
    if (a.Source != b.Source)
        return a.Source < b.Source;
    return a.Forecast.ConditionCostMilli < b.Forecast.ConditionCostMilli;
}

inline bool ReceiptTotalsValid(const ReceiptTotals& totals)
{
    return totals.Commitments >= 0 && totals.CommittedMinutes >= 0 && totals.LearningMilliXp >= 0 &&
           totals.ConditionCostMilli >= 0 && totals.Displaced >= 0 && totals.DisplacedOverlapMinutes >= 0 &&
           totals.DisplacedLearningMilliXp >= 0;
}

/** The instant a day number opens (inverse of GameClock::DayNumber at midnight). */
inline GameInstant ReceiptDayStart(std::int64_t day)
{
    const std::int64_t offset = GameClock::DayNumber(0) * MinutesPerDay - GameClock::StartOfDay(0);
    return day * MinutesPerDay - offset;
}

inline bool ReceiptDayIsMonday(std::int64_t day)
{
    return GameClock::ToCalendar(ReceiptDayStart(day)).DayOfWeek == Weekday::Monday;
}
} // namespace PerfectWorld::ReceiptInternal
