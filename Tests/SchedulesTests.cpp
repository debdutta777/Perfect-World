// ROADMAP M2-06: schedules and minimal off-screen progress (routine, exceptions, gaps, validation).
#include "SchedulesTestSupport.h"

#include <cstdlib>
#include <iostream>

int Checks = 0;

void Check(bool condition, const char* message)
{
    ++Checks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

// Defined in SchedulesProgressTests.cpp.
void TestAdvance();
void TestText();
void TestRandomSchedules();

namespace
{
void TestRoutineAndCalendar()
{
    const PersonSchedule p = Fixture();
    Check(ValidatePersonSchedule(p).empty() && ValidateScheduleTravel(Travel()).empty(), "fixture and travel table validate");
    Check(ValidateScheduleTravelCoverage(p, Travel()).empty(), "travel covers every zone pair");
    Check(ValidateScheduleRoutineTravel(p, Travel()).empty(), "the fixture routine squeezes nothing");
    Check(GameClock::ToCalendar(0).DayOfWeek == Weekday::Monday && GameClock::ToCalendar(0).Hour == 6, "instant 0 is Monday 06:00");

    const ScheduleResolution morning = Where(p, 0);
    Check(morning.Valid && morning.Status == ScheduleStatus::Stay && morning.Zone == Home && morning.Doing == "morning", "the first minute is the morning block");
    Check(morning.SourceId == "routine:2" && morning.SegmentStart == 0 && morning.SegmentEnd == 90 && morning.NominalMinutes == 90, "morning bounds");
    ExpectStay(p, 89, Home, "morning", "07:29 still at home");
    ExpectTravel(p, 90, Home, School, 0, "07:30 sets out for school");
    ExpectTravel(p, 105, Home, School, 500, "halfway through the 30 minute trip");
    ExpectTravel(p, 119, Home, School, 966, "progress is floor(1000 * 29 / 30)");
    const ScheduleResolution inClass = Where(p, 120);
    Check(inClass.Status == ScheduleStatus::Stay && inClass.Zone == School && inClass.Kind == Act && inClass.Doing == Class, "08:00 in class on arrival");
    Check(inClass.SegmentStart == 120 && inClass.SegmentEnd == 540 && inClass.NominalMinutes == 420 && !inClass.Squeezed, "class bounds, nothing squeezed");
    const ScheduleResolution travelling = Where(p, 100);
    Check(travelling.Doing == Class && travelling.SourceId == "routine:3" && travelling.Kind == Act, "a trip names what it is for");
    ExpectIdle(p, At(0, 15, 14), School, "after class the student lingers at school");
    ExpectTravel(p, At(0, 15, 15), School, Field, 0, "15:15 heads for the field");
    ExpectStay(p, At(0, 15, 30), Field, Run, "15:30 running");
    ExpectIdle(p, At(0, 17, 49), Field, "lingers at the field");
    ExpectTravel(p, At(0, 17, 55), Field, Home, 500, "the reverse of a listed leg is used");
    ExpectStay(p, At(0, 18, 0), Home, Study, "18:00 studying at home");
    ExpectStay(p, At(0, 20, 0), Home, "evening", "adjacent blocks in one zone need no trip");
    ExpectStay(p, At(0, 23, 0), Home, "sleeping", "asleep");
    ExpectStay(p, At(5, 3, 0), Home, "sleeping", "asleep on Saturday at 03:00");
    ExpectStay(p, At(5, 10, 30), Library, Study, "weekend study at the library");
    ExpectTravel(p, At(5, 9, 50), Home, Library, 500, "10 of the 20 minutes to the library");
    ExpectIdle(p, At(5, 12, 5), Library, "lingering until it is time to leave");
    // The weekday mask follows the real calendar for a month of days.
    for (int day = 0; day < 31; ++day)
    {
        const Weekday weekday = GameClock::ToCalendar(At(day, 10, 0)).DayOfWeek;
        const bool school = weekday != Weekday::Saturday && weekday != Weekday::Sunday;
        const ScheduleResolution r = Where(p, At(day, 10, 0));
        Check(school ? (r.Zone == School && r.Doing == Class) : (r.Zone == Library && r.Doing == Study), "ten o'clock follows the calendar weekday");
    }
    Check(!Where(p, -1).Valid && !Where(p, ScheduleMaxInstant + 1).Valid, "instants outside the range are invalid");
    Check(Where(p, 777).SegmentStart <= 777 && Where(p, 777).SegmentEnd > 777, "the segment contains the instant");
}

void TestExceptionsAndPriority()
{
    const PersonSchedule p = BusyFixture();
    // The assembly (30) beats school (20): the walk there comes out of the school day and the walk back delays the rest.
    ExpectStay(p, At(1, 9, 0), School, Class, "school before the assembly");
    const ScheduleResolution first = Where(p, At(1, 9, 0));
    Check(first.SegmentEnd == At(1, 9, 45) && first.Squeezed && first.NominalMinutes == 420, "school ends early for the trip");
    ExpectTravel(p, At(1, 9, 52), School, Field, 466, "squeezed trip to the assembly");
    Check(Where(p, At(1, 9, 52)).Squeezed, "the trip carries the squeezed flag");
    ExpectStay(p, At(1, 10, 30), Field, "assembly", "the assembly");
    Check(!Where(p, At(1, 10, 30)).Squeezed && Where(p, At(1, 10, 30)).SourceId == "ex_assembly", "the winner is not squeezed and names its exception");
    ExpectTravel(p, At(1, 11, 5), Field, School, 333, "walking back");
    const ScheduleResolution rest = Where(p, At(1, 12, 0));
    Check(rest.Status == ScheduleStatus::Stay && rest.Doing == Class && rest.SegmentStart == At(1, 11, 15) && rest.Squeezed, "the rest of the day starts late");
    ExpectStay(p, At(0, 10, 0), School, Class, "other days are untouched");
    // Equal priorities: the exception that starts later wins (ex_b over ex_a), and the earlier one leaves in time.
    ExpectStay(p, At(2, 10, 30), Library, "alpha", "ex_a holds its first part");
    ExpectTravel(p, At(2, 10, 50), Library, Field, 500, "ex_a gives way to ex_b with a trip between");
    ExpectStay(p, At(2, 12, 0), Field, "beta", "ex_b wins where they overlap");
    ExpectTravel(p, At(2, 13, 5), Field, School, 333, "the school resumes late after beta");
    // The list order never matters.
    PersonSchedule reversed = p;
    std::reverse(reversed.Exceptions.begin(), reversed.Exceptions.end());
    for (GameInstant t = At(1, 0, 0); t < At(6, 0, 0); t += 7)
        Check(SameId(Where(p, t), Where(reversed, t)), "exception order does not change the answer");
    // Same priority and same start: the smaller ID wins.
    PersonSchedule same = Fixture();
    same.Exceptions = {Exception("ex_d", At(2, 12, 0), At(2, 13, 0), Home, "dee", 70), Exception("ex_c", At(2, 12, 0), At(2, 13, 0), Home, "cee", 70)};
    ExpectStay(same, At(2, 12, 30), Home, "cee", "equal priority and start: the smaller ID wins");
    // An exception equal to the routine priority beats it; one below does not.
    ExpectStay(p, At(2, 23, 0), Home, "watching", "an exception equal to the routine priority wins");
    PersonSchedule below = Fixture();
    below.Exceptions = {Exception("ex_low", At(2, 22, 30), At(2, 23, 30), Home, "watching", 49)};
    ExpectStay(below, At(2, 23, 0), Home, "sleeping", "an exception below the routine priority loses");
    ExpectStay(below, At(2, 10, 0), School, Class, "and does not disturb other blocks");
    // A sick spell (40) beats school but not sleep (50).
    ExpectStay(p, At(3, 10, 0), Home, "sick", "sick at home on Thursday");
    ExpectStay(p, At(3, 23, 0), Home, "sleeping", "sleep outranks the sick spell");
    ExpectStay(p, At(4, 12, 0), Home, "sick", "sick on Friday");
    ExpectStay(p, At(5, 3, 0), Home, "sleeping", "asleep before the spell ends");
    ExpectStay(p, At(5, 7, 0), Home, "morning", "Saturday morning after the spell");
    // A 30-day holiday (80) beats everything, keeps its true bounds in the middle, and the edges squeeze the neighbours.
    PersonSchedule holiday = Fixture();
    holiday.Exceptions = {Exception("ex_holiday", At(20, 0, 0), At(50, 0, 0), Library, "holiday", 80)};
    for (const GameInstant t : {At(35, 12, 0), At(35, 3, 0), At(21, 0, 5), At(49, 23, 59)})
    {
        const ScheduleResolution r = Where(holiday, t);
        Check(r.Status == ScheduleStatus::Stay && r.Zone == Library && r.SegmentStart == At(20, 0, 0) && r.SegmentEnd == At(50, 0, 0), "holiday keeps its true bounds");
    }
    ExpectTravel(holiday, At(19, 23, 50), Home, Library, 500, "sleep gives up its last 20 minutes for the trip");
    ExpectTravel(holiday, At(50, 0, 10), Library, Home, 500, "the trip home delays the next sleep");
    // Priority 0..1000 and the sweep never leave a hole.
    for (GameInstant t = At(0, 0, 0) + 360; t < At(10, 0, 0); t += 13)
        Check(Where(p, t).Valid, "every minute resolves");
}

void TestGapsAndSqueezes()
{
    PersonSchedule empty;
    empty.PersonId = Student;
    empty.HomeZone = Home;
    ExpectIdle(empty, 12345, Home, "no schedule means idle at home");
    Check(!NextScheduleBoundary(empty, Travel(), 0).has_value(), "no schedule never changes");

    PersonSchedule once = empty;
    once.Routine = {Entry(0x02, Hm(8, 0), Hm(9, 0), School, Act, Class, 5)};
    ExpectIdle(once, At(0, 7, 0), School, "last week's block is the predecessor, so no trip is needed");
    ExpectStay(once, At(0, 8, 30), School, Class, "the weekly block");
    ExpectIdle(once, At(3, 12, 0), School, "after the one block the person stays where it ended");
    once.Routine.push_back(Entry(0x08, Hm(10, 0), Hm(11, 0), Home, Role, "visit", 5));
    ExpectIdle(once, At(1, 12, 0), School, "after Monday and before Wednesday's visit");
    ExpectTravel(once, At(2, 9, 45), School, Home, 500, "the trip is timed to arrive exactly at the start");
    ExpectIdle(once, At(2, 11, 30), Home, "after the visit the person stays at home");

    // Overlapping routine entries: the higher priority wins, equal priority is refused.
    PersonSchedule over = empty;
    over.Routine = {Entry(ScheduleAllWeekdays, 0, Hm(12, 0), Home, Role, "a", 1), Entry(ScheduleAllWeekdays, Hm(6, 0), Hm(8, 0), Home, Role, "b", 2)};
    Check(ValidatePersonSchedule(over).empty(), "overlap with different priorities is allowed");
    ExpectStay(over, At(1, 5, 0), Home, "a", "lower priority outside the overlap");
    ExpectStay(over, At(1, 7, 0), Home, "b", "higher priority inside the overlap");
    ExpectStay(over, At(1, 9, 0), Home, "a", "and the lower one resumes");
    over.Routine[1].Priority = 1;
    Check(!ValidatePersonSchedule(over).empty(), "equal-priority overlap is refused");

    // A routine that cannot fit its trips is refused by the routine check and squeezed in resolution.
    PersonSchedule tight = Fixture();
    tight.Routine[3].StartMinute = Hm(7, 50);
    Check(!ValidateScheduleRoutineTravel(tight, Travel()).empty(), "a 10 minute gap for a 30 minute trip is refused");
    ExpectStay(tight, At(0, 7, 10), Home, "morning", "still at breakfast");
    Check(Where(tight, At(0, 7, 10)).Squeezed && Where(tight, At(0, 7, 10)).SegmentEnd == At(0, 7, 20), "the lower priority stay is shortened");
    ExpectTravel(tight, At(0, 7, 25), Home, School, 166, "the whole trip is still travelled");
    Check(Where(tight, At(0, 7, 25)).Squeezed, "and flagged");

    // A stay squeezed to nothing disappears (the earlier or the later one, whichever has less priority).
    PersonSchedule gone = empty;
    gone.Routine = {Entry(ScheduleAllWeekdays, Hm(8, 0), Hm(8, 10), Home, Role, "a", 10), Entry(ScheduleAllWeekdays, Hm(8, 10), Hm(12, 0), School, Role, "b", 20)};
    ExpectIdle(gone, At(1, 8, 5), School, "the 10 minute stay cannot give 30 minutes and vanishes");
    Check(!Where(gone, At(1, 8, 10)).Squeezed && Where(gone, At(1, 8, 10)).Doing == "b", "the survivor is not squeezed");
    PersonSchedule dropped = empty;
    dropped.Routine = {Entry(ScheduleAllWeekdays, Hm(8, 0), Hm(12, 0), School, Role, "a", 50), Entry(ScheduleAllWeekdays, Hm(12, 0), Hm(12, 10), Home, Role, "b", 10)};
    ExpectIdle(dropped, At(1, 12, 5), School, "a late arrival that would eat the whole stay drops it");

    // Travel table behaviour.
    const ScheduleTravelTable t = Travel();
    Check(ScheduleTravelMinutes(t, Home, Home) == 0 && ScheduleTravelMinutes(t, Home, School) == 30 && ScheduleTravelMinutes(t, School, Home) == 30, "same zone, listed and reverse legs");
    Check(ScheduleTravelMinutes(t, "loc_x", Home) == ScheduleFallbackTravelMinutes, "an unknown pair uses the fallback");
    ScheduleTravelTable asym;
    asym.Legs = {{"loc_a", "loc_b", 10}, {"loc_b", "loc_a", 20}};
    Check(ValidateScheduleTravel(asym).empty() && ScheduleTravelMinutes(asym, "loc_a", "loc_b") == 10 && ScheduleTravelMinutes(asym, "loc_b", "loc_a") == 20, "an explicit reverse leg may differ");
    ScheduleTravelTable bad = asym;
    bad.Legs = {{"loc_b", "loc_a", 20}, {"loc_a", "loc_b", 10}};
    Check(!ValidateScheduleTravel(bad).empty(), "unsorted legs are refused");
    bad.Legs = {{"loc_a", "loc_b", 10}, {"loc_a", "loc_b", 10}};
    Check(!ValidateScheduleTravel(bad).empty(), "duplicate legs are refused");
    for (const ScheduleTravelLeg& leg : std::vector<ScheduleTravelLeg>{{"loc_a", "loc_a", 5}, {"loc_a", "loc_b", 0}, {"loc_a", "loc_b", 721}, {"Loc_A", "loc_b", 5}})
    {
        bad.Legs = {leg};
        Check(!ValidateScheduleTravel(bad).empty(), "a self-leg, zero or oversize minutes or a bad token is refused");
    }
    ScheduleTravelTable missing;
    missing.Legs = {{Home, School, 30}};
    Check(!ValidateScheduleTravelCoverage(Fixture(), missing).empty(), "a missing leg is reported");

    PersonSchedule week = empty;
    week.Routine = {Entry(ScheduleAllWeekdays, 0, 720, Home, Role, "a", 1), Entry(ScheduleAllWeekdays, 720, 1440, Home, Role, "b", 1)};
    Check(ScheduleCoversWholeWeek(week) && !ScheduleCoversWholeWeek(Fixture()), "week coverage");
    week.Routine[1].StartMinute = 721;
    Check(!ScheduleCoversWholeWeek(week), "one missing minute is found");
}

void TestValidationRefusals()
{
    const PersonSchedule base = Fixture();
    auto refused = [&](auto edit, const char* message) {
        PersonSchedule p = base;
        edit(p);
        Check(!ValidatePersonSchedule(p).empty(), message);
    };
    refused([](PersonSchedule& p) { p.PersonId = "P_X"; }, "bad person ID");
    refused([](PersonSchedule& p) { p.HomeZone = "Home Zone"; }, "bad home zone");
    refused([](PersonSchedule& p) { p.Routine[0].WeekdayMask = 0; }, "empty mask");
    refused([](PersonSchedule& p) { p.Routine[0].WeekdayMask = 0x80; }, "out-of-range mask");
    refused([](PersonSchedule& p) { p.Routine[0].StartMinute = p.Routine[0].EndMinute; }, "empty range");
    refused([](PersonSchedule& p) { p.Routine[0].EndMinute = 1441; }, "past midnight");
    refused([](PersonSchedule& p) { p.Routine[0].StartMinute = -1; }, "negative start");
    refused([](PersonSchedule& p) { p.Routine[0].Zone = "Zone"; }, "bad zone token");
    refused([](PersonSchedule& p) { p.Routine[0].Doing = ""; }, "empty doing");
    refused([](PersonSchedule& p) { p.Routine[0].Priority = 1001; }, "priority too high");
    refused([](PersonSchedule& p) { p.Routine[0].Priority = -1; }, "negative priority");
    refused([](PersonSchedule& p) { p.Routine.resize(ScheduleMaxRoutineEntries + 1, p.Routine[2]); }, "too many entries");
    refused([](PersonSchedule& p) { p.Exceptions = {Exception("routine:3", 0, 10, Home, "x", 1)}; }, "reserved exception ID");
    refused([](PersonSchedule& p) { p.Exceptions = {Exception("ex_a", 0, 10, Home, "x", 1), Exception("ex_a", 20, 30, Home, "x", 1)}; }, "duplicate exception ID");
    refused([](PersonSchedule& p) { p.Exceptions = {Exception("ex_a", 10, 10, Home, "x", 1)}; }, "empty exception range");
    refused([](PersonSchedule& p) { p.Exceptions = {Exception("ex_a", -5, 10, Home, "x", 1)}; }, "negative exception start");
    refused([](PersonSchedule& p) { p.Exceptions = {Exception("ex_a", 0, ScheduleMaxInstant + 1, Home, "x", 1)}; }, "exception past the maximum instant");
    refused([](PersonSchedule& p) { p.Exceptions = {Exception("ex_a", 0, 10, Home, "x", 1001)}; }, "exception priority too high");
    refused([](PersonSchedule& p) { p.Exceptions = {Exception("Ex A", 0, 10, Home, "x", 1)}; }, "bad exception ID");
    refused([](PersonSchedule& p) { p.Exceptions.assign(ScheduleMaxExceptions + 1, Exception("ex_a", 0, 10, Home, "x", 1)); }, "too many exceptions");
    Check(ValidatePersonSchedule(base).empty() && IsValidIdentityId("npc_teacher_roy"), "the unmodified fixture is still valid");
}
} // namespace

int main()
{
    TestRoutineAndCalendar();
    TestExceptionsAndPriority();
    TestGapsAndSqueezes();
    TestValidationRefusals();
    TestAdvance();
    TestText();
    TestRandomSchedules();
    std::cout << "Schedules tests passed (" << Checks << " checks).\n";
    return 0;
}
