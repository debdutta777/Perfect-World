// Mira Sen's authored schedule (LANE_LIFE_PLAN cast table; review step 4): loaded by the generic npc_schedule loader and checked
// against her week: morning study at home, school 10:00-15:50 on weekdays, Tuesday and Thursday 16:30-18:00 at the stationery
// packing desk, Monday/Wednesday/Friday afternoons on her own plans, a lighter weekend, asleep from 22:30. Her anchor never
// coincides with Anindita's. The care worker, Farida and Anindita are covered by their own tests.
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
int MrChecks = 0;
void MrRequire(bool condition, const char* message)
{
    ++MrChecks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

constexpr std::string_view MrScheduleId = "npc_schedule.mira_sen";
constexpr std::string_view MrPersonId = "person.mira_sen";
constexpr std::string_view MrAninditaScheduleId = "npc_schedule.anindita_sen";
constexpr std::string_view MrHomeZone = "location.nabapara.sen_home";
constexpr std::string_view MrSchoolZone = "location.nabapara.school";
constexpr std::string_view MrOutZone = "location.nabapara.mira_out";
constexpr std::string_view MrDeskZone = "anchor.stationery.packing_desk";
constexpr std::string_view MrCounterZone = "anchor.stationery.counter";

// Weekday masks (Sunday is bit 0): Monday..Friday, Tuesday and Thursday, Monday/Wednesday/Friday, Saturday, Sunday.
constexpr int MrTueThu = 20;
constexpr int MrMonWedFri = 42;

/** Day 0 is a Monday: instant 0 is Monday 06:00, and the first week is skipped so that no instant is negative. */
GameInstant MrAt(int day, int minute)
{
    return static_cast<GameInstant>(day + 7) * MinutesPerDay + minute - 360;
}

JsonValue& MrField(JsonValue& object, std::string_view name)
{
    for (auto& [key, value] : object.Object)
        if (key == name)
            return value;
    std::exit(2);
}

/** The routine entry with this zone and weekday mask. */
JsonValue& MrEntry(JsonValue& root, std::string_view zone, int days)
{
    for (auto& item : MrField(root, "routine").Array)
        if (MrField(item, "zone").String == zone && *MrField(item, "days").Integer == days)
            return item;
    std::exit(3);
}

ContentDb MrDatabase(const JsonValue& root)
{
    ContentDb db;
    const std::string id(MrScheduleId);
    db.Records.emplace(id, ContentRecord{"npc_schedule", id, "mira_sen.json", root, {}});
    return db;
}

void MrRefuse(const JsonValue& root, const char* message)
{
    const auto db = MrDatabase(root);
    MrRequire(!LoadNpcSchedule(db, MrScheduleId).Ok(), message);
    std::vector<ContentIssue> issues;
    ContentSchema::ValidateNpcScheduleSemantics(db, issues);
    MrRequire(!issues.empty(), "semantic entry point reports the rejected record");
}

void MrMalformed(const JsonValue& root)
{
    auto bad = root;
    MrField(bad, "person").String = "person.mira_sen_two";
    MrRefuse(bad, "unfrozen identity refused");
    bad = root;
    MrField(bad, "home").String = "location.nabapara.other_home";
    MrRefuse(bad, "wrong home refused");
    bad = root;
    MrField(bad, "workplace").String = "location.nabapara.other";
    MrRefuse(bad, "wrong workplace refused");
    bad = root;
    MrField(bad, "door").String = "door.nabapara.other";
    MrRefuse(bad, "wrong door refused");
    bad = root;
    MrField(bad, "speedCmPerMinute").Integer = 141;
    MrRefuse(bad, "wrong travel speed refused");
    bad = root;
    bad.Object.push_back({"unexpected", JsonValue{}});
    MrRefuse(bad, "unknown field refused");
    bad = root;
    MrField(MrField(bad, "routine").Array.front(), "role").String = "unknown_role";
    MrRefuse(bad, "unknown role refused");
    bad = root;
    MrField(MrEntry(bad, MrDeskZone, MrTueThu), "start").Integer = 980;
    MrRefuse(bad, "desk work before 16:30 refused");
    bad = root;
    MrField(MrEntry(bad, MrDeskZone, MrTueThu), "end").Integer = 1090;
    MrRefuse(bad, "desk work past 18:00 refused");
    bad = root;
    MrField(MrEntry(bad, MrDeskZone, MrTueThu), "start").Integer = 989;
    MrRefuse(bad, "squeezed arrival at the desk refused");
    bad = root;
    MrField(MrEntry(bad, MrSchoolZone, 62), "start").Integer = 580;
    MrRefuse(bad, "squeezed arrival at school refused");
    bad = root;
    MrField(MrEntry(bad, MrSchoolZone, 62), "days").Integer = 0;
    MrRefuse(bad, "empty weekday mask refused");
    bad = root;
    MrField(bad, "routine").Array.erase(MrField(bad, "routine").Array.begin() + (&MrEntry(bad, MrOutZone, MrMonWedFri) - &MrField(bad, "routine").Array.front()));
    MrRefuse(bad, "unexplained idle gap on Monday, Wednesday and Friday refused");
    bad = root;
    MrField(MrEntry(bad, MrOutZone, MrMonWedFri), "zone").String = std::string(MrCounterZone);
    MrRefuse(bad, "a Monday, Wednesday or Friday afternoon at the shop refused");
    bad = root;
    MrField(MrEntry(bad, MrDeskZone, MrTueThu), "zone").String = "anchor.stationery.stock_room";
    MrRefuse(bad, "unapproved zone refused");
    bad = root;
    MrField(bad, "travel").Array.pop_back();
    MrRefuse(bad, "missing explicit leg refused");
    bad = root;
    MrField(bad, "travel").Array.erase(MrField(bad, "travel").Array.begin() + 6);
    MrRefuse(bad, "a missing middle leg refused");
    bad = root;
    MrField(MrField(bad, "travel").Array.front(), "minutes").Integer = 1;
    MrRefuse(bad, "short travel duration refused");
    bad = root;
    MrField(MrField(bad, "travel").Array.front(), "routes").Array.front().String = "route.unknown";
    MrRefuse(bad, "unknown route refused");
    bad = root;
    MrField(MrField(bad, "routes").Array.front(), "lengthCm").Integer = 1;
    MrRefuse(bad, "undersized frozen geometry refused");
    bad = root;
    MrField(bad, "routes").Array.back() = MrField(bad, "routes").Array.front();
    MrRefuse(bad, "duplicate approved route refused");
    bad = root;
    MrField(bad, "routes").Array.pop_back();
    MrRefuse(bad, "missing approved route refused");
    MrRequire(!LoadNpcSchedule(ContentDb{}, MrScheduleId).Ok(), "missing content refused");
    MrRequire(!LoadNpcSchedule(MrDatabase(root), "npc_schedule.nobody").Ok(), "unknown schedule identity refused");
}

ScheduleResolution MrAtMinute(const NpcSchedule& schedule, int day, int minute)
{
    return ResolveSchedule(schedule.Person, schedule.Travel, MrAt(day, minute));
}

bool MrAtShop(const ScheduleResolution& resolved)
{
    return resolved.Status == ScheduleStatus::Stay && resolved.Zone.starts_with("anchor.stationery.");
}

/** Day 0..8: Monday, Tuesday, ..., Sunday, Monday, Tuesday. */
void MrWeek(const NpcSchedule& schedule)
{
    for (int day = 0; day <= 8; ++day)
    {
        const int weekday = day % 7; // 0 Monday .. 6 Sunday
        const bool school = weekday <= 4;
        const bool tueThu = weekday == 1 || weekday == 3;
        const bool monWedFri = weekday == 0 || weekday == 2 || weekday == 4;
        const bool saturday = weekday == 5;
        const bool sunday = weekday == 6;
        for (int minute = 0; minute < 1440; ++minute)
        {
            const auto resolved = MrAtMinute(schedule, day, minute);
            MrRequire(resolved.Valid && !resolved.Squeezed && resolved.Status != ScheduleStatus::Idle,
                      "every minute, including the weekly rollover, has an explicit stay or travel");
            const auto intent = ResolveNpcIntentAt(schedule.Person, schedule.Travel, MrAt(day, minute), false, false);
            MrRequire(intent.Ok(), "every authored role maps to an NPC intent");
            if (MrAtShop(resolved))
                MrRequire(tueThu && minute >= 990 && minute < 1080 && resolved.Zone == MrDeskZone, "at the shop only at the desk, Tuesday and Thursday, 16:30 to 18:00");
            if (resolved.Status == ScheduleStatus::Stay && resolved.Zone == MrSchoolZone)
                MrRequire(school && minute >= 590 && minute < 950, "at school only on weekdays between 09:50 and 15:50");
            if (resolved.Status == ScheduleStatus::Stay && resolved.Zone == MrOutZone)
                MrRequire((monWedFri && minute >= 968 && minute < 1080) || (saturday && minute >= 660 && minute < 960), "out on her own plans only in the authored afternoons");
            if (minute < 375 || minute >= 1350)
            {
                MrRequire(resolved.Status == ScheduleStatus::Stay && resolved.Zone == MrHomeZone && resolved.Doing == "sleeping",
                          "asleep at home from 22:30 until 06:15, every day");
                MrRequire(intent.Intent.Availability == PersonAvailability::Asleep, "asleep means unavailable");
            }
            if (school && minute >= 420 && minute < 570)
                MrRequire(resolved.Status == ScheduleStatus::Stay && resolved.Zone == MrHomeZone && resolved.Doing == "studying", "morning study at home on a school day");
            if (minute >= 590 && minute < 950)
            {
                if (school)
                    MrRequire(resolved.Status == ScheduleStatus::Stay && resolved.Zone == MrSchoolZone && resolved.Doing == "studying", "in class from 09:50 to the 15:50 bell");
                else
                    MrRequire(resolved.Zone != MrSchoolZone, "never at school at the weekend");
            }
            if (minute >= 570 && minute < 590 && school)
                MrRequire(resolved.Status == ScheduleStatus::Travel && resolved.Zone == MrSchoolZone && resolved.FromZone == MrHomeZone, "twenty minutes walking to school");
        }
        if (school)
        {
            const auto assembly = MrAtMinute(schedule, day, 600);
            MrRequire(assembly.Status == ScheduleStatus::Stay && assembly.Zone == MrSchoolZone, "at school for the 10:00 assembly");
            const auto last = MrAtMinute(schedule, day, 949);
            MrRequire(last.Zone == MrSchoolZone && last.SegmentEnd == MrAt(day, 950), "held at school until the 15:50 bell");
            MrRequire(MrAtMinute(schedule, day, 950).Status == ScheduleStatus::Travel, "from 15:50 she is walking");
        }
        if (tueThu)
        {
            const auto walking = MrAtMinute(schedule, day, 989);
            MrRequire(walking.Status == ScheduleStatus::Travel && walking.Zone == MrDeskZone && walking.FromZone == MrSchoolZone, "forty minutes from school to the desk, still walking at 16:29");
            const auto arrive = MrAtMinute(schedule, day, 990);
            MrRequire(MrAtShop(arrive) && arrive.Zone == MrDeskZone && arrive.Doing == "serving", "16:30 sharp: at the packing desk");
            const auto last = MrAtMinute(schedule, day, 1079);
            MrRequire(MrAtShop(last) && last.SegmentEnd == MrAt(day, 1080), "the desk is held until 18:00");
            MrRequire(MrAtMinute(schedule, day, 1080).Status == ScheduleStatus::Travel, "from 18:00 she walks home");
            MrRequire(MrAtMinute(schedule, day, 1107).Status == ScheduleStatus::Travel, "twenty-eight minutes home from the desk");
            const auto home = MrAtMinute(schedule, day, 1108);
            MrRequire(home.Status == ScheduleStatus::Stay && home.Zone == MrHomeZone && home.Doing == "at_home", "home by 18:28");
            const auto intent = ResolveNpcIntentAt(schedule.Person, schedule.Travel, MrAt(day, 1020), false, false);
            MrRequire(intent.Ok() && intent.Intent.State == NpcState::Work && intent.Intent.WorkActionId == "work.serve" &&
                          intent.Intent.Availability == PersonAvailability::Awake && intent.Intent.bTalkable,
                      "at the desk she works and may be spoken to");
        }
        if (monWedFri)
        {
            const auto out = MrAtMinute(schedule, day, 968);
            MrRequire(out.Status == ScheduleStatus::Stay && out.Zone == MrOutZone && out.Doing == "resting", "her own afternoon begins 18 minutes after the bell");
            MrRequire(MrAtMinute(schedule, day, 967).Status == ScheduleStatus::Travel, "eighteen minutes from school to her own plans");
            MrRequire(MrAtMinute(schedule, day, 1080).Status == ScheduleStatus::Travel && MrAtMinute(schedule, day, 1090).Status == ScheduleStatus::Travel,
                      "eleven minutes home from 18:00");
            const auto home = MrAtMinute(schedule, day, 1091);
            MrRequire(home.Status == ScheduleStatus::Stay && home.Zone == MrHomeZone, "home by 18:11");
            for (int minute = 950; minute < 1200; ++minute)
                MrRequire(!MrAtShop(MrAtMinute(schedule, day, minute)), "never at the shop on a Monday, Wednesday or Friday");
        }
        if (saturday)
        {
            MrRequire(MrAtMinute(schedule, day, 500).Doing == "studying" && MrAtMinute(schedule, day, 500).Zone == MrHomeZone, "Saturday starts with light study at home");
            const auto out = MrAtMinute(schedule, day, 700);
            MrRequire(out.Status == ScheduleStatus::Stay && out.Zone == MrOutZone, "Saturday midday is her own plans");
            MrRequire(MrAtMinute(schedule, day, 971).Zone == MrHomeZone && MrAtMinute(schedule, day, 971).Status == ScheduleStatus::Stay, "Saturday afternoon she is home by 16:11");
            for (int minute = 0; minute < 1440; ++minute)
                MrRequire(!MrAtShop(MrAtMinute(schedule, day, minute)), "never at the shop on a Saturday");
        }
        if (sunday)
        {
            for (int minute = 375; minute < 1350; ++minute)
            {
                const auto resolved = MrAtMinute(schedule, day, minute);
                MrRequire(resolved.Status == ScheduleStatus::Stay && resolved.Zone == MrHomeZone, "Sunday is spent at home");
            }
            MrRequire(MrAtMinute(schedule, day, 800).Doing == "reading", "Sunday afternoon reading");
        }
        const auto evening = MrAtMinute(schedule, day, 1250);
        MrRequire(evening.Status == ScheduleStatus::Stay && evening.Zone == MrHomeZone && evening.Doing == "studying", "evening study at home");
        const auto bed = MrAtMinute(schedule, day, 1350);
        MrRequire(bed.Zone == MrHomeZone && bed.Doing == "sleeping", "sleep from 22:30");
    }
    // Weekend to Monday: Sunday night is sleep, and Monday begins the school routine without a gap.
    MrRequire(MrAtMinute(schedule, 6, 1439).Doing == "sleeping" && MrAtMinute(schedule, 7, 0).Doing == "sleeping", "Sunday night sleep runs into Monday");
    MrRequire(MrAtMinute(schedule, 7, 380).Doing == "at_home", "Monday breakfast at home");
    MrRequire(MrAtMinute(schedule, 7, 600).Zone == MrSchoolZone, "Monday 10:00 at school");
}

void MrAdvance(const NpcSchedule& schedule)
{
    auto whole = StartSchedulePerson(MrPersonId, MrAt(0, 0));
    auto split = whole;
    const auto end = MrAt(8, 0);
    const auto full = AdvanceSchedulePerson(schedule.Person, schedule.Travel, whole, end);
    MrRequire(full.Ok, "whole week advance succeeds");
    std::vector<ScheduleEvent> events;
    while (split.At < end)
    {
        const auto part = AdvanceSchedulePerson(schedule.Person, schedule.Travel, split, std::min(end, split.At + 37));
        MrRequire(part.Ok, "split advance succeeds");
        events.insert(events.end(), part.Events.begin(), part.Events.end());
    }
    MrRequire(whole == split && full.Events == events, "split and whole advances produce identical state and events");
    // Tuesday (day 1) and Thursday (day 3) each begin the desk stay at 16:30; no other day does.
    int deskStays = 0;
    for (const auto& event : full.Events)
        if (event.Kind == ScheduleEventKind::StayBegan && event.Zone == MrDeskZone)
        {
            ++deskStays;
            const GameInstant dayIndex = (event.At + 360) / MinutesPerDay - 7;
            MrRequire((dayIndex == 1 || dayIndex == 3) && (event.At + 360) % MinutesPerDay == 990, "a desk stay begins at 16:30 on a Tuesday or Thursday");
        }
    MrRequire(deskStays == 2, "exactly two desk stays in the first seven days");
}

void MrBuiltRoutes(const NpcSchedule& schedule, const NpcSchedule& anindita)
{
    const std::vector<int> lengths{727, 1100, 2000, 2800, 3700, 1500, 2400};
    MrRequire(schedule.Routes.size() == lengths.size(), "seven approved routes");
    for (std::size_t i = 0; i < lengths.size(); ++i)
        MrRequire(schedule.Routes[i].LengthCm == lengths[i], "authored geometry matches the plan");
    // The three shop legs are the very polylines of Anindita's file.
    for (const auto& id : {"route.stationery.counter_to_packing_desk", "route.stationery.counter_to_door", "route.sen_home.footpath_to_home"})
    {
        const auto mine = std::find_if(schedule.Routes.begin(), schedule.Routes.end(), [&](const auto& r) { return r.Id == id; });
        const auto hers = std::find_if(anindita.Routes.begin(), anindita.Routes.end(), [&](const auto& r) { return r.Id == id; });
        MrRequire(mine != schedule.Routes.end() && hers != anindita.Routes.end() && mine->LengthCm == hers->LengthCm && mine->From == hers->From &&
                      mine->To == hers->To,
                  "the shared shop route is identical in both files");
    }
    MrRequire(schedule.Paths.size() == 15, "all fifteen undirected legs between six zones are explicit");
    for (std::size_t i = 0; i < schedule.Paths.size(); ++i)
    {
        int length = 0;
        for (const auto& id : schedule.Paths[i].Routes)
        {
            const auto route = std::find_if(schedule.Routes.begin(), schedule.Routes.end(), [&](const auto& item) { return item.Id == id; });
            MrRequire(route != schedule.Routes.end(), "path uses authored geometry");
            length += route->LengthCm;
        }
        MrRequire(schedule.Paths[i].Minutes == (length + 139) / 140, "every leg is the integer ceiling of its total length at 140 cm per minute");
    }
    const auto leg = [&](std::string_view from, std::string_view to) {
        for (const auto& item : schedule.Paths)
            if ((item.From == from && item.To == to) || (item.From == to && item.To == from))
                return item.Minutes;
        return -1;
    };
    MrRequire(leg(MrHomeZone, MrSchoolZone) == 20, "home to school is twenty minutes");
    MrRequire(leg(MrSchoolZone, MrDeskZone) == 40, "school to the packing desk is forty minutes");
    MrRequire(leg(MrDeskZone, MrHomeZone) == 28, "the desk to home matches Anindita's twenty-eight");
    MrRequire(leg(MrSchoolZone, MrOutZone) == 18 && leg(MrOutZone, MrHomeZone) == 11, "own-plans legs are 18 and 11 minutes");
    MrRequire(leg("anchor.stationery.counter", MrDeskZone) == 6 && leg("anchor.stationery.counter", "anchor.stationery.front_door") == 8 &&
                  leg("anchor.stationery.front_door", MrDeskZone) == 14,
              "the stationery legs match Anindita's 6, 8 and 14 minutes");
}

/** The two women never stand on the same anchor at the same minute. */
void MrCoexistence(const NpcSchedule& mira, const NpcSchedule& anindita)
{
    int together = 0;
    for (int day = 0; day <= 8; ++day)
    {
        for (int minute = 0; minute < 1440; ++minute)
        {
            const auto a = MrAtMinute(anindita, day, minute);
            const auto m = MrAtMinute(mira, day, minute);
            if (a.Status == ScheduleStatus::Stay && m.Status == ScheduleStatus::Stay && a.Zone.starts_with("anchor.") && a.Zone == m.Zone)
                MrRequire(false, "never standing on the same anchor");
            if (MrAtShop(a) && MrAtShop(m))
            {
                ++together;
                MrRequire(m.Zone == MrDeskZone && a.Zone != MrDeskZone, "when both are in the shop she is at the desk and her mother is not");
            }
        }
    }
    MrRequire(together > 0, "they do work in the shop together on Tuesday and Thursday afternoons");
}
} // namespace

int main(int argc, char** argv)
{
    MrRequire(argc == 3, "content file arguments supplied");
    std::ifstream input(argv[1]);
    MrRequire(input.good(), "Mira authored content exists");
    const std::string text{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    std::ifstream anInput(argv[2]);
    MrRequire(anInput.good(), "Anindita authored content exists");
    const std::string anText{std::istreambuf_iterator<char>(anInput), std::istreambuf_iterator<char>()};
    const auto parsed = ParseJson(text);
    const auto anParsed = ParseJson(anText);
    MrRequire(parsed.Value.has_value() && anParsed.Value.has_value(), "both JSON files parse");
    const auto db = MrDatabase(*parsed.Value);
    const auto loaded = LoadNpcSchedule(db, MrScheduleId);
    if (!loaded.Ok())
        std::cerr << loaded.Error << '\n';
    MrRequire(loaded.Ok(), "typed Mira content loads and validates");
    const auto again = LoadNpcSchedule(db, MrScheduleId);
    MrRequire(again.Ok() && again.Value->Person == loaded.Value->Person && again.Value->Travel == loaded.Value->Travel, "typed loading is deterministic");
    MrRequire(loaded.Value->Person.PersonId == MrPersonId && loaded.Value->Person.HomeZone == MrHomeZone &&
                  ValidateNpcScheduleContent(*loaded.Value).empty(),
              "the decoded schedule revalidates");
    std::vector<ContentIssue> issues;
    ContentSchema::ValidateNpcScheduleSemantics(db, issues);
    MrRequire(issues.empty(), "exported semantic validator accepts the content");
    const auto registered = LoadContent({{"Data/Content/npc_schedule/npc_schedule.mira_sen.json", text}});
    for (const auto& issue : registered.Issues)
        std::cerr << issue.File << ' ' << issue.Id << ": " << issue.Message << '\n';
    MrRequire(registered.Ok(), "Mira loads through the registered schema");
    MrRequire(LoadNpcSchedule(registered.Db, MrScheduleId).Ok(), "registered content supplies the typed schedule");
    MrRequire(!LoadNpcSchedule(registered.Db, CareWorkerScheduleId).Ok(), "the care worker is not supplied by Mira's file");
    MrRequire(!LoadNpcSchedule(registered.Db, FaridaScheduleId).Ok(), "Farida is not supplied by Mira's file");
    MrRequire(!LoadNpcSchedule(registered.Db, MrAninditaScheduleId).Ok(), "Anindita is not supplied by Mira's file");

    ContentDb anDb;
    const std::string anId(MrAninditaScheduleId);
    anDb.Records.emplace(anId, ContentRecord{"npc_schedule", anId, "anindita_sen.json", *anParsed.Value, {}});
    const auto anindita = LoadNpcSchedule(anDb, MrAninditaScheduleId);
    MrRequire(anindita.Ok(), "Anindita's schedule still loads beside Mira's");

    MrMalformed(*parsed.Value);
    MrWeek(*loaded.Value);
    MrAdvance(*loaded.Value);
    MrBuiltRoutes(*loaded.Value, *anindita.Value);
    MrCoexistence(*loaded.Value, *anindita.Value);
    std::cout << "PASS: " << MrChecks << " assertions\n";
}
