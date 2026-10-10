#pragma once

#include "GameClock.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * Opportunity receipts (ROADMAP M2-11; GAME_PLAN pillar 2 "Time is the
 * currency ... the game shows you what it displaced"; SYSTEMS_CATALOGUE
 * "Opportunity receipts": what a commitment displaced, only alternatives the
 * player knew about).
 *
 * A standalone, pure, integer-only module over a plain state (ReceiptState).
 * It reads no clock, owns no stats and is not yet a member of WorldState, the
 * reducer, the hash, Activities or the save. The integration step calls
 * RecordCommitment from the activity-begin command, passing the commitment's
 * own forecast (what the activity will credit and cost, from its definition and
 * ForecastPractice) and the alternatives the player was shown, as plain data.
 *
 * What a receipt is. One entry per commitment: the commitment (activity ID,
 * start, duration, the instant it was decided, its own forecast) and the
 * alternatives it displaced, each with the window, source and forecast values
 * it had when the decision was made. The entry is a snapshot. Nothing in this
 * module has an edit, remove or reorder call for an entry, and later knowledge
 * (a newly learned alternative, a changed forecast) is never applied to an
 * existing one: that alternative appears on the next receipt that it
 * displaces, if any. The only operations that change the state are
 * RecordCommitment (append) and FoldReceipts (the 120-day compaction below,
 * which removes detail but preserves every total).
 *
 * Which candidates are displaced (DecideDisplaced). A candidate is displaced by
 * a commitment exactly when ALL of:
 *   - it was known: KnownAt <= the commitment's DecidedAt ("only alternatives
 *     the player knew about"; SYSTEMS_CATALOGUE test: no line names an
 *     alternative whose KnownAt falls after the commitment);
 *   - it was available: the caller marked it Available at decision time;
 *   - its window overlaps the commitment's. Windows are half-open
 *     [Start, Start + Duration) in game minutes, so back to back windows do not
 *     overlap, and a partial overlap displaces (the overlap in minutes is
 *     recorded);
 *   - it is not the commitment's own activity (an activity is not its own
 *     alternative).
 * Candidates sharing an ID are one alternative: only the best ranked is kept.
 * The result is independent of the order the candidates were passed in.
 *
 * Ranking and the per-commitment cap. Displaced alternatives are ranked by
 * forecast learning (milli-XP) descending, then overlap minutes descending,
 * then ID, then every remaining field ascending, so the ranking is total. An
 * entry stores the first ReceiptMaxDisplacedPerCommitment; the rest are
 * summarised in the entry's Omitted* counters (count, overlap minutes,
 * forecast learning). Totals always include the omitted ones.
 *
 * The per-day bound and its summarisation rule. A commitment belongs to the
 * game day it STARTS on (GameClock::DayNumber of Start), even if it crosses
 * midnight. A day keeps at most ReceiptMaxEntriesPerDay full entries; a further
 * commitment on that day is not stored as an entry but added to the day's
 * overflow totals (commitment, minutes, forecasts and all of its displaced
 * alternatives, none of them lost from the sums). So the day's totals are exact
 * however many commitments the day held, the stored detail is bounded, and no
 * stored entry is ever rewritten. The overflow record is a running sum, not an
 * entry.
 *
 * Receipts are ordered by the order they were recorded (Seq, dense from 1 and
 * never reused). Recording requires DecidedAt not earlier than the last
 * recorded DecidedAt: decisions happen in time order.
 *
 * Retention (FoldReceipts, SYSTEMS_CATALOGUE "dailies fold into weeklies after
 * 120 days"). Days older than ReceiptDailyRetentionDays are replaced by weekly
 * totals, keyed by the Monday that opens the week and always folded whole
 * weeks at a time. Folding preserves the sum of all totals; it never changes
 * the days that are kept. Recording a commitment on a folded day is refused.
 *
 * Totals ("what did today cost me"): ReceiptDayQuery gives a day's entries and
 * its ReceiptTotals; because the totals are plain sums over the commitments, a
 * caller can reconcile them against the command log of the same day.
 *
 * Text form: SerialiseReceipts / DeserialiseReceipts, integers and tokens only,
 * canonical (the same state always gives the same text, and only that text
 * parses back). A truncated or edited text is refused, never half-read.
 */

/** A token such as an activity or alternative ID: 1 to 128 characters of a-z, 0-9, '_' and '.'. */
constexpr std::size_t ReceiptMaxIdLength = 128;
/** Full entries a single game day keeps; further commitments that day go to the day's overflow totals. */
constexpr std::int32_t ReceiptMaxEntriesPerDay = 24;
/** Displaced alternatives stored on one entry; the rest are summarised in the Omitted* counters. */
constexpr std::int32_t ReceiptMaxDisplacedPerCommitment = 8;
/** Candidates a single record call may be given. */
constexpr std::size_t ReceiptMaxCandidates = 256;
/** Days of full detail FoldReceipts keeps (SYSTEMS_CATALOGUE). */
constexpr std::int64_t ReceiptDailyRetentionDays = 120;
/** Hard bounds on the stored state; recording past them is refused until the caller folds. */
constexpr std::size_t ReceiptMaxEntries = 4096;
constexpr std::size_t ReceiptMaxOverflowDays = 4096;
constexpr std::size_t ReceiptMaxWeeks = 6000;
/** A window lasts 1 minute to 7 days. */
constexpr std::int32_t ReceiptMaxDurationMinutes = 7 * 24 * 60;
/** A forecast value (milli-XP or milli condition cost) is 0 to this. */
constexpr std::int32_t ReceiptMaxForecastMilli = 1'000'000;
/** An instant is 0 to about a hundred game years. */
constexpr GameInstant ReceiptMaxInstant = 100 * 366 * MinutesPerDay;
/** The text form's version header. */
inline constexpr std::string_view ReceiptTextHeader = "pw.receipts/1";

/** What an activity is forecast to credit and cost; plain data from the caller. */
struct ReceiptForecast
{
    /** Learning credit in milli-XP, as shown to the player (ForecastPractice). */
    std::int32_t LearningMilliXp = 0;
    /** Condition cost in milli of the caller's unit (energy, stress or a blend; the caller decides). */
    std::int32_t ConditionCostMilli = 0;

    friend bool operator==(const ReceiptForecast&, const ReceiptForecast&) = default;
};

/** What the player committed to, with the instant they decided. */
struct ReceiptCommitment
{
    std::string ActivityId;
    /** When the decision was made; alternatives known after this are not displaced by it. */
    GameInstant DecidedAt = 0;
    /** The window [Start, Start + DurationMinutes); Start >= DecidedAt. */
    GameInstant Start = 0;
    std::int32_t DurationMinutes = 0;
    ReceiptForecast Forecast;
};

/** An alternative the player was shown, as known when the decision was made. */
struct ReceiptCandidate
{
    std::string Id;
    /** Where the player learned of it ("source.noticeboard", "source.friend"); a token. */
    std::string Source;
    GameInstant KnownAt = 0;
    GameInstant Start = 0;
    std::int32_t DurationMinutes = 0;
    /** False when it could not have been taken at decision time (closed, blocked, unaffordable). */
    bool Available = true;
    ReceiptForecast Forecast;
};

/** A displaced alternative as the receipt recorded it. */
struct ReceiptDisplaced
{
    std::string Id;
    std::string Source;
    GameInstant KnownAt = 0;
    GameInstant Start = 0;
    std::int32_t DurationMinutes = 0;
    /** Minutes of this alternative's window the commitment took; 1 to the shorter of the two durations. */
    std::int32_t OverlapMinutes = 0;
    ReceiptForecast Forecast;

    friend bool operator==(const ReceiptDisplaced&, const ReceiptDisplaced&) = default;
};

/** One receipt. Never edited after it is recorded. */
struct ReceiptEntry
{
    /** Dense from 1, assigned in recording order, never reused. */
    std::uint64_t Seq = 0;
    std::string ActivityId;
    GameInstant DecidedAt = 0;
    GameInstant Start = 0;
    std::int32_t DurationMinutes = 0;
    ReceiptForecast Forecast;
    /** In rank order (see the header), at most ReceiptMaxDisplacedPerCommitment. */
    std::vector<ReceiptDisplaced> Displaced;
    /** Displaced alternatives beyond the cap: how many, their summed overlap, their summed forecast learning. */
    std::int32_t OmittedCount = 0;
    std::int64_t OmittedOverlapMinutes = 0;
    std::int64_t OmittedLearningMilliXp = 0;

    friend bool operator==(const ReceiptEntry&, const ReceiptEntry&) = default;
};

/** Plain sums over commitments; the unit of every day, overflow and weekly figure. */
struct ReceiptTotals
{
    std::int64_t Commitments = 0;
    std::int64_t CommittedMinutes = 0;
    std::int64_t LearningMilliXp = 0;
    std::int64_t ConditionCostMilli = 0;
    /** Displaced alternatives, counting each displacement (one alternative displaced twice counts twice). */
    std::int64_t Displaced = 0;
    std::int64_t DisplacedOverlapMinutes = 0;
    std::int64_t DisplacedLearningMilliXp = 0;

    friend bool operator==(const ReceiptTotals&, const ReceiptTotals&) = default;
};

/** The running sum for commitments beyond a day's entry cap. */
struct ReceiptOverflow
{
    std::int64_t Day = 0;
    ReceiptTotals Totals;

    friend bool operator==(const ReceiptOverflow&, const ReceiptOverflow&) = default;
};

/** Everything a folded week kept. */
struct ReceiptWeek
{
    /** The day number of the Monday that opens the week. */
    std::int64_t StartDay = 0;
    ReceiptTotals Totals;

    friend bool operator==(const ReceiptWeek&, const ReceiptWeek&) = default;
};

struct ReceiptState
{
    /** The Seq the next entry gets; 1 for a new state. */
    std::uint64_t NextSeq = 1;
    /** The latest DecidedAt recorded (including overflowed commitments); 0 for a new state. */
    GameInstant LastDecidedAt = 0;
    /** Days before this are folded into Weeks. 0 until the first fold; a Monday after it. */
    std::int64_t FoldedBeforeDay = 0;
    /** In Seq order. */
    std::vector<ReceiptEntry> Entries;
    /** Sorted by Day. */
    std::vector<ReceiptOverflow> Overflow;
    /** Sorted by StartDay. */
    std::vector<ReceiptWeek> Weeks;

    friend bool operator==(const ReceiptState&, const ReceiptState&) = default;
};

enum class ReceiptError : std::uint8_t
{
    None = 0,
    InvalidCommitment,
    InvalidCandidate,
    TooManyCandidates,
    OutOfOrder,
    AlreadyFolded,
    Full
};

/** The stable rejection code, for example "receipt.out_of_order". Empty for None. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view ReceiptErrorCode(ReceiptError error) noexcept;

[[nodiscard]] PERFECTWORLDCORE_API bool IsReceiptIdWellFormed(std::string_view id) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API bool IsReceiptCommitmentValid(const ReceiptCommitment& commitment) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API bool IsReceiptCandidateValid(const ReceiptCandidate& candidate) noexcept;
/** True when the state obeys every invariant above (order, bounds, rank order, overlap arithmetic, folded weeks). */
[[nodiscard]] PERFECTWORLDCORE_API bool IsReceiptStateValid(const ReceiptState& state);

/** The game day a window starting at `start` belongs to. */
[[nodiscard]] PERFECTWORLDCORE_API std::int64_t ReceiptDayOf(GameInstant start) noexcept;
/** The day number of the Monday that opens the week containing `instant`. */
[[nodiscard]] PERFECTWORLDCORE_API std::int64_t ReceiptWeekStartDay(GameInstant instant) noexcept;

/**
 * Which candidates the commitment displaced, fully ranked and uncapped, with
 * the rules in the header. An ill-formed candidate (IsReceiptCandidateValid
 * false) is skipped; RecordCommitment refuses it instead. An ill-formed
 * commitment displaces nothing. Pure and deterministic.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<ReceiptDisplaced> DecideDisplaced(
    const ReceiptCommitment& commitment, const std::vector<ReceiptCandidate>& candidates);

struct ReceiptRecordResult
{
    ReceiptError Error = ReceiptError::None;
    /** The new entry's Seq, or 0 when the commitment went to the day's overflow totals. */
    std::uint64_t Seq = 0;
    /** True when the day was full and the commitment was summarised into the overflow totals. */
    bool Summarised = false;
    /** All displaced alternatives, including any beyond the stored cap. */
    std::int32_t DisplacedCount = 0;

    [[nodiscard]] bool Ok() const noexcept { return Error == ReceiptError::None; }
};

/**
 * Record a commitment and the alternatives it displaced. Decides with
 * DecideDisplaced, stores the entry (or adds to the day's overflow), and
 * returns what happened. A refusal changes nothing.
 */
PERFECTWORLDCORE_API ReceiptRecordResult RecordCommitment(ReceiptState& state, const ReceiptCommitment& commitment,
                                                          const std::vector<ReceiptCandidate>& candidates);

/** The totals one entry contributes: its commitment and every displacement, omitted ones included. */
[[nodiscard]] PERFECTWORLDCORE_API ReceiptTotals ReceiptEntryTotals(const ReceiptEntry& entry);
/** Adds `add` into `into`. */
PERFECTWORLDCORE_API void AddReceiptTotals(ReceiptTotals& into, const ReceiptTotals& add);

struct ReceiptDay
{
    std::int64_t Day = 0;
    /** True when the day is older than FoldedBeforeDay: its detail is gone and only its week remains. */
    bool Folded = false;
    /** The day's entries in Seq order (copies). */
    std::vector<ReceiptEntry> Entries;
    /** The commitments beyond the entry cap, summed; zero when the day never filled. */
    ReceiptTotals OverflowTotals;
    /** Entries and overflow together: what the day cost. Zero for a folded or empty day. */
    ReceiptTotals Totals;
};

/** "What did today cost me." */
[[nodiscard]] PERFECTWORLDCORE_API ReceiptDay ReceiptDayQuery(const ReceiptState& state, std::int64_t day);
/** The folded week that opens on `startDay`, or null. */
[[nodiscard]] PERFECTWORLDCORE_API const ReceiptWeek* FindReceiptWeek(const ReceiptState& state,
                                                                      std::int64_t startDay) noexcept;
/** The sum of every day and every week the state holds: constant across FoldReceipts. */
[[nodiscard]] PERFECTWORLDCORE_API ReceiptTotals ReceiptGrandTotals(const ReceiptState& state);

/**
 * Fold whole weeks older than ReceiptDailyRetentionDays before `now` into
 * weekly totals. Returns how many entries were folded. Idempotent for a given
 * `now`, never folds backwards, and keeps every total. A state that is
 * invalid is left alone.
 */
PERFECTWORLDCORE_API std::size_t FoldReceipts(ReceiptState& state, GameInstant now);

/**
 * The state as text, integers and tokens only, one record per line:
 *   pw.receipts/1
 *   state next_seq=N last_decided=T folded_before=D
 *   entries=E
 *   e seq=.. activity=.. decided=.. start=.. minutes=.. learning=.. condition=.. omitted=.. omitted_overlap=.. omitted_learning=.. displaced=M
 *   d id=.. source=.. known=.. start=.. minutes=.. overlap=.. learning=.. condition=..     (M lines follow each entry)
 *   overflow=O
 *   o day=.. commitments=.. committed_minutes=.. learning=.. condition=.. displaced=.. overlap=.. displaced_learning=..
 *   weeks=W
 *   w start=.. commitments=.. (the same seven totals as an overflow line)
 *   end
 * Empty for an invalid state.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseReceipts(const ReceiptState& state);

struct ReceiptParse
{
    /** Empty on success; otherwise why the text was refused. */
    std::string Error;
    ReceiptState State;

    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/** Reads SerialiseReceipts' text. Refuses truncation, extra text, non-canonical numbers and any invalid state. */
[[nodiscard]] PERFECTWORLDCORE_API ReceiptParse DeserialiseReceipts(std::string_view text);
} // namespace PerfectWorld
