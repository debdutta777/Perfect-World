#include "SchoolLessonContent.h"

#include <algorithm>

namespace PerfectWorld
{
namespace
{
constexpr std::int64_t A1AbsenceWindowDays = 14;
const char* A1Fallback = "invalid_state";
void A1Reason(const SchoolLessonContent& content, std::string_view code, std::string& reason, std::string& recovery)
{
    const auto* e = FindSchoolLessonExplanation(content, code);
    if (e == nullptr) e = FindSchoolLessonExplanation(content, A1Fallback);
    if (e == nullptr) return;
    reason = e->TextKey;
    recovery = e->RecoveryTextKey;
}
} // namespace

const SchoolDayRow* SchoolDayView::FindRow(std::string_view bindingId) const
{
    for (const auto& row : Rows) if (row.BindingId == bindingId) return &row;
    return nullptr;
}

SchoolDayView BuildSchoolDayView(const SchoolLessonContent& content, const SchoolSessionsState& sessions,
    const DayLoopState& day, const SchoolSessionCatalogues& cats)
{
    SchoolDayView view;
    if (!cats.Day.School || !cats.Day.Calendar || !IsDayLoopStateValid(day)) { view.Issue = "school.day.unavailable"; return view; }
    const auto* stream = FindSchoolLessonStream(content, cats.Day.School->Stream);
    if (stream == nullptr) { view.Issue = "school.day.no_timetable"; return view; }
    const auto& school = *cats.Day.School;
    const auto& calendar = *cats.Day.Calendar;
    view.Stream = stream->Stream;
    view.Day = GameClock::DayNumber(day.Now) - GameClock::DayNumber(0);
    view.MinuteOfDay = static_cast<std::int32_t>(day.Now - GameClock::StartOfDay(day.Now));
    view.Kind = ClassifyCalendarDay(calendar, view.Day);
    view.SchoolDay = view.Kind == SchoolDayKind::SchoolDay;
    view.LessonOpen = sessions.Active.has_value();
    if (sessions.Active) view.ActiveBindingId = sessions.Active->Binding.Id;
    for (const auto& r : sessions.Periods)
        if (r.Day == view.Day && r.Mode == SchoolLessonMode::Foreground) ++view.ForegroundUsed;
    for (const auto& d : day.Attendance.Days)
        if (d.Day < view.Day && d.Day >= view.Day - A1AbsenceWindowDays && d.Outcome == AttendanceOutcome::Absent) ++view.RecentAbsentDays;
    if (view.RecentAbsentDays > 0)
        A1Reason(content, "after_absence", view.AbsenceTextKey, view.AbsenceRecoveryTextKey);
    if (!view.SchoolDay)
    {
        std::string span;
        if (view.Kind == SchoolDayKind::Holiday) { if (const auto* h = CalendarHolidayOn(calendar, view.Day)) span = h->Id; }
        if (view.Kind == SchoolDayKind::Break) { if (const auto* b = CalendarBreakOn(calendar, view.Day)) span = b->Id; }
        if (const auto* closure = FindSchoolLessonClosure(content, view.Kind, span)) view.ClosureTextKey = closure->TextKey;
        if (const auto next = CalendarNextSchoolDay(calendar, view.Day)) view.NextSchoolDay = *next;
        return view;
    }
    const auto attended = [&](std::int32_t period) {
        for (const auto& d : day.Attendance.Days)
            if (d.Day == view.Day) return (d.PeriodMask & (std::uint32_t{1} << period)) != 0;
        return false;
    };
    for (const auto& lesson : stream->Lessons)
    {
        const auto& b = lesson.Binding;
        SchoolDayRow row;
        row.BindingId = b.Id;
        row.Period = b.Period;
        row.Subject = b.Subject;
        row.Structure = b.Structure;
        row.TitleTextKey = b.TitleTextKey;
        row.BriefTextKey = b.BriefTextKey;
        row.RoleTextKey = b.RoleTextKey;
        std::int32_t index = 0;
        for (const auto& block : school.Blocks)
            if (block.Kind == BellBlockKind::Period && index++ == b.Period)
            { row.Room = block.Room; row.StartMinute = block.StartMinute; row.EndMinute = block.EndMinute; }
        const auto record = std::find_if(sessions.Periods.begin(), sessions.Periods.end(),
            [&](const SchoolPeriodRecord& r) { return r.Day == view.Day && r.Period == b.Period; });
        if (record != sessions.Periods.end())
        {
            row.DoneMode = record->Mode;
            if (record->Cancelled) { row.Status = SchoolDayRowStatus::Cancelled; A1Reason(content, "period_used", row.ReasonTextKey, row.RecoveryTextKey); }
            else row.Status = record->Completed ? SchoolDayRowStatus::Done : SchoolDayRowStatus::Active;
        }
        else if (view.MinuteOfDay < row.StartMinute)
        {
            row.Status = SchoolDayRowStatus::Upcoming;
            A1Reason(content, "not_at_bell", row.ReasonTextKey, row.RecoveryTextKey);
        }
        else if (view.MinuteOfDay > row.StartMinute)
        {
            row.Status = SchoolDayRowStatus::Missed;
            A1Reason(content, attended(b.Period) ? "missed_attended" : "missed_absent", row.ReasonTextKey, row.RecoveryTextKey);
        }
        else
        {
            const auto period = QuerySchoolPeriod(sessions, day, b, cats);
            if (period.Issue.Ok())
            {
                row.Status = SchoolDayRowStatus::Open;
                row.ForegroundAvailable = period.ForegroundAvailable;
                row.RoutineAvailable = period.RoutineAvailable;
                row.AttendAvailable = period.AttendAvailable;
                std::string unused;
                if (!row.ForegroundAvailable) A1Reason(content, "foreground_limit", row.ForegroundReasonTextKey, unused);
                if (!row.RoutineAvailable) A1Reason(content, "not_secure", row.RoutineReasonTextKey, unused);
            }
            else
            {
                row.Status = SchoolDayRowStatus::Unavailable;
                const std::string code = period.Issue.Code.starts_with("school.session.") ? period.Issue.Code.substr(15) : period.Issue.Code;
                A1Reason(content, code, row.ReasonTextKey, row.RecoveryTextKey);
            }
        }
        view.Rows.push_back(std::move(row));
    }
    return view;
}
} // namespace PerfectWorld
