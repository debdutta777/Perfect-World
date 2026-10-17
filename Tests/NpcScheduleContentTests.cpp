#include "NpcScheduleContent.h"

#include "ContentSchemas.h"
#include "NpcBehaviour.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

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
Weekday DayOf(int day)
{
    return GameClock::ToCalendar(At(day, 720)).DayOfWeek;
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
    const std::string id(CareWorkerScheduleId);
    db.Records.emplace(id, ContentRecord{"npc_schedule", id, "care_worker.json", root, {}});
    return db;
}
void Refuse(const JsonValue& root, const char* message)
{
    const auto db = Database(root);
    Require(!LoadNpcSchedule(db, CareWorkerScheduleId).Ok(), message);
    std::vector<ContentIssue> issues;
    ContentSchema::ValidateNpcScheduleSemantics(db, issues);
    Require(!issues.empty(), "semantic entry point reports the rejected record");
}

void Malformed(const JsonValue& root)
{
    auto bad = root;
    Field(bad, "person").String = "person.care_worker_two";
    Refuse(bad, "unfrozen identity refused");
    bad = root;
    Field(bad, "home").String = "location.nabapara.other_home";
    Refuse(bad, "wrong home refused");
    bad = root;
    Field(bad, "workplace").String = "location.nabapara.other";
    Refuse(bad, "wrong workplace refused");
    bad = root;
    Field(bad, "door").String = "door.nabapara.other";
    Refuse(bad, "wrong door refused");
    bad = root;
    Field(bad, "speedCmPerMinute").Integer = 141;
    Refuse(bad, "wrong travel speed refused");
    bad = root;
    bad.Object.push_back({"unexpected", JsonValue{}});
    Refuse(bad, "unknown field refused");
    bad = root;
    Field(Field(bad, "routine").Array.front(), "role").String = "unknown_role";
    Refuse(bad, "unknown role refused");
    bad = root;
    Field(Field(bad, "routine").Array[2], "start").Integer = 400;
    Refuse(bad, "duty before seven refused");
    bad = root;
    Field(Field(bad, "routine").Array[9], "end").Integer = 910;
    Refuse(bad, "duty past fifteen hundred refused");
    bad = root;
    Field(Field(bad, "routine").Array[2], "start").Integer = 419;
    Refuse(bad, "squeezed arrival refused");
    bad = root;
    Field(Field(bad, "routine").Array[3], "days").Integer = 0;
    Refuse(bad, "empty weekday mask refused");
    bad = root;
    Field(bad, "routine").Array.erase(Field(bad, "routine").Array.begin() + 6);
    Refuse(bad, "unexplained idle gap on weekdays refused");
    bad = root;
    for (auto& entry : Field(bad, "routine").Array)
        if (Field(entry, "days").Integer == 65)
            Field(entry, "days").Integer = 64;
    Refuse(bad, "missing Sunday coverage refused");
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
    Field(Field(bad, "routes").Array.front(), "lengthCm").Integer = 1;
    Refuse(bad, "undersized frozen geometry refused");
    bad = root;
    Field(bad, "routes").Array.back() = Field(bad, "routes").Array.front();
    Refuse(bad, "duplicate approved route refused");
    bad = root;
    Field(bad, "routes").Array.pop_back();
    Refuse(bad, "missing approved route refused");
    Require(!LoadNpcSchedule(ContentDb{}, CareWorkerScheduleId).Ok(), "missing content refused");
    Require(!LoadNpcSchedule(Database(root), "npc_schedule.nobody").Ok(), "unknown schedule identity refused");
}

bool AtWork(const ScheduleResolution& resolved)
{
    return resolved.Status == ScheduleStatus::Stay && resolved.Zone.starts_with("anchor.care_home.");
}

void Week(const NpcSchedule& schedule)
{
    bool sawWeekday = false;
    bool sawSaturday = false;
    bool sawSunday = false;
    for (int day = 1; day <= 8; ++day)
    {
        const auto weekday = DayOf(day);
        sawWeekday = sawWeekday || (weekday != Weekday::Saturday && weekday != Weekday::Sunday);
        sawSaturday = sawSaturday || weekday == Weekday::Saturday;
        sawSunday = sawSunday || weekday == Weekday::Sunday;
        for (int minute = 0; minute < 1440; ++minute)
        {
            const auto resolved = ResolveSchedule(schedule.Person, schedule.Travel, At(day, minute));
            Require(resolved.Valid && !resolved.Squeezed && resolved.Status != ScheduleStatus::Idle,
                    "every minute, including the weekly rollover, has an explicit stay or travel");
            const auto intent = ResolveNpcIntentAt(schedule.Person, schedule.Travel, At(day, minute), false, false);
            Require(intent.Ok(), "every authored role maps to an NPC intent");
            // At the care home only inside the duty window 07:00 to 15:00.
            if (AtWork(resolved))
                Require(minute >= 420 && minute < 900, "at the care home only between 07:00 and 15:00");
            if (minute < 360 || minute >= 1320)
            {
                Require(resolved.Status == ScheduleStatus::Stay && resolved.Zone == CareWorkerHomeZone && resolved.Doing == "sleeping",
                        "asleep at her own home through the night");
                Require(intent.Intent.Availability == PersonAvailability::Asleep, "asleep means unavailable");
            }
            if (minute >= 928 && minute < 1320)
                Require(resolved.Status == ScheduleStatus::Stay && resolved.Zone == CareWorkerHomeZone,
                        "off duty and at home from the evening");
            if (minute >= 420 && minute < 900 && resolved.Status == ScheduleStatus::Stay)
                Require(AtWork(resolved), "on duty she is at a care home anchor");
        }
        // Handover: the last duty stay is at the staff desk and ends exactly at 15:00.
        const auto handover = ResolveSchedule(schedule.Person, schedule.Travel, At(day, 899));
        Require(AtWork(handover) && handover.Zone == "anchor.care_home.staff_desk" && handover.Doing == "caring" &&
                    handover.SegmentEnd == At(day, 900),
                "the 15:00 handover is made at the staff desk");
        const auto after = ResolveSchedule(schedule.Person, schedule.Travel, At(day, 900));
        Require(after.Status == ScheduleStatus::Travel && after.Zone == CareWorkerHomeZone,
                "from 15:00 she walks home, off the lane");
        for (const auto minute : {430, 700, 880})
        {
            const auto intent = ResolveNpcIntentAt(schedule.Person, schedule.Travel, At(day, minute), false, false);
            Require(intent.Ok() && intent.Intent.State == NpcState::Work && intent.Intent.WorkActionId == "work.care",
                    "morning and afternoon duty is the caring role");
        }
        const auto seven = ResolveSchedule(schedule.Person, schedule.Travel, At(day, 420));
        Require(AtWork(seven) && seven.Zone == "anchor.care_home.common_room" && seven.Doing == "caring",
                "she is in the common room, caring, at 07:00 sharp");
    }
    Require(sawWeekday && sawSaturday && sawSunday, "the eight days cover weekdays and the weekend");
}

void Variation(const NpcSchedule& schedule)
{
    bool weekdayDifferent = false;
    for (int minute = 420; minute < 900; ++minute)
    {
        // A Monday against a Saturday: find one of each in the eight days.
        int monday = 0;
        int saturday = 0;
        for (int day = 1; day <= 8; ++day)
        {
            monday = DayOf(day) == Weekday::Monday ? day : monday;
            saturday = DayOf(day) == Weekday::Saturday ? day : saturday;
        }
        Require(monday != 0 && saturday != 0, "a Monday and a Saturday are in range");
        const auto a = ResolveSchedule(schedule.Person, schedule.Travel, At(monday, minute));
        const auto b = ResolveSchedule(schedule.Person, schedule.Travel, At(saturday, minute));
        weekdayDifferent = weekdayDifferent || a.Zone != b.Zone || a.Doing != b.Doing;
    }
    Require(weekdayDifferent, "the weekend routine differs from a weekday within the same duty hours");
}

void Advance(const NpcSchedule& schedule)
{
    auto whole = StartSchedulePerson(CareWorkerPersonId, At(1, 0));
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
}
} // namespace

int main(int argc, char** argv)
{
    Require(argc == 2, "content file argument supplied");
    std::ifstream input(argv[1]);
    Require(input.good(), "care worker authored content exists");
    const std::string text{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const auto parsed = ParseJson(text);
    Require(parsed.Value.has_value(), "care worker JSON parses");
    const auto db = Database(*parsed.Value);
    const auto loaded = LoadNpcSchedule(db, CareWorkerScheduleId);
    if (!loaded.Ok())
        std::cerr << loaded.Error << '\n';
    Require(loaded.Ok(), "typed care worker content loads and validates");
    const auto again = LoadNpcSchedule(db, CareWorkerScheduleId);
    Require(again.Ok() && again.Value->Person == loaded.Value->Person && again.Value->Travel == loaded.Value->Travel,
            "typed loading is deterministic");
    Require(loaded.Value->Person.PersonId == CareWorkerPersonId && ValidateNpcScheduleContent(*loaded.Value).empty(),
            "the decoded schedule revalidates");
    std::vector<ContentIssue> issues;
    ContentSchema::ValidateNpcScheduleSemantics(db, issues);
    Require(issues.empty(), "exported semantic validator accepts the care worker content");
    const auto registered = LoadContent({{"Data/Content/npc_schedule/npc_schedule.care_worker.json", text}});
    for (const auto& issue : registered.Issues)
        std::cerr << issue.File << ' ' << issue.Id << ": " << issue.Message << '\n';
    Require(registered.Ok(), "the care worker loads through the registered schema");
    Require(LoadNpcSchedule(registered.Db, CareWorkerScheduleId).Ok(), "registered content supplies the typed schedule");
    Require(!LoadNpcSchedule(registered.Db, FaridaScheduleId).Ok(), "Farida is not supplied by the care worker file");
    Malformed(*parsed.Value);
    Week(*loaded.Value);
    Variation(*loaded.Value);
    Advance(*loaded.Value);
    std::cout << "PASS: " << Assertions << " assertions\n";
}
