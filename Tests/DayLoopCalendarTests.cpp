// ROADMAP M3-09 integration: the day loop passes the school calendar to attendance, so a festival day is
// not closed as an absence at the next wake and arriving at school on one is refused.
#include "DayLoopTestSupport.h"

#include "SchoolCalendar.h"

using namespace PerfectWorld;

namespace
{
std::int64_t Day(const char* date)
{
    const std::optional<CalendarDay> day = ParseCalendarDate(date);
    Check(day.has_value(), "the test date exists");
    return day.value_or(0);
}

/** A rested player at 06:00 on the date, with nothing planned. */
DayLoopState MorningOf(const char* date)
{
    DayLoopState state = NewDayLoopState(Day(date) * MinutesPerDay);
    state.Condition.Energy = MeterMax;
    Check(IsDayLoopStateValid(state), "a life can open on any day");
    return state;
}

DayLoopResult Apply(DayLoopState& state, const DayLoopCatalogues& cats, const DayCommand& command, const char* message)
{
    const DayLoopResult result = ApplyDayCommand(state, cats, command);
    if (!result.Ok())
        std::cerr << "refused: " << DayLoopErrorCode(result.Error) << ' ' << result.Detail << '\n';
    Check(result.Ok(), message);
    return result;
}

/** A whole day spent away from school: plan, wait until 22:00, sleep, wake at 06:00 the next morning. */
void SkipDay(DayLoopState& state, const DayLoopCatalogues& cats)
{
    Apply(state, cats, PlanCmd({}), "plan the day");
    Apply(state, cats, PassCmd(960, DayPassKind::Wait), "the day passes");
    Apply(state, cats, SimpleCmd(DayCommandKind::GoToSleep), "sleep");
    Apply(state, cats, SimpleCmd(DayCommandKind::Wake, 480), "wake");
}

void TestFestivalNotAnAbsence(const SchoolCalendar& calendar)
{
    DayLoopCatalogues withCalendar = DayLoopCats();
    withCalendar.Calendar = &calendar;
    const DayLoopCatalogues without = DayLoopCats();

    // Thursday 14 August is missed, then Independence Day (Friday 15 August), then the weekend.
    DayLoopState plain = MorningOf("2042-08-14");
    DayLoopState dated = plain;
    SkipDay(plain, without);
    SkipDay(dated, withCalendar);
    Check(plain == dated && plain.Attendance.AbsentDays == 1 && plain.Attendance.NextDay == Day("2042-08-15"),
          "waking on the holiday morning closes the missed Thursday either way");

    SkipDay(plain, without);
    SkipDay(dated, withCalendar);
    Check(plain.Attendance.AbsentDays == 2 && plain.Attendance.SchoolDays == 2, "without a calendar the festival is an absence at the next wake");
    Check(dated.Attendance.AbsentDays == 1 && dated.Attendance.SchoolDays == 1 && dated.Attendance.NextDay == Day("2042-08-16") &&
              dated.Attendance.Days.size() == 1 && dated.Attendance.Days[0].Day == Day("2042-08-14"),
          "with a calendar Independence Day is not closed as an absence at the next wake");
    Check(IsDayLoopStateValid(dated) && HashDayLoop(dated) != HashDayLoop(plain), "the state stays valid");

    // The weekend is not an absence either; Monday's wake counts nothing new.
    SkipDay(dated, withCalendar);
    SkipDay(dated, withCalendar);
    Check(dated.Now == Day("2042-08-18") * MinutesPerDay && dated.Attendance.AbsentDays == 1 && dated.Attendance.NextDay == Day("2042-08-18"),
          "a weekend adds no absences");

    // A normal school day missed is still an absence at the wake.
    DayLoopState ordinary = MorningOf("2042-09-01");
    SkipDay(ordinary, withCalendar);
    Check(ordinary.Attendance.AbsentDays == 1 && ordinary.Attendance.SchoolDays == 1, "a missed ordinary school day is still an absence");

    // The Puja vacation: waking after the Tuesday of the first vacation week closes nothing.
    DayLoopState vacation = MorningOf("2042-10-28");
    vacation.Attendance = StartAttendanceRecord(Day("2042-10-28"));
    SkipDay(vacation, withCalendar);
    Check(vacation.Attendance.AbsentDays == 0 && vacation.Attendance.SchoolDays == 0 && vacation.Attendance.NextDay == Day("2042-10-29"),
          "a day of the Puja vacation is not an absence");
}

void TestArrivalOnAHoliday(const SchoolCalendar& calendar)
{
    DayLoopCatalogues withCalendar = DayLoopCats();
    withCalendar.Calendar = &calendar;
    const DayLoopCatalogues without = DayLoopCats();

    // 09:50 on Independence Day.
    DayLoopState holiday = MorningOf("2042-08-15");
    Apply(holiday, withCalendar, PlanCmd({}), "plan");
    Apply(holiday, withCalendar, PassCmd(230, DayPassKind::Wait), "wait until 09:50");
    DayLoopState untried = holiday;
    const DayLoopResult refused = ApplyDayCommand(holiday, withCalendar, SimpleCmd(DayCommandKind::ArriveSchool));
    Check(!refused.Ok() && refused.Error == DayLoopError::AttendanceRefused && refused.Detail.find("Independence Day") != std::string::npos &&
              holiday == untried,
          "arriving at school on Independence Day is refused with the holiday named, and changes nothing");
    Check(DayLoopErrorCode(refused.Error) == "dayloop.attendance_refused", "the refusal keeps its stable day loop code");

    // With no calendar the same command is the old behaviour: an on-time day.
    DayLoopState old = untried;
    const DayLoopResult accepted = Apply(old, without, SimpleCmd(DayCommandKind::ArriveSchool), "no calendar, no holiday");
    Check(accepted.Events.front().Attendance == AttendanceOutcome::OnTime && old.Attendance.SchoolDays == 1, "without a calendar the holiday is a school day");

    // The day before, the calendar changes nothing.
    DayLoopState ordinary = MorningOf("2042-08-14");
    Apply(ordinary, withCalendar, PlanCmd({}), "plan");
    Apply(ordinary, withCalendar, PassCmd(230, DayPassKind::Wait), "wait until 09:50");
    const DayLoopResult normal = Apply(ordinary, withCalendar, SimpleCmd(DayCommandKind::ArriveSchool), "an ordinary Thursday");
    Check(normal.Events.front().Attendance == AttendanceOutcome::OnTime && ordinary.Phase == DayPhase::School, "an ordinary day is attended as ever");
}
} // namespace

void TestDayLoopCalendar(const PerfectWorld::SchoolCalendar& calendar)
{
    TestFestivalNotAnAbsence(calendar);
    TestArrivalOnAHoliday(calendar);
}
