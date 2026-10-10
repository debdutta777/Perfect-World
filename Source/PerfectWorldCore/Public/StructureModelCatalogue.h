#pragma once

#include "ContentRegistry.h"

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
/** The enumerations, chosen numbers, catalogue types and prices of the "model a system" structure. Rules: see StructureModel.h. */

// ---- Enumerations and their stable names (content and save text) ------------------

/** The kind of quantity the case leaves unknown. */
enum class ModelUnknown : std::uint8_t
{
    Stock,
    Inflow,
    Outflow,
    Time
};
inline constexpr std::array<std::string_view, 4> ModelUnknownNames{"stock", "inflow", "outflow", "time"};

/** The player's seat in the group. */
enum class ModelRole : std::uint8_t
{
    Solver,
    Experimenter,
    Recorder,
    Critic,
    Presenter
};
inline constexpr std::array<std::string_view, 5> ModelRoleNames{"solver", "experimenter", "recorder", "critic", "presenter"};

/** What is short in this case. */
enum class ModelScarce : std::uint8_t
{
    Time,
    Readings,
    Equipment,
    Help
};
inline constexpr std::array<std::string_view, 4> ModelScarceNames{"time", "readings", "equipment", "help"};

/** What is really behind the discrepancy. */
enum class ModelFault : std::uint8_t
{
    None,
    Timing,
    HiddenFlow,
    Scale
};
inline constexpr std::array<std::string_view, 4> ModelFaultNames{"none", "timing", "hidden_flow", "scale"};

enum class ModelGoal : std::uint8_t
{
    UnderstandModel,
    TestMeasurements,
    ExplainToGroup
};
inline constexpr std::array<std::string_view, 3> ModelGoalNames{"understand_model", "test_measurements", "explain_to_group"};

enum class ModelBeat : std::uint8_t
{
    BuildModel,
    ResolveDiscrepancy,
    PresentClaim
};
inline constexpr std::array<std::string_view, 3> ModelBeatNames{"build_model", "resolve_discrepancy", "present_claim"};

/** None is "not yet chosen" and never a choice. */
enum class ModelInvestigation : std::uint8_t
{
    None,
    AcceptReadings,
    CheckTimes,
    LookForFlow,
    CompareMarkings
};
inline constexpr std::array<std::string_view, 5> ModelInvestigationNames{"none", "accept_readings", "check_times", "look_for_flow",
                                                                         "compare_markings"};

enum class ModelClaim : std::uint8_t
{
    Estimate,
    BoundedRange,
    NeedMoreMeasurement
};
inline constexpr std::array<std::string_view, 3> ModelClaimNames{"estimate", "bounded_range", "need_more_measurement"};

/** The kinds a case's quantity can have. The first four are the ModelUnknown kinds. */
inline constexpr std::array<std::string_view, 5> ModelQuantityKindNames{"stock", "inflow", "outflow", "time", "context"};
inline constexpr std::array<std::string_view, 4> ModelSubjectNames{"mathematics", "physics", "accounting", "geography"};

/** The stable name of an enumerator, or empty for a value outside the enum. */
template <typename Enum, std::size_t N>
[[nodiscard]] constexpr std::string_view ModelNameOf(const std::array<std::string_view, N>& names, Enum value) noexcept
{
    const auto index = static_cast<std::size_t>(value);
    return index < N ? names[index] : std::string_view();
}

/** True and `out` set when `text` is one of `names`. */
template <typename Enum, std::size_t N>
[[nodiscard]] constexpr bool ModelParseName(const std::array<std::string_view, N>& names, std::string_view text, Enum& out) noexcept
{
    for (std::size_t i = 0; i < N; ++i)
    {
        if (names[i] == text)
        {
            out = static_cast<Enum>(i);
            return true;
        }
    }
    return false;
}

/** A well-formed content ID: [a-z0-9_.]+, at most ModelMaxIdLength long. */
inline constexpr std::size_t ModelMaxIdLength = 128;
[[nodiscard]] constexpr bool ModelIdWellFormed(std::string_view id) noexcept
{
    if (id.empty() || id.size() > ModelMaxIdLength)
        return false;
    for (const char c : id)
    {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.'))
            return false;
    }
    return true;
}

// ---- Chosen numbers (the docs fix the shape, not these) ---------------------------

/** A 45-minute period is nine five-minute units (SCHOOL_AND_PROGRESSION "Time economy"). */
inline constexpr std::int32_t ModelPeriodUnits = 9;
inline constexpr std::int32_t ModelUnitMinutes = 5;
/** Not documented: the bell, settling and handing in take two units; the rest is working time. */
inline constexpr std::int32_t ModelSettleUnits = 2;
inline constexpr std::int32_t ModelWorkUnits = ModelPeriodUnits - ModelSettleUnits;
/** Not documented: the working time when time is the scarce resource (a late start loses two more units). */
inline constexpr std::int32_t ModelScarceTimeUnits = ModelWorkUnits - 2;
/** Documented: a practised method clears a group in two units, deriving takes four. */
inline constexpr std::int32_t ModelCommitKnownUnits = 2;
inline constexpr std::int32_t ModelCommitDeriveUnits = 4;
/** Not documented: a worked example, the surcharge when help is scarce, and the other costs below. */
inline constexpr std::int32_t ModelExampleUnits = 1;
inline constexpr std::int32_t ModelHelpSurcharge = 2;
inline constexpr std::int32_t ModelCheckTimesUnits = 1;
inline constexpr std::int32_t ModelCompareMarkingsUnits = 1;
inline constexpr std::int32_t ModelLookForFlowUnits = 2;
inline constexpr std::int32_t ModelReadingsSurcharge = 1;
inline constexpr std::int32_t ModelEquipmentSurcharge = 1;
inline constexpr std::int32_t ModelExplainUnits = 1;
/** One unit off the beat's work for a fitting role or goal; sources do not stack. */
inline constexpr std::int32_t ModelDiscountUnits = 1;
/** The learning value of every exposure, foreground or routine: the activity's normal allowance. */
inline constexpr std::int32_t ModelNormalLearningPermille = 1000;
/** Not documented: the last cases never offered again, and what makes a family secure. */
inline constexpr std::size_t ModelRecentKept = 3;
inline constexpr std::uint32_t ModelSecureSoundExposures = 4;
inline constexpr std::size_t ModelSecureDistinctCases = 3;
/** Not documented: bounds that keep a state and its text finite. */
inline constexpr std::size_t ModelMaxFamilies = 64;
inline constexpr std::size_t ModelMaxSeen = 256;
inline constexpr std::size_t ModelMaxLinks = 16;

// ---- The catalogue ----------------------------------------------------------------

struct ModelQuantity
{
    std::string Id;
    std::string Name;
    /** One of ModelQuantityKindNames. */
    std::string Kind;
    /** False for the one unknown quantity. */
    bool Given = true;

    friend bool operator==(const ModelQuantity&, const ModelQuantity&) = default;
};

struct ModelLink
{
    std::string Id;
    std::string From;
    std::string To;
    /** True for a link of the right model. The player's interface never shows this. */
    bool Sound = false;

    friend bool operator==(const ModelLink&, const ModelLink&) = default;
};

struct ModelMethod
{
    std::string Id;
    std::string Name;
    /** The skill level (milli-XP) at which the character has the method. */
    std::int32_t KnownAtMilli = 0;

    friend bool operator==(const ModelMethod&, const ModelMethod&) = default;
};

struct ModelCaseDef
{
    std::string Id;
    std::string FamilyId;
    std::string Name;
    std::string Subject;
    std::string Brief;
    ModelUnknown Unknown = ModelUnknown::Stock;
    ModelRole Role = ModelRole::Solver;
    ModelScarce Scarce = ModelScarce::Time;
    ModelFault Fault = ModelFault::None;
    std::string MethodId;
    std::int32_t MethodKnownAtMilli = 0;
    std::vector<ModelQuantity> Quantities;
    /** The candidate links the player chooses among, sound and unsound, in authored order. */
    std::vector<ModelLink> Links;
};

struct ModelFamilyDef
{
    std::string Id;
    std::string Name;
    std::string SkillId;
    std::int32_t MinCases = 0;
    std::vector<ModelMethod> Methods;
    /** Sorted by Id. */
    std::vector<ModelCaseDef> Cases;
};

struct ModelCatalogue
{
    /** Sorted by Id. */
    std::vector<ModelFamilyDef> Families;

    [[nodiscard]] PERFECTWORLDCORE_API const ModelFamilyDef* FindFamily(std::string_view id) const noexcept;
    [[nodiscard]] PERFECTWORLDCORE_API const ModelCaseDef* FindCase(std::string_view id) const noexcept;
};

struct ModelCatalogueResult
{
    /** Empty on success; otherwise the record and what is wrong with it. */
    std::string Error;
    ModelCatalogue Catalogue;

    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/**
 * Reads every model family and case. Refuses (never coerces) a case whose family or
 * method is missing, whose sound links are empty, or that cannot be completed within
 * its budget by a character with no method under any goal.
 */
[[nodiscard]] PERFECTWORLDCORE_API ModelCatalogueResult LoadModelCatalogue(const ContentDb& content);

// ---- Costs ------------------------------------------------------------------------

/** The units a period holds when `scarce` is the case's scarce resource. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t ModelBudgetUnits(ModelScarce scarce) noexcept;
/** The investigation that resolves a fault (AcceptReadings for none). */
[[nodiscard]] PERFECTWORLDCORE_API ModelInvestigation ModelMatchingInvestigation(ModelFault fault) noexcept;
/** What asking for a worked example costs in this case. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t ModelExampleCost(const ModelCaseDef& def) noexcept;
/** What committing the model costs, for a character who has (or has been shown) the method or not. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t ModelCommitCost(const ModelCaseDef& def, ModelGoal goal, bool methodKnown) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t ModelInvestigationCost(const ModelCaseDef& def, ModelGoal goal,
                                                                       ModelInvestigation investigation) noexcept;
/** What explaining to the group costs (the optional part of presenting a claim). */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t ModelExplainCost(const ModelCaseDef& def, ModelGoal goal) noexcept;
/** True when the role or goal owes the group an explanation. */
[[nodiscard]] PERFECTWORLDCORE_API bool ModelExplanationOwed(const ModelCaseDef& def, ModelGoal goal) noexcept;

} // namespace PerfectWorld
