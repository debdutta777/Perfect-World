#include "ExamContract.h"

#include "Countdown.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <utility>

using namespace PerfectWorld;
namespace
{
std::size_t P401Assertions = 0;
void P401Require(bool value, std::string_view message)
{
    ++P401Assertions;
    if (!value) { std::cerr << "P401 FAIL: " << message << '\n'; std::exit(1); }
}
ContentDb P401Read(const std::filesystem::path& root)
{
    ContentDb db;
    for (const auto* type : {"tower_scenario", "school_followup", "problem_case"})
    {
        std::error_code error;
        std::filesystem::directory_iterator it(root / type, error), end;
        P401Require(!error, "real content directory opens");
        while (it != end)
        {
            if (it->path().extension() == ".json" && it->is_regular_file(error))
            {
                std::ifstream input(it->path(), std::ios::binary);
                P401Require(input.good(), "real content record opens");
                const std::string text{std::istreambuf_iterator<char>(input), {}};
                auto parsed = ParseJson(text);
                P401Require(parsed.Value && parsed.Value->IsObject(), "real JSON parses");
                const auto* id = parsed.Value->Find("id");
                const auto* actualType = parsed.Value->Find("type");
                P401Require(id && id->IsString() && actualType && actualType->String == type, "record identity matches folder");
                const auto key = id->String;
                P401Require(db.Records.emplace(key, ContentRecord{type, key, it->path().generic_string(),
                    std::move(*parsed.Value), {}}).second, "real content IDs are unique");
            }
            P401Require(!error, "file metadata is readable");
            it.increment(error);
            P401Require(!error, "content directory iteration succeeds");
        }
    }
    return db;
}
JsonValue& P401Field(JsonValue& object, std::string_view name)
{
    for (auto& [key, value] : object.Object) if (key == name) return value;
    P401Require(false, "mutation field exists");
    std::exit(2);
}
void P401FrozenRules()
{
    P401Require(P401ExamContractVersion == 1 && P401ExamEntries().size() == 4, "four entry choices frozen");
    P401Require(P401ExamTransitions().size() == 8 && P401ExamExits().size() == 5, "accepted transitions and five exits frozen");
    P401Require(!P401ExamIsExit(TowerParticipation::Unset) && !P401ExamIsExit(TowerParticipation::Entered), "live states are not exits");
    for (const auto& exit : P401ExamExits()) P401Require(P401ExamIsExit(exit.Participation), "every terminal state is an exit");
    P401Require(!P401ExamIsExit(static_cast<TowerParticipation>(255)), "unknown participation is not a supported exit");
    const auto morning = GameClock::ToCalendar(P401ExamMorning());
    P401Require(morning.Year == 2043 && morning.Month == 2 && morning.Day == 2 && morning.Hour == 6 && morning.Minute == 0,
        "national examination remains 2 February 2043 at 06:00");
    const auto clock = P401ExamMissionClock();
    P401Require(clock.Mode == ClockMode::Mission && clock.Interrupt == InterruptPolicy::Immediate &&
        clock.Resolve == ResolutionBoundary::OnComplete, "mission can be interrupted and resolves at exit");
    P401Require(clock.Mission.CapMinutes == 180 && clock.Mission.RatePermille == 250 &&
        clock.Mission.FixedMinutes == 0 && clock.CostMinutes == 180, "outside rate and cap frozen");
    const std::array<std::int64_t, 8> mission{0, 1, 3, 4, 719, 720, 721, 10000};
    const std::array<std::int64_t, 8> outside{0, 0, 0, 1, 179, 180, 180, 180};
    for (std::size_t i = 0; i < mission.size(); ++i)
        P401Require(MissionOutsideMinutes(clock.Mission, mission[i]) == outside[i],
            "mission rounding and single outside cap retain frozen boundary values");
    P401Require(!TimeModeRule(ClockMode::Mission).ManualSaveAllowed,
        "automatic recovery snapshots do not change the mission manual-save rule");
    const auto scoring = P401ExamScoring();
    P401Require(scoring.DisciplinePercent == 80 && scoring.JudgmentPercent == 20 &&
        scoring.EvidenceCeiling == 1000 && scoring.DepthPerClear == 1000 && scoring.DefeatPenalty == 500,
        "scoring dimensions retain existing Tower tuning");
    P401Require(scoring.Evidence.size() == 2 && scoring.Credentials.size() == AptitudePathCount,
        "two evidence dimensions and ten credential disciplines");
    const auto resume = P401ExamResumeRules();
    P401Require(resume.AfterEveryAcceptedCommand && resume.PreserveActiveFloor && resume.PreserveFrozenPreparation &&
        resume.PreserveRunnerAndBeat && resume.ReplayChoiceProvenance && resume.PublishExitAndOutsideChargeTogether &&
        resume.TerminalStateIsImmutable, "resume contract preserves progress and an atomic terminal charge");
}
void P401RealContent(const ContentDb& db)
{
    const auto report = P401ValidateExamContract(db);
    for (const auto& issue : report.Issues) std::cerr << issue.Id << ": " << issue.Message << '\n';
    P401Require(report.Ok() && report.NonCombatClimb && report.ZeroPracticeContentViable, "real content has a complete zero-practice non-combat path");
    P401Require(report.Routes.size() == 9 && report.DecisionSequences == 243 && report.RouteSequences == 27,
        "all nine real routes, 243 local choices and 27 route combinations visited");
    const auto catalogue = LoadTowerScenarioCatalogue(db, true);
    P401Require(catalogue.Ok(), "real scenario references and both scoring metrics resolve");
    std::array<bool, TowerMethodCount> methods{};
    for (const auto& row : report.Routes)
    {
        const auto* route = FindTowerRoute(row.RouteId);
        P401Require(route && row.Floor == route->Floor && row.DisciplineMask == route->DisciplineMask, "matrix projects core route authority");
        P401Require(row.NonCombatCompletes && row.CombatCompletes == route->AllowsCombat && !row.ScenarioId.empty(),
            "each allowed method has a real completion");
        for (std::size_t i = 0; i < methods.size(); ++i)
        {
            const auto method = static_cast<TowerMethod>(i);
            P401Require(row.Methods[i] == (method == route->Method || (route->AllowsCombat && method == TowerMethod::Combat)),
                "matrix contains exactly the methods allowed by this route");
            methods[i] = methods[i] || row.Methods[i];
        }
    }
    P401Require(std::all_of(methods.begin(), methods.end(), [](bool value) { return value; }), "all six methods covered by selectable routes");
    const auto followups = LoadSchoolFollowupContent(db);
    const std::array<std::string_view, 3> routes{"tower.route.pace", "tower.route.changed_rule", "tower.route.alliance"};
    for (const bool strong : {true, false})
    {
        std::vector<TowerFloorAttempt> attempts;
        for (const auto id : routes)
        {
            const auto& scenario = *catalogue.Catalogue.FindByRoute(id);
            if (strong) attempts.push_back(BuildOptimalNonCombatAttempt(scenario).Attempt);
            else
            {
                std::vector<std::string_view> choices;
                for (const auto& beat : scenario.Beats)
                {
                    const TowerScenarioDecision* lowest = nullptr;
                    for (const auto& decision : beat.Decisions)
                        if (!decision.IsCombat && !decision.IsMistake && (!lowest || decision.ScoreMilli < lowest->ScoreMilli)) lowest = &decision;
                    P401Require(lowest != nullptr, "real low-evidence non-combat choice exists");
                    choices.push_back(lowest->Id);
                }
                const auto run = EvaluateTowerScenarioRun(scenario, choices);
                P401Require(run.Valid && !run.UsedCombat, "real low-evidence path completes");
                attempts.push_back(run.Attempt);
            }
        }
        TowerSessionView view;
        view.Participation = TowerParticipation::Completed;
        view.Result = ResolveTower(attempts);
        const auto outcome = strong ? "strong" : "middle";
        P401Require(view.Result.Completed && SchoolFollowupOutcome(view) == outcome, "real decisions produce strong and middle witnesses");
        const auto actions = QuerySchoolFollowups(followups.Catalogue, view);
        P401Require(std::any_of(actions.begin(), actions.end(), [&](const auto& action) { return action.Outcome == outcome; }),
            "real result has its outcome appointment");
    }
    // All six outcomes must have an appointment AND an eligible Tower offer.
    for (const auto& exit : P401ExamExits())
    for (const auto decile : {1, 2, 5, 6, 10})
    {
        TowerSessionView view;
        view.Participation = exit.Participation;
        view.Result = ResolveTower({});
        view.Result.Decile = decile;
        const auto outcome = SchoolFollowupOutcome(view);
        const auto actions = QuerySchoolFollowups(followups.Catalogue, view);
        P401Require(!outcome.empty() && std::any_of(actions.begin(), actions.end(), [&](const auto& action) {
            return action.Outcome == outcome && !action.AppointmentId.empty(); }), "terminal bands expose their specific next action");
        P401Require(std::any_of(actions.begin(), actions.end(), [](const auto& action) { return !action.OfferId.empty(); }),
            "terminal bands expose an eligible actionable offer");
    }
    P401Require(!P401ValidateExamContract({}).Ok(), "empty content cannot pass vacuously");
}
void P401BrokenContent(const ContentDb& db)
{
    for (const auto* record : db.OfType("tower_scenario"))
    {
        auto missing = db;
        missing.Records.erase(record->Id);
        P401Require(!P401ValidateExamContract(missing).Ok(), "every real route is mandatory");
    }
    for (const auto* outcome : {"strong", "middle", "poor", "withdrawn", "deferred", "declined"})
    {
        auto missing = db;
        missing.Records.erase("school_followup.p2." + std::string(outcome));
        P401Require(!P401ValidateExamContract(missing).Ok(), "each of six outcome continuations is mandatory");
    }
    auto missing = db;
    for (const auto* record : db.OfType("school_followup")) missing.Records.erase(record->Id);
    P401Require(!P401ValidateExamContract(missing).Ok(), "absent follow-up pack is refused");
    for (const auto* offer : {"apprenticeship", "study_bridge", "community_practice", "retake"})
    {
        auto broken = db;
        broken.Records.erase("school_followup.p2." + std::string(offer));
        P401Require(!P401ValidateExamContract(broken).Ok(), "rank-independent offers retain actual content mappings");
    }
    auto broken = db;
    auto& data = broken.Records.find("p2.pace")->second.Data;
    auto& firstBeat = P401Field(data, "beats").Array.front();
    P401Field(P401Field(firstBeat, "recovery"), "nextBeat").String = "beat.missing";
    P401Require(!P401ValidateExamContract(broken).Ok(), "broken recovery cannot strand a choice");
    broken = db;
    auto& encounter = broken.Records.find("p2.encounter")->second.Data;
    auto& finalBeat = P401Field(encounter, "beats").Array.back();
    auto& decision = P401Field(finalBeat, "decisions").Array.front();
    P401Field(decision, "isCombat").Bool = true;
    P401Field(decision, "method").String = "combat";
    P401Require(!P401ValidateExamContract(broken).Ok(), "combat cannot feed common judgment");
    broken = db;
    P401Field(broken.Records.find("p2.changed_rule")->second.Data, "classroomModel").String = "problem_case.missing";
    P401Require(!P401ValidateExamContract(broken).Ok(), "floor-two classroom model must really exist");
}
TowerRunnerCatalogue P401RunnerFixture()
{
    // A separate rule-layer test double, NOT a conversion of tower_scenario data.
    TowerRunnerCatalogue runners;
    for (const auto& route : TowerRoutes())
    {
        TowerRunnerScenario scenario;
        scenario.Id = "p401." + std::string(route.Id);
        scenario.RouteId = route.Id;
        scenario.TitleTextKey = "p401.title";
        for (const auto* id : {"p401.beat.a", "p401.beat.b", "p401.beat.c"})
        {
            TowerRunnerBeat beat;
            beat.Id = id;
            beat.PromptTextKey = "p401.prompt";
            beat.Choices = {{"p401.safe", "p401.safe.text", 60, 1, 1, 1000, 1000},
                {"p401.risk", "p401.risk.text", 2, 4, 0, 0, 0}};
            scenario.Beats.push_back(std::move(beat));
        }
        runners.Scenarios.push_back(std::move(scenario));
    }
    return runners;
}
void P401TerminalRules(const TowerSessionState& state, const TowerRunnerCatalogue& runners,
    const CountdownProfile& preparation, const SchoolFollowupContent& followups)
{
    P401Require(P401ExamIsExit(state.Participation), "terminal fixture is a supported exit");
    const auto view = QueryTowerSession(state, runners);
    const auto outcome = SchoolFollowupOutcome(view);
    const auto actions = QuerySchoolFollowups(followups, view);
    P401Require(!outcome.empty() && std::any_of(actions.begin(), actions.end(), [&](const auto& action) {
        return action.Outcome == outcome && !action.AppointmentId.empty(); }), "actual exit exposes its real outcome appointment");
    P401Require(std::any_of(actions.begin(), actions.end(), [](const auto& action) { return !action.OfferId.empty(); }),
        "actual exit exposes an eligible offer");
    for (const auto action : {TowerSessionAction::Enter, TowerSessionAction::OpenRoute, TowerSessionAction::Choose,
        TowerSessionAction::Withdraw, TowerSessionAction::Defer, TowerSessionAction::Decline})
    {
        TowerSessionCommand command;
        command.Kind = action;
        const auto again = P401ApplyExamChoice(state, runners, command, preparation, P401ExamMorning());
        P401Require(!again.Resolution.Ok() && again.State == state && again.Resolution.OutsideMinutes == 0,
            "every terminal state refuses re-entry and a duplicate charge");
    }
}
void P401Withdrawal(const TowerSessionState& state, const TowerRunnerCatalogue& runners,
    const CountdownProfile& preparation, const SchoolFollowupContent& followups)
{
    const auto before = state;
    const auto view = QueryTowerSession(state, runners);
    TowerSessionCommand command;
    command.Kind = TowerSessionAction::Withdraw;
    // Exit remains available if runner content becomes unavailable mid-session.
    const auto result = P401ApplyExamChoice(state, {}, command, {}, P401ExamMorning());
    P401Require(result.Resolution.Ok() && result.State.Participation == TowerParticipation::Withdrawn && state == before,
        "withdrawal from every reachable boundary or beat is pure and needs no catalogue");
    P401Require(result.State.Active == state.Active && result.State.Attempts == state.Attempts &&
        result.Resolution.Result.Credentials == view.Result.Credentials &&
        result.Resolution.Result.MissionMinutes == view.Result.MissionMinutes &&
        result.Resolution.OutsideMinutes == view.Result.OutsideMinutes,
        "withdrawal preserves the recovery snapshot and counts only completed evidence plus all elapsed time");
    P401TerminalRules(result.State, runners, preparation, followups);
}
void P401SessionRules(const SchoolFollowupContent& followups)
{
    const auto runners = P401RunnerFixture();
    P401Require(P401ValidateZeroPracticeRunner(runners).Ok(), "runner proof actually completes every route and method with zero practice");
    auto broken = runners;
    broken.Scenarios.back().Beats.back().Choices.front().Recovery = 0;
    P401Require(!P401ValidateZeroPracticeRunner(broken).Ok(), "runner with no zero-practice ceiling is refused");
    CountdownProfile zero;
    zero.Final = true;
    zero.RouteDelayMinutes = 180;
    const TowerSessionState initial;
    auto unfinished = zero;
    unfinished.Final = false;
    const auto notReady = P401ApplyExamChoice(initial, runners, {}, unfinished, P401ExamMorning());
    P401Require(!notReady.Resolution.Ok() && notReady.State == initial, "unfinished preparation refuses sit without mutation");
    const auto early = P401ApplyExamChoice(initial, runners, {}, zero, P401ExamMorning() - 1);
    P401Require(!early.Resolution.Ok() && early.State == initial, "early sit refuses without mutation");
    const auto absent = P401ApplyExamChoice(initial, {}, {}, zero, P401ExamMorning());
    P401Require(!absent.Resolution.Ok() && absent.State == initial && absent.Resolution.OutsideMinutes == 0,
        "sit requires a complete runner catalogue without mutating its input");
    for (const auto action : {TowerSessionAction::Defer, TowerSessionAction::Decline})
    {
        TowerSessionCommand choice;
        choice.Kind = action;
        const auto exit = P401ApplyExamChoice(initial, {}, choice, {}, P401ExamMorning());
        P401Require(exit.Resolution.Ok() && P401ExamIsExit(exit.State.Participation) &&
            exit.Resolution.OutsideMinutes == 0, "defer and decline require neither preparation nor runner content");
        P401TerminalRules(exit.State, runners, zero, followups);
    }
    auto entered = P401ApplyExamChoice(initial, runners, {}, zero, P401ExamMorning());
    P401Require(entered.Resolution.Ok() && initial.Participation == TowerParticipation::Unset &&
        entered.State.Participation == TowerParticipation::Entered && entered.Resolution.OutsideMinutes == 0, "pure sit returns a candidate and no world charge");
    P401Require(entered.Resolution.Result.OutsideMinutes == 45 && !P401ExamIsExit(entered.State.Participation),
        "route-delay cost in a nested preview does not authorise an outside-world charge");
    TowerSessionCommand command;
    command.Kind = TowerSessionAction::OpenRoute;
    command.RouteId = TowerRoutes().front().Id;
    command.Method = TowerRoutes().front().Method;
    auto altered = zero;
    altered.Preparation.fill(1000);
    auto opened = P401ApplyExamChoice(entered.State, runners, command, altered, P401ExamMorning());
    P401Require(opened.Resolution.Ok() && opened.State.Active->Conditions.Reserve == 4, "preparation remains the entry snapshot");
    auto invalid = command;
    invalid.RouteId = "tower.route.missing";
    const auto unknown = P401ApplyExamChoice(entered.State, runners, invalid, zero, P401ExamMorning());
    P401Require(!unknown.Resolution.Ok() && unknown.State == entered.State && unknown.Resolution.OutsideMinutes == 0,
        "unknown route cannot strand a candidate or charge time");
    invalid = command;
    invalid.Method = TowerMethod::Negotiation;
    const auto wrongMethod = P401ApplyExamChoice(entered.State, runners, invalid, zero, P401ExamMorning());
    P401Require(!wrongMethod.Resolution.Ok() && wrongMethod.State == entered.State && wrongMethod.Resolution.OutsideMinutes == 0,
        "unsupported route method refuses without changing the recovery snapshot");
    command = {};
    command.Kind = TowerSessionAction::Choose;
    command.Runner = opened.State.Runner;
    command.ChoiceId = "p401.safe";
    const auto beat = P401ApplyExamChoice(opened.State, runners, command, zero, P401ExamMorning());
    const auto resumed = P401ApplyExamChoice(TowerSessionState(opened.State), runners, command, zero, P401ExamMorning());
    P401Require(beat.Resolution.Ok() && beat.State == resumed.State && beat.Resolution.Result == resumed.Resolution.Result,
        "in-memory resume preserves nonce, evidence and elapsed time; no codec claim");
    const auto duplicate = P401ApplyExamChoice(beat.State, runners, command, zero, P401ExamMorning());
    P401Require(!duplicate.Resolution.Ok() && duplicate.State == beat.State && duplicate.Resolution.OutsideMinutes == 0,
        "stale beat cannot duplicate time or evidence");
    const auto beforeRead = beat.State;
    (void)QueryTowerSession(beat.State, runners);
    P401Require(beat.State == beforeRead, "reading preserves the whole resume point");
    command = {};
    command.Kind = TowerSessionAction::Withdraw;
    const auto withdrew = P401ApplyExamChoice(beat.State, runners, command, zero, P401ExamMorning());
    P401Require(withdrew.Resolution.Ok() && withdrew.State.Participation == TowerParticipation::Withdrawn &&
        withdrew.Resolution.Result.MissionMinutes == 240 && withdrew.Resolution.OutsideMinutes == 60 &&
        withdrew.Resolution.Result.Credentials.empty(), "active time and route delay charged without unfinished credentials");
    P401TerminalRules(withdrew.State, runners, zero, followups);
    for (const auto& entry : P401ExamEntries())
    {
        command = {};
        command.Kind = entry.Action;
        const auto& state = entry.From == TowerParticipation::Entered ? entered.State : initial;
        const auto result = P401ApplyExamChoice(state, runners, command, zero, P401ExamMorning());
        P401Require(result.Resolution.Ok() && result.State.Participation == entry.To, "entry table agrees with TowerSession");
        if (P401ExamIsExit(result.State.Participation)) P401TerminalRules(result.State, runners, zero, followups);
    }
    command = {};
    command.Kind = TowerSessionAction::Choose;
    command.Runner = opened.State.Runner;
    command.ChoiceId = "p401.risk";
    const auto strained = P401ApplyExamChoice(opened.State, runners, command, zero, P401ExamMorning());
    P401Require(strained.Resolution.Ok() && strained.State.Active.has_value(), "first risky choice consumes reserve");
    command.Beat = strained.State.Active->Beat;
    const auto ejected = P401ApplyExamChoice(strained.State, runners, command, zero, P401ExamMorning());
    P401Require(ejected.Resolution.Ok() && ejected.State.Participation == TowerParticipation::Ejected &&
        ejected.Resolution.Result.Ejected && ejected.Resolution.Result.ScorePoints == -500 &&
        ejected.Resolution.Result.Credentials.empty(), "defeat costs rank without injury or invented credentials");
    P401TerminalRules(ejected.State, runners, zero, followups);
    const auto poorView = QueryTowerSession(ejected.State, runners);
    const auto poorActions = QuerySchoolFollowups(followups, poorView);
    P401Require(SchoolFollowupOutcome(poorView) == "poor" &&
        std::any_of(poorActions.begin(), poorActions.end(), [](const auto& action) { return action.Outcome == "poor"; }),
        "an actual ejected runner receives the real poor-result continuation");
    auto state = entered.State;
    for (std::int32_t floor = 1; floor <= TowerFloorCount; ++floor)
    {
        P401Withdrawal(state, runners, zero, followups);
        const auto route = std::find_if(TowerRoutes().begin(), TowerRoutes().end(), [&](const auto& item) { return item.Floor == floor; });
        command = {};
        command.Kind = TowerSessionAction::OpenRoute;
        command.RouteId = route->Id;
        command.Method = route->Method;
        auto step = P401ApplyExamChoice(state, runners, command, zero, P401ExamMorning());
        P401Require(step.Resolution.Ok(), "non-combat floor opens");
        state = std::move(step.State);
        while (state.Active)
        {
            P401Withdrawal(state, runners, zero, followups);
            command = {};
            command.Kind = TowerSessionAction::Choose;
            command.Runner = state.Runner;
            command.Beat = state.Active->Beat;
            command.ChoiceId = "p401.safe";
            step = P401ApplyExamChoice(state, runners, command, zero, P401ExamMorning());
            P401Require(step.Resolution.Ok(), "zero-practice choice advances");
            state = std::move(step.State);
            P401Require(step.Resolution.OutsideMinutes == (P401ExamIsExit(state.Participation) ? 180 : 0),
                "outside cap is applied once at exit, never once per floor");
        }
    }
    P401Require(state.Participation == TowerParticipation::Completed, "zero-practice runner finishes all floors");
    P401TerminalRules(state, runners, zero, followups);
}
} // namespace
int main(int argc, char** argv)
{
    P401Require(argc == 2, "supply Data/Content root");
    const auto db = P401Read(argv[1]);
    P401FrozenRules();
    P401RealContent(db);
    P401BrokenContent(db);
    P401SessionRules(LoadSchoolFollowupContent(db).Catalogue);
    std::cout << "P401 ExamContract: " << P401Assertions << " assertions passed\n";
}
