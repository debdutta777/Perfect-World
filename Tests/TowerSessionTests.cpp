#include "TowerSession.h"

#include "Countdown.h"
#include "TowerScenarioSelection.h"

#include "P2CTestSupport.h"

#include <set>

using namespace PerfectWorld;
namespace
{
TowerRunnerCatalogue P2CTowerContent()
{
    TowerRunnerCatalogue c;
    for (const auto& route : TowerRoutes())
    {
        TowerRunnerScenario s;
        s.Id = "scenario." + std::string(route.Id); s.RouteId = route.Id; s.TitleTextKey = "phase2.test.title";
        for (int beat = 0; beat < 3; ++beat)
        {
            TowerRunnerBeat b;
            b.Id = "beat." + std::to_string(beat); b.PromptTextKey = "phase2.test.prompt"; b.ClueTextKeys = {"phase2.test.clue"};
            b.Choices = {{"choice.careful", "phase2.test.careful", 5, 1, 1, 1000, 1000},
                {"choice.rush", "phase2.test.rush", 2, 4, 0, 200, 200}};
            s.Beats.push_back(b);
        }
        c.Scenarios.push_back(std::move(s));
    }
    return c;
}
TowerSessionResult P2CChoose(TowerSessionState& s, const TowerRunnerCatalogue& c, const CountdownProfile& prep, std::string choice)
{
    const auto v = QueryTowerSession(s, c);
    TowerSessionCommand command; command.Kind = TowerSessionAction::Choose;
    command.Runner = v.Runner; command.Beat = v.Beat; command.ChoiceId = std::move(choice);
    const auto result = ApplyTowerSession(s, c, command, prep, CountdownEndInstant());
    const auto after = s;
    P2CExpect(!ApplyTowerSession(s, c, command, prep, CountdownEndInstant()).Ok() && s == after, "duplicate beat cannot score twice");
    return result;
}
void P404BRunnerSelection()
{
    auto catalogue = P2CTowerContent();
    auto variant = catalogue.Scenarios.front();
    variant.Id += ".p404b";
    for (auto& beat : variant.Beats) beat.PromptTextKey = "phase2.test.variant";
    catalogue.Scenarios.push_back(variant);
    P2CExpect(ValidateTowerRunnerCatalogue(catalogue).Ok(), "runner permits multiple unique scenarios on one route");
    auto duplicate = catalogue;
    duplicate.Scenarios.push_back(variant);
    P2CExpect(!ValidateTowerRunnerCatalogue(duplicate).Ok(), "runner still refuses duplicate scenario IDs");
    auto missing = catalogue;
    missing.Scenarios.erase(missing.Scenarios.begin() + 1);
    P2CExpect(!ValidateTowerRunnerCatalogue(missing).Ok(), "runner refuses an uncovered route even at nine scenarios");
    CountdownProfile preparation; preparation.Final = true;
    std::set<std::string> drawn;
    for (std::uint32_t attempt = 0; attempt < 32; ++attempt)
    {
        TowerSessionState state;
        TowerSessionCommand command;
        P2CExpect(ApplyTowerSession(state, catalogue, command, preparation, CountdownEndInstant(), 404, attempt).Ok(), "multi-scenario runner enters");
        command.Kind = TowerSessionAction::OpenRoute;
        command.RouteId = variant.RouteId;
        command.Method = TowerRoutes().front().Method;
        P2CExpect(ApplyTowerSession(state, catalogue, command, preparation, CountdownEndInstant(), 404, attempt).Ok(), "multi-scenario route opens");
        const auto* selected = P404BSelectRouteScenario(catalogue.Scenarios, variant.RouteId, 404, attempt);
        P2CExpect(state.Active && state.Active->ScenarioId == selected->Id, "OpenRoute uses the shared keyed selection");
        drawn.insert(state.Active->ScenarioId);
        auto resumed = state;
        auto reordered = catalogue;
        std::reverse(reordered.Scenarios.begin(), reordered.Scenarios.end());
        P2CExpect(QueryTowerSession(resumed, reordered).PromptTextKey == selected->Beats.front().PromptTextKey,
            "view resolves stored scenario ID after catalogue reordering");
        for (std::size_t beat = 0; beat < selected->Beats.size(); ++beat)
        {
            command.Kind = TowerSessionAction::Choose; command.Runner = state.Runner;
            command.Beat = beat; command.ChoiceId = "choice.careful";
            P2CExpect(ApplyTowerSession(state, catalogue, command, preparation, CountdownEndInstant(), 404, attempt).Ok(), "selected runner scores");
            P2CExpect(ApplyTowerSession(resumed, reordered, command, preparation, CountdownEndInstant(), 999, 999).Ok() && resumed == state,
                "resumed choices use stored ID without redrawing from changed selection inputs");
        }
        P2CExpect(state.Attempts.back().ScenarioId == selected->Id, "completed attempt retains selected provenance");
    }
    P2CExpect(drawn.size() == 2, "both runner variants can be drawn");
}
} // namespace
int main()
{
    P404BRunnerSelection();
    auto c = P2CTowerContent();
    P2CExpect(ValidateTowerRunnerCatalogue(c).Ok(), "all nine routes within envelope");
    auto bad = c; bad.Scenarios.front().Beats.front().Choices.front().DisciplineMilli = 1001;
    P2CExpect(!ValidateTowerRunnerCatalogue(bad).Ok(), "out-of-envelope content refuses");
    CountdownProfile prep; prep.Final = true;
    for (const auto& route : TowerRoutes())
    for (const bool combat : {false, true})
    {
        if (combat && !route.AllowsCombat) continue;
        TowerSessionState s;
        TowerSessionCommand command;
        P2CExpect(ApplyTowerSession(s, c, command, prep, CountdownEndInstant()).Ok(), "enter optional examination");
        // Reach the selected route's floor using the first route on earlier floors.
        for (int floor = 1; floor <= route.Floor; ++floor)
        {
            const auto r = std::find_if(TowerRoutes().begin(), TowerRoutes().end(), [&](const auto& item) { return item.Floor == floor; });
            const auto& chosen = floor == route.Floor ? route : *r;
            command = {}; command.Kind = TowerSessionAction::OpenRoute; command.RouteId = chosen.Id;
            command.Method = floor == route.Floor && combat ? TowerMethod::Combat : chosen.Method;
            P2CExpect(ApplyTowerSession(s, c, command, prep, CountdownEndInstant()).Ok(), "existing route/method opens");
            for (int beat = 0; beat < 3; ++beat) P2CExpect(P2CChoose(s, c, prep, "choice.careful").Ok(), "core scores authored action");
            P2CExpect(s.Attempts.back().DisciplineMilli == 1000 && s.Attempts.back().CommonJudgmentMilli == 1000, "equal ceiling for all nine routes and six methods");
        }
    }
    TowerSessionState poor;
    TowerSessionCommand cmd;
    P2CExpect(ApplyTowerSession(poor, c, cmd, prep, CountdownEndInstant()).Ok(), "poor-run enter");
    cmd.Kind = TowerSessionAction::OpenRoute; cmd.RouteId = TowerRoutes().front().Id; cmd.Method = TowerRoutes().front().Method;
    P2CExpect(ApplyTowerSession(poor, c, cmd, prep, CountdownEndInstant()).Ok(), "poor-run route");
    P2CExpect(P2CChoose(poor, c, prep, "choice.rush").Ok(), "first strenuous beat");
    const auto ejected = P2CChoose(poor, c, prep, "choice.rush");
    P2CExpect(ejected.Ok() && poor.Participation == TowerParticipation::Ejected && ejected.Result.Decile == 10 && ejected.Result.OfferIds.size() >= 3,
        "defeat costs rank and retains actionable bottom-decile offers");
    for (const auto kind : {TowerSessionAction::Decline, TowerSessionAction::Defer})
    {
        TowerSessionState s; cmd = {}; cmd.Kind = kind;
        const auto result = ApplyTowerSession(s, {}, cmd, {}, 0);
        P2CExpect(result.Ok() && result.OutsideMinutes == 0 && result.Result.OfferIds.size() >= 3, "declined/deferred ordinary futures");
    }
    TowerSessionState s; cmd = {};
    P2CExpect(!ApplyTowerSession(s, c, cmd, prep, CountdownEndInstant() - 1).Ok(), "fixed examination boundary");
    P2CExpect(ApplyTowerSession(s, c, cmd, prep, CountdownEndInstant()).Ok(), "withdrawal enter");
    cmd.Kind = TowerSessionAction::OpenRoute; cmd.RouteId = TowerRoutes().front().Id; cmd.Method = TowerRoutes().front().Method;
    P2CExpect(ApplyTowerSession(s, c, cmd, prep, CountdownEndInstant()).Ok(), "withdrawal route");
    P2CExpect(P2CChoose(s, c, prep, "choice.careful").Ok(), "partial floor time");
    cmd = {}; cmd.Kind = TowerSessionAction::Withdraw;
    const auto withdrew = ApplyTowerSession(s, c, cmd, prep, CountdownEndInstant());
    P2CExpect(withdrew.Ok() && withdrew.Result.MissionMinutes == 5 && withdrew.OutsideMinutes > 0 && withdrew.Result.Credentials.empty(),
        "withdrawal pays unfinished runner time without fabricated credentials");
    auto high = prep; high.Preparation.fill(1000); high.Composure = 1000;
    for (std::size_t method = 0; method < TowerMethodCount; ++method)
    {
        const auto lowConditions = TowerConditionsFromPreparation(prep, static_cast<TowerMethod>(method));
        const auto highConditions = TowerConditionsFromPreparation(high, static_cast<TowerMethod>(method));
        P2CExpect(highConditions.Reserve == 10 && lowConditions.Reserve == 4 && highConditions.MinuteDiscount == 2,
            "preparation changes runner resources and costs, never self-awarded scores");
    }
    std::cout << "P2C TowerSession passed\n";
}
