// ROADMAP M3-01: resolving a declared interaction, the interruption policies, the
// mission clock cap, and the properties that hold over random declarations.
#include "TimeContract.h"
#include "TimeContractTestSupport.h"

using namespace PerfectWorld;

namespace
{
using M = ClockMode;
using I = InterruptPolicy;
using R = ResolutionBoundary;

InteractionDeclaration MissionFixed(std::int32_t minutes, I interrupt = I::None)
{
    InteractionDeclaration d = MakeDecl("test.mission", M::Mission, minutes, interrupt, R::OnComplete);
    d.Mission.FixedMinutes = minutes;
    return d;
}

InteractionDeclaration MissionRated(std::int32_t ratePermille, std::int32_t cap, I interrupt = I::None)
{
    InteractionDeclaration d = MakeDecl("test.mission", M::Mission, cap, interrupt, R::OnComplete);
    d.Mission.RatePermille = ratePermille;
    d.Mission.CapMinutes = cap;
    return d;
}
} // namespace

PerfectWorld::InteractionDeclaration TimeTestRandomDeclaration(TimeTestRng& rng)
{
    for (;;)
    {
        InteractionDeclaration d;
        d.Id = "test.random";
        d.Mode = static_cast<M>(rng.Range(0, 6));
        const ClockModeRule& rule = TimeModeRule(d.Mode);
        if (d.Mode == M::Mission)
        {
            if (rng.Range(0, 1) == 0)
                d.Mission.FixedMinutes = d.CostMinutes = static_cast<std::int32_t>(rng.Range(1, 5000));
            else
            {
                d.Mission.RatePermille = static_cast<std::int32_t>(rng.Range(1, 1000));
                d.Mission.CapMinutes = d.CostMinutes = static_cast<std::int32_t>(rng.Range(1, 5000));
            }
        }
        else
        {
            const std::int64_t top = rng.Range(0, 3) == 0 ? std::min<std::int64_t>(rule.MaxCostMinutes, 3000) : std::min<std::int64_t>(rule.MaxCostMinutes, 100);
            d.CostMinutes = static_cast<std::int32_t>(rng.Range(rule.MinCostMinutes, std::max<std::int64_t>(rule.MinCostMinutes, top)));
        }
        d.Interrupt = static_cast<I>(rng.Range(0, 2));
        d.Resolve = static_cast<R>(rng.Range(0, 2));
        d.Beats = static_cast<std::int32_t>(rng.Range(1, 8));
        if (ValidateDeclaration(d) == TimeContractError::None)
            return d;
    }
}

std::vector<PerfectWorld::TimeEvent> TimeTestRandomEvents(TimeTestRng& rng)
{
    std::vector<TimeEvent> events;
    const std::int64_t count = rng.Range(0, 10);
    for (std::int64_t i = 0; i < count; ++i)
    {
        const std::int64_t pick = rng.Range(0, 99);
        if (pick < 60)
            events.push_back(Progress(rng.Range(0, 9) == 0 ? rng.Range(0, 2000) : rng.Range(0, 40)));
        else if (pick < 82)
            events.push_back(InterruptEvent());
        else if (pick < 94)
            events.push_back(FinishEvent());
        else
            events.push_back(CancelEvent());
    }
    return events;
}

void TestResolveBasics()
{
    // A scene runs to its declared cost and charges it, however far the progress overshoots.
    TimeResolution r = ResolveInteraction(MakeDecl("a.class", M::Scene, 45, I::None, R::OnComplete), {Progress(10), Progress(100)});
    Expect(r.Ok() && r.Outcome == TimeOutcome::Completed && r.ElapsedMinutes == 45 && r.ChargedMinutes == 45, "a scene completes at its cost");
    Expect(r.UnusedProgress == 65 && r.OutcomeBeats == 1 && r.BeatsCompleted == 1 && r.EventsConsumed == 2, "the overshoot is reported, not charged");

    r = ResolveInteraction(MakeDecl("a.class", M::Scene, 45, I::None, R::OnComplete), {Progress(10)});
    Expect(r.Outcome == TimeOutcome::Running && r.ChargedMinutes == 0 && r.ElapsedMinutes == 10, "an on_complete scene charges nothing until it ends");
    r = ResolveInteraction(MakeDecl("a.class", M::Scene, 45, I::None, R::PerBeat, 3), {Progress(20)});
    Expect(r.Outcome == TimeOutcome::Running && r.ChargedMinutes == 15 && r.BeatsCompleted == 1 && r.OutcomeBeats == 1, "a per-beat scene charges each beat as it completes");
    r = ResolveInteraction(MakeDecl("a.class", M::Scene, 45, I::None, R::OnComplete), {});
    Expect(r.Ok() && r.Outcome == TimeOutcome::Running && r.ChargedMinutes == 0 && r.ElapsedMinutes == 0, "nothing happened yet");

    // on_commit: charged and resolved the moment it is committed; nothing after counts.
    r = ResolveInteraction(MakeDecl("a.wait", M::Scene, 30, I::None, R::OnCommit), {Progress(5), InterruptEvent(), CancelEvent()});
    Expect(r.Outcome == TimeOutcome::Completed && r.ChargedMinutes == 30 && r.EventsConsumed == 0 && r.OutcomeBeats == 1, "on_commit charges the whole cost at commit");

    // Cancel: pro rata, no outcome (the Activities cancel rule).
    r = ResolveInteraction(MakeDecl("a.class", M::Scene, 45, I::None, R::OnComplete), {Progress(20), CancelEvent(), Progress(5)});
    Expect(r.Outcome == TimeOutcome::Cancelled && r.ChargedMinutes == 20 && r.OutcomeBeats == 0 && r.EventsConsumed == 2, "a cancelled scene is charged pro rata and has no outcome");
    r = ResolveInteraction(MakeDecl("a.class", M::Scene, 45, I::None, R::PerBeat, 3), {Progress(20), CancelEvent()});
    Expect(r.Outcome == TimeOutcome::Cancelled && r.ChargedMinutes == 20 && r.BeatsCompleted == 1 && r.OutcomeBeats == 1, "a cancelled per-beat scene keeps its completed beats");

    // Finish.
    r = ResolveInteraction(MakeDecl("a.class", M::Scene, 45, I::None, R::OnComplete), {Progress(5), FinishEvent()});
    Expect(r.Error == TimeContractError::InvalidEvent, "a scene cannot be finished early, only cancelled");
    r = ResolveInteraction(MakeDecl("a.talk", M::Conversation, 30, I::None, R::OnComplete, 3), {Progress(8), FinishEvent()});
    Expect(r.Outcome == TimeOutcome::Completed && r.ChargedMinutes == 8 && r.OutcomeBeats == 3, "a talk may close early and is charged what it used");
    r = ResolveInteraction(MakeDecl("a.walk", M::FreeRoam, 0, I::None, R::OnComplete), {Progress(5), Progress(7), FinishEvent()});
    Expect(r.Outcome == TimeOutcome::Completed && r.ChargedMinutes == 12 && r.ElapsedMinutes == 12, "open-ended free roam charges the minutes that ran");
    r = ResolveInteraction(MakeDecl("a.run", M::FreeRoam, 30, I::None, R::OnComplete), {Progress(12), Progress(40)});
    Expect(r.Outcome == TimeOutcome::Completed && r.ChargedMinutes == 30, "a declared free-roam activity ends at its cost");

    // Invalid input.
    r = ResolveInteraction(MakeDecl("a.class", M::Scene, 45, I::None, R::OnComplete), {Progress(-1)});
    Expect(r.Error == TimeContractError::InvalidEvent, "negative time is refused");
    r = ResolveInteraction(MakeDecl("a.class", M::Scene, 45, I::None, R::OnComplete), {Progress(MaxSingleAdvanceMinutes + 1)});
    Expect(r.Error == TimeContractError::InvalidEvent, "an absurd advance is refused");
    r = ResolveInteraction(MakeDecl("a.class", M::Scene, 0, I::None, R::OnComplete), {});
    Expect(r.Error == TimeContractError::CostOutOfRange && !r.Ok(), "an unsound declaration resolves to its error");
    TimeContractError error = TimeContractError::None;
    InteractionRun run = BeginInteraction(MakeDecl("a.class", M::Scene, 45, I::None, R::OnComplete), error);
    Expect(StepInteraction(run, Progress(45)) == StepResult::Consumed, "step to the end");
    const InteractionRun done = run;
    Expect(StepInteraction(run, Progress(1)) == StepResult::AlreadyResolved && run == done, "a resolved run is untouched by further events");
    (void)BeginInteraction(MakeDecl("A", M::Scene, 45, I::None, R::OnComplete), error);
    Expect(error == TimeContractError::BadId, "Begin reports why a declaration is refused");

    // The pauses never consume opportunity time, whatever happens during them.
    for (const M paused : {M::UiPaused, M::AccessibilityPause})
    {
        r = ResolveInteraction(MakeDecl("a.menu", paused, 0, I::None, R::OnComplete), {Progress(600), InterruptEvent(), Progress(100000), FinishEvent()});
        Expect(r.Outcome == TimeOutcome::Completed && r.ChargedMinutes == 0 && r.ElapsedMinutes == 0, "a pause adds no opportunity time");
        Expect(r.InterruptsHeld == 1, "an interrupt waits until the pause ends");
        r = ResolveInteraction(MakeDecl("a.menu", paused, 0, I::None, R::OnCommit), {});
        Expect(r.Outcome == TimeOutcome::Completed && r.ChargedMinutes == 0, "opening a menu costs nothing");
    }

    // A conversation never exceeds its declared cost; a free one costs nothing.
    r = ResolveInteraction(MakeDecl("a.talk", M::Conversation, 10, I::None, R::OnComplete, 2), {Progress(3), Progress(500)});
    Expect(r.Outcome == TimeOutcome::Completed && r.ChargedMinutes == 10 && r.ElapsedMinutes == 10, "a three-minute talk cannot cost more than it declared");
    r = ResolveInteraction(MakeDecl("a.talk", M::Conversation, 0, I::None, R::OnComplete), {Progress(500), FinishEvent()});
    Expect(r.Outcome == TimeOutcome::Completed && r.ChargedMinutes == 0 && r.ElapsedMinutes == 0, "a conversation that declared no cost is free however long it runs");
    r = ResolveInteraction(MakeDecl("a.talk", M::Conversation, 20, I::AtBeat, R::OnComplete, 4), {Progress(7), InterruptEvent(), Progress(100)});
    Expect(r.Outcome == TimeOutcome::InterruptedAtBeat && r.ChargedMinutes == 10 && r.BeatsCompleted == 2, "an interrupted talk ends at the exchange");

    // Compression stops only on a whole beat.
    const InteractionDeclaration week = MakeDecl("a.week", M::Compressed, 7 * 1440, I::AtBeat, R::PerBeat, 7);
    r = ResolveInteraction(week, {Progress(3 * 1440 + 500), CancelEvent()});
    Expect(r.Outcome == TimeOutcome::Cancelled && r.ElapsedMinutes == 3 * 1440 && r.ChargedMinutes == 3 * 1440 && r.OutcomeBeats == 3, "a stopped compression keeps only whole days");
    r = ResolveInteraction(week, {Progress(3 * 1440 + 500), InterruptEvent(), Progress(5000)});
    Expect(r.Outcome == TimeOutcome::InterruptedAtBeat && r.ElapsedMinutes == 4 * 1440 && r.BeatsCompleted == 4, "an interrupt stops the span at the next day boundary");
    r = ResolveInteraction(week, {Progress(7 * 1440)});
    Expect(r.Outcome == TimeOutcome::Completed && r.ChargedMinutes == 7 * 1440 && r.OutcomeBeats == 7, "a compressed week charges the span resolved");

    // Free roam one game minute per real second: the declared ticks are minutes.
    Expect(TimeModeRule(M::FreeRoam).WorldMilliMinutesPerRealSecond * 60 == 60'000, "sixty real seconds are sixty thousand milli-minutes");
}

void TestInterruptionPolicies()
{
    // none: held, the activity runs through.
    TimeResolution r = ResolveInteraction(MakeDecl("a.x", M::Scene, 45, I::None, R::OnComplete), {Progress(10), InterruptEvent(), InterruptEvent(), Progress(100)});
    Expect(r.Outcome == TimeOutcome::Completed && r.InterruptsHeld == 2 && r.ChargedMinutes == 45, "interrupt none: held until the end");

    // at_beat: 45 minutes in three beats of 15.
    const InteractionDeclaration beats = MakeDecl("a.x", M::Scene, 45, I::AtBeat, R::PerBeat, 3);
    r = ResolveInteraction(beats, {Progress(20), InterruptEvent(), Progress(30)});
    Expect(r.Outcome == TimeOutcome::InterruptedAtBeat && r.ElapsedMinutes == 30 && r.ChargedMinutes == 30, "at_beat waits for the next beat");
    Expect(r.UnusedProgress == 20 && r.BeatsCompleted == 2 && r.OutcomeBeats == 2, "the rest of the progress is returned, two beats count");
    r = ResolveInteraction(MakeDecl("a.x", M::Scene, 45, I::AtBeat, R::OnComplete, 3), {Progress(20), InterruptEvent(), Progress(30)});
    Expect(r.Outcome == TimeOutcome::InterruptedAtBeat && r.ChargedMinutes == 30 && r.OutcomeBeats == 0, "an on_complete scene stopped at a beat applies no outcome");
    r = ResolveInteraction(beats, {Progress(15), InterruptEvent(), Progress(30)});
    Expect(r.Outcome == TimeOutcome::InterruptedAtBeat && r.ElapsedMinutes == 15 && r.EventsConsumed == 2, "an interrupt exactly on a beat stops at once");
    r = ResolveInteraction(beats, {InterruptEvent(), Progress(30)});
    Expect(r.Outcome == TimeOutcome::InterruptedAtBeat && r.ElapsedMinutes == 0 && r.ChargedMinutes == 0, "an interrupt before the start stops with nothing charged");
    r = ResolveInteraction(beats, {Progress(40), InterruptEvent(), Progress(2), Progress(10)});
    Expect(r.Outcome == TimeOutcome::Completed && r.ElapsedMinutes == 45 && r.OutcomeBeats == 3, "an interrupt in the last beat lets it finish");
    r = ResolveInteraction(beats, {Progress(20), InterruptEvent(), Progress(5), InterruptEvent(), Progress(3), Progress(30)});
    Expect(r.Outcome == TimeOutcome::InterruptedAtBeat && r.ElapsedMinutes == 30 && r.InterruptsHeld == 1, "the first interrupt decides where it stops; later ones are held");
    r = ResolveInteraction(beats, {Progress(20), InterruptEvent(), CancelEvent()});
    Expect(r.Outcome == TimeOutcome::Cancelled && r.ElapsedMinutes == 20, "leaving beats waiting for the beat");
    const InteractionDeclaration uneven = MakeDecl("a.x", M::Scene, 10, I::AtBeat, R::PerBeat, 3);
    r = ResolveInteraction(uneven, {Progress(4), InterruptEvent(), Progress(9)});
    Expect(r.ElapsedMinutes == 6 && r.BeatsCompleted == 2, "uneven beats stop on their floored boundaries (3, 6, 10)");

    // immediate: ends where it is, elapsed charged pro rata.
    const InteractionDeclaration sleep = MakeDecl("a.sleep", M::Scene, 480, I::Immediate, R::OnComplete);
    r = ResolveInteraction(sleep, {Progress(130), InterruptEvent(), Progress(999)});
    Expect(r.Outcome == TimeOutcome::InterruptedImmediately && r.ElapsedMinutes == 130 && r.ChargedMinutes == 130 && r.OutcomeBeats == 0, "immediate: a breach ends sleep and charges the minutes slept");
    Expect(r.EventsConsumed == 2, "events after an immediate stop are not consumed");
    r = ResolveInteraction(MakeDecl("a.run", M::FreeRoam, 30, I::Immediate, R::OnComplete), {Progress(11), InterruptEvent()});
    Expect(r.Outcome == TimeOutcome::InterruptedImmediately && r.ChargedMinutes == 11, "immediate in free roam");

    // A compressed span can only be interrupted at a beat.
    r = ResolveInteraction(MakeDecl("a.week", M::Compressed, 7 * 1440, I::None, R::OnComplete), {Progress(2000), InterruptEvent(), Progress(9000)});
    Expect(r.Outcome == TimeOutcome::Completed && r.InterruptsHeld == 1 && r.ChargedMinutes == 7 * 1440, "an unattended week with interrupt none runs through");
}

void TestMissionClock()
{
    const InteractionDeclaration rated = MissionRated(500, 100);
    Expect(MissionOutsideMinutes(rated.Mission, 0) == 0 && MissionOutsideMinutes(rated.Mission, 100) == 50 && MissionOutsideMinutes(rated.Mission, 101) == 50,
           "mission minutes times the rate");
    Expect(MissionOutsideMinutes(rated.Mission, 200) == 100 && MissionOutsideMinutes(rated.Mission, 9'000'000) == 100, "the cap holds however long the mission runs");
    Expect(MissionOutsideMinutes(rated.Mission, -5) == 0, "negative mission time adds nothing");
    Expect(MissionOutsideMinutes(MissionFixed(30).Mission, 1) == 30 && MissionOutsideMinutes(MissionFixed(30).Mission, 99999) == 30, "a fixed mission costs its fixed minutes");

    TimeResolution r = ResolveInteraction(rated, {Progress(60), Progress(40), FinishEvent()});
    Expect(r.Outcome == TimeOutcome::Completed && r.ElapsedMinutes == 100 && r.ChargedMinutes == 50 && r.OutcomeBeats == 1, "a mission is charged mission minutes times rate at exit");
    r = ResolveInteraction(rated, {Progress(5000), FinishEvent()});
    Expect(r.ElapsedMinutes == 5000 && r.ChargedMinutes == 100, "the mission clock caps how fast the outside world advances");
    r = ResolveInteraction(rated, {Progress(100)});
    Expect(r.Outcome == TimeOutcome::Running && r.ChargedMinutes == 0, "the outside world waits while the mission runs");
    r = ResolveInteraction(rated, {Progress(40), InterruptEvent(), Progress(60)});
    Expect(r.Outcome == TimeOutcome::Running, "interrupt none: held in a mission");
    Expect(r.InterruptsHeld == 1 && r.ElapsedMinutes == 100, "the mission clock keeps running through a held interrupt");
    r = ResolveInteraction(MissionFixed(600, I::Immediate), {Progress(7), InterruptEvent()});
    Expect(r.Outcome == TimeOutcome::InterruptedImmediately && r.ChargedMinutes == 600, "ejection charges the fixed minutes");
    r = ResolveInteraction(MissionRated(250, 2880, I::Immediate), {Progress(400), InterruptEvent()});
    Expect(r.Outcome == TimeOutcome::InterruptedImmediately && r.ChargedMinutes == 100, "ejection charges the rate for the minutes spent");
    r = ResolveInteraction(MissionRated(250, 2880), {Progress(40), CancelEvent()});
    Expect(r.Outcome == TimeOutcome::Cancelled && r.ChargedMinutes == 10 && r.OutcomeBeats == 0, "leaving a mission early still charges the rate");
    r = ResolveInteraction(MissionRated(1000, 60 * 1440), {Progress(3 * 1440), FinishEvent()});
    Expect(r.ChargedMinutes == 3 * 1440, "at the top rate the outside world keeps pace with the mission clock, never faster");
    for (std::int64_t minutes : {0, 1, 99, 1000, 100000})
        Expect(MissionOutsideMinutes(MissionRated(1000, 5000).Mission, minutes) <= minutes, "the outside world never outruns the mission clock");
}

void TestRandomResolution()
{
    TimeTestRng rng(20260711);
    for (int trial = 0; trial < 10000; ++trial)
    {
        const InteractionDeclaration d = TimeTestRandomDeclaration(rng);
        const std::vector<TimeEvent> events = TimeTestRandomEvents(rng);
        const TimeResolution a = ResolveInteraction(d, events);
        const TimeResolution b = ResolveInteraction(d, events);
        Expect(SameResolution(a, b) && a.EventsConsumed == b.EventsConsumed && a.UnusedProgress == b.UnusedProgress, "resolution is deterministic");
        Expect(a.ChargedMinutes >= 0 && a.ElapsedMinutes >= 0 && a.OutcomeBeats >= 0 && a.OutcomeBeats <= d.Beats, "no negative or excess quantities");
        if (d.CostMinutes > 0 || d.Mode != M::FreeRoam)
            Expect(a.ChargedMinutes <= d.CostMinutes, "no interaction is ever charged more than it declared");
        if (!TimeModeRule(d.Mode).ChargesOpportunityTime)
            Expect(a.ChargedMinutes == 0 && a.ElapsedMinutes == 0, "a pause never consumes opportunity time");
        if (d.Mode == M::Conversation && d.CostMinutes == 0)
            Expect(a.ChargedMinutes == 0, "a free conversation is free");
        if (d.Mode == M::Mission)
            Expect(a.ChargedMinutes <= d.CostMinutes && a.ChargedMinutes <= a.ElapsedMinutes + d.Mission.FixedMinutes, "the mission clock caps the outside world");
        if (d.Resolve == R::OnComplete && a.Outcome != TimeOutcome::Completed)
            Expect(a.OutcomeBeats == 0, "an unfinished on_complete interaction applies no outcome");

        // Additivity: splitting any progress event anywhere changes nothing that must match.
        std::vector<TimeEvent> split;
        for (const TimeEvent& event : events)
        {
            std::int64_t left = event.Minutes;
            if (event.Kind != TimeEventKind::Progress || left < 2)
            {
                split.push_back(event);
                continue;
            }
            while (left > 0)
            {
                const std::int64_t piece = rng.Range(1, left);
                split.push_back(Progress(piece));
                left -= piece;
            }
        }
        Expect(SameResolution(a, ResolveInteraction(d, split)), "splitting progress does not change the resolution");

        // Save and load at every step equals an uninterrupted run, and the text round-trips.
        TimeContractError error = TimeContractError::None;
        InteractionRun live = BeginInteraction(d, error);
        InteractionRun reloaded = live;
        for (const TimeEvent& event : events)
        {
            const std::string text = SerialiseInteractionRun(live);
            Expect(!text.empty(), "every reachable run serialises");
            const InteractionRunParse parsed = DeserialiseInteractionRun(text);
            Expect(parsed.Ok() && parsed.Run == live, "every reachable run round-trips");
            reloaded = parsed.Run;
            const StepResult one = StepInteraction(live, event);
            const StepResult two = StepInteraction(reloaded, event);
            Expect(one == two && live == reloaded, "a reloaded run continues exactly like the live one");
            if (one != StepResult::Consumed)
                break;
        }
    }
}
