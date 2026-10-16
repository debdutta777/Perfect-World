#include "FaridaSchedule.h"

#include "ContentSchemas.h"
#include "NpcBehaviour.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>

using namespace PerfectWorld;
namespace
{
int Assertions = 0;
void Require(bool condition, const char* message)
{
    ++Assertions;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
GameInstant At(int day, int minute)
{
    return static_cast<GameInstant>(day) * MinutesPerDay + minute - 360;
}
JsonValue& Field(JsonValue& object, std::string_view name)
{
    for (auto& [key, value] : object.Object)
        if (key == name)
            return value;
    std::exit(2);
}
ContentDb Database(const JsonValue& root)
{
    ContentDb db;
    const std::string id(FaridaScheduleId);
    db.Records.emplace(id, ContentRecord{"npc_schedule", id, "farida.json", root, {}});
    return db;
}
void Refuse(const JsonValue& root, const char* message)
{
    const auto db = Database(root);
    Require(!LoadFaridaSchedule(db).Ok(), message);
    std::vector<ContentIssue> issues;
    ContentSchema::ValidateNpcScheduleSemantics(db, issues);
    Require(!issues.empty(), "semantic entry point reports rejected record");
}

void Malformed(const JsonValue& root)
{
    auto bad = root;
    Field(bad, "routine").Array.front().Object.pop_back();
    Refuse(bad, "missing routine role refused");
    bad = root;
    Field(Field(bad, "routine").Array.front(), "start").Integer = std::nullopt;
    Refuse(bad, "noninteger time refused");
    bad = root;
    Field(Field(bad, "routine").Array.front(), "days").Integer = 0;
    Refuse(bad, "empty weekday mask refused");
    bad = root;
    Field(Field(bad, "routine").Array.front(), "role").String = "unknown_role";
    Refuse(bad, "unknown role refused");
    bad = root;
    Field(bad, "person").String = "person.farida";
    Refuse(bad, "unfrozen identity refused");
    bad = root;
    bad.Object.push_back({"unexpected", JsonValue{}});
    Refuse(bad, "unknown field refused");
    bad = root;
    Field(bad, "speedCmPerMinute").Integer = 141;
    Refuse(bad, "wrong travel speed refused");
    bad = root;
    Field(bad, "door").String = "door.nabapara.other";
    Refuse(bad, "wrong workplace door refused");
    bad = root;
    Field(bad, "workplace").String = "location.nabapara.other";
    Refuse(bad, "wrong workplace refused");
    bad = root;
    Field(bad, "travel").Array.pop_back();
    Refuse(bad, "missing explicit leg refused");
    bad = root;
    Field(Field(bad, "travel").Array.front(), "minutes").Integer = 1;
    Refuse(bad, "short travel duration refused");
    bad = root;
    Field(Field(bad, "travel").Array.front(), "routes").Array.front().String = "route.unknown";
    Refuse(bad, "unknown route refused");
    bad = root;
    Field(Field(bad, "travel").Array.front(), "routes").Array.clear();
    Refuse(bad, "empty explicit route path refused");
    bad = root;
    Field(bad, "routes").Array.back() = Field(bad, "routes").Array.front();
    Refuse(bad, "duplicate approved route refused");
    bad = root;
    auto& loop = Field(Field(bad, "travel").Array.front(), "routes").Array;
    loop.push_back(loop.front());
    loop.push_back(loop.front());
    Refuse(bad, "route loop refused even when it reaches destination");
    bad = root;
    auto& paths = Field(bad, "travel").Array;
    auto multi = std::find_if(paths.begin(), paths.end(), [](auto& item) { return Field(item, "routes").Array.size() > 1; });
    Require(multi != paths.end(), "content contains a composed route");
    auto& ids = Field(*multi, "routes").Array;
    std::reverse(ids.begin(), ids.end());
    Refuse(bad, "disconnected route sequence refused");
    bad = root;
    Field(Field(bad, "routes").Array.front(), "lengthCm").Integer = 1;
    Refuse(bad, "undersized frozen geometry refused");
    bad = root;
    Field(Field(bad, "routes").Array.front(), "to").String = "anchor.shanti_chai.stove";
    Refuse(bad, "wrong route endpoint refused");
    bad = root;
    Field(bad, "routine").Array.erase(Field(bad, "routine").Array.begin() + 7);
    Refuse(bad, "unexplained midday idle refused");
    bad = root;
    for (auto& entry : Field(bad, "routine").Array)
        Field(entry, "days").Integer = 126;
    Refuse(bad, "missing Sunday coverage refused");
    bad = root;
    Field(Field(bad, "routine").Array[4], "start").Integer = 359;
    Refuse(bad, "squeezed routine refused");
    Require(!ParseJson("{\"id\":1,\"id\":2}").Value, "duplicate JSON keys refused");
    Require(!ParseJson("{\"routine\":[").Value, "truncated JSON refused");
    Require(!LoadFaridaSchedule(ContentDb{}).Ok(), "missing content refused");
}

void Week(const FaridaSchedule& schedule)
{
    for (int day = 1; day <= 8; ++day)
    {
        for (int minute = 0; minute < 1440; ++minute)
        {
            const auto resolved = ResolveSchedule(schedule.Person, schedule.Travel, At(day, minute));
            Require(resolved.Valid && !resolved.Squeezed && resolved.Status != ScheduleStatus::Idle,
                    "all minutes including weekly rollover have explicit stay or travel");
            const bool serving = (minute >= 360 && minute < 690) || (minute >= 930 && minute < 1140);
            Require((resolved.Status == ScheduleStatus::Stay && resolved.Doing == "serving") == serving,
                    "both serving windows match frozen business hours exactly");
        }
        for (const auto minute : {315, 330, 360, 600, 780, 930, 1100, 1150, 1205, 1330})
        {
            const auto result = ResolveNpcIntentAt(schedule.Person, schedule.Travel, At(day, minute), false, false);
            Require(result.Ok(), "authored role maps to NPC intent");
            const bool awake = minute == 780 || (minute >= 360 && minute < 690) || (minute >= 930 && minute < 1140);
            const auto expected = awake ? PersonAvailability::Awake :
                                  minute == 1330 ? PersonAvailability::Asleep : PersonAvailability::Busy;
            Require(result.Intent.Availability == expected, "preparation/rest/cleanup/closing/sleep availability");
        }
    }
    const auto opening = ResolveSchedule(schedule.Person, schedule.Travel, 0);
    Require(opening.Valid && opening.Status == ScheduleStatus::Stay && opening.Doing == "serving",
            "campaign starts in morning service without a special clock offset");
}

void Advance(const FaridaSchedule& schedule)
{
    auto whole = StartSchedulePerson(FaridaPersonId, At(1, 0));
    auto split = whole;
    const auto end = At(8, 0);
    const auto full = AdvanceSchedulePerson(schedule.Person, schedule.Travel, whole, end);
    Require(full.Ok, "whole week advance succeeds");
    std::vector<ScheduleEvent> events;
    while (split.At < end)
    {
        const auto part = AdvanceSchedulePerson(schedule.Person, schedule.Travel, split, std::min(end, split.At + 37));
        Require(part.Ok, "split advance succeeds");
        events.insert(events.end(), part.Events.begin(), part.Events.end());
    }
    Require(whole == split && full.Events == events, "split and whole advances produce identical state and events");
    Require(HashSchedulePersonState(whole) == HashSchedulePersonState(split), "split and whole state hashes agree");
}
} // namespace

int main(int argc, char** argv)
{
    Require(argc == 2, "content file argument supplied");
    std::ifstream input(argv[1]);
    Require(input.good(), "Farida authored content exists");
    const std::string text{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const auto parsed = ParseJson(text);
    Require(parsed.Value.has_value(), "Farida JSON parses");
    const auto db = Database(*parsed.Value);
    const auto loaded = LoadFaridaSchedule(db);
    if (!loaded.Ok())
        std::cerr << loaded.Error << '\n';
    Require(loaded.Ok(), "typed Farida content loads and validates");
    const auto again = LoadFaridaSchedule(db);
    Require(again.Ok() && again.Value->Person == loaded.Value->Person && again.Value->Travel == loaded.Value->Travel,
            "typed loading deterministic");
    Require(ContentSchema::NpcScheduleSchema().Type == "npc_schedule", "NPC schema exported separately from school bells");
    std::vector<ContentIssue> issues;
    ContentSchema::ValidateNpcScheduleSemantics(db, issues);
    Require(issues.empty(), "exported semantic validator accepts Farida content");
    const auto known = KnownContentTypes();
    if (std::find(known.begin(), known.end(), "npc_schedule") != known.end())
    {
        // The registry requires the content path layout <type>/<id>.json.
        const auto registered = LoadContent({{"Data/Content/npc_schedule/npc_schedule.farida_qadri.json", text}});
        for (const auto& issue : registered.Issues)
            std::cerr << issue.File << ' ' << issue.Id << ": " << issue.Message << '\n';
        Require(registered.Ok(), "Farida loads through registered schema");
        Require(LoadFaridaSchedule(registered.Db).Ok(), "registered content supplies typed schedule");
    }
    else
        std::cout << "NOT RUN: shared schema registration (CAT integration pending)\n";
    Malformed(*parsed.Value);
    Week(*loaded.Value);
    Advance(*loaded.Value);
    std::cout << "PASS: " << Assertions << " assertions\n";
}
