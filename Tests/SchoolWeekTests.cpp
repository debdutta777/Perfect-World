#include "SchoolWeek.h"

#include "P2CTestSupport.h"
#include "WorldReducer.h"

using namespace PerfectWorld;
int main(int argc, char** argv)
{
    P2CExpect(argc == 2, "content root"); const P2CFixture f(argv[1]);
    const auto cats = f.Cats();
    SchoolWeekState standing;
    SchoolStandingOrder order; order.Id = "order.school";
    P2CExpect(SetSchoolStandingOrder(standing, order).Ok(), "standing order stored");
    auto overlapping = order; overlapping.Id = "order.overlap";
    P2CExpect(!SetSchoolStandingOrder(standing, overlapping).Ok() && standing.Orders.size() == 1, "overlapping weekday orders refuse");
    WeekPlan base; base.Days.resize(7);
    WeekEntry endWeek; endWeek.Kind = WeekEntryKind::Spend; endWeek.StartMinute = 1439; endWeek.Minutes = 1;
    base.Days.back().Entries.push_back(endWeek);
    auto world = MakeWorldState(14);
    const auto first = BuildSchoolWeekPlan(world, standing, {}, base, f.Bindings(), cats);
    P2CExpect(first.Ok() && CompressSchoolWeek(world, {}, first.Plan, cats).Stop == WeekStop::FirstNeedsPlay, "new lessons interrupt even attendance orders");
    SchoolSessionsState known;
    auto firstDay = P2CDayAtBell(cats);
    for (const auto& binding : f.Bindings())
    {
        const auto target = CalendarDayStartInstant(0) + P2CBellMinute(*cats.Day.School, binding.Period);
        if (firstDay.Now < target)
        {
            DayCommand pass; pass.Kind = DayCommandKind::Pass; pass.Minutes = target - firstDay.Now;
            P2CExpect(ApplyDayCommand(firstDay, cats.Day, pass).Ok(), "first school day passing time");
        }
        const auto opened = OpenSchoolLesson(known, firstDay, cats, binding, SchoolLessonMode::Attend, ModelGoal::UnderstandModel, 0, 14);
        P2CExpect(opened.Ok() && CloseSchoolLesson(known, firstDay, cats, opened.Session).Ok(), "first lesson played before compression");
    }
    world.DayLoop = firstDay;
    P2CExpect(!world.Clock.AdvanceTo(firstDay.Now).Rejected, "align test baseline clock");
    WorldSim nextWeek(world, MaxEventsPerAdvance, cats.Day);
    P2CExpect(nextWeek.Submit({CommandSource::System, AdvanceTime{CalendarDayStartInstant(7) + 360 - firstDay.Now}}).Accepted, "advance to next week");
    world = nextWeek.State();
    const auto built = BuildSchoolWeekPlan(world, standing, known, base, f.Bindings(), cats);
    P2CExpect(built.Ok() && built.Plan.Lessons.size() == static_cast<std::size_t>(5 * BellPeriodCount(*cats.Day.School)), "actual weekday bells expanded");
    const auto compressed = CompressSchoolWeek(world, known, built.Plan, cats);
    if (!compressed.Ok()) std::cerr << compressed.Issue.Code << ' ' << compressed.Issue.Detail << '\n';
    P2CExpect(compressed.Ok() && compressed.Stop == WeekStop::None && compressed.Sessions.Periods.size() == known.Periods.size() + built.Plan.Lessons.size(), "school week resolves through existing owners");
    P2CExpect(compressed.ActivityEvents.size() == built.Plan.Lessons.size(), "raw per-instance events for R9; no aggregate double-credit");
    const auto fine = PlayWeekStepByStep(world, built.Plan.Week, cats.Day);
    P2CExpect(fine.Ok(), "existing fine-grained reference resolves week");
    auto fineDay = fine.World.DayLoop;
    fineDay.Seq = compressed.World.DayLoop.Seq; // documented command-count exclusion only
    P2CExpect(fineDay == compressed.World.DayLoop && fine.World.Clock.Now() == compressed.World.Clock.Now(),
        "first week agrees with quarter-hour reference on every DayLoop field except command count");
    auto manualWorld = world; auto manualSessions = known;
    // Scene-by-scene reference: feed each authored arrival/wait through existing
    // WeekCompression and every lesson through the frozen school session commands.
    for (std::size_t d = 0; d < built.Plan.Week.Days.size(); ++d)
    for (std::size_t e = 0; e < built.Plan.Week.Days[d].Entries.size(); ++e)
    {
        SchoolWeekPlan one;
        const auto target = GameClock::StartOfDay(world.Clock.Now()) + static_cast<GameInstant>(d) * MinutesPerDay;
        const auto offset = GameClock::DayNumber(target) - GameClock::DayNumber(manualWorld.Clock.Now());
        one.Week.Days.resize(static_cast<std::size_t>(offset) + 1);
        one.Week.Days.back().Entries.push_back(built.Plan.Week.Days[d].Entries[e]);
        for (const auto& lesson : built.Plan.Lessons)
            if (lesson.Day == static_cast<std::int32_t>(d) && lesson.Entry == static_cast<std::int32_t>(e))
                one.Lessons.push_back({static_cast<std::int32_t>(offset), 0, lesson.Binding, lesson.Mode});
        const auto step = CompressSchoolWeek(manualWorld, manualSessions, one, cats);
        P2CExpect(step.Ok(), "one commitment at a time"); manualWorld = step.World; manualSessions = step.Sessions;
    }
    P2CExpect(manualWorld.DayLoop == compressed.World.DayLoop && manualSessions == compressed.Sessions &&
        manualWorld.Clock.Now() == compressed.World.Clock.Now(), "manual/compressed school state parity");
    // A second week starts from the first week's exact persistent output.
    const auto secondPlan = BuildSchoolWeekPlan(compressed.World, standing, compressed.Sessions, base, f.Bindings(), cats);
    P2CExpect(secondPlan.Ok(), "second week plan");
    const auto second = CompressSchoolWeek(compressed.World, compressed.Sessions, secondPlan.Plan, cats);
    P2CExpect(second.Ok() && second.Sessions.Periods.size() > compressed.Sessions.Periods.size(), "two consecutive weeks resolve");
    const auto fineSecond = PlayWeekStepByStep(compressed.World, secondPlan.Plan.Week, cats.Day);
    auto fineSecondDay = fineSecond.World.DayLoop;
    fineSecondDay.Seq = second.World.DayLoop.Seq;
    P2CExpect(fineSecond.Ok() && fineSecondDay == second.World.DayLoop && fineSecond.World.Clock.Now() == second.World.Clock.Now(),
        "second week agrees with quarter-hour reference");
    standing.SceneDays = {7};
    const auto nominated = BuildSchoolWeekPlan(world, standing, known, base, f.Bindings(), cats);
    P2CExpect(nominated.Ok(), "nominated plan");
    const auto stop = CompressSchoolWeek(world, known, nominated.Plan, cats);
    P2CExpect(stop.Ok() && stop.Stop == WeekStop::FirstNeedsPlay && stop.World.Clock.Now() == world.Clock.Now() && stop.Segments.empty(), "nominated first costs nothing");
    standing.SceneDays.clear(); order.LessonMode = SchoolLessonMode::Routine;
    P2CExpect(SetSchoolStandingOrder(standing, order).Ok(), "routine order");
    const auto routine = BuildSchoolWeekPlan(world, standing, {}, base, f.Bindings(), cats);
    P2CExpect(routine.Ok() && CompressSchoolWeek(world, {}, routine.Plan, cats).Stop == WeekStop::FirstNeedsPlay, "unmastered first interrupts routine compression");
    auto missing = built.Plan; missing.Lessons.clear();
    const auto failed = CompressSchoolWeek(world, known, missing, cats);
    P2CExpect(!failed.Ok() && failed.World.DayLoop == world.DayLoop && failed.Sessions == known, "cannot bypass exactly-once lesson ownership");
    std::cout << "P2C SchoolWeek passed\n";
}
