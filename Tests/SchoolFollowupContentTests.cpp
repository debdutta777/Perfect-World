#include "SchoolFollowupContent.h"

#include "Countdown.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>

using namespace PerfectWorld;
namespace
{
int A8Assertions = 0;
void A8Expect(bool value, std::string_view message)
{
    ++A8Assertions;
    if (!value) { std::cerr << "A8 FAIL: " << message << '\n'; std::exit(1); }
}
ContentDb A8Read(const std::filesystem::path& root)
{
    ContentDb db;
    std::error_code error;
    std::filesystem::directory_iterator it(root / "school_followup", error), end;
    A8Expect(!error, "follow-up content directory opens");
    while (it != end)
    {
        if (it->path().extension() == ".json")
        {
            std::ifstream input(it->path(), std::ios::binary);
            A8Expect(input.good(), "authored record opens");
            const std::string text{std::istreambuf_iterator<char>(input), {}};
            auto parsed = ParseJson(text);
            A8Expect(parsed.Value.has_value(), "authored JSON parses");
            const auto* id = parsed.Value->Find("id");
            A8Expect(id && id->IsString(), "record has identity");
            const std::string key = id->String;
            A8Expect(db.Records.emplace(key, ContentRecord{"school_followup", key,
                it->path().generic_string(), std::move(*parsed.Value), {}}).second, "record ID is unique");
        }
        it.increment(error);
        A8Expect(!error, "directory iteration succeeds");
    }
    return db;
}
JsonValue& A8Field(ContentDb& db, std::string_view id, std::string_view field)
{
    auto& record = db.Records.find(id)->second;
    for (auto& [key, value] : record.Data.Object) if (key == field) return value;
    std::exit(2);
}
TowerRunnerCatalogue A8Runners()
{
    // Same frozen runner fixture pattern as TowerSessionTests.cpp: three authored
    // beats, careful/rush choices. The UI never sees the evidence numbers.
    TowerRunnerCatalogue catalogue;
    for (const auto& route : TowerRoutes())
    {
        TowerRunnerScenario scenario;
        scenario.Id = "scenario." + std::string(route.Id);
        scenario.RouteId = route.Id;
        scenario.TitleTextKey = "phase2.a8.test.title";
        for (int index = 0; index < 3; ++index)
        {
            TowerRunnerBeat beat;
            beat.Id = "beat." + std::to_string(index);
            beat.PromptTextKey = "phase2.a8.test.prompt";
            beat.Choices = {{"careful", "phase2.a8.test.careful", 5, 1, 1, 1000, 1000},
                {"middle", "phase2.a8.test.middle", 5, 1, 1, 300, 300},
                {"rush", "phase2.a8.test.rush", 2, 4, 0, 0, 0}};
            scenario.Beats.push_back(std::move(beat));
        }
        catalogue.Scenarios.push_back(std::move(scenario));
    }
    return catalogue;
}
TowerSessionView A8Play(const TowerRunnerCatalogue& catalogue, const char* choice)
{
    TowerSessionState state;
    CountdownProfile preparation; preparation.Final = true;
    TowerSessionCommand command;
    A8Expect(ApplyTowerSession(state, catalogue, command, preparation, CountdownEndInstant()).Ok(), "enter tower through frozen API");
    for (int floor = 1; floor <= 3 && state.Participation == TowerParticipation::Entered; ++floor)
    {
        const auto route = std::find_if(TowerRoutes().begin(), TowerRoutes().end(), [&](const auto& value) { return value.Floor == floor; });
        command = {}; command.Kind = TowerSessionAction::OpenRoute; command.RouteId = route->Id; command.Method = route->Method;
        A8Expect(ApplyTowerSession(state, catalogue, command, preparation, CountdownEndInstant()).Ok(), "open route");
        while (state.Active && state.Participation == TowerParticipation::Entered)
        {
            const auto view = QueryTowerSession(state, catalogue);
            command = {}; command.Kind = TowerSessionAction::Choose;
            command.Runner = view.Runner; command.Beat = view.Beat; command.ChoiceId = choice;
            A8Expect(ApplyTowerSession(state, catalogue, command, preparation, CountdownEndInstant()).Ok(), "core scores choice");
        }
    }
    return QueryTowerSession(state, catalogue);
}
void A8Continuation(const SchoolFollowupContent& content, const TowerSessionView& view, const char* outcome)
{
    A8Expect(SchoolFollowupOutcome(view) == outcome, "terminal outcome uses authoritative view");
    const auto actions = QuerySchoolFollowups(content, view);
    std::set<std::string> kinds, mappedOffers;
    int specific = 0;
    for (const auto& action : actions)
    {
        kinds.insert(action.Kind);
        specific += action.Outcome == outcome ? 1 : 0;
        A8Expect(action.DayOffset >= 1 && action.DayOffset <= 7 && !action.AppointmentId.empty(), "next week has a concrete appointment identity");
        if (!action.OfferId.empty())
        {
            A8Expect(std::find(view.Result.OfferIds.begin(), view.Result.OfferIds.end(), action.OfferId) != view.Result.OfferIds.end(), "no invented eligible offer");
            mappedOffers.insert(action.OfferId);
        }
    }
    A8Expect(specific == 1, "one outcome-specific conversation");
    A8Expect(kinds.contains("school") && kinds.contains("study") && kinds.contains("rest"), "ordinary school life continues");
    A8Expect(mappedOffers.size() == view.Result.OfferIds.size() && mappedOffers.size() >= 3, "all eligible offers are actionable, including bottom decile");
}
}

int main(int argc, char** argv)
{
    A8Expect(argc == 2, "content root argument supplied");
    const auto db = A8Read(argv[1]);
    const auto loaded = LoadSchoolFollowupContent(db);
    for (const auto& issue : loaded.Issues) std::cerr << issue.Id << ": " << issue.Message << '\n';
    A8Expect(loaded.Ok() && loaded.Catalogue.Actions.size() == 19, "all authored follow-ups load without shared registration");
    std::ifstream wordsInput(std::filesystem::path(argv[1]).parent_path() / "Text/en/phase2_a8.json", std::ios::binary);
    A8Expect(wordsInput.good(), "English text fragment opens");
    const std::string wordsText{std::istreambuf_iterator<char>(wordsInput), {}};
    const auto words = ParseJson(wordsText);
    A8Expect(words.Value && words.Value->IsObject(), "English fragment is a text-key map");
    for (const auto& action : loaded.Catalogue.Actions)
        for (const auto& key : {action.TitleTextKey, action.BriefTextKey})
        {
            const auto* value = words.Value->Find(key);
            A8Expect(value && value->IsString() && !value->String.empty(), "every authored action has English text");
        }
    const auto runners = A8Runners();
    A8Expect(ValidateTowerRunnerCatalogue(runners).Ok(), "runner fixture conforms to frozen contracts");
    A8Continuation(loaded.Catalogue, A8Play(runners, "careful"), "strong");
    A8Continuation(loaded.Catalogue, A8Play(runners, "middle"), "middle");
    const auto poor = A8Play(runners, "rush");
    A8Expect(poor.Result.Decile == 10, "real runner result reaches bottom decile");
    A8Continuation(loaded.Catalogue, poor, "poor");
    for (const auto kind : {TowerSessionAction::Defer, TowerSessionAction::Decline, TowerSessionAction::Withdraw})
    {
        TowerSessionState state;
        CountdownProfile preparation; preparation.Final = true;
        TowerSessionCommand command;
        if (kind == TowerSessionAction::Withdraw)
            A8Expect(ApplyTowerSession(state, runners, command, preparation, CountdownEndInstant()).Ok(), "enter before withdrawal");
        command.Kind = kind;
        A8Expect(ApplyTowerSession(state, runners, command, preparation, CountdownEndInstant()).Ok(), "core accepts optional participation decision");
        A8Continuation(loaded.Catalogue, QueryTowerSession(state, runners), kind == TowerSessionAction::Defer ? "deferred" :
            kind == TowerSessionAction::Decline ? "declined" : "withdrawn");
    }
    for (const auto participation : {TowerParticipation::Unset, TowerParticipation::Entered})
    {
        TowerSessionView view; view.Participation = participation;
        A8Expect(QuerySchoolFollowups(loaded.Catalogue, view).empty(), "no final offers before terminal decision");
    }
    auto changed = poor; changed.Result.OfferIds.clear();
    const auto ordinary = QuerySchoolFollowups(loaded.Catalogue, changed);
    A8Expect(ordinary.size() == 4, "lost eligibility keeps ordinary life and conversation only");
    changed = poor; changed.Result.Error = TowerError::InvalidScore;
    A8Expect(QuerySchoolFollowups(loaded.Catalogue, changed).empty(), "invalid result exposes no final actions");
    for (const int decile : {0, 11})
    {
        changed = poor; changed.Result.Decile = decile;
        A8Expect(QuerySchoolFollowups(loaded.Catalogue, changed).empty(), "invalid attempted decile exposes no final actions");
    }
    auto bad = db;
    A8Field(bad, "school_followup.p2.apprenticeship", "offerId").String = "offer.tower.invented";
    A8Expect(!LoadSchoolFollowupContent(bad).Ok(), "unknown offer refused");
    bad = db; A8Field(bad, "school_followup.p2.school", "durationMinutes").Integer = 121;
    A8Expect(!LoadSchoolFollowupContent(bad).Ok(), "out-of-bounds duration refused");
    bad = db; bad.Records.erase("school_followup.p2.study_bridge");
    A8Expect(!LoadSchoolFollowupContent(bad).Ok(), "missing bottom-decile offer refused");
    bad = db; A8Field(bad, "school_followup.p2.school", "appointmentId").String = "appointment.p2.rest";
    A8Expect(!LoadSchoolFollowupContent(bad).Ok(), "duplicate appointment refused");
    bad = db; bad.Records.begin()->second.Data.Object.push_back({"unexpected", {}});
    A8Expect(!LoadSchoolFollowupContent(bad).Ok(), "unknown field refused at public DB boundary");
    bad = db; bad.Records.begin()->second.ParentId = "nested";
    A8Expect(!LoadSchoolFollowupContent(bad).Ok(), "forged metadata refused");
    bad = db; bad.Records.erase("school_followup.p2.school");
    A8Expect(!LoadSchoolFollowupContent(bad).Ok(), "missing school continuation refused");
    bad = db; A8Field(bad, "school_followup.p2.deferred", "outcome").String = "unknown";
    A8Expect(!LoadSchoolFollowupContent(bad).Ok(), "unknown participation label refused");
    bad = db; A8Field(bad, "school_followup.p2.rest", "minute").Integer = 1439;
    A8Expect(!LoadSchoolFollowupContent(bad).Ok(), "appointments cannot cross midnight");
    std::cout << "A8 SchoolFollowupContent: " << A8Assertions << " assertions passed\n";
}
