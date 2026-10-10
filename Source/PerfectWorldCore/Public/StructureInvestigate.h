#pragma once

#include "StructureCommon.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace PerfectWorld
{
/**
 * The "investigate a fault" structure and its classroom family (ROADMAP M2-09;
 * SCHOOL_AND_PROGRESSION "Four interaction structures": "Choose tests with different
 * costs; eliminate causes", for computing, practical science and repair;
 * ALTERNATE_WORLD: "Four possible causes; you have time to test two before the 14:00
 * submission"; "A misconception left unresolved reappears"; SYSTEMS_CATALOGUE "The
 * engines ... Fault"; "Never invalidate a correct committed solution through a
 * hidden stat roll. Always state the cost before commitment").
 *
 * A pure, integer-only runner for one exposure over a plain state (InvestigateState),
 * with the conventions of StructureCommon.h. Not yet a member of WorldState, the
 * reducer or the save.
 *
 * Content: investigate_family and investigate_case records (Data/Content). A case has
 * a set of candidate causes, exactly one of them the real fault (the player's
 * interface never shows which), and a set of tests. A test has a cost in units, a
 * kind (observe, measure, swap a part) and the causes it implicates: it reads
 * positive exactly when the fault is among them, which eliminates every cause outside
 * the implicated set; negative eliminates the implicated ones. The player sees the
 * result as soon as a test is run, so the choice of the next test is adaptive.
 *
 * One exposure has two beats, not three. The first, isolate, is a loop: run any
 * number of different tests the units allow, each at its quoted price (the structure
 * has no fixed order, only a budget: "you have time to test two"). The second,
 * conclude, is one decision that ends the exposure: diagnose a cause, or report
 * honestly that the fault is not isolated. Reporting is free, so no exposure strands.
 *
 * Prices. Each test has an authored base. A character without the case's method pays
 * InvestigateUnfamiliarUnits more for a measure or swap test (anyone can look; the
 * instrument and the spanner need showing). A scarce resource adds
 * InvestigateScarceSurcharge to the matching kind: equipment to measure, parts to
 * swap, access to observe; time shortens the period to the scarce-time units. A role
 * or the goal that matches a test's kind takes InvestigateDiscountUnits off it (not
 * stacked, never below InvestigateMinTestUnits, so a test is never free and the
 * number of tests is bounded by the budget). The critic's honest report is never
 * called abandonment.
 *
 * Failure meanings, diagnostic and never just "wrong answer":
 *   WrongCause: the diagnosis names a cause that is not the fault.
 *   Unsupported: the diagnosis was made while other causes still stood (a guess,
 *     right or wrong; the grade rests on the evidence, not on luck).
 *   Redundant (wasted time): a test whose result was already settled by earlier ones.
 *   Abandoned: reporting "not isolated" when the tests had in fact isolated the
 *     fault (not for the critic, whose honest report is the job).
 * Unsound = WrongCause or Unsupported; Partial = Redundant or Abandoned only;
 * otherwise Sound. The honest report on an unfinished search is Sound: "I cannot say
 * yet" is a posture, as a hedge on untrusted readings is in the model structure.
 * Learning is the activity's normal allowance in every case.
 */

enum class InvestigateGoal : std::uint8_t
{
    Inspect,
    Measure,
    SwapParts
};
inline constexpr std::array<std::string_view, 3> InvestigateGoalNames{"inspect", "measure", "swap_parts"};

/** The two beats: a loop of tests, then one conclusion. The state needs no beat field: the exposure is isolating until it ends. */
inline constexpr std::array<std::string_view, 2> InvestigateBeatNames{"isolate", "conclude"};

enum class InvestigateTestKind : std::uint8_t
{
    Observe,
    Measure,
    Swap
};
inline constexpr std::array<std::string_view, 3> InvestigateTestKindNames{"observe", "measure", "swap"};

enum class InvestigateRole : std::uint8_t
{
    Inspector,
    Experimenter,
    Repairer,
    Critic
};
inline constexpr std::array<std::string_view, 4> InvestigateRoleNames{"inspector", "experimenter", "repairer", "critic"};

enum class InvestigateScarce : std::uint8_t
{
    Time,
    Equipment,
    Parts,
    Access
};
inline constexpr std::array<std::string_view, 4> InvestigateScarceNames{"time", "equipment", "parts", "access"};

/** How the fault shows itself, the case's varied unknown. */
enum class InvestigateSymptom : std::uint8_t
{
    WrongOutput,
    NoResponse,
    Intermittent
};
inline constexpr std::array<std::string_view, 3> InvestigateSymptomNames{"wrong_output", "no_response", "intermittent"};
inline constexpr std::array<std::string_view, 3> InvestigateSubjectNames{"computing", "practical_science", "repair"};

enum class InvestigateActionKind : std::uint8_t
{
    RunTest,
    Diagnose,
    ReportInconclusive
};
inline constexpr std::array<std::string_view, 3> InvestigateDecisionNames{"run_test", "diagnose", "report_inconclusive"};

inline constexpr std::uint8_t InvestigateFailWrongCause = 1;
inline constexpr std::uint8_t InvestigateFailUnsupported = 2;
inline constexpr std::uint8_t InvestigateFailRedundant = 4;
inline constexpr std::uint8_t InvestigateFailAbandoned = 8;
inline constexpr std::array<std::string_view, 4> InvestigateFailureNames{"wrong_cause", "unsupported_diagnosis", "redundant_test",
                                                                         "abandoned_search"};

// ---- Chosen numbers (the docs fix the shape, not these) ----------------------------

/** Not documented: the surcharge for an unfamiliar method on a measure or swap test, and for a scarce resource. */
inline constexpr std::int32_t InvestigateUnfamiliarUnits = 1;
inline constexpr std::int32_t InvestigateScarceSurcharge = 1;
/** Not documented: a test never costs less than this, so tests are bounded by the budget. */
inline constexpr std::int32_t InvestigateMinTestUnits = 1;
/** Not documented: a bound that keeps a state and its text finite. */
inline constexpr std::size_t InvestigateMaxTests = 8;

// ---- The catalogue ----------------------------------------------------------------

struct InvestigateCause
{
    std::string Id;
    std::string Name;
    /** True for the one real fault. The player's interface never shows this. */
    bool Fault = false;
};

struct InvestigateTest
{
    std::string Id;
    std::string Name;
    InvestigateTestKind Kind = InvestigateTestKind::Observe;
    /** The authored base price, 1 to 3. */
    std::int32_t Units = 1;
    /** The causes this test implicates: it reads positive exactly when the fault is among them. */
    std::vector<std::string> Implicates;
};

struct InvestigateCaseDef : StructureCaseBase
{
    InvestigateSymptom Symptom = InvestigateSymptom::WrongOutput;
    InvestigateRole Role = InvestigateRole::Inspector;
    InvestigateScarce Scarce = InvestigateScarce::Time;
    /** The candidate causes and the tests, in authored order. */
    std::vector<InvestigateCause> Causes;
    std::vector<InvestigateTest> Tests;
};

using InvestigateFamilyDef = StructureFamilyDef<InvestigateCaseDef>;
using InvestigateCatalogue = StructureCatalogue<InvestigateCaseDef>;
using InvestigateCatalogueResult = StructureCatalogueResult<InvestigateCatalogue>;

/**
 * Reads every investigate family and case. Refuses (never coerces) a case whose family
 * or method is missing, without exactly one fault, whose tests name unknown causes or
 * implicate none or all of them, or whose fault cannot be isolated within its budget
 * by a character with no method under any goal.
 */
[[nodiscard]] PERFECTWORLDCORE_API InvestigateCatalogueResult LoadInvestigateCatalogue(const ContentDb& content);

// ---- Reading the case -------------------------------------------------------------

/** The real fault, or null for a case with none. */
[[nodiscard]] PERFECTWORLDCORE_API const InvestigateCause* InvestigateFaultOf(const InvestigateCaseDef& def) noexcept;
/** What running `test` shows: true when the fault is among the causes it implicates. */
[[nodiscard]] PERFECTWORLDCORE_API bool InvestigateTestPositive(const InvestigateCaseDef& def, const InvestigateTest& test) noexcept;
/** The cause IDs still standing after the tests in `testsRun`, in authored order. Unknown test IDs are ignored. */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<std::string> InvestigateRemaining(const InvestigateCaseDef& def,
                                                                                 const std::vector<std::string>& testsRun);

// ---- Costs ------------------------------------------------------------------------

/** The units a period holds when `scarce` is the case's scarce resource. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t InvestigateBudgetUnits(InvestigateScarce scarce) noexcept;
/** What running `test` costs for a character who has the case's method or not. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t InvestigateTestCost(const InvestigateCaseDef& def, InvestigateGoal goal,
                                                                    const InvestigateTest& test, bool methodKnown) noexcept;

/** The cheapest further tests that isolate the fault. */
struct InvestigatePlan
{
    /** False when no further tests isolate it (or it is isolated already, in which case Tests is empty and Found is true). */
    bool Found = false;
    /** Test IDs in authored order. */
    std::vector<std::string> Tests;
    std::int32_t Units = 0;
};
/**
 * The cheapest set of tests not yet in `testsRun` that, with them, isolate the fault. Ties go to the
 * first set in a fixed enumeration (subsets of the unrun tests in authored order, counted upward by
 * binary value), so the plan is deterministic. An isolated fault gives Found with no tests.
 */
[[nodiscard]] PERFECTWORLDCORE_API InvestigatePlan InvestigatePlanFor(const InvestigateCaseDef& def, InvestigateGoal goal, bool methodKnown,
                                                                      const std::vector<std::string>& testsRun);

// ---- State ------------------------------------------------------------------------

/** The exposure in progress. Idle when CaseId is empty. */
struct InvestigateExposure : StructureExposureBase
{
    InvestigateGoal Goal = InvestigateGoal::Inspect;
    /** The tests run, in the order they were run. */
    std::vector<std::string> TestsRun;

    friend bool operator==(const InvestigateExposure&, const InvestigateExposure&) = default;
};

using InvestigateState = StructureState<InvestigateExposure>;

/** True when the state is internally consistent (sorted, bounded, tests unique, secure never ahead of the counts). */
[[nodiscard]] PERFECTWORLDCORE_API bool IsInvestigateStateValid(const InvestigateState& state);
/** True when the family has been mastered. */
[[nodiscard]] PERFECTWORLDCORE_API bool InvestigateFamilySecure(const InvestigateState& state, std::string_view familyId) noexcept;

// ---- Selection and starting -------------------------------------------------------

/** The next case for `familyId`, deterministic and never one of the recent ones. Changes nothing. */
[[nodiscard]] PERFECTWORLDCORE_API StructureSelection SelectInvestigateCase(const InvestigateCatalogue& catalogue, const InvestigateState& state,
                                                                            std::string_view familyId, std::uint64_t worldSeed, GameInstant at);

/** Presents `caseId` with the goal the player chose; `skillMilli` fixes whether the method is practised. Refused while an exposure runs. */
PERFECTWORLDCORE_API StructureBeginResult BeginInvestigateExposure(InvestigateState& state, const InvestigateCatalogue& catalogue,
                                                                   std::string_view caseId, InvestigateGoal goal, std::int32_t skillMilli);

/** Abandons the running exposure: no outcome, no mastery change, and the case stays seen. */
PERFECTWORLDCORE_API StructureError CancelInvestigateExposure(InvestigateState& state);

// ---- Playing ----------------------------------------------------------------------

/** One decision, as plain data. Only the fields of its kind are read. */
struct InvestigateAction
{
    InvestigateActionKind Kind = InvestigateActionKind::RunTest;
    /** RunTest: the test ID. */
    std::string Test;
    /** Diagnose: the cause ID. */
    std::string Cause;

    friend bool operator==(const InvestigateAction&, const InvestigateAction&) = default;
};

/** The outcome of a finished exposure, with what the grade rests on. */
struct InvestigateOutcome : StructureOutcome
{
    /** The cause diagnosed; empty for an honest report. */
    std::string Diagnosed;
    std::int32_t TestsRun = 0;
    std::int32_t RedundantTests = 0;
    /** Causes still standing when the exposure ended. */
    std::int32_t RemainingCauses = 0;

    friend bool operator==(const InvestigateOutcome&, const InvestigateOutcome&) = default;
};

using InvestigateStepResult = StructureStepResult<InvestigateOutcome>;

/** What `action` would cost, before it is committed. Changes nothing. */
[[nodiscard]] PERFECTWORLDCORE_API StructureQuote QuoteInvestigateAction(const InvestigateState& state, const InvestigateCatalogue& catalogue,
                                                                         const InvestigateAction& action);

/** Applies one decision. Diagnosing or reporting ends the exposure and records its outcome once. A refusal changes nothing. */
PERFECTWORLDCORE_API InvestigateStepResult ApplyInvestigateAction(InvestigateState& state, const InvestigateCatalogue& catalogue,
                                                                  const InvestigateAction& action);

/**
 * The next decision of the standard careful policy: run the next test of the cheapest
 * isolating plan (InvestigatePlanFor), then diagnose the fault; if no plan fits the
 * units left, report honestly. From the start of any authored case it earns a Sound
 * result inside the budget, and it is what routine abbreviation is equal to.
 */
[[nodiscard]] PERFECTWORLDCORE_API InvestigateAction InvestigateStandardAction(const InvestigateCaseDef& def, const InvestigateExposure& exposure);

/** Resolves one exposure of a secure family without manual play. See StructureCommon.h for the trade-off. */
PERFECTWORLDCORE_API StructureRoutineResult ResolveInvestigateRoutine(InvestigateState& state, const InvestigateCatalogue& catalogue,
                                                                      std::string_view familyId);

// ---- Text -------------------------------------------------------------------------

/**
 * The state as text, integers and stable IDs only, one record per line:
 *   pw.structure_investigate/1 / families=N / (family=... / seen=... / recent=...) ... /
 *   exposure=- or <family>,<case>,<goal>,<total>,<left>,<known> / tests=<id>,... or - / end
 * The tests are listed in the order they were run. Each line ends in a newline and
 * the same state always gives the same text. Empty for an inconsistent state.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseStructureInvestigate(const InvestigateState& state);

struct InvestigateParse
{
    /** Empty on success; otherwise why the text was refused. */
    std::string Error;
    InvestigateState State;

    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/** Reads SerialiseStructureInvestigate's text. Refuses truncation, extra text, malformed numbers and inconsistent state. */
[[nodiscard]] PERFECTWORLDCORE_API InvestigateParse DeserialiseStructureInvestigate(std::string_view text);
} // namespace PerfectWorld
