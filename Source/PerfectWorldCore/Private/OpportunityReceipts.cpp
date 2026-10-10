#include "OpportunityReceipts.h"

#include "OpportunityReceiptsInternal.h"

#include <algorithm>
#include <set>

// Opportunity receipts (ROADMAP M2-11): deciding what a commitment displaced,
// recording it, the per-day query and the 120-day fold. The rules are in the
// header; state validation is OpportunityReceiptsState.cpp and the text form is
// OpportunityReceiptsText.cpp.
namespace PerfectWorld
{
using namespace ReceiptInternal;

namespace
{
/** Finds (or inserts, keeping the order) the weekly record that opens on `startDay`. */
ReceiptWeek& ReceiptWeekFor(ReceiptState& state, std::int64_t startDay)
{
    auto it = std::lower_bound(state.Weeks.begin(), state.Weeks.end(), startDay,
                               [](const ReceiptWeek& week, std::int64_t key) { return week.StartDay < key; });
    if (it == state.Weeks.end() || it->StartDay != startDay)
    {
        ReceiptWeek week;
        week.StartDay = startDay;
        it = state.Weeks.insert(it, week);
    }
    return *it;
}

ReceiptEntry ReceiptBuildEntry(const ReceiptCommitment& commitment, const std::vector<ReceiptDisplaced>& ranked)
{
    ReceiptEntry entry;
    entry.ActivityId = commitment.ActivityId;
    entry.DecidedAt = commitment.DecidedAt;
    entry.Start = commitment.Start;
    entry.DurationMinutes = commitment.DurationMinutes;
    entry.Forecast = commitment.Forecast;
    const std::size_t kept = std::min<std::size_t>(ranked.size(), ReceiptMaxDisplacedPerCommitment);
    entry.Displaced.assign(ranked.begin(), ranked.begin() + static_cast<std::ptrdiff_t>(kept));
    for (std::size_t i = kept; i < ranked.size(); ++i)
    {
        ++entry.OmittedCount;
        entry.OmittedOverlapMinutes += ranked[i].OverlapMinutes;
        entry.OmittedLearningMilliXp += ranked[i].Forecast.LearningMilliXp;
    }
    return entry;
}
} // namespace

std::string_view ReceiptErrorCode(ReceiptError error) noexcept
{
    switch (error)
    {
    case ReceiptError::None:
        return {};
    case ReceiptError::InvalidCommitment:
        return "receipt.invalid_commitment";
    case ReceiptError::InvalidCandidate:
        return "receipt.invalid_candidate";
    case ReceiptError::TooManyCandidates:
        return "receipt.too_many_candidates";
    case ReceiptError::OutOfOrder:
        return "receipt.out_of_order";
    case ReceiptError::AlreadyFolded:
        return "receipt.already_folded";
    case ReceiptError::Full:
        return "receipt.full";
    }
    return {};
}

bool IsReceiptIdWellFormed(std::string_view id) noexcept
{
    if (id.empty() || id.size() > ReceiptMaxIdLength)
        return false;
    for (const char c : id)
    {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
        if (!ok)
            return false;
    }
    return true;
}

bool IsReceiptCommitmentValid(const ReceiptCommitment& commitment) noexcept
{
    return IsReceiptIdWellFormed(commitment.ActivityId) && commitment.DecidedAt >= 0 &&
           commitment.Start >= commitment.DecidedAt &&
           ReceiptWindowValid(commitment.Start, commitment.DurationMinutes) &&
           ReceiptForecastValid(commitment.Forecast);
}

bool IsReceiptCandidateValid(const ReceiptCandidate& candidate) noexcept
{
    return IsReceiptIdWellFormed(candidate.Id) && IsReceiptIdWellFormed(candidate.Source) && candidate.KnownAt >= 0 &&
           candidate.KnownAt <= ReceiptMaxInstant && ReceiptWindowValid(candidate.Start, candidate.DurationMinutes) &&
           ReceiptForecastValid(candidate.Forecast);
}

std::int64_t ReceiptDayOf(GameInstant start) noexcept
{
    return GameClock::DayNumber(start);
}

std::int64_t ReceiptWeekStartDay(GameInstant instant) noexcept
{
    const int sinceMonday = (static_cast<int>(GameClock::ToCalendar(instant).DayOfWeek) + 6) % 7;
    return GameClock::DayNumber(instant) - sinceMonday;
}

std::vector<ReceiptDisplaced> DecideDisplaced(const ReceiptCommitment& commitment,
                                              const std::vector<ReceiptCandidate>& candidates)
{
    std::vector<ReceiptDisplaced> found;
    if (!IsReceiptCommitmentValid(commitment))
        return found;
    for (const ReceiptCandidate& candidate : candidates)
    {
        if (!IsReceiptCandidateValid(candidate) || !candidate.Available || candidate.KnownAt > commitment.DecidedAt ||
            candidate.Id == commitment.ActivityId)
            continue;
        const std::int64_t overlap = ReceiptOverlapMinutes(commitment.Start, commitment.DurationMinutes,
                                                           candidate.Start, candidate.DurationMinutes);
        if (overlap <= 0)
            continue;
        ReceiptDisplaced line;
        line.Id = candidate.Id;
        line.Source = candidate.Source;
        line.KnownAt = candidate.KnownAt;
        line.Start = candidate.Start;
        line.DurationMinutes = candidate.DurationMinutes;
        line.OverlapMinutes = static_cast<std::int32_t>(overlap);
        line.Forecast = candidate.Forecast;
        found.push_back(std::move(line));
    }
    std::sort(found.begin(), found.end(), ReceiptRankLess);
    // One alternative per ID: the best ranked wins, so the result does not depend on the order passed in.
    std::set<std::string> seen;
    std::vector<ReceiptDisplaced> unique;
    unique.reserve(found.size());
    for (ReceiptDisplaced& line : found)
    {
        if (seen.insert(line.Id).second)
            unique.push_back(std::move(line));
    }
    return unique;
}

ReceiptTotals ReceiptEntryTotals(const ReceiptEntry& entry)
{
    ReceiptTotals totals;
    totals.Commitments = 1;
    totals.CommittedMinutes = entry.DurationMinutes;
    totals.LearningMilliXp = entry.Forecast.LearningMilliXp;
    totals.ConditionCostMilli = entry.Forecast.ConditionCostMilli;
    totals.Displaced = static_cast<std::int64_t>(entry.Displaced.size()) + entry.OmittedCount;
    totals.DisplacedOverlapMinutes = entry.OmittedOverlapMinutes;
    totals.DisplacedLearningMilliXp = entry.OmittedLearningMilliXp;
    for (const ReceiptDisplaced& line : entry.Displaced)
    {
        totals.DisplacedOverlapMinutes += line.OverlapMinutes;
        totals.DisplacedLearningMilliXp += line.Forecast.LearningMilliXp;
    }
    return totals;
}

void AddReceiptTotals(ReceiptTotals& into, const ReceiptTotals& add)
{
    into.Commitments += add.Commitments;
    into.CommittedMinutes += add.CommittedMinutes;
    into.LearningMilliXp += add.LearningMilliXp;
    into.ConditionCostMilli += add.ConditionCostMilli;
    into.Displaced += add.Displaced;
    into.DisplacedOverlapMinutes += add.DisplacedOverlapMinutes;
    into.DisplacedLearningMilliXp += add.DisplacedLearningMilliXp;
}

ReceiptRecordResult RecordCommitment(ReceiptState& state, const ReceiptCommitment& commitment,
                                     const std::vector<ReceiptCandidate>& candidates)
{
    ReceiptRecordResult result;
    if (!IsReceiptCommitmentValid(commitment))
    {
        result.Error = ReceiptError::InvalidCommitment;
        return result;
    }
    if (candidates.size() > ReceiptMaxCandidates)
    {
        result.Error = ReceiptError::TooManyCandidates;
        return result;
    }
    for (const ReceiptCandidate& candidate : candidates)
    {
        if (!IsReceiptCandidateValid(candidate))
        {
            result.Error = ReceiptError::InvalidCandidate;
            return result;
        }
    }
    if (commitment.DecidedAt < state.LastDecidedAt)
    {
        result.Error = ReceiptError::OutOfOrder;
        return result;
    }
    const std::int64_t day = ReceiptDayOf(commitment.Start);
    if (day < state.FoldedBeforeDay)
    {
        result.Error = ReceiptError::AlreadyFolded;
        return result;
    }

    const std::vector<ReceiptDisplaced> ranked = DecideDisplaced(commitment, candidates);
    ReceiptEntry entry = ReceiptBuildEntry(commitment, ranked);
    result.DisplacedCount = static_cast<std::int32_t>(ranked.size());

    std::int32_t onDay = 0;
    for (const ReceiptEntry& existing : state.Entries)
    {
        if (ReceiptDayOf(existing.Start) == day)
            ++onDay;
    }
    if (onDay >= ReceiptMaxEntriesPerDay)
    {
        auto it = std::lower_bound(state.Overflow.begin(), state.Overflow.end(), day,
                                   [](const ReceiptOverflow& overflow, std::int64_t key) { return overflow.Day < key; });
        if (it == state.Overflow.end() || it->Day != day)
        {
            if (state.Overflow.size() >= ReceiptMaxOverflowDays)
            {
                result.Error = ReceiptError::Full;
                return result;
            }
            ReceiptOverflow created;
            created.Day = day;
            it = state.Overflow.insert(it, created);
        }
        AddReceiptTotals(it->Totals, ReceiptEntryTotals(entry));
        result.Summarised = true;
    }
    else
    {
        if (state.Entries.size() >= ReceiptMaxEntries)
        {
            result.Error = ReceiptError::Full;
            return result;
        }
        entry.Seq = state.NextSeq++;
        result.Seq = entry.Seq;
        state.Entries.push_back(std::move(entry));
    }
    state.LastDecidedAt = commitment.DecidedAt;
    return result;
}

ReceiptDay ReceiptDayQuery(const ReceiptState& state, std::int64_t day)
{
    ReceiptDay out;
    out.Day = day;
    if (day < state.FoldedBeforeDay)
    {
        out.Folded = true;
        return out;
    }
    for (const ReceiptEntry& entry : state.Entries)
    {
        if (ReceiptDayOf(entry.Start) != day)
            continue;
        out.Entries.push_back(entry);
        AddReceiptTotals(out.Totals, ReceiptEntryTotals(entry));
    }
    for (const ReceiptOverflow& overflow : state.Overflow)
    {
        if (overflow.Day == day)
        {
            out.OverflowTotals = overflow.Totals;
            AddReceiptTotals(out.Totals, overflow.Totals);
        }
    }
    return out;
}

const ReceiptWeek* FindReceiptWeek(const ReceiptState& state, std::int64_t startDay) noexcept
{
    for (const ReceiptWeek& week : state.Weeks)
    {
        if (week.StartDay == startDay)
            return &week;
    }
    return nullptr;
}

ReceiptTotals ReceiptGrandTotals(const ReceiptState& state)
{
    ReceiptTotals totals;
    for (const ReceiptEntry& entry : state.Entries)
        AddReceiptTotals(totals, ReceiptEntryTotals(entry));
    for (const ReceiptOverflow& overflow : state.Overflow)
        AddReceiptTotals(totals, overflow.Totals);
    for (const ReceiptWeek& week : state.Weeks)
        AddReceiptTotals(totals, week.Totals);
    return totals;
}

std::size_t FoldReceipts(ReceiptState& state, GameInstant now)
{
    if (!IsReceiptStateValid(state))
        return 0;
    const GameInstant horizon = now - ReceiptDailyRetentionDays * MinutesPerDay;
    if (horizon < 0)
        return 0;
    const std::int64_t cutoff = ReceiptWeekStartDay(horizon);
    if (cutoff <= state.FoldedBeforeDay)
        return 0;

    std::size_t folded = 0;
    std::vector<ReceiptEntry> kept;
    kept.reserve(state.Entries.size());
    for (ReceiptEntry& entry : state.Entries)
    {
        if (ReceiptDayOf(entry.Start) >= cutoff)
        {
            kept.push_back(std::move(entry));
            continue;
        }
        AddReceiptTotals(ReceiptWeekFor(state, ReceiptWeekStartDay(entry.Start)).Totals, ReceiptEntryTotals(entry));
        ++folded;
    }
    state.Entries = std::move(kept);

    std::vector<ReceiptOverflow> keptOverflow;
    for (const ReceiptOverflow& overflow : state.Overflow)
    {
        if (overflow.Day >= cutoff)
            keptOverflow.push_back(overflow);
        else
            AddReceiptTotals(ReceiptWeekFor(state, ReceiptWeekStartDay(ReceiptDayStart(overflow.Day))).Totals,
                             overflow.Totals);
    }
    state.Overflow = std::move(keptOverflow);
    state.FoldedBeforeDay = cutoff;
    return folded;
}
} // namespace PerfectWorld
