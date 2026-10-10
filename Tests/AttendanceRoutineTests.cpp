// ROADMAP M2-07: the bell schedule as M2-06 routine entries. One calendar for classmates and the player.
#include "AttendanceTestSupport.h"

using namespace PerfectWorld;

namespace
{
constexpr const char* School = "institution.nabapara_vidyalaya";
constexpr const char* Classmate = "p_0123456789abcdef";
constexpr const char* OtherClassmate = "p_fedcba9876543210";
constexpr const char* Player = "p_1111111111111111";
constexpr std::int32_t HomeTripMinutes = 30;

/** The school day plus home before and after school (lower priority) and home all weekend. */
PersonSchedule StudentWithHome(const BellSchedule& schedule, const char* id, std::int32_t homeUntil = 540)
{
    PersonSchedule person = BuildStudentSchedule(id, "home", schedule);
    person.Routine.push_back({AttendanceSchoolWeekdays, 0, homeUntil, "home", ScheduleDoingKind::Role, "at_home", 100});
    person.Routine.push_back({AttendanceSchoolWeekdays, 1000, 1440, "home", ScheduleDoingKind::Role, "at_home", 100});
    person.Routine.push_back({0x41, 0, 1440, "home", ScheduleDoingKind::Role, "at_home", 100});
    return person;
}

ScheduleTravelTable TravelWithHome()
{
    ScheduleTravelTable table = AttendanceSchoolTravel(RealSchedules());
    for (const BellSchedule& schedule : RealSchedules().Schedules)
    {
        for (const BellBlock& block : schedule.Blocks)
            Check(AttendanceAddTravelLeg(table, "home", block.Room, HomeTripMinutes), "a home leg is accepted");
    }
    return table;
}

std::string_view ExpectedDoing(const BellBlock& block)
{
    if (block.Kind == BellBlockKind::Period)
        return AttendanceClassActivityId;
    return block.Kind == BellBlockKind::Assembly ? AttendanceAssemblyRole : AttendanceBreakRole;
}

void TestRoutineEntries()
{
    for (const BellSchedule& schedule : RealSchedules().Schedules)
    {
        const std::vector<ScheduleEntry> routine = BuildSchoolRoutine(schedule);
        Check(routine.size() == schedule.Blocks.size(), "one routine entry per block");
        for (std::size_t i = 0; i < routine.size(); ++i)
        {
            const BellBlock& block = schedule.Blocks[i];
            Check(routine[i].StartMinute == block.StartMinute && routine[i].EndMinute == block.EndMinute && routine[i].Zone == block.Room &&
                      routine[i].Doing == ExpectedDoing(block) && routine[i].WeekdayMask == 0x3E && routine[i].Priority == 600,
                  "the entry carries the block's time, room and doing, Monday to Friday at the school priority");
            Check(routine[i].Kind == (block.Kind == BellBlockKind::Period ? ScheduleDoingKind::Activity : ScheduleDoingKind::Role),
                  "periods are activities, assembly and tiffin are roles");
        }
        AttendanceRoutineOptions options;
        options.Priority = 900;
        options.Weekdays = 0x02;
        const std::vector<ScheduleEntry> custom = BuildSchoolRoutine(schedule, options);
        Check(custom[0].Priority == 900 && custom[0].WeekdayMask == 0x02, "the options change priority and weekdays");
    }
}

void TestValidAndResolves()
{
    const ScheduleTravelTable travel = TravelWithHome();
    Check(ValidateScheduleTravel(travel).empty(), "the travel table is canonical");
    for (const BellSchedule& schedule : RealSchedules().Schedules)
    {
        const PersonSchedule person = StudentWithHome(schedule, Classmate);
        Check(ValidatePersonSchedule(person).empty(), "the student's schedule is valid");
        Check(ValidateScheduleTravelCoverage(person, travel).empty(), "every pair of zones has a leg");
        Check(ValidateScheduleRoutineTravel(person, travel).empty(), "the routine never squeezes a trip");

        std::int64_t stays = 0;
        for (std::int64_t day = 0; day < 7; ++day)
        {
            for (std::int32_t minute = 0; minute < MinutesPerDay; ++minute)
            {
                if (AttendanceAtMinute(day, minute) < 0)
                    continue; // before the campaign opens at 06:00 on day 0
                const ScheduleResolution r = ResolveSchedule(person, travel, AttendanceAtMinute(day, minute));
                Check(r.Valid, "the instant resolves");
                const BellBlock* block = day < 5 ? BellBlockAt(schedule, minute) : nullptr;
                if (block != nullptr)
                {
                    ++stays;
                    Check(r.Status == ScheduleStatus::Stay && r.Zone == block->Room && r.Doing == ExpectedDoing(*block) && !r.Squeezed,
                          "inside a block the student is in its room doing its activity");
                    Check(r.Kind == (block->Kind == BellBlockKind::Period ? ScheduleDoingKind::Activity : ScheduleDoingKind::Role),
                          "the resolved kind matches");
                }
                else if (day >= 5)
                    Check(r.Status == ScheduleStatus::Stay && r.Zone == "home", "the weekend is at home");
                else if (minute < 480 || minute > 1000)
                    Check(r.Zone == "home" || r.Status == ScheduleStatus::Travel, "outside school hours the student is home or on the way");
            }
        }
        Check(stays == 5 * (15 + 6 * 45 + 30), "five days of block minutes were resolved: assembly, six periods and tiffin");

        // A passing gap in a new room is a walk that arrives exactly on the next bell.
        const BellSchedule& s = schedule;
        const BellBlock& beforeTiffin = s.Blocks[3];
        const BellBlock& tiffin = s.Blocks[4];
        const ScheduleResolution walk = ResolveSchedule(person, travel, AttendanceAtMinute(0, tiffin.StartMinute - 2));
        Check(walk.Status == ScheduleStatus::Travel && walk.Zone == tiffin.Room && walk.SegmentEnd == AttendanceAtMinute(0, tiffin.StartMinute),
              "the walk to tiffin arrives on the bell");
        Check(beforeTiffin.Room != tiffin.Room, "tiffin is in another room");
    }
}

void TestSharedCalendar()
{
    const ScheduleTravelTable travel = TravelWithHome();
    const BellSchedule& sci = *RealSchedules().Find(School, "science");
    const BellSchedule& com = *RealSchedules().Find(School, "commerce");
    const PersonSchedule a = StudentWithHome(sci, Classmate);
    const PersonSchedule b = StudentWithHome(sci, OtherClassmate);
    const PersonSchedule c = StudentWithHome(com, Classmate);
    std::int64_t differs = 0;
    for (std::int32_t minute = 0; minute < MinutesPerDay; ++minute)
    {
        const ScheduleResolution ra = ResolveSchedule(a, travel, AttendanceAtMinute(2, minute));
        const ScheduleResolution rb = ResolveSchedule(b, travel, AttendanceAtMinute(2, minute));
        const ScheduleResolution rc = ResolveSchedule(c, travel, AttendanceAtMinute(2, minute));
        Check(ra.Status == rb.Status && ra.Zone == rb.Zone && ra.Doing == rb.Doing && ra.SegmentStart == rb.SegmentStart,
              "two science classmates share one calendar");
        differs += (ra.Zone != rc.Zone) ? 1 : 0;
    }
    Check(differs > 0, "a commerce student is in another room");
    Check(ResolveSchedule(a, travel, AttendanceAtMinute(2, 700)).Zone == "nhss.room_xi_science" &&
              ResolveSchedule(c, travel, AttendanceAtMinute(2, 700)).Zone == "nhss.room_xi_commerce",
          "mid-morning each student is in their own stream's room");
    Check(ResolveSchedule(a, travel, AttendanceAtMinute(2, 870)).Zone == "nhss.science_lab" &&
              ResolveSchedule(c, travel, AttendanceAtMinute(2, 870)).Zone == "nhss.room_xi_commerce",
          "in the lab period the science student is in the lab and the commerce student is not");

    // Unseen classmates still get their scheduled practice: a 5-day jump equals stepping boundary by boundary.
    const GameInstant end = AttendanceAtMinute(5, 0);
    SchedulePersonState jumped = StartSchedulePerson(Classmate, 0);
    Check(AdvanceSchedulePerson(a, travel, jumped, end).Ok, "the jump is accepted");
    SchedulePersonState stepped = StartSchedulePerson(Classmate, 0);
    while (const std::optional<GameInstant> next = NextScheduleBoundary(a, travel, stepped.At))
    {
        if (*next > end)
            break;
        Check(AdvanceSchedulePerson(a, travel, stepped, *next).Ok, "a boundary step is accepted");
    }
    Check(AdvanceSchedulePerson(a, travel, stepped, end).Ok, "the last step is accepted");
    Check(jumped == stepped && HashSchedulePersonState(jumped) == HashSchedulePersonState(stepped), "jumping equals stepping");
    const ScheduleActivityTally& tally = jumped.Tally.at(std::string(AttendanceClassActivityId));
    Check(tally.Sessions == 30 && tally.Minutes == 30 * 45, "a school week is thirty 45-minute class sessions");
}

void TestNpcNeverLate()
{
    const BellSchedule& sci = *RealSchedules().Find(School, "science");
    const ScheduleTravelTable travel = TravelWithHome();
    // The student is booked at home right up to the bell: M2-06 shortens the lower-priority home stay instead.
    const PersonSchedule tight = StudentWithHome(sci, Classmate, 600);
    Check(!ValidateScheduleRoutineTravel(tight, travel).empty(), "a booking with no room to walk is a squeezed routine");
    for (std::int64_t day = 0; day < 5; ++day)
    {
        const ScheduleResolution atBell = ResolveSchedule(tight, travel, AttendanceAtMinute(day, 600));
        Check(atBell.Status == ScheduleStatus::Stay && atBell.Zone == "nhss.court" && atBell.Doing == AttendanceAssemblyRole,
              "the NPC is in assembly exactly at the bell");
        const ScheduleResolution before = ResolveSchedule(tight, travel, AttendanceAtMinute(day, 599));
        Check(before.Status == ScheduleStatus::Travel && before.Squeezed && before.SegmentEnd == AttendanceAtMinute(day, 600),
              "the NPC left home early and arrives on the bell");
        const ScheduleResolution left = ResolveSchedule(tight, travel, AttendanceAtMinute(day, 600 - HomeTripMinutes - 1));
        Check(left.Status == ScheduleStatus::Stay && left.Zone == "home" && left.Squeezed, "the squeezed stay is the home stay");
    }
}

void TestPlayerArrival()
{
    // The player is the one whose choices can move arrival: an oversleep exception outranks the school.
    const BellSchedule& sci = *RealSchedules().Find(School, "science");
    const ScheduleTravelTable travel = TravelWithHome();
    PersonSchedule player = StudentWithHome(sci, Player);
    player.Exceptions.push_back({"oversleep", AttendanceAtMinute(0, 540), AttendanceAtMinute(0, 603), "home", ScheduleDoingKind::Role,
                                 "sleeping", 700});
    Check(ValidatePersonSchedule(player).empty(), "the player's schedule is valid");
    GameInstant arrival = -1;
    for (std::int32_t minute = 540; minute < 700 && arrival < 0; ++minute)
    {
        const ScheduleResolution r = ResolveSchedule(player, travel, AttendanceAtMinute(0, minute));
        if (r.Status == ScheduleStatus::Travel && r.Zone.rfind("nhss.", 0) == 0)
            arrival = r.SegmentEnd;
    }
    // Home until 10:03, then the 30-minute trip: the assembly is gone and the first period is entered at 10:33.
    Check(arrival == AttendanceAtMinute(0, 633), "the oversleeping player reaches school at 10:33");
    AttendanceRecord record = StartAttendanceRecord(0);
    const AttendanceResult result = RecordAttendanceArrival(record, sci, arrival);
    Check(result.Ok && result.Day.Outcome == AttendanceOutcome::VeryLate && result.Day.PeriodMask == 0x3E && result.Day.ArrivalMinute == 633,
          "33 minutes after the bell is very late, with the first period missed");
    // The same player who is not delayed: the unmodified calendar puts them at the court on the bell.
    const PersonSchedule punctual = StudentWithHome(sci, Player);
    GameInstant onTime = -1;
    for (std::int32_t minute = 540; minute < 700 && onTime < 0; ++minute)
    {
        const ScheduleResolution r = ResolveSchedule(punctual, travel, AttendanceAtMinute(1, minute));
        if (r.Status == ScheduleStatus::Travel && r.Zone.rfind("nhss.", 0) == 0)
            onTime = r.SegmentEnd;
    }
    Check(onTime == AttendanceAtMinute(1, 600), "the unmodified calendar arrives exactly on the bell");
    AttendanceRecord second = StartAttendanceRecord(1);
    const AttendanceResult ok = RecordAttendanceArrival(second, sci, onTime);
    Check(ok.Ok && ok.Day.Outcome == AttendanceOutcome::OnTime && ok.Day.PeriodMask == 0x3F, "arriving on the bell is on time");
}
} // namespace

void TestContentAndRoutine()
{
    TestRoutineEntries();
    TestValidAndResolves();
    TestSharedCalendar();
    TestNpcNeverLate();
    TestPlayerArrival();
}
