#pragma once

#include "StructureCommon.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace PerfectWorld
{
/**
 * The "construct an explanation" structure and its classroom family (ROADMAP M2-09;
 * SCHOOL_AND_PROGRESSION "Four interaction structures": "Place a claim, its
 * supporting evidence, and its limitation into three slots", for literature,
 * history, economics and interpretation; "An essay's defensible interpretation must
 * not be graded like a circuit with one hidden right answer"; "What varies on the
 * fortieth class"; "Mastery must remove repetition"; SYSTEMS_CATALOGUE "The
 * engines Explain, Model, Fault and Coordinate are pure Step functions").
 *
 * A pure, integer-only runner for one exposure over a plain state (ExplainState),
 * with the conventions of StructureCommon.h (units, quotes, selection, mastery,
 * routine, save text). Not yet a member of WorldState, the reducer or the save.
 *
 * Content: explain_family and explain_case records (Data/Content), checked by the one
 * validator (ContentStructureSchemas.cpp) and read by LoadExplainCatalogue. A case
 * offers several claims; at least two are defensible and the rest are not, so there
 * is no single hidden right answer. Each claim is supported by some evidence items,
 * contradicted by others; each limitation bounds some claims.
 *
 * One exposure is three beats, the three slots:
 *   1. Claim. The player picks one claim. The method is reading the source closely:
 *      practised, it takes ExplainReadKnownUnits; unfamiliar, ExplainReadDeriveUnits,
 *      or first ask for a model paragraph (ExplainParagraphUnits, then the practised
 *      price) as the model structure's worked example.
 *   2. Evidence. The player places up to ExplainEvidenceSlots items, each costing
 *      ExplainEvidenceUnits. At least one is required.
 *   3. Limitation. The player names one limitation or states none (free). Naming one
 *      finishes the exposure; so does stating none.
 * The player chooses a goal first (argue the claim, weigh the evidence, state the
 * limits), which only makes that slot cheaper. A role does the same for its own slot
 * (author the claim, researcher the evidence, critic the limitation); the recorder's
 * notes make the model paragraph cheaper. A role and a goal do not stack on one slot.
 *
 * Scarce resources: time (StructureScarceTimeUnits units for the period), sources (each
 * evidence item costs ExplainSourcesSurcharge more), help (the model paragraph costs
 * ExplainHelpSurcharge more), counterpoint (naming a limitation costs
 * ExplainCounterpointSurcharge more).
 *
 * Failure meanings, diagnostic and never just "wrong answer":
 *   Indefensible: the claim is one the sources contradict.
 *   Unsupported: a defensible claim with no placed evidence that supports it.
 *   Overreach: no limitation, or one that does not bound this claim.
 *   Padding (wasted time): placed evidence that does not support the claim.
 * Unsound = Indefensible or Unsupported; Partial = Overreach or Padding only; else
 * Sound. A defensible claim is graded on its own evidence and limits, so every
 * defensible claim can be completed Sound (the loader refuses a case where one cannot
 * within the budget). Learning is the activity's normal allowance in every case.
 */

enum class ExplainGoal : std::uint8_t
{
    ArgueClaim,
    WeighEvidence,
    StateLimits
};
inline constexpr std::array<std::string_view, 3> ExplainGoalNames{"argue_claim", "weigh_evidence", "state_limits"};

enum class ExplainBeat : std::uint8_t
{
    Claim,
    Evidence,
    Limitation
};
inline constexpr std::array<std::string_view, 3> ExplainBeatNames{"claim", "evidence", "limitation"};

/** The player's seat in the group. Not documented as a closed set; chosen for this structure. */
enum class ExplainRole : std::uint8_t
{
    Author,
    Researcher,
    Critic,
    Recorder
};
inline constexpr std::array<std::string_view, 4> ExplainRoleNames{"author", "researcher", "critic", "recorder"};

enum class ExplainScarce : std::uint8_t
{
    Time,
    Sources,
    Help,
    Counterpoint
};
inline constexpr std::array<std::string_view, 4> ExplainScarceNames{"time", "sources", "help", "counterpoint"};

/** What the question asks for, the case's varied unknown. */
enum class ExplainQuestion : std::uint8_t
{
    Meaning,
    Cause,
    Judgement
};
inline constexpr std::array<std::string_view, 3> ExplainQuestionNames{"meaning", "cause", "judgement"};
inline constexpr std::array<std::string_view, 4> ExplainSubjectNames{"literature", "history", "economics", "interpretation"};

enum class ExplainActionKind : std::uint8_t
{
    ModelParagraph,
    ChooseClaim,
    PlaceEvidence,
    PlaceLimitation
};
/** The decisions, in the order of the beats they belong to. */
inline constexpr std::array<std::string_view, 4> ExplainDecisionNames{"model_paragraph", "choose_claim", "place_evidence", "place_limitation"};

inline constexpr std::uint8_t ExplainFailIndefensible = 1;
inline constexpr std::uint8_t ExplainFailUnsupported = 2;
inline constexpr std::uint8_t ExplainFailOverreach = 4;
inline constexpr std::uint8_t ExplainFailPadding = 8;
inline constexpr std::array<std::string_view, 4> ExplainFailureNames{"indefensible_reading", "unsupported_claim", "overreach", "padding"};

// ---- Chosen numbers (the docs fix the shape, not these) ----------------------------

/** Not documented: a close reading takes two units practised, four unfamiliar (the model structure's group clearing). */
inline constexpr std::int32_t ExplainReadKnownUnits = ModelCommitKnownUnits;
inline constexpr std::int32_t ExplainReadDeriveUnits = ModelCommitDeriveUnits;
/** Not documented: the model paragraph, and its surcharge when help is scarce. */
inline constexpr std::int32_t ExplainParagraphUnits = 1;
inline constexpr std::int32_t ExplainHelpSurcharge = 2;
/** Not documented: one evidence item, the surcharge when sources are scarce, and the number of evidence slots. */
inline constexpr std::int32_t ExplainEvidenceUnits = 1;
inline constexpr std::int32_t ExplainSourcesSurcharge = 1;
inline constexpr std::size_t ExplainEvidenceSlots = 2;
/** Not documented: naming a limitation, and the surcharge when no one argues the other side. */
inline constexpr std::int32_t ExplainLimitationUnits = 1;
inline constexpr std::int32_t ExplainCounterpointSurcharge = 1;

// ---- The catalogue ----------------------------------------------------------------

struct ExplainClaim
{
    std::string Id;
    std::string Name;
    /** False for a reading the sources contradict. The player's interface never shows this. */
    bool Defensible = false;
};

struct ExplainEvidence
{
    std::string Id;
    std::string Name;
    /** The claims this item supports and the claims it cuts against. Neither: an item beside the point. */
    std::vector<std::string> Supports;
    std::vector<std::string> Against;
};

struct ExplainLimitation
{
    std::string Id;
    std::string Name;
    /** The claims this limitation honestly bounds. */
    std::vector<std::string> Bounds;
};

struct ExplainCaseDef : StructureCaseBase
{
    ExplainQuestion Question = ExplainQuestion::Meaning;
    ExplainRole Role = ExplainRole::Author;
    ExplainScarce Scarce = ExplainScarce::Time;
    /** The candidate claims, evidence and limitations, in authored order. */
    std::vector<ExplainClaim> Claims;
    std::vector<ExplainEvidence> Evidence;
    std::vector<ExplainLimitation> Limitations;
};

using ExplainFamilyDef = StructureFamilyDef<ExplainCaseDef>;
using ExplainCatalogue = StructureCatalogue<ExplainCaseDef>;
using ExplainCatalogueResult = StructureCatalogueResult<ExplainCatalogue>;

/**
 * Reads every explain family and case. Refuses (never coerces) a case whose family or
 * method is missing, that has no defensible claim, whose defensible claim has no
 * supporting item or bounding limitation, or that cannot be completed Sound within
 * its budget by a character with no method under any goal.
 */
[[nodiscard]] PERFECTWORLDCORE_API ExplainCatalogueResult LoadExplainCatalogue(const ContentDb& content);

/** The claim, evidence and limitation of one Sound plan. ClaimId is empty when the claim has none. */
struct ExplainPlan
{
    std::string ClaimId;
    std::string EvidenceId;
    std::string LimitationId;
};
/** The first Sound plan for `claimId`: its first supporting item and its first bounding limitation. */
[[nodiscard]] PERFECTWORLDCORE_API ExplainPlan ExplainPlanFor(const ExplainCaseDef& def, std::string_view claimId);

// ---- Costs ------------------------------------------------------------------------

/** The units a period holds when `scarce` is the case's scarce resource. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t ExplainBudgetUnits(ExplainScarce scarce) noexcept;
/** What asking for a model paragraph costs in this case. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t ExplainParagraphCost(const ExplainCaseDef& def) noexcept;
/** What choosing the claim costs, for a character who has (or has been shown) the method or not. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t ExplainClaimCost(const ExplainCaseDef& def, ExplainGoal goal, bool methodShown) noexcept;
/** What placing `items` pieces of evidence costs. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t ExplainEvidenceCost(const ExplainCaseDef& def, ExplainGoal goal, std::size_t items) noexcept;
/** What naming a limitation costs. Stating none is free. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t ExplainLimitationCost(const ExplainCaseDef& def, ExplainGoal goal) noexcept;

// ---- State ------------------------------------------------------------------------

/** The exposure in progress. Idle when CaseId is empty. */
struct ExplainExposure : StructureExposureBase
{
    ExplainGoal Goal = ExplainGoal::ArgueClaim;
    ExplainBeat Beat = ExplainBeat::Claim;
    bool ParagraphTaken = false;
    /** The chosen claim, once chosen. */
    std::string ClaimId;
    /** The placed evidence IDs, sorted, once placed. */
    std::vector<std::string> EvidenceIds;

    friend bool operator==(const ExplainExposure&, const ExplainExposure&) = default;
};

using ExplainState = StructureState<ExplainExposure>;

/** True when the state is internally consistent (sorted, bounded, beat and fields agree, secure never ahead of the counts). */
[[nodiscard]] PERFECTWORLDCORE_API bool IsExplainStateValid(const ExplainState& state);
/** True when the family has been mastered. */
[[nodiscard]] PERFECTWORLDCORE_API bool ExplainFamilySecure(const ExplainState& state, std::string_view familyId) noexcept;

// ---- Selection and starting -------------------------------------------------------

/** The next case for `familyId`, deterministic and never one of the recent ones. Changes nothing. */
[[nodiscard]] PERFECTWORLDCORE_API StructureSelection SelectExplainCase(const ExplainCatalogue& catalogue, const ExplainState& state,
                                                                        std::string_view familyId, std::uint64_t worldSeed, GameInstant at);

/**
 * Presents `caseId` with the goal the player chose. `skillMilli` is the character's
 * level in the family's skill; it fixes whether the method is practised. The case is
 * recorded as seen and recent. Refused while an exposure runs. A refusal changes nothing.
 */
PERFECTWORLDCORE_API StructureBeginResult BeginExplainExposure(ExplainState& state, const ExplainCatalogue& catalogue,
                                                               std::string_view caseId, ExplainGoal goal, std::int32_t skillMilli);

/** Abandons the running exposure: no outcome, no mastery change, and the case stays seen. */
PERFECTWORLDCORE_API StructureError CancelExplainExposure(ExplainState& state);

// ---- Playing ----------------------------------------------------------------------

/** One decision, as plain data. Only the fields of its kind are read. */
struct ExplainAction
{
    ExplainActionKind Kind = ExplainActionKind::ChooseClaim;
    /** ChooseClaim: the claim ID. */
    std::string Claim;
    /** PlaceEvidence: the item IDs, at most ExplainEvidenceSlots. */
    std::vector<std::string> Evidence;
    /** PlaceLimitation: the limitation ID, or empty to state none. */
    std::string Limitation;

    friend bool operator==(const ExplainAction&, const ExplainAction&) = default;
};

/** The outcome of a finished exposure, with what the grade rests on. */
struct ExplainOutcome : StructureOutcome
{
    std::int32_t PlacedItems = 0;
    std::int32_t SupportingItems = 0;
    bool Bounded = false;

    friend bool operator==(const ExplainOutcome&, const ExplainOutcome&) = default;
};

using ExplainStepResult = StructureStepResult<ExplainOutcome>;

/**
 * What `action` would cost, before it is committed. Changes nothing. ChooseClaim with
 * no claim and PlaceEvidence with no items quote their price (one item) without
 * checking a choice, so an interface can show every price before anything is chosen.
 */
[[nodiscard]] PERFECTWORLDCORE_API StructureQuote QuoteExplainAction(const ExplainState& state, const ExplainCatalogue& catalogue,
                                                                     const ExplainAction& action);

/** Applies one decision. Naming (or declining) the limitation ends the exposure and records its outcome once. A refusal changes nothing. */
PERFECTWORLDCORE_API ExplainStepResult ApplyExplainAction(ExplainState& state, const ExplainCatalogue& catalogue, const ExplainAction& action);

/**
 * The next decision of the standard careful policy: ask for a model paragraph only
 * when that is cheaper than reading unaided, choose the first defensible claim that
 * has a plan, place its first supporting item, and name its first bounding
 * limitation. It always earns a Sound result inside the budget, and it is what
 * routine abbreviation is equal to.
 */
[[nodiscard]] PERFECTWORLDCORE_API ExplainAction ExplainStandardAction(const ExplainCaseDef& def, const ExplainExposure& exposure);

/** Resolves one exposure of a secure family without manual play. See StructureCommon.h for the trade-off. */
PERFECTWORLDCORE_API StructureRoutineResult ResolveExplainRoutine(ExplainState& state, const ExplainCatalogue& catalogue,
                                                                  std::string_view familyId);

// ---- Text -------------------------------------------------------------------------

/**
 * The state as text, integers and stable IDs only, one record per line:
 *   pw.structure_explain/1 / families=N / (family=<id>,<exposures>,<sound>,<secure> /
 *   seen=<id>,... or - / recent=<id>,... or -) ... / exposure=- or
 *   <family>,<case>,<goal>,<beat>,<total>,<left>,<known>,<paragraph> /
 *   claim=<id> or - / evidence=<id>,... or - / end
 * Each line ends in a newline and the same state always gives the same text. Empty
 * for an inconsistent state.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseStructureExplain(const ExplainState& state);

struct ExplainParse
{
    /** Empty on success; otherwise why the text was refused. */
    std::string Error;
    ExplainState State;

    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/** Reads SerialiseStructureExplain's text. Refuses truncation, extra text, malformed numbers and inconsistent state. */
[[nodiscard]] PERFECTWORLDCORE_API ExplainParse DeserialiseStructureExplain(std::string_view text);
} // namespace PerfectWorld
