#pragma once

#include "Condition.h"
#include "ContentRegistry.h"
#include "GameClock.h"
#include "LatentStats.h"

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
 * Activities: a declared duration, clock mode, place, condition cost, outcome,
 * learning allowance split and limited attribute evidence (ROADMAP M2-03;
 * SYSTEMS_CATALOGUE "Activities and interaction structures";
 * SCHOOL_AND_PROGRESSION "Every activity produces three separate records").
 *
 * The catalogue is content: Data/Content/activity/*.json, checked by the one
 * validator. This module reads those records (LoadActivityCatalogue) and runs
 * them (StartActivity / CompleteActivity / CancelActivity) over a small plain
 * state. It is pure: it never reads or advances the clock, it owns neither the
 * condition nor the stats, and it is not yet a member of WorldState, the
 * reducer, the hash or WorldSave. The integration step wraps it in the
 * `activity.begin|cancel` commands and the "activities/complete/<instance>"
 * scheduled event; the caller passes the current instant and the condition.
 *
 * Exactly once. A started activity has an Instance number; its outcome is
 * applied by the one call that ends it (complete or cancel), which clears the
 * activity and records the Instance as applied. A second end finds nothing to
 * end and is refused, so no path applies an outcome twice. Every event carries
 * its Instance, which is also the idempotency key for whoever consumes it.
 * The state saves as text (SerialiseActivityRunner), so a save taken
 * mid-activity restores the same activity and completes it once.
 *
 * Outcome on completion, applied together or not at all:
 *   - condition: the declared duration spent in the declared condition activity
 *     (AdvanceCondition), then the meal if any (ApplyMeal), then the stress
 *     delta if any (AdjustStress). The caller must not also integrate these
 *     minutes elsewhere;
 *   - learning: the allowance (milli-XP) divided across skills by the split;
 *   - evidence: the declared attribute evidence (never growth itself).
 * Cancel (documented): the activity ends early. The condition is charged for
 * the minutes actually elapsed (pro rata, SYSTEMS.md §2.5 rule 4); the meal,
 * the stress delta, learning and evidence are not applied, because an unfinished
 * session teaches nothing and no meal was eaten. Cancelling at or after the end
 * is refused: that activity should be completed.
 */

enum class ActivityClockMode : std::uint8_t
{
    FreeRoam,
    Scene,
    Mission,
    Compressed
};

enum class ActivityInterrupt : std::uint8_t
{
    None,
    AtBeat,
    Immediate
};

struct ActivityShare
{
    std::string SkillId;
    /** Parts per thousand of the allowance. The shares of one activity sum to exactly ActivitySplitWhole. */
    std::int32_t Permille = 0;

    friend bool operator==(const ActivityShare&, const ActivityShare&) = default;
};

struct ActivityEvidenceDef
{
    Attribute Attr = Attribute::Body;
    /** Milli of evidence toward the attribute. */
    std::int32_t Milli = 0;

    friend bool operator==(const ActivityEvidenceDef&, const ActivityEvidenceDef&) = default;
};

/** The whole of a learning allowance, in parts per thousand. */
constexpr std::int32_t ActivitySplitWhole = 1000;
/** Attribute evidence is at most this fraction of the learning allowance: one twentieth (SYSTEMS_CATALOGUE). */
constexpr std::int32_t ActivityEvidenceDivisor = 20;
/** The place kind that matches every place. */
inline constexpr std::string_view ActivityAnyPlace = "any";

struct ActivityDef
{
    std::string Id;
    std::string Name;
    std::int32_t DurationMinutes = 0;
    ActivityClockMode Clock = ActivityClockMode::Scene;
    ActivityInterrupt Interrupt = ActivityInterrupt::AtBeat;
    /** A place kind ("home", "classroom", "dining", "outdoors") or ActivityAnyPlace. */
    std::string Place;
    ConditionActivity Condition = ConditionActivity::Awake;
    bool HasMeal = false;
    MealSize Meal = MealSize::Snack;
    std::int32_t StressDelta = 0;
    /** The practical outcome kind, such as "studied" or "ate". */
    std::string Outcome;
    /** Milli-XP; 0 for an activity that teaches nothing, in which case Split is empty. */
    std::int32_t AllowanceMilliXp = 0;
    std::vector<ActivityShare> Split;
    std::vector<ActivityEvidenceDef> Evidence;
};

struct ActivityCatalogue
{
    /** Sorted by Id. */
    std::vector<ActivityDef> Defs;

    [[nodiscard]] PERFECTWORLDCORE_API const ActivityDef* Find(std::string_view id) const noexcept;
};

struct ActivityCatalogueResult
{
    /** Empty on success; otherwise the record and what is wrong with it. */
    std::string Error;
    ActivityCatalogue Catalogue;

    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/** Reads every activity record. Refuses (never coerces) a split that does not sum to the whole or evidence over its cap. */
[[nodiscard]] PERFECTWORLDCORE_API ActivityCatalogueResult LoadActivityCatalogue(const ContentDb& content);

struct SkillCredit
{
    std::string SkillId;
    std::int32_t MilliXp = 0;

    friend bool operator==(const SkillCredit&, const SkillCredit&) = default;
};

struct EvidenceCredit
{
    Attribute Attr = Attribute::Body;
    std::int32_t Milli = 0;

    friend bool operator==(const EvidenceCredit&, const EvidenceCredit&) = default;
};

/**
 * Divides `allowance` by parts per thousand using the largest-remainder rule:
 * each share gets its floor, and the leftover milli go one each to the largest
 * remainders (ties to the earlier share). The result sums to exactly
 * `allowance` whenever the parts sum to ActivitySplitWhole; otherwise it is
 * empty. A negative allowance or part is refused the same way.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<std::int32_t> SplitAllowance(std::int64_t allowance,
                                                                            const std::vector<std::int32_t>& permille);

/** The skill credits a completion of `def` grants: its allowance by its split, in split order, omitting zero credits. */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<SkillCredit> ActivityLearningCredits(const ActivityDef& def);

/**
 * The runner's whole state: one activity at a time. Idle when ActivityId is
 * empty. Instance counts the activities ever started; AppliedInstance is the
 * last whose outcome was applied, so Instance == AppliedInstance exactly when
 * idle and Instance == AppliedInstance + 1 while one is running.
 */
struct ActivityRunnerState
{
    std::string ActivityId;
    GameInstant StartedAt = 0;
    std::uint64_t Instance = 0;
    std::uint64_t AppliedInstance = 0;

    [[nodiscard]] bool Active() const noexcept { return !ActivityId.empty(); }

    friend bool operator==(const ActivityRunnerState&, const ActivityRunnerState&) = default;
};

/** True when the state is internally consistent (the invariants above, a well-formed ID, no negative start). */
[[nodiscard]] PERFECTWORLDCORE_API bool IsActivityRunnerStateValid(const ActivityRunnerState& state);

enum class ActivityError : std::uint8_t
{
    None = 0,
    UnknownActivity,
    Busy,
    NotActive,
    NotFinished,
    Finished,
    WrongPlace,
    InvalidTime,
    InvalidCondition,
    InvalidState
};

/** The stable rejection code, for example "activity.busy". Empty for None. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view ActivityErrorCode(ActivityError error) noexcept;

enum class ActivityEventKind : std::uint8_t
{
    Started,
    Completed,
    Cancelled
};

struct ActivityEvent
{
    ActivityEventKind Kind = ActivityEventKind::Started;
    std::uint64_t Instance = 0;
    std::string ActivityId;
    /** The instant the call was made. */
    GameInstant At = 0;
    /** Started: the declared duration. Completed: the duration. Cancelled: the minutes actually elapsed. */
    std::int64_t Minutes = 0;
    /** Completed only: the practical outcome kind. */
    std::string Outcome;
    /** Completed only. */
    std::vector<SkillCredit> Learning;
    /** Completed only. */
    std::vector<EvidenceCredit> Evidence;

    friend bool operator==(const ActivityEvent&, const ActivityEvent&) = default;
};

struct ActivityResult
{
    ActivityError Error = ActivityError::None;
    std::vector<ActivityEvent> Events;

    [[nodiscard]] bool Ok() const noexcept { return Error == ActivityError::None; }
};

/**
 * Begin an activity at `now`, in a place of kind `placeKind` (must match the
 * activity's place unless that is "any"). Refused while another activity runs.
 * A refusal changes nothing.
 */
PERFECTWORLDCORE_API ActivityResult StartActivity(ActivityRunnerState& state, const ActivityCatalogue& catalogue,
                                                  std::string_view activityId, GameInstant now,
                                                  std::string_view placeKind);

/**
 * Finish the running activity at `now` (not before StartedAt + duration) and
 * apply its outcome to `condition`, once. The learning and evidence come back
 * in the event for the stats owner to apply. A refusal changes nothing,
 * including `condition`.
 */
PERFECTWORLDCORE_API ActivityResult CompleteActivity(ActivityRunnerState& state, const ActivityCatalogue& catalogue,
                                                     ConditionState& condition, GameInstant now);

/** End the running activity early at `now` with the partial outcome documented above. */
PERFECTWORLDCORE_API ActivityResult CancelActivity(ActivityRunnerState& state, const ActivityCatalogue& catalogue,
                                                   ConditionState& condition, GameInstant now);

/**
 * The runner state as text: a header line, four "key=value" lines and "end",
 * each ending in a newline, integers only. The same state always gives the same
 * text. Empty for an inconsistent state.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseActivityRunner(const ActivityRunnerState& state);

struct ActivityRunnerParse
{
    /** Empty on success; otherwise why the text was refused. */
    std::string Error;
    ActivityRunnerState State;

    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/** Reads SerialiseActivityRunner's text. Refuses truncation, extra text, malformed numbers and inconsistent state. */
[[nodiscard]] PERFECTWORLDCORE_API ActivityRunnerParse DeserialiseActivityRunner(std::string_view text);
} // namespace PerfectWorld
