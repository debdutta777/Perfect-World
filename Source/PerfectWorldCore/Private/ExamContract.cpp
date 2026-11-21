#include "ExamContract.h"

#include "Countdown.h"

#include <algorithm>
#include <utility>

namespace PerfectWorld
{
namespace
{
using P401P = TowerParticipation;
using P401A = TowerSessionAction;
constexpr std::array<P401ExamEntryRule, 4> P401Entries{{
    {P401ExamEntry::Sit, P401P::Unset, P401A::Enter, P401P::Entered},
    {P401ExamEntry::Withdraw, P401P::Entered, P401A::Withdraw, P401P::Withdrawn},
    {P401ExamEntry::Defer, P401P::Unset, P401A::Defer, P401P::Deferred},
    {P401ExamEntry::Decline, P401P::Unset, P401A::Decline, P401P::Declined}
}};
constexpr std::array<P401ExamTransition, 8> P401Transitions{{
    {P401P::Unset, P401A::Enter, P401P::Entered},
    {P401P::Unset, P401A::Defer, P401P::Deferred},
    {P401P::Unset, P401A::Decline, P401P::Declined},
    {P401P::Entered, P401A::OpenRoute, P401P::Entered},
    {P401P::Entered, P401A::Choose, P401P::Entered},
    {P401P::Entered, P401A::Choose, P401P::Completed},
    {P401P::Entered, P401A::Choose, P401P::Ejected},
    {P401P::Entered, P401A::Withdraw, P401P::Withdrawn}
}};
constexpr std::array<P401ExamExitRule, 5> P401Exits{{
    {P401P::Completed, true, {}}, {P401P::Ejected, true, {}},
    {P401P::Withdrawn, false, "withdrawn"}, {P401P::Deferred, false, "deferred"},
    {P401P::Declined, false, "declined"}
}};
void P401Issue(P401ExamValidation& report, std::string_view id, std::string message)
{
    report.Issues.push_back({"P401.exam_contract", std::string(id), std::move(message)});
}
bool P401Continuation(const SchoolFollowupContent& content, const TowerSessionView& view)
{
    const auto outcome = SchoolFollowupOutcome(view);
    if (outcome.empty()) return false;
    const auto actions = QuerySchoolFollowups(content, view);
    const bool specific = std::any_of(actions.begin(), actions.end(), [&](const auto& action) {
        return action.Outcome == outcome && !action.AppointmentId.empty();
    });
    const bool offer = std::any_of(actions.begin(), actions.end(), [](const auto& action) {
        return !action.OfferId.empty() && !action.AppointmentId.empty();
    });
    return specific && offer;
}
void P401Receipt(P401ExamValidation& report, const SchoolFollowupContent& content,
    const std::vector<TowerFloorAttempt>& attempts, TowerParticipation participation)
{
    TowerSessionView view;
    view.Participation = participation;
    view.Result = ResolveTower(attempts);
    const bool matches = participation == P401P::Completed ? view.Result.Completed :
        participation == P401P::Ejected ? view.Result.Ejected : view.Result.Withdrew;
    if (!view.Result.Ok() || !matches || !P401Continuation(content, view))
        P401Issue(report, attempts.empty() ? "empty" : attempts.back().RouteId, "exit lacks a usable follow-up and offer");
}
/** Loaded scenario graphs are bounded, acyclic and have one successor per beat
 * after optional recovery. Follow graph IDs, never assume vector order is play order.
 */
void P401DecisionPaths(const TowerScenario& scenario, std::string_view cursor,
    std::vector<std::string_view>& choices, const std::vector<TowerFloorAttempt>& baseline,
    const SchoolFollowupContent& followups, P401ExamValidation& report)
{
    if (cursor == "complete")
    {
        ++report.DecisionSequences;
        const auto run = EvaluateTowerScenarioRun(scenario, choices);
        if (!run.Valid || run.CombatFedCommonJudgment)
        {
            P401Issue(report, scenario.Id, "authored choice sequence has no legal completion");
            return;
        }
        auto receipt = baseline;
        receipt[static_cast<std::size_t>(scenario.Floor - 1)] = run.Attempt;
        P401Receipt(report, followups, receipt, P401P::Completed);
        return;
    }
    const auto beat = std::find_if(scenario.Beats.begin(), scenario.Beats.end(),
        [&](const auto& item) { return item.Id == cursor; });
    if (beat == scenario.Beats.end() || choices.size() >= scenario.Beats.size())
    {
        P401Issue(report, scenario.Id, "authored choice sequence has no exit");
        return;
    }
    for (const auto& decision : beat->Decisions)
    {
        choices.push_back(decision.Id);
        P401DecisionPaths(scenario, decision.IsMistake ? beat->Recovery.NextBeat : decision.NextBeat,
            choices, baseline, followups, report);
        choices.pop_back();
    }
}
SchoolIssue P401RunnerIssue(std::string_view reason) { return {"P401.exam_contract." + std::string(reason), {}}; }
SchoolIssue P401RunFloor(TowerSessionState& state, const TowerRunnerCatalogue& runners,
    const TowerRouteDefinition& route, TowerMethod method, const CountdownProfile& zero)
{
    TowerSessionCommand command;
    command.Kind = P401A::OpenRoute;
    command.RouteId = route.Id;
    command.Method = method;
    auto applied = ApplyTowerSession(state, runners, command, zero, CountdownEndInstant());
    if (!applied.Ok()) return applied.Issue;
    const auto scenario = std::find_if(runners.Scenarios.begin(), runners.Scenarios.end(),
        [&](const auto& item) { return state.Active && item.Id == state.Active->ScenarioId; });
    if (scenario == runners.Scenarios.end()) return P401RunnerIssue("missing_runner");
    for (const auto& beat : scenario->Beats)
    {
        const auto safe = std::find_if(beat.Choices.begin(), beat.Choices.end(), [](const auto& choice) {
            return !choice.P405CombatOnly && choice.DisciplineMilli == TowerScoreMaxMilli && choice.CommonJudgmentMilli == TowerScoreMaxMilli &&
                choice.Recovery >= choice.Strain;
        });
        if (safe == beat.Choices.end()) return P401RunnerIssue("no_zero_practice_choice");
        const auto view = QueryTowerSession(state, runners);
        command = {};
        command.Kind = P401A::Choose;
        command.Runner = view.Runner;
        command.Beat = view.Beat;
        command.ChoiceId = safe->Id;
        applied = ApplyTowerSession(state, runners, command, zero, CountdownEndInstant());
        if (!applied.Ok()) return applied.Issue;
    }
    return {};
}
} // namespace

std::span<const P401ExamEntryRule> P401ExamEntries() noexcept { return P401Entries; }
std::span<const P401ExamTransition> P401ExamTransitions() noexcept { return P401Transitions; }
std::span<const P401ExamExitRule> P401ExamExits() noexcept { return P401Exits; }
bool P401ExamIsExit(TowerParticipation participation) noexcept
{
    return std::any_of(P401Exits.begin(), P401Exits.end(),
        [&](const auto& exit) { return exit.Participation == participation; });
}
GameInstant P401ExamMorning() noexcept { return CountdownEndInstant(); }
InteractionDeclaration P401ExamMissionClock() { return TowerMissionDeclaration(); }
P401ExamScoreDimensions P401ExamScoring() noexcept
{
    P401ExamScoreDimensions out;
    out.Evidence = {TowerScenarioMetric::Discipline, TowerScenarioMetric::CommonJudgment};
    out.Credentials = TowerCredentials();
    return out;
}
P401ExamStepResult P401ApplyExamChoice(const TowerSessionState& state, const TowerRunnerCatalogue& runners,
    const TowerSessionCommand& command, const CountdownProfile& preparation, GameInstant now,
    std::uint64_t worldSeed, std::uint32_t attemptNumber)
{
    P401ExamStepResult out;
    out.State = state;
    out.Resolution = ApplyTowerSession(out.State, runners, command, preparation, now, worldSeed, attemptNumber);
    return out;
}
std::vector<P401ExamRouteRow> P401ExamRouteMatrix(const TowerScenarioCatalogue& scenarios)
{
    std::vector<P401ExamRouteRow> rows;
    for (const auto& route : TowerRoutes())
    {
        P401ExamRouteRow row;
        row.RouteId = route.Id;
        row.Floor = route.Floor;
        row.DisciplineMask = route.DisciplineMask;
        row.Methods[static_cast<std::size_t>(route.Method)] = true;
        row.Methods[static_cast<std::size_t>(TowerMethod::Combat)] = route.AllowsCombat;
        if (const auto* scenario = scenarios.P404BSelectForRoute(route.Id, 0, 0))
        {
            row.ScenarioId = scenario->Id;
            row.NonCombatCompletes = true;
            row.CombatCompletes = route.AllowsCombat;
            // P405E: the ID represents the standalone draw; completion covers
            // every variant a life's seed and sitting ordinal could select.
            for (const auto& variant : scenarios.Scenarios)
            {
                if (variant.RouteId != route.Id) continue;
                const auto nonCombat = BuildOptimalNonCombatAttempt(variant);
                row.NonCombatCompletes = row.NonCombatCompletes && nonCombat.Valid &&
                    !nonCombat.UsedCombat && !nonCombat.CombatFedCommonJudgment;
                if (route.AllowsCombat)
                {
                    const auto combat = BuildOptimalCombatAttempt(variant);
                    row.CombatCompletes = row.CombatCompletes && combat.Valid &&
                        combat.UsedCombat && !combat.CombatFedCommonJudgment;
                }
            }
        }
        rows.push_back(std::move(row));
    }
    return rows;
}
P401ExamValidation P401ValidateExamContract(const ContentDb& db)
{
    P401ExamValidation report;
    const auto scenarios = LoadTowerScenarioCatalogue(db, true);
    const auto followups = LoadSchoolFollowupContent(db);
    report.Issues = scenarios.Issues;
    report.Issues.insert(report.Issues.end(), followups.Issues.begin(), followups.Issues.end());
    if (followups.Catalogue.Actions.empty()) P401Issue(report, "school_followup", "missing continuation content");
    report.Routes = P401ExamRouteMatrix(scenarios.Catalogue);
    if (!report.Ok()) return report;

    std::array<bool, TowerMethodCount> methods{};
    std::array<bool, AptitudePathCount> disciplines{};
    std::vector<TowerFloorAttempt> baseline;
    for (const auto& row : report.Routes)
    {
        if (!row.NonCombatCompletes || (row.Methods[static_cast<std::size_t>(TowerMethod::Combat)] && !row.CombatCompletes))
            P401Issue(report, row.RouteId, "supported method has no complete scenario path");
        for (std::size_t i = 0; i < methods.size(); ++i) methods[i] = methods[i] || row.Methods[i];
        const auto* route = FindTowerRoute(row.RouteId);
        for (std::size_t i = 0; i < disciplines.size(); ++i)
            disciplines[i] = disciplines[i] || TowerRouteCovers(*route, static_cast<AptitudePath>(i));
    }
    if (std::find(methods.begin(), methods.end(), false) != methods.end() ||
        std::find(disciplines.begin(), disciplines.end(), false) != disciplines.end())
        P401Issue(report, "matrix", "method or non-combat credential coverage is incomplete");
    for (std::int32_t floor = 1; floor <= TowerFloorCount; ++floor)
    {
        const auto route = std::find_if(TowerRoutes().begin(), TowerRoutes().end(),
            [&](const auto& item) { return item.Floor == floor; });
        if (route == TowerRoutes().end()) { P401Issue(report, "matrix", "floor has no route"); return report; }
        baseline.push_back(BuildOptimalNonCombatAttempt(*scenarios.Catalogue.P404BSelectForRoute(route->Id, 0, 0)).Attempt);
    }
    report.NonCombatClimb = ResolveTower(baseline).Completed;
    CountdownProfile zero;
    zero.Final = true;
    report.ZeroPracticeContentViable = report.NonCombatClimb;
    for (std::size_t i = 0; i < TowerMethodCount; ++i)
    {
        const auto conditions = TowerConditionsFromPreparation(zero, static_cast<TowerMethod>(i));
        report.ZeroPracticeContentViable = report.ZeroPracticeContentViable && conditions.Reserve > 0 && conditions.MinuteDiscount == 0;
    }
    if (!report.ZeroPracticeContentViable) P401Issue(report, "zero_practice", "non-combat completion unavailable without preparation");

    for (const auto& scenario : scenarios.Catalogue.Scenarios)
    {
        std::vector<std::string_view> choices;
        P401DecisionPaths(scenario, scenario.EntryBeat, choices, baseline, followups.Catalogue, report);
    }
    // P405E: all 27 route triples for one variant per route in a sitting, not
    // the Cartesian product of variant pools. Every variant's local paths were
    // resolved above. Separable floor scoring and rank-independent offers below
    // cover mixtures without assuming seed-zero variants are the only content.
    for (const auto& first : TowerRoutes()) if (first.Floor == 1)
    for (const auto& second : TowerRoutes()) if (second.Floor == 2)
    for (const auto& third : TowerRoutes()) if (third.Floor == 3)
    {
        ++report.RouteSequences;
        std::vector<TowerFloorAttempt> receipt;
        for (const auto* route : {&first, &second, &third})
            receipt.push_back(BuildOptimalNonCombatAttempt(*scenarios.Catalogue.P404BSelectForRoute(route->Id, 0, 0)).Attempt);
        P401Receipt(report, followups.Catalogue, receipt, P401P::Completed);
    }
    for (std::size_t depth = 0; depth < baseline.size(); ++depth)
    {
        std::vector<TowerFloorAttempt> prefix(baseline.begin(), baseline.begin() + depth);
        P401Receipt(report, followups.Catalogue, prefix, P401P::Withdrawn);
        auto defeat = baseline[depth];
        defeat.Defeated = true;
        prefix.push_back(defeat);
        P401Receipt(report, followups.Catalogue, prefix, P401P::Ejected);
    }
    // Classification probes, not fabricated scored receipts. Every future rank
    // band must have an outcome appointment PLUS a universally eligible offer.
    TowerSessionView probe;
    const auto empty = ResolveTower({});
    for (const auto& exit : P401Exits)
    {
        probe.Participation = exit.Participation;
        for (std::int32_t decile = 1; decile <= 10; ++decile)
        {
            probe.Result = empty;
            probe.Result.Decile = decile;
            probe.Result.OfferIds.clear();
            for (const auto& offer : TowerOffers())
                if (offer.MaxDecile == 10 && !offer.NeedsCredential && !offer.NextAction.empty())
                    probe.Result.OfferIds.push_back(offer.Id);
            if (!P401Continuation(followups.Catalogue, probe))
                P401Issue(report, SchoolFollowupOutcome(probe), "terminal band lacks an unconditional next step");
        }
    }
    return report;
}
SchoolIssue P401ValidateZeroPracticeRunner(const TowerRunnerCatalogue& runners)
{
    if (const auto issue = ValidateTowerRunnerCatalogue(runners); !issue.Ok()) return issue;
    CountdownProfile zero;
    zero.Final = true;
    for (const auto& scenario : runners.Scenarios)
    for (const bool combat : {false, true})
    {
        const auto& target = *FindTowerRoute(scenario.RouteId);
        if (combat && !target.AllowsCombat) continue;
        // P405D: isolate this variant on its route so the proof executes all
        // candidates, including those not selected by the standalone seed.
        auto pinned = runners;
        std::erase_if(pinned.Scenarios, [&](const auto& item) {
            return item.RouteId == target.Id && item.Id != scenario.Id;
        });
        TowerSessionState state;
        auto entered = ApplyTowerSession(state, pinned, {}, zero, CountdownEndInstant());
        if (!entered.Ok()) return entered.Issue;
        for (std::int32_t floor = 1; floor <= TowerFloorCount; ++floor)
        {
            const auto first = std::find_if(TowerRoutes().begin(), TowerRoutes().end(),
                [&](const auto& route) { return route.Floor == floor; });
            if (first == TowerRoutes().end()) return P401RunnerIssue("missing_floor");
            const auto& route = floor == target.Floor ? target : *first;
            const auto method = floor == target.Floor && combat ? TowerMethod::Combat : route.Method;
            if (const auto issue = P401RunFloor(state, pinned, route, method, zero); !issue.Ok()) return issue;
        }
        const auto view = QueryTowerSession(state, pinned);
        if (state.Participation != P401P::Completed || !view.Result.Ok() || !view.Result.Completed)
            return P401RunnerIssue("zero_practice_unfinished");
    }
    return {};
}
} // namespace PerfectWorld
