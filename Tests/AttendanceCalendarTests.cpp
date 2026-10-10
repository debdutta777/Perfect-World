// ROADMAP M3-09 integration: attendance respects the 2042-43 school calendar (SchoolCalendar.h, "Attendance
// integration"). Holidays, breaks and weekends are never absences; recording on one is refused; no calendar
// means exactly the old Monday to Friday rule.
#include "AttendanceTestSupport.h"

#include "SchoolCalendar.h"

using namespace PerfectWorld;

namespace
{
const SchoolCalendar* RealCalendar()
{
    static const ContentLoadResult content = LoadContent(RealContentFiles());
    static const SchoolCalendarResult calendars = LoadSchoolCalendars(content.Db);
    Check(content.Ok() && calendars.Ok(), "the real content and calendars load");
    const SchoolCalendar* calendar = calendars.Set.Find("institution.nabapara_vidyalaya", "stage.higher_secondary");
    Check(calendar != nullptr, "the Nabapara Class XI calendar exists");
    return calendar;
}

const BellSchedule& Science()
{
    return *RealSchedules().Find("institution.nabapara_vidyalaya", "science");
}

std::int64_t Day(const char* date)
{
    const std::optional<CalendarDay> day = ParseCalendarDate(date);
    Check(day.has_value(), "the test date exists");
    return day.value_or(0);
}

/** 08:00 on the date, before the 10:00 bell. */
GameInstant Morning(const char* date)
{
    return AttendanceAtMinute(Day(date), 8 * 60);
}

/** 09:50 on the date, ten minutes before the bell. */
GameInstant OnTimeArrival(const char* date)
{
    return AttendanceAtMinute(Day(date), 590);
}

void TestSchoolDayRule()
{
    const SchoolCalendar* calendar = RealCalendar();
    Check(!AttendanceIsSchoolDay(calendar, Day("2042-08-15")), "Independence Day is not a school day");
    Check(AttendanceIsSchoolDay(calendar, Day("2042-08-14")) && AttendanceIsSchoolDay(calendar, Day("2042-08-18")), "the days around it are");
    Check(!AttendanceIsSchoolDay(calendar, Day("2042-10-28")), "the Puja vacation is not school");
    Check(!AttendanceIsSchoolDay(calendar, Day("2042-08-16")), "a Saturday is not school");
    Check(!AttendanceIsSchoolDay(calendar, Day("2043-03-02")), "a weekday after the year is not school");
    for (std::int64_t day = -10; day < 400; ++day)
    {
        Check(AttendanceIsSchoolDay(nullptr, day) == AttendanceIsSchoolDay(day), "no calendar is the Monday to Friday rule");
        Check(AttendanceIsSchoolDay(calendar, day) == CalendarIsSchoolDay(*calendar, day), "a calendar is the calendar's answer");
        Check(!AttendanceIsSchoolDay(calendar, day) || AttendanceIsSchoolDay(day), "a calendar only removes weekdays from the old rule");
    }
}

void TestClosedDays()
{
    const SchoolCalendar* calendar = RealCalendar();
    const BellSchedule& s = Science();

    // Independence Day (Friday): Thursday 14 August is missed, Friday 15 August is a holiday.
    AttendanceRecord plain = StartAttendanceRecord(Day("2042-08-14"));
    AttendanceRecord withCalendar = plain;
    Check(CloseAttendanceBefore(plain, s, Morning("2042-08-18")).DaysClosedAbsent == 2, "without a calendar the holiday is closed as an absence");
    const AttendanceResult closed = CloseAttendanceBefore(withCalendar, s, Morning("2042-08-18"), calendar);
    Check(closed.Ok && closed.DaysClosedAbsent == 1 && withCalendar.AbsentDays == 1 && withCalendar.NextDay == Day("2042-08-18"),
          "with a calendar only Thursday is an absence; Independence Day is not");
    Check(withCalendar.Days.size() == 1 && withCalendar.Days[0].Day == Day("2042-08-14") && ValidateAttendanceRecord(withCalendar).empty(),
          "the record holds Thursday alone and is consistent");

    // Arriving on the Monday after closes the same way.
    AttendanceRecord arrive = StartAttendanceRecord(Day("2042-08-14"));
    const AttendanceResult monday = RecordAttendanceArrival(arrive, s, OnTimeArrival("2042-08-18"), AttendanceStaysUntilDismissal, calendar);
    Check(monday.Ok && monday.DaysClosedAbsent == 1 && arrive.SchoolDays == 2 && arrive.AbsentDays == 1 && arrive.OnTimeDays == 1,
          "arriving after the holiday closes only Thursday");

    // The Puja vacation: five school days in the week of 13 October, none in the three weeks that follow.
    AttendanceRecord vacationPlain = StartAttendanceRecord(Day("2042-10-13"));
    AttendanceRecord vacation = vacationPlain;
    Check(CloseAttendanceBefore(vacationPlain, s, Morning("2042-11-03")).DaysClosedAbsent == 15, "without a calendar the vacation weeks are absences");
    Check(CloseAttendanceBefore(vacation, s, Morning("2042-11-03"), calendar).DaysClosedAbsent == 5 && vacation.AbsentDays == 5,
          "with a calendar only the five days before the vacation are absences");
    Check(RecordAttendanceArrival(vacation, s, OnTimeArrival("2042-11-03"), AttendanceStaysUntilDismissal, calendar).Ok &&
              vacation.SchoolDays == 6 && vacation.NextDay == Day("2042-11-04"),
          "term 2 opens with a normal recorded day");

    // A weekend is not an absence: Friday 22 August is missed, the weekend is not.
    AttendanceRecord weekend = StartAttendanceRecord(Day("2042-08-22"));
    Check(CloseAttendanceBefore(weekend, s, Morning("2042-08-25"), calendar).DaysClosedAbsent == 1, "a missed Friday is one absence, not three");

    // A normal missed school day still is an absence.
    AttendanceRecord normal = StartAttendanceRecord(Day("2042-09-01"));
    Check(CloseAttendanceBefore(normal, s, Morning("2042-09-04"), calendar).DaysClosedAbsent == 3 && normal.AbsentDays == 3,
          "three missed ordinary school days are three absences");

    // Over the whole year the absences are exactly the calendar's school days.
    AttendanceRecord year = StartAttendanceRecord(calendar->FirstDay);
    const AttendanceResult all = CloseAttendanceBefore(year, s, Morning("2043-02-27"), calendar);
    Check(all.Ok && all.DaysClosedAbsent == CalendarCountSchoolDays(*calendar, calendar->FirstDay, calendar->LastDay - 1) &&
              year.AbsentDays == all.DaysClosedAbsent && ValidateAttendanceRecord(year).empty(),
          "a skipped year is absent on exactly the calendar's school days");
    for (const AttendanceDay& entry : year.Days)
        Check(CalendarIsSchoolDay(*calendar, entry.Day), "every absence is a calendar school day");
}

void TestRefusals()
{
    const SchoolCalendar* calendar = RealCalendar();
    const BellSchedule& s = Science();
    AttendanceRecord record = StartAttendanceRecord(Day("2042-08-11"));
    Check(RecordAttendanceArrival(record, s, OnTimeArrival("2042-08-12"), AttendanceStaysUntilDismissal, calendar).Ok, "an ordinary day is recorded");
    const AttendanceRecord frozen = record;

    const AttendanceResult holiday = RecordAttendanceArrival(record, s, OnTimeArrival("2042-08-15"), AttendanceStaysUntilDismissal, calendar);
    Check(!holiday.Ok && holiday.Code == AttendanceNotSchoolDayCode && holiday.Code == "attendance.not_school_day" &&
              holiday.Error.find("Independence Day") != std::string::npos && record == frozen,
          "arriving on Independence Day is refused by code and name, and changes nothing");
    const AttendanceResult holidayAbsence = RecordAttendanceAbsence(record, s, OnTimeArrival("2042-08-15"), calendar);
    Check(!holidayAbsence.Ok && holidayAbsence.Code == AttendanceNotSchoolDayCode && record == frozen, "an absence on a holiday is refused too");
    const AttendanceResult vacation = RecordAttendanceArrival(record, s, OnTimeArrival("2042-10-28"), AttendanceStaysUntilDismissal, calendar);
    Check(!vacation.Ok && vacation.Error.find("Puja vacation") != std::string::npos && record == frozen, "a day in the vacation is refused and named");
    const AttendanceResult saturday = RecordAttendanceArrival(record, s, OnTimeArrival("2042-08-16"), AttendanceStaysUntilDismissal, calendar);
    Check(!saturday.Ok && saturday.Code == AttendanceNotSchoolDayCode && record == frozen, "a Saturday is refused with the same code");
    const AttendanceResult outside = RecordAttendanceArrival(record, s, OnTimeArrival("2043-03-02"), AttendanceStaysUntilDismissal, calendar);
    Check(!outside.Ok && outside.Code == AttendanceNotSchoolDayCode && record == frozen, "a day after the year is refused");
    const AttendanceResult closeOnly = CloseAttendanceBefore(record, s, Morning("2042-08-15"), calendar);
    Check(closeOnly.Ok && closeOnly.DaysClosedAbsent == 2 && closeOnly.Code.empty(), "closing up to a holiday morning succeeds");

    // Without a calendar the rule and the text are as before: the holiday records, a weekend says what it always said.
    AttendanceRecord old = frozen;
    const AttendanceResult noCalendar = RecordAttendanceArrival(old, s, OnTimeArrival("2042-08-15"));
    Check(noCalendar.Ok && noCalendar.Code.empty() && noCalendar.DaysClosedAbsent == 2, "no calendar: Independence Day is an ordinary day");
    const AttendanceResult plainSaturday = RecordAttendanceArrival(old, s, OnTimeArrival("2042-08-16"));
    Check(!plainSaturday.Ok && plainSaturday.Error == "that day is not a school day" && plainSaturday.Code == AttendanceNotSchoolDayCode,
          "no calendar: a Saturday is refused with the original text");
}

void TestSaveFormat()
{
    const SchoolCalendar* calendar = RealCalendar();
    const BellSchedule& s = Science();
    // 1-5 September 2042 has no closure: the calendar changes nothing, down to the byte.
    AttendanceRecord plain = StartAttendanceRecord(Day("2042-09-01"));
    AttendanceRecord withCalendar = plain;
    const char* dates[] = {"2042-09-01", "2042-09-02", "2042-09-04", "2042-09-05"};
    for (const char* date : dates)
    {
        const GameInstant when = AttendanceAtMinute(Day(date), 603);
        Check(RecordAttendanceArrival(plain, s, when).Ok, "recorded without a calendar");
        Check(RecordAttendanceArrival(withCalendar, s, when, AttendanceStaysUntilDismissal, calendar).Ok, "recorded with a calendar");
    }
    Check(plain == withCalendar && SerialiseAttendanceRecord(plain) == SerialiseAttendanceRecord(withCalendar) &&
              SerialiseAttendanceRecord(plain).starts_with("pw.attendance 1\nrange "),
          "the same days give byte-identical text with and without a calendar");

    // A record that skipped a holiday still round-trips through the unchanged format.
    AttendanceRecord skipped = StartAttendanceRecord(Day("2042-08-14"));
    Check(RecordAttendanceArrival(skipped, s, OnTimeArrival("2042-08-18"), AttendanceStaysUntilDismissal, calendar).Ok, "a record that skips a holiday");
    const std::optional<AttendanceRecord> parsed = ParseAttendanceRecord(SerialiseAttendanceRecord(skipped));
    Check(parsed.has_value() && *parsed == skipped && ValidateAttendanceRecord(skipped).empty(), "it saves and loads unchanged");
}
} // namespace

void TestAttendanceCalendar()
{
    TestSchoolDayRule();
    TestClosedDays();
    TestRefusals();
    TestSaveFormat();
}
