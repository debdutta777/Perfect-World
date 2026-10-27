#pragma once

#include "ContentRegistry.h"
#include "Tower.h"

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
enum class TowerScenarioMetric : std::uint8_t
{
    Discipline = 0,
    CommonJudgment = 1
};

[[nodiscard]] PERFECTWORLDCORE_API std::string_view TowerScenarioMetricId(TowerScenarioMetric metric);
[[nodiscard]] PERFECTWORLDCORE_API bool TowerScenarioMetricFromId(
    std::string_view id,
    TowerScenarioMetric& outMetric);

struct TowerScenarioClue
{
    std::string Id;
    TowerMethod Method = TowerMethod::Analysis;
    std::string TextKey;

    bool operator==(const TowerScenarioClue&) const = default;
};

struct TowerScenarioDecision
{
    std::string Id;
    std::string LabelKey;
    std::string OutcomeKey;
    TowerMethod Method = TowerMethod::Analysis;
    bool IsCombat = false;
    bool IsMistake = false;
    std::int32_t ScoreMilli = 0;
    std::int32_t Minutes = 1;
    std::string NextBeat;

    bool operator==(const TowerScenarioDecision&) const = default;
};

struct TowerScenarioRecovery
{
    std::string Id;
    std::string PromptKey;
    std::string ActionKey;
    std::string OutcomeKey;
    TowerMethod Method = TowerMethod::Analysis;
    std::int32_t RestoredScoreMilli = 0;
    std::int32_t Minutes = 1;
    std::string NextBeat;

    bool operator==(const TowerScenarioRecovery&) const = default;
};

struct TowerScenarioBeat
{
    std::string Id;
    std::string PromptKey;
    TowerScenarioMetric Metric = TowerScenarioMetric::Discipline;
    std::vector<TowerScenarioClue> Clues;
    std::vector<TowerScenarioDecision> Decisions;
    TowerScenarioRecovery Recovery;

    bool operator==(const TowerScenarioBeat&) const = default;
};

struct TowerScenario
{
    std::string Id;
    std::string Note;
    std::string RouteId;
    std::uint8_t Floor = 1;
    std::string RunnerId;
    TowerMethod PrimaryMethod = TowerMethod::Analysis;
    bool AllowsCombat = false;
    std::string ClassroomModelId;
    std::string ChangedRuleKey;
    std::string TitleKey;
    std::string SetupKey;
    std::string EntryBeat;
    std::vector<TowerScenarioBeat> Beats;

    bool operator==(const TowerScenario&) const = default;
};

struct TowerScenarioCatalogue
{
    std::vector<TowerScenario> Scenarios;

    [[nodiscard]] const TowerScenario* FindById(std::string_view id) const;
    [[nodiscard]] const TowerScenario* FindByRoute(std::string_view routeId) const;
};

struct TowerScenarioResult
{
    std::optional<TowerScenario> Value;
    std::string Error;

    [[nodiscard]] bool Ok() const { return Value.has_value() && Error.empty(); }
};

struct TowerScenarioCatalogueResult
{
    TowerScenarioCatalogue Catalogue;
    std::vector<ContentIssue> Issues;

    [[nodiscard]] bool Ok() const { return Issues.empty(); }
};

struct TowerScenarioRunResult
{
    bool Valid = false;
    std::string Error;
    TowerFloorAttempt Attempt;
    /** True when any chosen decision was combat (TowerFloorAttempt has no combat flag; the runner owns it). */
    bool UsedCombat = false;
    /** True when a combat decision sat in a common_judgment beat; the content rules forbid it, so this stays false. */
    bool CombatFedCommonJudgment = false;
    std::vector<std::string> VisitedBeats;
    std::int32_t RecoveriesUsed = 0;
};

/** Decode and validate a single tower_scenario record. */
[[nodiscard]] PERFECTWORLDCORE_API TowerScenarioResult LoadTowerScenario(const ContentRecord& record);

/** Decode and validate all tower_scenario records in a ContentDb, including 9-route and 6-method coverage when complete. */
[[nodiscard]] PERFECTWORLDCORE_API TowerScenarioCatalogueResult LoadTowerScenarioCatalogue(
    const ContentDb& db,
    bool requireFullRouteCoverage = false);

/** Standalone loader for tower_scenario ContentFiles (usable before or after TopLevelSchemas registration). */
[[nodiscard]] PERFECTWORLDCORE_API TowerScenarioCatalogueResult LoadTowerScenarioFiles(
    std::span<const ContentFile> files,
    bool requireFullRouteCoverage = true);

/** Collect all localization text keys referenced by a scenario. */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<std::string> CollectTowerScenarioTextKeys(
    const TowerScenario& scenario);

/** Parse a flat or nested JSON localization bundle ("strings" object or top-level key->string pairs). */
[[nodiscard]] PERFECTWORLDCORE_API bool LoadTowerScenarioTextTable(
    std::string_view jsonText,
    std::map<std::string, std::string>& outTable,
    std::string& outError);

/** Validate that every text key used in the catalogue exists and is non-empty in the provided text table. */
PERFECTWORLDCORE_API void ValidateTowerScenarioTextKeys(
    const TowerScenarioCatalogue& catalogue,
    const std::map<std::string, std::string>& textTable,
    std::vector<ContentIssue>& issues);

/** Step through a scenario from EntryBeat to "complete" using explicit decision IDs (recovering automatically on mistakes). */
[[nodiscard]] PERFECTWORLDCORE_API TowerScenarioRunResult EvaluateTowerScenarioRun(
    const TowerScenario& scenario,
    std::span<const std::string_view> decisionIds);

/** Build the optimal non-combat run across all beats of a scenario. */
[[nodiscard]] PERFECTWORLDCORE_API TowerScenarioRunResult BuildOptimalNonCombatAttempt(
    const TowerScenario& scenario);

/** Build the optimal combat-enabled run across all beats of a combat-allowing scenario. */
[[nodiscard]] PERFECTWORLDCORE_API TowerScenarioRunResult BuildOptimalCombatAttempt(
    const TowerScenario& scenario);

/** Build a run that triggers a mistake on the given beat index and recovers through the beat's recovery path. */
[[nodiscard]] PERFECTWORLDCORE_API TowerScenarioRunResult BuildRecoveredMistakeAttempt(
    const TowerScenario& scenario,
    std::size_t mistakeBeatIndex);

namespace ContentSchema
{
struct TypeSchema;
[[nodiscard]] PERFECTWORLDCORE_API const TypeSchema& TowerScenarioSchema();
PERFECTWORLDCORE_API void ValidateTowerScenarioSemantics(
    const ContentDb& db,
    std::vector<ContentIssue>& issues);
} // namespace ContentSchema

} // namespace PerfectWorld
