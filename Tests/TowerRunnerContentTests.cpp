#include "TowerRunnerContent.h"

#include "Countdown.h"
#include "ExamContract.h"
#include "P2CTestSupport.h"
#include "TowerScenarioSelection.h"

#include <set>

using namespace PerfectWorld;

namespace
{
TowerSessionCommand P405Action(TowerSessionAction kind)
{
    TowerSessionCommand command;
    command.Kind = kind;
    return command;
}

void P405Projection(const ContentDb& db, const TowerRunnerCatalogue& runners)
{
    const auto graphs = LoadTowerScenarioCatalogue(db, true);
    P2CExpect(graphs.Ok() && runners.Scenarios.size() == 15, "all fifteen real scenarios retained");
    for (const auto& runner : runners.Scenarios)
    {
        const auto* graph = graphs.Catalogue.FindById(runner.Id);
        P2CExpect(graph && runner.RouteId == graph->RouteId && runner.TitleTextKey == graph->TitleKey &&
            runner.Beats.size() == graph->Beats.size(), "stable scenario identity and route");
        std::string cursor = graph->EntryBeat;
        for (const auto& beat : runner.Beats)
        {
            const auto source = std::find_if(graph->Beats.begin(), graph->Beats.end(),
                [&](const auto& item) { return item.Id == cursor; });
            P2CExpect(source != graph->Beats.end() && beat.Id == cursor && beat.PromptTextKey == source->PromptKey &&
                beat.Choices.size() == source->Decisions.size(), "graph order and authored choices preserved");
            std::int32_t ceiling = 0;
            for (const auto& decision : source->Decisions)
                if (!decision.IsMistake && !decision.IsCombat) ceiling = std::max(ceiling, decision.ScoreMilli);
            bool perfect = false, mistake = false;
            for (std::size_t i = 0; i < beat.Choices.size(); ++i)
            {
                const auto& choice = beat.Choices[i];
                const auto& original = source->Decisions[i];
                const auto earned = original.IsMistake ? source->Recovery.RestoredScoreMilli : original.ScoreMilli;
                const auto normalized = std::min(1000, earned * 1000 / ceiling);
                P2CExpect(choice.Id == original.Id && choice.TextKey == original.LabelKey &&
                    choice.MissionMinutes == original.Minutes + (original.IsMistake ? source->Recovery.Minutes : 0) &&
                    choice.P405CombatOnly == original.IsCombat && choice.Strain == (original.IsMistake ? 4 : 0) &&
                    choice.Recovery == 0, "stable choice IDs, recovery costs and method provenance");
                P2CExpect(choice.DisciplineMilli == (source->Metric == TowerScenarioMetric::Discipline ? normalized : 1000) &&
                    choice.CommonJudgmentMilli == (source->Metric == TowerScenarioMetric::CommonJudgment ? normalized : 1000),
                    "only the authored metric varies; combat never varies common judgment");
                perfect = perfect || (!choice.P405CombatOnly && choice.Strain == 0 &&
                    choice.DisciplineMilli == 1000 && choice.CommonJudgmentMilli == 1000);
                mistake = mistake || choice.Strain == 4;
            }
            P2CExpect(perfect && mistake, "each beat has a safe ceiling and a reserve-consuming mistake");
            cursor = source->Recovery.NextBeat;
        }
        P2CExpect(cursor == "complete", "linear adapter reaches graph completion");
    }
}

void P405Selection(const ContentDb& db, const TowerRunnerCatalogue& runners)
{
    const auto graphs = LoadTowerScenarioCatalogue(db, true);
    auto reversed = runners;
    std::reverse(reversed.Scenarios.begin(), reversed.Scenarios.end());
    std::set<std::string> seen;
    for (const auto& route : TowerRoutes())
    for (std::uint64_t seed = 0; seed < 64; ++seed)
    for (std::uint32_t attempt = 0; attempt < 4; ++attempt)
    {
        const auto* authored = graphs.Catalogue.P404BSelectForRoute(route.Id, seed, attempt);
        const auto* selected = P404BSelectRouteScenario(runners.Scenarios, route.Id, seed, attempt);
        const auto* reordered = P404BSelectRouteScenario(reversed.Scenarios, route.Id, seed, attempt);
        P2CExpect(authored && selected && reordered && selected->Id == authored->Id && reordered->Id == selected->Id,
            "graph and runner selectors use identical stable-ID pools");
        seen.insert(selected->Id);
    }
    P2CExpect(seen.size() == 15, "every real variant can be selected");
}

void P405EveryVariant(const TowerRunnerCatalogue& runners)
{
    CountdownProfile zero;
    zero.Final = true;
    for (const auto& target : runners.Scenarios)
    for (const bool eject : {false, true})
    {
        // Keep this target as its route's sole candidate; all other routes remain covered.
        auto one = runners;
        std::erase_if(one.Scenarios, [&](const auto& scenario) {
            return scenario.RouteId == target.RouteId && scenario.Id != target.Id;
        });
        P2CExpect(P401ValidateZeroPracticeRunner(one).Ok(), "every variant completes at zero preparation");
        TowerSessionState state;
        P2CExpect(ApplyTowerSession(state, one, {}, zero, CountdownEndInstant()).Ok(), "enter real runner");
        const auto* route = FindTowerRoute(target.RouteId);
        std::int64_t elapsed = 0;
        for (std::int32_t floor = 1; floor <= TowerFloorCount && state.Participation == TowerParticipation::Entered; ++floor)
        {
            const auto first = std::find_if(TowerRoutes().begin(), TowerRoutes().end(),
                [&](const auto& item) { return item.Floor == floor; });
            const auto& opened = floor == route->Floor ? *route : *first;
            auto command = P405Action(TowerSessionAction::OpenRoute);
            command.RouteId = opened.Id;
            command.Method = opened.Method;
            P2CExpect(ApplyTowerSession(state, one, command, zero, CountdownEndInstant()).Ok(), "open real route");
            while (state.Active)
            {
                const auto* active = P404BSelectRouteScenario(one.Scenarios, opened.Id, 0, 0);
                const auto& beat = active->Beats[state.Active->Beat];
                const bool mistake = floor == route->Floor && (eject || state.Active->Beat == 0);
                const auto choice = std::find_if(beat.Choices.begin(), beat.Choices.end(), [&](const auto& item) {
                    return !item.P405CombatOnly && (mistake ? item.Strain == 4 :
                        item.Strain == 0 && item.DisciplineMilli == 1000 && item.CommonJudgmentMilli == 1000);
                });
                P2CExpect(choice != beat.Choices.end(), "authored terminal path available");
                command = P405Action(TowerSessionAction::Choose);
                command.Runner = state.Runner;
                command.Beat = state.Active->Beat;
                command.ChoiceId = choice->Id;
                elapsed += choice->MissionMinutes;
                P2CExpect(ApplyTowerSession(state, one, command, zero, CountdownEndInstant()).Ok(), "play real action");
            }
        }
        P2CExpect(state.Participation == (eject ? TowerParticipation::Ejected : TowerParticipation::Completed) &&
            state.Attempts[static_cast<std::size_t>(route->Floor - 1)].ScenarioId == target.Id &&
            QueryTowerSession(state, one).Result.MissionMinutes == elapsed,
            "every variant supports one recovered mistake, repeated-mistake ejection and full authored time");
    }
}

void P405Failures(const ContentDb& db)
{
    P2CExpect(!LoadTowerRunners({}).Ok(), "empty catalogue refused");
    auto missing = db;
    missing.Records.erase("p2.mediation");
    const auto absent = LoadTowerRunners(missing);
    P2CExpect(!absent.Ok() && absent.Catalogue.Scenarios.empty(), "missing route publishes no partial catalogue");
    auto invalid = db;
    invalid.Records.erase("problem_case.xi_spring_mass_shm");
    P2CExpect(!LoadTowerRunners(invalid).Ok(), "classroom reference must resolve");
}
} // namespace

int main(int argc, char** argv)
{
    P2CExpect(argc == 2, "content root");
    const P2CFixture fixture(argv[1]);
    const auto loaded = LoadTowerRunners(fixture.Content);
    for (const auto& issue : loaded.Issues) std::cerr << issue.Id << ": " << issue.Message << '\n';
    P2CExpect(loaded.Ok() && ValidateTowerRunnerCatalogue(loaded.Catalogue).Ok() &&
        P401ValidateZeroPracticeRunner(loaded.Catalogue).Ok(), "production runners satisfy both contracts");
    P405Projection(fixture.Content, loaded.Catalogue);
    P405Selection(fixture.Content, loaded.Catalogue);
    P405EveryVariant(loaded.Catalogue);
    P405Failures(fixture.Content);
    std::cout << "P405 TowerRunnerContent passed\n";
}
