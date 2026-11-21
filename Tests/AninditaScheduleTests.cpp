// Anindita Sen's authored schedule (order AN; LANE_LIFE_PLAN cast table): loaded by the generic npc_schedule loader and checked
// against her day: 07:30 stock check, 08:00-13:00 counter, a midday meal closure at home, 15:00-19:30 counter, reconciliation
// at the packing desk, home, asleep from 22:30. The care worker's schedule is covered by NpcScheduleContentTests.
#include "NpcScheduleContent.h"

#include "ContentSchemas.h"
#include "NpcBehaviour.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

using namespace PerfectWorld;
namespace
{
int AnChecks = 0;
void AnRequire(bool condition, const char* message)
{
    ++AnChecks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

constexpr std::string_view AnScheduleId = "npc_schedule.anindita_sen";
constexpr std::string_view AnPersonId = "person.anindita_sen";
constexpr std::string_view AnHomeZone = "location.nabapara.sen_home";
constexpr std::string_view AnCounterZone = "anchor.stationery.counter";
constexpr std::string_view AnStockZone = "anchor.stationery.stock_room";
constexpr std::string_view AnPackingZone = "anchor.stationery.packing_desk";

GameInstant AnAt(int day, int minute)
{
    return static_cast<GameInstant>(day) * MinutesPerDay + minute - 360;
}

JsonValue& AnField(JsonValue& object, std::string_view name)
{
    for (auto& [key, value] : object.Object)
        if (key == name)
            return value;
    std::exit(2);
}

ContentDb AnDatabase(const JsonValue& root)
{
    ContentDb db;
    const std::string id(AnScheduleId);
    db.Records.emplace(id, ContentRecord{"npc_schedule", id, "anindita_sen.json", root, {}});
    return db;
}

void AnRefuse(const JsonValue& root, const char* message)
{
    const auto db = AnDatabase(root);
    AnRequire(!LoadNpcSchedule(db, AnScheduleId).Ok(), message);
    std::vector<ContentIssue> issues;
    ContentSchema::ValidateNpcScheduleSemantics(db, issues);
    AnRequire(!issues.empty(), "semantic entry point reports the rejected record");
}

void AnMalformed(const JsonValue& root)
{
    auto bad = root;
    AnField(bad, "person").String = "person.anindita_sen_two";
    AnRefuse(bad, "unfrozen identity refused");
    bad = root;
    AnField(bad, "home").String = "location.nabapara.other_home";
    AnRefuse(bad, "wrong home refused");
    bad = root;
    AnField(bad, "workplace").String = "location.nabapara.other";
    AnRefuse(bad, "wrong workplace refused");
    bad = root;
    AnField(bad, "door").String = "door.nabapara.other";
    AnRefuse(bad, "wrong door refused");
    bad = root;
    AnField(bad, "speedCmPerMinute").Integer = 141;
    AnRefuse(bad, "wrong travel speed refused");
    bad = root;
    bad.Object.push_back({"unexpected", JsonValue{}});
    AnRefuse(bad, "unknown field refused");
    bad = root;
    AnField(AnField(bad, "routine").Array.front(), "role").String = "unknown_role";
    AnRefuse(bad, "unknown role refused");
    bad = root;
    AnField(AnField(bad, "routine").Array[2], "start").Integer = 400;
    AnRefuse(bad, "work before the 07:30 duty start refused");
    bad = root;
    AnField(AnField(bad, "routine").Array[10], "end").Integer = 1300;
    AnRefuse(bad, "work past 21:00 refused");
    bad = root;
    AnField(AnField(bad, "routine").Array[2], "start").Integer = 449;
    AnRefuse(bad, "squeezed arrival refused");
    bad = root;
    AnField(AnField(bad, "routine").Array[3], "days").Integer = 0;
    AnRefuse(bad, "empty weekday mask refused");
    bad = root;
    AnField(bad, "routine").Array.erase(AnField(bad, "routine").Array.begin() + 6);
    AnRefuse(bad, "unexplained idle gap refused");
    bad = root;
    AnField(bad, "travel").Array.pop_back();
    AnRefuse(bad, "missing explicit leg refused");
    bad = root;
    AnField(AnField(bad, "travel").Array.front(), "minutes").Integer = 1;
    AnRefuse(bad, "short travel duration refused");
    bad = root;
    AnField(AnField(bad, "travel").Array.front(), "routes").Array.front().String = "route.unknown";
    AnRefuse(bad, "unknown route refused");
    bad = root;
    AnField(AnField(bad, "routes").Array.front(), "lengthCm").Integer = 1;
    AnRefuse(bad, "undersized frozen geometry refused");
    bad = root;
    AnField(bad, "routes").Array.back() = AnField(bad, "routes").Array.front();
    AnRefuse(bad, "duplicate approved route refused");
    bad = root;
    AnField(bad, "routes").Array.pop_back();
    AnRefuse(bad, "missing approved route refused");
    bad = root;
    AnField(AnField(bad, "routine").Array[3], "zone").String = "anchor.stationery.back_room";
    AnRefuse(bad, "unapproved zone refused");
    AnRequire(!LoadNpcSchedule(ContentDb{}, AnScheduleId).Ok(), "missing content refused");
    AnRequire(!LoadNpcSchedule(AnDatabase(root), "npc_schedule.nobody").Ok(), "unknown schedule identity refused");
}

ScheduleResolution AnAtMinute(const NpcSchedule& schedule, int day, int minute)
{
    return ResolveSchedule(schedule.Person, schedule.Travel, AnAt(day, minute));
}

bool AnAtShop(const ScheduleResolution& resolved)
{
    return resolved.Status == ScheduleStatus::Stay && resolved.Zone.starts_with("anchor.stationery.");
}

void AnDay(const NpcSchedule& schedule)
{
    for (int day = 1; day <= 8; ++day)
    {
        for (int minute = 0; minute < 1440; ++minute)
        {
            const auto resolved = AnAtMinute(schedule, day, minute);
            AnRequire(resolved.Valid && !resolved.Squeezed && resolved.Status != ScheduleStatus::Idle,
                      "every minute, including the weekly rollover, has an explicit stay or travel");
            const auto intent = ResolveNpcIntentAt(schedule.Person, schedule.Travel, AnAt(day, minute), false, false);
            AnRequire(intent.Ok(), "every authored role maps to an NPC intent");
            if (AnAtShop(resolved))
                AnRequire(minute >= 450 && minute < 1260, "at the shop only between 07:30 and 21:00");
            if (minute < 375 || minute >= 1350)
            {
                AnRequire(resolved.Status == ScheduleStatus::Stay && resolved.Zone == AnHomeZone && resolved.Doing == "sleeping",
                          "asleep at home from 22:30 until 06:15");
                AnRequire(intent.Intent.Availability == PersonAvailability::Asleep, "asleep means unavailable");
            }
        }
        // 07:30 stock check in the stock room.
        const auto stock = AnAtMinute(schedule, day, 450);
        AnRequire(AnAtShop(stock) && stock.Zone == AnStockZone && stock.Doing == "stocking", "07:30 sharp: the stock check in the stock room");
        // At the counter by 08:00, serving, until the 13:00 closure (apart from the morning restock).
        const auto open = AnAtMinute(schedule, day, 480);
        AnRequire(AnAtShop(open) && open.Zone == AnCounterZone && open.Doing == "serving", "08:00 sharp: at the counter, serving");
        for (const int minute : {500, 620, 700, 779})
            AnRequire(AnAtMinute(schedule, day, minute).Zone == AnCounterZone, "the morning is at the counter");
        const auto restock = AnAtMinute(schedule, day, 640);
        AnRequire(AnAtShop(restock) && restock.Zone == AnStockZone, "a morning restock from the stock room");
        // Meal closure: she walks home at 13:00 and is at home, off the lane, through the middle of the closure.
        const auto leaves = AnAtMinute(schedule, day, 780);
        AnRequire(leaves.Status == ScheduleStatus::Travel && leaves.Zone == AnHomeZone, "from 13:00 she walks home for the meal");
        const auto meal = AnAtMinute(schedule, day, 840);
        AnRequire(meal.Status == ScheduleStatus::Stay && meal.Zone == AnHomeZone, "she eats at home during the closure");
        for (int minute = 780; minute < 900; ++minute)
            AnRequire(!AnAtShop(AnAtMinute(schedule, day, minute)), "never in the shop during the 13:00 to 15:00 closure");
        // Back at the counter at 15:00 sharp, serving until 19:30.
        const auto reopen = AnAtMinute(schedule, day, 900);
        AnRequire(AnAtShop(reopen) && reopen.Zone == AnCounterZone && reopen.Doing == "serving", "15:00 sharp: back at the counter");
        const auto evening = AnAtMinute(schedule, day, 1169);
        AnRequire(AnAtShop(evening) && evening.Zone == AnCounterZone && evening.SegmentEnd == AnAt(day, 1170),
                  "the counter is held until 19:30");
        // Reconciliation at the packing desk, then home, then bed.
        const auto books = AnAtMinute(schedule, day, 1200);
        AnRequire(AnAtShop(books) && books.Zone == AnPackingZone, "after closing she reconciles at the packing desk");
        const auto home = AnAtMinute(schedule, day, 1260);
        AnRequire(home.Status == ScheduleStatus::Stay && home.Zone == AnHomeZone && home.Doing == "at_home", "home by 21:00");
        const auto bed = AnAtMinute(schedule, day, 1350);
        AnRequire(bed.Zone == AnHomeZone && bed.Doing == "sleeping", "sleep from 22:30");
        // Intents: serving is Awake; stocking and the reconciliation are Busy.
        const auto serving = ResolveNpcIntentAt(schedule.Person, schedule.Travel, AnAt(day, 600), false, false);
        AnRequire(serving.Ok() && serving.Intent.State == NpcState::Work && serving.Intent.WorkActionId == "work.serve" &&
                      serving.Intent.Availability == PersonAvailability::Awake,
                  "at the counter she serves and may be spoken to");
        const auto counting = ResolveNpcIntentAt(schedule.Person, schedule.Travel, AnAt(day, 460), false, false);
        AnRequire(counting.Ok() && counting.Intent.WorkActionId == "work.stock" && counting.Intent.Availability == PersonAvailability::Busy,
                  "the stock check is busy work");
    }
}

void AnAdvance(const NpcSchedule& schedule)
{
    auto whole = StartSchedulePerson(AnPersonId, AnAt(1, 0));
    auto split = whole;
    const auto end = AnAt(8, 0);
    const auto full = AdvanceSchedulePerson(schedule.Person, schedule.Travel, whole, end);
    AnRequire(full.Ok, "whole week advance succeeds");
    std::vector<ScheduleEvent> events;
    while (split.At < end)
    {
        const auto part = AdvanceSchedulePerson(schedule.Person, schedule.Travel, split, std::min(end, split.At + 37));
        AnRequire(part.Ok, "split advance succeeds");
        events.insert(events.end(), part.Events.begin(), part.Events.end());
    }
    AnRequire(whole == split && full.Events == events, "split and whole advances produce identical state and events");
}

void AnBuiltRoutes(const NpcSchedule& schedule)
{
    const std::vector<int> lengths{635, 727, 1100, 2000};
    AnRequire(schedule.Routes.size() == lengths.size(), "built shop has four routes including off-lane home");
    for (std::size_t i = 0; i < lengths.size(); ++i)
        AnRequire(schedule.Routes[i].LengthCm == lengths[i], "authored geometry matches the built polyline");
    const std::vector<int> minutes{8, 6, 5, 23, 14, 13, 15, 10, 28, 27};
    AnRequire(schedule.Paths.size() == minutes.size(), "all ten undirected legs are explicit");
    for (std::size_t i = 0; i < schedule.Paths.size(); ++i)
    {
        int length = 0;
        for (const auto& id : schedule.Paths[i].Routes)
        {
            const auto route = std::find_if(schedule.Routes.begin(), schedule.Routes.end(),
                                          [&](const auto& item) { return item.Id == id; });
            AnRequire(route != schedule.Routes.end(), "path uses authored geometry");
            length += route->LengthCm;
        }
        AnRequire(schedule.Paths[i].Minutes == minutes[i] && minutes[i] == (length + 139) / 140,
                  "every leg is the integer ceiling of its total length at 140 cm per minute");
    }
    for (const auto [minute, zone] : {std::pair{423, AnStockZone}, {780, AnHomeZone},
                                     {877, AnCounterZone}, {1170, AnPackingZone}, {1232, AnHomeZone}})
    {
        const auto walking = AnAtMinute(schedule, 1, minute);
        AnRequire(walking.Status == ScheduleStatus::Travel && walking.Zone == zone, "revised departure boundary begins travel");
    }
    AnRequire(AnAtMinute(schedule, 1, 803).Zone == AnHomeZone &&
                  AnAtMinute(schedule, 1, 803).Status == ScheduleStatus::Stay, "23 minute lunch journey ends at 13:23");
}
} // namespace

int main(int argc, char** argv)
{
    AnRequire(argc == 2, "content file argument supplied");
    std::ifstream input(argv[1]);
    AnRequire(input.good(), "Anindita authored content exists");
    const std::string text{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const auto parsed = ParseJson(text);
    AnRequire(parsed.Value.has_value(), "Anindita JSON parses");
    const auto db = AnDatabase(*parsed.Value);
    const auto loaded = LoadNpcSchedule(db, AnScheduleId);
    if (!loaded.Ok())
        std::cerr << loaded.Error << '\n';
    AnRequire(loaded.Ok(), "typed Anindita content loads and validates");
    const auto again = LoadNpcSchedule(db, AnScheduleId);
    AnRequire(again.Ok() && again.Value->Person == loaded.Value->Person && again.Value->Travel == loaded.Value->Travel,
              "typed loading is deterministic");
    AnRequire(loaded.Value->Person.PersonId == AnPersonId && loaded.Value->Person.HomeZone == AnHomeZone &&
                  ValidateNpcScheduleContent(*loaded.Value).empty(),
              "the decoded schedule revalidates");
    std::vector<ContentIssue> issues;
    ContentSchema::ValidateNpcScheduleSemantics(db, issues);
    AnRequire(issues.empty(), "exported semantic validator accepts the content");
    const auto registered = LoadContent({{"Data/Content/npc_schedule/npc_schedule.anindita_sen.json", text}});
    for (const auto& issue : registered.Issues)
        std::cerr << issue.File << ' ' << issue.Id << ": " << issue.Message << '\n';
    AnRequire(registered.Ok(), "Anindita loads through the registered schema");
    AnRequire(LoadNpcSchedule(registered.Db, AnScheduleId).Ok(), "registered content supplies the typed schedule");
    AnRequire(!LoadNpcSchedule(registered.Db, CareWorkerScheduleId).Ok(), "the care worker is not supplied by Anindita's file");
    AnRequire(!LoadNpcSchedule(registered.Db, FaridaScheduleId).Ok(), "Farida is not supplied by Anindita's file");
    AnMalformed(*parsed.Value);
    AnDay(*loaded.Value);
    AnAdvance(*loaded.Value);
    AnBuiltRoutes(*loaded.Value);
    std::cout << "PASS: " << AnChecks << " assertions\n";
}
