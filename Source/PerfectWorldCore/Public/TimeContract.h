#pragma once

#include "ContentRegistry.h"
#include "GameClock.h"

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
 * The time contract (ROADMAP M3-01; GAME_PLAN §5; SYSTEMS §2.5; SYSTEMS_CATALOGUE
 * "Calendar and time granularity"). The portable half: pure, integer-only rules.
 * The Unreal half (the free-roam ticker, the pause overlays reading this state)
 * is the PIE rung and is not here. This module reads no clock and advances none;
 * it says what each mode does, whether a declaration is sound, how a declared
 * interaction resolves under interruptions, and how a plan resolved at
 * different granularities must agree (the helper M3-02 builds on).
 *
 * THE SIX MODES (GAME_PLAN §5) plus the seventh the SYSTEMS table lists:
 *   free_roam            world clock RUNS at 1 game minute per real second; walking, looking, waiting.
 *   conversation         world PAUSED; charged nothing, or the declared cost (per exchange, at talk.close).
 *   scene                world PAUSED while played; the declared cost, charged at the resolution boundary.
 *   ui_paused            PAUSED; phone, planning, receipts, news, menus, reading. Never charged.
 *   accessibility_pause  PAUSED; pause, text speed, subtitles. Never charged; works over every mode.
 *   mission              world PAUSED, the mission clock runs; charged at exit: fixed minutes, or
 *                        mission minutes x rate, capped. The outside world therefore never runs faster
 *                        than the declared rate and never past the cap.
 *   compressed           the clock JUMPS event by event over a routine span; charged the span resolved.
 *
 * A DECLARATION is clock{mode, interrupt: none|at_beat|immediate, resolve:
 * on_commit|on_complete|per_beat} plus the cost in game minutes (SYSTEMS §2.5;
 * the activity content states cost as durationMinutes) and the number of beats
 * the cost is divided into (default 1). Beat k ends at floor(cost * k / beats).
 *
 * INTERRUPTION (SYSTEMS §2.5 rule 4). none: an Interrupt event is held; the
 * interaction runs to its end. at_beat: it stops at the next beat boundary
 * (immediately when already on one); reaching the cost first completes it.
 * immediate: it stops where it is, and the elapsed minutes are charged pro rata.
 *
 * RESOLUTION BOUNDARY. on_commit: the whole cost is charged and the outcome applied
 * at the moment of commitment, so nothing can interrupt it (declared interrupt must
 * be none, beats 1). on_complete: the outcome applies only when it completes; a
 * stopped one is charged pro rata and applies no outcome (the Activities cancel
 * rule). per_beat: each beat's outcome applies as that beat completes.
 *
 * CHOSEN NUMBERS (the docs leave them open): the Max* and Mission* constants below,
 * the rule that a scene costs at least one minute, that conversation, free roam
 * and mission may be ended early (Finish), that a compressed span stops only at a
 * beat boundary, and that real time spent outside free roam or in a pause
 * carries no fractional second into the next free-roam stretch.
 */

enum class ClockMode : std::uint8_t
{
    FreeRoam,
    Conversation,
    Scene,
    UiPaused,
    AccessibilityPause,
    Mission,
    Compressed
};

constexpr std::size_t ClockModeCount = 7;

enum class InterruptPolicy : std::uint8_t
{
    None,
    AtBeat,
    Immediate
};

enum class ResolutionBoundary : std::uint8_t
{
    OnCommit,
    OnComplete,
    PerBeat
};

enum class WorldClockBehaviour : std::uint8_t
{
    Runs,
    Paused,
    MissionClockRuns,
    Jumps
};

/** How a mode charges game time to the world clock. */
enum class ChargeRule : std::uint8_t
{
    PerRealSecond,
    DeclaredAtClose,
    DeclaredAtResolution,
    Never,
    MissionFormula,
    SpanResolved
};

/** One game minute per real second (GAME_PLAN §5), as milli-minutes per real second. */
constexpr std::int32_t FreeRoamMilliMinutesPerRealSecond = 1000;
constexpr std::int32_t RealMillisecondsPerGameMinute = 1000;
/** A scene, or free-roam activity, is at most one day (the activity content bound). */
constexpr std::int32_t TimeSceneMaxCostMinutes = 1440;
/** Chosen: a conversation may declare at most an hour, so a talk never silently costs hours. */
constexpr std::int32_t TimeConversationMaxCostMinutes = 60;
/** Chosen: the outside world never runs faster than the mission clock (parts per thousand). */
constexpr std::int32_t TimeMissionMaxRatePermille = 1000;
/** Chosen: an expedition costs weeks of a life, not years; at most sixty days. */
constexpr std::int32_t TimeMissionMaxCapMinutes = 60 * 1440;
/** Chosen: one compressed resolution spans at most a year. */
constexpr std::int32_t TimeCompressedMaxCostMinutes = 366 * 1440;
/** Chosen: at most this many beats in one declaration (a year's days, with room). */
constexpr std::int32_t TimeMaxBeats = 400;

/** The rules of one mode. */
struct ClockModeRule
{
    ClockMode Mode = ClockMode::Scene;
    /** The content name, for example "free_roam". */
    std::string_view Name;
    WorldClockBehaviour World = WorldClockBehaviour::Paused;
    ChargeRule Charge = ChargeRule::Never;
    /** The world's game rate against real time, in milli-minutes per real second; 0 when it is not tied to real time. */
    std::int32_t WorldMilliMinutesPerRealSecond = 0;
    /** True when the ui_paused / accessibility_pause overlay stops this mode's running clock. */
    bool PausedByOverlays = true;
    /** True when the mode may consume opportunity time (false for ui_paused and accessibility_pause). */
    bool ChargesOpportunityTime = true;
    /** SYSTEMS §2.5 (saves): manual saves are refused while the mode is mission. */
    bool ManualSaveAllowed = true;
    /** True when the player or the game may end it before its declared cost (Finish). */
    bool EarlyFinishAllowed = false;
    std::int32_t MinCostMinutes = 0;
    std::int32_t MaxCostMinutes = 0;
    /** Bit masks over InterruptPolicy and ResolutionBoundary values. */
    std::uint32_t AllowedInterrupts = 0;
    std::uint32_t AllowedResolutions = 0;
    std::int32_t MaxBeats = 1;
};

[[nodiscard]] PERFECTWORLDCORE_API const ClockModeRule& TimeModeRule(ClockMode mode) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::string_view ClockModeName(ClockMode mode) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API bool ClockModeFromName(std::string_view name, ClockMode& out) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API bool InterruptPolicyFromName(std::string_view name, InterruptPolicy& out) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API bool ResolutionBoundaryFromName(std::string_view name, ResolutionBoundary& out) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::string_view InterruptPolicyName(InterruptPolicy policy) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::string_view ResolutionBoundaryName(ResolutionBoundary boundary) noexcept;

/** Mission terms: exactly one of FixedMinutes, or RatePermille with CapMinutes. All zero for every other mode. */
struct MissionTerms
{
    std::int32_t FixedMinutes = 0;
    /** Outside-world minutes charged per 1000 mission minutes. */
    std::int32_t RatePermille = 0;
    std::int32_t CapMinutes = 0;

    friend bool operator==(const MissionTerms&, const MissionTerms&) = default;
};

/** The outside-world minutes a mission of `missionMinutes` costs: the fixed amount, or minutes x rate capped. */
[[nodiscard]] PERFECTWORLDCORE_API std::int64_t MissionOutsideMinutes(const MissionTerms& terms,
                                                                      std::int64_t missionMinutes) noexcept;

struct InteractionDeclaration
{
    /** The content ID of what declares it, such as "activity.attend_class". */
    std::string Id;
    ClockMode Mode = ClockMode::Scene;
    /** The declared cost in game minutes. For a mission, the most the outside world can advance. */
    std::int32_t CostMinutes = 0;
    InterruptPolicy Interrupt = InterruptPolicy::AtBeat;
    ResolutionBoundary Resolve = ResolutionBoundary::OnComplete;
    std::int32_t Beats = 1;
    MissionTerms Mission;

    friend bool operator==(const InteractionDeclaration&, const InteractionDeclaration&) = default;
};

enum class TimeContractError : std::uint8_t
{
    None = 0,
    MissingDeclaration,
    UnknownMode,
    UnknownInterrupt,
    UnknownResolve,
    BadId,
    CostOutOfRange,
    InterruptNotAllowed,
    ResolveNotAllowed,
    BeatsOutOfRange,
    CommitCannotBeInterrupted,
    MissionTermsMissing,
    MissionTermsInvalid,
    MissionTermsOnOtherMode,
    InvalidEvent,
    InvalidState,
    PlanNotRoutine,
    PlanOutOfOrder,
    PlanOverlap,
    PlanOutOfSpan,
    PlanSpanInvalid,
    PlanNeedsPlay
};

/** A stable code such as "time.cost_out_of_range". Empty for None. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view TimeContractErrorCode(TimeContractError error) noexcept;

/** Checks a declaration against its mode's rules. None when sound. */
[[nodiscard]] PERFECTWORLDCORE_API TimeContractError ValidateDeclaration(const InteractionDeclaration& declaration);

/** True when Progress events add nothing: the pauses, and a conversation that declared no cost ("Nothing"). */
[[nodiscard]] PERFECTWORLDCORE_API bool InteractionAbsorbsProgress(const InteractionDeclaration& declaration) noexcept;

/** Beat boundary k (0..Beats): the minute at which beat k ends, floor(cost * k / beats). */
[[nodiscard]] PERFECTWORLDCORE_API std::int64_t BeatBoundary(const InteractionDeclaration& declaration, std::int32_t k) noexcept;
/** How many whole beats `progress` minutes cover (all of them at or past the cost; 0 for a zero cost). */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t BeatsCoveredBy(const InteractionDeclaration& declaration, std::int64_t progress) noexcept;

// ---------------------------------------------------------------------------
// Resolving a declared interaction under a sequence of events.
// ---------------------------------------------------------------------------

enum class TimeEventKind : std::uint8_t
{
    /** Minutes passed (interaction minutes; mission minutes for a mission). Absorbed by the paused modes. */
    Progress,
    /** The world interrupts; the declared policy decides what happens. */
    Interrupt,
    /** The interaction ends normally (talk.close, the menu closes, the mission is left). Only where EarlyFinishAllowed. */
    Finish,
    /** The player leaves early; charged pro rata, no outcome (a compressed span stops at its last whole beat). */
    Cancel
};

struct TimeEvent
{
    TimeEventKind Kind = TimeEventKind::Progress;
    std::int64_t Minutes = 0;

    friend bool operator==(const TimeEvent&, const TimeEvent&) = default;
};

enum class TimeOutcome : std::uint8_t
{
    Running,
    Completed,
    InterruptedAtBeat,
    InterruptedImmediately,
    Cancelled
};

/**
 * An interaction in flight. This is the state the step function advances and the
 * text form saves; ResolveInteraction is just the fold of StepInteraction over events.
 */
struct InteractionRun
{
    InteractionDeclaration Decl;
    std::int64_t Progress = 0;
    /** The pending at-beat stop point in minutes; -1 when none. */
    std::int64_t StopPoint = -1;
    std::int32_t InterruptsHeld = 0;
    std::int32_t EventsConsumed = 0;
    /** The part of the final consumed Progress event not used because the interaction resolved inside it. */
    std::int64_t UnusedProgress = 0;
    TimeOutcome Outcome = TimeOutcome::Running;

    friend bool operator==(const InteractionRun&, const InteractionRun&) = default;
};

enum class StepResult : std::uint8_t
{
    Consumed,
    AlreadyResolved,
    Invalid
};

struct TimeResolution
{
    TimeContractError Error = TimeContractError::None;
    TimeOutcome Outcome = TimeOutcome::Running;
    /** Interaction minutes that passed (mission minutes for a mission). */
    std::int64_t ElapsedMinutes = 0;
    /** Game minutes the world clock advances because of this interaction. */
    std::int64_t ChargedMinutes = 0;
    std::int32_t BeatsCompleted = 0;
    /** Beats whose outcome applies: all on completion under on_complete, each completed beat under per_beat. */
    std::int32_t OutcomeBeats = 0;
    std::int32_t InterruptsHeld = 0;
    /** Events used, and the leftover of the last one. NOT part of the equivalence (granularity changes them). */
    std::int32_t EventsConsumed = 0;
    std::int64_t UnusedProgress = 0;

    [[nodiscard]] bool Ok() const noexcept { return Error == TimeContractError::None; }
    [[nodiscard]] bool Resolved() const noexcept { return Outcome != TimeOutcome::Running; }
};

/** Starts a run. An on_commit declaration resolves at once. An unsound declaration is refused through `error`. */
[[nodiscard]] PERFECTWORLDCORE_API InteractionRun BeginInteraction(const InteractionDeclaration& declaration, TimeContractError& error);
/** Applies one event. A resolved run is left untouched (AlreadyResolved); a malformed event is Invalid. */
[[nodiscard]] PERFECTWORLDCORE_API StepResult StepInteraction(InteractionRun& run, const TimeEvent& event);
/** What the run has resolved so far (charged-so-far while still running). */
[[nodiscard]] PERFECTWORLDCORE_API TimeResolution DescribeInteraction(const InteractionRun& run);
/** Begin, step every event until it resolves (events after that are not consumed), describe. */
[[nodiscard]] PERFECTWORLDCORE_API TimeResolution ResolveInteraction(const InteractionDeclaration& declaration,
                                                                     const std::vector<TimeEvent>& events);

/** Text form of a run in flight; strict, so a truncated or edited text is refused (empty / Error). */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseInteractionRun(const InteractionRun& run);
struct InteractionRunParse
{
    std::string Error;
    InteractionRun Run;
    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};
[[nodiscard]] PERFECTWORLDCORE_API InteractionRunParse DeserialiseInteractionRun(std::string_view text);

// ---------------------------------------------------------------------------
// The mode controller: which mode is live, and free-roam real time.
// ---------------------------------------------------------------------------

/**
 * The live clock mode. A base mode (free roam, conversation, scene, mission,
 * compressed) with two overlay flags: ui_paused (phone, menus, reading) and
 * accessibility_pause. Overlays stop the running clock and are never charged;
 * accessibility wins over ui. Real time only becomes game time in free roam,
 * one minute per whole real second; the sub-second carry is dropped whenever the
 * base mode changes.
 */
struct TimeModeState
{
    ClockMode Base = ClockMode::FreeRoam;
    bool UiOpen = false;
    bool AccessibilityOpen = false;
    /** Real milliseconds toward the next free-roam minute, 0..999; 0 outside free roam. */
    std::int32_t CarryMilliseconds = 0;

    friend bool operator==(const TimeModeState&, const TimeModeState&) = default;
};

[[nodiscard]] PERFECTWORLDCORE_API bool IsTimeModeStateValid(const TimeModeState& state) noexcept;
/** The mode in force: accessibility_pause, else ui_paused, else the base. */
[[nodiscard]] PERFECTWORLDCORE_API ClockMode EffectiveClockMode(const TimeModeState& state) noexcept;
/** True when real time is becoming game time right now (free roam, no overlay). */
[[nodiscard]] PERFECTWORLDCORE_API bool WorldClockRuns(const TimeModeState& state) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API bool ManualSaveAllowed(const TimeModeState& state) noexcept;
/** Changes the base mode; refused for the overlay modes and while an overlay is open. Drops the carry. */
[[nodiscard]] PERFECTWORLDCORE_API bool EnterBaseMode(TimeModeState& state, ClockMode mode) noexcept;
PERFECTWORLDCORE_API void SetUiPause(TimeModeState& state, bool open) noexcept;
PERFECTWORLDCORE_API void SetAccessibilityPause(TimeModeState& state, bool open) noexcept;
/** Real time spent: the whole game minutes it adds. Zero unless WorldClockRuns; negative input adds nothing. */
[[nodiscard]] PERFECTWORLDCORE_API std::int64_t AdvanceRealTime(TimeModeState& state, std::int64_t realMilliseconds) noexcept;

[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseTimeModeState(const TimeModeState& state);
struct TimeModeStateParse
{
    std::string Error;
    TimeModeState State;
    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};
[[nodiscard]] PERFECTWORLDCORE_API TimeModeStateParse DeserialiseTimeModeState(std::string_view text);

// ---------------------------------------------------------------------------
// Declarations in content.
// ---------------------------------------------------------------------------

struct DeclarationParse
{
    TimeContractError Error = TimeContractError::None;
    InteractionDeclaration Declaration;
    [[nodiscard]] bool Ok() const noexcept { return Error == TimeContractError::None; }
};

/**
 * The declaration an activity record makes: its clock{mode, interrupt, resolve}
 * block, its durationMinutes as the cost, one beat. A record with no clock block,
 * or an unknown name, is refused; a parsed declaration is also validated.
 */
[[nodiscard]] PERFECTWORLDCORE_API DeclarationParse DeclarationFromActivityRecord(const ContentRecord& record);

/** Every activity record's declaration checked; one line per failure ("activity.x: time.cost_out_of_range"). Empty when all sound. */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<std::string> ValidateActivityDeclarations(const ContentDb& content);

// ---------------------------------------------------------------------------
// The equivalence rule (SYSTEMS §2.5 rule 2, §8 "granularity equivalence").
// ---------------------------------------------------------------------------

/**
 * A routine plan: declared interactions at fixed starts inside a span. "Scenes
 * played with routine choices", "a routine week" and "a season" are the same
 * plan resolved with a different step size; the persistent outcome (the ledger)
 * must not depend on it. Mission and compressed interactions are not routine and
 * are refused. A plan holding a first (FirstOfKind) is never auto-resolved: any
 * coarse granularity refuses it with PlanNeedsPlay.
 */
struct PlannedInteraction
{
    InteractionDeclaration Decl;
    /** Minutes from the start of the plan. */
    std::int64_t StartMinute = 0;
    bool FirstOfKind = false;
    /** The player's standing orders chose this; compression discloses it (counted in the ledger). */
    bool DecidedByOrder = false;
};

struct TimePlan
{
    std::int64_t SpanMinutes = 0;
    std::uint64_t Seed = 0;
    /** Sorted by StartMinute, each ending (StartMinute + cost) at or before the next start and at or before the span. */
    std::vector<PlannedInteraction> Items;
};

enum class TimeGranularity : std::uint8_t
{
    /** Each interaction on its own, its progress fed in SceneChunkMinutes at a time. */
    Scenes,
    Day,
    Week,
    Span
};

constexpr std::int64_t TimeSceneChunkMinutes = 15;

/**
 * The persistent outcome of a resolved plan. Every field except Steps must be
 * identical at every granularity; Steps is excluded (see TimeEquivalenceExcluded).
 */
struct TimeLedger
{
    std::int64_t EndMinute = 0;
    std::array<std::int64_t, ClockModeCount> ChargedByMode{};
    std::int64_t TotalCharged = 0;
    std::int32_t Completed = 0;
    std::int32_t OutcomeBeats = 0;
    /** Interactions chosen by standing order: what the receipt must disclose. */
    std::int32_t DecidedByOrder = 0;
    /** Fold of the keyed per-day draw of every day crossed, in day order. */
    std::uint64_t DayDrawHash = 0;
    /** Fold of each interaction's keyed draw, ID, charge and outcome, in plan order. */
    std::uint64_t InteractionHash = 0;
    /** Resolution steps taken. Excluded from equivalence. */
    std::int32_t Steps = 0;

    friend bool operator==(const TimeLedger&, const TimeLedger&) = default;
};

/** What a different granularity is allowed to change; everything else in TimeLedger must match. */
inline constexpr std::array<std::string_view, 6> TimeEquivalenceExcluded{
    "steps", "events_consumed", "unused_progress", "receipts", "real_time_spent", "command_count"};
/** The ledger quantities that must match, by name (the names CompareLedgers returns). */
inline constexpr std::array<std::string_view, 8> TimeEquivalenceQuantities{
    "end_minute", "charged_by_mode", "total_charged", "completed", "outcome_beats",
    "decided_by_order", "day_draw_hash", "interaction_hash"};

/** The keyed per-day draw: a pure function of (seed, day, index), never of the order resolved in. */
[[nodiscard]] PERFECTWORLDCORE_API std::uint64_t TimeKeyedDraw(std::uint64_t seed, std::int64_t day, std::int64_t index) noexcept;

[[nodiscard]] PERFECTWORLDCORE_API TimeContractError ValidatePlan(const TimePlan& plan);

struct TimePlanResult
{
    TimeContractError Error = TimeContractError::None;
    TimeLedger Ledger;
    [[nodiscard]] bool Ok() const noexcept { return Error == TimeContractError::None; }
};

[[nodiscard]] PERFECTWORLDCORE_API TimePlanResult ResolvePlan(const TimePlan& plan, TimeGranularity granularity);

/** The first must-match quantity that differs (a TimeEquivalenceQuantities name), or empty when equivalent. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view CompareLedgers(const TimeLedger& a, const TimeLedger& b) noexcept;

struct EquivalenceReport
{
    TimeContractError Error = TimeContractError::None;
    /** Empty when every granularity gave the same ledger; else "<granularity>: <quantity>". */
    std::string Difference;
    [[nodiscard]] bool Equivalent() const noexcept { return Error == TimeContractError::None && Difference.empty(); }
};

/** Resolves the plan as scenes, days, weeks and one span, and compares the three coarser ledgers with the first. */
[[nodiscard]] PERFECTWORLDCORE_API EquivalenceReport CompareGranularities(const TimePlan& plan);
} // namespace PerfectWorld
