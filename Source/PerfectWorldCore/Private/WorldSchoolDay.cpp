#include "WorldReducer.h"

#include <algorithm>
#include <utility>

namespace PerfectWorld
{
namespace
{
Rejection R67Refusal(std::string code, std::string message)
{
    return {std::move(code), std::move(message)};
}

std::string R67OutcomeKey(AttendanceOutcome outcome)
{
    switch (outcome)
    {
    case AttendanceOutcome::OnTime: return "school.arrival.on_time";
    case AttendanceOutcome::Late: return "school.arrival.late";
    case AttendanceOutcome::VeryLate: return "school.arrival.very_late";
    case AttendanceOutcome::Absent: return "school.arrival.absent";
    }
    return {};
}

std::optional<Rejection> R67WorldRefusal(const WorldState& state)
{
    if (!IsWorldDayInStep(state))
        return R67Refusal("school.invalid_state", "The world and day must be valid and share one clock.");
    if (!IsWorldQuiescent(state))
        return R67Refusal("world.unsettled", "Events are still due; settle them first.");
    if (!OpenConversationPersonId(state).empty())
        return R67Refusal("school.conversation_open", "Finish the conversation first.");
    return std::nullopt;
}

std::optional<Rejection> R67DayRefusal(const DayLoopResult& result)
{
    if (result.Ok())
        return std::nullopt;
    return R67Refusal(std::string(DayLoopErrorCode(result.Error)), result.Detail);
}

DayCommand R67DayCommand(const Command& command)
{
    DayCommand day;
    if (const auto* plan = std::get_if<SchoolPlanDay>(&command.Payload))
    {
        day.Kind = DayCommandKind::PlanDay;
        day.Plan = plan->Plan;
    }
    else
        day.Kind = DayCommandKind::ArriveSchool;
    return day;
}
} // namespace

SchoolArrivalView QuerySchoolArrival(const WorldState& state, const DayLoopCatalogues& school)
{
    SchoolArrivalView view;
    const auto refuse = [&view](Rejection refusal) {
        if (view.ReasonTextKey.empty())
            view.ReasonTextKey = refusal.Code;
        view.Refusal = std::move(refusal);
        return view;
    };
    if (auto refusal = R67WorldRefusal(state))
        return refuse(*refusal);
    if (school.School == nullptr || school.Calendar == nullptr)
        return refuse(R67Refusal("school.missing_content", "A bell schedule and calendar must be bound to the school."));
    const BellSchedule& bell = *school.School;
    const SchoolCalendar& calendar = *school.Calendar;
    if (!ValidateBellSchedule(bell).empty() || !ValidateSchoolCalendar(calendar).empty() ||
        bell.InstitutionId != calendar.InstitutionId || bell.StageId != calendar.StageId)
        return refuse(R67Refusal("school.invalid_content", "The school's bell schedule and calendar must be valid and match."));

    view.HasSchool = true;
    view.GatesOpenMinute = bell.GatesOpenMinute;
    view.FirstBellMinute = BellFirstBell(bell);
    view.DismissalMinute = bell.Bells.back();
    const GameInstant now = state.Clock.Now();
    const CalendarDay day = CalendarDayOfInstant(now);
    view.DayKind = ClassifyCalendarDay(calendar, day);
    if (const CalendarHoliday* holiday = CalendarHolidayOn(calendar, day))
        view.ClosureId = holiday->Id;
    else if (const CalendarBreak* closure = CalendarBreakOn(calendar, day))
        view.ClosureId = closure->Id;
    if (view.DayKind != SchoolDayKind::SchoolDay)
    {
        view.ReasonTextKey = "school.day." + std::string(SchoolDayKindName(view.DayKind));
        return refuse(R67Refusal("school.not_school_day", "The school calendar has no classes today."));
    }

    const std::int64_t campaignDay = GameClock::DayNumber(now) - GameClock::DayNumber(0);
    const AttendanceRecord& attendance = state.DayLoop.Attendance;
    const auto recorded = std::find_if(attendance.Days.begin(), attendance.Days.end(),
                                      [campaignDay](const AttendanceDay& entry) { return entry.Day == campaignDay; });
    if (recorded != attendance.Days.end())
    {
        view.Recorded = *recorded;
        view.ReasonTextKey = R67OutcomeKey(recorded->Outcome);
    }
    if (campaignDay < attendance.NextDay)
        return refuse(R67Refusal("school.already_recorded", "Today's attendance is already recorded."));
    const auto minute = static_cast<std::int32_t>(now - GameClock::StartOfDay(now));
    view.Forecast = ForecastAttendance(bell, minute);
    if (state.DayLoop.Asleep)
        return refuse(R67Refusal("dayloop.asleep", "Wake before arriving at school."));
    if (!state.DayLoop.Planned)
        return refuse(R67Refusal("dayloop.plan_first", "Make today's plan first; an empty plan is allowed."));
    if (state.DayLoop.Runner.Active())
        return refuse(R67Refusal("dayloop.busy", "Finish or cancel the running activity first."));
    if (minute < bell.GatesOpenMinute)
        return refuse(R67Refusal("school.gate_closed", "The school gate has not opened yet."));

    // The module owns catch-up bounds, period masks, warnings and standing. A trial also detects day-sequence limits.
    DayLoopState trial = state.DayLoop;
    DayCommand arrive;
    arrive.Kind = DayCommandKind::ArriveSchool;
    const DayLoopResult result = ApplyDayCommand(trial, school, arrive);
    if (auto refusal = R67DayRefusal(result))
        return refuse(*refusal);
    view.CanArrive = true;
    view.ReasonTextKey = R67OutcomeKey(view.Forecast->Outcome);
    return view;
}

std::optional<Rejection> WorldSim::ValidateSchoolCommand(const Command& command) const
{
    if (std::holds_alternative<SchoolArrive>(command.Payload))
    {
        const SchoolArrivalView view = SchoolArrivalToday();
        if (!view.CanArrive)
            return view.Refusal;
        return std::nullopt;
    }
    if (auto refusal = R67WorldRefusal(State_))
        return refusal;
    DayLoopState trial = State_.DayLoop;
    return R67DayRefusal(ApplyDayCommand(trial, School_, R67DayCommand(command)));
}

std::optional<Rejection> WorldSim::ApplySchoolCommand(const Command& command, CommandResult& result)
{
    // No world snapshot is needed: both commands are free and only replace the day after successful application.
    DayLoopState next = State_.DayLoop;
    DayLoopResult applied = ApplyDayCommand(next, School_, R67DayCommand(command));
    if (auto refusal = R67DayRefusal(applied))
        return refusal;
    State_.DayLoop = std::move(next);
    for (DayLoopEvent& event : applied.Events)
        result.Events.push_back({0, State_.Clock.Now(), std::move(event)});
    return std::nullopt;
}
} // namespace PerfectWorld
