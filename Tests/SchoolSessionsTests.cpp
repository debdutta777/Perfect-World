#include "SchoolSessions.h"

#include "P2CTestSupport.h"

using namespace PerfectWorld;
int main(int argc, char** argv)
{
    P2CExpect(argc == 2, "content root argument");
    const P2CFixture f(argv[1]);
    for (const auto stream : {"science", "commerce", "humanities"})
    {
        const auto cats = f.Cats(stream);
        const auto bindings = f.Bindings(stream);
        SchoolSessionsState state;
        auto day = P2CDayAtBell(cats);
        for (std::int32_t p = 0; p < 3; ++p)
        {
            const auto target = CalendarDayStartInstant(0) + P2CBellMinute(*cats.Day.School, p);
            if (day.Now < target)
            {
                DayCommand pass; pass.Kind = DayCommandKind::Pass; pass.Minutes = target - day.Now;
                P2CExpect(ApplyDayCommand(day, cats.Day, pass).Ok(), "passing period");
            }
            const auto before = state;
            const auto beforeDay = day;
            const auto opened = OpenSchoolLesson(state, day, cats, bindings[p], SchoolLessonMode::Foreground, ModelGoal::UnderstandModel, 100000, 12);
            if (p == 2)
            {
                P2CExpect(!opened.Ok() && opened.Issue.Code == "school.session.foreground_limit" && state == before && day == beforeDay,
                    "third foreground refuses atomically for all streams");
                break;
            }
            P2CExpect(opened.Ok(), "foreground opens");
            if (p == 0)
            {
                const auto close = CloseSchoolLesson(state, day, cats, opened.Session, true, 1);
                P2CExpect(close.Ok() && state.Periods.back().Cancelled && !state.Periods.back().Completed, "cancel consumes foreground admission");
                P2CExpect(std::none_of(close.Events.begin(), close.Events.end(), [](const auto& e) { return e.Kind == DayLoopEventKind::Completed; }), "cancel has no learning");
            }
            else
            {
                P2CPlayLesson(state, cats, opened.Session);
                // Value copy models the exact state boundary that the codec must preserve.
                auto reload = state; auto reloadDay = day;
                const auto close = CloseSchoolLesson(reload, reloadDay, cats, opened.Session);
                P2CExpect(close.Ok() && close.Events.size() == 1 && !close.Events.front().Credited.empty(), "close emits bounded learning once");
                const auto saved = reload; const auto savedDay = reloadDay;
                P2CExpect(!CloseSchoolLesson(reload, reloadDay, cats, opened.Session).Ok() && reload == saved && reloadDay == savedDay, "duplicate close is atomic");
                state = std::move(reload); day = std::move(reloadDay);
            }
        }
        auto sunday = NewDayLoopState(CalendarDayStartInstant(6) + 600);
        P2CExpect(QuerySchoolPeriod({}, sunday, bindings.front(), cats).Issue.Code == "school.session.closed_day", "weekend unavailable reason");
        P2CExpect(!QuerySchoolPeriod({}, NewDayLoopState(0), bindings.front(), cats).Issue.Ok(), "outside bell cannot fabricate attendance");
    }
    const auto cats = f.Cats();
    for (const auto kind : {SchoolStructure::Model, SchoolStructure::Explain, SchoolStructure::Investigate, SchoolStructure::Coordinate})
    {
        auto binding = f.Bindings().front(); binding.Structure = kind;
        SchoolLessonGoal goal = ModelGoal::UnderstandModel;
        if (kind == SchoolStructure::Explain) { binding.FamilyId = f.Explain.Families.front().Id; goal = ExplainGoal::ArgueClaim; }
        if (kind == SchoolStructure::Investigate) { binding.FamilyId = f.Investigate.Families.front().Id; goal = InvestigateGoal::Inspect; }
        if (kind == SchoolStructure::Coordinate) { binding.FamilyId = f.Coordinate.Families.front().Id; goal = CoordinateGoal::OrganiseWork; }
        SchoolSessionsState state;
        std::int64_t calendarDay = 0;
        // Forty actual foreground exposures per family; no synthesized historical mastery.
        for (int exposure = 0; exposure < 40; ++exposure)
        {
            while (!AttendanceIsSchoolDay(cats.Day.Calendar, calendarDay)) ++calendarDay;
            auto day = P2CDayAtBell(cats, calendarDay++);
            const auto opened = OpenSchoolLesson(state, day, cats, binding, SchoolLessonMode::Foreground, goal, 100000, 17);
            P2CExpect(opened.Ok(), "forty-exposure admission");
            P2CPlayLesson(state, cats, opened.Session);
            P2CExpect(CloseSchoolLesson(state, day, cats, opened.Session).Ok(), "forty-exposure close");
        }
        while (!AttendanceIsSchoolDay(cats.Day.Calendar, calendarDay)) ++calendarDay;
        auto routineDay = P2CDayAtBell(cats, calendarDay);
        auto foregroundDay = routineDay;
        auto foregroundState = state;
        const auto routine = OpenSchoolLesson(state, routineDay, cats, binding, SchoolLessonMode::Routine, goal, 100000, 17);
        P2CExpect(routine.Ok(), "mastery enables routine");
        const auto manual = OpenSchoolLesson(foregroundState, foregroundDay, cats, binding, SchoolLessonMode::Foreground, goal, 100000, 17);
        P2CExpect(manual.Ok(), "manual parity admission"); P2CPlayLesson(foregroundState, cats, manual.Session);
        const auto r = CloseSchoolLesson(state, routineDay, cats, routine.Session);
        const auto m = CloseSchoolLesson(foregroundState, foregroundDay, cats, manual.Session);
        P2CExpect(r.Ok() && m.Ok() && routineDay == foregroundDay && r.Events.front().Credited == m.Events.front().Credited,
            "routine and careful foreground have identical normal learning/condition/time");
    }
    std::cout << "P2C SchoolSessions passed\n";
}
