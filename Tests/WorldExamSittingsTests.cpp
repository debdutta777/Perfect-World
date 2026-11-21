#include "WorldExamSittings.h"

#include "P402ExamTestSupport.h"

#include <iostream>
#include <tuple>
#include <variant>
#include <vector>

// P4-09: annual sittings to age twenty, the late-entry window and its reduced allowance, ExamTransition events (exactly once,
// never on refusal, reload or replay), and the pw.examination/2 envelope (P4-02's /1 bytes unchanged without earlier sittings).
using namespace PerfectWorld;

namespace
{
using A = TowerSessionAction;
using K = ExamTransitionKind;

std::vector<ExamTransition> P409Transitions(const CommandResult& result)
{
    std::vector<ExamTransition> out;
    for (const DomainEvent& event : result.Events)
        if (const auto* t = std::get_if<ExamTransition>(&event.Payload))
            out.push_back(*t);
    return out;
}

/** The single transition of `result`, demanded to be of `kind` at `sitting`. */
ExamTransition P409One(const CommandResult& result, K kind, std::uint32_t sitting, std::string_view what)
{
    const std::vector<ExamTransition> all = P409Transitions(result);
    P2CExpect(all.size() == 1 && all.front().Kind == kind && all.front().Sitting == sitting, what);
    return all.front();
}

/** A routed sim with the date of birth bound (the creation record's, as the game binds it on Continue). */
WorldSim P409Born(const P402World& w, WorldState state, const DateOfBirth* birth)
{
    return WorldSim(std::move(state), MaxEventsPerAdvance, w.W.School.Cats.Day, {}, w.W.School.School(), w.W.Bind(),
                    P402WorldExamCatalogues{&w.Runners, nullptr, birth});
}

/** The undecided morning `minutes` after the opening, reached by an unrouted sim (a routed life is stopped at 06:00). */
WorldState P409LateMorning(const P402World& w, std::int64_t minutes)
{
    WorldSim passer = w.W.Unbound(w.Morning);
    if (minutes > 0)
        S04Accept(passer, AdvanceTime{minutes}, "an unrouted sim lets the opening pass");
    return passer.State();
}

/** Move a decided life to `at` (stopping at a deferred candidate's next sitting is the caller's expectation). */
void P409AdvanceTo(WorldSim& sim, GameInstant at, std::string_view what)
{
    const CommandResult moved = S04Accept(sim, AdvanceTime{at - sim.State().Clock.Now()}, what);
    P2CExpect(!moved.Partial && sim.State().Clock.Now() == at, what);
}

void P409Pure()
{
    P2CExpect(P409SittingAt(0) == CountdownEndInstant() && P409SittingAt(1) == GameClock::FromCalendar(2044, 2, 2, 6, 0) &&
                  P409SittingAt(4) == GameClock::FromCalendar(2047, 2, 2, 6, 0),
              "sitting k opens on 2 February (2043 + k) at 06:00");
    P2CExpect(!P409SittingOpenedBy(P409SittingAt(0) - 1) && P409SittingOpenedBy(P409SittingAt(0)) == 0u &&
                  P409SittingOpenedBy(P409SittingAt(1) - 1) == 0u && P409SittingOpenedBy(P409SittingAt(1)) == 1u,
              "the latest opened sitting changes exactly at each opening");
    for (std::uint32_t k = 0; k <= 3; ++k)
        P2CExpect(P409SittingAllowsAge(k, nullptr), "every legal life may sit through 2046");
    P2CExpect(!P409SittingAllowsAge(4, nullptr), "unbound, the earliest legal birth (3 June 2025) is 21 in 2047");
    const DateOfBirth youngest = LatestDateOfBirth; // 2 June 2026
    const DateOfBirth edgeIn{2026, 2, 3};
    const DateOfBirth edgeOut{2026, 2, 2};
    P2CExpect(P409SittingAllowsAge(4, &youngest) && !P409SittingAllowsAge(5, &youngest), "the youngest life sits up to 2047");
    P2CExpect(P409SittingAllowsAge(4, &edgeIn) && !P409SittingAllowsAge(4, &edgeOut),
              "twenty on the morning is allowed; a twenty-first birthday that very day is not");
}

void P409LateWindow(const P402World& w)
{
    for (const std::int64_t late : {std::int64_t{1}, P409LateEntryMinutes})
    {
        WorldSim sim = w.Sim(P409LateMorning(w, late));
        const P402ExamWorldView before = sim.ExaminationWorldView();
        P2CExpect(before.CanSit && !before.DecisionRequired && before.Sittings.EntryClosesAt == CountdownEndInstant() + 60,
                  "inside the window Sit is offered");
        const GameInstant now = sim.State().Clock.Now();
        const CommandResult sat = S04Accept(sim, P402Cmd(A::Enter), "a late candidate enters inside the window");
        const ExamTransition t = P409One(sat, K::Sat, 0, "one Sat event");
        P2CExpect(t.LateMinutes == late && t.Participation == TowerParticipation::Entered && sat.From == now && sat.To == now,
                  "Sat reports the lateness and costs no time");
        const P402ExamWorldView view = sim.ExaminationWorldView();
        P2CExpect(view.Sittings.LateMinutes == late && view.Sittings.AllowanceMinutes == TowerWorldCapMinutes - late,
                  "the reduced allowance is shown");
        (void)P402Reload(w, sim.State(), "a late entered examination saves and reloads (no schema change)");
    }
    WorldSim missed = w.Sim(P409LateMorning(w, P409LateEntryMinutes + 1));
    S04Refused(missed, P402Cmd(A::Enter), "exam.missed", "one minute past the window the sitting has passed");
    const P402ExamWorldView view = missed.ExaminationWorldView();
    P2CExpect(view.SitRefusal == "exam.missed" && view.CanDefer && view.CanDecline && view.Sittings.HasNextSitting &&
                  view.Sittings.NextSittingAt == P409SittingAt(1),
              "a missed morning still offers deferral and names the next sitting");
}

void P409ReducedClock(const P402World& w)
{
    // Time called: 60 minutes late, a long floor of slow beats runs past the 120-minute allowance.
    {
        WorldSim sim = w.Sim(P409LateMorning(w, 60), &w.LongRunners);
        S04Accept(sim, P402Cmd(A::Enter), "late sit");
        S04Accept(sim, P402Cmd(A::OpenRoute, std::string(P402Climb[0]), TowerMethod::Endurance), "a long floor opens");
        CommandResult last;
        for (int beat = 0; beat < 16 && sim.State().Examination.Participation == TowerParticipation::Entered; ++beat)
        {
            P2CExpect(sim.ExaminationWorldView().OutsideMinutes <= 120, "while Entered the pending minutes fit the allowance");
            last = P402Choose(sim, "slow", "a slow beat");
        }
        const ExamTransition t = P409One(last, K::Withdrew, 0, "time called publishes one Withdrew");
        P2CExpect(t.TimeCalled && t.OutsideMinutes == 120 && t.LateMinutes == 60 &&
                      sim.State().Examination.Participation == TowerParticipation::Withdrawn && sim.State().Examination.Active,
                  "the session is withdrawn with the unfinished floor kept as time, not credit");
        P2CExpect(sim.State().Clock.Now() == CountdownEndInstant() + TowerWorldCapMinutes &&
                      sim.State().Awakening.ExaminationAt == CountdownEndInstant() &&
                      sim.ExaminationResult().Score.OutsideMinutes > 120,
                  "the hall closes at 09:00 for everyone: the charge stops at the allowance, the deadline never moves");
        P2CExpect(std::holds_alternative<TimeAdvanced>(last.Events.at(1).Payload), "the exit's time follows its event");
        (void)P402Reload(w, sim.State(), "a time-called session saves and reloads");
    }
    // The same nine slow beats on time are never called: P4-01's rule is unchanged for punctual candidates.
    {
        WorldSim sim = w.Sim(w.Morning, &w.LongRunners);
        S04Accept(sim, P402Cmd(A::Enter), "punctual sit");
        S04Accept(sim, P402Cmd(A::OpenRoute, std::string(P402Climb[0]), TowerMethod::Endurance), "a long floor opens");
        for (int beat = 0; beat < 9; ++beat)
            P2CExpect(P409Transitions(P402Choose(sim, "slow", "a slow beat")).empty(), "no transition mid-floor");
        P2CExpect(sim.State().Examination.Participation == TowerParticipation::Entered, "on time, no time is called");
    }
    // Late slow climb: whichever comes first (the bell or the last floor), the exit is charged at most the allowance.
    {
        WorldSim sim = w.Sim(P409LateMorning(w, 60));
        S04Accept(sim, P402Cmd(A::Enter), "late sit");
        CommandResult last;
        for (const auto route : P402Climb)
        {
            if (sim.State().Examination.Participation != TowerParticipation::Entered)
                break;
            S04Accept(sim, P402Cmd(A::OpenRoute, std::string(route), FindTowerRoute(route)->Method), "a floor opens");
            for (int beat = 0; beat < 3 && sim.State().Examination.Participation == TowerParticipation::Entered; ++beat)
                last = P402Choose(sim, "slow", "a slow beat");
        }
        const std::vector<ExamTransition> all = P409Transitions(last);
        P2CExpect(all.size() == 1 && (all.front().Kind == K::Finished || (all.front().Kind == K::Withdrew && all.front().TimeCalled)),
                  "the slow late climb ends exactly once, finished or with time called");
        P2CExpect(all.front().OutsideMinutes == std::min<std::int64_t>(sim.ExaminationResult().Score.OutsideMinutes, 120) &&
                      sim.State().Clock.Now() == CountdownEndInstant() + 60 + all.front().OutsideMinutes,
                  "a late exit is charged at most the allowance");
        (void)P402Reload(w, sim.State(), "a late closed session reloads");
    }
    // A brisk late climb finishes well inside the allowance and is charged exactly its own time.
    {
        WorldSim sim = w.Sim(P409LateMorning(w, 30));
        S04Accept(sim, P402Cmd(A::Enter), "late sit");
        CommandResult last;
        for (const auto route : P402Climb)
        {
            S04Accept(sim, P402Cmd(A::OpenRoute, std::string(route), FindTowerRoute(route)->Method), "a floor opens");
            for (int beat = 0; beat < 3; ++beat)
                last = P402Choose(sim, "careful", "a careful beat");
        }
        const ExamTransition t = P409One(last, K::Finished, 0, "one Finished event");
        P2CExpect(t.Participation == TowerParticipation::Completed && t.LateMinutes == 30 &&
                      t.OutsideMinutes == sim.ExaminationResult().Score.OutsideMinutes &&
                      sim.State().Clock.Now() == CountdownEndInstant() + 30 + t.OutsideMinutes,
                  "a completed late climb inside the allowance pays its own time");
    }
}

void P409Events(const P402World& w)
{
    for (const auto& [kind, event, participation] :
         {std::tuple{A::Defer, K::Deferred, TowerParticipation::Deferred}, std::tuple{A::Decline, K::Declined, TowerParticipation::Declined}})
    {
        WorldSim sim = w.Sim(w.Morning);
        const ExamTransition t = P409One(S04Accept(sim, P402Cmd(kind), "an exit before entry"), event, 0, "one exit event");
        P2CExpect(t.Participation == participation && t.OutsideMinutes == 0 && t.LateMinutes == 0, "a free exit");
        const CommandResult again = sim.Submit({CommandSource::Player, P402Cmd(kind)});
        P2CExpect(!again.Accepted && again.Events.empty(), "a duplicate is refused and publishes nothing");
    }
    WorldSim sim = w.Sim(w.Morning);
    P409One(S04Accept(sim, P402Cmd(A::Enter), "sit"), K::Sat, 0, "Sat once");
    P2CExpect(sim.Submit({CommandSource::Player, P402Cmd(A::Enter)}).Events.empty(), "a second Enter publishes nothing");
    P402Floor(sim, P402Climb[0], "careful");
    const WorldState mid = sim.State();
    const CommandResult out = S04Accept(sim, P402Cmd(A::Withdraw), "withdraw at the floor boundary");
    const ExamTransition t = P409One(out, K::Withdrew, 0, "one Withdrew");
    P2CExpect(!t.TimeCalled && t.OutsideMinutes == out.To - out.From && std::holds_alternative<ExamTransition>(out.Events.front().Payload),
              "Withdrew carries the charge and precedes its TimeAdvanced");
    P2CExpect(sim.Submit({CommandSource::Player, P402Cmd(A::Withdraw)}).Events.empty(), "a duplicate withdrawal publishes nothing");

    // Reload: the loaded world resumes silently; the finished session refuses again with no event; replay rebuilds the same
    // world without a published event (ReplayResult carries none).
    WorldSim resumed = w.Sim(P402Reload(w, sim.State(), "reload after the exit"));
    P2CExpect(resumed.Submit({CommandSource::Player, P402Cmd(A::Withdraw)}).Events.empty(), "no event after Continue");
    const ReplayResult replay = ReplayCommandLogFromSnapshot(w.Morning, sim.Log(), w.Replay(&w.Runners));
    P2CExpect(replay.Complete && replay.Hash == HashWorldState(sim.State()), "replay reproduces the world");
    ReplayOptions keep = w.Replay(&w.Runners);
    keep.KeepResults = true;
    const ReplayResult onto = ReplayCommandLogFromSnapshot(sim.State(), sim.Log(), keep);
    bool republished = false;
    for (const CommandResult& r : onto.Results)
        republished = republished || !P409Transitions(r).empty();
    P2CExpect(!onto.Complete && !republished, "replaying onto the finished world publishes no transition again");
    WorldSim cont = w.Sim(mid);
    P2CExpect(P409Transitions(S04Accept(cont, P402Cmd(A::Withdraw), "the same exit once more from the copy")).size() == 1,
              "each accepted transition publishes exactly one event");

    WorldSim eject = w.Sim(w.Morning);
    S04Accept(eject, P402Cmd(A::Enter), "sit");
    S04Accept(eject, P402Cmd(A::OpenRoute, std::string(P402Climb[0]), TowerMethod::Endurance), "open");
    std::vector<ExamTransition> seen;
    for (int i = 0; i < 3 && eject.State().Examination.Participation == TowerParticipation::Entered; ++i)
        for (const ExamTransition& e : P409Transitions(P402Choose(eject, "rush", "rush")))
            seen.push_back(e);
    P2CExpect(seen.size() == 1 && seen.front().Kind == K::Finished && seen.front().Participation == TowerParticipation::Ejected,
              "an ejection publishes Finished once");
}

void P409Retakes(const P402World& w)
{
    // Deferred: the next sitting meets the routed life like the first one did.
    WorldSim sim = w.Sim(w.Morning);
    S04Accept(sim, P402Cmd(A::Defer), "defer to a later year");
    const CommandResult stop = S04Accept(sim, AdvanceTime{P409SittingAt(1) + 600 - sim.State().Clock.Now()}, "a year passes");
    P2CExpect(stop.Partial && sim.State().Clock.Now() == P409SittingAt(1), "a deferred candidate is stopped at the next sitting");
    S04Refused(sim, AdvanceTime{1}, "exam.decision_required", "the deferred candidate decides before time moves on");
    const P402ExamWorldView view = sim.ExaminationWorldView();
    P2CExpect(view.DecisionRequired && view.CanSit && view.Sittings.Sitting == 0 && view.Sittings.SitTarget == 1, "Sit 2044");
    const ExamTransition t = P409One(S04Accept(sim, P402Cmd(A::Enter), "the retake"), K::Sat, 1, "Sat at sitting 1");
    P2CExpect(t.LateMinutes == 0 && sim.State().ExaminationEarlier.size() == 1 &&
                  sim.State().ExaminationEarlier.front().Participation == TowerParticipation::Deferred &&
                  sim.State().Examination.Participation == TowerParticipation::Entered &&
                  sim.State().Examination.Preparation.RouteDelayMinutes == 0,
              "the deferral is archived and the retake starts fresh, without the 2043 route delay");
    for (const auto route : P402Climb)
        P402Floor(sim, route, "careful");
    P2CExpect(sim.State().Examination.Participation == TowerParticipation::Completed && sim.ExaminationResult().Ok(),
              "the retake completes with a result");
    const std::string text = SerialiseExamState(sim.State());
    P2CExpect(text.rfind("pw.examination/2\nsittings=1\nparticipation=5\n", 0) == 0, "the envelope carries the earlier sitting");
    (void)P402Reload(w, sim.State(), "a retake world saves and reloads at schema 13");

    // Withdrawn (a poor exit) and declined lives are not stopped, but enter the next sitting's window.
    for (const A exit : {A::Withdraw, A::Decline})
    {
        WorldSim life = w.Sim(w.Morning);
        if (exit == A::Withdraw)
            S04Accept(life, P402Cmd(A::Enter), "sit");
        S04Accept(life, P402Cmd(exit), "leave the first sitting");
        P409AdvanceTo(life, P409SittingAt(1) + 30, "time passes the next opening without a stop");
        P409One(S04Accept(life, P402Cmd(A::Enter), "a late retake"), K::Sat, 1, "Sat at sitting 1");
        P2CExpect(P409EntryLateness(life.State()) == 30 && life.State().ExaminationEarlier.size() == 1, "a late retake");
        (void)P402Reload(w, life.State(), "the late retake reloads");
    }

    // A missed first morning is recorded as absence when the next sitting is taken.
    WorldSim passer = w.W.Unbound(w.Morning);
    S04Accept(passer, AdvanceTime{P409SittingAt(1) - passer.State().Clock.Now()}, "an unrouted year passes undecided");
    WorldSim absent = w.Sim(passer.State());
    S04Accept(absent, P402Cmd(A::Enter), "the first sitting is the 2044 one");
    P2CExpect(absent.State().ExaminationEarlier.size() == 1 && absent.State().ExaminationEarlier.front() == TowerSessionState{},
              "absence from 2043 is an Unset record");
    (void)P402Reload(w, absent.State(), "the absent-then-sat world reloads");
}

void P409AgeLimit(const P402World& w)
{
    WorldSim sim = w.Sim(w.Morning);
    S04Accept(sim, P402Cmd(A::Decline), "decline the first sitting");
    P409AdvanceTo(sim, P409SittingAt(4), "four years pass for a declined life");
    S04Refused(sim, P402Cmd(A::Enter), "exam.age_limit", "unbound, 2047 is past twenty for the earliest birth");
    S04Refused(sim, P402Cmd(A::Defer), "exam.age_limit", "nor can a sitting past twenty be deferred");
    P2CExpect(!sim.ExaminationWorldView().CanSit && !sim.ExaminationWorldView().Sittings.HasNextSitting, "no sitting left");
    const DateOfBirth youngest = LatestDateOfBirth;
    WorldSim young = P409Born(w, sim.State(), &youngest);
    P409One(S04Accept(young, P402Cmd(A::Enter), "a candidate born 2 June 2026 is twenty in 2047"), K::Sat, 4, "Sat at 4");
    const std::vector<TowerSessionState>& earlier = young.State().ExaminationEarlier;
    P2CExpect(earlier.size() == 4 && earlier[0].Participation == TowerParticipation::Declined && earlier[1] == TowerSessionState{} &&
                  earlier[3] == TowerSessionState{},
              "the declined sitting and three absences are archived in order");
    S04Accept(young, P402Cmd(A::Withdraw), "withdraw the last sitting");
    (void)P402Reload(w, young.State(), "the last possible sitting reloads without the birth binding");
    P2CExpect(!young.ExaminationWorldView().Sittings.HasNextSitting, "2048 is past twenty for everyone");
    P409AdvanceTo(young, P409SittingAt(5), "another year");
    S04Refused(young, P402Cmd(A::Enter), "exam.age_limit", "no sitting beyond the codec's last");

    // Deferring the last sitting the limit allows ends the prompts: no stop at a sitting the candidate may not take.
    WorldSim last = w.Sim(w.Morning);
    S04Accept(last, P402Cmd(A::Decline), "decline");
    P409AdvanceTo(last, P409SittingAt(3), "three years");
    S04Accept(last, P402Cmd(A::Defer), "defer 2046, the last unbound sitting");
    P409AdvanceTo(last, P409SittingAt(4) + 1, "no stop at 2047 for an unbound life");
}

void P409Codec(const P402World& w)
{
    P2CExpect(SerialiseExamState(w.Morning).rfind("pw.examination/1\n", 0) == 0, "one sitting keeps P4-02's exact envelope");
    WorldSim sim = w.Sim(w.Morning);
    S04Accept(sim, P402Cmd(A::Decline), "decline");
    P409AdvanceTo(sim, P409SittingAt(1), "a year");
    S04Accept(sim, P402Cmd(A::Defer), "defer 2044");
    const WorldState world = sim.State();
    const std::string text = SerialiseExamState(world);
    const auto replaced = [&](std::string_view from, std::string_view to) {
        std::string out = text;
        const std::size_t at = out.find(from);
        P2CExpect(at != std::string::npos, "tamper anchor exists");
        out.replace(at, from.size(), to);
        return out;
    };
    const std::vector<std::pair<std::string, std::string>> bad = {
        {"version 1 with an earlier body", replaced("pw.examination/2\nsittings=1\n", "pw.examination/1\n")},
        {"zero sittings", replaced("sittings=1", "sittings=0")},
        {"too many sittings", replaced("sittings=1", "sittings=5")},
        {"non-canonical count", replaced("sittings=1", "sittings=01")},
        {"earlier sitting entered", replaced("participation=6", "participation=1")},
        {"unknown header", replaced("pw.examination/2", "pw.examination/3")},
    };
    for (const auto& [name, candidate] : bad)
    {
        WorldState target = world;
        target.Examination = {};
        target.ExaminationEarlier.clear();
        P2CExpect(!DeserialiseExamState(candidate, target) && target.Examination == TowerSessionState{} &&
                      target.ExaminationEarlier.empty(),
                  "refused: " + name);
    }
    WorldState target = world;
    target.ExaminationEarlier.clear();
    target.Examination = {};
    P2CExpect(DeserialiseExamState(text, target) && target.ExaminationEarlier == world.ExaminationEarlier &&
                  target.Examination == world.Examination,
              "the canonical /2 text decodes exactly");
    WorldState undecided = world;
    undecided.Examination = {};
    P2CExpect(!IsExamStateValidInWorld(undecided) && P402SaveText(w, undecided).empty(),
              "a later sitting is never undecided: it opens only by a decision");
    WorldState lateEntered = P409LateMorning(w, 61);
    lateEntered.Examination.Participation = TowerParticipation::Entered;
    P2CExpect(!IsExamStateValidInWorld(lateEntered), "an entered session outside the window is impossible");
    // A save from before P4-09 (pw.examination/1, written by P4-02's schema 13) still loads: same bytes, same hash.
    WorldSim old = w.Sim(w.Morning);
    S04Accept(old, P402Cmd(A::Defer), "defer");
    const WorldState loaded = P402Reload(w, old.State(), "a one-sitting world round-trips");
    P2CExpect(loaded.ExaminationEarlier.empty() && SerialiseExamState(loaded).rfind("pw.examination/1\n", 0) == 0,
              "legacy envelopes decode with no earlier sitting");
}
} // namespace

int main(int argc, char** argv)
{
    P2CExpect(argc == 2, "content root");
    const P2CFixture fixture(argv[1]);
    const P402World world(fixture);
    P409Pure();
    P409LateWindow(world);
    P409ReducedClock(world);
    P409Events(world);
    P409Retakes(world);
    P409AgeLimit(world);
    P409Codec(world);
    std::cout << "P4-09 exam sittings passed\n";
    return 0;
}
