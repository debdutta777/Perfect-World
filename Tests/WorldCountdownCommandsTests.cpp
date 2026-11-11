#include "WorldReducer.h"

#include "R94AwakeningTestSupport.h"

#include <algorithm>
#include <array>
#include <iostream>

using namespace PerfectWorld;

namespace
{
WorldAwakeningCatalogues P37Binding(const R94World& world, const RelationshipBook* relationships = nullptr)
{
    auto binding = world.Bind();
    binding.Countdown.Contacts = {{"teacher", "npc_teacher"}, {"sibling", "npc_sibling"},
                                  {"neighbour", "npc_neighbour"}, {"friend", "npc_friend"}};
    binding.Countdown.Relationships = relationships;
    return binding;
}

WorldSim P37Bound(const R94World& world, WorldState state, const RelationshipBook* relationships = nullptr,
                  std::size_t cap = MaxEventsPerAdvance)
{
    return WorldSim(std::move(state), cap, world.School.Cats.Day, {}, world.School.School(), P37Binding(world, relationships));
}

WorldSim P37Opening(const R94World& world, std::uint64_t seed, const RelationshipBook* relationships = nullptr)
{
    auto state = world.Seeded(R94At(2043, 1, 23, 9, 0), seed);
    auto sim = P37Bound(world, std::move(state), relationships);
    S04Accept(sim, TutorialEnter{}, "the compulsory supervised session opens");
    S04Accept(sim, TutorialWithdraw{}, "safe withdrawal grants without combat");
    S04Accept(sim, AdvanceTime{CountdownStartInstant() - sim.State().Clock.Now()}, "ordinary life reaches opening");
    P2CExpect(sim.Countdown() && sim.Countdown()->Obligations.size() == 3, "all three standing contacts are bound");
    return sim;
}

WorldState P37Reload(const R94World& world, const WorldState& state)
{
    const auto text = SerializeWorldSave(world.Save(state));
    P2CExpect(!text.empty(), "command-produced state saves through existing schema");
    WorldSaveData loaded;
    const auto result = DeserializeWorldSave(text, loaded);
    P2CExpect(result.Succeeded() && result.HashVerified && result.SourceVersion == 12 &&
                  HashWorldState(loaded.World) == HashWorldState(state), "existing hash 9 authenticates the reload");
    return loaded.World;
}

WeekDayPlan P37DayEnd()
{
    WeekEntry rest;
    rest.Kind = WeekEntryKind::Spend;
    rest.StartMinute = 1439;
    rest.Minutes = 1;
    rest.Pass = DayPassKind::Rest;
    return {{rest}};
}

void P37FinishDays(WorldSim& sim, bool daily)
{
    int calls = 0;
    int news = 0;
    while (!sim.Countdown()->Closed && calls++ < 12)
    {
        const auto day = GameClock::DayNumber(sim.State().Clock.Now()) - GameClock::DayNumber(CountdownStartInstant());
        const auto count = daily ? 1 : static_cast<int>(std::min<std::int64_t>(WeekMaxDays, CountdownDays - day));
        const auto before = sim.State().Seq;
        S04Accept(sim, CountdownResolveDays{std::vector<WeekDayPlan>(count, P37DayEnd())}, "routine days honour every reserved block");
        P2CExpect(sim.State().Seq == before + 1 && IsWorldDayInStep(sim.State()), "one outer command owns valid time and sequence");
        P2CExpect(sim.LastCountdownResolveResult().has_value(), "the accepted resolver exposes its actual recap");
        news += sim.LastCountdownResolveResult()->StoppedForNews ? 1 : 0;
    }
    P2CExpect(sim.Countdown()->Closed && sim.State().Clock.Now() == CountdownEndInstant() && news == 1,
              "all ten days reach exactly 2 February 2043 06:00 with one news interrupt");
    P2CExpect(sim.State().LiveAwakenedProfile && sim.State().LiveAwakenedProfile->SupervisedPracticeMinutes == 0,
              "ignoring every supernatural practice session remains a complete route");
}

void P37TenDayProperty(const R94World& world)
{
    RelationshipBook relationships;
    for (const std::string person : {"npc_friend", "npc_neighbour", "npc_sibling", "npc_teacher"})
    {
        Relationship pair;
        pair.Low = person;
        pair.High = "player";
        pair.Trust = pair.Respect = 10000;
        P2CExpect(relationships.RestorePair(std::move(pair)), "trusted relationship fixture");
    }
    const std::array policies{CountdownProposal::Honour, CountdownProposal::Decline, CountdownProposal::Defer, CountdownProposal::Share};
    for (std::uint64_t seed = 1; seed <= 12; ++seed)
    {
        for (const auto policy : policies)
        {
            auto sim = P37Opening(world, seed, &relationships);
            const auto originalGrant = R94Profile(sim.State().Awakening.Granted->StartingProfile);
            const auto first = CountdownReservedBlocks(*sim.Countdown()).front();
            S04Refused(sim, AdvanceTime{first.End - sim.State().Clock.Now()}, "countdown.plan_conflict",
                       "bare time cannot invent honouring a promise");
            // A colliding player plan refuses, then removing that entry always leaves the reserved-block route available.
            const auto index = GameClock::DayNumber(first.Start) - GameClock::DayNumber(sim.State().Clock.Now());
            std::vector<WeekDayPlan> conflict(static_cast<std::size_t>(index) + 1);
            auto collision = first.Entry;
            conflict.back().Entries.push_back(collision);
            S04Refused(sim, CountdownResolveDays{conflict}, "countdown.plan_conflict", "a conflicting plan is atomic");
            if (policy != CountdownProposal::Honour)
            {
                const auto obligations = sim.Countdown()->Obligations;
                for (const auto& obligation : obligations)
                    S04Accept(sim, CountdownNegotiateObligation{obligation.Id, policy}, "one disclosed negotiation per obligation");
                S04Refused(sim, CountdownNegotiateObligation{obligations.front().Id, policy}, "countdown.negotiated",
                           "repeated negotiation never rerolls");
            }
            auto restored = P37Bound(world, P37Reload(world, sim.State()), &relationships);
            P37FinishDays(restored, seed % 2 == 0);
            const auto completedCountdown = *restored.Countdown();
            P2CExpect(std::all_of(completedCountdown.Obligations.begin(), completedCountdown.Obligations.end(),
                          [](const auto& item) { return item.Done || item.Disposition == CountdownDisposition::Declined; }),
                      "every promise was performed or explicitly declined");
            P2CExpect(R94Profile(restored.State().Awakening.Granted->StartingProfile) == originalGrant,
                      "ordinary countdown resolution preserves the immutable grant");
            P37Reload(world, restored.State());
        }
    }
}

void P37OffersAndCredits(const R94World& world)
{
    RelationshipBook closeTeacher;
    Relationship teacher;
    teacher.Low = "npc_teacher";
    teacher.High = "player";
    teacher.Respect = 10000;
    P2CExpect(closeTeacher.RestorePair(std::move(teacher)), "a teacher relationship shapes the disclosed odds");
    auto original = P37Opening(world, 3701, &closeTeacher);
    const auto beforeTake = original.State().Clock.Now();
    S04Accept(original, CountdownTakeOpportunity{"opp.paper_marathon"}, "a real study opportunity commits once");
    const auto commitment = *CountdownFindCommitment(*original.Countdown(), "opp.paper_marathon");
    S04Accept(original, CountdownTakeOpportunity{"opp.teacher_review"}, "a contact-shaped opportunity commits once");
    const auto teacherCommitment = *CountdownFindCommitment(*original.Countdown(), "opp.teacher_review");
    P2CExpect(original.State().Clock.Now() == beforeTake, "taking an offer reserves time without moving it");
    S04Refused(original, CountdownTakeOpportunity{"opp.paper_marathon"}, "countdown.taken", "duplicate commit is refused");
    S04Refused(original, CountdownTakeOpportunity{"opp.quiet_day"}, "countdown.group_taken", "mutual exclusion stays authoritative");
    S04Refused(original, CountdownTakeOpportunity{"opp.missing"}, "countdown.unknown_opportunity", "contract refusal code survives routing");
    S04Refused(original, CountdownNegotiateObligation{"oblig.sibling_pickup", CountdownProposal::Honour},
               "countdown.proposal_unavailable", "honour is executed by the plan, never a negotiation");
    auto restored = P37Bound(world, P37Reload(world, original.State()));
    P2CExpect(*CountdownFindCommitment(*restored.Countdown(), commitment.Id) == commitment,
              "reload keeps committed seed, odds, contact and window");
    const auto displayed = CountdownOffers(restored.State(), *restored.Countdown(), restored.AwakeningContent().Countdown);
    const auto teacherOffer = std::find_if(displayed.begin(), displayed.end(), [](const auto& offer) {
        return offer.Def && offer.Def->Id == "opp.teacher_review";
    });
    P2CExpect(teacherOffer != displayed.end() && teacherOffer->Odds == teacherCommitment.Odds &&
                  teacherOffer->Contact == teacherCommitment.Contact,
              "a changed relationship after reload cannot change the committed disclosure");
    const auto grant = R94Profile(restored.State().Awakening.Granted->StartingProfile);
    const auto owner = restored.State().Awakening;
    P37FinishDays(original, true);
    P37FinishDays(restored, false);
    const auto a = *CountdownFindCommitment(*original.Countdown(), commitment.Id);
    const auto b = *CountdownFindCommitment(*restored.Countdown(), commitment.Id);
    P2CExpect(a == b && a.Resolved && a.Seed == commitment.Seed && a.Odds == commitment.Odds,
              "daily and batched commands produce the same committed outcome without reroll");
    P2CExpect(*CountdownFindCommitment(*original.Countdown(), teacherCommitment.Id) ==
                  *CountdownFindCommitment(*restored.Countdown(), teacherCommitment.Id),
              "contact-shaped committed outcomes survive changed relationships, reload and batching");
    P2CExpect(original.State().Awakening.Stats == restored.State().Awakening.Stats &&
                  original.State().Awakening.AppliedActivityInstances == restored.State().Awakening.AppliedActivityInstances &&
                  restored.State().Awakening.AppliedActivityInstances.size() == owner.AppliedActivityInstances.size() + 1,
              "routine activity completions grow the one owner exactly once across batching and reload");
    P2CExpect(R94Profile(restored.State().Awakening.Granted->StartingProfile) == grant,
              "live ordinary learning never rewrites the grant");
    auto replay = restored.State().Awakening;
    const auto& events = restored.LastCountdownResolveResult()->Week.ActivityEvents;
    const auto credited = ApplyActivityCredits(replay, events);
    P2CExpect(credited.Ok() && credited.Applied == 0 && replay.Stats == restored.State().Awakening.Stats,
              "redelivered completion events are inert");
}

void P37AbsenceAndLateMakeUp(const R94World& world)
{
    const auto appointment = R94At(2043, 1, 23, 9, 0);
    auto sim = P37Bound(world, world.Seeded(appointment - 1, 3702));
    const auto initial = sim.State();
    const auto originalSeq = sim.State().Seq;
    const auto stopped = S04Accept(sim, AdvanceTime{10 * MinutesPerDay}, "a long skip interrupts at compulsory supervision");
    P2CExpect(stopped.Partial && stopped.To == appointment && sim.State().Awakening.MakeUp.Phase == AwakeningMakeUpPhase::Required &&
                  !sim.State().Awakening.Granted && sim.State().Seq == originalSeq + 1 && sim.Log().size() == 1,
              "unattended 09:00 runs the System absence effect inside one advancing transaction");
    ReplayOptions replay;
    replay.School = world.School.Cats.Day;
    replay.Lessons = world.School.School();
    replay.Awakening = P37Binding(world);
    const auto replayed = ReplayCommandLogFromSnapshot(initial, sim.Log(), replay);
    P2CExpect(replayed.Complete && replayed.Hash == HashWorldState(sim.State()),
              "replaying the outer command reproduces the System absence effect without another logged command");
    S04Refused(sim, TutorialAbsent{}, "tutorial.source", "players cannot manufacture the System effect");
    S04Refused(sim, TutorialEnter{}, "tutorial.make_up_required", "absence cannot enter the missed original slot");
    const auto missedSlot = appointment + MinutesPerDay;
    S04Accept(sim, TutorialMakeUp{missedSlot}, "schedule a make-up");
    const auto missed = S04Accept(sim, AdvanceTime{2 * MinutesPerDay}, "a skipped make-up is also a boundary");
    P2CExpect(missed.Partial && missed.To == missedSlot && sim.State().Awakening.MakeUp.Phase == AwakeningMakeUpPhase::Required &&
                  sim.Countdown() && sim.Countdown()->Start == CountdownStartInstant(), "absence still opens countdown on schedule");
    // Honour the three contacts' blocks before reaching the last legal make-up slot.
    S04Accept(sim, CountdownResolveDays{std::vector<WeekDayPlan>(4, P37DayEnd())}, "ordinary promises remain playable before make-up");
    S04Accept(sim, CountdownResolveDays{std::vector<WeekDayPlan>(4, P37DayEnd())}, "news stops the plan after its day");
    S04Accept(sim, CountdownResolveDays{std::vector<WeekDayPlan>(3, P37DayEnd())}, "the teacher's obligation is honoured");
    const auto lastSlot = CountdownEndInstant() - TutorialSessionMinutes - 1;
    const auto deadline = S04Accept(sim, AdvanceTime{CountdownEndInstant() - sim.State().Clock.Now()}, "time stops at the last playable slot");
    P2CExpect(deadline.Partial && deadline.To == lastSlot, "a long skip cannot bypass the final make-up opportunity");
    S04Refused(sim, AdvanceTime{2}, "tutorial.make_up_required", "the last supervised slot cannot be lost");
    auto late = P37Bound(world, P37Reload(world, sim.State()));
    const auto countdownBefore = *late.Countdown();
    S04Accept(late, TutorialMakeUp{lastSlot}, "explicitly book the last legal slot");
    S04Accept(late, TutorialEnter{}, "supervised make-up remains playable at the boundary");
    S04Accept(late, TutorialWithdraw{}, "safe withdrawal still grants one minute before the fixed deadline");
    P2CExpect(late.State().Clock.Now() == CountdownEndInstant() - 1 && late.State().Awakening.Granted &&
                  late.State().Awakening.AppointmentAt == appointment && late.State().Awakening.ExaminationAt == CountdownEndInstant() &&
                  late.Countdown()->End == countdownBefore.End && late.Countdown()->Seed == countdownBefore.Seed &&
                  late.Countdown()->Obligations == countdownBefore.Obligations,
              "late make-up preserves all ten-day history and never resets the deadline");
    S04Accept(late, AdvanceTime{1}, "examination morning remains the original minute");
    P2CExpect(late.Countdown()->Closed && late.CountdownView().ExaminationDue, "the fixed countdown closes normally");
    P37Reload(world, late.State());

    auto exact = P37Bound(world, world.Seeded(appointment - 1, 3703));
    const auto arrived = S04Accept(exact, AdvanceTime{1}, "arrive exactly at 09:00");
    P2CExpect(!arrived.Partial && exact.State().Awakening.MakeUp.Phase == AwakeningMakeUpPhase::NotRequired,
              "reaching the appointment permits attendance rather than declaring absence early");
    S04Accept(exact, TutorialEnter{}, "on-time entry remains available");
    S04Refused(exact, CountdownResolveDays{{P37DayEnd()}}, "tutorial.active", "routine days cannot skip an active supervised session");
}

void P37AtomicTimePaths(const R94World& world)
{
    const auto appointment = R94At(2043, 1, 23, 9, 0);
    auto sim = P37Bound(world, world.Seeded(appointment - 30, 3704));
    S04Accept(sim, SchoolPlanDay{}, "plan before the appointment");
    DayCommand sleep;
    sleep.Kind = DayCommandKind::GoToSleep;
    S04Accept(sim, WorldDayCommand{sleep}, "sleep before supervision");
    DayCommand wake;
    wake.Kind = DayCommandKind::Wake;
    wake.Minutes = 120;
    S04Refused(sim, WorldDayCommand{wake}, "day.charge_interrupted", "a staged sleep cannot bypass the tutorial boundary");
    wake.Minutes = 30;
    S04Accept(sim, WorldDayCommand{wake}, "split sleep at the appointment");
    const auto missed = S04Accept(sim, AdvanceTime{1}, "the now unattended slot issues absence");
    P2CExpect(missed.Partial && missed.To == appointment && sim.State().Awakening.MakeUp.Phase == AwakeningMakeUpPhase::Required,
              "the split time path preserves the System effect");

    auto studying = P37Bound(world, world.Seeded(appointment - 30, 3706));
    S04Accept(studying, SchoolPlanDay{}, "plan ordinary study before the appointment");
    DayCommand study;
    study.Kind = DayCommandKind::Commit;
    study.ActivityId = "activity.study_at_home";
    study.PlaceKind = "home";
    S04Accept(studying, WorldDayCommand{study}, "commit ordinary study");
    study = {};
    study.Kind = DayCommandKind::Start;
    S04Accept(studying, WorldDayCommand{study}, "start study");
    study.Kind = DayCommandKind::Complete;
    S04Refused(studying, WorldDayCommand{study}, "day.charge_interrupted", "an activity cannot silently consume compulsory supervision");
    study.Kind = DayCommandKind::Cancel;
    study.Minutes = 30;
    S04Accept(studying, WorldDayCommand{study}, "stop the activity at the appointment");
    S04Accept(studying, TutorialEnter{}, "the interrupted activity leaves the supervised session reachable");

    auto opening = P37Opening(world, 3705);
    S04Accept(opening, CountdownTakeOpportunity{"opp.ground_trial"}, "reserve an ordinary sports opportunity");
    const auto eventAt = opening.State().Clock.Now() + 30;
    S04Accept(opening, ScheduleWorldEvent{eventAt, "test/p37/first"}, "first event", CommandSource::System);
    S04Accept(opening, ScheduleWorldEvent{eventAt, "test/p37/second"}, "second event", CommandSource::System);
    auto capped = P37Bound(world, opening.State(), nullptr, 1);
    S04Refused(capped, CountdownResolveDays{{P37DayEnd()}}, "countdown.world_busy", "event-capped resolution rolls back time and settlement");
    P2CExpect(!capped.LastCountdownResolveResult(), "a refused command publishes no recap");
    auto whole = P37Bound(world, capped.State());
    S04Accept(whole, CountdownResolveDays{{P37DayEnd()}}, "the same plan can retry with room for events");
    const auto recap = whole.LastCountdownResolveResult()->State;
    S04Refused(whole, CountdownResolveDays{}, "countdown.no_days", "invalid plans retain their contract code");
    P2CExpect(whole.LastCountdownResolveResult()->State == recap, "late refusals retain the previous accepted recap");
}
} // namespace

int main(int argc, char** argv)
{
    P2CExpect(argc == 2, "content root");
    const P2CFixture fixture(argv[1]);
    const R94World world(fixture);
    P37TenDayProperty(world);
    P37OffersAndCredits(world);
    P37AbsenceAndLateMakeUp(world);
    P37AtomicTimePaths(world);
    std::cout << "P3-07 WorldCountdownCommands passed\n";
    return 0;
}
