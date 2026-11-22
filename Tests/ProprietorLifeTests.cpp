// R4-02: the two working proprietors' authored schedules (Debashis Pal, repair workshop; Mahua Saha, hardware store). Each loads
// through the generic npc_schedule loader and is checked for every minute of eight days (a full week and the Monday after): the
// shop is staffed exactly in the door's business hours, the staff counter is held at the opening and closing minutes, the work
// anchors are used in their own roles, meals, closure, rest and sleep happen where and when authored, an interruption (a chat, a
// blocked route) suspends and resumes the work intent, and every route and leg is the integer ceiling of its geometry. Their
// conversations and world save are covered by ProprietorConversationTests.cpp.
#include "NpcScheduleContent.h"

#include "ContentSchemas.h"
#include "Doors.h"
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
int PlChecks = 0;
void PlCheck(bool condition, const char* message)
{
    ++PlChecks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

struct PlSample
{
    int Minute;
    NpcState State;
};

struct PlCase
{
    std::string_view Name, ScheduleId, PersonId, Home, DoorId, Prefix, Counter, Workplace;
    int OpenA0, OpenA1, OpenB0, OpenB1;
    int DutyStart, DutyEnd;
    int WakeWeek, WakeSunday;
    int MiddayMinute;
    std::string_view MiddayZone, MiddayDoing;
    int CookWeek, CookSunday, EveningCookMinute, ReadMinute;
    std::vector<int> RouteCm;
    std::size_t Legs;
    int HomeLeaves; // departures from home per day
    std::vector<PlSample> Samples;
    std::vector<std::string_view> EveryDay;   // anchor suffixes visited every day
    std::vector<std::string_view> WeekdaysOnly; // visited Monday to Saturday, never on Sunday
};

/** Day 0 is a Monday: instant 0 is Monday 06:00, and the first week is skipped so that no instant is negative. */
GameInstant PlAt(int day, int minute) { return static_cast<GameInstant>(day + 7) * MinutesPerDay + minute - 360; }
bool PlSunday(int day) { return day % 7 == 6; }

JsonValue& PlField(JsonValue& object, std::string_view name)
{
    for (auto& [key, value] : object.Object)
        if (key == name)
            return value;
    std::exit(2);
}
/** The first routine entry with this zone, weekday mask and start minute. */
JsonValue& PlEntryOf(JsonValue& root, std::string_view zone, int days, int start)
{
    for (auto& item : PlField(root, "routine").Array)
        if (PlField(item, "zone").String == zone && PlField(item, "days").Integer == days && PlField(item, "start").Integer == start)
            return item;
    std::exit(3);
}
std::size_t PlIndexOf(JsonValue& root, std::string_view zone, int days, int start)
{
    return static_cast<std::size_t>(&PlEntryOf(root, zone, days, start) - PlField(root, "routine").Array.data());
}

ContentDb PlDatabase(const PlCase& c, const JsonValue& root)
{
    ContentDb db;
    const std::string id(c.ScheduleId);
    db.Records.emplace(id, ContentRecord{"npc_schedule", id, "proprietor.json", root, {}});
    return db;
}
void PlRefuse(const PlCase& c, const JsonValue& root, const char* message)
{
    const auto db = PlDatabase(c, root);
    PlCheck(!LoadNpcSchedule(db, c.ScheduleId).Ok(), message);
    std::vector<ContentIssue> issues;
    ContentSchema::ValidateNpcScheduleSemantics(db, issues);
    PlCheck(!issues.empty(), "semantic entry point reports the rejected record");
}

ScheduleResolution PlResolve(const NpcSchedule& s, int day, int minute) { return ResolveSchedule(s.Person, s.Travel, PlAt(day, minute)); }
bool PlStarts(const std::string& zone, std::string_view prefix) { return zone.starts_with(prefix); }
/** Stationed in the shop: a stay at one of its anchors, or a walk between two of its anchors. */
bool PlInShop(const PlCase& c, const ScheduleResolution& r)
{
    return PlStarts(r.Zone, c.Prefix) && (r.Status == ScheduleStatus::Stay || PlStarts(r.FromZone, c.Prefix));
}
bool PlOpen(const PlCase& c, int minute)
{
    return (minute >= c.OpenA0 && minute < c.OpenA1) || (minute >= c.OpenB0 && minute < c.OpenB1);
}
std::string PlZone(const PlCase& c, std::string_view suffix) { return std::string(c.Prefix) + std::string(suffix); }

void PlMalformed(const PlCase& c, const JsonValue& root, std::string_view firstShopZone, int firstShopStart, std::string_view lastShopZone,
                 int lastShopStart)
{
    auto bad = root;
    PlField(bad, "person").String = std::string(c.PersonId) + "_two";
    PlRefuse(c, bad, "unfrozen identity refused");
    bad = root;
    PlField(bad, "home").String = "location.nabapara.other_home";
    PlRefuse(c, bad, "wrong home refused");
    bad = root;
    PlField(bad, "workplace").String = "location.nabapara.other";
    PlRefuse(c, bad, "wrong workplace refused");
    bad = root;
    PlField(bad, "door").String = "door.nabapara.other";
    PlRefuse(c, bad, "wrong door refused");
    bad = root;
    PlField(bad, "speedCmPerMinute").Integer = 141;
    PlRefuse(c, bad, "wrong travel speed refused");
    bad = root;
    bad.Object.push_back({"unexpected", JsonValue{}});
    PlRefuse(c, bad, "unknown field refused");
    bad = root;
    PlField(PlField(bad, "routine").Array.front(), "role").String = "unknown_role";
    PlRefuse(c, bad, "unknown role refused");
    bad = root;
    PlField(PlEntryOf(bad, firstShopZone, 126, firstShopStart), "start").Integer = c.DutyStart - 1;
    PlRefuse(c, bad, "work before the duty start refused");
    bad = root;
    PlField(PlEntryOf(bad, lastShopZone, 126, lastShopStart), "end").Integer = c.DutyEnd + 1;
    PlRefuse(c, bad, "work past the duty end refused");
    bad = root;
    PlField(PlEntryOf(bad, firstShopZone, 126, firstShopStart), "start").Integer = firstShopStart - 1;
    PlRefuse(c, bad, "squeezed arrival refused");
    bad = root;
    PlField(PlField(bad, "routine").Array[3], "days").Integer = 0;
    PlRefuse(c, bad, "empty weekday mask refused");
    bad = root;
    PlField(bad, "routine").Array.erase(PlField(bad, "routine").Array.begin() + static_cast<std::ptrdiff_t>(PlIndexOf(bad, firstShopZone, 126, firstShopStart)));
    PlRefuse(c, bad, "unexplained idle gap refused");
    bad = root;
    PlField(bad, "travel").Array.pop_back();
    PlRefuse(c, bad, "missing explicit leg refused");
    bad = root;
    std::swap(PlField(bad, "travel").Array[0], PlField(bad, "travel").Array[1]);
    PlRefuse(c, bad, "travel legs out of ascending order refused");
    bad = root;
    PlField(PlField(bad, "travel").Array.front(), "minutes").Integer = 1;
    PlRefuse(c, bad, "short travel duration refused");
    bad = root;
    PlField(PlField(bad, "travel").Array.front(), "routes").Array.front().String = "route.unknown";
    PlRefuse(c, bad, "unknown route refused");
    bad = root;
    PlField(PlField(bad, "routes").Array.front(), "lengthCm").Integer = 1;
    PlRefuse(c, bad, "undersized frozen geometry refused");
    bad = root;
    PlField(bad, "routes").Array.back() = PlField(bad, "routes").Array.front();
    PlRefuse(c, bad, "duplicate approved route refused");
    bad = root;
    PlField(bad, "routes").Array.pop_back();
    PlRefuse(c, bad, "missing approved route refused");
    bad = root;
    PlField(PlField(bad, "routine").Array[3], "zone").String = PlZone(c, "back_room");
    PlRefuse(c, bad, "unapproved zone refused");
    PlCheck(!LoadNpcSchedule(ContentDb{}, c.ScheduleId).Ok(), "missing content refused");
    PlCheck(!LoadNpcSchedule(PlDatabase(c, root), "npc_schedule.nobody").Ok(), "unknown schedule identity refused");
}

void PlDay(const PlCase& c, const NpcSchedule& schedule)
{
    for (int day = 0; day < 8; ++day)
    {
        const bool sunday = PlSunday(day);
        int counterOpen = 0;
        int serving = 0;
        std::vector<std::string> visited;
        for (int minute = 0; minute < 1440; ++minute)
        {
            const auto r = PlResolve(schedule, day, minute);
            PlCheck(r.Valid && !r.Squeezed && r.Status != ScheduleStatus::Idle, "every minute has an explicit stay or travel");
            const auto intent = ResolveNpcIntentAt(schedule.Person, schedule.Travel, PlAt(day, minute), false, false);
            PlCheck(intent.Ok(), "every authored role maps to an NPC intent");
            const bool atShop = r.Status == ScheduleStatus::Stay && PlStarts(r.Zone, c.Prefix);
            if (atShop)
            {
                PlCheck(minute >= c.DutyStart && minute < c.DutyEnd, "at the shop only inside the duty hours");
                visited.push_back(r.Zone);
                const bool counter = r.Zone == c.Counter;
                if (r.Doing == "serving")
                {
                    PlCheck(counter, "serving happens only at the staff counter");
                    PlCheck(PlOpen(c, minute), "nobody serves a shut shop");
                    PlCheck(intent.Intent.State == NpcState::Work && intent.Intent.WorkActionId == "work.serve" &&
                                intent.Intent.Availability == PersonAvailability::Awake,
                            "at the counter the proprietor serves and may be spoken to");
                    ++serving;
                }
                else if (r.Doing == "repairing" || r.Doing == "inspecting" || r.Doing == "stocking")
                {
                    PlCheck(intent.Intent.State == NpcState::Work && intent.Intent.Availability == PersonAvailability::Busy,
                            "bench, rack, table and weighing work is busy and declines a chat");
                    PlCheck(intent.Intent.WorkActionId == "work." + std::string(r.Doing == "repairing" ? "repair" : r.Doing == "inspecting" ? "inspect" : "stock"),
                            "each role performs its own action");
                }
                else
                {
                    PlCheck(r.Doing == "break" && intent.Intent.State == NpcState::Idle, "the only unworked shop stay is the meal break");
                }
            }
            if (PlOpen(c, minute))
            {
                PlCheck(PlInShop(c, r), "in the door's business hours the proprietor is in the shop");
                ++counterOpen;
            }
            else
            {
                PlCheck(!atShop || r.Doing != "serving", "never serving outside the hours");
            }
            if (minute >= 1380 || minute < (sunday ? c.WakeSunday : c.WakeWeek))
            {
                PlCheck(r.Status == ScheduleStatus::Stay && r.Zone == c.Home && r.Doing == "sleeping" &&
                            intent.Intent.Availability == PersonAvailability::Asleep && !intent.Intent.bTalkable,
                        "asleep at home from 23:00 until waking, and not to be spoken to");
            }
        }
        PlCheck(counterOpen == (c.OpenA1 - c.OpenA0) + (c.OpenB1 - c.OpenB0), "every business minute was checked");
        // Opened sharp, held to the close, and the counter is held for a real share of each window.
        for (const int open : {c.OpenA0, c.OpenB0})
        {
            const auto r = PlResolve(schedule, day, open);
            PlCheck(r.Status == ScheduleStatus::Stay && r.Zone == c.Counter && r.Doing == "serving", "at the counter, serving, the minute the door opens");
        }
        for (const int close : {c.OpenA1, c.OpenB1})
        {
            const auto r = PlResolve(schedule, day, close - 1);
            PlCheck(r.Status == ScheduleStatus::Stay && r.Zone == c.Counter && r.Doing == "serving" && r.SegmentEnd == PlAt(day, close),
                    "the counter is held until the minute the door closes");
        }
        int morning = 0;
        int afternoon = 0;
        for (int minute = c.OpenA0; minute < c.OpenA1; ++minute)
            morning += PlResolve(schedule, day, minute).Zone == c.Counter && PlResolve(schedule, day, minute).Status == ScheduleStatus::Stay ? 1 : 0;
        for (int minute = c.OpenB0; minute < c.OpenB1; ++minute)
            afternoon += PlResolve(schedule, day, minute).Zone == c.Counter && PlResolve(schedule, day, minute).Status == ScheduleStatus::Stay ? 1 : 0;
        PlCheck(morning * 100 >= (c.OpenA1 - c.OpenA0) * 45 && afternoon * 100 >= (c.OpenB1 - c.OpenB0) * 45,
                "the counter is staffed for at least 45 percent of each business window");
        PlCheck(serving == morning + afternoon, "serving is exactly the time at the counter");
        // Work anchors are all used in their own roles; some on every day, some Monday to Saturday only.
        for (const std::string_view suffix : c.EveryDay)
            PlCheck(std::find(visited.begin(), visited.end(), PlZone(c, suffix)) != visited.end(), "every day uses this anchor");
        for (const std::string_view suffix : c.WeekdaysOnly)
            PlCheck((std::find(visited.begin(), visited.end(), PlZone(c, suffix)) != visited.end()) == !sunday, "this anchor is used Monday to Saturday only");
        // Meals, closure, evening and rest.
        const auto midday = PlResolve(schedule, day, c.MiddayMinute);
        PlCheck(midday.Status == ScheduleStatus::Stay && midday.Zone == c.MiddayZone && midday.Doing == c.MiddayDoing, "the midday meal is taken where authored");
        PlCheck(!PlOpen(c, c.MiddayMinute), "the midday meal falls in the closure");
        const auto breakfast = PlResolve(schedule, day, sunday ? c.CookSunday : c.CookWeek);
        PlCheck(breakfast.Zone == c.Home && breakfast.Doing == "cooking", "cooks breakfast at home");
        const auto supper = PlResolve(schedule, day, c.EveningCookMinute);
        PlCheck(supper.Zone == c.Home && supper.Doing == "cooking", "cooks supper at home");
        const auto reading = PlResolve(schedule, day, c.ReadMinute);
        PlCheck(reading.Zone == c.Home && reading.Doing == "reading", "reads at home before bed");
        const auto late = PlResolve(schedule, day, c.DutyEnd + 60);
        PlCheck(late.Zone == c.Home && !PlInShop(c, late), "an hour after the duty ends she is home");
    }
}

void PlInterruption(const PlCase& c, const NpcSchedule& schedule)
{
    for (const auto& sample : c.Samples)
    {
        const GameInstant at = PlAt(0, sample.Minute);
        const auto base = ResolveNpcIntentAt(schedule.Person, schedule.Travel, at, false, false);
        PlCheck(base.Ok() && base.Intent.State == sample.State, "the sampled minute is in the expected state");
        const auto chat = ResolveNpcIntentAt(schedule.Person, schedule.Travel, at, true, false);
        PlCheck(chat.Ok() && chat.Intent.State == NpcState::Chat && chat.Intent.ResumeState == base.Intent.State &&
                    chat.Intent.ResumeWorkActionId == base.Intent.WorkActionId && chat.Intent.AnchorId == base.Intent.AnchorId,
                "a chat suspends the work or walk and keeps what resumes");
        const auto blocked = ResolveNpcIntentAt(schedule.Person, schedule.Travel, at, false, true);
        PlCheck(blocked.Ok() && blocked.Intent.State == NpcState::Yield && blocked.Intent.ResumeState == base.Intent.State &&
                    blocked.Intent.AnchorId == base.Intent.AnchorId,
                "a blocked route holds the person in an explicit yield");
        const auto resumed = ResolveNpcIntentAt(schedule.Person, schedule.Travel, at, false, false);
        PlCheck(resumed.Ok() && resumed.Intent == base.Intent, "asking again without the flag resumes exactly the same intent");
    }
    const auto asleep = ResolveNpcIntentAt(schedule.Person, schedule.Travel, PlAt(0, 100), true, false);
    PlCheck(!asleep.Ok() && asleep.Error == NpcIntentError::ConversationUnavailable, "nobody chats with her while she sleeps");
}

void PlAdvance(const PlCase& c, const NpcSchedule& schedule)
{
    auto whole = StartSchedulePerson(c.PersonId, PlAt(0, 0));
    auto split = whole;
    const auto end = PlAt(7, 0);
    const auto full = AdvanceSchedulePerson(schedule.Person, schedule.Travel, whole, end);
    PlCheck(full.Ok, "whole week advance succeeds");
    std::vector<ScheduleEvent> events;
    while (split.At < end)
    {
        const auto part = AdvanceSchedulePerson(schedule.Person, schedule.Travel, split, std::min(end, split.At + 37));
        PlCheck(part.Ok, "split advance succeeds");
        events.insert(events.end(), part.Events.begin(), part.Events.end());
    }
    PlCheck(whole == split && full.Events == events, "split and whole advances produce identical state and events");
    int leaves = 0;
    int returns = 0;
    for (const auto& event : full.Events)
    {
        if (event.Kind != ScheduleEventKind::TravelBegan)
            continue;
        leaves += event.FromZone == c.Home && PlStarts(event.Zone, c.Prefix) ? 1 : 0;
        returns += event.Zone == c.Home && PlStarts(event.FromZone, c.Prefix) ? 1 : 0;
    }
    PlCheck(leaves == 7 * c.HomeLeaves && returns == 7 * c.HomeLeaves, "she walks to the shop and home the authored number of times each day");
}

void PlRoutes(const PlCase& c, const NpcSchedule& schedule)
{
    PlCheck(schedule.Routes.size() == c.RouteCm.size(), "every approved route is authored");
    for (std::size_t i = 0; i < c.RouteCm.size(); ++i)
        PlCheck(schedule.Routes[i].LengthCm == c.RouteCm[i], "authored geometry matches the built polyline or its sub-path");
    PlCheck(schedule.Paths.size() == c.Legs, "every undirected leg is explicit");
    for (std::size_t i = 0; i < schedule.Paths.size(); ++i)
    {
        const auto& path = schedule.Paths[i];
        int length = 0;
        std::string at = path.From;
        for (const auto& id : path.Routes)
        {
            const auto route = std::find_if(schedule.Routes.begin(), schedule.Routes.end(), [&](const auto& item) { return item.Id == id; });
            PlCheck(route != schedule.Routes.end(), "a leg uses authored geometry");
            at = route->From == at ? route->To : route->From;
            length += route->LengthCm;
        }
        PlCheck(at == path.To && path.Minutes == (length + 139) / 140, "every leg ends where it says and is the integer ceiling of its length at 140 cm per minute");
        if (i > 0)
            PlCheck(schedule.Paths[i - 1].From < path.From || (schedule.Paths[i - 1].From == path.From && schedule.Paths[i - 1].To < path.To),
                    "legs are strictly ascending (from, to)");
    }
}

void PlRun(const PlCase& c, const char* file, std::string_view firstShopZone, int firstShopStart, std::string_view lastShopZone, int lastShopStart)
{
    std::ifstream input(file);
    PlCheck(input.good(), "authored content exists");
    const std::string text{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const auto parsed = ParseJson(text);
    PlCheck(parsed.Value.has_value(), "the JSON file parses");
    const auto db = PlDatabase(c, *parsed.Value);
    const auto loaded = LoadNpcSchedule(db, c.ScheduleId);
    if (!loaded.Ok())
        std::cerr << loaded.Error << '\n';
    PlCheck(loaded.Ok(), "typed content loads and validates");
    const auto again = LoadNpcSchedule(db, c.ScheduleId);
    PlCheck(again.Ok() && again.Value->Person == loaded.Value->Person && again.Value->Travel == loaded.Value->Travel, "typed loading is deterministic");
    PlCheck(loaded.Value->Person.PersonId == c.PersonId && loaded.Value->Person.HomeZone == c.Home && ValidateNpcScheduleContent(*loaded.Value).empty(),
            "the decoded schedule revalidates");
    std::vector<ContentIssue> issues;
    ContentSchema::ValidateNpcScheduleSemantics(db, issues);
    PlCheck(issues.empty(), "exported semantic validator accepts the content");
    const std::string registeredName = "Data/Content/npc_schedule/" + std::string(c.ScheduleId) + ".json";
    const auto registered = LoadContent({{registeredName, text}});
    for (const auto& issue : registered.Issues)
        std::cerr << issue.File << ' ' << issue.Id << ": " << issue.Message << '\n';
    PlCheck(registered.Ok() && LoadNpcSchedule(registered.Db, c.ScheduleId).Ok(), "loads through the registered schema");
    PlCheck(!LoadNpcSchedule(registered.Db, CareWorkerScheduleId).Ok() && !LoadNpcSchedule(registered.Db, "npc_schedule.anindita_sen").Ok(),
            "the others are not supplied by this file");
    // The door table, once it holds this door, must agree exactly with the hours authored here.
    if (const DoorDef* door = FindDoor(c.DoorId))
    {
        PlCheck(door->Access == DoorAccess::BusinessHours && door->OpeningWindows.size() == 2 && door->OpeningWindows[0].OpenMinute == c.OpenA0 &&
                    door->OpeningWindows[0].CloseMinute == c.OpenA1 && door->OpeningWindows[1].OpenMinute == c.OpenB0 &&
                    door->OpeningWindows[1].CloseMinute == c.OpenB1,
                "the door table's business windows are exactly the hours the schedule keeps");
    }
    PlMalformed(c, *parsed.Value, firstShopZone, firstShopStart, lastShopZone, lastShopStart);
    PlDay(c, *loaded.Value);
    PlInterruption(c, *loaded.Value);
    PlAdvance(c, *loaded.Value);
    PlRoutes(c, *loaded.Value);
}
} // namespace

int main(int argc, char** argv)
{
    PlCheck(argc == 3, "two content file arguments supplied (Debashis, Mahua)");
    const PlCase debashis{"Debashis", "npc_schedule.debashis_pal", "person.debashis_pal", "location.nabapara.pal_home",
                          "door.nabapara.repair_workshop_front", "anchor.repair_workshop.", "anchor.repair_workshop.staff_counter",
                          "location.nabapara.repair_workshop", 480, 720, 840, 1110, 450, 1140, 330, 390, 780, "location.nabapara.pal_home", "at_home",
                          400, 430, 1190, 1330, {1582, 953, 947, 1537, 306, 590, 1399, 1680}, 15, 2,
                          {{500, NpcState::Work}, {600, NpcState::Work}, {470, NpcState::Work}, {548, NpcState::Travel}, {650, NpcState::Work}, {930, NpcState::Work}},
                          {"staff_counter", "workbench", "tool_rack"}, {"finishing_table"}};
    const PlCase mahua{"Mahua", "npc_schedule.mahua_saha", "person.mahua_saha", "location.nabapara.saha_home", "door.nabapara.hardware_store_front",
                       "anchor.hardware_store.", "anchor.hardware_store.staff_counter", "location.nabapara.hardware_store", 540, 780, 900, 1200, 500, 1230,
                       390, 450, 840, "anchor.hardware_store.stock_room", "break", 440, 490, 1270, 1365,
                       {2100, 1135, 1197, 158, 1505, 1820}, 10, 1,
                       {{600, NpcState::Work}, {650, NpcState::Work}, {520, NpcState::Work}, {628, NpcState::Travel}, {870, NpcState::Work}, {1215, NpcState::Work}},
                       {"staff_counter", "stock_room", "weigh_station"}, {}};
    PlRun(debashis, argv[1], "anchor.repair_workshop.workbench", 462, "anchor.repair_workshop.tool_rack", 1117);
    PlRun(mahua, argv[2], "anchor.hardware_store.stock_room", 500, "anchor.hardware_store.weigh_station", 1209);
    std::cout << "PerfectWorldProprietorLifeTests passed (" << PlChecks << " checks)\n";
    return 0;
}
