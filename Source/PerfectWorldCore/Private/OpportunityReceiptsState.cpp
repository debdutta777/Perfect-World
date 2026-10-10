#include "OpportunityReceipts.h"

#include "OpportunityReceiptsInternal.h"

#include <map>
#include <set>

// The invariants of ReceiptState (ROADMAP M2-11). Serialise refuses a state
// that fails this, and Deserialise refuses text that decodes to one, so a
// stored receipt can only ever be one that RecordCommitment and FoldReceipts
// could have produced.
namespace PerfectWorld
{
using namespace ReceiptInternal;

namespace
{
bool ReceiptEntryValid(const ReceiptEntry& entry, std::uint64_t previousSeq, GameInstant previousDecided,
                       const ReceiptState& state)
{
    ReceiptCommitment commitment;
    commitment.ActivityId = entry.ActivityId;
    commitment.DecidedAt = entry.DecidedAt;
    commitment.Start = entry.Start;
    commitment.DurationMinutes = entry.DurationMinutes;
    commitment.Forecast = entry.Forecast;
    if (!IsReceiptCommitmentValid(commitment) || entry.Seq <= previousSeq || entry.Seq >= state.NextSeq ||
        entry.DecidedAt < previousDecided || entry.DecidedAt > state.LastDecidedAt ||
        ReceiptDayOf(entry.Start) < state.FoldedBeforeDay)
        return false;
    if (entry.Displaced.size() > static_cast<std::size_t>(ReceiptMaxDisplacedPerCommitment))
        return false;

    std::set<std::string> ids;
    const ReceiptDisplaced* previous = nullptr;
    for (const ReceiptDisplaced& line : entry.Displaced)
    {
        const std::int64_t overlap =
            ReceiptOverlapMinutes(entry.Start, entry.DurationMinutes, line.Start, line.DurationMinutes);
        if (!IsReceiptIdWellFormed(line.Id) || !IsReceiptIdWellFormed(line.Source) || line.Id == entry.ActivityId ||
            line.KnownAt < 0 || line.KnownAt > entry.DecidedAt || !ReceiptWindowValid(line.Start, line.DurationMinutes) ||
            !ReceiptForecastValid(line.Forecast) || overlap <= 0 || overlap != line.OverlapMinutes ||
            !ids.insert(line.Id).second)
            return false;
        if (previous != nullptr && !ReceiptRankLess(*previous, line))
            return false;
        previous = &line;
    }

    if (entry.OmittedCount < 0 || entry.OmittedOverlapMinutes < 0 || entry.OmittedLearningMilliXp < 0)
        return false;
    if (entry.OmittedCount == 0)
        return entry.OmittedOverlapMinutes == 0 && entry.OmittedLearningMilliXp == 0;
    // Alternatives are only omitted beyond the cap, and each omitted one overlapped by at least a minute.
    return entry.Displaced.size() == static_cast<std::size_t>(ReceiptMaxDisplacedPerCommitment) &&
           entry.OmittedOverlapMinutes >= entry.OmittedCount &&
           entry.OmittedOverlapMinutes <= static_cast<std::int64_t>(entry.OmittedCount) * ReceiptMaxDurationMinutes &&
           entry.OmittedLearningMilliXp <= static_cast<std::int64_t>(entry.OmittedCount) * ReceiptMaxForecastMilli;
}

/** A folded or overflow total: non-negative, at least one commitment, and each minute figure consistent with its count. */
bool ReceiptSummaryValid(const ReceiptTotals& totals)
{
    return ReceiptTotalsValid(totals) && totals.Commitments >= 1 && totals.CommittedMinutes >= totals.Commitments &&
           totals.DisplacedOverlapMinutes >= totals.Displaced;
}
} // namespace

bool IsReceiptStateValid(const ReceiptState& state)
{
    if (state.NextSeq < 1 || state.LastDecidedAt < 0 || state.LastDecidedAt > ReceiptMaxInstant ||
        state.FoldedBeforeDay < 0)
        return false;
    if (state.FoldedBeforeDay != 0 && !ReceiptDayIsMonday(state.FoldedBeforeDay))
        return false;
    if (state.Entries.size() > ReceiptMaxEntries || state.Overflow.size() > ReceiptMaxOverflowDays ||
        state.Weeks.size() > ReceiptMaxWeeks)
        return false;

    std::map<std::int64_t, std::int32_t> perDay;
    std::uint64_t previousSeq = 0;
    GameInstant previousDecided = 0;
    for (const ReceiptEntry& entry : state.Entries)
    {
        if (!ReceiptEntryValid(entry, previousSeq, previousDecided, state))
            return false;
        previousSeq = entry.Seq;
        previousDecided = entry.DecidedAt;
        if (++perDay[ReceiptDayOf(entry.Start)] > ReceiptMaxEntriesPerDay)
            return false;
    }

    bool first = true;
    std::int64_t previousDay = 0;
    for (const ReceiptOverflow& overflow : state.Overflow)
    {
        if ((!first && overflow.Day <= previousDay) || overflow.Day < state.FoldedBeforeDay ||
            !ReceiptSummaryValid(overflow.Totals))
            return false;
        // An overflow exists only for a day whose entries are full.
        const auto found = perDay.find(overflow.Day);
        if (found == perDay.end() || found->second != ReceiptMaxEntriesPerDay)
            return false;
        first = false;
        previousDay = overflow.Day;
    }

    first = true;
    for (const ReceiptWeek& week : state.Weeks)
    {
        if ((!first && week.StartDay <= previousDay) || week.StartDay < 0 || week.StartDay + 7 > state.FoldedBeforeDay ||
            !ReceiptDayIsMonday(week.StartDay) || !ReceiptSummaryValid(week.Totals))
            return false;
        first = false;
        previousDay = week.StartDay;
    }
    return true;
}
} // namespace PerfectWorld
