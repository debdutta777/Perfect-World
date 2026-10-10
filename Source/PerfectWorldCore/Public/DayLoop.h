#pragma once

#include "Activities.h"
#include "Attendance.h"
#include "Condition.h"
#include "GameClock.h"
#include "OpportunityReceipts.h"
#include "PracticeWindow.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * The day loop and recovery checkpoints (ROADMAP M2-12; GAME_PLAN "Day: morning
 * plan -> commitments -> free blocks -> evening -> sleep -> receipt";
 * SYSTEMS "Life slots and recovery checkpoints": an automatic checkpoint on
 * waking after sleep).
 *
 * The first step that composes the standalone M2 modules. DayLoopState holds
 * the player's Condition, ActivityRunner, Practice window, Receipts and
 * AttendanceRecord, the current game time, a day phase, the current commitment,
 * the last action and a dense command sequence number. Nothing here
 * re-implements a module: every command calls the module that owns the rule
 * and the envelope embeds each module's own text. Not yet a member of
 * WorldState, the reducer, the world hash or WorldSave.
 *
 * Time. The loop owns the one clock for these modules, and time moves only
 * inside an accepted command, so a command log replays exactly:
 *   - Pass spends minutes awake, resting or walking, through Condition;
 *   - Complete moves the clock to the end of the running activity and the
 *     activity applies its own condition cost for that duration; Cancel moves
 *     it by the elapsed minutes it is given (the activity charges pro rata);
 *   - Wake sleeps the minutes it is given and moves the clock by them.
 * While an activity runs the clock stands at its start. Planning, committing,
 * starting and the phone cost nothing ("free time is free").
 *
 * The day. Phase is a function of the rest of the state (DayLoopPhaseOf), and a
 * state whose Phase disagrees is invalid:
 *   Sleep      asleep (GoToSleep until Wake);
 *   Plan       awake and the morning plan is not yet made (PlanDay, which may
 *              be an empty plan, is required before anything spends time);
 *   School     the student has arrived at school today and the clock is before
 *              dismissal (or the departure minute they gave);
 *   Evening    from 18:00 until 06:00;
 *   FreeBlock  every other awake time.
 * PlanDay is free and may be repeated all day. The plan is the player's
 * intention, shown on the phone; committing to something else is allowed.
 *
 * Commitments and receipts. Commit decides (the alternatives the player was
 * shown are passed here, because this is the instant they were shown), Start
 * begins, Complete finishes. A commitment is recorded in OpportunityReceipts
 * when the activity completes, from the alternatives stored at Commit, with the
 * forecast taken at the activity's own start so it equals the credit. Cancel
 * records nothing: an unfinished session earns nothing and the receipt lists
 * what was done. Learning credits go through PracticeWindow at the activity's
 * START instant, in the practice family DayLoopPracticeFamily gives it; the
 * credits the stats owner must apply come back in the Completed event after
 * the tier and the daily budget.
 *
 * Refusal. ApplyDayCommand validates every precondition and works on a copy,
 * so a refused command leaves the state byte for byte unchanged.
 *
 * Wake closes earlier unrecorded school days as absent (CloseAttendanceBefore),
 * folds old receipts (FoldReceipts), counts the wake and reports CheckpointDue.
 * The rotating checkpoint index is the wake count modulo CheckpointCount
 * (Checkpoints.h). Neither failing to catch attendance up nor anything else
 * after the sleep can refuse a Wake: a player is never stuck asleep.
 */

constexpr std::size_t DayLoopMaxPlanItems = 24;
constexpr std::int64_t DayLoopMaxPassMinutes = MinutesPerDay;
constexpr std::int64_t DayLoopMaxSleepMinutes = MinutesPerDay;
/** Evening opens at 18:00 and lasts until the morning at 06:00 (chosen). */
constexpr std::int32_t DayLoopEveningStartMinute = 18 * 60;
constexpr std::int32_t DayLoopMorningStartMinute = 6 * 60;
constexpr GameInstant DayLoopMaxInstant = ReceiptMaxInstant;
/** Rotating recovery checkpoints a life keeps (SYSTEMS: checkpoint_1..3). */
constexpr std::int32_t DayLoopCheckpointCount = 3;
/** Alternatives one commitment may carry. */
constexpr std::size_t DayLoopMaxAlternatives = 64;
/** The envelope's version header. */
inline constexpr std::string_view DayLoopTextHeader = "pw.dayloop/1";

enum class DayPhase : std::uint8_t
{
    Plan = 0,
    School,
    FreeBlock,
    Evening,
    Sleep
};

/** The stable lowercase name: "plan", "school", "free_block", "evening", "sleep". */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view DayPhaseName(DayPhase phase) noexcept;

enum class DayCommandKind : std::uint8_t
{
    PlanDay = 0,
    Commit,
    Start,
    Complete,
    Cancel,
    Pass,
    ArriveSchool,
    GoToSleep,
    Wake
};

/** The stable name, for example "plan_day", "arrive_school". */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view DayCommandName(DayCommandKind kind) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::optional<DayCommandKind> DayCommandFromName(std::string_view name) noexcept;

/** One intention in the morning plan: start at this minute of the day. */
struct DayPlanItem
{
    std::int32_t StartMinute = 0;
    std::string ActivityId;

    friend bool operator==(const DayPlanItem&, const DayPlanItem&) = default;
};

/** The activity the player has decided on; it is running exactly when the runner is active. */
struct DayLoopCommitment
{
    std::string ActivityId;
    /** The kind of place the player is in ("home", "classroom"); must match the activity's place. */
    std::string PlaceKind;
    GameInstant DecidedAt = 0;
    /** The alternatives shown at the decision; the receipt keeps those it displaced. */
    std::vector<ReceiptCandidate> Alternatives;

    /** ReceiptCandidate has no comparison of its own, so the fields are compared here. */
    friend bool operator==(const DayLoopCommitment& a, const DayLoopCommitment& b)
    {
        if (a.ActivityId != b.ActivityId || a.PlaceKind != b.PlaceKind || a.DecidedAt != b.DecidedAt ||
            a.Alternatives.size() != b.Alternatives.size())
            return false;
        for (std::size_t i = 0; i < a.Alternatives.size(); ++i)
        {
            const ReceiptCandidate& x = a.Alternatives[i];
            const ReceiptCandidate& y = b.Alternatives[i];
            if (x.Id != y.Id || x.Source != y.Source || x.KnownAt != y.KnownAt || x.Start != y.Start ||
                x.DurationMinutes != y.DurationMinutes || x.Available != y.Available || !(x.Forecast == y.Forecast))
                return false;
        }
        return true;
    }
};

struct DayLastAction
{
    DayCommandKind Kind = DayCommandKind::PlanDay;
    /** The clock after the command. */
    GameInstant At = 0;
    /** The activity the command was about; empty for time, sleep and school commands. */
    std::string ActivityId;

    friend bool operator==(const DayLastAction&, const DayLastAction&) = default;
};

struct DayLoopState
{
    ConditionState Condition;
    ActivityRunnerState Runner;
    PracticeState Practice;
    ReceiptState Receipts;
    AttendanceRecord Attendance;

    GameInstant Now = 0;
    DayPhase Phase = DayPhase::Plan;
    /** Commands accepted so far, dense from 0. */
    std::uint64_t Seq = 0;
    /** Times the player has woken; the checkpoint rotation counts them. */
    std::uint32_t Wakes = 0;
    bool Planned = false;
    std::vector<DayPlanItem> Plan;
    bool Asleep = false;
    /** When the player fell asleep; 0 unless Asleep. */
    GameInstant SleptAt = 0;
    /** The school day ends here (dismissal or the given departure); 0 until the student arrives. */
    GameInstant SchoolUntil = 0;
    bool HasCommitment = false;
    DayLoopCommitment Commitment;
    bool HasLastAction = false;
    DayLastAction Last;

    friend bool operator==(const DayLoopState&, const DayLoopState&) = default;
};

/** A fresh life at `start` (the campaign opens at instant 0, 06:00 on Monday 2 June 2042): awake, nothing planned. */
[[nodiscard]] PERFECTWORLDCORE_API DayLoopState NewDayLoopState(GameInstant start = 0);

/** The phase the rest of the state implies. */
[[nodiscard]] PERFECTWORLDCORE_API DayPhase DayLoopPhaseOf(const DayLoopState& state);

/** True when every module's state is valid and the fields agree (phase, running activity, commitment, plan, clock). */
[[nodiscard]] PERFECTWORLDCORE_API bool IsDayLoopStateValid(const DayLoopState& state);

/**
 * What the commands read: all are borrowed. A missing school means no school commands.
 * A set Calendar (found with SchoolCalendarSet::Find for the school's institution and stage)
 * is passed to the attendance calls: holidays, breaks and non-term days are never closed as
 * absences at a wake and ArriveSchool on one is refused (AttendanceRefused). Null keeps the
 * Monday to Friday rule.
 */
struct DayLoopCatalogues
{
    const ActivityCatalogue* Activities = nullptr;
    const BellSchedule* School = nullptr;
    const SchoolCalendar* Calendar = nullptr;
};

/** Spending time without an activity. */
enum class DayPassKind : std::uint8_t
{
    Wait = 0, // awake
    Rest,     // sitting or lying down awake
    Walk      // light exertion, such as the walk to school
};

struct DayCommand
{
    DayCommandKind Kind = DayCommandKind::PlanDay;
    /** PlanDay. */
    std::vector<DayPlanItem> Plan;
    /** Commit. */
    std::string ActivityId;
    std::string PlaceKind;
    std::vector<ReceiptCandidate> Alternatives;
    /** Pass: minutes spent. Cancel: minutes already elapsed. Wake: minutes slept. */
    std::int64_t Minutes = 0;
    /** Pass. */
    DayPassKind Pass = DayPassKind::Wait;
    /** ArriveSchool: the minute of the day the student leaves, or AttendanceStaysUntilDismissal. */
    std::int32_t DepartureMinute = AttendanceStaysUntilDismissal;
};

enum class DayLoopError : std::uint8_t
{
    None = 0,
    InvalidState,
    MissingCatalogue,
    InvalidCommand,
    UnknownActivity,
    WrongPlace,
    Asleep,
    NotAsleep,
    PlanFirst,
    Busy,
    NotCommitted,
    AlreadyCommitted,
    NotRunning,
    InvalidTime,
    NoSchool,
    ActivityRefused,
    PracticeRefused,
    ReceiptRefused,
    AttendanceRefused,
    ConditionRefused
};

/** The stable rejection code, for example "dayloop.plan_first". Empty for None. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view DayLoopErrorCode(DayLoopError error) noexcept;

enum class DayLoopEventKind : std::uint8_t
{
    Planned = 0,
    Committed,
    Started,
    Completed,
    Cancelled,
    Passed,
    ArrivedAtSchool,
    SleepBegan,
    Woke,
    CheckpointDue
};

struct DayLoopEvent
{
    DayLoopEventKind Kind = DayLoopEventKind::Planned;
    GameInstant At = 0;
    std::string ActivityId;
    /** Started, Completed, Cancelled: the runner's instance number. */
    std::uint64_t Instance = 0;
    /** Minutes spent (Passed), slept (Woke) or elapsed (Cancelled); the duration for Completed. */
    std::int64_t Minutes = 0;
    /** Completed. */
    std::string Outcome;
    /** Completed: the milli-XP to apply to the stats, after the practice tier and the daily budget. */
    std::vector<SkillCredit> Credited;
    std::vector<EvidenceCredit> Evidence;
    PracticeLabel Label = PracticeLabel::UsefulPractice;
    std::vector<std::string> Consolidates;
    /** Completed: the receipt entry's Seq, or 0 when the day's overflow totals took it. */
    std::uint64_t ReceiptSeq = 0;
    /** ArrivedAtSchool. */
    AttendanceOutcome Attendance = AttendanceOutcome::OnTime;
    /** CheckpointDue: which rotating checkpoint to write (Checkpoints.h). */
    std::int32_t CheckpointIndex = 0;
};

struct DayLoopResult
{
    DayLoopError Error = DayLoopError::None;
    /** The owning module's own code or sentence when it refused, for example "activity.wrong_place". */
    std::string Detail;
    std::vector<DayLoopEvent> Events;
    /** True when a Wake was accepted: the caller writes a checkpoint now. */
    bool CheckpointDue = false;
    std::int32_t CheckpointIndex = 0;

    [[nodiscard]] bool Ok() const noexcept { return Error == DayLoopError::None; }
};

/**
 * Validate `command` against `state`, then apply it. A refusal returns the
 * error and changes nothing, including `state`. Pure: same state, catalogues
 * and command give the same result.
 */
PERFECTWORLDCORE_API DayLoopResult ApplyDayCommand(DayLoopState& state, const DayLoopCatalogues& catalogues,
                                                   const DayCommand& command);

/**
 * The practice family of an activity: "family." plus its outcome, for example
 * "family.studied". A default until the activity schema carries a family field
 * (an open question): every activity with the same outcome shares one window.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string DayLoopPracticeFamily(const ActivityDef& def);

/** What an activity would cost and teach if started now: the forecast shown before the player commits. */
struct DayActivityForecast
{
    bool Ok = false;
    std::string FamilyId;
    /** The practice forecast (empty credit for an activity that teaches nothing). */
    PracticeForecast Practice;
    /** Learning is the forecast credit in milli-XP; the condition cost is energy lost plus hunger gained over the duration. */
    ReceiptForecast Receipt;
};

[[nodiscard]] PERFECTWORLDCORE_API DayActivityForecast ForecastDayActivity(const DayLoopState& state,
                                                                           const ActivityCatalogue& catalogue,
                                                                           std::string_view activityId,
                                                                           GameInstant startAt);

/** What the load screen shows about a life slot (GAME_PLAN: current commitments and last action). */
struct DayLoopSlotSummary
{
    GameInstant Now = 0;
    DayPhase Phase = DayPhase::Plan;
    std::uint64_t Seq = 0;
    std::uint32_t Wakes = 0;
    bool HasCommitment = false;
    std::string CommitmentActivityId;
    /** True when the committed activity is running. */
    bool CommitmentStarted = false;
    bool HasLastAction = false;
    DayCommandKind LastKind = DayCommandKind::PlanDay;
    GameInstant LastAt = 0;
    std::string LastActivityId;

    friend bool operator==(const DayLoopSlotSummary&, const DayLoopSlotSummary&) = default;
};

[[nodiscard]] PERFECTWORLDCORE_API DayLoopSlotSummary SummariseDayLoop(const DayLoopState& state);

/**
 * The envelope, "pw.dayloop/1":
 *   pw.dayloop/1
 *   state now=.. phase=.. seq=.. wakes=.. planned=.. asleep=.. slept_at=.. school_until=..
 *   condition energy=.. hunger=.. health=.. stress=..
 *   last kind=<name|none> at=.. activity=<id|->
 *   plan=N  /  p start=.. activity=..                      (N lines)
 *   commitment=0|1  /  c activity=.. place=.. decided=.. alternatives=K  /  a id=.. source=.. known=.. start=..
 *                      minutes=.. available=.. learning=.. condition=..     (K lines)
 *   section runner|practice|receipts|attendance <bytes>   (then exactly that many bytes of the module's own text)
 *   hash <16 lowercase hex>
 *   end
 * Integers and tokens only. The same state always gives the same text; empty
 * for an invalid state.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseDayLoop(const DayLoopState& state);

struct DayLoopParse
{
    /** Empty on success; otherwise why the text was refused. */
    std::string Error;
    DayLoopState State;

    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/**
 * Reads SerialiseDayLoop's text. Refuses truncation at any byte, extra text,
 * non-canonical numbers, a wrong hash, any section its module refuses and any
 * state that is not valid. Only the canonical text parses.
 */
[[nodiscard]] PERFECTWORLDCORE_API DayLoopParse ParseDayLoop(std::string_view text);

/**
 * The combined FNV-1a 64 hash of the state: over the core text and the four
 * module texts, each framed by its name and byte length, in the envelope's
 * order. Zero for an invalid state. The envelope stores it.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::uint64_t HashDayLoop(const DayLoopState& state);

/** The load screen's summary, read from a whole envelope (parsed strictly); empty when the text is refused. */
[[nodiscard]] PERFECTWORLDCORE_API std::optional<DayLoopSlotSummary> ReadDayLoopSlotSummary(std::string_view text);
} // namespace PerfectWorld
