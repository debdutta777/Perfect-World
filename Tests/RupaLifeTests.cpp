// Rupa Ghosh, who keeps the tea row house and works away from the lane (NR-01): her authored schedule loads through the generic
// npc_schedule loader and is checked for every minute of nine days including a weekend; she sleeps, works and is at home exactly as
// authored; her front door's visiting hours are read from the door table (not copied) and are met by a host who is home on every
// day, with genuine awake periods inside them and the one stretch (Monday to Friday, just after 16:00) in which she is still
// walking home and nobody answers; routes and travel legs are the integer ceilings of the authored geometry. Her conversation and
// world save are covered by RupaConversationTests.cpp.
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
int RgChecks = 0;
}

void Check(bool condition, const char* message)
{
    ++RgChecks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

namespace
{
constexpr std::string_view RgScheduleId = "npc_schedule.rupa_ghosh";
constexpr std::string_view RgPerson = "person.rupa_ghosh";
constexpr std::string_view RgHome = "location.nabapara.tea_row_house";
constexpr std::string_view RgWork = "location.nabapara.rupa_work";
constexpr std::string_view RgDoorId = "door.nabapara.tea_row_house_front";

/** Day 0 is a Monday: instant 0 is Monday 06:00, and the first week is skipped so that no instant is negative. */
GameInstant RgAt(int day, int minute) { return static_cast<GameInstant>(day + 7) * MinutesPerDay + minute - 360; }

JsonValue& RgField(JsonValue& object, std::string_view name)
{
    for (auto& [key, value] : object.Object)
        if (key == name)
            return value;
    std::exit(2);
}

/** The first routine entry with this zone, weekday mask and start minute. */
JsonValue& RgEntryOf(JsonValue& root, std::string_view zone, int days, int start)
{
    for (auto& item : RgField(root, "routine").Array)
        if (RgField(item, "zone").String == zone && *RgField(item, "days").Integer == days && *RgField(item, "start").Integer == start)
            return item;
    std::exit(3);
}

ContentDb RgDatabase(const JsonValue& root)
{
    ContentDb db;
    const std::string id(RgScheduleId);
    db.Records.emplace(id, ContentRecord{"npc_schedule", id, "rupa_ghosh.json", root, {}});
    return db;
}

void RgRefuse(const JsonValue& root, const char* message)
{
    const auto db = RgDatabase(root);
    Check(!LoadNpcSchedule(db, RgScheduleId).Ok(), message);
    std::vector<ContentIssue> issues;
    ContentSchema::ValidateNpcScheduleSemantics(db, issues);
    Check(!issues.empty(), "semantic entry point reports the rejected record");
}

void RgMalformed(const JsonValue& root)
{
    auto bad = root;
    RgField(bad, "person").String = "person.rupa_ghosh_two";
    RgRefuse(bad, "unfrozen identity refused");
    bad = root;
    RgField(bad, "home").String = "location.nabapara.other_home";
    RgRefuse(bad, "wrong home refused");
    bad = root;
    RgField(bad, "workplace").String = "location.nabapara.other";
    RgRefuse(bad, "wrong workplace refused");
    bad = root;
    RgField(bad, "door").String = "door.nabapara.other";
    RgRefuse(bad, "wrong door refused");
    bad = root;
    RgField(bad, "speedCmPerMinute").Integer = 141;
    RgRefuse(bad, "wrong travel speed refused");
    bad = root;
    bad.Object.push_back({"unexpected", JsonValue{}});
    RgRefuse(bad, "unknown field refused");
    bad = root;
    RgField(RgField(bad, "routine").Array.front(), "role").String = "unknown_role";
    RgRefuse(bad, "unknown role refused");
    bad = root;
    RgField(RgEntryOf(bad, RgWork, 62, 480), "start").Integer = 470;
    RgRefuse(bad, "a squeezed arrival at work refused");
    bad = root;
    RgField(RgEntryOf(bad, RgWork, 62, 480), "start").Integer = 470;
    RgField(RgEntryOf(bad, RgHome, 126, 420), "end").Integer = 459;
    RgRefuse(bad, "work before 08:00 refused");
    bad = root;
    RgField(RgEntryOf(bad, RgWork, 62, 765), "end").Integer = 975;
    RgField(RgEntryOf(bad, RgHome, 62, 971), "start").Integer = 986;
    RgRefuse(bad, "work after 16:00 refused");
    bad = root;
    RgField(RgEntryOf(bad, RgWork, 62, 480), "zone").String = "location.nabapara.rupa_office";
    RgRefuse(bad, "unapproved zone refused");
    bad = root;
    RgField(RgEntryOf(bad, RgHome, 62, 971), "days").Integer = 0;
    RgRefuse(bad, "empty weekday mask refused");
    bad = root;
    RgField(bad, "routine").Array.erase(RgField(bad, "routine").Array.begin() +
                                        (&RgEntryOf(bad, RgHome, 62, 971) - &RgField(bad, "routine").Array.front()));
    RgRefuse(bad, "an unexplained gap after the walk home refused");
    bad = root;
    RgField(bad, "travel").Array.pop_back();
    RgRefuse(bad, "missing explicit leg refused");
    bad = root;
    RgField(RgField(bad, "travel").Array.front(), "minutes").Integer = 1;
    RgRefuse(bad, "short travel duration refused");
    bad = root;
    RgField(RgField(bad, "travel").Array.front(), "routes").Array.front().String = "route.unknown";
    RgRefuse(bad, "unknown route refused");
    bad = root;
    RgField(RgField(bad, "routes").Array.front(), "lengthCm").Integer = 1;
    RgRefuse(bad, "undersized frozen geometry refused");
    bad = root;
    RgField(bad, "routes").Array.pop_back();
    RgRefuse(bad, "missing approved route refused");
    Check(!LoadNpcSchedule(ContentDb{}, RgScheduleId).Ok(), "missing content refused");
    Check(!LoadNpcSchedule(RgDatabase(root), "npc_schedule.nobody").Ok(), "unknown schedule identity refused");
}

ScheduleResolution RgResolve(const NpcSchedule& schedule, int day, int minute)
{
    return ResolveSchedule(schedule.Person, schedule.Travel, RgAt(day, minute));
}

NpcIntentResult RgIntent(const NpcSchedule& schedule, int day, int minute)
{
    return ResolveNpcIntentAt(schedule.Person, schedule.Travel, RgAt(day, minute), false, false);
}

bool RgAtHome(const ScheduleResolution& resolved) { return resolved.Status == ScheduleStatus::Stay && resolved.Zone == RgHome; }
bool RgAtWork(const ScheduleResolution& resolved) { return resolved.Status == ScheduleStatus::Stay && resolved.Zone == RgWork; }

/** Day 0..8: Monday, Tuesday, ..., Sunday, Monday, Tuesday. */
void RgWeek(const NpcSchedule& schedule)
{
    for (int day = 0; day <= 8; ++day)
    {
        const int weekday = day % 7; // 0 Monday .. 6 Sunday
        const bool weekdayMonFri = weekday <= 4;
        const bool saturday = weekday == 5;
        const bool sunday = weekday == 6;
        const int workEnd = saturday ? 780 : 960;
        for (int minute = 0; minute < 1440; ++minute)
        {
            const auto resolved = RgResolve(schedule, day, minute);
            Check(resolved.Valid && !resolved.Squeezed && resolved.Status != ScheduleStatus::Idle,
                  "every minute, including the weekly rollover, has an explicit stay or travel");
            const auto intent = RgIntent(schedule, day, minute);
            Check(intent.Ok(), "every authored role maps to an NPC intent");
            if (minute < 330 || minute >= 1320)
            {
                Check(RgAtHome(resolved) && resolved.Doing == "sleeping" && intent.Intent.Availability == PersonAvailability::Asleep,
                      "asleep at home from 22:00 until 05:30, every day");
            }
            if (RgAtWork(resolved))
            {
                Check(!sunday && minute >= 480 && minute < workEnd, "at work only Monday to Saturday in the working hours (Saturday to 13:00)");
                Check(resolved.Doing == "stocking" || resolved.Doing == "break", "work is stocking, with a break");
                if (resolved.Doing == "stocking")
                    Check(intent.Intent.Availability == PersonAvailability::Busy && intent.Intent.State == NpcState::Work, "work declines conversation");
            }
            if (resolved.Status == ScheduleStatus::Travel)
            {
                Check(!sunday && intent.Intent.State == NpcState::Travel && intent.Intent.Availability == PersonAvailability::Busy, "walking declines conversation, never on Sunday");
                Check((minute >= 469 && minute < 480) || (minute >= workEnd && minute < workEnd + 11), "the only walks are the eleven minutes each way");
            }
            if (resolved.Status == ScheduleStatus::Stay && resolved.Doing == "cooking")
                Check(intent.Intent.Availability == PersonAvailability::Busy, "cooking declines conversation");
        }
        // Key moments.
        Check(RgAtHome(RgResolve(schedule, day, 100)) && RgResolve(schedule, day, 340).Doing == "at_home", "sleep ends at 05:30 and the morning begins at home");
        Check(RgResolve(schedule, day, 1030).Doing == "cooking" && RgAtHome(RgResolve(schedule, day, 1030)), "17:10: cooking at home, every day");
        Check(RgResolve(schedule, day, 1100).Doing == "at_home" && RgResolve(schedule, day, 1250).Doing == "sweeping" && RgResolve(schedule, day, 1290).Doing == "reading",
              "supper, the evening household work and reading");
        Check(RgAtHome(RgResolve(schedule, day, 1100)) && RgAtHome(RgResolve(schedule, day, 1169)) && RgAtHome(RgResolve(schedule, day, 1170)), "home from 18:00 to 19:30 and after");
        if (!sunday)
        {
            Check(RgResolve(schedule, day, 380).Doing == "cooking" && RgResolve(schedule, day, 440).Doing == "at_home", "breakfast is cooked, then eaten");
            Check(RgResolve(schedule, day, 469).Status == ScheduleStatus::Travel && RgResolve(schedule, day, 479).Status == ScheduleStatus::Travel, "eleven minutes walking to work");
            const auto arrive = RgResolve(schedule, day, 480);
            Check(RgAtWork(arrive) && arrive.Doing == "stocking", "08:00: at work");
            Check(RgResolve(schedule, day, workEnd - 1).Zone == RgWork && RgResolve(schedule, day, workEnd).Status == ScheduleStatus::Travel, "work ends and the walk home begins");
            Check(RgAtHome(RgResolve(schedule, day, workEnd + 11)) && RgResolve(schedule, day, workEnd + 10).Status == ScheduleStatus::Travel, "home eleven minutes after work ends");
        }
        if (weekdayMonFri)
        {
            Check(RgResolve(schedule, day, 730).Doing == "break" && RgResolve(schedule, day, 770).Doing == "stocking", "the midday break, then work resumes");
            Check(RgResolve(schedule, day, 975).Doing == "resting" && RgResolve(schedule, day, 1019).Doing == "resting", "back at 16:11 and resting until 17:00");
        }
        if (saturday)
        {
            Check(RgResolve(schedule, day, 610).Doing == "break" && RgResolve(schedule, day, 640).Doing == "stocking", "Saturday: the break at 10:00");
            Check(RgAtHome(RgResolve(schedule, day, 791)) && RgResolve(schedule, day, 850).Doing == "sweeping", "Saturday: home at 13:11, then the house is swept");
            for (int minute = 791; minute < 1440; ++minute)
                Check(RgAtHome(RgResolve(schedule, day, minute)), "Saturday afternoon and evening are spent at home");
        }
        if (sunday)
        {
            for (int minute = 0; minute < 1440; ++minute)
                Check(RgAtHome(RgResolve(schedule, day, minute)), "Sunday is spent entirely at home");
            Check(RgResolve(schedule, day, 380).Doing == "at_home" && RgResolve(schedule, day, 430).Doing == "cooking" && RgResolve(schedule, day, 600).Doing == "sweeping" &&
                      RgResolve(schedule, day, 700).Doing == "cooking" && RgResolve(schedule, day, 900).Doing == "resting",
                  "Sunday: a slow start, then the household work, the midday meal and a rest");
        }
    }
    // Weekend to Monday: Sunday night is sleep, and Monday walks to work without a gap.
    Check(RgResolve(schedule, 6, 1439).Doing == "sleeping" && RgResolve(schedule, 7, 0).Doing == "sleeping", "Sunday night sleep runs into Monday");
    Check(RgResolve(schedule, 7, 470).Status == ScheduleStatus::Travel && RgResolve(schedule, 7, 480).Zone == RgWork, "Monday 08:00 at work");
}

/** The core's own visiting hours are read from the door table; the host is home, and awake for genuine stretches, on every day. */
void RgVisitingHours(const NpcSchedule& schedule)
{
    const DoorDef* door = FindDoor(RgDoorId);
    Check(door != nullptr && door->Access == DoorAccess::Private && door->LocationId == RgHome, "the row house's front door is private and is her home");
    Check(door->VisitingWindows.size() == 1 && door->VisitingWindows[0].OpenMinute == 960 && door->VisitingWindows[0].CloseMinute == 1170,
          "visiting hours are 16:00-19:30");
    const DoorOpeningWindow window = door->VisitingWindows[0];
    int answered = 0;
    int away = 0;
    for (int day = 0; day <= 8; ++day)
    {
        const int weekday = day % 7;
        const bool workDay = weekday <= 5;
        const bool monFri = weekday <= 4;
        int awakeMinutes = 0;
        int longestAwake = 0;
        int run = 0;
        for (int minute = window.OpenMinute; minute < window.CloseMinute; ++minute)
        {
            const auto resolved = RgResolve(schedule, day, minute);
            const auto intent = RgIntent(schedule, day, minute);
            // The same rule the lane applies (PerfectWorldLaneLifeKnock.cpp IsHostHome): at the home's location, idle or at work.
            const bool hostHome = RgAtHome(resolved) && intent.Ok() && (intent.Intent.State == NpcState::Idle || intent.Intent.State == NpcState::Work);
            const bool talkable = hostHome && intent.Intent.Availability == PersonAvailability::Awake && intent.Intent.bTalkable;
            DoorUseContext context;
            context.IsHostHome = hostHome;
            const DoorKnockResult knock = EvaluateDoorKnock(*door, minute, context);
            if (monFri && minute < 971)
            {
                Check(!hostHome && resolved.Status == ScheduleStatus::Travel, "Monday to Friday 16:00-16:10: still walking home, so nobody is home");
                Check(knock.Answer == DoorKnockAnswer::NoAnswer, "nobody answers while she is on the road");
                ++away;
                run = 0;
                continue;
            }
            Check(hostHome, "she is home in every other minute of the visiting hours");
            Check(knock.Answer == DoorKnockAnswer::Admitted, "a knock in visiting hours at home is answered");
            ++answered;
            if (talkable)
            {
                ++awakeMinutes;
                longestAwake = std::max(longestAwake, ++run);
            }
            else
            {
                run = 0;
                Check(resolved.Doing == "cooking", "the only home minutes she cannot talk are the cooking hour");
            }
        }
        Check(awakeMinutes >= 139, "at least 139 awake and talkable minutes inside the visiting hours every day");
        Check(longestAwake >= 90, "one unbroken awake stretch of at least 90 minutes (18:00-19:30) every day");
        // Outside visiting hours a host who is home does not receive, and a host who is away does not answer.
        DoorUseContext home;
        home.IsHostHome = true;
        Check(EvaluateDoorKnock(*door, 900, home).Answer == DoorKnockAnswer::NotReceiving && EvaluateDoorKnock(*door, 1170, home).Answer == DoorKnockAnswer::NotReceiving,
              "outside the visiting hours nobody receives");
        Check(EvaluateDoorKnock(*door, 1170, home).MinutesUntilVisiting == 1230, "the next visiting window opens at 16:00 tomorrow");
        DoorUseContext gone;
        gone.IsHostHome = false;
        Check(EvaluateDoorKnock(*door, 1000, gone).Answer == DoorKnockAnswer::NoAnswer, "away from home nobody answers");
        // Her home during the working hours: she is away every Monday to Saturday morning, and the door is not receiving then anyway.
        if (workDay)
            Check(!RgAtHome(RgResolve(schedule, day, 600)) && !RgAtHome(RgResolve(schedule, day, 750)), "at work in the middle of the working day");
    }
    Check(answered == 7 * (210 - 11) + 2 * 210, "9 days of visiting minutes were all checked: 1813 answered");
    Check(away == 7 * 11, "and the 77 minutes of the walk home found nobody");
}

void RgAdvance(const NpcSchedule& schedule)
{
    auto whole = StartSchedulePerson(RgPerson, RgAt(0, 0));
    auto split = whole;
    const auto end = RgAt(8, 0);
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
    int toWork = 0;
    int toHome = 0;
    for (const auto& event : full.Events)
    {
        if (event.Kind != ScheduleEventKind::TravelBegan)
            continue;
        if (event.FromZone == RgHome && event.Zone == RgWork)
            ++toWork;
        if (event.FromZone == RgWork && event.Zone == RgHome)
            ++toHome;
    }
    Check(toWork == 7 && toHome == 7, "seven walks to work and seven home from Monday to the following Monday (Monday to Saturday, then Monday)");
}

void RgRoutes(const NpcSchedule& schedule)
{
    Check(schedule.Routes.size() == 1 && schedule.Routes[0].LengthCm == 1540 && schedule.Routes[0].Id == "route.rupa_work.home_to_work", "one approved route of 1540 cm");
    Check(schedule.Paths.size() == 1, "the single leg between the two zones is explicit");
    const auto& path = schedule.Paths.front();
    Check(path.From == RgWork && path.To == RgHome && path.Routes.size() == 1 && path.Routes[0] == "route.rupa_work.home_to_work", "the leg uses the route");
    Check(path.Minutes == (1540 + 139) / 140 && path.Minutes == 11, "the leg is the integer ceiling of 1540 cm at 140 cm per minute: 11 minutes");
}
} // namespace

int main(int argc, char** argv)
{
    Check(argc == 2, "content file argument supplied");
    std::ifstream input(argv[1]);
    Check(input.good(), "Rupa authored content exists");
    const std::string text{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const auto parsed = ParseJson(text);
    Check(parsed.Value.has_value(), "the JSON file parses");
    const auto db = RgDatabase(*parsed.Value);
    const auto loaded = LoadNpcSchedule(db, RgScheduleId);
    if (!loaded.Ok())
        std::cerr << loaded.Error << '\n';
    Check(loaded.Ok(), "typed Rupa content loads and validates");
    const auto again = LoadNpcSchedule(db, RgScheduleId);
    Check(again.Ok() && again.Value->Person == loaded.Value->Person && again.Value->Travel == loaded.Value->Travel, "typed loading is deterministic");
    Check(loaded.Value->Person.PersonId == RgPerson && loaded.Value->Person.HomeZone == RgHome && ValidateNpcScheduleContent(*loaded.Value).empty(), "the decoded schedule revalidates");
    std::vector<ContentIssue> issues;
    ContentSchema::ValidateNpcScheduleSemantics(db, issues);
    Check(issues.empty(), "exported semantic validator accepts the content");
    const auto registered = LoadContent({{"Data/Content/npc_schedule/npc_schedule.rupa_ghosh.json", text}});
    for (const auto& issue : registered.Issues)
        std::cerr << issue.File << ' ' << issue.Id << ": " << issue.Message << '\n';
    Check(registered.Ok() && LoadNpcSchedule(registered.Db, RgScheduleId).Ok(), "she loads through the registered schema");
    Check(!LoadNpcSchedule(registered.Db, CareWorkerScheduleId).Ok() && !LoadNpcSchedule(registered.Db, "npc_schedule.subrata_dey").Ok(), "the others are not supplied by her file");

    RgMalformed(*parsed.Value);
    RgWeek(*loaded.Value);
    RgVisitingHours(*loaded.Value);
    RgAdvance(*loaded.Value);
    RgRoutes(*loaded.Value);
    std::cout << "PerfectWorldRupaLifeTests passed (" << RgChecks << " checks)\n";
    return 0;
}
