#include "TowerRunnerContent.h"

#include "ExamContract.h"

#include <algorithm>
#include <utility>

namespace PerfectWorld
{
namespace
{
bool P405Adapt(const TowerScenario& source, TowerRunnerScenario& target)
{
    target.Id = source.Id;
    target.RouteId = source.RouteId;
    target.TitleTextKey = source.TitleKey;
    std::string cursor = source.EntryBeat;
    while (cursor != "complete")
    {
        const auto found = std::find_if(source.Beats.begin(), source.Beats.end(),
            [&](const auto& beat) { return beat.Id == cursor; });
        if (found == source.Beats.end() || target.Beats.size() >= TowerRunnerBeatLimit) return false;
        const auto& beat = *found;
        std::int32_t ceiling = 0;
        for (const auto& decision : beat.Decisions)
            if (!decision.IsCombat && !decision.IsMistake) ceiling = std::max(ceiling, decision.ScoreMilli);
        if (ceiling == 0) return false;
        TowerRunnerBeat converted;
        converted.Id = beat.Id;
        converted.PromptTextKey = beat.PromptKey;
        for (const auto& clue : beat.Clues) converted.ClueTextKeys.push_back(clue.TextKey);
        for (const auto& decision : beat.Decisions)
        {
            TowerRunnerChoice choice;
            choice.Id = decision.Id;
            choice.TextKey = decision.LabelKey;
            choice.MissionMinutes = decision.Minutes + (decision.IsMistake ? beat.Recovery.Minutes : 0);
            // Do not silently truncate authored recovery time to the runner bound.
            if (choice.MissionMinutes > 60) return false;
            choice.Strain = decision.IsMistake ? 4 : 0;
            const auto earned = decision.IsMistake ? beat.Recovery.RestoredScoreMilli : decision.ScoreMilli;
            const auto normalized = std::min(TowerScoreMaxMilli, earned * TowerScoreMaxMilli / ceiling);
            choice.DisciplineMilli = beat.Metric == TowerScenarioMetric::Discipline ? normalized : TowerScoreMaxMilli;
            choice.CommonJudgmentMilli = beat.Metric == TowerScenarioMetric::CommonJudgment ? normalized : TowerScoreMaxMilli;
            choice.P405CombatOnly = decision.IsCombat;
            converted.Choices.push_back(std::move(choice));
        }
        target.Beats.push_back(std::move(converted));
        cursor = beat.Recovery.NextBeat;
    }
    return target.Beats.size() == source.Beats.size();
}
} // namespace

TowerRunnerCatalogueResult LoadTowerRunners(const ContentDb& db)
{
    TowerRunnerCatalogueResult out;
    const auto source = LoadTowerScenarioCatalogue(db, true);
    out.Issues = source.Issues;
    if (!out.Ok()) return out;
    for (const auto& scenario : source.Catalogue.Scenarios)
    {
        TowerRunnerScenario runner;
        if (!P405Adapt(scenario, runner))
            out.Issues.push_back({"tower_scenario", scenario.Id, "P405.runner.unsupported_projection"});
        else out.Catalogue.Scenarios.push_back(std::move(runner));
    }
    if (out.Ok())
        if (const auto issue = ValidateTowerRunnerCatalogue(out.Catalogue); !issue.Ok())
            out.Issues.push_back({"tower_scenario", {}, issue.Code});
    if (out.Ok())
        if (const auto issue = P401ValidateZeroPracticeRunner(out.Catalogue); !issue.Ok())
            out.Issues.push_back({"tower_scenario", {}, issue.Code});
    if (!out.Ok()) out.Catalogue = {};
    return out;
}
} // namespace PerfectWorld
