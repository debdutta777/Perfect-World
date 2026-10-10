#pragma once

#include "ContentRegistry.h"
#include "GameClock.h"
#include "StructureModelCatalogue.h"

#include <array>
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
 * The "model a system" structure, its classroom family and routine abbreviation
 * (ROADMAP M2-08; SCHOOL_AND_PROGRESSION "Four interaction structures", "A worked
 * example: the water tank", "What varies on the fortieth class", "Mastery must
 * remove repetition", "Tests and assessments"; SYSTEMS_CATALOGUE "Activities and
 * interaction structures").
 *
 * A pure, integer-only runner for one exposure over a plain state
 * (StructureModelState). It reads no clock, owns no stats and is not yet a member
 * of WorldState, the reducer, the hash or WorldSave; the integration step calls it
 * from the activity layer and passes the character's skill in.
 *
 * Content. The structure, the problem families and their cases are records
 * (Data/Content/structure, problem_family, problem_case), checked by the one
 * validator (ContentProblemSchemas.cpp) and read by LoadModelCatalogue. A case is
 * authored, never randomised piece by piece: "Do not randomise these independently.
 * Author compatible cases and track which the player has seen."
 *
 * One exposure is the three beats of the water tank:
 *   1. Build the model. The player connects quantities by choosing links from the
 *      case's candidates. A practised method (the character's skill is at or above
 *      the method's threshold) clears the beat in ModelCommitKnownUnits; an
 *      unfamiliar one either derives it (ModelCommitDeriveUnits) or first asks for a
 *      worked example (ModelExampleUnits, then the known cost): "a practiced
 *      character already has the method; an unfamiliar one spends the beat asking
 *      for a worked example". The committed set of links is the player's answer.
 *   2. Resolve the discrepancy. "Only one investigation fits before the group must
 *      report": exactly one of accept the readings, check the times, look for a
 *      hidden flow, or compare the markings. The case's fault says which is the
 *      right one; "sometimes the measurements are fine" (fault none).
 *   3. Present a claim: an estimate, a bounded range, or "another measurement is
 *      needed", with or without an explanation for the group.
 * The player chooses a goal first (understand the model, test the measurements,
 * explain it to the group), which only makes that kind of work cheaper.
 *
 * Units. A 45-minute period is nine five-minute units (the "eighteen five-minute
 * units" of a 90-minute paper, halved); the bell and handing in take two, leaving
 * seven of working time (five when time is the scarce resource). Every action's cost is quoted before it is
 * committed (QuoteModelAction), and ApplyModelAction charges exactly the quote.
 * A cost depends on the character's method, the goal, the case's role and its
 * scarce resource, never on a hidden roll: a correct committed model is never
 * refused or reversed because of a stat (SCHOOL_AND_PROGRESSION: "Never invalidate
 * a correct committed solution through a hidden stat roll. Always state the cost
 * before commitment."). The only refusal is the player's own spent time (UnitsShort:
 * for example, deriving the model leaves too little to look for a hidden flow), and
 * every beat after the first has a free way forward (accept the readings; present
 * without explaining), while the first beat always fits the budget, so no exposure
 * can strand. Presenting the claim itself costs nothing.
 *
 * Roles and scarce resources vary by case (the authored variation):
 *   - role discount, one unit off that beat's work (not stacked with a goal's):
 *     solver and the goal "understand the model" discount the model; experimenter and
 *     "test the measurements" discount the investigation; recorder, presenter and
 *     "explain to the group" make the explanation free. Recorder and presenter (and
 *     that goal) must explain or the group cannot follow. A critic's hedge is never
 *     wasted time: raising the limitation is the critic's job.
 *   - scarce time: the budget is ModelScarceTimeUnits, not ModelWorkUnits. Scarce help: a
 *     worked example costs ModelHelpSurcharge more. Scarce readings: checking the
 *     times costs ModelReadingsSurcharge more. Scarce equipment: comparing the
 *     markings and looking for a flow cost ModelEquipmentSurcharge more.
 *
 * Outcome, diagnostic rather than "wrong answer" (SCHOOL_AND_PROGRESSION "what
 * failure means"): four failure kinds that can combine.
 *   WrongModel: the committed links are not exactly the sound ones.
 *   UnsupportedConclusion: an estimate while the discrepancy is unresolved.
 *   WastedTime: an investigation that found nothing to resolve, or a hedge when the
 *     model is sound and the readings were fine or resolved ("hedging wastes the
 *     period") unless the player is the critic.
 *   UnclearExplanation: an explanation was owed and not given.
 * Grade: Sound (no failure), Partial (only WastedTime or UnclearExplanation: the
 * answer was right but the period or the explanation was not), Unsound (WrongModel
 * or UnsupportedConclusion). Learning is the activity's normal allowance in every
 * case (ModelNormalLearningPermille): the practical outcome varies with play,
 * learning does not, so play is never "a permanent XP bonus".
 *
 * Case selection (SelectModelCase) is deterministic and avoids repeats: it never
 * offers one of the last ModelRecentKept cases shown (fewer when the family is
 * small), and offers a case the player has not yet seen whenever one is eligible.
 * The pick is a keyed draw on the Opportunity stream from (world seed, instant,
 * exposure count, family), so it is stateless and asking twice gives one answer.
 *
 * Mastery. A family becomes secure when a finished foreground exposure brings
 * ModelSecureSoundExposures graded Sound with at least ModelSecureDistinctCases
 * different cases met, and stays secure (decay belongs to a later step).
 *
 * Routine abbreviation (ResolveModelRoutine): "Attend using established routine:
 * normal attendance, normal learning, no manual play." Allowed only for a secure
 * family with no exposure running. It resolves a Sound result at the full normal
 * learning value, equal to what the standard careful policy (ModelStandardAction)
 * earns in foreground. The documented trade-off: no case is presented (Seen and
 * Recent are unchanged), nothing new is observed (Outcome::Foreground is false),
 * and it counts as an exposure but not as a Sound one. There is no XP penalty.
 *
 * Save. SerialiseStructureModel writes the whole state as text, so a save taken
 * mid-exposure restores the same exposure and finishes it once.
 */

// ---- State ------------------------------------------------------------------------

/** What the player has seen and mastered in one family. */
struct ModelFamilyRecord
{
    std::string FamilyId;
    /** Case IDs ever presented, sorted, no duplicates. */
    std::vector<std::string> Seen;
    /** The last cases presented, oldest first, at most ModelRecentKept, each also in Seen. */
    std::vector<std::string> Recent;
    /** Exposures finished, foreground and routine. */
    std::uint32_t Exposures = 0;
    /** Foreground exposures graded Sound. */
    std::uint32_t SoundExposures = 0;
    bool Secure = false;

    friend bool operator==(const ModelFamilyRecord&, const ModelFamilyRecord&) = default;
};

/** The exposure in progress. Idle when CaseId is empty. */
struct ModelExposure
{
    std::string FamilyId;
    std::string CaseId;
    ModelGoal Goal = ModelGoal::UnderstandModel;
    ModelBeat Beat = ModelBeat::BuildModel;
    std::int32_t UnitsTotal = 0;
    std::int32_t UnitsLeft = 0;
    /** Fixed at the start from the character's skill, so a quote never moves mid-exposure. */
    bool MethodKnown = false;
    bool ExampleTaken = false;
    ModelInvestigation Investigation = ModelInvestigation::None;
    /** The committed link IDs, sorted, once the model is committed. */
    std::vector<std::string> Links;

    [[nodiscard]] bool Active() const noexcept { return !CaseId.empty(); }

    friend bool operator==(const ModelExposure&, const ModelExposure&) = default;
};

struct StructureModelState
{
    /** Sorted by FamilyId, no duplicates. */
    std::vector<ModelFamilyRecord> Families;
    ModelExposure Exposure;

    [[nodiscard]] PERFECTWORLDCORE_API const ModelFamilyRecord* FindFamily(std::string_view familyId) const noexcept;

    friend bool operator==(const StructureModelState&, const StructureModelState&) = default;
};

/** True when the state is internally consistent (sorted, bounded, beat and fields agree, secure never ahead of the counts). */
[[nodiscard]] PERFECTWORLDCORE_API bool IsStructureModelStateValid(const StructureModelState& state);
/** True when the family has been mastered. */
[[nodiscard]] PERFECTWORLDCORE_API bool ModelFamilySecure(const StructureModelState& state, std::string_view familyId) noexcept;

enum class ModelError : std::uint8_t
{
    None = 0,
    UnknownFamily,
    UnknownCase,
    Busy,
    NotActive,
    WrongBeat,
    ExampleNotNeeded,
    BadModel,
    UnknownLink,
    UnitsShort,
    NotSecure,
    InvalidSkill,
    InvalidState
};

/** The stable rejection code, for example "model.units_short". Empty for None. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view ModelErrorCode(ModelError error) noexcept;

// ---- Selection and starting -------------------------------------------------------

struct ModelSelection
{
    ModelError Error = ModelError::None;
    std::string CaseId;
    /** How many cases were eligible for the draw. */
    std::size_t Candidates = 0;
    /** True when the draw was restricted to cases the player has not seen. */
    bool FromUnseen = false;

    [[nodiscard]] bool Ok() const noexcept { return Error == ModelError::None; }
};

/** The next case for `familyId`, deterministic and never one of the recent ones. Changes nothing. */
[[nodiscard]] PERFECTWORLDCORE_API ModelSelection SelectModelCase(const ModelCatalogue& catalogue, const StructureModelState& state,
                                                                  std::string_view familyId, std::uint64_t worldSeed,
                                                                  GameInstant at);

struct ModelBeginResult
{
    ModelError Error = ModelError::None;
    std::int32_t Units = 0;
    bool MethodKnown = false;

    [[nodiscard]] bool Ok() const noexcept { return Error == ModelError::None; }
};

/**
 * Presents `caseId` with the goal the player chose. `skillMilli` is the character's
 * level in the family's skill; it fixes whether the method is practised. The case
 * is recorded as seen and recent. Refused while an exposure runs. A refusal changes
 * nothing.
 */
PERFECTWORLDCORE_API ModelBeginResult BeginModelExposure(StructureModelState& state, const ModelCatalogue& catalogue,
                                                         std::string_view caseId, ModelGoal goal, std::int32_t skillMilli);

/** Abandons the running exposure: no outcome, no mastery change, and the case stays seen. */
PERFECTWORLDCORE_API ModelError CancelModelExposure(StructureModelState& state);

// ---- Playing ----------------------------------------------------------------------

enum class ModelActionKind : std::uint8_t
{
    WorkedExample,
    CommitModel,
    Investigate,
    PresentClaim
};

/** One decision, as plain data. Only the fields of its kind are read. */
struct ModelAction
{
    ModelActionKind Kind = ModelActionKind::CommitModel;
    /** CommitModel: the chosen link IDs. */
    std::vector<std::string> Links;
    /** Investigate. */
    ModelInvestigation Investigation = ModelInvestigation::AcceptReadings;
    /** PresentClaim. */
    ModelClaim Claim = ModelClaim::Estimate;
    bool Explain = false;

    friend bool operator==(const ModelAction&, const ModelAction&) = default;
};

struct ModelQuote
{
    /** None when the action is legal now, else why not (UnitsShort still carries Units). */
    ModelError Error = ModelError::None;
    std::int32_t Units = 0;
    std::int32_t UnitsAfter = 0;

    [[nodiscard]] bool Ok() const noexcept { return Error == ModelError::None; }
    friend bool operator==(const ModelQuote&, const ModelQuote&) = default;
};

/**
 * What `action` would cost, before it is committed. Changes nothing. A CommitModel
 * with no links quotes the cost without checking a model, so an interface can show
 * every price before the player has chosen anything.
 */
[[nodiscard]] PERFECTWORLDCORE_API ModelQuote QuoteModelAction(const StructureModelState& state, const ModelCatalogue& catalogue,
                                                               const ModelAction& action);

enum class ModelGrade : std::uint8_t
{
    Sound,
    Partial,
    Unsound
};

/** Failure kinds, a bit set in ModelOutcome::Failures. */
inline constexpr std::uint8_t ModelFailWrongModel = 1;
inline constexpr std::uint8_t ModelFailUnsupported = 2;
inline constexpr std::uint8_t ModelFailWastedTime = 4;
inline constexpr std::uint8_t ModelFailUnclear = 8;

struct ModelOutcome
{
    std::string FamilyId;
    /** Empty for a routine resolution. */
    std::string CaseId;
    ModelGrade Grade = ModelGrade::Sound;
    std::uint8_t Failures = 0;
    /** False for routine: no manual play, nothing new observed. */
    bool Foreground = true;
    std::int32_t LearningPermille = ModelNormalLearningPermille;
    std::int32_t UnitsSpent = 0;
    /** Sound links left out, and unsound links chosen. */
    std::int32_t MissingLinks = 0;
    std::int32_t ExtraLinks = 0;
    bool BecameSecure = false;

    friend bool operator==(const ModelOutcome&, const ModelOutcome&) = default;
};

struct ModelStepResult
{
    ModelError Error = ModelError::None;
    /** The units charged, equal to the quote taken just before. */
    std::int32_t Charged = 0;
    /** True when the claim ended the exposure; Outcome is then set. */
    bool Finished = false;
    ModelOutcome Outcome;

    [[nodiscard]] bool Ok() const noexcept { return Error == ModelError::None; }
};

/** Applies one decision. Presenting a claim ends the exposure and records its outcome once. A refusal changes nothing. */
PERFECTWORLDCORE_API ModelStepResult ApplyModelAction(StructureModelState& state, const ModelCatalogue& catalogue,
                                                      const ModelAction& action);

/**
 * The next decision of the standard careful policy: ask for an example only when that
 * is cheaper than deriving, commit exactly the sound links, run the matching
 * investigation, present an estimate and explain when affordable. It always earns a
 * Sound result inside the budget, and it is what routine abbreviation is equal to.
 */
[[nodiscard]] PERFECTWORLDCORE_API ModelAction ModelStandardAction(const ModelCaseDef& def, const ModelExposure& exposure);

struct ModelRoutineResult
{
    ModelError Error = ModelError::None;
    ModelOutcome Outcome;

    [[nodiscard]] bool Ok() const noexcept { return Error == ModelError::None; }
};

/** Resolves one exposure of a secure family without manual play. See the header comment for the trade-off. */
PERFECTWORLDCORE_API ModelRoutineResult ResolveModelRoutine(StructureModelState& state, const ModelCatalogue& catalogue,
                                                            std::string_view familyId);

// ---- Text -------------------------------------------------------------------------

/**
 * The state as text, integers and stable IDs only, one record per line:
 *   pw.structure_model/1 / families=N / (family=<id>,<exposures>,<sound>,<secure> /
 *   seen=<id>,... or - / recent=<id>,... or -) ... / exposure=- or
 *   <family>,<case>,<goal>,<beat>,<total>,<left>,<known>,<example>,<investigation> /
 *   links=<id>,... or - / end
 * Each line ends in a newline and the same state always gives the same text. Empty
 * for an inconsistent state.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseStructureModel(const StructureModelState& state);

struct StructureModelParse
{
    /** Empty on success; otherwise why the text was refused. */
    std::string Error;
    StructureModelState State;

    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/** Reads SerialiseStructureModel's text. Refuses truncation, extra text, malformed numbers and inconsistent state. */
[[nodiscard]] PERFECTWORLDCORE_API StructureModelParse DeserialiseStructureModel(std::string_view text);
} // namespace PerfectWorld
