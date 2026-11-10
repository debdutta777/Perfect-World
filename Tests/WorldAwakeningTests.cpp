// R9-04: the Round 9 Awakening through WorldSim - the supervised tutorial commands and their one atomic grant, the
// countdown's scheduled 18:00 opening as a boundary of every time path, optional countdown practice, late make-up against
// the fixed 2 February 2043 06:00 deadline, and the one growth owner credited exactly once from lessons, weeks and days.
#include "R94AwakeningTestSupport.h"

#include <set>

using namespace PerfectWorld;

namespace
{
const GameInstant R94Appointment = R94At(2043, 1, 23, 9, 0);
const GameInstant R94Deadline = R94At(2043, 2, 2, 6, 0);

void R94Tutorial(const R94World& w)
{
    WorldState world = w.Seeded(R94Appointment, 0x9401);
    {
        WorldState unseeded = world;
        unseeded.Awakening = {};
        WorldSim sim = w.Bound(unseeded);
        S04Refused(sim, TutorialEnter{}, "tutorial.invalid_state", "an unseeded life has no session to enter");
    }
    {
        WorldSim sim = w.School.Continue(world);
        S04Refused(sim, TutorialEnter{}, "tutorial.missing_content", "entry needs the bound ability recipes");
    }
    WorldSim sim = w.Bound(world);
    S04Refused(sim, TutorialAct{"tutorial.station.crossing", "tutorial.crossing.steady_classmate"}, "tutorial.not_active",
               "no action before entry");
    S04Refused(sim, TutorialAbsent{}, "tutorial.source", "the player cannot record their own absence");
    S04Refused(sim, TutorialFinish{}, "tutorial.source", "the player cannot force the grant");
    const GameInstant entered = sim.State().Clock.Now();
    S04Accept(sim, TutorialEnter{}, "the supervised session opens at the appointment");
    P2CExpect(sim.State().Tutorial.Phase == TutorialDungeonPhase::Active && sim.State().Clock.Now() == entered &&
                  !sim.State().Awakening.Granted,
              "entry is free and grants nothing");
    S04Refused(sim, AdvanceTime{10}, "tutorial.active", "the clock is paused while the session is played");
    DayCommand pass;
    pass.Kind = DayCommandKind::Pass;
    pass.Minutes = 5;
    S04Refused(sim, WorldDayCommand{pass}, "tutorial.active", "the day waits for the session");
    S04Refused(sim, TutorialEnter{}, "tutorial.session_started", "there is one session");
    S04Refused(sim, TutorialAct{"tutorial.station.safe_area", "tutorial.crossing.steady_classmate"}, "tutorial.wrong_choice_or_station",
               "the wrong station is refused with the contract's code");
    S04Accept(sim, TutorialAssist{}, "assistance is free");
    S04Accept(sim, TutorialRecover{}, "recovery retries the same situation");
    P2CExpect(sim.State().Tutorial.NextSituation == 0 && sim.State().Awakening.Conduct.Entries.empty() &&
                  sim.State().Clock.Now() == entered,
              "assistance and recovery record nothing and spend nothing");
    S04Refused(sim, TutorialFinish{}, "tutorial.source", "finish is a system continuation");
    {
        WorldSim system = w.Bound(sim.State());
        const auto early = S04Send(system, TutorialFinish{}, CommandSource::System);
        P2CExpect(!early.Accepted && early.Why.Code == "tutorial.not_ready", "an active session is not ready to grant");
    }
    const CommandResult granted = R94PlayTutorial(sim, 1);
    const WorldState& done = sim.State();
    P2CExpect(done.Tutorial.Phase == TutorialDungeonPhase::Completed && done.Awakening.Granted &&
                  done.Clock.Now() == entered + TutorialSessionMinutes && done.Awakening.Granted->At == done.Clock.Now() &&
                  done.Awakening.Conduct.Entries.size() == TutorialSituationCount,
              "the last act exits, charges the disclosed hour once and commits the grant");
    P2CExpect(done.LiveAwakenedProfile && R94Profile(*done.LiveAwakenedProfile) == R94Profile(done.Awakening.Granted->StartingProfile),
              "the live practice profile starts as the grant's snapshot");
    bool advanced = false;
    for (const DomainEvent& event : granted.Events)
        advanced = advanced || std::holds_alternative<TimeAdvanced>(event.Payload);
    P2CExpect(granted.Accepted && advanced && IsWorldDayInStep(done), "the grant command publishes its charged hour");
    S04Refused(sim, TutorialEnter{}, "tutorial.session_started", "re-entry never earns another ability");
    const std::string starting = R94Profile(done.Awakening.Granted->StartingProfile);
    S04Accept(sim, AdvanceTime{120}, "ordinary life resumes at once");
    P2CExpect(R94Profile(sim.State().Awakening.Granted->StartingProfile) == starting, "the committed grant never changes");
}

void R94Withdrawal(const R94World& w)
{
    WorldSim sim = w.Bound(w.Seeded(R94Appointment, 0x9402));
    S04Accept(sim, TutorialEnter{}, "entry");
    S04Accept(sim, TutorialWithdraw{}, "safe withdrawal exits at once");
    P2CExpect(sim.State().Awakening.Granted && sim.State().Tutorial.Withdrawn &&
                  sim.State().Awakening.Conduct.Entries.size() == 1 && sim.State().Clock.Now() == R94Appointment + TutorialSessionMinutes,
              "withdrawal still finishes the supervised Awakening, recording only the current situation");
}

void R94GrantRollback(const R94World& w)
{
    const WorldState world = w.Seeded(R94Appointment, 0x9403);
    {
        // Content that cannot resolve: the act, its conduct, the charged hour and the grant all roll back together.
        WorldSim broken = w.Bound(world, false, MaxEventsPerAdvance, false);
        S04Accept(broken, TutorialEnter{}, "entry with recipes bound");
        for (std::size_t i = 0; i + 1 < TutorialSituationCount; ++i)
        {
            const auto& s = TutorialSituations()[broken.State().Tutorial.NextSituation];
            S04Accept(broken, TutorialAct{std::string(s.StationId), std::string(s.Choices[0].Id)}, "an act");
        }
        const auto& last = TutorialSituations()[broken.State().Tutorial.NextSituation];
        const std::uint64_t before = HashWorldState(broken.State());
        const auto refused = S04Send(broken, TutorialAct{std::string(last.StationId), std::string(last.Choices[0].Id)});
        P2CExpect(!refused.Accepted && refused.Why.Code.starts_with("tutorial.") && HashWorldState(broken.State()) == before &&
                      broken.State().Clock.Now() == R94Appointment && !broken.State().Awakening.Granted,
                  "a grant that cannot resolve refuses the whole exit atomically");
        WorldSim fixed = w.Bound(broken.State());
        const auto retried = S04Send(fixed, TutorialAct{std::string(last.StationId), std::string(last.Choices[0].Id)});
        P2CExpect(retried.Accepted && fixed.State().Awakening.Granted, "corrected content can retry the same exit");
    }
    {
        // An event-capped exit hour is refused, not published half-way.
        WorldSim sim = w.Bound(world, false, 1);
        S04Accept(sim, ScheduleWorldEvent{R94Appointment + 30, "test/r94/one"}, "a scheduled event", CommandSource::System);
        S04Accept(sim, ScheduleWorldEvent{R94Appointment + 30, "test/r94/two"}, "another", CommandSource::System);
        S04Accept(sim, TutorialEnter{}, "entry");
        const std::uint64_t before = HashWorldState(sim.State());
        const auto refused = S04Send(sim, TutorialWithdraw{});
        P2CExpect(!refused.Accepted && refused.Why.Code == "tutorial.charge_interrupted" && HashWorldState(sim.State()) == before,
                  "an interrupted exit charge rolls back the withdrawal");
    }
}

void R94Boundary(const R94World& w)
{
    const GameInstant opening = CountdownStartInstant();
    const WorldState evening = w.Seeded(opening - 60, 0x9404);
    {
        WorldSim sim = w.Bound(evening);
        S04Accept(sim, AdvanceTime{59}, "to 17:59");
        P2CExpect(sim.State().CountdownText.empty() && !sim.Countdown(), "17:59 does not open early");
        const auto opened = S04Accept(sim, AdvanceTime{1}, "to 18:00");
        bool registered = false;
        for (const DomainEvent& event : opened.Events)
            registered = registered || std::holds_alternative<GateRegistered>(event.Payload);
        P2CExpect(sim.Countdown() && sim.Countdown()->Start == opening && sim.Countdown()->End == R94Deadline && registered &&
                      opened.Seq == evening.Seq + 2 && IsWorldDayInStep(sim.State()),
                  "18:00 opens the countdown inside the one advancing command, without nested command sequences");
        const CountdownState first = *sim.Countdown();
        S04Accept(sim, AdvanceTime{90}, "the evening continues");
        P2CExpect(sim.Countdown()->Seed == first.Seed && sim.Countdown()->Start == first.Start, "synchronising never reopens or rerolls");
        P2CExpect(!sim.CountdownView().Awakened && !sim.CountdownView().PreparationAvailable && sim.CountdownView().OrdinaryDayAvailable,
                  "an unawakened life still sees the public countdown but cannot practise");
    }
    {
        WorldSim sim = w.Bound(evening);
        S04Accept(sim, AdvanceTime{3 * MinutesPerDay}, "a long skip across the opening");
        P2CExpect(sim.Countdown() && sim.State().Clock.Now() == opening - 60 + 3 * MinutesPerDay && IsWorldDayInStep(sim.State()),
                  "a long skip stops at 18:00, opens the countdown and carries on to its target");
    }
    {
        WorldSim sim = w.Bound(evening);
        DayCommand sleep;
        sleep.Kind = DayCommandKind::GoToSleep;
        S04Accept(sim, WorldDayCommand{sleep}, "to sleep before the opening");
        DayCommand wake;
        wake.Kind = DayCommandKind::Wake;
        wake.Minutes = 240;
        S04Accept(sim, WorldDayCommand{wake}, "waking after the opening");
        P2CExpect(sim.Countdown() && sim.Countdown()->Start == opening && !sim.State().DayLoop.Asleep && IsWorldDayInStep(sim.State()),
                  "the countdown opens while the player sleeps through 18:00");
    }
    {
        // A countdown opened with a teacher contact holds the teacher's hall setup as a standing obligation: bare time cannot
        // pass over it (R9-05: no fabricated promise). Countdown day resolution is not routed yet (R9-04 notes).
        WorldSim sim = w.Bound(evening, true);
        S04Accept(sim, AdvanceTime{120}, "the opening with a teacher contact");
        P2CExpect(sim.Countdown() && !sim.Countdown()->Obligations.empty(), "the obligation is recorded at the opening");
        S04Refused(sim, AdvanceTime{R94At(2043, 1, 29, 16, 0) - sim.State().Clock.Now()}, "countdown.plan_conflict",
                   "an owed block is never skipped by ordinary time");
    }
    {
        WorldSim sim = w.Unbound(evening);
        S04Accept(sim, AdvanceTime{120}, "an unbound sim crosses the evening");
        P2CExpect(sim.State().CountdownText.empty(), "a sim without the countdown context never opens or synchronises it");
    }
    {
        // A boundary refusal (no school calendar bound) refuses the whole advance atomically.
        const DayLoopCatalogues noCalendar{w.School.Cats.Day.Activities, w.School.Cats.Day.School, nullptr};
        WorldSim sim(evening, MaxEventsPerAdvance, noCalendar, {}, w.School.School(), w.Bind());
        S04Refused(sim, AdvanceTime{120}, "countdown.missing_calendar", "a countdown that cannot open refuses the advance");
        S04Accept(sim, AdvanceTime{59}, "time before the opening still passes");
    }
}

void R94LateMakeUp(const R94World& w)
{
    // Opened with no teacher contact (no standing obligation); the make-up and practice borrow a supervising teacher.
    WorldSim opening = w.Bound(w.Seeded(R94Appointment, 0x9405), false);
    S04Accept(opening, TutorialAbsent{}, "the appointment records an absence", CommandSource::System);
    P2CExpect(opening.State().Awakening.MakeUp.Phase == AwakeningMakeUpPhase::Required && !opening.State().Awakening.Granted,
              "absence requires a make-up and grants nothing");
    const auto unawakened = opening.PreviewCountdownPractice(PowerDimension::Precision, 30);
    P2CExpect(!unawakened.Available(), "no practice before the countdown or the grant");
    const GameInstant makeUp = R94At(2043, 1, 25, 10, 0);
    S04Accept(opening, AdvanceTime{makeUp - opening.State().Clock.Now()}, "two days pass; the countdown opened on schedule");
    WorldSim sim = w.Bound(opening.State(), true);
    P2CExpect(sim.Countdown() && sim.Countdown()->End == R94Deadline, "absence never delays the scheduled opening");
    S04Refused(sim, CountdownPracticeConfirm{sim.PreviewCountdownPractice(PowerDimension::Precision, 30)}, "countdown.closed",
               "practice needs the supervised Awakening");
    S04Accept(sim, TutorialMakeUp{makeUp}, "a supervised make-up slot now");
    S04Accept(sim, TutorialEnter{}, "the late session opens");
    R94PlayTutorial(sim);
    const WorldState& granted = sim.State();
    P2CExpect(granted.Awakening.Granted && granted.Awakening.MakeUp.Phase == AwakeningMakeUpPhase::Completed &&
                  granted.Awakening.ExaminationAt == R94Deadline && granted.Awakening.AppointmentAt == R94Appointment &&
                  sim.Countdown()->End == R94Deadline,
              "late make-up grants with the original appointment and the fixed deadline");
    const WorldCountdownView view = sim.CountdownView();
    P2CExpect(view.PreparationAvailable && view.RemainingMinutes == R94Deadline - granted.Clock.Now(),
              "the late life prepares only in the time left");

    const auto offer = sim.PreviewCountdownPractice(PowerDimension::Precision, 30);
    P2CExpect(offer.Available() && offer.Supervisor == "npc_teacher" && offer.Start == granted.Clock.Now(), "an honest disclosure");
    auto stale = offer;
    ++stale.WorldSeq;
    S04Refused(sim, CountdownPracticeConfirm{stale}, "countdown.bad_plan", "a stale disclosure is refused");
    const std::string starting = R94Profile(granted.Awakening.Granted->StartingProfile);
    const std::int64_t minutesBefore = granted.LiveAwakenedProfile->SupervisedPracticeMinutes;
    S04Accept(sim, CountdownPracticeConfirm{offer}, "the disclosed practice is confirmed");
    P2CExpect(sim.State().Clock.Now() == offer.End && sim.State().LiveAwakenedProfile->SupervisedPracticeMinutes == minutesBefore + 30 &&
                  R94Profile(sim.State().Awakening.Granted->StartingProfile) == starting && sim.LastCountdownPracticeResult() &&
                  sim.State().DayLoop.Receipts.LastDecidedAt == offer.Start,
              "practice charges its minutes, records its receipt and changes only the live profile");
    S04Refused(sim, CountdownPracticeConfirm{offer}, "countdown.bad_plan", "the same disclosure cannot be spent twice");

    // With no obligation owed the late life runs to the fixed deadline and on into ordinary life.
    WorldSim free = w.Bound(sim.State(), false);
    S04Accept(free, AdvanceTime{R94Deadline - free.State().Clock.Now()}, "to examination morning");
    P2CExpect(free.Countdown()->Closed && free.Countdown()->End == R94Deadline && free.State().Clock.Now() == R94Deadline,
              "the countdown closes at exactly 2 February 2043 06:00");
    S04Accept(free, AdvanceTime{60}, "ordinary life after the deadline");
    P2CExpect(free.CountdownView().ExaminationDue && !free.CountdownView().PreparationAvailable, "no practice after the deadline");
}

std::vector<DayLoopEvent> R94Completed(const CommandResult& result)
{
    return S04Completions(result);
}

void R94Credits(const R94World& w)
{
    const S04World& s = w.School;
    WorldState start = w.Seeded(0, 0x9406);
    WorldSim sim = w.Bound(start);
    S04Morning(sim, s.Cats);
    S04PassTo(sim, P2CBellMinute(*s.Cats.Day.School, 0));
    const SchoolLessonBinding& first = s.Binding(0);
    S04Accept(sim, SchoolLessonOpen{first.Id, SchoolLessonMode::Foreground, S04Goal(first.Structure)}, "a foreground lesson");
    S04PlayLesson(sim, s.Cats);
    const AwakeningState before = sim.State().Awakening;
    const auto closed = S04Accept(sim, SchoolLessonClose{sim.State().SchoolSessions.Active->Session, false, 0}, "recorded");
    const auto completions = R94Completed(closed);
    AwakeningState expected = before;
    const auto credited = ApplyActivityCredits(expected, completions);
    P2CExpect(!completions.empty() && credited.Ok() && credited.Applied == completions.size() &&
                  sim.State().Awakening.Stats == expected.Stats &&
                  sim.State().Awakening.AppliedActivityInstances == expected.AppliedActivityInstances,
              "the close credits its completions exactly once to the one owner");
    const std::string evidenceId = "school.lesson." + std::to_string(completions.front().Instance);
    const auto& items = sim.State().Awakening.ProjectionEvidence.Items;
    P2CExpect(std::any_of(items.begin(), items.end(), [&](const AptitudeEvidence& e) { return e.Id == evidenceId && e.At == closed.To; }),
              "a paid foreground close adds the evidence it observed");
    AwakeningState replay = sim.State().Awakening;
    const auto again = ApplyActivityCredits(replay, completions);
    P2CExpect(again.Ok() && again.Applied == 0 && again.Replayed == completions.size(), "the consumed instances never credit twice");

    // An attended (not observed) lesson credits but adds no projection evidence.
    S04PassTo(sim, P2CBellMinute(*s.Cats.Day.School, 1));
    S04Accept(sim, SchoolLessonOpen{s.Binding(1).Id, SchoolLessonMode::Attend, {}}, "an attended lesson");
    const std::size_t evidenceBefore = sim.State().Awakening.ProjectionEvidence.Items.size();
    const auto attended = S04Accept(sim, SchoolLessonClose{sim.State().SchoolSessions.Active->Session, false, 0}, "attended");
    P2CExpect(sim.State().Awakening.ProjectionEvidence.Items.size() == evidenceBefore && !R94Completed(attended).empty() &&
                  sim.State().Awakening.AppliedActivityInstances.back() == R94Completed(attended).back().Instance,
              "attendance credits learning but never projection evidence");

    // A broken owner refuses the close atomically with school.learning_refused.
    S04PassTo(sim, P2CBellMinute(*s.Cats.Day.School, 2));
    S04Accept(sim, SchoolLessonOpen{s.Binding(2).Id, SchoolLessonMode::Attend, {}}, "another attended lesson");
    WorldState broken = sim.State();
    broken.Awakening.Stats.Skills.clear();
    WorldSim brokenSim = w.Bound(broken);
    S04Refused(brokenSim, SchoolLessonClose{broken.SchoolSessions.Active->Session, false, 0}, "school.learning_refused",
               "a refused credit refuses the whole close");
    S04Accept(sim, SchoolLessonClose{sim.State().SchoolSessions.Active->Session, false, 0}, "the valid owner records it");

    // The week: its per-instance completions are credited once, never its segment totals as well.
    WeekPlan base;
    base.Days.resize(7);
    WeekEntry endWeek;
    endWeek.Kind = WeekEntryKind::Spend;
    endWeek.StartMinute = 1439;
    endWeek.Minutes = 1;
    base.Days.back().Entries.push_back(endWeek);
    SchoolStandingOrder order;
    order.Id = "order.school";
    S04Accept(sim, SchoolWeekOrder{order, false}, "a standing order");
    S04Accept(sim, AdvanceTime{CalendarDayStartInstant(7) + 360 - sim.State().Clock.Now()}, "to next Monday morning");
    const AwakeningState weekBefore = sim.State().Awakening;
    const auto week = S04Accept(sim, SchoolWeekResolve{base}, "a week resolves");
    const auto weekCompletions = R94Completed(week);
    AwakeningState weekExpected = weekBefore;
    P2CExpect(ApplyActivityCredits(weekExpected, weekCompletions).Ok() && sim.State().Awakening.Stats == weekExpected.Stats &&
                  sim.State().Awakening.AppliedActivityInstances.size() == weekBefore.AppliedActivityInstances.size() + weekCompletions.size(),
              "the week grows the owner by exactly its completions");
    std::set<std::uint64_t> unique(sim.State().Awakening.AppliedActivityInstances.begin(), sim.State().Awakening.AppliedActivityInstances.end());
    P2CExpect(unique.size() == sim.State().Awakening.AppliedActivityInstances.size(), "no instance is consumed twice");

    // An ordinary day activity is credited in its own command.
    DayCommand commit;
    commit.Kind = DayCommandKind::Commit;
    commit.ActivityId = "activity.study_at_home";
    commit.PlaceKind = "home";
    if (!sim.State().DayLoop.Planned)
        S04Accept(sim, SchoolPlanDay{}, "plan");
    S04PassTo(sim, 18 * 60);
    S04Accept(sim, WorldDayCommand{commit}, "commit to study");
    DayCommand begin;
    begin.Kind = DayCommandKind::Start;
    S04Accept(sim, WorldDayCommand{begin}, "start study");
    DayCommand complete;
    complete.Kind = DayCommandKind::Complete;
    const AwakeningState dayBefore = sim.State().Awakening;
    const auto studied = S04Accept(sim, WorldDayCommand{complete}, "complete study");
    AwakeningState dayExpected = dayBefore;
    P2CExpect(!R94Completed(studied).empty() && ApplyActivityCredits(dayExpected, R94Completed(studied)).Ok() &&
                  sim.State().Awakening.Stats == dayExpected.Stats,
              "a completed day activity grows the owner once");

    // Unseeded worlds keep S04's behaviour: nothing grows.
    WorldSim plain = w.School.Sim(0x9407);
    S04Morning(plain, s.Cats);
    S04PassTo(plain, P2CBellMinute(*s.Cats.Day.School, 0));
    S04Accept(plain, SchoolLessonOpen{first.Id, SchoolLessonMode::Attend, {}}, "an unseeded lesson");
    S04Accept(plain, SchoolLessonClose{plain.State().SchoolSessions.Active->Session, false, 0}, "an unseeded close");
    P2CExpect(!plain.State().Awakening.Seeded && plain.State().Awakening.AppliedActivityInstances.empty(),
              "an unseeded life has no growth owner");
}

void R94WeekOpening(const R94World& w)
{
    WorldSim sim = w.Bound(w.Seeded(CountdownStartInstant() - 3 * MinutesPerDay, 0x9408));
    WeekPlan base;
    base.Days.resize(7);
    WeekEntry endWeek;
    endWeek.Kind = WeekEntryKind::Spend;
    endWeek.StartMinute = 1439;
    endWeek.Minutes = 1;
    base.Days.back().Entries.push_back(endWeek);
    S04Refused(sim, SchoolWeekResolve{base}, "countdown.not_at_start", "a routed week cannot compress through the opening");
    WeekPlan shortWeek;
    shortWeek.Days.resize(1);
    shortWeek.Days.back().Entries.push_back(endWeek);
    S04Accept(sim, SchoolWeekResolve{shortWeek}, "a week that ends before the opening resolves");
    P2CExpect(sim.State().CountdownText.empty() && sim.State().Clock.Now() < CountdownStartInstant(), "and leaves the countdown unopened");
}
} // namespace

int main(int argc, char** argv)
{
    P2CExpect(argc == 2, "content root");
    const P2CFixture fixture(argv[1]);
    const R94World world(fixture);
    R94Tutorial(world);
    R94Withdrawal(world);
    R94GrantRollback(world);
    R94Boundary(world);
    R94LateMakeUp(world);
    R94Credits(world);
    R94WeekOpening(world);
    std::cout << "R9-04 WorldAwakening passed\n";
    return 0;
}
