#include "TowerSession.h"

#include "Countdown.h"

#include <algorithm>
#include <limits>
#include <set>
#include <utility>

namespace PerfectWorld
{
namespace
{
SchoolIssue P2CTowerIssue(std::string_view reason) { return {"tower.session." + std::string(reason), {}}; }
const TowerRunnerScenario* P2CScenario(const TowerRunnerCatalogue& c, std::string_view route)
{
    for (const auto& s : c.Scenarios) if (s.RouteId == route) return &s;
    return nullptr;
}
bool P2CTowerTerminal(TowerParticipation p)
{
    return p != TowerParticipation::Unset && p != TowerParticipation::Entered;
}
bool P2CPreparationValid(const CountdownProfile& p)
{
    return p.Final && p.RouteDelayMinutes >= 0 && p.RouteDelayMinutes <= TowerWorldCapMinutes &&
        p.Composure >= 0 && p.Composure <= CountdownScoreMax &&
        std::all_of(p.Preparation.begin(), p.Preparation.end(), [](auto v) { return v >= 0 && v <= CountdownScoreMax; });
}
TowerResult P2CTowerScore(const TowerSessionState& state, bool includeActive)
{
    std::vector<TowerFloorAttempt> attempts;
    for (const auto& a : state.Attempts)
    {
        const auto* route = FindTowerRoute(a.RouteId);
        if (!route) { TowerResult bad; bad.Error = TowerError::UnknownRoute; return bad; }
        attempts.push_back({route->Id, a.Method, a.DisciplineMilli, a.CommonJudgmentMilli, a.MissionMinutes, a.Defeated});
    }
    auto result = ResolveTower(attempts);
    if (result.Ok())
    {
        // Unfinished work consumes mission time but supplies no credential or depth.
        if (includeActive && state.Active) result.MissionMinutes += state.Active->MissionMinutes;
        result.MissionMinutes += state.Preparation.RouteDelayMinutes;
        result.OutsideMinutes = MissionOutsideMinutes(TowerMissionDeclaration().Mission, result.MissionMinutes);
    }
    return result;
}
} // namespace

SchoolIssue ValidateTowerRunnerCatalogue(const TowerRunnerCatalogue& catalogue)
{
    if (catalogue.Scenarios.size() != TowerRoutes().size()) return P2CTowerIssue("route_coverage");
    std::set<std::string> routes, scenarios;
    for (const auto& s : catalogue.Scenarios)
    {
        if (!FindTowerRoute(s.RouteId) || !routes.insert(s.RouteId).second || !IsReceiptIdWellFormed(s.Id) ||
            !scenarios.insert(s.Id).second || !IsReceiptIdWellFormed(s.TitleTextKey) ||
            s.Beats.empty() || s.Beats.size() > TowerRunnerBeatLimit) return P2CTowerIssue("bad_scenario");
        std::set<std::string> beats;
        for (const auto& b : s.Beats)
        {
            if (!IsReceiptIdWellFormed(b.Id) || !beats.insert(b.Id).second || !IsReceiptIdWellFormed(b.PromptTextKey) ||
                b.ClueTextKeys.size() > 16 || b.Choices.empty() || b.Choices.size() > TowerRunnerChoiceLimit)
                return P2CTowerIssue("bad_beat");
            for (const auto& clue : b.ClueTextKeys) if (!IsReceiptIdWellFormed(clue)) return P2CTowerIssue("bad_clue");
            std::set<std::string> choices;
            bool safePerfect = false;
            for (const auto& c : b.Choices)
            {
                if (!IsReceiptIdWellFormed(c.Id) || !choices.insert(c.Id).second || !IsReceiptIdWellFormed(c.TextKey) ||
                    c.MissionMinutes < 1 || c.MissionMinutes > 60 || c.Strain < 0 || c.Strain > 4 ||
                    c.Recovery < 0 || c.Recovery > 4 || c.DisciplineMilli < 0 || c.DisciplineMilli > TowerScoreMaxMilli ||
                    c.CommonJudgmentMilli < 0 || c.CommonJudgmentMilli > TowerScoreMaxMilli)
                    return P2CTowerIssue("performance_bounds");
                safePerfect = safePerfect || (c.DisciplineMilli == TowerScoreMaxMilli && c.CommonJudgmentMilli == TowerScoreMaxMilli && c.Recovery >= c.Strain);
            }
            // Every route retains the original Tower ceiling even with zero preparation.
            if (!safePerfect) return P2CTowerIssue("ceiling_unreachable");
        }
    }
    return {};
}

TowerRunnerConditions TowerConditionsFromPreparation(const CountdownProfile& p, TowerMethod method)
{
    const auto index = static_cast<std::size_t>(method);
    if (index >= TowerMethodCount) return {};
    const auto prep = std::clamp(p.Preparation[index], 0, CountdownScoreMax);
    return {4 + prep / 250 + std::clamp(p.Composure, 0, CountdownScoreMax) / 500, prep / 500};
}

TowerSessionResult ApplyTowerSession(TowerSessionState& state, const TowerRunnerCatalogue& c, const TowerSessionCommand& command,
    const CountdownProfile& preparation, GameInstant now)
{
    TowerSessionResult result;
    if (now < 0 || now > DayLoopMaxInstant) { result.Issue = P2CTowerIssue("bad_time"); return result; }
    if (P2CTowerTerminal(state.Participation)) { result.Issue = P2CTowerIssue("closed"); return result; }
    auto next = state;
    switch (command.Kind)
    {
    case TowerSessionAction::Enter:
        if (next.Participation != TowerParticipation::Unset) { result.Issue = P2CTowerIssue("already_entered"); break; }
        if (now < CountdownEndInstant() || !P2CPreparationValid(preparation)) { result.Issue = P2CTowerIssue("not_ready"); break; }
        result.Issue = ValidateTowerRunnerCatalogue(c);
        if (!result.Issue.Ok()) break;
        next.Preparation = {preparation.Preparation, preparation.Composure, preparation.RouteDelayMinutes};
        next.Participation = TowerParticipation::Entered;
        break;
    case TowerSessionAction::OpenRoute:
    {
        if (next.Participation != TowerParticipation::Entered || next.Active) { result.Issue = P2CTowerIssue("busy"); break; }
        result.Issue = ValidateTowerRunnerCatalogue(c);
        if (!result.Issue.Ok()) break;
        const auto* route = FindTowerRoute(command.RouteId);
        const auto* scenario = P2CScenario(c, command.RouteId);
        if (!route || !scenario) { result.Issue = P2CTowerIssue("unknown_route"); break; }
        if (route->Floor != static_cast<std::int32_t>(next.Attempts.size()) + 1) { result.Issue = P2CTowerIssue("floor_order"); break; }
        if (command.Method != route->Method && !(command.Method == TowerMethod::Combat && route->AllowsCombat))
            { result.Issue = P2CTowerIssue("method_not_allowed"); break; }
        if (next.NextRunner == std::numeric_limits<std::uint64_t>::max()) { result.Issue = P2CTowerIssue("full"); break; }
        TowerRunnerRun run;
        run.ScenarioId = scenario->Id;
        run.RouteId = command.RouteId;
        run.Method = command.Method;
        CountdownProfile frozen;
        frozen.Preparation = next.Preparation.Preparation;
        frozen.Composure = next.Preparation.Composure;
        run.Conditions = TowerConditionsFromPreparation(frozen, command.Method);
        next.Runner = next.NextRunner++;
        next.Active = std::move(run);
        break;
    }
    case TowerSessionAction::Choose:
    {
        if (!next.Active || next.Runner != command.Runner || next.Active->Beat != command.Beat)
            { result.Issue = P2CTowerIssue("stale_beat"); break; }
        result.Issue = ValidateTowerRunnerCatalogue(c);
        if (!result.Issue.Ok()) break;
        auto& run = *next.Active;
        const auto* scenario = P2CScenario(c, run.RouteId);
        if (!scenario || scenario->Id != run.ScenarioId || run.Beat >= scenario->Beats.size())
            { result.Issue = P2CTowerIssue("invalid_state"); break; }
        const auto& beat = scenario->Beats[run.Beat];
        const auto choice = std::find_if(beat.Choices.begin(), beat.Choices.end(), [&](const auto& item) { return item.Id == command.ChoiceId; });
        if (choice == beat.Choices.end()) { result.Issue = P2CTowerIssue("unknown_choice"); break; }
        run.MissionMinutes += std::max(1, choice->MissionMinutes - run.Conditions.MinuteDiscount);
        const bool defeated = choice->Strain > run.Conditions.Reserve;
        run.Conditions.Reserve = std::clamp(run.Conditions.Reserve - choice->Strain + choice->Recovery, 0, 10);
        run.Choices.push_back(choice->Id);
        run.DisciplineSum += choice->DisciplineMilli;
        run.JudgmentSum += choice->CommonJudgmentMilli;
        ++run.Beat;
        if (defeated || run.Beat == scenario->Beats.size())
        {
            const auto divisor = static_cast<std::int32_t>(scenario->Beats.size());
            next.Attempts.push_back({run.RouteId, run.Method, run.DisciplineSum / divisor, run.JudgmentSum / divisor,
                run.MissionMinutes, defeated, run.ScenarioId, run.Choices});
            next.Active.reset();
            if (defeated) next.Participation = TowerParticipation::Ejected;
            else if (next.Attempts.size() == static_cast<std::size_t>(TowerFloorCount)) next.Participation = TowerParticipation::Completed;
        }
        break;
    }
    case TowerSessionAction::Withdraw:
        if (next.Participation != TowerParticipation::Entered) result.Issue = P2CTowerIssue("not_entered");
        else next.Participation = TowerParticipation::Withdrawn;
        break;
    case TowerSessionAction::Defer:
    case TowerSessionAction::Decline:
        if (next.Participation != TowerParticipation::Unset) result.Issue = P2CTowerIssue("already_entered");
        else next.Participation = command.Kind == TowerSessionAction::Defer ? TowerParticipation::Deferred : TowerParticipation::Declined;
        break;
    default: result.Issue = P2CTowerIssue("bad_command"); break;
    }
    if (!result.Ok()) return result;
    result.Result = P2CTowerScore(next, P2CTowerTerminal(next.Participation));
    if (!result.Result.Ok()) { result.Issue = {std::string(TowerErrorCode(result.Result.Error)), {}}; return result; }
    if (P2CTowerTerminal(next.Participation)) result.OutsideMinutes = result.Result.OutsideMinutes;
    state = std::move(next);
    return result;
}

TowerSessionView QueryTowerSession(const TowerSessionState& state, const TowerRunnerCatalogue& c)
{
    TowerSessionView view;
    view.Participation = state.Participation;
    view.NextFloor = static_cast<std::int32_t>(state.Attempts.size()) + 1;
    view.Runner = state.Runner;
    view.Result = P2CTowerScore(state, true);
    if (!state.Active)
    {
        if (state.Participation == TowerParticipation::Entered)
            for (const auto& route : TowerRoutes()) if (route.Floor == view.NextFloor) view.AvailableRouteIds.emplace_back(route.Id);
        return view;
    }
    const auto& run = *state.Active;
    const auto* scenario = P2CScenario(c, run.RouteId);
    if (!scenario || run.Beat >= scenario->Beats.size()) return view;
    view.Beat = run.Beat;
    view.Conditions = run.Conditions;
    const auto& beat = scenario->Beats[run.Beat];
    view.PromptTextKey = beat.PromptTextKey;
    view.ClueTextKeys = beat.ClueTextKeys;
    if (!P2CTowerTerminal(state.Participation))
        for (const auto& choice : beat.Choices)
            view.Choices.push_back({choice.Id, choice.TextKey, std::max(1, choice.MissionMinutes - run.Conditions.MinuteDiscount), choice.Strain});
    return view;
}
} // namespace PerfectWorld
