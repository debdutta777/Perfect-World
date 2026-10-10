#pragma once

#include "ContentRegistry.h"
#include "GameClock.h"
#include "StructureModelCatalogue.h"

#include <algorithm>
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
 * What the three structures after "model a system" share (ROADMAP M2-09; the rules
 * of the first structure are in StructureModel.h and are not changed by this file):
 * "Construct an explanation" (StructureExplain.h), "Investigate a fault"
 * (StructureInvestigate.h) and "Coordinate a group" (StructureCoordinate.h).
 * SCHOOL_AND_PROGRESSION: "They share controls; they do not share meaning." The
 * controls are the conventions in this file; the meaning (beats, decisions, what
 * failure means) is each structure's own.
 *
 * Shared conventions, all mirroring StructureModel:
 *   - Time economy. A 45-minute period is nine five-minute units: two for the bell
 *     and handing in, seven of working time, five when time is the scarce resource.
 *     Every decision is quoted before it is committed and charged exactly its
 *     quote. Cost depends on the character's method, the goal, the case's role and
 *     its scarce resource, never on a hidden roll. The only refusal is the player's
 *     own spent time (UnitsShort), and every beat has a free way forward, so no
 *     exposure can strand. "Never invalidate a correct committed solution through a
 *     hidden stat roll. Always state the cost before commitment."
 *   - Case selection is a keyed draw on the Opportunity stream that never repeats
 *     one of the last StructureRecentKept cases and prefers cases not yet seen.
 *   - Mastery. A family is secure after StructureSecureSoundExposures foreground
 *     exposures graded Sound with StructureSecureDistinctCases different cases seen.
 *   - Routine abbreviation. A secure family with no exposure running may be resolved
 *     "using established routine": a Sound result at the full normal learning value,
 *     equal to what the structure's standard careful policy earns in foreground. It
 *     presents no case, observes nothing, and counts as an exposure but not as a
 *     Sound one. There is no XP penalty and no bonus for playing by hand.
 *   - Save. Each structure serialises its whole state as text (integers and stable
 *     IDs, one record per line) so a save taken mid-exposure restores the same
 *     exposure and finishes it once. Every truncation is refused.
 *   - Grades. Sound (no failure), Partial (right but wasteful or loosely bounded),
 *     Unsound (the central claim of the structure is not borne out). Failures are
 *     bit sets with a different meaning in each structure, never just "wrong answer".
 * None of the three is yet a member of WorldState, the reducer, the hash or WorldSave.
 */

// ---- Chosen numbers shared with the model structure --------------------------------

inline constexpr std::int32_t StructureWorkUnits = ModelWorkUnits;
inline constexpr std::int32_t StructureScarceTimeUnits = ModelScarceTimeUnits;
inline constexpr std::int32_t StructureDiscountUnits = ModelDiscountUnits;
inline constexpr std::int32_t StructureLearningPermille = ModelNormalLearningPermille;
inline constexpr std::size_t StructureRecentKept = ModelRecentKept;
inline constexpr std::uint32_t StructureSecureSoundExposures = ModelSecureSoundExposures;
inline constexpr std::size_t StructureSecureDistinctCases = ModelSecureDistinctCases;
inline constexpr std::size_t StructureMaxFamilies = ModelMaxFamilies;
inline constexpr std::size_t StructureMaxSeen = ModelMaxSeen;
/** Not documented: the most IDs in one saved list (claims, evidence, tests, assignments, members spoken to). */
inline constexpr std::size_t StructureMaxListed = 16;

/** The units a period holds when time is (or is not) the scarce resource. */
[[nodiscard]] constexpr std::int32_t StructureBudgetUnits(bool timeScarce) noexcept
{
    return timeScarce ? StructureScarceTimeUnits : StructureWorkUnits;
}

// ---- Errors, grades and shared results ---------------------------------------------

enum class StructureError : std::uint8_t
{
    None = 0,
    UnknownFamily,
    UnknownCase,
    Busy,
    NotActive,
    WrongBeat,
    /** The decision is malformed: nothing chosen, a repeat, a list that is too long, a value outside its enum. */
    BadChoice,
    /** The decision names a claim, evidence, limitation, test, cause, member or task the case does not have. */
    UnknownItem,
    /** A test already run, a member already spoken to: once is enough. */
    AlreadyDone,
    /** A worked example or model paragraph for a character who does not need one. */
    NotNeeded,
    UnitsShort,
    NotSecure,
    InvalidSkill,
    InvalidState
};

/** The stable rejection code, for example "structure.units_short". Empty for None. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view StructureErrorCode(StructureError error) noexcept;

enum class StructureGrade : std::uint8_t
{
    Sound,
    Partial,
    Unsound
};

/** The skill level (milli-XP) at which the character has a method. */
struct StructureMethod
{
    std::string Id;
    std::string Name;
    std::int32_t KnownAtMilli = 0;

    friend bool operator==(const StructureMethod&, const StructureMethod&) = default;
};

/** What the player has seen and mastered in one family (the same record as ModelFamilyRecord). */
struct StructureFamilyRecord
{
    std::string FamilyId;
    /** Case IDs ever presented, sorted, no duplicates. */
    std::vector<std::string> Seen;
    /** The last cases presented, oldest first, at most StructureRecentKept, each also in Seen. */
    std::vector<std::string> Recent;
    /** Exposures finished, foreground and routine. */
    std::uint32_t Exposures = 0;
    /** Foreground exposures graded Sound. */
    std::uint32_t SoundExposures = 0;
    bool Secure = false;

    friend bool operator==(const StructureFamilyRecord&, const StructureFamilyRecord&) = default;
};

/** The fields every structure's running exposure carries. Idle when CaseId is empty. */
struct StructureExposureBase
{
    std::string FamilyId;
    std::string CaseId;
    std::int32_t UnitsTotal = 0;
    std::int32_t UnitsLeft = 0;
    /** Fixed at the start from the character's skill, so a quote never moves mid-exposure. */
    bool MethodKnown = false;

    [[nodiscard]] bool Active() const noexcept { return !CaseId.empty(); }
    friend bool operator==(const StructureExposureBase&, const StructureExposureBase&) = default;
};

/** One structure's whole state: what was seen and mastered, and the exposure in progress. */
template <typename ExposureT>
struct StructureState
{
    /** Sorted by FamilyId, no duplicates. */
    std::vector<StructureFamilyRecord> Families;
    ExposureT Exposure;

    [[nodiscard]] const StructureFamilyRecord* FindFamily(std::string_view familyId) const noexcept
    {
        const auto it = std::lower_bound(Families.begin(), Families.end(), familyId,
                                         [](const StructureFamilyRecord& record, std::string_view key) { return record.FamilyId < key; });
        return it != Families.end() && it->FamilyId == familyId ? &*it : nullptr;
    }

    friend bool operator==(const StructureState&, const StructureState&) = default;
};

struct StructureSelection
{
    StructureError Error = StructureError::None;
    std::string CaseId;
    /** How many cases were eligible for the draw. */
    std::size_t Candidates = 0;
    /** True when the draw was restricted to cases the player has not seen. */
    bool FromUnseen = false;

    [[nodiscard]] bool Ok() const noexcept { return Error == StructureError::None; }
};

struct StructureBeginResult
{
    StructureError Error = StructureError::None;
    std::int32_t Units = 0;
    bool MethodKnown = false;

    [[nodiscard]] bool Ok() const noexcept { return Error == StructureError::None; }
};

struct StructureQuote
{
    /** None when the decision is legal now, else why not (UnitsShort still carries Units). */
    StructureError Error = StructureError::None;
    std::int32_t Units = 0;
    std::int32_t UnitsAfter = 0;

    [[nodiscard]] bool Ok() const noexcept { return Error == StructureError::None; }
    friend bool operator==(const StructureQuote&, const StructureQuote&) = default;
};

/** The result of a finished (or routine) exposure; each structure extends it with its own detail. */
struct StructureOutcome
{
    std::string FamilyId;
    /** Empty for a routine resolution. */
    std::string CaseId;
    StructureGrade Grade = StructureGrade::Sound;
    /** A bit set of the structure's own failure kinds. */
    std::uint8_t Failures = 0;
    /** False for routine: no manual play, nothing new observed. */
    bool Foreground = true;
    std::int32_t LearningPermille = StructureLearningPermille;
    std::int32_t UnitsSpent = 0;
    bool BecameSecure = false;

    friend bool operator==(const StructureOutcome&, const StructureOutcome&) = default;
};

template <typename OutcomeT>
struct StructureStepResult
{
    StructureError Error = StructureError::None;
    /** The units charged, equal to the quote taken just before. */
    std::int32_t Charged = 0;
    /** True when the decision ended the exposure; Outcome is then set. */
    bool Finished = false;
    OutcomeT Outcome;

    [[nodiscard]] bool Ok() const noexcept { return Error == StructureError::None; }
};

struct StructureRoutineResult
{
    StructureError Error = StructureError::None;
    StructureOutcome Outcome;

    [[nodiscard]] bool Ok() const noexcept { return Error == StructureError::None; }
};

// ---- The catalogue, shared shape ---------------------------------------------------

/** The fields of a case that every structure's family needs. */
struct StructureCaseBase
{
    std::string Id;
    std::string FamilyId;
    std::string Name;
    std::string Subject;
    std::string Brief;
    std::string MethodId;
    std::int32_t MethodKnownAtMilli = 0;
};

struct StructureFamilyHeader
{
    std::string Id;
    std::string Name;
    std::string SkillId;
    std::int32_t MinCases = 0;
    std::vector<StructureMethod> Methods;
};

template <typename CaseT>
struct StructureFamilyDef : StructureFamilyHeader
{
    /** Sorted by Id. */
    std::vector<CaseT> Cases;
};

template <typename CaseT>
struct StructureCatalogue
{
    /** Sorted by Id. */
    std::vector<StructureFamilyDef<CaseT>> Families;

    [[nodiscard]] const StructureFamilyDef<CaseT>* FindFamily(std::string_view id) const noexcept
    {
        const auto it = std::lower_bound(Families.begin(), Families.end(), id,
                                         [](const StructureFamilyDef<CaseT>& family, std::string_view key) { return family.Id < key; });
        return it != Families.end() && it->Id == id ? &*it : nullptr;
    }

    [[nodiscard]] const CaseT* FindCase(std::string_view id) const noexcept
    {
        for (const StructureFamilyDef<CaseT>& family : Families)
        {
            const auto it = std::lower_bound(family.Cases.begin(), family.Cases.end(), id,
                                             [](const CaseT& def, std::string_view key) { return def.Id < key; });
            if (it != family.Cases.end() && it->Id == id)
                return &*it;
        }
        return nullptr;
    }
};

template <typename CatalogueT>
struct StructureCatalogueResult
{
    /** Empty on success; otherwise the record and what is wrong with it. */
    std::string Error;
    CatalogueT Catalogue;

    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/**
 * The deterministic pick among `caseIds` (sorted), never one of the recent ones and
 * preferring one not yet seen, as a keyed draw from (world seed, instant, exposure
 * count, family). Changes nothing; asking twice gives one answer.
 */
[[nodiscard]] PERFECTWORLDCORE_API StructureSelection StructureSelectCase(const std::vector<std::string>& caseIds,
                                                                          const StructureFamilyRecord* record,
                                                                          std::string_view familyId, std::uint64_t worldSeed,
                                                                          GameInstant at);

/** True when `families` is sorted by ID, bounded, and every record is consistent. */
[[nodiscard]] PERFECTWORLDCORE_API bool IsStructureFamilyListValid(const std::vector<StructureFamilyRecord>& families);
} // namespace PerfectWorld
