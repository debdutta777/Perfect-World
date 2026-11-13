// Subrata Dey, the lane caretaker (UP-03a): his authored schedule loads through the generic npc_schedule loader and is checked for
// every minute of nine days including a weekend; he is at home, awake, in every minute of his flat's visiting hours (read from the
// door table, not copied) so a knock is answered; routes and travel legs are the integer ceilings of the authored geometry.
// His conversation and world save are covered by SubrataConversationTests.cpp.
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
int SbChecks = 0;
}

void Check(bool condition, const char* message)
{
    ++SbChecks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

namespace
{
constexpr std::string_view SbScheduleId = "npc_schedule.subrata_dey";
constexpr std::string_view SbPerson = "person.subrata_dey";
constexpr std::string_view SbHome = "location.nabapara.caretaker_flat";
constexpr std::string_view SbLane = "location.nabapara.lane_round";
constexpr std::string_view SbEntry = "anchor.tall_flats.front_door";
constexpr std::string_view SbBoard = "anchor.tall_flats.noticeboard";
constexpr std::string_view SbStair = "anchor.tall_flats.stair";
constexpr std::string_view SbDoorId = "door.nabapara.caretaker_flat_front";

/** Day 0 is a Monday: instant 0 is Monday 06:00, and the first week is skipped so that no instant is negative. */
GameInstant SbAt(int day, int minute) { return static_cast<GameInstant>(day + 7) * MinutesPerDay + minute - 360; }

JsonValue& SbField(JsonValue& object, std::string_view name)
{
    for (auto& [key, value] : object.Object)
        if (key == name)
            return value;
    std::exit(2);
}

/** The first routine entry with this zone and weekday mask. */
JsonValue& SbEntryOf(JsonValue& root, std::string_view zone, int days)
{
    for (auto& item : SbField(root, "routine").Array)
        if (SbField(item, "zone").String == zone && *SbField(item, "days").Integer == days)
            return item;
    std::exit(3);
}

ContentDb SbDatabase(const JsonValue& root)
{
    ContentDb db;
    const std::string id(SbScheduleId);
    db.Records.emplace(id, ContentRecord{"npc_schedule", id, "subrata_dey.json", root, {}});
    return db;
}

void SbRefuse(const JsonValue& root, const char* message)
{
    const auto db = SbDatabase(root);
    Check(!LoadNpcSchedule(db, SbScheduleId).Ok(), message);
    std::vector<ContentIssue> issues;
    ContentSchema::ValidateNpcScheduleSemantics(db, issues);
    Check(!issues.empty(), "semantic entry point reports the rejected record");
}

void SbMalformed(const JsonValue& root)
{
    auto bad = root;
    SbField(bad, "person").String = "person.subrata_dey_two";
    SbRefuse(bad, "unfrozen identity refused");
    bad = root;
    SbField(bad, "home").String = "location.nabapara.other_home";
    SbRefuse(bad, "wrong home refused");
    bad = root;
    SbField(bad, "workplace").String = "location.nabapara.other";
    SbRefuse(bad, "wrong workplace refused");
    bad = root;
    SbField(bad, "door").String = "door.nabapara.other";
    SbRefuse(bad, "wrong door refused");
    bad = root;
    SbField(bad, "speedCmPerMinute").Integer = 141;
    SbRefuse(bad, "wrong travel speed refused");
    bad = root;
    bad.Object.push_back({"unexpected", JsonValue{}});
    SbRefuse(bad, "unknown field refused");
    bad = root;
    SbField(SbField(bad, "routine").Array.front(), "role").String = "unknown_role";
    SbRefuse(bad, "unknown role refused");
    bad = root;
    SbField(SbEntryOf(bad, SbBoard, 62), "start").Integer = 520;
    SbRefuse(bad, "a squeezed arrival at the noticeboard refused");
    bad = root;
    SbField(SbEntryOf(bad, SbEntry, 126), "start").Integer = 430;
    SbRefuse(bad, "work at the tall flats before 07:16 refused");
    bad = root;
    SbField(SbEntryOf(bad, SbBoard, 62), "end").Integer = 1030;
    SbRefuse(bad, "work at the tall flats after 17:00 refused");
    bad = root;
    SbField(SbEntryOf(bad, SbBoard, 62), "zone").String = "anchor.tall_flats.roof";
    SbRefuse(bad, "unapproved zone refused");
    bad = root;
    SbField(SbEntryOf(bad, SbHome, 62), "days").Integer = 0;
    SbRefuse(bad, "empty weekday mask refused");
    bad = root;
    SbField(bad, "routine").Array.erase(SbField(bad, "routine").Array.begin() + (&SbEntryOf(bad, SbLane, 126) - &SbField(bad, "routine").Array.front()));
    SbRefuse(bad, "an unexplained gap after the lane round refused");
    bad = root;
    SbField(bad, "travel").Array.pop_back();
    SbRefuse(bad, "missing explicit leg refused");
    bad = root;
    SbField(bad, "travel").Array.erase(SbField(bad, "travel").Array.begin() + 4);
    SbRefuse(bad, "a missing middle leg refused");
    bad = root;
    SbField(SbField(bad, "travel").Array.front(), "minutes").Integer = 1;
    SbRefuse(bad, "short travel duration refused");
    bad = root;
    SbField(SbField(bad, "travel").Array.front(), "routes").Array.front().String = "route.unknown";
    SbRefuse(bad, "unknown route refused");
    bad = root;
    SbField(SbField(bad, "routes").Array.front(), "lengthCm").Integer = 1;
    SbRefuse(bad, "undersized frozen geometry refused");
    bad = root;
    SbField(bad, "routes").Array.pop_back();
    SbRefuse(bad, "missing approved route refused");
    Check(!LoadNpcSchedule(ContentDb{}, SbScheduleId).Ok(), "missing content refused");
    Check(!LoadNpcSchedule(SbDatabase(root), "npc_schedule.nobody").Ok(), "unknown schedule identity refused");
}

ScheduleResolution SbResolve(const NpcSchedule& schedule, int day, int minute)
{
    return ResolveSchedule(schedule.Person, schedule.Travel, SbAt(day, minute));
}

bool SbAtHome(const ScheduleResolution& resolved) { return resolved.Status == ScheduleStatus::Stay && resolved.Zone == SbHome; }
bool SbAtTallFlats(const ScheduleResolution& resolved) { return resolved.Status == ScheduleStatus::Stay && resolved.Zone.starts_with("anchor.tall_flats."); }

/** Day 0..8: Monday, Tuesday, ..., Sunday, Monday, Tuesday. */
void SbWeek(const NpcSchedule& schedule)
{
    for (int day = 0; day <= 8; ++day)
    {
        const int weekday = day % 7; // 0 Monday .. 6 Sunday
        const bool weekdayMonFri = weekday <= 4;
        const bool saturday = weekday == 5;
        const bool sunday = weekday == 6;
        for (int minute = 0; minute < 1440; ++minute)
        {
            const auto resolved = SbResolve(schedule, day, minute);
            Check(resolved.Valid && !resolved.Squeezed && resolved.Status != ScheduleStatus::Idle,
                  "every minute, including the weekly rollover, has an explicit stay or travel");
            const auto intent = ResolveNpcIntentAt(schedule.Person, schedule.Travel, SbAt(day, minute), false, false);
            Check(intent.Ok(), "every authored role maps to an NPC intent");
            if (minute < 315 || minute >= 1290)
            {
                Check(SbAtHome(resolved) && resolved.Doing == "sleeping" && intent.Intent.Availability == PersonAvailability::Asleep,
                      "asleep at home from 21:30 until 05:15, every day");
            }
            if (SbAtTallFlats(resolved))
            {
                Check((!sunday && minute >= 436 && minute < 1020) || (sunday && resolved.Zone == SbEntry && minute >= 513 && minute < 543),
                      "at the tall flats only in the daytime duty hours (Sunday only the entry check)");
                if (resolved.Zone == SbStair)
                    Check(weekdayMonFri && ((minute >= 488 && minute < 520 && resolved.Doing == "sweeping") || (minute >= 908 && minute < 950 && resolved.Doing == "repairing")),
                          "the stair is swept in the morning and attended to in the afternoon, Monday to Friday only");
                if (resolved.Zone == SbBoard)
                    Check(!sunday && resolved.Doing == "observing" && intent.Intent.Availability == PersonAvailability::Awake && intent.Intent.bTalkable,
                          "at the noticeboard he can be spoken to, never on Sunday");
                if (resolved.Doing == "sweeping" || resolved.Doing == "repairing" || resolved.Doing == "inspecting")
                    Check(intent.Intent.Availability == PersonAvailability::Busy && intent.Intent.State == NpcState::Work, "a job in hand declines conversation");
            }
            if (resolved.Status == ScheduleStatus::Stay && resolved.Zone == SbLane)
                Check(!sunday && ((minute >= 375 && minute < 420) || (minute >= 1248 && minute < 1272)), "the lane round is Monday to Saturday, early morning and evening");
            if (resolved.Status == ScheduleStatus::Travel)
                Check(intent.Intent.State == NpcState::Travel && intent.Intent.Availability == PersonAvailability::Busy, "walking declines conversation");
        }
        // Key moments.
        Check(SbResolve(schedule, day, 100).Doing == "sleeping" && SbResolve(schedule, day, 320).Doing == "at_home", "sleep ends at 05:15 and breakfast follows");
        Check(SbAtHome(SbResolve(schedule, day, 1100)) && SbAtHome(SbResolve(schedule, day, 1229)) && SbAtHome(SbResolve(schedule, day, 1290)), "home from 17:30 to 20:30, and again at the 21:30 sleep");
        if (!sunday)
        {
            const auto lane = SbResolve(schedule, day, 375);
            Check(lane.Status == ScheduleStatus::Stay && lane.Zone == SbLane && lane.Doing == "inspecting", "06:15: on the lane round");
            Check(SbResolve(schedule, day, 374).Status == ScheduleStatus::Travel, "eighteen minutes walking to the lane");
            const auto entry = SbResolve(schedule, day, 436);
            Check(entry.Zone == SbEntry && entry.Doing == "inspecting", "07:16: checking the shared entry");
            Check(SbResolve(schedule, day, 1260).Zone == SbLane && SbResolve(schedule, day, 1260).Doing == "inspecting", "the evening round of the lane");
        }
        if (weekdayMonFri)
        {
            Check(SbResolve(schedule, day, 500).Zone == SbStair && SbResolve(schedule, day, 540).Zone == SbBoard, "stair, then noticeboard, in the morning");
            Check(SbAtHome(SbResolve(schedule, day, 590)) && SbResolve(schedule, day, 589).Status == ScheduleStatus::Travel, "home for the midday meal at 09:50");
            Check(SbResolve(schedule, day, 750).Doing == "resting" && SbResolve(schedule, day, 820).Doing == "reading", "midday rest, then reading");
            Check(SbResolve(schedule, day, 885).Zone == SbEntry && SbResolve(schedule, day, 930).Zone == SbStair && SbResolve(schedule, day, 980).Zone == SbBoard,
                  "the afternoon: entry, stair, noticeboard");
            Check(SbAtHome(SbResolve(schedule, day, 1050)) && SbResolve(schedule, day, 1049).Status == ScheduleStatus::Travel, "home from 17:30");
        }
        if (saturday)
        {
            Check(SbResolve(schedule, day, 500).Zone == SbBoard, "Saturday: the noticeboard straight after the entry");
            Check(SbAtHome(SbResolve(schedule, day, 557)), "Saturday: home for the meal at 09:17");
            for (int minute = 557; minute < 1230; ++minute)
                Check(SbAtHome(SbResolve(schedule, day, minute)), "Saturday afternoon and evening are spent at home");
        }
        if (sunday)
        {
            Check(SbResolve(schedule, day, 400).Doing == "at_home" && SbResolve(schedule, day, 520).Zone == SbEntry, "Sunday: a late start and one check of the entry");
            for (int minute = 576; minute < 1440; ++minute)
                Check(SbAtHome(SbResolve(schedule, day, minute)), "Sunday afternoon and evening are spent at home");
            Check(SbResolve(schedule, day, 1260).Doing == "resting", "Sunday evening he rests, no round");
        }
    }
    // Weekend to Monday: Sunday night is sleep, and Monday begins the round without a gap.
    Check(SbResolve(schedule, 6, 1439).Doing == "sleeping" && SbResolve(schedule, 7, 0).Doing == "sleeping", "Sunday night sleep runs into Monday");
    Check(SbResolve(schedule, 7, 375).Zone == SbLane, "Monday 06:15 on the lane");
}

/** The core's own visiting hours are read from the door table; at home and awake in every one of those minutes. */
void SbVisitingHours(const NpcSchedule& schedule)
{
    const DoorDef* door = FindDoor(SbDoorId);
    Check(door != nullptr && door->Access == DoorAccess::Private && door->LocationId == SbHome, "the caretaker flat's front door is private and is his home");
    Check(door->VisitingWindows.size() == 2 && door->VisitingWindows[0].OpenMinute == 600 && door->VisitingWindows[0].CloseMinute == 750 &&
              door->VisitingWindows[1].OpenMinute == 1050 && door->VisitingWindows[1].CloseMinute == 1230,
          "visiting hours are 10:00-12:30 and 17:30-20:30");
    int answered = 0;
    for (int day = 0; day <= 8; ++day)
    {
        for (const DoorOpeningWindow& window : door->VisitingWindows)
        {
            for (int minute = window.OpenMinute; minute < window.CloseMinute; ++minute)
            {
                const auto resolved = SbResolve(schedule, day, minute);
                const auto intent = ResolveNpcIntentAt(schedule.Person, schedule.Travel, SbAt(day, minute), false, false);
                const bool hostHome = SbAtHome(resolved) && intent.Ok() && intent.Intent.Availability == PersonAvailability::Awake && intent.Intent.bTalkable;
                Check(hostHome, "he is home, awake and able to talk in every minute of the visiting hours");
                DoorUseContext context;
                context.IsHostHome = hostHome;
                Check(EvaluateDoorKnock(*door, minute, context).Answer == DoorKnockAnswer::Admitted, "a knock in visiting hours is answered");
                ++answered;
            }
        }
        // Not home means no answer: on the round, the knock finds nobody.
        DoorUseContext away;
        away.IsHostHome = false;
        Check(EvaluateDoorKnock(*door, 500, away).Answer != DoorKnockAnswer::Admitted, "away from home nobody answers");
    }
    Check(answered == 9 * (150 + 180), "9 days of 330 visiting minutes were all checked");
}

void SbAdvance(const NpcSchedule& schedule)
{
    auto whole = StartSchedulePerson(SbPerson, SbAt(0, 0));
    auto split = whole;
    const auto end = SbAt(8, 0);
    const auto full = AdvanceSchedulePerson(schedule.Person, schedule.Travel, whole, end);
    Check(full.Ok, "whole week advance succeeds");
    std::vector<ScheduleEvent> events;
    while (split.At < end)
    {
        const auto part = AdvanceSchedulePerson(schedule.Person, schedule.Travel, split, std::min(end, split.At + 37));
        Check(part.Ok, "split advance succeeds");
        events.insert(events.end(), part.Events.begin(), part.Events.end());
    }
    Check(whole == split && full.Events == events, "split and whole advances produce identical state and events");
    int noticeboardStays = 0;
    for (const auto& event : full.Events)
        if (event.Kind == ScheduleEventKind::StayBegan && event.Zone == SbBoard)
            ++noticeboardStays;
    Check(noticeboardStays == 5 * 2 + 1 + 2, "two stays on each of five weekdays, one on Saturday, and two on the following Monday");
}

void SbRoutes(const NpcSchedule& schedule)
{
    const std::vector<int> lengths{2400, 2200, 500, 500};
    Check(schedule.Routes.size() == lengths.size(), "four approved routes");
    for (std::size_t i = 0; i < lengths.size(); ++i)
        Check(schedule.Routes[i].LengthCm == lengths[i], "authored geometry matches the contract");
    Check(schedule.Paths.size() == 10, "all ten undirected legs between five zones are explicit");
    for (const auto& path : schedule.Paths)
    {
        int length = 0;
        for (const auto& id : path.Routes)
        {
            const auto route = std::find_if(schedule.Routes.begin(), schedule.Routes.end(), [&](const auto& item) { return item.Id == id; });
            Check(route != schedule.Routes.end(), "path uses authored geometry");
            length += route->LengthCm;
        }
        Check(path.Minutes == (length + 139) / 140, "every leg is the integer ceiling of its total length at 140 cm per minute");
    }
    const auto leg = [&](std::string_view from, std::string_view to) {
        for (const auto& item : schedule.Paths)
            if ((item.From == from && item.To == to) || (item.From == to && item.To == from))
                return item.Minutes;
        return -1;
    };
    Check(leg(SbHome, SbLane) == 18 && leg(SbLane, SbEntry) == 16 && leg(SbEntry, SbHome) == 33, "home, lane and entry legs are 18, 16 and 33 minutes");
    Check(leg(SbEntry, SbBoard) == 4 && leg(SbBoard, SbStair) == 4 && leg(SbEntry, SbStair) == 8 && leg(SbBoard, SbHome) == 37, "tall-flats legs");
}

} // namespace

int main(int argc, char** argv)
{
    Check(argc == 2, "content file argument supplied");
    std::ifstream input(argv[1]);
    Check(input.good(), "Subrata authored content exists");
    const std::string text{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const auto parsed = ParseJson(text);
    Check(parsed.Value.has_value(), "the JSON file parses");
    const auto db = SbDatabase(*parsed.Value);
    const auto loaded = LoadNpcSchedule(db, SbScheduleId);
    if (!loaded.Ok())
        std::cerr << loaded.Error << '\n';
    Check(loaded.Ok(), "typed Subrata content loads and validates");
    const auto again = LoadNpcSchedule(db, SbScheduleId);
    Check(again.Ok() && again.Value->Person == loaded.Value->Person && again.Value->Travel == loaded.Value->Travel, "typed loading is deterministic");
    Check(loaded.Value->Person.PersonId == SbPerson && loaded.Value->Person.HomeZone == SbHome && ValidateNpcScheduleContent(*loaded.Value).empty(), "the decoded schedule revalidates");
    std::vector<ContentIssue> issues;
    ContentSchema::ValidateNpcScheduleSemantics(db, issues);
    Check(issues.empty(), "exported semantic validator accepts the content");
    const auto registered = LoadContent({{"Data/Content/npc_schedule/npc_schedule.subrata_dey.json", text}});
    for (const auto& issue : registered.Issues)
        std::cerr << issue.File << ' ' << issue.Id << ": " << issue.Message << '\n';
    Check(registered.Ok() && LoadNpcSchedule(registered.Db, SbScheduleId).Ok(), "he loads through the registered schema");
    Check(!LoadNpcSchedule(registered.Db, CareWorkerScheduleId).Ok() && !LoadNpcSchedule(registered.Db, "npc_schedule.mira_sen").Ok(), "the others are not supplied by his file");

    SbMalformed(*parsed.Value);
    SbWeek(*loaded.Value);
    SbVisitingHours(*loaded.Value);
    SbAdvance(*loaded.Value);
    SbRoutes(*loaded.Value);
    std::cout << "PerfectWorldSubrataLifeTests passed (" << SbChecks << " checks)\n";
    return 0;
}
