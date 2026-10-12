#pragma once

#include "DayLoop.h"
#include "TimeContract.h"
#include "WorldState.h"

#include <array>
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
 * Week compression under the equivalence rule (ROADMAP M3-02; SYSTEMS §2.5 rules 2 and 3,
 * §8 "granularity equivalence"; GAME_PLAN §5 "Equivalence rule"; SYSTEMS_CATALOGUE
 * "Calendar and time granularity"; TimeContract.h, the helper this builds on).
 *
 * A WEEK PLAN is plain data: for each of up to seven calendar days, the commitments the
 * player would make, in time order: an activity (committed, started, completed), arriving
 * at school, going to sleep for so long, or spending time (waiting, resting, walking).
 * Times are minutes of the calendar day, 0 to 1439, as DayPlanItem's are. Time between one
 * commitment and the next is spent awake (a Wait pass). Day 0 is the calendar day the
 * world's clock is in.
 *
 * TWO WAYS TO RESOLVE THE SAME PLAN, both through ApplyDayCommand (nothing is
 * re-implemented) and both keeping the world clock in step through the reducer
 * (WorldSim, so scheduled events and Gate onsets fire as time passes):
 *   CompressWeek         resolves the span between commitments in the fewest commands
 *                        (one Pass of up to a day); firsts and uncovered choices stop it.
 *   PlayWeekStepByStep   the reference: the same commitments with the span spent in small
 *                        Pass commands (WeekStepChunkMinutes by default), as scene-by-scene
 *                        play would; nothing stops it, the player is there.
 *
 * THE EQUIVALENCE RULE, as built. Both produce the same persistent state: every section of
 * the world, field by field (StateDiff.h), and the same ledger (below), EXCEPT the
 * quantities the time contract excludes (TimeEquivalenceExcluded): the number of resolution
 * steps, events consumed, unused progress, the receipt text, real time spent, and the
 * command count. The command count is WorldState::Seq (the reducer's accepted commands) and
 * DayLoopState::Seq (the day loop's), named in WeekEquivalenceIgnoredPaths; everything else,
 * the opportunity receipts and the attendance record included, must match exactly.
 * CompareWeekResults says what differs. BuildWeekTimePlan gives the same week as a TimePlan
 * so the time-contract helper (CompareGranularities, ResolvePlan) can resolve it as scenes,
 * days, a week and a span, and CompareWeekToTimeLedger ties what the day loop did to what
 * the contract says the week costs.
 *
 * DISCLOSURE (rule 3). Compression decides nothing the player has not ordered:
 *   - an entry marked DecidedByOrder was chosen by a standing order; it is counted in the
 *     ledger (decided_by_order) and listed in WeekResult::Disclosures with its order id,
 *     and RenderWeekReceipt prints it. Resolving the week step by step discloses the same list.
 *   - an entry marked FirstOfKind (the first time the player does a kind of thing) never
 *     auto-resolves: compression stops before it (WeekStop::FirstNeedsPlay).
 *   - an entry marked NeedsChoice (a consequential choice no standing order covers) stops
 *     compression before it and is the week's ONE interrupt (WeekMaxInterrupts). When the
 *     week has already used its interrupt (WeekPlan::InterruptsUsed), the stop is
 *     WeekStop::InterruptBudgetSpent and is not counted again: the day is a scene day.
 *   A stopped result is a success. The world is as it is after the last entry before the stop,
 *   exactly what playing those entries step by step gives (PlayWeekStepByStep with that limit),
 *   and EntriesResolved says how many were done.
 *
 * REFUSAL. A plan that does not validate, or a command the day loop refuses, returns the
 * error with the world exactly as it was given. Nothing is half-applied.
 *
 * CHOSEN (the docs leave them open): the Max* constants below; the Pass chunk sizes; that a
 * gap between commitments is spent awake; that the morning plan (PlanDay) is issued at the
 * week's first commitment, at each calendar day's first, and at the first after waking, and
 * lists the day's activity entries from there on; that an empty day issues nothing; that a first or an uncovered choice stops the whole compression (the
 * caller plays that entry and compresses the rest); that the week's ledger counts a gap
 * longer than a day as several units of at most a day.
 */

constexpr std::int32_t WeekMaxDays = 7;
/** Entries one day may hold (twice the day plan's item limit, since waits and sleep are entries too). */
constexpr std::size_t WeekMaxEntriesPerDay = 2 * DayLoopMaxPlanItems;
/** SYSTEMS_CATALOGUE: InterruptsUsed is at most 1 a week. */
constexpr std::int32_t WeekMaxInterrupts = 1;
/** The step-by-step reference spends time in quarter hours, as the time contract's scene granularity does. */
constexpr std::int64_t WeekStepChunkMinutes = TimeSceneChunkMinutes;
/** Compression spends a span in one Pass of up to a day, the most a Pass can take. */
constexpr std::int64_t WeekCompressedChunkMinutes = DayLoopMaxPassMinutes;

/** The paths of persistent state that count commands, not outcomes: the "command_count" exclusion of TimeEquivalenceExcluded. */
inline constexpr std::array<std::string_view, 2> WeekEquivalenceIgnoredPaths{"meta.seq", "dayloop.core.state.seq"};

enum class WeekEntryKind : std::uint8_t
{
    /** Commit, start and complete an activity. */
    Activity = 0,
    /** Arrive at school (DayCommandKind::ArriveSchool); takes no time itself. */
    School,
    /** Go to sleep and wake after SleepMinutes. */
    Sleep,
    /** Spend Minutes waiting, resting or walking. */
    Spend
};

/** "activity", "school", "sleep", "spend". */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view WeekEntryKindName(WeekEntryKind kind) noexcept;

struct WeekEntry
{
    WeekEntryKind Kind = WeekEntryKind::Activity;
    /** Minute of the calendar day the commitment begins, 0 to 1439. */
    std::int32_t StartMinute = 0;

    /** Activity. */
    std::string ActivityId;
    /** Activity: the kind of place the player is in, which must suit the activity ("home", "classroom", "dining"). */
    std::string PlaceKind;
    /** Activity: the alternatives the player was shown (kept by the receipt). */
    std::vector<ReceiptCandidate> Alternatives;
    /** School: the minute of the day the student leaves, or AttendanceStaysUntilDismissal. */
    std::int32_t DepartureMinute = AttendanceStaysUntilDismissal;
    /** Sleep: minutes slept, 1 to 1440. Spend: minutes spent, 1 to 1440. */
    std::int32_t Minutes = 0;
    /** Spend: how the time is spent. */
    DayPassKind Pass = DayPassKind::Wait;

    /** The first time the player does this kind of thing: never auto-resolved. */
    bool FirstOfKind = false;
    /** A consequential choice no standing order covers: compression stops before it. */
    bool NeedsChoice = false;
    /** A standing order chose this entry: disclosed on the receipt and counted. Needs OrderId. */
    bool DecidedByOrder = false;
    /** The standing order's id (a token such as "order.study_after_school"). */
    std::string OrderId;
};

struct WeekDayPlan
{
    std::vector<WeekEntry> Entries;
};

struct WeekPlan
{
    /** One to WeekMaxDays days; day 0 is the calendar day the world is in. */
    std::vector<WeekDayPlan> Days;
    /** Keys the per-day draws of the time-contract view (BuildWeekTimePlan). */
    std::uint64_t Seed = 0;
    /** Interrupts this week has already used (0 or 1). */
    std::int32_t InterruptsUsed = 0;
};

enum class WeekError : std::uint8_t
{
    None = 0,
    /** The world's day is not in step with its clock, or the day is not valid. */
    InvalidWorld,
    /** The player is asleep, an activity runs or a commitment is open: finish that first. */
    WorldBusy,
    /** The catalogues the week needs (activities) are not supplied. */
    MissingCatalogue,
    NoDays,
    TooManyDays,
    TooManyEntries,
    UnknownActivity,
    WrongPlace,
    /** An entry's start minute is outside the day. */
    BadMinute,
    /** An entry's other fields are out of range or malformed (minutes, ids, alternatives). */
    BadEntry,
    /** An entry starts before the previous one ends, or before now. */
    Overlap,
    BadFlags,
    BadInterruptCount,
    NoSchool,
    /** The time contract rejects the week (a mission or compressed activity is not routine, an unsound declaration). */
    NotRoutine,
    BeyondCalendar,
    /** The day loop refused a command; Detail says which and why. */
    DayCommandRefused,
    /** The reducer refused to move the world clock; Detail says why. */
    ClockRefused
};

/** A stable code such as "week.overlap". Empty for None. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view WeekErrorCode(WeekError error) noexcept;

/** Why a plan or a run was refused, and where. */
struct WeekPlanIssue
{
    WeekError Error = WeekError::None;
    /** The day and entry the problem is in; -1 when it is the plan as a whole. */
    std::int32_t Day = -1;
    std::int32_t Entry = -1;
    std::string Detail;

    [[nodiscard]] bool Ok() const noexcept { return Error == WeekError::None; }
};

/**
 * Checks the plan against the world and the catalogues without running it: the world is in step
 * and idle, the days and entries are in bounds and in time order without overlap, every activity
 * exists and suits its place, school is available for School entries, the flags agree, and the
 * time contract accepts the week as a routine plan (ValidatePlan). Pure.
 */
[[nodiscard]] PERFECTWORLDCORE_API WeekPlanIssue ValidateWeekPlan(const WorldState& world, const WeekPlan& plan,
                                                                  const DayLoopCatalogues& catalogues);

struct WeekTimePlanResult
{
    WeekPlanIssue Issue;
    TimePlan Plan;
    [[nodiscard]] bool Ok() const noexcept { return Issue.Ok(); }
};

/**
 * The week as a time-contract TimePlan: each activity, sleep, spend and gap as a declared
 * interaction (an activity's own mode, interrupt policy and duration; resolve on_complete;
 * waits and gaps are free_roam; sleep is a scene), at its start in minutes from the world's
 * clock, over a span ending where the last of them ends. Only the first `entryLimit` entries
 * (counted day by day, from 0) are included; a negative limit means all of them.
 * Interactions the player will play (FirstOfKind) and standing-order decisions carry over.
 * Arriving at school takes no time and is not an interaction the contract charges, so it is not
 * in the plan (and the helper would skip a zero-length item that ends the span).
 */
[[nodiscard]] PERFECTWORLDCORE_API WeekTimePlanResult BuildWeekTimePlan(const WorldState& world, const WeekPlan& plan,
                                                                       const DayLoopCatalogues& catalogues,
                                                                       std::int32_t entryLimit = -1);

enum class WeekStop : std::uint8_t
{
    None = 0,
    /** Compression stopped before a first of its kind. */
    FirstNeedsPlay,
    /** Compression stopped before a consequential choice no order covers: the week's interrupt. */
    ChoiceNeeded,
    /** As ChoiceNeeded, but the week's interrupt was already used; the day is a scene day. */
    InterruptBudgetSpent
};

/** "none", "first_needs_play", "choice_needed", "interrupt_budget_spent". */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view WeekStopName(WeekStop stop) noexcept;

/** What resolving the week cost and earned, from what the day loop actually did. Compared across granularities. */
struct WeekLedger
{
    GameInstant StartInstant = 0;
    GameInstant EndInstant = 0;
    /** Game minutes charged, by the mode of the interaction that spent them (activities by their own mode, waits and gaps free_roam, sleep scene). */
    std::array<std::int64_t, ClockModeCount> ChargedByMode{};
    std::int64_t TotalCharged = 0;
    /** Interactions completed that took time: activities, sleeps, spends and gaps (a gap over a day counts once per day). */
    std::int32_t Completed = 0;
    /** Entries a standing order chose. */
    std::int32_t DecidedByOrder = 0;
    /** Milli-XP the stats owner must apply, summed by skill, sorted by skill id. */
    std::vector<SkillCredit> Credits;
    /** Attribute evidence, summed by attribute, in attribute order. */
    std::vector<EvidenceCredit> Evidence;

    friend bool operator==(const WeekLedger&, const WeekLedger&) = default;
};

/** One choice made on the player's behalf. */
struct WeekDisclosure
{
    std::int32_t Day = 0;
    std::int32_t Entry = 0;
    WeekEntryKind Kind = WeekEntryKind::Activity;
    /** The activity id, or the kind's name for school, sleep and spend. */
    std::string What;
    std::string OrderId;
    /** The instant the entry began. */
    GameInstant At = 0;

    friend bool operator==(const WeekDisclosure&, const WeekDisclosure&) = default;
};

struct WeekEntryOutcome
{
    std::int32_t Day = 0;
    std::int32_t Entry = 0;
    WeekEntryKind Kind = WeekEntryKind::Activity;
    std::string What;
    GameInstant StartedAt = 0;
    GameInstant EndedAt = 0;
    /** Activity: milli-XP credited to the stats owner. School: the attendance outcome code (see AttendanceOutcome), else 0. */
    std::int64_t Value = 0;

    friend bool operator==(const WeekEntryOutcome&, const WeekEntryOutcome&) = default;
};

struct WeekResult
{
    WeekPlanIssue Issue;
    /** The world after the week; the world as given when Issue is not Ok. */
    WorldState World;
    WeekStop Stop = WeekStop::None;
    /** The day and entry compression stopped before; -1 when it did not stop. */
    std::int32_t StopDay = -1;
    std::int32_t StopEntry = -1;
    /** Entries resolved, counted day by day. */
    std::int32_t EntriesResolved = 0;
    /** Interrupts the week has used after this result (the plan's count, plus one for a ChoiceNeeded stop). */
    std::int32_t InterruptsUsed = 0;
    WeekLedger Ledger;
    std::vector<WeekDisclosure> Disclosures;
    std::vector<WeekEntryOutcome> Outcomes;
    /** Day-loop commands applied and clock advances submitted. Excluded from equivalence ("command_count"). */
    std::uint64_t DayCommands = 0;
    std::uint64_t ClockAdvances = 0;

    [[nodiscard]] bool Ok() const noexcept { return Issue.Ok(); }
};

/** Resolves the week with compression: the fewest commands, stopping at firsts and uncovered choices. Pure. */
[[nodiscard]] PERFECTWORLDCORE_API WeekResult CompressWeek(const WorldState& world, const WeekPlan& plan,
                                                           const DayLoopCatalogues& catalogues);

struct WeekStepOptions
{
    /** Minutes in each small Pass; at least 1. */
    std::int64_t ChunkMinutes = WeekStepChunkMinutes;
    /** Resolve only the first this-many entries (counted day by day); negative means all. */
    std::int32_t EntryLimit = -1;
};

/** The reference: the same plan command by command at fine granularity. Never stops for firsts or choices. Pure. */
[[nodiscard]] PERFECTWORLDCORE_API WeekResult PlayWeekStepByStep(const WorldState& world, const WeekPlan& plan,
                                                                const DayLoopCatalogues& catalogues,
                                                                const WeekStepOptions& options = {});

struct WeekEquivalence
{
    /**
     * Empty when equivalent. Otherwise the quantities that differ: ledger names ("end_minute", "charged_by_mode",
     * "total_charged", "completed", "decided_by_order", "credits", "evidence"), "entries_resolved", "disclosures",
     * and "state <path>: <a> -> <b>" for each persistent field that differs.
     */
    std::vector<std::string> Differences;

    [[nodiscard]] bool Equivalent() const noexcept { return Differences.empty(); }
};

/** Compares two resolutions of the same plan on every quantity that must match, ignoring WeekEquivalenceIgnoredPaths. */
[[nodiscard]] PERFECTWORLDCORE_API WeekEquivalence CompareWeekResults(const WeekResult& a, const WeekResult& b);

/**
 * Ties the day loop to the time contract: the first of end_minute, charged_by_mode, total_charged,
 * completed or decided_by_order (TimeEquivalenceQuantities names) on which the week's ledger and
 * the time-contract ledger of the same plan disagree; empty when they agree.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view CompareWeekToTimeLedger(const WeekResult& result, const TimeLedger& ledger);

/** The weekly receipt as text: what was resolved, what was done on the player's behalf, and why it stopped. Stable. */
[[nodiscard]] PERFECTWORLDCORE_API std::string RenderWeekReceipt(const WeekResult& result);
} // namespace PerfectWorld
