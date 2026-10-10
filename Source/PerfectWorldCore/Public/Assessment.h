#pragma once

#include "ContentRegistry.h"
#include "StructureCoordinate.h"
#include "StructureExplain.h"
#include "StructureInvestigate.h"
#include "StructureModel.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * Assessments: surveying the paper and diagnostic marking (ROADMAP M3-07;
 * SCHOOL_AND_PROGRESSION "Tests and assessments", "The academic loop", "Time economy";
 * SYSTEMS_CATALOGUE "School and education": commands assessment.survey and .order, event
 * AssessmentMarked{diagnostics}).
 *
 * A pure, integer-only runner for one written paper over a plain state (AssessmentState).
 * It reads no clock, owns no stats and is not yet a member of WorldState, the reducer, the
 * day loop or WorldSave; the integration step passes the skill levels in at the start.
 *
 * Content. A paper is an `assessment_paper` record (Data/Content/assessment_paper, checked
 * by ContentAssessmentSchemas.cpp and read by LoadAssessmentCatalogue). It names the
 * calendar assessment window it is sat in (SchoolCalendar.h) and five question groups,
 * each one question pointing at an existing case of one of the four structures, so the
 * examination re-asks what the classroom practised.
 *
 * Time economy (documented). "A 90-minute paper is eighteen five-minute units. Five groups
 * worth twenty marks each. A practiced method clears a routine group in two units;
 * deriving an unfamiliar one takes four; verification costs one. A prepared character
 * clears and checks everything. An unprepared one must triage." The constants below carry
 * those numbers; a prepared character's paper costs the survey plus five times three
 * units, and an unprepared one's costs more than the eighteen it has.
 *
 * The academic loop, as commands:
 *   1. Survey the paper (SurveyAssessment). A documented step ("five question groups
 *      with marks, familiarity and estimated work. The character supplies the estimate,
 *      with honest uncertainty") but not priced; chosen: AssessmentSurveyUnits. It is
 *      optional and shows, for every group, the familiarity of the method (practised, shaky,
 *      unfamiliar) and an estimated range of the units to clear it. The range always
 *      contains the true price and is wide only where the character cannot tell.
 *   2. Choose order and ambition: allocate the units across the groups
 *      (AllocateAssessmentUnits) and answer the groups in any order. An allocation is a
 *      promise of time and the most its group may spend; the unspent part returns to the
 *      pool when the group is answered. Promises never exceed the units that remain, and
 *      the survey is paid from the units not promised.
 *   3. Execute (ApplyAssessmentAnswer). One answer settles a group. The abbreviated
 *      resolution (the full structure runners play a 45-minute period, not a group of a
 *      paper) has four decisions:
 *        Mode Work: clear the group by the structure's own method, AssessmentClearKnownUnits
 *          if the method is practised, AssessmentClearDeriveUnits if not. The core answer
 *          is then sound; a stat never invalidates it.
 *        Mode Guess: put something down for AssessmentGuessUnits. The core answer is wrong.
 *        Confirm: re-examine the answer, AssessmentVerifyUnits more. It finds what the case hides
 *          (its trap, below). On a case that hides nothing it finds nothing and costs only
 *          its unit: time is the whole price, so a prepared character confirms everything.
 *        Hedge: bound the claim instead of committing to it. Free. It covers a trap that
 *          was not confirmed; on a clean case, or one already confirmed, it wastes the
 *          answer (and is never wasted for the critic, whose job it is).
 *      Every price is quoted before it is committed (QuoteAssessmentAnswer) and charged
 *      exactly. The only refusals are the player's own spent time (the allocation, the
 *      units) and malformed choices.
 *   4. Diagnostic marking (MarkAssessment, after FinishAssessment). Per question: the
 *      failure kinds that occurred, a grade and the marks awarded. Marking is a pure
 *      function of the paper and the state.
 *
 * The trap of a case is what Confirm would find, from the structure's own content:
 *   model       a hidden fault behind the discrepancy (ModelFault is not None);
 *   explain     the limitation every defensible claim needs (always);
 *   investigate an intermittent symptom, so one clearing proves nothing;
 *   coordinate  a seam in the hand-over (the case's seam flag).
 * A group that is neither confirmed nor hedged on a trap carries that structure's own
 * "claimed too much" failure.
 *
 * Failure kinds. SCHOOL_AND_PROGRESSION "What varies on the fortieth class" names four:
 * a wrong model, an unsupported conclusion, wasted time and an unclear explanation. Each
 * structure has its own failure bits (ModelFail*, ExplainFail*, InvestigateFail*,
 * CoordinateFail*); AssessmentKindsOf maps them, and the abbreviated resolution produces
 * each structure's own bits (AssessmentNativeFailures) and reports both:
 *   wrong model         model WrongModel; explain Indefensible; investigate WrongCause;
 *                       coordinate Overloaded and Mismatched.
 *   unsupported         model Unsupported; explain Unsupported; investigate Unsupported;
 *                       coordinate Uninformed and Unsettled.
 *   wasted time         model WastedTime; explain Padding; investigate Redundant and
 *                       Abandoned; coordinate Wasted. (A paper makes them by a needless
 *                       hedge, as Abandoned for investigate; Redundant is for real exposures.)
 *   unclear explanation model Unclear; explain Overreach (a claim with no stated limit);
 *                       coordinate Seam (the parts do not join).
 * A group never reached is unfinished work, reported as wasted time with no native bits.
 * (Step 6 of "The academic loop" names a missing concept, a misapplied method, an
 * unsupported claim and unfinished work as four next actions: the same failures read as
 * advice, left to the interface.)
 *
 * Grades (chosen fractions): Unsound (a wrong model or an unsupported conclusion) earns a
 * quarter of the group's marks, Partial (only wasted time or an unclear explanation) three
 * quarters, Sound all, a group never reached none.
 *
 * Familiarity is fixed when the paper begins, from the character's skill, so a quote never
 * moves ("Always state the cost before commitment"). SerialiseAssessment saves the state.
 */

// ---- Documented numbers ---------------------------------------------------------------

inline constexpr std::int32_t AssessmentPaperMinutes = 90;
inline constexpr std::int32_t AssessmentUnitMinutes = ModelUnitMinutes;
inline constexpr std::int32_t AssessmentPaperUnits = AssessmentPaperMinutes / AssessmentUnitMinutes;
inline constexpr std::size_t AssessmentGroupCount = 5;
inline constexpr std::int32_t AssessmentGroupMarks = 20;
inline constexpr std::int32_t AssessmentPaperMarks = AssessmentGroupMarks * static_cast<std::int32_t>(AssessmentGroupCount);
/** A practised method clears a group in two units, deriving an unfamiliar one takes four. */
inline constexpr std::int32_t AssessmentClearKnownUnits = ModelCommitKnownUnits;
inline constexpr std::int32_t AssessmentClearDeriveUnits = ModelCommitDeriveUnits;
/** Verification costs one. */
inline constexpr std::int32_t AssessmentVerifyUnits = 1;

// ---- Chosen numbers (the docs fix the shape, not these) -------------------------------

/** Not documented: surveying the paper costs one unit, so a prepared character still has two spare. */
inline constexpr std::int32_t AssessmentSurveyUnits = 1;
/** Not documented: putting down a guess costs one unit. */
inline constexpr std::int32_t AssessmentGuessUnits = 1;
/** Not documented: a skill within this many milli-XP below a method's threshold is shaky: the character cannot tell if it is known. */
inline constexpr std::int32_t AssessmentShakyMilli = 10000;
/** Not documented: the share of a group's marks, in parts per thousand, by grade. */
inline constexpr std::int32_t AssessmentSoundPermille = 1000;
inline constexpr std::int32_t AssessmentPartialPermille = 750;
inline constexpr std::int32_t AssessmentUnsoundPermille = 250;

// ---- Names and kinds ------------------------------------------------------------------

/** The structure a question belongs to; the names are the `engine` of the structure records. */
enum class AssessmentStructure : std::uint8_t
{
    Model,
    Explain,
    Investigate,
    Coordinate
};
inline constexpr std::array<std::string_view, 4> AssessmentStructureNames{"model", "explain", "investigate", "coordinate"};
/** The content record type of each structure's cases. */
inline constexpr std::array<std::string_view, 4> AssessmentCaseTypes{"problem_case", "explain_case", "investigate_case", "coordinate_case"};

/** The four kinds of failure, a bit set in AssessmentGroupMark::Failures. */
inline constexpr std::uint8_t AssessmentFailWrongModel = 1;
inline constexpr std::uint8_t AssessmentFailUnsupported = 2;
inline constexpr std::uint8_t AssessmentFailWastedTime = 4;
inline constexpr std::uint8_t AssessmentFailUnclear = 8;
inline constexpr std::array<std::string_view, 4> AssessmentFailureNames{"wrong_model", "unsupported_conclusion", "wasted_time",
                                                                        "unclear_explanation"};

/**
 * The four kinds a structure's own failure bits stand for (the table in the header comment).
 * `nativeFailures` is a bit set of ModelFail*, ExplainFail*, InvestigateFail* or CoordinateFail*
 * according to `structure`; bits the structure does not define are ignored.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::uint8_t AssessmentKindsOf(AssessmentStructure structure, std::uint8_t nativeFailures) noexcept;

enum class AssessmentMode : std::uint8_t
{
    Work,
    Guess
};
inline constexpr std::array<std::string_view, 2> AssessmentModeNames{"work", "guess"};

enum class AssessmentFamiliarity : std::uint8_t
{
    Practised,
    Shaky,
    Unfamiliar
};
inline constexpr std::array<std::string_view, 3> AssessmentFamiliarityNames{"practised", "shaky", "unfamiliar"};

enum class AssessmentGrade : std::uint8_t
{
    Sound,
    Partial,
    Unsound,
    /** The group was never answered. */
    Unattempted
};
inline constexpr std::array<std::string_view, 4> AssessmentGradeNames{"sound", "partial", "unsound", "unattempted"};

enum class AssessmentError : std::uint8_t
{
    None = 0,
    UnknownPaper,
    /** A paper is already being sat. */
    Busy,
    NotActive,
    /** The paper has been handed in. */
    Finished,
    NotFinished,
    AlreadySurveyed,
    UnknownGroup,
    AlreadyAnswered,
    /** An enum outside its range. */
    BadChoice,
    /** The units not allocated cannot pay (the survey, a new allocation). */
    UnitsShort,
    /** The answer costs more than the group's allocation. */
    AllocationShort,
    BadAllocation,
    InvalidSkill,
    InvalidState
};

/** The stable rejection code, for example "assessment.allocation_short". Empty for None. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view AssessmentErrorCode(AssessmentError error) noexcept;

// ---- The catalogue ---------------------------------------------------------------------

/** One question group of a paper, with the facts of its case that marking needs. */
struct AssessmentQuestion
{
    std::string GroupId;
    std::string Name;
    std::int32_t Marks = AssessmentGroupMarks;
    AssessmentStructure Structure = AssessmentStructure::Model;
    std::string CaseId;
    std::string FamilyId;
    /** The skill the family trains and the level (milli-XP) at which the case's method is known. */
    std::string SkillId;
    std::int32_t MethodKnownAtMilli = 0;
    /** What Confirm would find (see the header comment). */
    bool Trap = false;
    /** True when the player's seat is the critic's: a hedge is then never wasted time. */
    bool Critic = false;

    friend bool operator==(const AssessmentQuestion&, const AssessmentQuestion&) = default;
};

struct AssessmentPaperDef
{
    std::string Id;
    std::string Name;
    /** The calendar and the assessment window of that calendar the paper is sat in. */
    std::string CalendarId;
    std::string WindowId;
    std::int32_t Units = AssessmentPaperUnits;
    std::vector<AssessmentQuestion> Questions;

    friend bool operator==(const AssessmentPaperDef&, const AssessmentPaperDef&) = default;
};

/** The structure catalogues the questions are read from. All four are needed for a paper that uses all four. */
struct AssessmentStructures
{
    const ModelCatalogue* Model = nullptr;
    const ExplainCatalogue* Explain = nullptr;
    const InvestigateCatalogue* Investigate = nullptr;
    const CoordinateCatalogue* Coordinate = nullptr;
};

struct AssessmentCatalogue
{
    /** Sorted by Id. */
    std::vector<AssessmentPaperDef> Papers;

    [[nodiscard]] PERFECTWORLDCORE_API const AssessmentPaperDef* FindPaper(std::string_view id) const noexcept;
};

struct AssessmentCatalogueResult
{
    /** Empty on success; otherwise the record and what is wrong with it. */
    std::string Error;
    AssessmentCatalogue Catalogue;

    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/**
 * Reads every assessment_paper record against the structure catalogues. Refuses (never
 * coerces) a paper that is not 18 units in five groups of 20 marks, or whose case is
 * missing from the catalogue of its structure.
 */
[[nodiscard]] PERFECTWORLDCORE_API AssessmentCatalogueResult LoadAssessmentCatalogue(const ContentDb& content,
                                                                                     const AssessmentStructures& structures);

// ---- State -----------------------------------------------------------------------------

/** What the character brings: skill levels in milli-XP; a skill not listed is zero. */
struct AssessmentSkillLevel
{
    std::string SkillId;
    std::int32_t Milli = 0;

    friend bool operator==(const AssessmentSkillLevel&, const AssessmentSkillLevel&) = default;
};
using AssessmentSkills = std::vector<AssessmentSkillLevel>;

struct AssessmentGroupState
{
    /** Fixed when the paper begins: the method is practised, or is within AssessmentShakyMilli of being so. */
    bool Known = false;
    bool Shaky = false;
    /** The most this group may spend. */
    std::int32_t Allocated = 0;
    bool Answered = false;
    /** The answer, once given. */
    AssessmentMode Mode = AssessmentMode::Work;
    bool Confirm = false;
    bool Hedge = false;
    std::int32_t Spent = 0;

    friend bool operator==(const AssessmentGroupState&, const AssessmentGroupState&) = default;
};

struct AssessmentState
{
    /** The paper being (or just) sat; empty when idle. */
    std::string PaperId;
    bool Surveyed = false;
    /** Handed in: no more commands, and the paper can be marked. */
    bool Finished = false;
    std::int32_t UnitsLeft = 0;
    std::vector<AssessmentGroupState> Groups;

    [[nodiscard]] bool Active() const noexcept { return !PaperId.empty() && !Finished; }

    friend bool operator==(const AssessmentState&, const AssessmentState&) = default;
};

/** True when the state is idle, or internally consistent (units add up, allocations fit, answers are priced). */
[[nodiscard]] PERFECTWORLDCORE_API bool IsAssessmentStateValid(const AssessmentState& state);

/** The units a group's answer would cost: the clearing by mode and familiarity, plus the confirmation. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t AssessmentAnswerUnits(bool methodKnown, AssessmentMode mode, bool confirm) noexcept;
/** The units of the paper not promised to an unanswered group. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t AssessmentUnallocated(const AssessmentState& state) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API AssessmentFamiliarity AssessmentFamiliarityOf(const AssessmentGroupState& group) noexcept;
/** The units to clear a group, as the character estimates them: low and high, always containing the true price. */
[[nodiscard]] PERFECTWORLDCORE_API std::pair<std::int32_t, std::int32_t> AssessmentEstimate(AssessmentFamiliarity familiarity) noexcept;

/** Starts `paper` for a character with `skills`. Refused while a paper is active; a finished paper is replaced. */
PERFECTWORLDCORE_API AssessmentError BeginAssessment(AssessmentState& state, const AssessmentPaperDef& paper, const AssessmentSkills& skills);
/** Abandons the paper (active or finished) and returns to idle. Nothing is marked. */
PERFECTWORLDCORE_API AssessmentError CancelAssessment(AssessmentState& state);
/** Hands the paper in; groups not answered are unfinished. Marking is then possible. */
PERFECTWORLDCORE_API AssessmentError FinishAssessment(AssessmentState& state);

// ---- Surveying -------------------------------------------------------------------------

struct AssessmentQuote
{
    /** None when the command is legal now, else why not (a short refusal still carries Units). */
    AssessmentError Error = AssessmentError::None;
    std::int32_t Units = 0;
    std::int32_t UnitsAfter = 0;

    [[nodiscard]] bool Ok() const noexcept { return Error == AssessmentError::None; }
    friend bool operator==(const AssessmentQuote&, const AssessmentQuote&) = default;
};

/** One group as the survey shows it. */
struct AssessmentSurveyLine
{
    std::size_t Group = 0;
    std::string GroupId;
    std::int32_t Marks = 0;
    AssessmentStructure Structure = AssessmentStructure::Model;
    AssessmentFamiliarity Familiarity = AssessmentFamiliarity::Unfamiliar;
    std::int32_t EstimateLow = 0;
    std::int32_t EstimateHigh = 0;

    friend bool operator==(const AssessmentSurveyLine&, const AssessmentSurveyLine&) = default;
};

[[nodiscard]] PERFECTWORLDCORE_API AssessmentQuote QuoteAssessmentSurvey(const AssessmentState& state);

struct AssessmentSurveyResult
{
    AssessmentError Error = AssessmentError::None;
    /** The units charged, equal to the quote taken just before. */
    std::int32_t Charged = 0;
    std::vector<AssessmentSurveyLine> Lines;

    [[nodiscard]] bool Ok() const noexcept { return Error == AssessmentError::None; }
};

/** Pays for the survey and returns what it shows. A refusal changes nothing. */
PERFECTWORLDCORE_API AssessmentSurveyResult SurveyAssessment(AssessmentState& state, const AssessmentPaperDef& paper);
/** What the survey shows, free to read once it has been paid for; empty before. */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<AssessmentSurveyLine> AssessmentSurveyLines(const AssessmentState& state,
                                                                                         const AssessmentPaperDef& paper);

// ---- Allocating and answering ----------------------------------------------------------

/** Sets how many units an unanswered group may spend, replacing its earlier allocation. A refusal changes nothing. */
PERFECTWORLDCORE_API AssessmentError AllocateAssessmentUnits(AssessmentState& state, std::size_t group, std::int32_t units);

/** One group's answer, as plain data. */
struct AssessmentAnswer
{
    std::size_t Group = 0;
    AssessmentMode Mode = AssessmentMode::Work;
    bool Confirm = false;
    bool Hedge = false;

    friend bool operator==(const AssessmentAnswer&, const AssessmentAnswer&) = default;
};

/** What `answer` would cost, before it is committed. Changes nothing. */
[[nodiscard]] PERFECTWORLDCORE_API AssessmentQuote QuoteAssessmentAnswer(const AssessmentState& state, const AssessmentAnswer& answer);

struct AssessmentStepResult
{
    AssessmentError Error = AssessmentError::None;
    /** The units charged, equal to the quote taken just before. */
    std::int32_t Charged = 0;
    /** True when this was the last group and the paper is handed in. */
    bool Finished = false;

    [[nodiscard]] bool Ok() const noexcept { return Error == AssessmentError::None; }
};

/** Settles one group. Answering the last one hands the paper in. A refusal changes nothing. */
PERFECTWORLDCORE_API AssessmentStepResult ApplyAssessmentAnswer(AssessmentState& state, const AssessmentAnswer& answer);

// ---- Diagnostic marking ----------------------------------------------------------------

struct AssessmentGroupMark
{
    std::size_t Group = 0;
    std::string GroupId;
    AssessmentStructure Structure = AssessmentStructure::Model;
    std::string CaseId;
    std::int32_t Marks = 0;
    std::int32_t Awarded = 0;
    AssessmentGrade Grade = AssessmentGrade::Unattempted;
    /** The four kinds that occurred, a bit set of AssessmentFail*. */
    std::uint8_t Failures = 0;
    /** The structure's own failure bits behind them. */
    std::uint8_t Native = 0;
    AssessmentFamiliarity Familiarity = AssessmentFamiliarity::Unfamiliar;
    std::int32_t Allocated = 0;
    std::int32_t Spent = 0;

    friend bool operator==(const AssessmentGroupMark&, const AssessmentGroupMark&) = default;
};

struct AssessmentMarking
{
    AssessmentError Error = AssessmentError::None;
    std::vector<AssessmentGroupMark> Groups;
    std::int32_t Total = 0;
    std::int32_t MaxTotal = 0;
    std::int32_t UnitsSpent = 0;
    std::int32_t UnitsUnused = 0;

    [[nodiscard]] bool Ok() const noexcept { return Error == AssessmentError::None; }
    friend bool operator==(const AssessmentMarking&, const AssessmentMarking&) = default;
};

/** The structure's own failure bits for a group's answer (ModelFail*, ExplainFail*, InvestigateFail* or CoordinateFail*). */
[[nodiscard]] PERFECTWORLDCORE_API std::uint8_t AssessmentNativeFailures(const AssessmentQuestion& question, const AssessmentGroupState& group) noexcept;

/** Marks a finished paper. NotFinished while it is still being sat; InvalidState when the state does not fit the paper. */
[[nodiscard]] PERFECTWORLDCORE_API AssessmentMarking MarkAssessment(const AssessmentPaperDef& paper, const AssessmentState& state);

// ---- Text ------------------------------------------------------------------------------

/**
 * The state as text, integers and stable IDs only, one record per line:
 *   pw.assessment/1 / paper=<id> or - / (when a paper: state=<surveyed>,<finished>,<unitsLeft> /
 *   groups=N / group=<known>,<shaky>,<allocated>,<answered>,<mode>,<confirm>,<hedge>,<spent> ...) / end
 * Each line ends in a newline and the same state always gives the same text. Empty for an
 * inconsistent state.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseAssessment(const AssessmentState& state);

struct AssessmentParse
{
    /** Empty on success; otherwise why the text was refused. */
    std::string Error;
    AssessmentState State;

    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/** Reads SerialiseAssessment's text. Refuses truncation, extra text, malformed numbers and inconsistent state. */
[[nodiscard]] PERFECTWORLDCORE_API AssessmentParse DeserialiseAssessment(std::string_view text);
} // namespace PerfectWorld
