#include "WorldExamFollowup.h"

#include "P405ExamTestSupport.h"
#include "LegacyRecovery.h"

#include <iostream>

using namespace PerfectWorld;

namespace
{
constexpr std::string_view P407UMentor = "school_followup.p2.p4_withdrawn_school_mentor";

P407UFollowupView P407URow(const WorldSim& sim, std::string_view id)
{
    const auto rows = sim.P407UFollowups();
    const auto found = std::find_if(rows.begin(), rows.end(), [&](const auto& row) { return row.Action.Id == id; });
    P2CExpect(found != rows.end(), "the authoritative next step exists");
    return *found;
}

P407UFollowupCommand P407UCommand(const WorldSim& sim, P407UFollowupAction kind, std::string_view id = P407UMentor)
{
    return {kind, std::string(id), P407URow(sim, id).At};
}

WorldSim P407UWithdrawn(const P405World& w)
{
    auto sim = w.Sim(w.Base.Morning);
    S04Accept(sim, P402Cmd(TowerSessionAction::Enter), "sit before mentor follow-up");
    S04Accept(sim, P402Cmd(TowerSessionAction::Withdraw), "safe withdrawal yields a real result");
    return sim;
}

void P407UOutcome(WorldSim& sim, std::string_view outcome)
{
    P2CExpect(sim.ExaminationResult().Outcome == outcome, "fixture reaches requested outcome");
    const auto before = HashWorldState(sim.State());
    const auto rows = sim.P407UFollowups();
    P2CExpect(before == HashWorldState(sim.State()), "reading next steps is free and pure");
    std::size_t booked = 0;
    for (const auto& row : rows)
    {
        P2CExpect(row.CanBook, "every displayed next step has a resolved bookable slot");
        S04Accept(sim, P407UFollowupCommand{P407UFollowupAction::Book, row.Action.Id, row.At}, "offer books a real reservation");
        P2CExpect(P407URow(sim, row.Action.Id).Booked, "booking is visible in saved authority");
        S04Accept(sim, P407UCommand(sim, P407UFollowupAction::Cancel, row.Action.Id), "optional appointment can be cancelled");
        if (row.Action.Outcome == outcome) ++booked;
    }
    P2CExpect(booked >= 1, "each outcome class exposes an actionable next step");
}

void P407UOutcomes(const P405World& w)
{
    auto withdrawn = P407UWithdrawn(w);
    P407UOutcome(withdrawn, "withdrawn");
    for (auto kind : {TowerSessionAction::Decline, TowerSessionAction::Defer})
    {
        auto sim = w.Sim(w.Base.Morning);
        S04Accept(sim, P402Cmd(kind), "optional examination decision");
        P407UOutcome(sim, kind == TowerSessionAction::Decline ? "declined" : "deferred");
    }
    auto strong = w.Sim(w.Base.Morning);
    S04Accept(strong, P402Cmd(TowerSessionAction::Enter), "sit before a strong result");
    for (const auto route : P402Climb) P405Floor(strong, w.Runners, route);
    P407UOutcome(strong, "strong");
    auto poorState = w.Base.Morning;
    poorState.CountdownText.clear();
    poorState.Flags[std::string(AwakeningCountdownUnavailableFlag)] = true;
    auto poor = w.Sim(poorState);
    S04Accept(poor, P402Cmd(TowerSessionAction::Enter), "zero preparation can sit");
    P405Floor(poor, w.Runners, P402Climb[0], true);
    P407UOutcome(poor, "poor");
    auto middle = w.Sim(poorState);
    S04Accept(middle, P402Cmd(TowerSessionAction::Enter), "sit before a middle result");
    P405Floor(middle, w.Runners, P402Climb[0]);
    P405Floor(middle, w.Runners, P402Climb[1]);
    P405Floor(middle, w.Runners, P402Climb[2], true);
    P407UOutcome(middle, "middle");
}

void P407USession(const P405World& w)
{
    const auto initial = P407UWithdrawn(w).State();
    auto sim = w.Sim(initial); // The replay log starts at this terminal snapshot.
    auto book = P407UCommand(sim, P407UFollowupAction::Book);
    auto stale = book;
    ++stale.ExpectedAt;
    S04Refused(sim, stale, "p407u.stale", "a changed disclosure cannot book");
    S04Refused(sim, P407UFollowupCommand{P407UFollowupAction::Book, "school_followup.p2.p4_strong_mentor", book.ExpectedAt},
        "p407u.unavailable", "a forged ineligible result action cannot book");
    const auto now = sim.State().Clock.Now();
    S04Accept(sim, book, "book the school mentor");
    P2CExpect(sim.State().Clock.Now() == now, "booking charges no time");
    S04Refused(sim, book, "p407u.booked", "duplicate booking refuses atomically");
    S04Refused(sim, P407UCommand(sim, P407UFollowupAction::Attend), "p407u.not_due", "early attendance refuses");
    auto loaded = w.Sim(P402Reload(w.Base, sim.State(), "booking survives v13 save with existing flags and hash"));
    P2CExpect(P407URow(loaded, P407UMentor).Booked && P407URow(loaded, P407UMentor).At == book.ExpectedAt,
        "Continue retains the exact appointment");
    S04Accept(sim, AdvanceTime{book.ExpectedAt - now}, "ordinary time reaches the appointment");
    const auto row = P407URow(sim, P407UMentor);
    P2CExpect(row.CanAttend && row.Booked && !row.Attended, "the appointment is now attendable");
    const auto attend = P407UCommand(sim, P407UFollowupAction::Attend);
    const auto paid = S04Accept(sim, attend, "attend the mentor session");
    P2CExpect(paid.To - paid.From == row.Action.DurationMinutes && P407URow(sim, P407UMentor).Attended,
        "attendance spends the authored minutes and records completion");
    S04Refused(sim, attend, "p407u.attended", "a duplicate attendance cannot charge twice");
    S04Refused(sim, book, "p407u.attended", "a completed session cannot be booked again");
    loaded = w.Sim(P402Reload(w.Base, sim.State(), "completed session survives existing save envelope"));
    P2CExpect(P407URow(loaded, P407UMentor).Attended, "Continue retains attendance");
    S04Refused(loaded, attend, "p407u.attended", "reload does not permit a second attendance");
    const auto replay = ReplayCommandLogFromSnapshot(initial, sim.Log(), w.Replay());
    P2CExpect(replay.Complete && replay.Hash == HashWorldState(sim.State()), "booking and attendance replay identically");
    S04Refused(sim, SetWorldFlag{std::string(P407UFollowupFlagPrefix) + "forged", true},
        "flag.reserved", "generic flags cannot forge appointment authority");
}

void P407USlotsAndRollback(const P405World& w)
{
    auto sim = P407UWithdrawn(w);
    const auto row = P407URow(sim, P407UMentor);
    const auto second = std::find_if(w.Followups.Actions.begin(), w.Followups.Actions.end(),
        [](const auto& a) { return a.Id == "school_followup.p2.withdrawn"; });
    P2CExpect(second != w.Followups.Actions.end(), "existing A8 conversation");
    auto changed = w.Followups;
    for (auto& action : changed.Actions)
        if (action.Id == second->Id) { action.DayOffset = row.Action.DayOffset; action.Minute = row.Action.Minute; }
    auto collision = WorldSim(sim.State(), MaxEventsPerAdvance, w.Base.W.School.Cats.Day, {},
        w.Base.W.School.School(), w.Base.W.Bind(), {&w.Runners, &changed});
    S04Accept(collision, P407UCommand(collision, P407UFollowupAction::Book), "reserve mentor window");
    const auto other = P407URow(collision, second->Id);
    P2CExpect(other.CanBook && other.At > row.At, "overlapping appointments move to a free school day");
    S04Accept(sim, P407UCommand(sim, P407UFollowupAction::Book), "book before lateness");
    S04Accept(sim, AdvanceTime{row.At + 1 - sim.State().Clock.Now()}, "miss the optional appointment");
    const auto later = P407URow(sim, P407UMentor);
    P2CExpect(later.CanBook && later.At > row.At, "a missed appointment has a reachable new booking");
    S04Accept(sim, P407UCommand(sim, P407UFollowupAction::Book), "replace the missed reservation");
    S04Accept(sim, AdvanceTime{later.At - sim.State().Clock.Now()}, "reach the replacement appointment");
    S04Accept(sim, ScheduleWorldEvent{later.At + 1, "p407u/test/one"}, "first session event", CommandSource::System);
    S04Accept(sim, ScheduleWorldEvent{later.At + 2, "p407u/test/two"}, "second session event", CommandSource::System);
    auto capped = WorldSim(sim.State(), 1, w.Base.W.School.Cats.Day, {}, w.Base.W.School.School(),
        w.Base.W.Bind(), {&w.Runners, &w.Followups});
    S04Refused(capped, P407UCommand(capped, P407UFollowupAction::Attend), "p407u.charge_interrupted",
        "an interrupted charge restores clock, flags, events and sequence");
    P2CExpect(!P407URow(capped, P407UMentor).Attended, "interruption records no attendance");
    const auto school = w.Base.W.School.Cats.Day;
    const auto retake = P407URow(P407UWithdrawn(w), "school_followup.p2.retake");
    P2CExpect(CalendarRequiresAttendance(*school.Calendar, CalendarDayOfInstant(retake.At)),
        "a weekend retake suggestion moves to an open school day");
}
} // namespace

int main(int argc, char** argv)
{
    P2CExpect(argc == 2, "content root");
    const P2CFixture fixture(argv[1]);
    const P402World base(fixture);
    const P405World real(base, fixture.Content);
    P407UOutcomes(real);
    P407USession(real);
    P407USlotsAndRollback(real);
    std::cout << "P407U follow-up assertions passed\n";
    return 0;
}
