#include "WorldExam.h"

#include "LegacyRecovery.h"
#include "P402ExamTestSupport.h"

#include <iostream>

using namespace PerfectWorld;

namespace
{
using A = TowerSessionAction;

/** Every one of the six actions is refused on a closed session, and nothing (clock, seq, hash) moves. */
void P402AllRefused(WorldSim& sim, std::string_view what)
{
    const std::string route(P402Climb[0]);
    TowerSessionCommand choose = P402Cmd(A::Choose);
    choose.Runner = sim.State().Examination.Runner;
    choose.ChoiceId = "careful";
    for (const TowerSessionCommand& command : {P402Cmd(A::Enter), P402Cmd(A::OpenRoute, route, TowerMethod::Endurance), choose,
                                               P402Cmd(A::Withdraw), P402Cmd(A::Defer), P402Cmd(A::Decline)})
        S04Refused(sim, command, "tower.session.closed", what);
}

void P402Opening(const P402World& w)
{
    WorldSim early = w.Sim(w.Before);
    for (const A kind : {A::Enter, A::Defer, A::Decline})
        S04Refused(early, P402Cmd(kind), "exam.not_open", "nothing is decided before 06:00 on 2 February 2043");
    S04Refused(early, P402Cmd(A::Withdraw), "tower.session.not_entered", "withdrawal needs a sitting");
    const CommandResult stop = S04Accept(early, AdvanceTime{120}, "a long advance crosses the examination instant");
    P2CExpect(stop.Partial && early.State().Clock.Now() == CountdownEndInstant() && early.Countdown()->Closed,
              "every time path stops at the instant and the ten days close there");
    S04Refused(early, AdvanceTime{1}, "exam.decision_required", "an undecided awakened life cannot let time pass the instant");
    const P402ExamWorldView view = early.ExaminationWorldView();
    P2CExpect(view.DecisionRequired && view.CanSit && view.CanDefer && view.CanDecline && !view.CanWithdraw &&
                  view.OutsideMinutes == 0 && view.ExaminationAt == CountdownEndInstant(),
              "the view offers sit, defer and decline at the instant");
    P2CExpect(HashWorldState(early.State()) == HashWorldState(w.Morning), "both paths reach the same morning");
}

void P402EnterRules(const P402World& w)
{
    WorldSim unbound = w.Sim(w.Morning, nullptr);
    S04Refused(unbound, P402Cmd(A::Enter), "exam.missing_content", "no floors bound, no sitting");
    S04Accept(unbound, P402Cmd(A::Defer), "deferral needs no content");
    P2CExpect(unbound.State().Examination.Participation == TowerParticipation::Deferred, "deferred");
    P402AllRefused(unbound, "a deferred examination is closed");
    S04Accept(unbound, AdvanceTime{60}, "a decided life lets time pass");

    WorldSim sim = w.Sim(w.Morning);
    const std::uint64_t seq = sim.State().Seq;
    S04Accept(sim, P402Cmd(A::Enter), "sitting at the instant");
    const CountdownProfile expected = CountdownProfileOf(w.Morning, *sim.Countdown());
    const TowerSessionState& exam = sim.State().Examination;
    P2CExpect(exam.Participation == TowerParticipation::Entered && exam.Preparation.Preparation == expected.Preparation &&
                  exam.Preparation.Composure == expected.Composure && exam.Preparation.RouteDelayMinutes == expected.RouteDelayMinutes &&
                  sim.State().Clock.Now() == CountdownEndInstant() && sim.State().Seq == seq + 1,
              "Enter freezes the countdown's final profile and costs no time");
    S04Refused(sim, P402Cmd(A::Enter), "tower.session.already_entered", "a second Enter is refused");
    S04Refused(sim, P402Cmd(A::Defer), "tower.session.already_entered", "no deferral after sitting");
    S04Refused(sim, AdvanceTime{1}, "exam.active", "time waits while the examination runs");
    S04Refused(sim, WorldDayCommand{}, "exam.active", "the day waits");
    S04Refused(sim, ConversationOpen{"npc_teacher"}, "exam.active", "conversations wait");
    S04Accept(sim, SetWorldFlag{"p402.bookkeeping", true}, "system bookkeeping still runs", CommandSource::System);

    // Legacy post-deadline life (no grant): no sitting, but deferral and declining stay open.
    WorldState ungranted = w.Morning;
    ungranted.Awakening.Granted.reset();
    ungranted.LiveAwakenedProfile.reset();
    WorldSim legacy = w.Sim(ungranted);
    P2CExpect(!legacy.ExaminationWorldView().DecisionRequired, "a life that cannot sit is never stopped at the instant");
    S04Refused(legacy, P402Cmd(A::Enter), "exam.not_awakened", "sitting requires the compulsory Awakening");
    S04Accept(legacy, P402Cmd(A::Decline), "declining needs no Awakening");

    // Missing preparation, and the explicit P3-14 zero-preparation policy.
    WorldState noCountdown = w.Morning;
    noCountdown.CountdownText.clear();
    WorldSim missing = w.Sim(noCountdown);
    S04Refused(missing, P402Cmd(A::Enter), "exam.no_preparation", "no fabricated final profile");
    noCountdown.Flags[std::string(AwakeningCountdownUnavailableFlag)] = true;
    WorldSim zero = w.Sim(noCountdown);
    S04Accept(zero, P402Cmd(A::Enter), "a countdown-unavailable life sits with zero preparation");
    P2CExpect(zero.State().Examination.Preparation == TowerPreparationSnapshot{}, "the zero snapshot, nothing invented");
    for (const auto route : P402Climb)
        P402Floor(zero, route, "careful");
    P2CExpect(zero.State().Examination.Participation == TowerParticipation::Completed, "zero preparation still completes");

    // Late: an unrouted sim let the instant pass undecided.
    WorldSim passer = w.W.Unbound(w.Morning);
    S04Accept(passer, AdvanceTime{30}, "an unrouted sim passes the instant");
    WorldSim late = w.Sim(passer.State());
    S04Refused(late, P402Cmd(A::Enter), "exam.missed", "the sitting has passed");
    P2CExpect(late.ExaminationWorldView().SitRefusal == "exam.missed" && late.ExaminationWorldView().CanDefer, "late view");
    S04Accept(late, P402Cmd(A::Defer), "a late life may still defer");
}

void P402Climbing(const P402World& w)
{
    WorldSim sim = w.Sim(w.Morning);
    S04Accept(sim, P402Cmd(A::Enter), "sit");
    S04Refused(sim, P402Cmd(A::OpenRoute, "tower.route.alliance", TowerMethod::Negotiation), "tower.session.floor_order",
               "floors in order");
    S04Refused(sim, P402Cmd(A::OpenRoute, "tower.route.pace", TowerMethod::Combat), "tower.session.method_not_allowed",
               "combat only where allowed");
    S04Refused(sim, P402Cmd(A::OpenRoute, "tower.route.nowhere", TowerMethod::Endurance), "tower.session.unknown_route",
               "unknown route");
    TowerSessionCommand early = P402Cmd(A::Choose);
    early.ChoiceId = "careful";
    S04Refused(sim, early, "tower.session.stale_beat", "no floor open");
    S04Accept(sim, P402Cmd(A::OpenRoute, "tower.route.encounter", TowerMethod::Combat), "combat is a whole-route choice");
    S04Refused(sim, P402Cmd(A::OpenRoute, "tower.route.pace", TowerMethod::Endurance), "tower.session.busy", "one floor at a time");
    TowerSessionCommand stale = P402Cmd(A::Choose);
    stale.Runner = sim.ExaminationView().Runner;
    stale.Beat = 1;
    stale.ChoiceId = "careful";
    S04Refused(sim, stale, "tower.session.stale_beat", "the beat must match");
    stale.Beat = 0;
    stale.ChoiceId = "teleport";
    S04Refused(sim, stale, "tower.session.unknown_choice", "authored choices only");
    const GameInstant at = sim.State().Clock.Now();
    for (int beat = 0; beat < 3; ++beat)
        P402Choose(sim, "careful", "a beat");
    P402Floor(sim, P402Climb[1], "careful");
    P2CExpect(sim.State().Clock.Now() == at && sim.State().Examination.Attempts.size() == 2, "no time moves before the exit");
    const std::int64_t preview = sim.ExaminationWorldView().OutsideMinutes;
    S04Accept(sim, P402Cmd(A::OpenRoute, std::string(P402Climb[2]), TowerMethod::Negotiation), "the last floor opens");
    P402Choose(sim, "careful", "beat");
    P402Choose(sim, "careful", "beat");
    const std::uint64_t seq = sim.State().Seq;
    const CommandResult finish = P402Choose(sim, "careful", "the last choice finishes the examination");
    const TowerSessionState& exam = sim.State().Examination;
    const P402ExamFinish result = sim.ExaminationResult();
    const std::int64_t mission = 9 * 5 + exam.Preparation.RouteDelayMinutes;
    const std::int64_t outside = MissionOutsideMinutes(TowerMissionDeclaration().Mission, mission);
    P2CExpect(exam.Participation == TowerParticipation::Completed && result.Ok() && result.Score.Completed &&
                  result.Score.MissionMinutes == mission && finish.To - finish.From == outside &&
                  sim.State().Clock.Now() == at + outside && sim.State().Seq == seq + 1 && preview <= outside,
              "the last choice finishes and charges the mission's outside minutes once, in the same command");
    P402AllRefused(sim, "a completed examination is immutable");
    P2CExpect(sim.State().Clock.Now() == at + outside, "no second charge");
    S04Accept(sim, AdvanceTime{1}, "life goes on after the examination");
}

void P402WithdrawEverywhere(const P402World& w)
{
    int cases = 0;
    for (int floors = 0; floors <= 2; ++floors)
    {
        for (int beats = -1; beats <= 2; ++beats) // -1: at the floor boundary, 0..2: inside an open floor after that many beats
        {
            WorldSim sim = w.Sim(w.Morning);
            S04Accept(sim, P402Cmd(A::Enter), "sit");
            for (int floor = 0; floor < floors; ++floor)
                P402Floor(sim, P402Climb[floor], "careful");
            if (beats >= 0)
            {
                const TowerRouteDefinition* route = FindTowerRoute(P402Climb[floors]);
                S04Accept(sim, P402Cmd(A::OpenRoute, std::string(P402Climb[floors]), route->Method), "a floor opens");
                for (int beat = 0; beat < beats; ++beat)
                    P402Choose(sim, "slow", "a slow beat");
            }
            const P402ExamWorldView view = sim.ExaminationWorldView();
            P2CExpect(view.CanWithdraw, "withdrawal is always offered while sitting");
            const GameInstant before = sim.State().Clock.Now();
            const CommandResult out = S04Accept(sim, P402Cmd(A::Withdraw), "withdrawal is always possible");
            const TowerSessionState& exam = sim.State().Examination;
            P2CExpect(exam.Participation == TowerParticipation::Withdrawn && out.To - out.From == view.OutsideMinutes &&
                          sim.State().Clock.Now() == before + view.OutsideMinutes &&
                          exam.Attempts.size() == static_cast<std::size_t>(floors) && exam.Active.has_value() == (beats >= 0),
                      "withdrawal keeps finished floors and the open floor's time, charging exactly the disclosed minutes");
            P402AllRefused(sim, "a withdrawn examination is closed");
            ++cases;
        }
    }
    // Withdraw from every other state is refused without effect.
    WorldSim fresh = w.Sim(w.Morning);
    S04Refused(fresh, P402Cmd(A::Withdraw), "tower.session.not_entered", "undecided: nothing to withdraw from");
    // Withdrawal needs no content: a session continued without its runners can still leave.
    WorldSim sim = w.Sim(w.Morning);
    S04Accept(sim, P402Cmd(A::Enter), "sit");
    P402Floor(sim, P402Climb[0], "careful");
    WorldSim stranded = w.Sim(sim.State(), nullptr);
    S04Refused(stranded, P402Cmd(A::OpenRoute, std::string(P402Climb[1]), TowerMethod::Analysis), "exam.missing_content",
               "no content, no floor");
    S04Accept(stranded, P402Cmd(A::Withdraw), "but withdrawal always works");
    P2CExpect(cases == 12, "all twelve sitting states withdrew");
}

void P402Ejection(const P402World& w)
{
    WorldSim sim = w.Sim(w.Morning);
    S04Accept(sim, P402Cmd(A::Enter), "sit");
    S04Accept(sim, P402Cmd(A::OpenRoute, std::string(P402Climb[0]), TowerMethod::Endurance), "open");
    const std::int32_t reserve = sim.ExaminationView().Conditions.Reserve;
    // Rush (strain 4, no recovery) until the reserve cannot pay.
    const GameInstant at = sim.State().Clock.Now();
    for (int i = 0; i < 3 && sim.State().Examination.Participation == TowerParticipation::Entered; ++i)
        P402Choose(sim, "rush", "a reckless beat");
    P2CExpect(reserve < 8 && sim.State().Examination.Participation == TowerParticipation::Ejected &&
                  sim.State().Examination.Attempts.back().Defeated && sim.State().Clock.Now() > at,
              "an exhausted reserve ejects and the exit charges in the same command");
    P402AllRefused(sim, "an ejected examination is closed");
}

void P402ClockCap(const P402World& w)
{
    WorldSim sim = w.Sim(w.Morning, &w.LongRunners);
    S04Accept(sim, P402Cmd(A::Enter), "sit with sixteen-beat floors");
    for (const auto route : P402Climb)
        P402Floor(sim, route, "slow", 16);
    const P402ExamFinish result = sim.ExaminationResult();
    P2CExpect(result.Ok() && result.Score.MissionMinutes >= 3 * 16 * 58 &&
                  sim.State().Clock.Now() == CountdownEndInstant() + TowerWorldCapMinutes,
              "a mission of 48 hours still costs the outside world exactly the 180-minute cap");
}

void P402Rollback(const P402World& w)
{
    WorldSim sim = w.Sim(w.Morning, &w.Runners, 1);
    S04Accept(sim, P402Cmd(A::Enter), "sit");
    S04Accept(sim, P402Cmd(A::OpenRoute, std::string(P402Climb[0]), TowerMethod::Endurance), "open");
    P402Choose(sim, "slow", "an hour of mission time");
    S04Accept(sim, ScheduleWorldEvent{CountdownEndInstant() + 1, "p402/test/one"}, "an event", CommandSource::System);
    S04Accept(sim, ScheduleWorldEvent{CountdownEndInstant() + 2, "p402/test/two"}, "an event", CommandSource::System);
    const WorldState before = sim.State();
    S04Refused(sim, P402Cmd(A::Withdraw), "exam.charge_interrupted", "an incomplete charge publishes nothing");
    P2CExpect(sim.State().Examination == before.Examination && sim.State().Seq == before.Seq &&
                  sim.State().Clock.Now() == CountdownEndInstant() && sim.Log().size() == 5,
              "the session, clock, sequence and log are exactly as before");
    WorldSim roomy = w.Sim(before);
    S04Accept(roomy, P402Cmd(A::Withdraw), "with room for the events the same withdrawal completes");
}

void P402Replay(const P402World& w)
{
    WorldSim sim = w.Sim(w.Morning);
    S04Accept(sim, P402Cmd(A::Enter), "sit");
    P402Floor(sim, P402Climb[0], "careful");
    P402Floor(sim, P402Climb[1], "rush");
    if (sim.State().Examination.Participation == TowerParticipation::Entered)
        S04Accept(sim, P402Cmd(A::Withdraw), "withdraw");
    const ReplayResult again = ReplayCommandLogFromSnapshot(w.Morning, sim.Log(), w.Replay(&w.Runners));
    P2CExpect(again.Complete && again.Hash == HashWorldState(sim.State()), "replay from the morning reaches the same world");
    const ReplayResult twice = ReplayCommandLogFromSnapshot(sim.State(), sim.Log(), w.Replay(&w.Runners));
    P2CExpect(!twice.Complete && twice.Applied == 0 && twice.Refusal.Code == "tower.session.closed" &&
                  twice.Hash == HashWorldState(sim.State()),
              "replaying onto the finished world can never sit, score or charge again");
    const ReplayResult unbound = ReplayCommandLogFromSnapshot(w.Morning, sim.Log(), w.Replay(nullptr));
    P2CExpect(!unbound.Complete && unbound.Refusal.Code == "exam.missing_content", "replay needs the same runner binding");
}

void P402Tampered(const P402World& w)
{
    WorldSim sim = w.Sim(w.Morning);
    S04Accept(sim, P402Cmd(A::Enter), "sit");
    P402Floor(sim, P402Climb[0], "careful");
    WorldState forged = sim.State();
    forged.Examination.Attempts.front().DisciplineMilli = 999;
    P2CExpect(IsExamStateValidInWorld(forged), "a forged score can be structurally plausible");
    P2CExpect(!ExamStateMatchesRunners(forged.Examination, w.Runners) && ExamStateMatchesRunners(sim.State().Examination, w.Runners),
              "replaying provenance against the runners exposes it");
    WorldSim forgedSim = w.Sim(forged);
    S04Refused(forgedSim, P402Cmd(A::OpenRoute, std::string(P402Climb[1]), TowerMethod::Analysis), "exam.invalid_state",
               "a forged session cannot continue");
    S04Accept(forgedSim, P402Cmd(A::Withdraw), "yet it can always leave");
}
} // namespace

int main(int argc, char** argv)
{
    P2CExpect(argc == 2, "content root");
    const P2CFixture fixture(argv[1]);
    const P402World world(fixture);
    P402Opening(world);
    P402EnterRules(world);
    P402Climbing(world);
    P402WithdrawEverywhere(world);
    P402Ejection(world);
    P402ClockCap(world);
    P402Rollback(world);
    P402Replay(world);
    P402Tampered(world);
    std::cout << "P4-02 WorldExam passed\n";
    return 0;
}
