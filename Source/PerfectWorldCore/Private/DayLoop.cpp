#include "DayLoop.h"

#include <algorithm>
#include <array>

// The day loop's state, names, validity, forecast and slot summary (ROADMAP M2-12).
// Commands are in DayLoopApply.cpp and the text form in DayLoopText.cpp.
namespace PerfectWorld
{
namespace
{
constexpr std::array<std::string_view, 9> DayLoopCommandNames{"plan_day", "commit",       "start",
                                                              "complete", "cancel",       "pass",
                                                              "arrive_school", "go_to_sleep", "wake"};

bool DayLoopPlanItemsValid(const std::vector<DayPlanItem>& plan)
{
    if (plan.size() > DayLoopMaxPlanItems)
        return false;
    std::int32_t previous = -1;
    for (const DayPlanItem& item : plan)
    {
        if (item.StartMinute < 0 || item.StartMinute >= MinutesPerDay || item.StartMinute <= previous ||
            !IsReceiptIdWellFormed(item.ActivityId))
            return false;
        previous = item.StartMinute;
    }
    return true;
}

bool DayLoopCommitmentValid(const DayLoopState& state)
{
    const DayLoopCommitment& c = state.Commitment;
    if (!IsReceiptIdWellFormed(c.ActivityId) || !IsReceiptIdWellFormed(c.PlaceKind) || c.DecidedAt < 0 ||
        c.DecidedAt > state.Now || c.Alternatives.size() > DayLoopMaxAlternatives)
        return false;
    return std::all_of(c.Alternatives.begin(), c.Alternatives.end(),
                       [](const ReceiptCandidate& candidate) { return IsReceiptCandidateValid(candidate); });
}
} // namespace

std::string_view DayPhaseName(DayPhase phase) noexcept
{
    switch (phase)
    {
    case DayPhase::Plan:
        return "plan";
    case DayPhase::School:
        return "school";
    case DayPhase::FreeBlock:
        return "free_block";
    case DayPhase::Evening:
        return "evening";
    case DayPhase::Sleep:
        return "sleep";
    }
    return {};
}

std::string_view DayCommandName(DayCommandKind kind) noexcept
{
    const std::size_t index = static_cast<std::size_t>(kind);
    return index < DayLoopCommandNames.size() ? DayLoopCommandNames[index] : std::string_view();
}

std::optional<DayCommandKind> DayCommandFromName(std::string_view name) noexcept
{
    for (std::size_t i = 0; i < DayLoopCommandNames.size(); ++i)
    {
        if (DayLoopCommandNames[i] == name)
            return static_cast<DayCommandKind>(i);
    }
    return std::nullopt;
}

std::string_view DayLoopErrorCode(DayLoopError error) noexcept
{
    switch (error)
    {
    case DayLoopError::None:
        return {};
    case DayLoopError::InvalidState:
        return "dayloop.invalid_state";
    case DayLoopError::MissingCatalogue:
        return "dayloop.missing_catalogue";
    case DayLoopError::InvalidCommand:
        return "dayloop.invalid_command";
    case DayLoopError::UnknownActivity:
        return "dayloop.unknown_activity";
    case DayLoopError::WrongPlace:
        return "dayloop.wrong_place";
    case DayLoopError::Asleep:
        return "dayloop.asleep";
    case DayLoopError::NotAsleep:
        return "dayloop.not_asleep";
    case DayLoopError::PlanFirst:
        return "dayloop.plan_first";
    case DayLoopError::Busy:
        return "dayloop.busy";
    case DayLoopError::NotCommitted:
        return "dayloop.not_committed";
    case DayLoopError::AlreadyCommitted:
        return "dayloop.already_committed";
    case DayLoopError::NotRunning:
        return "dayloop.not_running";
    case DayLoopError::InvalidTime:
        return "dayloop.invalid_time";
    case DayLoopError::NoSchool:
        return "dayloop.no_school";
    case DayLoopError::ActivityRefused:
        return "dayloop.activity_refused";
    case DayLoopError::PracticeRefused:
        return "dayloop.practice_refused";
    case DayLoopError::ReceiptRefused:
        return "dayloop.receipt_refused";
    case DayLoopError::AttendanceRefused:
        return "dayloop.attendance_refused";
    case DayLoopError::ConditionRefused:
        return "dayloop.condition_refused";
    }
    return {};
}

DayLoopState NewDayLoopState(GameInstant start)
{
    DayLoopState state;
    state.Now = start;
    state.Attendance = StartAttendanceRecord(GameClock::DayNumber(start) - GameClock::DayNumber(0));
    state.Phase = DayLoopPhaseOf(state);
    return state;
}

DayPhase DayLoopPhaseOf(const DayLoopState& state)
{
    if (state.Asleep)
        return DayPhase::Sleep;
    if (!state.Planned)
        return DayPhase::Plan;
    if (state.Now < state.SchoolUntil)
        return DayPhase::School;
    const std::int64_t minute = state.Now - GameClock::StartOfDay(state.Now);
    if (minute >= DayLoopEveningStartMinute || minute < DayLoopMorningStartMinute)
        return DayPhase::Evening;
    return DayPhase::FreeBlock;
}

bool IsDayLoopStateValid(const DayLoopState& state)
{
    if (!IsConditionValid(state.Condition) || !IsActivityRunnerStateValid(state.Runner) ||
        !IsPracticeStateValid(state.Practice) || !IsReceiptStateValid(state.Receipts) ||
        !ValidateAttendanceRecord(state.Attendance).empty())
        return false;
    if (state.Now < 0 || state.Now > DayLoopMaxInstant || state.Phase != DayLoopPhaseOf(state) ||
        state.Practice.LastAt > state.Now || state.Receipts.LastDecidedAt > state.Now || state.Wakes > state.Seq)
        return false;

    if (state.Asleep)
    {
        if (state.SleptAt < 0 || state.SleptAt > state.Now || state.Runner.Active() || state.HasCommitment)
            return false;
    }
    else if (state.SleptAt != 0)
        return false;
    if (state.SchoolUntil < 0 || state.SchoolUntil > DayLoopMaxInstant)
        return false;

    if (!DayLoopPlanItemsValid(state.Plan) || (!state.Planned && !state.Plan.empty()))
        return false;

    if (state.HasCommitment)
    {
        if (!state.Planned || state.Asleep || !DayLoopCommitmentValid(state))
            return false;
    }
    else if (!(state.Commitment == DayLoopCommitment{}))
        return false;
    if (state.Runner.Active() &&
        (!state.HasCommitment || state.Runner.ActivityId != state.Commitment.ActivityId ||
         state.Runner.StartedAt != state.Now || state.Commitment.DecidedAt > state.Runner.StartedAt))
        return false;

    if (state.HasLastAction != (state.Seq > 0))
        return false;
    if (state.HasLastAction)
    {
        const DayLastAction& last = state.Last;
        if (static_cast<std::size_t>(last.Kind) >= DayLoopCommandNames.size() || last.At < 0 || last.At > state.Now ||
            (!last.ActivityId.empty() && !IsReceiptIdWellFormed(last.ActivityId)))
            return false;
    }
    else if (!(state.Last == DayLastAction{}))
        return false;
    return true;
}

std::string DayLoopPracticeFamily(const ActivityDef& def)
{
    return "family." + def.Outcome;
}

DayActivityForecast ForecastDayActivity(const DayLoopState& state, const ActivityCatalogue& catalogue,
                                        std::string_view activityId, GameInstant startAt)
{
    DayActivityForecast forecast;
    const ActivityDef* def = catalogue.Find(activityId);
    if (def == nullptr)
        return forecast;
    forecast.FamilyId = DayLoopPracticeFamily(*def);

    const std::vector<SkillCredit> base = ActivityLearningCredits(*def);
    if (!base.empty())
    {
        forecast.Practice = ForecastPractice(state.Practice, {forecast.FamilyId, base, startAt});
        if (!forecast.Practice.Ok())
            return forecast;
        for (const SkillCredit& credit : forecast.Practice.Credit)
            forecast.Receipt.LearningMilliXp += credit.MilliXp;
    }
    const ConditionRates rates = ConditionRatesFor(def->Condition);
    const std::int64_t perMinute = std::max(0, -rates.Energy) + std::max(0, rates.Hunger);
    forecast.Receipt.ConditionCostMilli = static_cast<std::int32_t>(
        std::min<std::int64_t>(perMinute * def->DurationMinutes, ReceiptMaxForecastMilli));
    forecast.Ok = true;
    return forecast;
}

DayLoopSlotSummary SummariseDayLoop(const DayLoopState& state)
{
    DayLoopSlotSummary summary;
    summary.Now = state.Now;
    summary.Phase = state.Phase;
    summary.Seq = state.Seq;
    summary.Wakes = state.Wakes;
    summary.HasCommitment = state.HasCommitment;
    if (state.HasCommitment)
    {
        summary.CommitmentActivityId = state.Commitment.ActivityId;
        summary.CommitmentStarted = state.Runner.Active();
    }
    summary.HasLastAction = state.HasLastAction;
    if (state.HasLastAction)
    {
        summary.LastKind = state.Last.Kind;
        summary.LastAt = state.Last.At;
        summary.LastActivityId = state.Last.ActivityId;
    }
    return summary;
}
} // namespace PerfectWorld
