// S04: the Phase 2 school commands through WorldSim - every command's success and refusal paths, atomic rollback of a late
// refusal, and exactly-once publication of completion learning. Driven only through Submit, over the real content.
#include "WorldReducer.h"

#include "AwakeningInputs.h"
#include "S04SchoolTestSupport.h"

#include <set>

using namespace PerfectWorld;

namespace
{
/** Every completion one life (the S04Week sim) published, in order: its day of lessons, then its compressed week. */
std::vector<DayLoopEvent> S04Published;

void S04Collect(const CommandResult& result)
{
    for (const DayLoopEvent& event : S04Completions(result))
        S04Published.push_back(event);
}

void S04LessonFlow(const S04World& w)
{
    WorldSim bare(0x5304);
    P2CExpect(!bare.Submit({CommandSource::Player, SchoolLessonOpen{w.Binding(0).Id, SchoolLessonMode::Attend, {}}}).Accepted &&
                  bare.Submit({CommandSource::Player, SchoolLessonOpen{w.Binding(0).Id, SchoolLessonMode::Attend, {}}}).Why.Code ==
                      "school.missing_content",
              "a sim without school content refuses lessons");
    WorldSim sim = w.Sim(0x5304);
    const SchoolLessonBinding& first = w.Binding(0);
    const SchoolLessonGoal goal = S04Goal(first.Structure);
    S04Refused(sim, SchoolLessonOpen{first.Id, SchoolLessonMode::Foreground, goal}, "school.session.busy",
               "before the morning plan the period cannot open");
    S04Refused(sim, SchoolLessonOpen{"school.lesson.nope", SchoolLessonMode::Attend, {}}, "school.session.bad_binding",
               "an unknown binding is refused at the command boundary");
    S04Morning(sim, w.Cats);
    S04PassTo(sim, P2CBellMinute(*w.Cats.Day.School, 0));
    S04Refused(sim, SchoolLessonOpen{first.Id, SchoolLessonMode::Routine, goal}, "school.session.not_secure",
               "routine needs existing mastery");

    const GameInstant bell = sim.State().Clock.Now();
    const auto opened = S04Accept(sim, SchoolLessonOpen{first.Id, SchoolLessonMode::Foreground, goal}, "the foreground lesson opens");
    const auto& sessions = sim.State().SchoolSessions;
    P2CExpect(sessions.Active && sessions.Active->Binding == first && sim.State().DayLoop.Runner.Active() &&
                  sim.State().Clock.Now() == bell && opened.Events.empty() && sim.LastSchoolLessonResult() &&
                  sim.LastSchoolLessonResult()->Session == sessions.Active->Session,
              "opening starts the class activity at the bell, free, and publishes its session");
    const std::uint64_t session = sessions.Active->Session;

    S04Refused(sim, AdvanceTime{1}, "school.lesson_open", "free time stops while a lesson is open");
    DayCommand pass;
    pass.Kind = DayCommandKind::Pass;
    pass.Minutes = 5;
    S04Refused(sim, WorldDayCommand{pass}, "school.lesson_open", "the day cannot be driven around an open lesson");
    S04Refused(sim, SchoolPlanDay{}, "school.lesson_open", "planning waits for the lesson");
    S04Refused(sim, SchoolWeekOrder{{"order.school"}, false}, "school.lesson_open", "the week waits for the lesson");
    S04Refused(sim, SchoolWeekResolve{}, "school.lesson_open", "the week cannot resolve over an open lesson");
    S04Refused(sim, SchoolLessonOpen{w.Binding(1).Id, SchoolLessonMode::Attend, {}}, "school.lesson_open", "one lesson at a time");
    S04Refused(sim, SchoolLessonChoose{session + 1, S04Standard(sessions, w.Cats)}, "school.session.stale_session",
               "a stale session id is refused");
    S04Refused(sim, SchoolLessonClose{session, false, 0}, "school.session.unfinished", "an unfinished foreground cannot be recorded");

    S04PlayLesson(sim, w.Cats);
    S04Refused(sim, SchoolLessonClose{session, true, 1}, "school.session.already_resolved",
               "a resolved foreground cannot be left early for free mastery");
    const GameInstant before = sim.State().Clock.Now();
    const std::int32_t duration = w.Cats.Day.Activities->Find(AttendanceClassActivityId)->DurationMinutes;
    const auto closed = S04Accept(sim, SchoolLessonClose{session, false, 0}, "the lesson is recorded");
    const auto completions = S04Completions(closed);
    const auto* advanced = closed.Events.empty() ? nullptr : std::get_if<TimeAdvanced>(&closed.Events.front().Payload);
    const SchoolPeriodRecord& record = sim.State().SchoolSessions.Periods.back();
    P2CExpect(sim.State().Clock.Now() == before + duration && advanced && advanced->From == before && advanced->To == before + duration &&
                  completions.size() == 1 && completions.front().Instance == record.ActivityInstance && record.Completed &&
                  !sim.State().SchoolSessions.Active && IsWorldDayInStep(sim.State()) &&
                  sim.State().Commerce.AllowanceSettledThrough == sim.State().Clock.Now(),
              "recording charges the bell period through the clock once and publishes one completion");
    P2CExpect(sim.LastSchoolLessonResult()->Events.size() == 1, "the published lesson result is the close's");
    S04Refused(sim, SchoolLessonClose{session, false, 0}, "school.session.stale_session", "a duplicate close is refused atomically");

    // Attend in the next period, then leave early: elapsed minutes only, no learning, the period still used.
    S04PassTo(sim, P2CBellMinute(*w.Cats.Day.School, 1));
    const auto attend = S04Accept(sim, SchoolLessonOpen{w.Binding(1).Id, SchoolLessonMode::Attend, {}}, "attend opens");
    const std::uint64_t second = sim.State().SchoolSessions.Active->Session;
    P2CExpect(second == session + 1 && sim.State().SchoolSessions.Active->ReadyToClose, "attend is ready at once");
    const GameInstant left = sim.State().Clock.Now();
    const auto cancelled = S04Accept(sim, SchoolLessonClose{second, true, 10}, "leaving early is accepted");
    P2CExpect(sim.State().Clock.Now() == left + 10 && S04Completions(cancelled).empty() &&
                  sim.State().SchoolSessions.Periods.back().Cancelled,
              "leaving early charges its minutes and earns nothing");
    S04Refused(sim, SchoolLessonOpen{w.Binding(1).Id, SchoolLessonMode::Attend, {}}, "school.session.period_used",
               "a cancelled period is spent");
    (void)attend;
}

/** A late refusal (the charge stopped by the event cap) restores the world exactly. */
void S04Rollback(const S04World& w)
{
    WorldSim sim = w.Sim(0x5305, 2);
    S04Morning(sim, w.Cats);
    S04PassTo(sim, P2CBellMinute(*w.Cats.Day.School, 0));
    const GameInstant now = sim.State().Clock.Now();
    for (int i = 1; i <= 4; ++i)
        S04Accept(sim, ScheduleWorldEvent{now + 5 + i, "test/s04/tick" + std::to_string(i)}, "events inside the period",
                  CommandSource::System);
    S04Accept(sim, SchoolLessonOpen{w.Binding(0).Id, SchoolLessonMode::Attend, {}}, "attend opens");
    const std::uint64_t session = sim.State().SchoolSessions.Active->Session;
    const std::uint64_t seq = sim.State().Seq;
    S04Refused(sim, SchoolLessonClose{session, false, 0}, "school.lesson.charge_interrupted",
               "an event-capped lesson charge refuses and restores the world");
    P2CExpect(sim.State().Seq == seq && sim.State().SchoolSessions.Active && sim.LastSchoolLessonResult()->Events.empty(),
              "nothing of the refused close is published");

    WorldState state = MakeWorldState(0x5306, w.Creator);
    P2CExpect(IssueSchoolWork(state.SchoolWork, w.Brief("school.work.capped", state.Clock.Now()), w.F.Papers, state.Clock.Now()).Ok(),
              "a brief is issued");
    WorldSim paper = w.Continue(state, 2);
    SchoolWorkCommand begin;
    begin.WorkId = "school.work.capped";
    S04Accept(paper, begin, "the paper begins");
    const GameInstant start = paper.State().Clock.Now();
    for (int i = 1; i <= 4; ++i)
        S04Accept(paper, ScheduleWorldEvent{start + 10 + i, "test/s04/paper" + std::to_string(i)}, "events inside the paper",
                  CommandSource::System);
    SchoolWorkCommand submit = begin;
    submit.Kind = SchoolWorkActionKind::Submit;
    S04Refused(paper, submit, "school.work.charge_interrupted", "an event-capped paper charge refuses and restores the world");
    P2CExpect(paper.State().SchoolWork.Records.front().StartedAt == start && !paper.State().SchoolWork.Records.front().SubmittedAt,
              "the paper is still open after the refused hand-in");
}

void S04Work(const S04World& w)
{
    WorldState state = MakeWorldState(0x5307, w.Creator);
    const GameInstant issued = state.Clock.Now();
    for (const char* id : {"school.work.a", "school.work.b"})
        P2CExpect(IssueSchoolWork(state.SchoolWork, w.Brief(id, issued), w.F.Papers, issued).Ok(), "a brief is issued");
    WorldSim sim = w.Continue(state);
    SchoolWorkCommand a;
    a.WorkId = "school.work.a";
    SchoolWorkCommand unknown = a;
    unknown.WorkId = "school.work.none";
    S04Refused(sim, unknown, "school.work.unknown_work", "unknown work is refused");
    SchoolWorkCommand submitEarly = a;
    submitEarly.Kind = SchoolWorkActionKind::Submit;
    P2CExpect(!S04Send(sim, submitEarly).Accepted && sim.State().Clock.Now() == issued, "an unbegun paper cannot be handed in");

    S04Accept(sim, a, "paper a begins");
    P2CExpect(sim.State().SchoolWork.Records[0].StartedAt == issued && sim.State().Clock.Now() == issued, "begin reserves; no time passes");
    S04Refused(sim, AdvanceTime{1}, "school.paper_open", "free time stops while a paper is open");
    S04Refused(sim, ConversationOpen{std::string(HouseholdAdultId), PersonAvailability::Awake}, "school.paper_open",
               "no conversation mid-paper");
    S04Refused(sim, SchoolLessonOpen{w.Binding(0).Id, SchoolLessonMode::Attend, {}}, "school.paper_open", "no lesson mid-paper");
    SchoolWorkCommand b = a;
    b.WorkId = "school.work.b";
    S04Refused(sim, b, "school.work.busy", "one paper at a time");

    SchoolWorkCommand survey = a;
    survey.Kind = SchoolWorkActionKind::Survey;
    S04Accept(sim, survey, "paper a is surveyed");
    P2CExpect(sim.LastSchoolWorkResult()->ChargedUnits > 0 && sim.State().Clock.Now() == issued, "surveying spends units, not world time");
    SchoolWorkCommand submit = a;
    submit.Kind = SchoolWorkActionKind::Submit;
    const auto handed = S04Accept(sim, submit, "paper a is handed in");
    const auto& record = sim.State().SchoolWork.Records[0];
    P2CExpect(sim.State().Clock.Now() == issued + AssessmentPaperMinutes && record.SubmittedAt == issued + AssessmentPaperMinutes &&
                  sim.LastSchoolWorkResult()->Marking.has_value() && sim.LastSchoolWorkResult()->WorldMinutes == AssessmentPaperMinutes &&
                  std::holds_alternative<TimeAdvanced>(handed.Events.front().Payload) &&
                  sim.State().Commerce.AllowanceSettledThrough == sim.State().Clock.Now(),
              "handing in charges the reserved 90 minutes once, with its marking");
    S04Refused(sim, submit, "school.work.already_submitted", "a duplicate hand-in charges nothing");

    S04Accept(sim, b, "paper b begins");
    SchoolWorkCommand surveyB = b;
    surveyB.Kind = SchoolWorkActionKind::Survey;
    S04Accept(sim, surveyB, "paper b is surveyed");
    const GameInstant beforeCancel = sim.State().Clock.Now();
    SchoolWorkCommand cancel = b;
    cancel.Kind = SchoolWorkActionKind::Cancel;
    S04Accept(sim, cancel, "paper b is set aside");
    const std::int32_t spent = sim.LastSchoolWorkResult()->WorldMinutes;
    P2CExpect(spent > 0 && sim.State().Clock.Now() == beforeCancel + spent && !sim.State().SchoolWork.Records[1].StartedAt &&
                  sim.State().SchoolWork.Records[1].Paper.PaperId.empty(),
              "setting aside charges the units already spent and keeps the brief for a restart");
    S04Accept(sim, AdvanceTime{1}, "time moves again once no paper is open");
}

void S04Week(const S04World& w)
{
    WorldSim sim = w.Sim(0x5308);
    SchoolStandingOrder order;
    order.Id = "order.school";
    S04Accept(sim, SchoolWeekOrder{order, false}, "a standing order is stored");
    auto overlap = order;
    overlap.Id = "order.overlap";
    S04Refused(sim, SchoolWeekOrder{overlap, false}, "school.week.overlapping_orders", "overlapping orders refuse");
    S04Refused(sim, SchoolWeekOrder{overlap, true}, "school.week.unknown_order", "removing an unknown order refuses");
    P2CExpect(sim.State().SchoolWeek.Orders.size() == 1, "one order kept");

    WeekPlan base;
    base.Days.resize(7);
    WeekEntry endWeek;
    endWeek.Kind = WeekEntryKind::Spend;
    endWeek.StartMinute = 1439;
    endWeek.Minutes = 1;
    base.Days.back().Entries.push_back(endWeek);
    // Day 0: new lessons stop compression before they would be played; whatever did resolve is the result's.
    const std::uint64_t seq = sim.State().Seq;
    S04Accept(sim, SchoolWeekResolve{base}, "an unplayed week resolves up to its first lesson");
    const auto& stopped = *sim.LastSchoolWeekResult();
    P2CExpect(stopped.Stop == WeekStop::FirstNeedsPlay && sim.State().Seq == seq + 1 &&
                  sim.State().Clock.Now() == stopped.World.Clock.Now() && sim.State().DayLoop == stopped.World.DayLoop &&
                  sim.State().SchoolSessions == stopped.Sessions,
              "a stopped week commits exactly what resolved");

    // Play the first day's lessons (Attend) through the commands, as a student would.
    if (!sim.State().DayLoop.Planned)
        S04Morning(sim, w.Cats);
    else if (sim.State().DayLoop.SchoolUntil <= sim.State().Clock.Now())
        S04Accept(sim, SchoolArrive{}, "arrival");
    for (std::int32_t period = 0; period < BellPeriodCount(*w.Cats.Day.School); ++period)
    {
        S04PassTo(sim, P2CBellMinute(*w.Cats.Day.School, period));
        S04Accept(sim, SchoolLessonOpen{w.Binding(period).Id, SchoolLessonMode::Attend, {}}, "a first lesson opens");
        S04Collect(S04Accept(sim, SchoolLessonClose{sim.State().SchoolSessions.Active->Session, false, 0}, "a first lesson closes"));
    }
    S04Accept(sim, AdvanceTime{CalendarDayStartInstant(7) + 360 - sim.State().Clock.Now()}, "to next Monday morning");
    const std::uint64_t seq2 = sim.State().Seq;
    const auto resolved = S04Accept(sim, SchoolWeekResolve{base}, "the known week resolves");
    const SchoolWeekResult& week = *sim.LastSchoolWeekResult();
    const auto published = S04Completions(resolved);
    P2CExpect(week.Ok() && week.Stop == WeekStop::None && !week.Lessons.empty(), "the week compresses through its lessons");
    P2CExpect(sim.State().Clock.Now() == week.World.Clock.Now() && sim.State().DayLoop == week.World.DayLoop &&
                  sim.State().SchoolSessions == week.Sessions && sim.State().Commerce == week.World.Commerce &&
                  sim.State().Seq == seq2 + 1 && IsWorldDayInStep(sim.State()),
              "the day, sessions and clock commit together; allowance settles once to the same wallet; one command counted");
    std::size_t completedInWeek = 0;
    for (const DayLoopEvent& event : week.ActivityEvents)
        completedInWeek += event.Kind == DayLoopEventKind::Completed ? 1 : 0;
    P2CExpect(published.size() == completedInWeek && completedInWeek >= week.Lessons.size(),
              "the week publishes each actual completion once, never the segments' aggregates as well");
    S04Collect(resolved);
}

/** R9 is not in WorldState: a test-side owner shows the published events are its exact, replay-safe input. */
void S04ExactlyOnce(const S04World& w)
{
    std::set<std::uint64_t> instances;
    for (const DayLoopEvent& event : S04Published)
        P2CExpect(event.Instance != 0 && instances.insert(event.Instance).second, "every completion instance is published once");
    auto seeded = SeedAwakeningInputs(w.F.Content, w.Creator.CaptureSave(), 0x5308);
    P2CExpect(seeded.Ok(), "a test-side Awakening owner seeds");
    const auto first = ApplyActivityCredits(seeded.State, S04Published);
    const auto again = ApplyActivityCredits(seeded.State, S04Published);
    P2CExpect(first.Ok() && first.Applied == S04Published.size() && again.Ok() && again.Applied == 0 &&
                  again.Replayed == S04Published.size(),
              "the published completions credit once and replay as no-ops");
}
} // namespace

int main(int argc, char** argv)
{
    P2CExpect(argc == 2, "content root argument");
    const P2CFixture fixture(argv[1]);
    const S04World world(fixture);
    S04LessonFlow(world);
    S04Rollback(world);
    S04Work(world);
    S04Week(world);
    S04ExactlyOnce(world);
    std::cout << "S04 WorldSchool passed\n";
    return 0;
}
