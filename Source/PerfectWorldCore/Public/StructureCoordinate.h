#pragma once

#include "StructureCommon.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace PerfectWorld
{
/**
 * The "coordinate a group" structure and its classroom family (ROADMAP M2-09;
 * SCHOOL_AND_PROGRESSION "Four interaction structures": "Assign work, exchange
 * information, settle a constraint, submit together", for projects, debate, events
 * and practicals; "the group's state; the player's role"; ALTERNATE_WORLD: "Get three
 * departments to agree a deadline", "Plan a ward rota"; SYSTEMS_CATALOGUE "The
 * engines ... Coordinate").
 *
 * A pure, integer-only runner for one exposure over a plain state (CoordinateState),
 * with the conventions of StructureCommon.h. Not yet a member of WorldState, the
 * reducer or the save.
 *
 * Content: coordinate_family and coordinate_case records (Data/Content). A case is a
 * small group project: the player is one member (marked self), the others are
 * classmates; tasks have a load and the members who suit them; each member has a
 * capacity. Some facts are held by classmates, and the critical ones reveal the one
 * constraint binding the group (capacity, a clash, the budget, the deadline).
 *
 * One exposure is the four beats of the docs, in order:
 *   1. Assign work. One decision: every task goes to one member. A practised method
 *      does it in CoordinateAssignKnownUnits; an unfamiliar one takes
 *      CoordinateAssignDeriveUnits. There is no hidden right assignment: any that
 *      keeps each member within capacity and each task with a member who suits it is
 *      Sound, and the loader refuses a case with none.
 *   2. Exchange information. The player talks to classmates one at a time, each once,
 *      at CoordinateExchangeUnits, and learns the facts that member holds. Ending the
 *      exchange (free) moves on. The constraint is only plain from the critical facts.
 *   3. Settle a constraint. One resolution: rebalance the work (fits a capacity
 *      constraint), reschedule (a clash), trim the scope (the budget), ask for an
 *      extension (the deadline), or leave it (free, and not a settlement). Only the
 *      matching one settles it, as "only one investigation fits" in the model.
 *   4. Submit together. Check the hand-over first (CoordinateCheckUnits) or submit
 *      as it stands (free). A seam, where the parts do not join, is only found by a
 *      check.
 * The player chooses a goal first, which only makes its own beat cheaper; a role does
 * the same (chair assigns, mediator exchanges, treasurer settles, scribe submits),
 * and the two do not stack on one beat.
 *
 * Scarce resources: time (the scarce-time units), attention (each conversation costs
 * CoordinateAttentionSurcharge more), space (settling costs CoordinateSpaceSurcharge
 * more), materials (the check costs CoordinateMaterialsSurcharge more).
 *
 * Failure meanings, diagnostic and never just "wrong answer":
 *   Overloaded: a member was given more work than their capacity.
 *   Mismatched: a task went to a member it does not suit (weaker work, but it joins).
 *   Uninformed: a critical fact was never exchanged, whatever was guessed.
 *   Unsettled: the constraint was left, or settled with a resolution that does not fit.
 *   Seam: the parts do not join and the hand-over was not checked.
 *   Wasted effort (the wasted time of the other structures): a conversation with a
 *     member who held nothing critical, or a check of a hand-over that had no seam.
 * Unsound = Overloaded, Uninformed, Unsettled or Seam; Partial = Mismatched or
 * Wasted effort only; otherwise Sound. Learning is the activity's normal allowance in
 * every case.
 */

enum class CoordinateGoal : std::uint8_t
{
    OrganiseWork,
    HearEveryone,
    SettleConstraint,
    SubmitCleanly
};
inline constexpr std::array<std::string_view, 4> CoordinateGoalNames{"organise_work", "hear_everyone", "settle_constraint", "submit_cleanly"};

enum class CoordinateBeat : std::uint8_t
{
    Assign,
    Exchange,
    Settle,
    Submit
};
inline constexpr std::array<std::string_view, 4> CoordinateBeatNames{"assign", "exchange", "settle", "submit"};

/** The player's seat in the group. Not documented as a closed set; chosen for this structure. */
enum class CoordinateRole : std::uint8_t
{
    Chair,
    Mediator,
    Treasurer,
    Scribe
};
inline constexpr std::array<std::string_view, 4> CoordinateRoleNames{"chair", "mediator", "treasurer", "scribe"};

enum class CoordinateScarce : std::uint8_t
{
    Time,
    Attention,
    Space,
    Materials
};
inline constexpr std::array<std::string_view, 4> CoordinateScarceNames{"time", "attention", "space", "materials"};

/** What binds the group, the case's varied unknown. */
enum class CoordinateConstraint : std::uint8_t
{
    Capacity,
    Clash,
    Budget,
    Deadline
};
inline constexpr std::array<std::string_view, 4> CoordinateConstraintNames{"capacity", "clash", "budget", "deadline"};
inline constexpr std::array<std::string_view, 4> CoordinateSubjectNames{"projects", "debate", "events", "practicals"};

/** None is "not yet chosen" and never a choice. */
enum class CoordinateResolution : std::uint8_t
{
    None,
    Leave,
    Rebalance,
    Reschedule,
    TrimScope,
    AskExtension
};
inline constexpr std::array<std::string_view, 6> CoordinateResolutionNames{"none", "leave", "rebalance", "reschedule", "trim_scope", "ask_extension"};

enum class CoordinateActionKind : std::uint8_t
{
    AssignWork,
    Exchange,
    EndExchange,
    Settle,
    Submit
};
inline constexpr std::array<std::string_view, 5> CoordinateDecisionNames{"assign_work", "exchange", "end_exchange", "settle", "submit"};

inline constexpr std::uint8_t CoordinateFailOverloaded = 1;
inline constexpr std::uint8_t CoordinateFailMismatched = 2;
inline constexpr std::uint8_t CoordinateFailUninformed = 4;
inline constexpr std::uint8_t CoordinateFailUnsettled = 8;
inline constexpr std::uint8_t CoordinateFailSeam = 16;
inline constexpr std::uint8_t CoordinateFailWasted = 32;
inline constexpr std::array<std::string_view, 6> CoordinateFailureNames{"overloaded_member", "mismatched_task", "uninformed_group",
                                                                         "unsettled_constraint", "seam_in_submission", "wasted_effort"};

// ---- Chosen numbers (the docs fix the shape, not these) ----------------------------

/** Not documented: assigning takes one unit with the method, three without. */
inline constexpr std::int32_t CoordinateAssignKnownUnits = 1;
inline constexpr std::int32_t CoordinateAssignDeriveUnits = 3;
/** Not documented: one conversation, and the surcharge when attention is scarce. */
inline constexpr std::int32_t CoordinateExchangeUnits = 1;
inline constexpr std::int32_t CoordinateAttentionSurcharge = 1;
/** Not documented: settling a constraint, and the surcharge when space is scarce. */
inline constexpr std::int32_t CoordinateSettleUnits = 1;
inline constexpr std::int32_t CoordinateSpaceSurcharge = 1;
/** Not documented: checking the hand-over, and the surcharge when materials are scarce. */
inline constexpr std::int32_t CoordinateCheckUnits = 1;
inline constexpr std::int32_t CoordinateMaterialsSurcharge = 1;

// ---- The catalogue ----------------------------------------------------------------

struct CoordinateMember
{
    std::string Id;
    std::string Name;
    /** The load this member can carry. */
    std::int32_t Capacity = 1;
    /** True for the player's own seat. */
    bool Self = false;
};

struct CoordinateTask
{
    std::string Id;
    std::string Name;
    std::int32_t Load = 1;
    /** The members who suit this task. */
    std::vector<std::string> Fits;
};

struct CoordinateFact
{
    std::string Id;
    /** The member who knows it. */
    std::string Holder;
    std::string Text;
    /** True for a fact that reveals the constraint. The player's interface never shows this. */
    bool Critical = false;
};

struct CoordinateAssignment
{
    std::string Task;
    std::string Member;

    friend bool operator==(const CoordinateAssignment&, const CoordinateAssignment&) = default;
};

struct CoordinateCaseDef : StructureCaseBase
{
    CoordinateRole Role = CoordinateRole::Chair;
    CoordinateScarce Scarce = CoordinateScarce::Time;
    /** What binds the group. Hidden until the critical facts are exchanged. */
    CoordinateConstraint Constraint = CoordinateConstraint::Capacity;
    /** True when the parts do not join and only a check of the hand-over finds it. */
    bool Seam = false;
    std::vector<CoordinateMember> Members;
    std::vector<CoordinateTask> Tasks;
    std::vector<CoordinateFact> Facts;
};

using CoordinateFamilyDef = StructureFamilyDef<CoordinateCaseDef>;
using CoordinateCatalogue = StructureCatalogue<CoordinateCaseDef>;
using CoordinateCatalogueResult = StructureCatalogueResult<CoordinateCatalogue>;

/**
 * Reads every coordinate family and case. Refuses (never coerces) a case whose family
 * or method is missing, without exactly one self member, with a task nobody suits or
 * no sound assignment, with a critical fact held by the player or nobody, or that
 * cannot be completed Sound within its budget by a character with no method under
 * any goal.
 */
[[nodiscard]] PERFECTWORLDCORE_API CoordinateCatalogueResult LoadCoordinateCatalogue(const ContentDb& content);

// ---- Reading the case -------------------------------------------------------------

/** The resolution that settles a constraint. */
[[nodiscard]] PERFECTWORLDCORE_API CoordinateResolution CoordinateMatchingResolution(CoordinateConstraint constraint) noexcept;
/** The first Sound assignment in authored order (each task to a member who suits it, nobody over capacity), or empty if none. */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<CoordinateAssignment> CoordinateSoundAssignment(const CoordinateCaseDef& def);
/** The members who hold at least one critical fact, in authored order. */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<std::string> CoordinateCriticalHolders(const CoordinateCaseDef& def);
/** The facts the player knows after talking to `heard`: their own and those held by the members spoken to. */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<std::string> CoordinateKnownFacts(const CoordinateCaseDef& def, const std::vector<std::string>& heard);

// ---- Costs ------------------------------------------------------------------------

/** The units a period holds when `scarce` is the case's scarce resource. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t CoordinateBudgetUnits(CoordinateScarce scarce) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t CoordinateAssignCost(const CoordinateCaseDef& def, CoordinateGoal goal, bool methodKnown) noexcept;
/** What one conversation costs. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t CoordinateExchangeCost(const CoordinateCaseDef& def, CoordinateGoal goal) noexcept;
/** What a resolution costs. Leaving the constraint is free. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t CoordinateSettleCost(const CoordinateCaseDef& def, CoordinateGoal goal,
                                                                     CoordinateResolution resolution) noexcept;
/** What submitting costs: nothing as it stands, the check's price when the hand-over is checked first. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t CoordinateSubmitCost(const CoordinateCaseDef& def, CoordinateGoal goal, bool checked) noexcept;

// ---- State ------------------------------------------------------------------------

/** The exposure in progress. Idle when CaseId is empty. */
struct CoordinateExposure : StructureExposureBase
{
    CoordinateGoal Goal = CoordinateGoal::OrganiseWork;
    CoordinateBeat Beat = CoordinateBeat::Assign;
    /** The committed assignment, sorted by task, once made. */
    std::vector<CoordinateAssignment> Assignment;
    /** The members spoken to, sorted. */
    std::vector<std::string> Heard;
    CoordinateResolution Resolution = CoordinateResolution::None;

    friend bool operator==(const CoordinateExposure&, const CoordinateExposure&) = default;
};

using CoordinateState = StructureState<CoordinateExposure>;

/** True when the state is internally consistent (sorted, bounded, beat and fields agree, secure never ahead of the counts). */
[[nodiscard]] PERFECTWORLDCORE_API bool IsCoordinateStateValid(const CoordinateState& state);
/** True when the family has been mastered. */
[[nodiscard]] PERFECTWORLDCORE_API bool CoordinateFamilySecure(const CoordinateState& state, std::string_view familyId) noexcept;

// ---- Selection and starting -------------------------------------------------------

/** The next case for `familyId`, deterministic and never one of the recent ones. Changes nothing. */
[[nodiscard]] PERFECTWORLDCORE_API StructureSelection SelectCoordinateCase(const CoordinateCatalogue& catalogue, const CoordinateState& state,
                                                                           std::string_view familyId, std::uint64_t worldSeed, GameInstant at);

/** Presents `caseId` with the goal the player chose; `skillMilli` fixes whether the method is practised. Refused while an exposure runs. */
PERFECTWORLDCORE_API StructureBeginResult BeginCoordinateExposure(CoordinateState& state, const CoordinateCatalogue& catalogue,
                                                                  std::string_view caseId, CoordinateGoal goal, std::int32_t skillMilli);

/** Abandons the running exposure: no outcome, no mastery change, and the case stays seen. */
PERFECTWORLDCORE_API StructureError CancelCoordinateExposure(CoordinateState& state);

// ---- Playing ----------------------------------------------------------------------

/** One decision, as plain data. Only the fields of its kind are read. */
struct CoordinateAction
{
    CoordinateActionKind Kind = CoordinateActionKind::AssignWork;
    /** AssignWork: one entry for every task of the case. */
    std::vector<CoordinateAssignment> Assignment;
    /** Exchange: the member to talk to. */
    std::string Member;
    CoordinateResolution Resolution = CoordinateResolution::Leave;
    /** Submit: check the hand-over first. */
    bool Check = false;

    friend bool operator==(const CoordinateAction&, const CoordinateAction&) = default;
};

/** The outcome of a finished exposure, with what the grade rests on. */
struct CoordinateOutcome : StructureOutcome
{
    std::int32_t OverloadedMembers = 0;
    std::int32_t MismatchedTasks = 0;
    /** Critical facts never exchanged. */
    std::int32_t MissedFacts = 0;
    /** Conversations with a member who held nothing critical. */
    std::int32_t IdleConversations = 0;

    friend bool operator==(const CoordinateOutcome&, const CoordinateOutcome&) = default;
};

using CoordinateStepResult = StructureStepResult<CoordinateOutcome>;

/**
 * What `action` would cost, before it is committed. Changes nothing. AssignWork with
 * no assignment quotes its price without checking a choice, so an interface can show
 * every price before anything is chosen.
 */
[[nodiscard]] PERFECTWORLDCORE_API StructureQuote QuoteCoordinateAction(const CoordinateState& state, const CoordinateCatalogue& catalogue,
                                                                        const CoordinateAction& action);

/** Applies one decision. Submitting ends the exposure and records its outcome once. A refusal changes nothing. */
PERFECTWORLDCORE_API CoordinateStepResult ApplyCoordinateAction(CoordinateState& state, const CoordinateCatalogue& catalogue,
                                                                const CoordinateAction& action);

/**
 * The next decision of the standard careful policy: the first Sound assignment, a
 * conversation with each member who holds a critical fact, the matching resolution,
 * and a check of the hand-over only when the case has a seam. It always earns a Sound
 * result inside the budget, and it is what routine abbreviation is equal to.
 */
[[nodiscard]] PERFECTWORLDCORE_API CoordinateAction CoordinateStandardAction(const CoordinateCaseDef& def, const CoordinateExposure& exposure);

/** Resolves one exposure of a secure family without manual play. See StructureCommon.h for the trade-off. */
PERFECTWORLDCORE_API StructureRoutineResult ResolveCoordinateRoutine(CoordinateState& state, const CoordinateCatalogue& catalogue,
                                                                     std::string_view familyId);

// ---- Text -------------------------------------------------------------------------

/**
 * The state as text, integers and stable IDs only, one record per line:
 *   pw.structure_coordinate/1 / families=N / (family=... / seen=... / recent=...) ... /
 *   exposure=- or <family>,<case>,<goal>,<beat>,<total>,<left>,<known> /
 *   assign=<task>:<member>,... or - / heard=<member>,... or - / resolution=<name> / end
 * Each line ends in a newline and the same state always gives the same text. Empty
 * for an inconsistent state.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseStructureCoordinate(const CoordinateState& state);

struct CoordinateParse
{
    /** Empty on success; otherwise why the text was refused. */
    std::string Error;
    CoordinateState State;

    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/** Reads SerialiseStructureCoordinate's text. Refuses truncation, extra text, malformed numbers and inconsistent state. */
[[nodiscard]] PERFECTWORLDCORE_API CoordinateParse DeserialiseStructureCoordinate(std::string_view text);
} // namespace PerfectWorld
