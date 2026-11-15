#include "ExamResult.h"

#include "Countdown.h"
#include "ExamContract.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <tuple>
#include <utility>

using namespace PerfectWorld;
namespace
{
std::size_t P403Assertions = 0;
void P403Require(bool value, std::string_view message)
{
    ++P403Assertions;
    if (!value) { std::cerr << "P403 FAIL: " << message << '\n'; std::exit(1); }
}
SchoolFollowupContent P403Read(const std::filesystem::path& root)
{
    ContentDb db;
    std::error_code error;
    std::filesystem::directory_iterator it(root / "school_followup", error), end;
    P403Require(!error, "follow-up directory opens");
    while (it != end)
    {
        if (it->path().extension() == ".json" && it->is_regular_file(error))
        {
            std::ifstream input(it->path(), std::ios::binary);
            P403Require(input.good(), "follow-up file opens");
            const std::string text{std::istreambuf_iterator<char>(input), {}};
            auto parsed = ParseJson(text);
            P403Require(parsed.Value && parsed.Value->IsObject(), "follow-up JSON parses");
            const auto* id = parsed.Value->Find("id");
            const auto* type = parsed.Value->Find("type");
            P403Require(id && id->IsString() && type && type->String == "school_followup", "content identity exists");
            const auto key = id->String;
            P403Require(db.Records.emplace(key, ContentRecord{"school_followup", key,
                it->path().generic_string(), std::move(*parsed.Value), {}}).second, "unique follow-up ID");
        }
        P403Require(!error, "metadata readable");
        it.increment(error);
        P403Require(!error, "directory iteration succeeds");
    }
    const auto loaded = LoadSchoolFollowupContent(db);
    P403Require(loaded.Ok() && loaded.Catalogue.Actions.size() == 19, "real complete follow-up pack loads");
    return loaded.Catalogue;
}
TowerSessionState P403Receipt(std::int32_t quality)
{
    TowerSessionState state;
    state.Participation = TowerParticipation::Completed;
    for (const auto id : {"tower.route.team_transit", "tower.route.changed_rule", "tower.route.mediation"})
    {
        const auto* route = FindTowerRoute(id);
        state.Attempts.push_back({std::string(id), route->Method, quality, quality, 12, false,
            "p403.fixture.scenario", {"p403.fixture.choice"}});
    }
    return state;
}
bool P403Has(const P403ExamResult& result, std::string_view suffix, std::size_t evidence = P403NoEvidence,
    std::string_view target = {})
{
    const auto key = "P403.exam.reason." + std::string(suffix);
    return std::any_of(result.Reasons.begin(), result.Reasons.end(), [&](const auto& reason) {
        return reason.TextKey == key && reason.Evidence == evidence && reason.TargetId == target;
    });
}
auto P403ActionFields(const SchoolFollowupAction& action)
{
    return std::tie(action.Id, action.Outcome, action.OfferId, action.AppointmentId, action.Kind,
        action.TitleTextKey, action.BriefTextKey, action.Destination, action.DayOffset, action.Minute, action.DurationMinutes);
}
void P403Same(const P403ExamResult& a, const P403ExamResult& b)
{
    P403Require(a.Issue.Code == b.Issue.Code && a.Issue.Detail == b.Issue.Detail && a.Participation == b.Participation &&
        a.Score == b.Score && a.Outcome == b.Outcome && a.Attempts == b.Attempts && a.Active == b.Active &&
        a.Reasons == b.Reasons && a.NextSteps.size() == b.NextSteps.size(), "entire derived report repeats identically");
    for (std::size_t i = 0; i < a.NextSteps.size(); ++i)
        P403Require(P403ActionFields(a.NextSteps[i]) == P403ActionFields(b.NextSteps[i]), "all follow-up fields repeat");
}
void P403Explanations(const P403ExamResult& result)
{
    P403Require(result.Ok() && !result.Outcome.empty() && !result.Reasons.empty(), "valid report has explanations");
    std::int64_t points = 0, mission = 0;
    std::size_t followupRows = 0, offerRows = 0;
    for (const auto& reason : result.Reasons)
    {
        P403Require(std::find(P403ExamReasonKeys().begin(), P403ExamReasonKeys().end(), reason.TextKey) !=
            P403ExamReasonKeys().end(), "every explanation uses a registered text key");
        P403Require(!reason.Dimension.empty(), "every reason names its dimension");
        P403Require(reason.Evidence == P403NoEvidence || reason.Evidence < result.Attempts.size() ||
            (result.Active && reason.Evidence == result.Attempts.size()), "reason points at retained provenance");
        if (reason.Dimension == "score_points" && reason.Evidence != P403NoEvidence) points += reason.Value;
        if (reason.Dimension == "mission_minutes") mission += reason.Value;
        followupRows += reason.TextKey == "P403.exam.reason.followup";
        offerRows += reason.TextKey == "P403.exam.reason.offer";
    }
    P403Require(points == result.Score.ScorePoints && mission == result.Score.MissionMinutes,
        "evidence contributions account for every point and elapsed minute");
    P403Require(followupRows == result.NextSteps.size() && offerRows == result.Score.OfferIds.size(), "availability fully explained");
    for (const auto suffix : {"total", "cohort", "decile", "outside_time", "support", "route_delay"})
        P403Require(P403Has(result, suffix), "global result dimension explained");
    P403Require(P403Has(result, "outcome", P403NoEvidence, result.Outcome), "classification explained");
    for (std::size_t i = 0; i < result.Attempts.size(); ++i)
    {
        const auto& attempt = result.Attempts[i];
        P403Require(P403Has(result, "elapsed", i), "every attempt's time explained");
        if (attempt.Defeated)
        {
            P403Require(P403Has(result, "defeat", i) && !P403Has(result, "quality", i) && !P403Has(result, "depth", i),
                "defeat explains lost points without unfinished quality credit");
            continue;
        }
        for (const auto suffix : {"discipline", "common_judgment", "quality", "depth"})
            P403Require(P403Has(result, suffix, i), "completed evidence explains both inputs, rounding and depth");
        for (const auto& reason : result.Reasons) if (reason.Evidence == i)
        {
            if (reason.TextKey == "P403.exam.reason.discipline")
                P403Require(reason.Value == attempt.DisciplineMilli && reason.WeightPercent == TowerDisciplineSharePercent,
                    "discipline input and weight exposed");
            if (reason.TextKey == "P403.exam.reason.common_judgment")
                P403Require(reason.Value == attempt.CommonJudgmentMilli && reason.WeightPercent == 100 - TowerDisciplineSharePercent,
                    "judgment input and weight exposed");
        }
    }
    for (const auto& definition : TowerCredentials())
    {
        const auto found = std::find_if(result.Score.Credentials.begin(), result.Score.Credentials.end(),
            [&](const auto& credential) { return credential.Id == definition.Id; });
        if (found == result.Score.Credentials.end())
            P403Require(P403Has(result, "unobserved", P403NoEvidence, definition.Id), "missing evidence explicitly unobserved");
        else
        {
            const auto source = std::find_if(result.Attempts.begin(), result.Attempts.end(),
                [&](const auto& attempt) { return attempt.RouteId == found->RouteId; });
            const auto index = static_cast<std::size_t>(source - result.Attempts.begin());
            P403Require(source != result.Attempts.end() && P403Has(result, "credential_best", index, definition.Id),
                "credential names its winning evidence");
            const auto row = std::find_if(result.Reasons.begin(), result.Reasons.end(), [&](const auto& reason) {
                return reason.TextKey == "P403.exam.reason.credential_best" && reason.TargetId == definition.Id;
            });
            P403Require(row != result.Reasons.end() && row->Value == found->ScoreMilli, "credential score equals winning quality");
        }
    }
    P403Require(std::any_of(result.NextSteps.begin(), result.NextSteps.end(), [&](const auto& action) {
        return action.Outcome == result.Outcome && !action.AppointmentId.empty();
    }), "outcome has a real authored appointment");
}
void P403Outcomes(const SchoolFollowupContent& content)
{
    std::array<TowerSessionState, 6> fixtures{P403Receipt(1000), P403Receipt(500), P403Receipt(1000), {}, {}, {}};
    fixtures[2].Participation = TowerParticipation::Ejected;
    fixtures[2].Attempts.resize(1);
    fixtures[2].Attempts.back().Defeated = true;
    fixtures[3] = P403Receipt(500);
    fixtures[3].Participation = TowerParticipation::Withdrawn;
    fixtures[3].Attempts.resize(1);
    fixtures[4].Participation = TowerParticipation::Deferred;
    fixtures[5].Participation = TowerParticipation::Declined;
    const std::array<std::string_view, 6> outcomes{"strong", "middle", "poor", "withdrawn", "deferred", "declined"};
    auto reversed = content;
    std::reverse(reversed.Actions.begin(), reversed.Actions.end());
    for (std::size_t i = 0; i < fixtures.size(); ++i)
    {
        const auto before = fixtures[i];
        const auto report = P403ResolveExamResult(fixtures[i], content);
        P403Require(report.Ok() && report.Outcome == outcomes[i], "each outcome class resolves");
        P403Require(fixtures[i] == before, "pure resolution leaves state untouched");
        P403Explanations(report);
        for (int repeat = 0; repeat < 32; ++repeat) P403Same(report, P403ResolveExamResult(fixtures[i], content));
        P403Same(report, P403ResolveExamResult(fixtures[i], reversed));
    }
    const auto zeroQuality = P403ResolveExamResult(P403Receipt(0), content);
    P403Require(zeroQuality.Ok() && zeroQuality.Score.Completed && !zeroQuality.Score.Credentials.empty(),
        "zero-quality completion remains observed evidence");
    P403Explanations(zeroQuality);
}
TowerRunnerCatalogue P403Runners()
{
    TowerRunnerCatalogue catalogue;
    for (const auto& route : TowerRoutes())
    {
        TowerRunnerScenario scenario;
        scenario.Id = "p403.runner." + std::string(route.Id);
        scenario.RouteId = route.Id;
        scenario.TitleTextKey = "p403.fixture.title";
        TowerRunnerBeat beat;
        beat.Id = "p403.beat";
        beat.PromptTextKey = "p403.fixture.prompt";
        beat.Choices.push_back({"p403.safe", "p403.fixture.safe", 12, 1, 1, 1000, 1000});
        beat.Choices.push_back({"p403.modest", "p403.fixture.modest", 12, 0, 0, 300, 400});
        scenario.Beats.push_back(std::move(beat));
        catalogue.Scenarios.push_back(std::move(scenario));
    }
    return catalogue;
}
void P403ZeroPractice(const SchoolFollowupContent& content)
{
    const auto runners = P403Runners();
    P403Require(ValidateTowerRunnerCatalogue(runners).Ok(), "synthetic runner fixture is legal");
    for (const auto choice : {"p403.safe", "p403.modest"})
    {
        CountdownProfile zero;
        zero.Final = true;
        TowerSessionState state;
        P403Require(ApplyTowerSession(state, runners, {}, zero, P401ExamMorning()).Ok(), "zero practice enters");
        for (const auto id : {"tower.route.team_transit", "tower.route.changed_rule", "tower.route.mediation"})
        {
            TowerSessionCommand command;
            command.Kind = TowerSessionAction::OpenRoute;
            command.RouteId = id;
            command.Method = FindTowerRoute(id)->Method;
            P403Require(command.Method != TowerMethod::Combat &&
                ApplyTowerSession(state, runners, command, zero, P401ExamMorning()).Ok(), "full non-combat route opens");
            P403Require(state.Active && state.Active->Conditions.Reserve == 4 &&
                state.Active->Conditions.MinuteDiscount == 0, "no practice prerequisite or hidden bonus");
            command = {};
            command.Kind = TowerSessionAction::Choose;
            command.Runner = state.Runner;
            command.ChoiceId = choice;
            P403Require(ApplyTowerSession(state, runners, command, zero, P401ExamMorning()).Ok(), "unpractised evidence accepted");
        }
        const auto result = P403ResolveExamResult(state, content);
        P403Require(result.Ok() && result.Score.Completed && result.Score.ScorePoints > 0 && result.Score.NationalRank > 0 &&
            !result.Score.Credentials.empty() && !result.Score.OfferIds.empty(), "zero practice has a substantive complete result");
        P403Require(result.Outcome == (std::string_view(choice) == "p403.safe" ? "strong" : "middle"),
            "zero practice preserves meaningful performance differences");
        P403Explanations(result);
    }
}
void P403Details(const SchoolFollowupContent& content)
{
    auto state = P403Receipt(501);
    state.Attempts[1].DisciplineMilli = 777;
    state.Attempts[1].CommonJudgmentMilli = 123;
    const auto result = P403ResolveExamResult(state, content);
    P403Explanations(result);
    P403Require(result.Score.ScorePoints == 4648, "mixed discipline/judgment fixture retains Tower integer rounding");
    P403Require(P403Has(result, "credential_best", 1, "credential.tower.clinician") &&
        P403Has(result, "credential_other", 0, "credential.tower.clinician") &&
        P403Has(result, "credential_other", 2, "credential.tower.clinician"), "best evidence replaces weaker evidence");
    const auto tied = P403ResolveExamResult(P403Receipt(501), content);
    P403Require(P403Has(tied, "credential_best", 0, "credential.tower.clinician"), "ties retain the first source");
    auto stronger = state;
    stronger.Attempts[0].DisciplineMilli += 100;
    const auto discipline = P403ResolveExamResult(stronger, content);
    P403Require(discipline.Score.ScorePoints == result.Score.ScorePoints + 80 && discipline.Reasons != result.Reasons,
        "changed discipline evidence moves quality and its explanation");
    stronger = state;
    stronger.Attempts[0].CommonJudgmentMilli += 100;
    const auto judgment = P403ResolveExamResult(stronger, content);
    P403Require(judgment.Score.ScorePoints == result.Score.ScorePoints + 20 && judgment.Reasons != result.Reasons,
        "changed non-combat judgment moves quality and its explanation");
    for (const auto& route : TowerRoutes())
    {
        auto routed = state;
        routed.Attempts[static_cast<std::size_t>(route.Floor - 1)].RouteId = route.Id;
        routed.Attempts[static_cast<std::size_t>(route.Floor - 1)].Method = route.Method;
        const auto report = P403ResolveExamResult(routed, content);
        P403Explanations(report);
        P403Require(report.Score.ScorePoints == result.Score.ScorePoints, "all routes use the shared scoring envelope");
    }
    auto prepared = state;
    prepared.Preparation.Preparation.fill(1000);
    prepared.Preparation.Composure = 1000;
    P403Same(result, P403ResolveExamResult(prepared, content));
    auto combat = state;
    combat.Attempts[0].RouteId = "tower.route.encounter";
    combat.Attempts[0].Method = TowerMethod::Combat;
    const auto fighting = P403ResolveExamResult(combat, content);
    combat.Attempts[0].Method = TowerMethod::Endurance;
    const auto evading = P403ResolveExamResult(combat, content);
    P403Require(fighting.Ok() && evading.Ok() && fighting.Score == evading.Score && fighting.Reasons == evading.Reasons,
        "combat has no greater ceiling or common-judgment bonus");
    state.Participation = TowerParticipation::Withdrawn;
    state.Attempts.resize(1);
    state.Preparation.RouteDelayMinutes = 180;
    TowerRunnerRun active;
    active.RouteId = "tower.route.changed_rule";
    active.ScenarioId = "p403.unfinished";
    active.Method = TowerMethod::Analysis;
    active.Choices = {"p403.unfinished.choice"};
    active.DisciplineSum = 1000;
    active.JudgmentSum = 1000;
    active.MissionMinutes = 1000;
    state.Active = active;
    const auto withdrawal = P403ResolveExamResult(state, content);
    P403Explanations(withdrawal);
    P403Require(withdrawal.Score.OutsideMinutes == 180 && withdrawal.Score.FloorsCleared == 1 &&
        P403Has(withdrawal, "unfinished", 1) && withdrawal.Active == state.Active,
        "unfinished withdrawal preserves provenance/time without awarding evidence");
    const auto owned = [&] { auto temporary = P403Receipt(1000); return P403ResolveExamResult(temporary, content); }();
    P403Require(owned.Attempts[0].ChoiceIds[0] == "p403.fixture.choice" &&
        owned.Score.Credentials[0].RouteId == "tower.route.team_transit", "report survives source destruction");
}
void P403Refusals(const SchoolFollowupContent& content)
{
    const auto rejected = [&](const TowerSessionState& state, const SchoolFollowupContent& pack, std::string_view code) {
        const auto result = P403ResolveExamResult(state, pack);
        P403Require(!result.Ok() && result.Issue.Code == code && result.Issue.Detail.empty() && result.Outcome.empty() &&
            result.Reasons.empty() && result.NextSteps.empty() && result.Attempts.empty() && !result.Active &&
            result.Score.Credentials.empty() && result.Score.ScorePoints == 0, "refusal has a key and no partial report");
    };
    for (auto p : {TowerParticipation::Unset, TowerParticipation::Entered, static_cast<TowerParticipation>(255)})
    { TowerSessionState state; state.Participation = p; rejected(state, content, "P403.exam.not_terminal"); }
    auto state = P403Receipt(1000);
    rejected(state, {}, "P403.exam.missing_followup");
    auto missing = content;
    std::erase_if(missing.Actions, [](const auto& action) { return action.Outcome == "strong"; });
    rejected(state, missing, "P403.exam.missing_followup");
    state.Attempts[0].DisciplineMilli = 1001;
    rejected(state, content, "tower.invalid_score");
    state = P403Receipt(1000);
    state.Attempts[0].RouteId = "p403.missing";
    rejected(state, content, "tower.unknown_route");
    state = P403Receipt(1000);
    state.Participation = TowerParticipation::Deferred;
    rejected(state, content, "P403.exam.inconsistent_exit");
    state = P403Receipt(1000);
    state.Attempts.pop_back();
    rejected(state, content, "P403.exam.inconsistent_exit");
    state = P403Receipt(1000);
    state.Preparation.RouteDelayMinutes = -1;
    rejected(state, content, "P403.exam.invalid_time");
    state.Preparation.RouteDelayMinutes = TowerWorldCapMinutes + 1;
    rejected(state, content, "P403.exam.invalid_time");
    state = {};
    state.Participation = TowerParticipation::Withdrawn;
    state.Active.emplace();
    state.Active->RouteId = "tower.route.encounter";
    state.Active->Method = TowerMethod::Endurance;
    state.Active->MissionMinutes = std::numeric_limits<std::int64_t>::max();
    rejected(state, content, "P403.exam.invalid_time");
    state.Active->MissionMinutes = 1;
    state.Active->RouteId = "tower.route.changed_rule";
    rejected(state, content, "P403.exam.invalid_active");
}
} // namespace
int main(int argc, char** argv)
{
    P403Require(argc == 2, "supply Data/Content root");
    const auto content = P403Read(argv[1]);
    P403Outcomes(content);
    P403ZeroPractice(content);
    P403Details(content);
    P403Refusals(content);
    std::cout << "P403 ExamResult: " << P403Assertions << " assertions passed\n";
}
