// ROADMAP M2-07: lateness boundaries, absences, thresholds and the running attendance record.
#include "AttendanceTestSupport.h"

#include <bit>

using namespace PerfectWorld;

namespace
{
constexpr const char* School = "institution.nabapara_vidyalaya";

const BellSchedule& Science()
{
    return *RealSchedules().Find(School, "science");
}

/** The n-th school day (Monday to Friday) as a day number. */
std::int64_t SchoolDay(std::int64_t n)
{
    return (n / 5) * 7 + n % 5;
}

void Expect(const AttendanceForecast& f, AttendanceOutcome outcome, std::uint32_t mask, const char* message)
{
    Check(f.Outcome == outcome && f.PeriodMask == mask && f.PeriodsAttended == std::popcount(mask) && f.PeriodsHeld == 6, message);
}

void TestBoundaries()
{
    const BellSchedule& s = Science();
    Check(BellFirstBell(s) == 600, "the bell is 10:00");
    // Before the bell, and exactly at it, is on time.
    Expect(ForecastAttendance(s, 0), AttendanceOutcome::OnTime, 0x3F, "arriving at midnight is on time (the day is still held)");
    Expect(ForecastAttendance(s, 570), AttendanceOutcome::OnTime, 0x3F, "arriving as the gate opens is on time");
    Expect(ForecastAttendance(s, 599), AttendanceOutcome::OnTime, 0x3F, "one minute before the bell is on time");
    const AttendanceForecast atBell = ForecastAttendance(s, 600);
    Expect(atBell, AttendanceOutcome::OnTime, 0x3F, "exactly at the bell is on time");
    Check(!atBell.AddsLateMark && !atBell.MissesAssembly && atBell.MinutesAfterBell == 0, "no mark and no missed assembly at the bell");
    // One minute after the bell: late, with a mark, still in every period.
    const AttendanceForecast oneAfter = ForecastAttendance(s, 601);
    Expect(oneAfter, AttendanceOutcome::Late, 0x3F, "one minute after the bell is late");
    Check(oneAfter.AddsLateMark && !oneAfter.MissesAssembly && oneAfter.MinutesAfterBell == 1, "a late mark, assembly not missed");
    Expect(ForecastAttendance(s, 600 + AttendanceBellGraceMinutes), AttendanceOutcome::Late, 0x3F, "the last minute of the grace is late");
    // One minute after the grace: very late, the assembly is missed.
    const AttendanceForecast pastGrace = ForecastAttendance(s, 600 + AttendanceBellGraceMinutes + 1);
    Expect(pastGrace, AttendanceOutcome::VeryLate, 0x3F, "one minute after the grace is very late");
    Check(pastGrace.AddsLateMark && pastGrace.MissesAssembly, "very late: a mark and the assembly is missed");
    // Each period is lost the minute after its own grace ends.
    const std::int32_t starts[] = {620, 670, 720, 805, 855, 905};
    for (int i = 0; i < 6; ++i)
    {
        const std::uint32_t full = 0x3Fu & ~((1u << i) - 1u);
        const std::uint32_t after = 0x3Fu & ~((1u << (i + 1)) - 1u);
        Expect(ForecastAttendance(s, starts[i] + AttendancePeriodGraceMinutes), AttendanceOutcome::VeryLate, full, "within a period's grace it is attended");
        const AttendanceForecast f = ForecastAttendance(s, starts[i] + AttendancePeriodGraceMinutes + 1);
        Expect(f, after == 0 ? AttendanceOutcome::Absent : AttendanceOutcome::VeryLate, after, "one minute past a period's grace it is missed");
        Check(f.AddsLateMark == (after != 0), "an absent day carries no late mark");
    }
    Check(ForecastAttendance(s, 911).MissesAssembly && ForecastAttendance(s, 1439).Outcome == AttendanceOutcome::Absent, "after the last grace the day is absent");
    // Leaving early loses the periods not finished.
    Expect(ForecastAttendance(s, 600, 950), AttendanceOutcome::OnTime, 0x3F, "staying to dismissal keeps every period");
    Expect(ForecastAttendance(s, 600, 949), AttendanceOutcome::OnTime, 0x1F, "leaving a minute before dismissal loses the last period");
    Expect(ForecastAttendance(s, 600, 900), AttendanceOutcome::OnTime, 0x1F, "leaving as period five ends keeps it");
    Expect(ForecastAttendance(s, 600, 665), AttendanceOutcome::OnTime, 0x01, "leaving after the first period keeps only it");
    Expect(ForecastAttendance(s, 600, 664), AttendanceOutcome::Absent, 0x00, "leaving before it ends is absent");
    Expect(ForecastAttendance(s, 700, 650), AttendanceOutcome::Absent, 0x00, "leaving before arriving is absent");
    // The three streams share the frame, so they share every answer.
    for (const char* stream : {"commerce", "humanities"})
    {
        const BellSchedule& other = *RealSchedules().Find(School, stream);
        for (std::int32_t m = 0; m <= 1440; ++m)
        {
            const AttendanceForecast a = ForecastAttendance(s, m);
            const AttendanceForecast b = ForecastAttendance(other, m);
            Check(a.Outcome == b.Outcome && a.PeriodMask == b.PeriodMask && a.AddsLateMark == b.AddsLateMark, "every stream answers every minute alike");
        }
    }
}

void TestSweeps()
{
    const BellSchedule& s = Science();
    for (const std::int32_t departure : {AttendanceStaysUntilDismissal, 949, 900, 800, 700, 665, 0})
    {
        AttendanceForecast before = ForecastAttendance(s, 0, departure);
        for (std::int32_t arrival = 1; arrival <= 1440; ++arrival)
        {
            const AttendanceForecast f = ForecastAttendance(s, arrival, departure);
            Check((f.PeriodMask & ~before.PeriodMask) == 0, "arriving later never attends a period that arriving earlier missed");
            Check(f.Outcome >= before.Outcome, "arriving later is never a better outcome");
            Check((f.Outcome == AttendanceOutcome::Absent) == (f.PeriodMask == 0), "absent exactly when no period is attended");
            Check(f.AddsLateMark == (f.Outcome == AttendanceOutcome::Late || f.Outcome == AttendanceOutcome::VeryLate), "a mark exactly for late and very late");
            Check(f.MinutesAfterBell == (arrival > 600 ? arrival - 600 : 0), "minutes after the bell");
            before = f;
        }
    }
}

AttendanceRecord Build(std::int64_t schoolDays, std::int64_t absent)
{
    AttendanceRecord record = StartAttendanceRecord(0);
    for (std::int64_t n = 0; n < schoolDays; ++n)
    {
        const AttendanceResult r = n < absent ? RecordAttendanceAbsence(record, Science(), AttendanceAtMinute(SchoolDay(n), 590))
                                              : RecordAttendanceArrival(record, Science(), AttendanceAtMinute(SchoolDay(n), 590));
        Check(r.Ok, "a school day is recorded");
    }
    return record;
}

void TestWeekAndThresholds()
{
    const BellSchedule& s = Science();
    AttendanceRecord record = StartAttendanceRecord(0);
    Check(AttendancePermille(record) == 1000 && AttendanceStandingOf(record) == AttendanceStanding::Good, "a fresh record is in good standing");
    // Monday on time, Tuesday late, Wednesday very late (first period still entered), Thursday absent, Friday on time.
    Check(RecordAttendanceArrival(record, s, AttendanceAtMinute(0, 590)).Day.Outcome == AttendanceOutcome::OnTime, "Monday on time");
    const AttendanceResult tue = RecordAttendanceArrival(record, s, AttendanceAtMinute(1, 603));
    Check(tue.Day.Outcome == AttendanceOutcome::Late && tue.Day.ArrivalMinute == 603 && tue.DaysClosedAbsent == 0, "Tuesday late");
    Check(RecordAttendanceArrival(record, s, AttendanceAtMinute(2, 620)).Day.Outcome == AttendanceOutcome::VeryLate, "Wednesday very late");
    const AttendanceResult thu = RecordAttendanceAbsence(record, s, AttendanceAtMinute(3, 12 * 60));
    Check(thu.Ok && thu.Day.Outcome == AttendanceOutcome::Absent && thu.Day.ArrivalMinute == 1440 && thu.Day.PeriodMask == 0, "Thursday absent");
    const AttendanceResult fri = RecordAttendanceArrival(record, s, AttendanceAtMinute(4, 600));
    Check(fri.Ok && fri.Day.Outcome == AttendanceOutcome::OnTime && fri.StandingBefore == AttendanceStanding::Warned &&
              fri.StandingAfter == AttendanceStanding::Warned,
          "Friday exactly at the bell is on time");
    Check(record.SchoolDays == 5 && record.OnTimeDays == 2 && record.LateDays == 1 && record.VeryLateDays == 1 && record.AbsentDays == 1 &&
              record.PeriodsHeld == 30 && record.PeriodsAttended == 24 && record.NextDay == 5 && record.Days.size() == 5,
          "the week's totals");
    Check(AttendanceLateMarks(record) == 2 && AttendanceLateWarnings(record) == 0 && !AttendanceGuardianNotified(record), "two late marks, no warning yet");
    Check(AttendancePermille(record) == 800 && AttendanceStandingOf(record) == AttendanceStanding::Warned, "24 of 30 periods is 80.0%, warned");
    Check(ValidateAttendanceRecord(record).empty(), "the record is consistent");

    // Late warnings every third mark, the guardian at the sixth.
    AttendanceRecord marks = StartAttendanceRecord(0);
    for (std::int64_t n = 0; n < 8; ++n)
    {
        const AttendanceResult r = RecordAttendanceArrival(marks, s, AttendanceAtMinute(SchoolDay(n), 601));
        Check(r.Ok && r.Day.Outcome == AttendanceOutcome::Late, "a late day");
        Check(r.LateWarningIssued == (n == 2 || n == 5), "a warning on the third and sixth marks only");
        Check(r.GuardianNoticeIssued == (n == 5), "the guardian is told on the sixth mark only");
        Check(AttendanceLateWarnings(marks) == (n + 1) / 3 && AttendanceGuardianNotified(marks) == (n + 1 >= 6), "warnings and notice follow the marks");
    }
    Check(AttendancePermille(marks) == 1000 && AttendanceStandingOf(marks) == AttendanceStanding::Good, "lateness alone never lowers period attendance");

    // Period attendance thresholds on 100 school days: 85.0% and 75.0% are the edges.
    const struct { std::int64_t Absent; std::int32_t Permille; AttendanceStanding Standing; } cases[] = {
        {0, 1000, AttendanceStanding::Good}, {15, 850, AttendanceStanding::Good},   {16, 840, AttendanceStanding::Warned},
        {25, 750, AttendanceStanding::Warned}, {26, 740, AttendanceStanding::AtRisk}, {100, 0, AttendanceStanding::AtRisk}};
    for (const auto& c : cases)
    {
        const AttendanceRecord r = Build(100, c.Absent);
        Check(AttendancePermille(r) == c.Permille && AttendanceStandingOf(r) == c.Standing, "the standing at the documented thresholds");
        Check(r.AbsentDays == c.Absent && ValidateAttendanceRecord(r).empty(), "the counts agree");
    }
    AttendanceRecord crossing = Build(100, 0);
    for (std::int64_t n = 100; n < 120; ++n)
        Check(RecordAttendanceAbsence(crossing, s, AttendanceAtMinute(SchoolDay(n), 600)).Ok, "extra absences recorded");
    Check(AttendanceStandingOf(crossing) == AttendanceStanding::Warned, "100 present and 20 absent is 83.3%, warned");
    AttendanceRecord edge = Build(100, 15);
    const AttendanceResult tip = RecordAttendanceAbsence(edge, s, AttendanceAtMinute(SchoolDay(100), 600));
    Check(tip.StandingBefore == AttendanceStanding::Good && tip.StandingAfter == AttendanceStanding::Warned, "the result reports the standing change");
}

void TestCatchUpAndRefusals()
{
    const BellSchedule& s = Science();
    AttendanceRecord record = StartAttendanceRecord(0);
    const AttendanceResult jump = RecordAttendanceArrival(record, s, AttendanceAtMinute(14, 590));
    Check(jump.Ok && jump.DaysClosedAbsent == 10 && record.SchoolDays == 11 && record.AbsentDays == 10 && record.OnTimeDays == 1 && record.NextDay == 15,
          "arriving on the third Monday closes the ten earlier school days as absent");
    Check(record.Days.front().Day == 0 && record.Days[9].Day == 11 && record.Days.back().Day == 14, "weekends are not school days");

    const AttendanceRecord frozen = record;
    const auto refused = [&](const AttendanceResult& r, const char* why) {
        Check(!r.Ok && !r.Error.empty() && record == frozen, why);
    };
    refused(RecordAttendanceArrival(record, s, AttendanceAtMinute(14, 600)), "a day already recorded is refused");
    refused(RecordAttendanceArrival(record, s, AttendanceAtMinute(7, 600)), "an earlier day is refused");
    refused(RecordAttendanceArrival(record, s, AttendanceAtMinute(19, 600)), "a Saturday is refused");
    refused(RecordAttendanceAbsence(record, s, AttendanceAtMinute(20, 600)), "a Sunday absence is refused");
    refused(RecordAttendanceArrival(record, s, -1), "an instant before the clock is refused");
    refused(RecordAttendanceArrival(record, s, ScheduleMaxInstant + 1), "an instant past the clock is refused");
    refused(RecordAttendanceArrival(record, s, AttendanceAtMinute(15, 600), -1), "a negative departure is refused");
    refused(RecordAttendanceArrival(record, s, AttendanceAtMinute(15, 600), 1441), "a departure past midnight is refused");
    BellSchedule broken = s;
    broken.Blocks[2].StartMinute = 640;
    refused(RecordAttendanceArrival(record, broken, AttendanceAtMinute(15, 600)), "an invalid schedule is refused");
    refused(RecordAttendanceAbsence(record, broken, AttendanceAtMinute(15, 600)), "an invalid schedule is refused for an absence");
    refused(CloseAttendanceBefore(record, broken, AttendanceAtMinute(30, 600)), "an invalid schedule is refused for a close");
    refused(CloseAttendanceBefore(record, s, ScheduleMaxInstant + 1), "a close past the clock is refused");

    AttendanceRecord old = StartAttendanceRecord(0);
    Check(RecordAttendanceArrival(old, s, AttendanceAtMinute(366, 590)).Ok, "exactly a year and a day behind is closed");
    AttendanceRecord stale = StartAttendanceRecord(0);
    const AttendanceResult tooFar = RecordAttendanceArrival(stale, s, AttendanceAtMinute(371, 590));
    Check(!tooFar.Ok && tooFar.Error.find("a year") != std::string::npos && stale == StartAttendanceRecord(0), "further behind is refused and untouched");

    // Closing before today, then arriving, matches arriving directly.
    AttendanceRecord closed = StartAttendanceRecord(0);
    const AttendanceResult close = CloseAttendanceBefore(closed, s, AttendanceAt(7, 8, 0));
    Check(close.Ok && close.DaysClosedAbsent == 5 && closed.NextDay == 7 && closed.AbsentDays == 5, "the first week is closed absent");
    Check(CloseAttendanceBefore(closed, s, AttendanceAt(7, 9, 0)).DaysClosedAbsent == 0, "closing again changes nothing");
    Check(CloseAttendanceBefore(closed, s, AttendanceAt(3, 9, 0)).Ok && closed.NextDay == 7, "closing for an earlier day changes nothing");
    Check(RecordAttendanceArrival(closed, s, AttendanceAtMinute(7, 590)).Ok && closed.SchoolDays == 6, "the next Monday is recorded after the close");
    AttendanceRecord direct = StartAttendanceRecord(0);
    Check(RecordAttendanceArrival(direct, s, AttendanceAtMinute(7, 590)).DaysClosedAbsent == 5 && direct == closed, "closing first equals arriving directly");
}

void TestTrimAndFuzz()
{
    const BellSchedule& s = Science();
    const AttendanceRecord big = Build(500, 7);
    Check(big.SchoolDays == 500 && big.Days.size() == AttendanceKeptDays && big.Days.front().Day == SchoolDay(100) && big.AbsentDays == 7 &&
              big.PeriodsHeld == 3000 && big.PeriodsAttended == 3000 - 42 && ValidateAttendanceRecord(big).empty(),
          "only the last 400 days are kept, the totals stay exact");

    for (std::uint64_t seed = 1; seed <= 400; ++seed)
    {
        AttendanceRecord a = StartAttendanceRecord(static_cast<std::int64_t>(seed % 30));
        AttendanceRecord b = a;
        AttendanceRng rng{seed * 7919};
        AttendanceRng again{seed * 7919};
        std::int64_t day = a.FirstDay;
        for (int step = 0; step < 40; ++step)
        {
            day += rng.Pick(0, 9);
            const std::int32_t minute = static_cast<std::int32_t>(rng.Pick(0, 1439));
            const std::int32_t departure = rng.Pick(0, 3) == 0 ? static_cast<std::int32_t>(rng.Pick(0, 1440)) : AttendanceStaysUntilDismissal;
            const int kind = static_cast<int>(rng.Pick(0, 3));
            const GameInstant instant = AttendanceAtMinute(day, minute);
            const AttendanceRecord before = a;
            const AttendanceResult r = kind == 0   ? RecordAttendanceAbsence(a, s, instant)
                                       : kind == 1 ? CloseAttendanceBefore(a, s, instant)
                                                   : RecordAttendanceArrival(a, s, instant, departure);
            Check(r.Ok || a == before, "a refused call leaves the record untouched");
            Check(ValidateAttendanceRecord(a).empty(), "the record stays consistent");
            Check(r.Ok || !r.Error.empty(), "a refusal says why");
        }
        // Deterministic replay from the same seed.
        std::int64_t replayDay = b.FirstDay;
        for (int step = 0; step < 40; ++step)
        {
            replayDay += again.Pick(0, 9);
            const std::int32_t minute = static_cast<std::int32_t>(again.Pick(0, 1439));
            const std::int32_t departure = again.Pick(0, 3) == 0 ? static_cast<std::int32_t>(again.Pick(0, 1440)) : AttendanceStaysUntilDismissal;
            const int kind = static_cast<int>(again.Pick(0, 3));
            const GameInstant instant = AttendanceAtMinute(replayDay, minute);
            if (kind == 0)
                (void)RecordAttendanceAbsence(b, s, instant);
            else if (kind == 1)
                (void)CloseAttendanceBefore(b, s, instant);
            else
                (void)RecordAttendanceArrival(b, s, instant, departure);
        }
        Check(a == b && HashAttendanceRecord(a) == HashAttendanceRecord(b), "the same calls give the same record and hash");
    }
}
} // namespace

void TestLateness()
{
    TestBoundaries();
    TestSweeps();
}

void TestRecord()
{
    TestWeekAndThresholds();
    TestCatchUpAndRefusals();
    TestTrimAndFuzz();
}
