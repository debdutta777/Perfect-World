#include "DayLoop.h"

#include <algorithm>
#include <limits>
#include <utility>

// ApplyDayCommand (ROADMAP M2-12). Each command runs on a working copy of the
// state and only an accepted, still-valid result replaces the caller's state,
// so a refusal changes nothing.
namespace PerfectWorld
{
namespace
{
struct DayLoopWork
{
    DayLoopState State;
    const DayLoopCatalogues& Cats;
    const DayCommand& Command;
    DayLoopResult Result;
    /** The activity the command was about, for the last-action record. */
    std::string LastActivity;
};

DayLoopResult DayLoopRefuse(DayLoopError error, std::string detail = {})
{
    DayLoopResult result;
    result.Error = error;
    result.Detail = std::move(detail);
    return result;
}

DayLoopEvent DayLoopEventAt(DayLoopEventKind kind, const DayLoopState& state)
{
    DayLoopEvent event;
    event.Kind = kind;
    event.At = state.Now;
    return event;
}

/** The refusal shared by every command that needs an awake player who has made the morning plan. */
std::optional<DayLoopResult> DayLoopNeedAwakePlanned(const DayLoopState& state)
{
    if (state.Asleep)
        return DayLoopRefuse(DayLoopError::Asleep);
    if (!state.Planned)
        return DayLoopRefuse(DayLoopError::PlanFirst);
    return std::nullopt;
}

ConditionActivity DayLoopPassActivity(DayPassKind kind, bool& valid)
{
    valid = true;
    switch (kind)
    {
    case DayPassKind::Wait:
        return ConditionActivity::Awake;
    case DayPassKind::Rest:
        return ConditionActivity::Resting;
    case DayPassKind::Walk:
        return ConditionActivity::LightExertion;
    }
    valid = false;
    return ConditionActivity::Awake;
}

DayLoopResult DayLoopPlanDay(DayLoopWork& w)
{
    DayLoopState& s = w.State;
    if (s.Asleep)
        return DayLoopRefuse(DayLoopError::Asleep);
    if (w.Cats.Activities == nullptr)
        return DayLoopRefuse(DayLoopError::MissingCatalogue);
    if (w.Command.Plan.size() > DayLoopMaxPlanItems)
        return DayLoopRefuse(DayLoopError::InvalidCommand, "the plan has too many items");
    std::int64_t earliestNext = 0;
    for (const DayPlanItem& item : w.Command.Plan)
    {
        const ActivityDef* def = w.Cats.Activities->Find(item.ActivityId);
        if (def == nullptr)
            return DayLoopRefuse(DayLoopError::UnknownActivity, item.ActivityId);
        if (item.StartMinute < 0 || item.StartMinute >= MinutesPerDay || item.StartMinute < earliestNext)
            return DayLoopRefuse(DayLoopError::InvalidCommand, "a plan item starts outside the day or overlaps the one before");
        earliestNext = static_cast<std::int64_t>(item.StartMinute) + def->DurationMinutes;
    }
    s.Plan = w.Command.Plan;
    s.Planned = true;
    w.Result.Events.push_back(DayLoopEventAt(DayLoopEventKind::Planned, s));
    return w.Result;
}

DayLoopResult DayLoopCommit(DayLoopWork& w)
{
    DayLoopState& s = w.State;
    if (auto refusal = DayLoopNeedAwakePlanned(s))
        return *refusal;
    if (s.HasCommitment)
        return DayLoopRefuse(DayLoopError::AlreadyCommitted);
    if (w.Cats.Activities == nullptr)
        return DayLoopRefuse(DayLoopError::MissingCatalogue);
    const DayCommand& c = w.Command;
    const ActivityDef* def = w.Cats.Activities->Find(c.ActivityId);
    if (def == nullptr)
        return DayLoopRefuse(DayLoopError::UnknownActivity, c.ActivityId);
    if (!IsReceiptIdWellFormed(c.PlaceKind))
        return DayLoopRefuse(DayLoopError::InvalidCommand, "the place kind is not a token");
    if (def->Place != ActivityAnyPlace && def->Place != c.PlaceKind)
        return DayLoopRefuse(DayLoopError::WrongPlace, std::string(ActivityErrorCode(ActivityError::WrongPlace)));
    if (c.Alternatives.size() > DayLoopMaxAlternatives ||
        !std::all_of(c.Alternatives.begin(), c.Alternatives.end(),
                     [](const ReceiptCandidate& candidate) { return IsReceiptCandidateValid(candidate); }))
        return DayLoopRefuse(DayLoopError::InvalidCommand, "the alternatives are too many or not valid");

    // Refuse now what could not be completed later: a practice window or a receipt book that cannot take it.
    const DayActivityForecast forecast = ForecastDayActivity(s, *w.Cats.Activities, c.ActivityId, s.Now);
    if (!forecast.Ok)
        return DayLoopRefuse(DayLoopError::PracticeRefused, std::string(PracticeErrorCode(forecast.Practice.Error)));
    ReceiptState trial = s.Receipts;
    const ReceiptRecordResult recorded = RecordCommitment(
        trial, {c.ActivityId, s.Now, s.Now, def->DurationMinutes, forecast.Receipt}, c.Alternatives);
    if (!recorded.Ok())
        return DayLoopRefuse(DayLoopError::ReceiptRefused, std::string(ReceiptErrorCode(recorded.Error)));

    s.HasCommitment = true;
    s.Commitment = {c.ActivityId, c.PlaceKind, s.Now, c.Alternatives};
    w.LastActivity = c.ActivityId;
    DayLoopEvent event = DayLoopEventAt(DayLoopEventKind::Committed, s);
    event.ActivityId = c.ActivityId;
    event.Minutes = def->DurationMinutes;
    w.Result.Events.push_back(std::move(event));
    return w.Result;
}

DayLoopResult DayLoopStart(DayLoopWork& w)
{
    DayLoopState& s = w.State;
    if (auto refusal = DayLoopNeedAwakePlanned(s))
        return *refusal;
    if (s.Runner.Active())
        return DayLoopRefuse(DayLoopError::Busy);
    if (!s.HasCommitment)
        return DayLoopRefuse(DayLoopError::NotCommitted);
    if (w.Cats.Activities == nullptr)
        return DayLoopRefuse(DayLoopError::MissingCatalogue);
    const ActivityResult started =
        StartActivity(s.Runner, *w.Cats.Activities, s.Commitment.ActivityId, s.Now, s.Commitment.PlaceKind);
    if (!started.Ok())
        return DayLoopRefuse(DayLoopError::ActivityRefused, std::string(ActivityErrorCode(started.Error)));
    w.LastActivity = s.Commitment.ActivityId;
    DayLoopEvent event = DayLoopEventAt(DayLoopEventKind::Started, s);
    event.ActivityId = s.Commitment.ActivityId;
    event.Instance = started.Events.front().Instance;
    event.Minutes = started.Events.front().Minutes;
    w.Result.Events.push_back(std::move(event));
    return w.Result;
}

DayLoopResult DayLoopComplete(DayLoopWork& w)
{
    DayLoopState& s = w.State;
    if (!s.Runner.Active() || !s.HasCommitment)
        return DayLoopRefuse(DayLoopError::NotRunning);
    if (w.Cats.Activities == nullptr)
        return DayLoopRefuse(DayLoopError::MissingCatalogue);
    const ActivityDef* def = w.Cats.Activities->Find(s.Runner.ActivityId);
    if (def == nullptr)
        return DayLoopRefuse(DayLoopError::UnknownActivity, s.Runner.ActivityId);
    const GameInstant startedAt = s.Runner.StartedAt;
    const GameInstant end = startedAt + def->DurationMinutes;
    if (end > DayLoopMaxInstant)
        return DayLoopRefuse(DayLoopError::InvalidTime);

    // The forecast is taken before anything is recorded, at the activity's own start, so it is the credit.
    const DayActivityForecast forecast = ForecastDayActivity(s, *w.Cats.Activities, def->Id, startedAt);
    if (!forecast.Ok)
        return DayLoopRefuse(DayLoopError::PracticeRefused, std::string(PracticeErrorCode(forecast.Practice.Error)));
    const ActivityResult done = CompleteActivity(s.Runner, *w.Cats.Activities, s.Condition, end);
    if (!done.Ok())
        return DayLoopRefuse(DayLoopError::ActivityRefused, std::string(ActivityErrorCode(done.Error)));
    const ActivityEvent& finished = done.Events.front();

    DayLoopEvent event;
    event.Kind = DayLoopEventKind::Completed;
    event.At = end;
    event.ActivityId = def->Id;
    event.Instance = finished.Instance;
    event.Minutes = finished.Minutes;
    event.Outcome = finished.Outcome;
    event.Evidence = finished.Evidence;
    if (!finished.Learning.empty())
    {
        const PracticeForecast practice = RecordPractice(s.Practice, {forecast.FamilyId, finished.Learning, startedAt});
        if (!practice.Ok())
            return DayLoopRefuse(DayLoopError::PracticeRefused, std::string(PracticeErrorCode(practice.Error)));
        for (const SkillCredit& credit : practice.Credit)
        {
            if (credit.MilliXp > 0)
                event.Credited.push_back(credit);
        }
        event.Label = practice.Label;
        event.Consolidates = practice.Consolidates;
    }
    const ReceiptRecordResult receipt = RecordCommitment(
        s.Receipts, {def->Id, s.Commitment.DecidedAt, startedAt, def->DurationMinutes, forecast.Receipt},
        s.Commitment.Alternatives);
    if (!receipt.Ok())
        return DayLoopRefuse(DayLoopError::ReceiptRefused, std::string(ReceiptErrorCode(receipt.Error)));
    event.ReceiptSeq = receipt.Seq;

    s.Now = end;
    s.HasCommitment = false;
    s.Commitment = {};
    w.LastActivity = def->Id;
    w.Result.Events.push_back(std::move(event));
    return w.Result;
}

DayLoopResult DayLoopCancel(DayLoopWork& w)
{
    DayLoopState& s = w.State;
    if (!s.HasCommitment)
        return DayLoopRefuse(DayLoopError::NotCommitted);
    const std::string activityId = s.Commitment.ActivityId;
    DayLoopEvent event;
    event.Kind = DayLoopEventKind::Cancelled;
    event.ActivityId = activityId;
    if (s.Runner.Active())
    {
        if (w.Cats.Activities == nullptr)
            return DayLoopRefuse(DayLoopError::MissingCatalogue);
        const ActivityDef* def = w.Cats.Activities->Find(activityId);
        if (def == nullptr)
            return DayLoopRefuse(DayLoopError::UnknownActivity, activityId);
        const std::int64_t elapsed = w.Command.Minutes;
        if (elapsed < 0 || elapsed >= def->DurationMinutes)
            return DayLoopRefuse(DayLoopError::InvalidCommand, "the elapsed minutes must be less than the duration");
        const GameInstant at = s.Runner.StartedAt + elapsed;
        const ActivityResult cancelled = CancelActivity(s.Runner, *w.Cats.Activities, s.Condition, at);
        if (!cancelled.Ok())
            return DayLoopRefuse(DayLoopError::ActivityRefused, std::string(ActivityErrorCode(cancelled.Error)));
        s.Now = at;
        event.Instance = cancelled.Events.front().Instance;
        event.Minutes = cancelled.Events.front().Minutes;
    }
    else if (w.Command.Minutes != 0)
        return DayLoopRefuse(DayLoopError::InvalidCommand, "a commitment that has not started has no elapsed minutes");
    event.At = s.Now;
    s.HasCommitment = false;
    s.Commitment = {};
    w.LastActivity = activityId;
    w.Result.Events.push_back(std::move(event));
    return w.Result;
}

DayLoopResult DayLoopPass(DayLoopWork& w)
{
    DayLoopState& s = w.State;
    if (auto refusal = DayLoopNeedAwakePlanned(s))
        return *refusal;
    if (s.Runner.Active())
        return DayLoopRefuse(DayLoopError::Busy);
    const std::int64_t minutes = w.Command.Minutes;
    bool valid = false;
    const ConditionActivity activity = DayLoopPassActivity(w.Command.Pass, valid);
    if (!valid || minutes < 1 || minutes > DayLoopMaxPassMinutes)
        return DayLoopRefuse(DayLoopError::InvalidCommand, "pass needs a known kind and 1 to 1440 minutes");
    if (s.Now + minutes > DayLoopMaxInstant)
        return DayLoopRefuse(DayLoopError::InvalidTime);
    if (!AdvanceCondition(s.Condition, activity, minutes))
        return DayLoopRefuse(DayLoopError::ConditionRefused);
    s.Now += minutes;
    DayLoopEvent event = DayLoopEventAt(DayLoopEventKind::Passed, s);
    event.Minutes = minutes;
    w.Result.Events.push_back(std::move(event));
    return w.Result;
}

DayLoopResult DayLoopArriveSchool(DayLoopWork& w)
{
    DayLoopState& s = w.State;
    if (auto refusal = DayLoopNeedAwakePlanned(s))
        return *refusal;
    if (s.Runner.Active())
        return DayLoopRefuse(DayLoopError::Busy);
    if (w.Cats.School == nullptr)
        return DayLoopRefuse(DayLoopError::NoSchool);
    const BellSchedule& school = *w.Cats.School;
    const std::int32_t departure = w.Command.DepartureMinute;
    const AttendanceResult attendance = RecordAttendanceArrival(s.Attendance, school, s.Now, departure, w.Cats.Calendar);
    if (!attendance.Ok)
        return DayLoopRefuse(DayLoopError::AttendanceRefused, attendance.Error);
    if (attendance.Day.Outcome != AttendanceOutcome::Absent && !school.Bells.empty())
    {
        const std::int32_t leaves = departure == AttendanceStaysUntilDismissal ? school.Bells.back() : departure;
        const GameInstant until = GameClock::StartOfDay(s.Now) + leaves;
        if (until > s.Now)
            s.SchoolUntil = until;
    }
    DayLoopEvent event = DayLoopEventAt(DayLoopEventKind::ArrivedAtSchool, s);
    event.Attendance = attendance.Day.Outcome;
    w.Result.Events.push_back(std::move(event));
    return w.Result;
}

DayLoopResult DayLoopGoToSleep(DayLoopWork& w)
{
    DayLoopState& s = w.State;
    if (s.Asleep)
        return DayLoopRefuse(DayLoopError::Asleep);
    if (s.Runner.Active() || s.HasCommitment)
        return DayLoopRefuse(DayLoopError::Busy, "finish or cancel the commitment first");
    s.Asleep = true;
    s.SleptAt = s.Now;
    w.Result.Events.push_back(DayLoopEventAt(DayLoopEventKind::SleepBegan, s));
    return w.Result;
}

DayLoopResult DayLoopWake(DayLoopWork& w)
{
    DayLoopState& s = w.State;
    if (!s.Asleep)
        return DayLoopRefuse(DayLoopError::NotAsleep);
    const std::int64_t minutes = w.Command.Minutes;
    if (minutes < 1 || minutes > DayLoopMaxSleepMinutes)
        return DayLoopRefuse(DayLoopError::InvalidCommand, "sleep lasts 1 to 1440 minutes");
    if (s.Now + minutes > DayLoopMaxInstant || s.Wakes == std::numeric_limits<std::uint32_t>::max())
        return DayLoopRefuse(DayLoopError::InvalidTime);
    if (!AdvanceCondition(s.Condition, ConditionActivity::Sleeping, minutes))
        return DayLoopRefuse(DayLoopError::ConditionRefused);

    s.Now += minutes;
    s.Asleep = false;
    s.SleptAt = 0;
    s.Planned = false;
    s.Plan.clear();
    // Earlier unrecorded school days become absences; failing to catch up never keeps a player asleep.
    if (w.Cats.School != nullptr)
        (void)CloseAttendanceBefore(s.Attendance, *w.Cats.School, s.Now, w.Cats.Calendar);
    (void)FoldReceipts(s.Receipts, s.Now);
    ++s.Wakes;

    DayLoopEvent woke = DayLoopEventAt(DayLoopEventKind::Woke, s);
    woke.Minutes = minutes;
    w.Result.Events.push_back(std::move(woke));
    DayLoopEvent due = DayLoopEventAt(DayLoopEventKind::CheckpointDue, s);
    due.CheckpointIndex = static_cast<std::int32_t>((s.Wakes - 1) % DayLoopCheckpointCount);
    w.Result.CheckpointDue = true;
    w.Result.CheckpointIndex = due.CheckpointIndex;
    w.Result.Events.push_back(std::move(due));
    return w.Result;
}

DayLoopResult DayLoopDispatch(DayLoopWork& w)
{
    switch (w.Command.Kind)
    {
    case DayCommandKind::PlanDay:
        return DayLoopPlanDay(w);
    case DayCommandKind::Commit:
        return DayLoopCommit(w);
    case DayCommandKind::Start:
        return DayLoopStart(w);
    case DayCommandKind::Complete:
        return DayLoopComplete(w);
    case DayCommandKind::Cancel:
        return DayLoopCancel(w);
    case DayCommandKind::Pass:
        return DayLoopPass(w);
    case DayCommandKind::ArriveSchool:
        return DayLoopArriveSchool(w);
    case DayCommandKind::GoToSleep:
        return DayLoopGoToSleep(w);
    case DayCommandKind::Wake:
        return DayLoopWake(w);
    }
    return DayLoopRefuse(DayLoopError::InvalidCommand, "unknown command kind");
}
} // namespace

DayLoopResult ApplyDayCommand(DayLoopState& state, const DayLoopCatalogues& catalogues, const DayCommand& command)
{
    if (!IsDayLoopStateValid(state) || state.Seq == std::numeric_limits<std::uint64_t>::max())
        return DayLoopRefuse(DayLoopError::InvalidState);

    DayLoopWork work{state, catalogues, command, {}, {}};
    DayLoopResult result = DayLoopDispatch(work);
    if (!result.Ok())
        return result;

    DayLoopState& next = work.State;
    ++next.Seq;
    next.HasLastAction = true;
    next.Last = {command.Kind, next.Now, work.LastActivity};
    next.Phase = DayLoopPhaseOf(next);
    // A bug must never put an invalid state into a life: refuse rather than commit it.
    if (!IsDayLoopStateValid(next))
        return DayLoopRefuse(DayLoopError::InvalidState, "the command would have left an invalid state");
    state = std::move(next);
    return result;
}
} // namespace PerfectWorld
