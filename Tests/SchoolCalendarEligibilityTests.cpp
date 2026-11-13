#include "SchoolCalendar.h"

#include "P2CTestSupport.h"
#include "Commerce.h"
#include "DayLoop.h"
#include "SchoolWeek.h"
#include "WorldState.h"

#include <utility>

using namespace PerfectWorld;

namespace
{
void P33Expect(bool condition, std::string_view message)
{
    if (!condition)
    {
        std::cerr << "P33 failure: " << message << '\n';
        std::exit(1);
    }
}

CalendarDay P33Date(std::string_view text)
{
    const auto day = ParseCalendarDate(text);
    P33Expect(day.has_value(), "test date exists");
    return *day;
}

void P33Eligibility(const SchoolCalendar& calendar, CalendarDay day, bool attendance, bool periods)
{
    P33Expect(CalendarRequiresAttendance(calendar, day) == attendance, "attendance eligibility");
    P33Expect(CalendarHasOrdinaryPeriods(calendar, day) == periods, "ordinary-period eligibility");
    P33Expect(CalendarIsSchoolDay(calendar, day) == attendance, "school-day compatibility query");
    P33Expect(AttendanceIsSchoolDay(&calendar, day) == attendance, "attendance consumer");
}

void P33DayKinds(const SchoolCalendar& calendar)
{
    struct P33Case
    {
        std::string_view Date;
        SchoolDayKind Kind;
        bool Attendance;
        bool Periods;
    };
    const P33Case cases[] = {
        {"2042-06-01", SchoolDayKind::OutsideYear, false, false},
        {"2042-06-02", SchoolDayKind::SchoolDay, true, true},
        {"2042-06-07", SchoolDayKind::NonSchoolWeekday, false, false},
        {"2042-08-15", SchoolDayKind::Holiday, false, false},
        {"2042-07-28", SchoolDayKind::SchoolDay, true, false},
        {"2042-08-01", SchoolDayKind::SchoolDay, true, false},
        {"2042-08-04", SchoolDayKind::SchoolDay, true, true},
        {"2042-09-20", SchoolDayKind::NonSchoolWeekday, false, false},
        {"2042-10-28", SchoolDayKind::Break, false, false},
        {"2042-12-05", SchoolDayKind::SchoolDay, true, false},
        {"2042-12-08", SchoolDayKind::SchoolDay, true, false},
        {"2042-12-12", SchoolDayKind::SchoolDay, true, false},
        {"2042-12-15", SchoolDayKind::SchoolDay, true, true},
        {"2042-12-24", SchoolDayKind::Break, false, false},
        {"2043-01-12", SchoolDayKind::SchoolDay, true, false},
        {"2043-01-16", SchoolDayKind::SchoolDay, true, false},
        {"2043-01-27", SchoolDayKind::Break, false, false},
        {"2043-02-02", SchoolDayKind::Break, false, false},
        {"2043-02-03", SchoolDayKind::SchoolDay, true, true},
        {"2043-02-27", SchoolDayKind::SchoolDay, true, true},
        {"2043-03-02", SchoolDayKind::OutsideYear, false, false},
    };
    for (const auto& sample : cases)
    {
        const auto day = P33Date(sample.Date);
        P33Expect(ClassifyCalendarDay(calendar, day) == sample.Kind, "day classification stays compatible");
        P33Eligibility(calendar, day, sample.Attendance, sample.Periods);
    }
    auto gap = calendar;
    gap.Terms.clear();
    P33Expect(ClassifyCalendarDay(gap, 0) == SchoolDayKind::OutsideTerm, "non-term weekday");
    P33Eligibility(gap, 0, false, false);

    auto overlap = calendar;
    const auto sports = P33Date("2042-12-05");
    CalendarHoliday closure;
    closure.First = sports;
    closure.Last = sports;
    overlap.Holidays.push_back(closure);
    P33Eligibility(overlap, sports, false, false);
    overlap.Holidays.pop_back();
    CalendarAssessment assessment;
    assessment.First = sports;
    assessment.Last = sports;
    overlap.Assessments.push_back(assessment);
    P33Eligibility(overlap, sports, true, false);
    overlap.Breaks.push_back(closure);
    P33Eligibility(overlap, sports, false, false);
}

template <typename Span>
bool P33Contains(const std::vector<Span>& spans, CalendarDay day)
{
    return std::any_of(spans.begin(), spans.end(), [&](const auto& span)
        { return span.First <= day && day <= span.Last; });
}

WorldState P33WorldAt(CalendarDay day, const BellSchedule& school)
{
    auto world = MakeWorldState(33);
    const auto at = CalendarDayStartInstant(day) + BellFirstBell(school);
    P33Expect(!world.Clock.AdvanceTo(at).Rejected, "test clock aligns with school morning");
    world.DayLoop = NewDayLoopState(at);
    const auto settled = SettleAllowance(world.Commerce, world.Commerce.AllowanceSettledThrough, at);
    P33Expect(settled.Ok(), "test allowance cursor aligns with clock");
    world.Commerce = settled.State;
    P33Expect(IsWorldDayInStep(world), "test world is in step");
    return world;
}

void P33PassTo(DayLoopState& day, const DayLoopCatalogues& cats, GameInstant target)
{
    if (day.Now == target) return;
    DayCommand pass;
    pass.Kind = DayCommandKind::Pass;
    pass.Minutes = target - day.Now;
    P33Expect(ApplyDayCommand(day, cats, pass).Ok(), "manual wait reaches next bell");
}

void P33WeekParity(CalendarDay first, std::string_view stream, const SchoolWeekResult& compressed,
    const WeekResult& fine)
{
    auto fineDay = fine.World.DayLoop;
    const auto& schoolDay = compressed.World.DayLoop;
    fineDay.Seq = schoolDay.Seq; // Pass granularity and segment planning change command count only.
    const std::pair<std::string_view, bool> fields[] = {
        {"Condition", fineDay.Condition == schoolDay.Condition},
        {"Runner", fineDay.Runner == schoolDay.Runner},
        {"Practice", fineDay.Practice == schoolDay.Practice},
        {"Receipts", fineDay.Receipts == schoolDay.Receipts},
        {"Attendance", fineDay.Attendance == schoolDay.Attendance},
        {"Now", fineDay.Now == schoolDay.Now},
        {"Phase", fineDay.Phase == schoolDay.Phase},
        {"Wakes", fineDay.Wakes == schoolDay.Wakes},
        {"Planned", fineDay.Planned == schoolDay.Planned},
        {"Plan", fineDay.Plan == schoolDay.Plan},
        {"Asleep", fineDay.Asleep == schoolDay.Asleep},
        {"SleptAt", fineDay.SleptAt == schoolDay.SleptAt},
        {"SchoolUntil", fineDay.SchoolUntil == schoolDay.SchoolUntil},
        {"HasCommitment", fineDay.HasCommitment == schoolDay.HasCommitment},
        {"Commitment", fineDay.Commitment == schoolDay.Commitment},
        {"HasLastAction", fineDay.HasLastAction == schoolDay.HasLastAction},
        {"Last", fineDay.Last == schoolDay.Last},
        {"Clock.Now", fine.World.Clock.Now() == compressed.World.Clock.Now()},
    };
    for (const auto& [field, equal] : fields)
    {
        if (equal) continue;
        std::cerr << "P33 week start=" << FormatCalendarDate(first) << " day=" << first
            << " stream=" << stream << " differs: " << field << '\n';
        if (field == "Plan")
            std::cerr << "  manual items=" << fineDay.Plan.size() << " compressed items=" << schoolDay.Plan.size() << '\n';
        P33Expect(false, "full calendar weeks agree on every day-loop field except command count");
    }
    // Retain the aggregate comparison so future state fields cannot escape this test.
    if (!(fineDay == schoolDay))
        std::cerr << "P33 week start=" << FormatCalendarDate(first) << " day=" << first
            << " stream=" << stream << " differs: unlisted DayLoop field\n";
    P33Expect(fineDay == schoolDay, "full calendar weeks agree on every day-loop field except command count");
}

void P33WholeCalendar(const P2CFixture& fixture, std::string_view stream)
{
    const auto cats = fixture.Cats(stream);
    const auto& calendar = *cats.Day.Calendar;
    const auto& school = *cats.Day.School;
    const auto bindings = fixture.Bindings(stream);
    SchoolWeekState standing;
    SchoolStandingOrder order;
    order.Id = "order.p33_school";
    P33Expect(SetSchoolStandingOrder(standing, order).Ok(), "attendance standing order accepted");
    WeekPlan base;
    base.Days.resize(1);
    WeekEntry finish;
    finish.Kind = WeekEntryKind::Spend;
    finish.StartMinute = school.Blocks.back().EndMinute;
    finish.Minutes = 1;
    base.Days.front().Entries.push_back(finish);
    SchoolSessionsState known;
    for (CalendarDay day = calendar.FirstDay; day <= calendar.LastDay; ++day)
    {
        // Independent oracle from the authored ranges, including assessment weekends.
        const auto weekday = static_cast<unsigned>((day + 1) % 7);
        const bool attendance = ((calendar.SchoolWeekdays >> weekday) & 1u) != 0 &&
            P33Contains(calendar.Terms, day) && !P33Contains(calendar.Breaks, day) &&
            !P33Contains(calendar.Holidays, day);
        const bool sports = std::any_of(calendar.Events.begin(), calendar.Events.end(), [&](const auto& event)
            { return event.Kind == "sports_day" && event.First <= day && day <= event.Last; });
        const bool periods = attendance && !sports && !P33Contains(calendar.Assessments, day);
        P33Eligibility(calendar, day, attendance, periods);
        const auto world = P33WorldAt(day, school);
        const auto built = BuildSchoolWeekPlan(world, standing, known, base, bindings, cats);
        P33Expect(built.Ok(), "calendar day expands into a valid plan");
        P33Expect(built.Plan.Lessons.size() == (periods ? bindings.size() : 0), "only ordinary dates expand lessons");
        const auto arrivals = std::count_if(built.Plan.Week.Days.front().Entries.begin(),
            built.Plan.Week.Days.front().Entries.end(), [](const auto& entry) { return entry.Kind == WeekEntryKind::School; });
        P33Expect(arrivals == (attendance ? 1 : 0), "replacement dates retain arrival; closures omit it");
        const auto compressed = CompressSchoolWeek(world, known, built.Plan, cats);
        P33Expect(compressed.Ok(), "compressed calendar day resolves or requests first play");
        auto manualDay = world.DayLoop;
        auto manualSessions = known;
        DayCommand command;
        command.Kind = DayCommandKind::PlanDay;
        P33Expect(ApplyDayCommand(manualDay, cats.Day, command).Ok(), "manual day planned");
        if (attendance)
        {
            command.Kind = DayCommandKind::ArriveSchool;
            P33Expect(ApplyDayCommand(manualDay, cats.Day, command).Ok(), "manual attendance recorded");
        }
        for (const auto& binding : bindings)
        {
            P33PassTo(manualDay, cats.Day, CalendarDayStartInstant(day) + P2CBellMinute(school, binding.Period));
            const auto view = QuerySchoolPeriod(manualSessions, manualDay, binding, cats);
            P33Expect(view.Issue.Ok() == periods && view.AttendAvailable == periods, "manual query matches compressed eligibility");
            const auto beforeDay = manualDay;
            const auto beforeSessions = manualSessions;
            const auto opened = OpenSchoolLesson(manualSessions, manualDay, cats, binding,
                SchoolLessonMode::Attend, ModelGoal::UnderstandModel, 0, 33);
            P33Expect(opened.Ok() == periods, "manual open matches compressed eligibility");
            if (periods)
                P33Expect(CloseSchoolLesson(manualSessions, manualDay, cats, opened.Session).Ok(), "ordinary manual lesson completes");
            else
                P33Expect(opened.Issue.Code == "school.session.closed_day" && manualDay == beforeDay &&
                    manualSessions == beforeSessions, "replacement and closed dates refuse atomically");
        }
        P33PassTo(manualDay, cats.Day, CalendarDayStartInstant(day) + finish.StartMinute + finish.Minutes);
        if (day == calendar.FirstDay)
        {
            P33Expect(periods && compressed.Stop == WeekStop::FirstNeedsPlay && compressed.Lessons.empty(),
                "first ordinary day requests the same lesson that manual play admits");
            known = manualSessions; // Genuine first-day completions make subsequent dates compressible.
            continue;
        }
        P33Expect(compressed.Stop == WeekStop::None, "familiar ordinary lessons and replacement days compress");
        P33Expect(compressed.Lessons.size() == (periods ? bindings.size() : 0) &&
            compressed.ActivityEvents.size() == compressed.Lessons.size(), "replacement days grant no ordinary lesson credit");
        P33Expect(manualSessions == compressed.Sessions && manualDay.Attendance == compressed.World.DayLoop.Attendance,
            "manual and compressed lessons and attendance agree over the full calendar");
        P33Expect(manualDay.Now == compressed.World.Clock.Now(), "manual and compressed day end at the same instant");
    }
    // Full seven-day plans cross assessment/closure boundaries as the actual week entry point does.
    for (CalendarDay first = calendar.FirstDay + WeekMaxDays; first <= calendar.LastDay; first += WeekMaxDays)
    {
        const auto world = P33WorldAt(first, school);
        WeekPlan week;
        week.Days.resize(static_cast<std::size_t>(std::min<CalendarDay>(WeekMaxDays, calendar.LastDay - first + 1)));
        week.Days.back().Entries.push_back(finish);
        const auto built = BuildSchoolWeekPlan(world, standing, known, week, bindings, cats);
        P33Expect(built.Ok(), "calendar week expands across replacement and closure boundaries");
        const auto compressed = CompressSchoolWeek(world, known, built.Plan, cats);
        P33Expect(compressed.Ok() && compressed.Stop == WeekStop::None, "calendar week compresses through actual lesson owner");
        const auto fine = PlayWeekStepByStep(world, built.Plan.Week, cats.Day);
        P33Expect(fine.Ok(), "calendar week resolves through fine-grained day commands");
        P33WeekParity(first, stream, compressed, fine);
        for (const auto& entry : compressed.World.DayLoop.Attendance.Days)
            P33Expect(CalendarRequiresAttendance(calendar, entry.Day), "compressed week never invents a closed-date absence");
    }
    // Ending on an ordinary school day must retain its intentions. A Sunday-only
    // final spend used to hide the empty plans produced by one-entry segments.
    for (const std::size_t days : {std::size_t{1}, std::size_t{5}})
    {
        const auto first = calendar.FirstDay + WeekMaxDays;
        const auto world = P33WorldAt(first, school);
        WeekPlan week;
        week.Days.resize(days);
        week.Days.back().Entries.push_back(finish);
        const auto built = BuildSchoolWeekPlan(world, standing, known, week, bindings, cats);
        P33Expect(built.Ok(), "short school week builds");
        const auto compressed = CompressSchoolWeek(world, known, built.Plan, cats);
        const auto fine = PlayWeekStepByStep(world, built.Plan.Week, cats.Day);
        P33Expect(compressed.Ok() && compressed.Stop == WeekStop::None && fine.Ok(), "short school week resolves");
        P33WeekParity(first, stream, compressed, fine);
        P33Expect(compressed.World.DayLoop.Plan.size() == bindings.size(), "school-day ending keeps all morning intentions");
    }
}

void P33ClosedAttendance(const SchoolSessionCatalogues& cats)
{
    const auto& calendar = *cats.Day.Calendar;
    const auto& school = *cats.Day.School;
    auto year = StartAttendanceRecord(calendar.FirstDay);
    const auto closed = CloseAttendanceBefore(year, school, CalendarDayStartInstant(calendar.LastDay + 1), &calendar);
    P33Expect(closed.Ok && year.AbsentDays == CalendarCountSchoolDays(calendar, calendar.FirstDay, calendar.LastDay),
        "skipping the whole calendar counts exactly attendance-required dates");
    P33Expect(ValidateAttendanceRecord(year).empty(), "attendance accounting stays valid");
    for (const auto& entry : year.Days)
        P33Expect(CalendarRequiresAttendance(calendar, entry.Day), "no absence on a non-attendance date");
    for (CalendarDay day = calendar.FirstDay; day <= calendar.LastDay; ++day)
    {
        if (CalendarRequiresAttendance(calendar, day)) continue;
        auto record = StartAttendanceRecord(day);
        const auto before = record;
        const auto at = CalendarDayStartInstant(day) + BellFirstBell(school);
        const auto absence = RecordAttendanceAbsence(record, school, at, &calendar);
        const auto arrival = RecordAttendanceArrival(record, school, at, AttendanceStaysUntilDismissal, &calendar);
        P33Expect(!absence.Ok && !arrival.Ok && absence.Code == AttendanceNotSchoolDayCode &&
            arrival.Code == AttendanceNotSchoolDayCode && record == before, "closed-date recording refuses without mutation");
        const auto caughtUp = CloseAttendanceBefore(record, school, CalendarDayStartInstant(day + 1), &calendar);
        P33Expect(caughtUp.Ok && caughtUp.DaysClosedAbsent == 0 && record.AbsentDays == 0, "closed-date catch-up creates no absence");
        auto sleeping = NewDayLoopState(CalendarDayStartInstant(day) + 18 * 60);
        DayCommand command;
        command.Kind = DayCommandKind::PlanDay;
        P33Expect(ApplyDayCommand(sleeping, cats.Day, command).Ok(), "closed-date evening planned");
        command.Kind = DayCommandKind::GoToSleep;
        P33Expect(ApplyDayCommand(sleeping, cats.Day, command).Ok(), "closed-date sleep begins");
        command.Kind = DayCommandKind::Wake;
        command.Minutes = 12 * 60;
        P33Expect(ApplyDayCommand(sleeping, cats.Day, command).Ok() && sleeping.Attendance.AbsentDays == 0 &&
            sleeping.Attendance.Days.empty(), "waking after a closure or preparation day creates no absence");
    }
}

void P33SuppliedLessons(const P2CFixture& fixture)
{
    const auto cats = fixture.Cats();
    const auto binding = fixture.Bindings().front();
    for (const std::string_view date : {"2042-12-05", "2042-07-28", "2042-08-15", "2042-06-07", "2043-01-27"})
    {
        const auto world = P33WorldAt(P33Date(date), *cats.Day.School);
        SchoolWeekPlan supplied;
        supplied.Week.Days.resize(1);
        WeekEntry wait;
        wait.Kind = WeekEntryKind::Spend;
        wait.StartMinute = BellFirstBell(*cats.Day.School);
        wait.Minutes = 1;
        supplied.Week.Days.front().Entries.push_back(wait);
        WeekEntry lesson;
        lesson.ActivityId = AttendanceClassActivityId;
        lesson.PlaceKind = "classroom";
        lesson.StartMinute = P2CBellMinute(*cats.Day.School, binding.Period);
        lesson.FirstOfKind = true;
        supplied.Week.Days.front().Entries.push_back(lesson);
        supplied.Lessons.push_back({0, 1, binding, SchoolLessonMode::Attend});
        const auto refused = CompressSchoolWeek(world, {}, supplied, cats);
        P33Expect(!refused.Ok() && refused.Issue.Code == "school.week.closed_day" && refused.Segments.empty() &&
            refused.World.Clock.Now() == world.Clock.Now() && refused.World.DayLoop == world.DayLoop &&
            refused.Sessions.Periods.empty(), "supplied closed-date lesson refuses before time or first-play interruption");
    }
}
} // namespace

int main(int argc, char** argv)
{
    P33Expect(argc == 2, "content root argument");
    const P2CFixture fixture(argv[1]);
    P33DayKinds(*fixture.Cats().Day.Calendar);
    for (const std::string_view stream : {"science", "commerce", "humanities"})
        P33WholeCalendar(fixture, stream);
    P33ClosedAttendance(fixture.Cats());
    P33SuppliedLessons(fixture);
    std::cout << "P33 school calendar eligibility passed\n";
}
