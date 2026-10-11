#include "TimeContract.h"

#include <algorithm>

// Resolving a declared interaction under interruptions (ROADMAP M3-01; SYSTEMS §2.5
// rule 4). The rules are in TimeContract.h. ResolveInteraction is the fold of
// StepInteraction, so stepping minute by minute and jumping in one go share one path.
namespace PerfectWorld
{
namespace
{
/** Mission progress has no declared length: it is bounded by neither the cost nor a beat. */
bool TimeContractBounded(const InteractionDeclaration& declaration)
{
    return declaration.Mode != ClockMode::Mission && declaration.CostMinutes > 0;
}

std::int32_t TimeContractBeatsDone(const InteractionRun& run)
{
    if (run.Outcome == TimeOutcome::Completed)
        return run.Decl.Beats;
    if (run.Decl.Mode == ClockMode::Mission)
        return 0;
    return BeatsCoveredBy(run.Decl, run.Progress);
}

/** Game minutes charged to the world clock at this point (so far, while the run is still going). */
std::int64_t TimeContractCharged(const InteractionRun& run)
{
    const bool running = run.Outcome == TimeOutcome::Running;
    switch (TimeModeRule(run.Decl.Mode).Charge)
    {
    case ChargeRule::Never:
        return 0;
    case ChargeRule::MissionFormula:
        return running ? 0 : MissionOutsideMinutes(run.Decl.Mission, run.Progress);
    case ChargeRule::PerRealSecond:
    case ChargeRule::SpanResolved:
        // The clock runs live in free roam and jumps as a span resolves: time already passed is already charged.
        return run.Progress;
    case ChargeRule::DeclaredAtClose:
    case ChargeRule::DeclaredAtResolution:
        if (!running)
            return run.Progress;
        // Still going: nothing is due until the boundary, except the beats that have already completed.
        return run.Decl.Resolve == ResolutionBoundary::PerBeat ? BeatBoundary(run.Decl, BeatsCoveredBy(run.Decl, run.Progress)) : 0;
    }
    return 0;
}

std::int32_t TimeContractOutcomeBeats(const InteractionRun& run)
{
    if (run.Decl.Resolve == ResolutionBoundary::OnCommit)
        return run.Decl.Beats;
    if (run.Decl.Resolve == ResolutionBoundary::OnComplete)
        return run.Outcome == TimeOutcome::Completed ? run.Decl.Beats : 0;
    return TimeContractBeatsDone(run);
}

void TimeContractResolveAt(InteractionRun& run, TimeOutcome outcome)
{
    run.Outcome = outcome;
    // A compressed span only stops on a whole beat: the partial beat was never resolved.
    if (run.Decl.Mode == ClockMode::Compressed && outcome != TimeOutcome::Completed)
        run.Progress = BeatBoundary(run.Decl, BeatsCoveredBy(run.Decl, run.Progress));
}

StepResult TimeContractStepProgress(InteractionRun& run, std::int64_t minutes)
{
    if (minutes < 0 || minutes > MaxSingleAdvanceMinutes || run.Progress + minutes > MaxSingleAdvanceMinutes)
        return StepResult::Invalid;
    ++run.EventsConsumed;
    if (InteractionAbsorbsProgress(run.Decl))
        return StepResult::Consumed; // a paused mode absorbs real time: nothing accrues
    if (!TimeContractBounded(run.Decl))
    {
        run.Progress += minutes;
        return StepResult::Consumed;
    }
    const std::int64_t cost = run.Decl.CostMinutes;
    const std::int64_t limit = run.StopPoint >= 0 ? std::min(cost, run.StopPoint) : cost;
    if (run.Progress + minutes < limit)
    {
        run.Progress += minutes;
        return StepResult::Consumed;
    }
    run.UnusedProgress = run.Progress + minutes - limit;
    run.Progress = limit;
    // Reaching the cost completes it even when a stop was pending on the same boundary.
    TimeContractResolveAt(run, limit == cost ? TimeOutcome::Completed : TimeOutcome::InterruptedAtBeat);
    return StepResult::Consumed;
}

StepResult TimeContractStepInterrupt(InteractionRun& run)
{
    ++run.EventsConsumed;
    switch (run.Decl.Interrupt)
    {
    case InterruptPolicy::None:
        ++run.InterruptsHeld;
        break;
    case InterruptPolicy::Immediate:
        TimeContractResolveAt(run, TimeOutcome::InterruptedImmediately);
        break;
    case InterruptPolicy::AtBeat:
    {
        if (run.StopPoint >= 0)
        {
            ++run.InterruptsHeld; // the first interrupt already decided where to stop
            break;
        }
        const std::int64_t cost = run.Decl.CostMinutes;
        const std::int64_t beat = (run.Progress * run.Decl.Beats + cost - 1) / cost; // smallest boundary at or after here
        const std::int64_t point = BeatBoundary(run.Decl, static_cast<std::int32_t>(beat));
        if (point == run.Progress)
            TimeContractResolveAt(run, TimeOutcome::InterruptedAtBeat);
        else
            run.StopPoint = point;
        break;
    }
    }
    return StepResult::Consumed;
}
} // namespace

InteractionRun BeginInteraction(const InteractionDeclaration& declaration, TimeContractError& error)
{
    InteractionRun run;
    error = ValidateDeclaration(declaration);
    if (error != TimeContractError::None)
        return run;
    run.Decl = declaration;
    if (declaration.Resolve == ResolutionBoundary::OnCommit)
    {
        run.Progress = declaration.CostMinutes;
        run.Outcome = TimeOutcome::Completed;
    }
    return run;
}

StepResult StepInteraction(InteractionRun& run, const TimeEvent& event)
{
    if (run.Outcome != TimeOutcome::Running)
        return StepResult::AlreadyResolved;
    switch (event.Kind)
    {
    case TimeEventKind::Progress:
        return TimeContractStepProgress(run, event.Minutes);
    case TimeEventKind::Interrupt:
        return TimeContractStepInterrupt(run);
    case TimeEventKind::Finish:
        if (!TimeModeRule(run.Decl.Mode).EarlyFinishAllowed)
            return StepResult::Invalid;
        ++run.EventsConsumed;
        TimeContractResolveAt(run, TimeOutcome::Completed);
        return StepResult::Consumed;
    case TimeEventKind::Cancel:
        ++run.EventsConsumed;
        TimeContractResolveAt(run, TimeOutcome::Cancelled);
        return StepResult::Consumed;
    }
    return StepResult::Invalid;
}

TimeResolution DescribeInteraction(const InteractionRun& run)
{
    TimeResolution resolution;
    resolution.Outcome = run.Outcome;
    resolution.ElapsedMinutes = run.Progress;
    resolution.ChargedMinutes = TimeContractCharged(run);
    resolution.BeatsCompleted = TimeContractBeatsDone(run);
    resolution.OutcomeBeats = TimeContractOutcomeBeats(run);
    resolution.InterruptsHeld = run.InterruptsHeld;
    resolution.EventsConsumed = run.EventsConsumed;
    resolution.UnusedProgress = run.UnusedProgress;
    return resolution;
}

TimeResolution ResolveInteraction(const InteractionDeclaration& declaration, const std::vector<TimeEvent>& events)
{
    TimeContractError error = TimeContractError::None;
    InteractionRun run = BeginInteraction(declaration, error);
    if (error != TimeContractError::None)
    {
        TimeResolution refused;
        refused.Error = error;
        return refused;
    }
    for (const TimeEvent& event : events)
    {
        const StepResult step = StepInteraction(run, event);
        if (step == StepResult::AlreadyResolved)
            break;
        if (step == StepResult::Invalid)
        {
            TimeResolution invalid = DescribeInteraction(run);
            invalid.Error = TimeContractError::InvalidEvent;
            return invalid;
        }
    }
    return DescribeInteraction(run);
}
} // namespace PerfectWorld
