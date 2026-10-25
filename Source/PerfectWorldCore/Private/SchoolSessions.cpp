#include "SchoolSessions.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace PerfectWorld
{
namespace
{
SchoolIssue P2CSessionIssue(std::string_view suffix, std::string detail = {})
{
    return {"school.session." + std::string(suffix), std::move(detail)};
}
SchoolIssue P2CRunnerIssue(ModelError error)
{
    return error == ModelError::None ? SchoolIssue{} : SchoolIssue{std::string(ModelErrorCode(error)), {}};
}
SchoolIssue P2CRunnerIssue(StructureError error)
{
    return error == StructureError::None ? SchoolIssue{} : SchoolIssue{std::string(StructureErrorCode(error)), {}};
}
bool P2CSecure(const SchoolSessionsState& state, const SchoolLessonBinding& b)
{
    switch (b.Structure)
    {
    case SchoolStructure::Model: return ModelFamilySecure(state.Model, b.FamilyId);
    case SchoolStructure::Explain: return ExplainFamilySecure(state.Explain, b.FamilyId);
    case SchoolStructure::Investigate: return InvestigateFamilySecure(state.Investigate, b.FamilyId);
    case SchoolStructure::Coordinate: return CoordinateFamilySecure(state.Coordinate, b.FamilyId);
    }
    return false;
}
SchoolIssue P2CBeginLesson(SchoolSessionsState& s, const SchoolSessionCatalogues& c,
    const SchoolLessonBinding& b, const SchoolLessonGoal& goal, std::int32_t skill, std::uint64_t seed, GameInstant at)
{
    switch (b.Structure)
    {
    case SchoolStructure::Model:
        if (const auto* g = std::get_if<ModelGoal>(&goal))
        {
            const auto pick = SelectModelCase(*c.Structures.Model, s.Model, b.FamilyId, seed, at);
            if (!pick.Ok()) return P2CRunnerIssue(pick.Error);
            return P2CRunnerIssue(BeginModelExposure(s.Model, *c.Structures.Model, pick.CaseId, *g, skill).Error);
        }
        break;
    case SchoolStructure::Explain:
        if (const auto* g = std::get_if<ExplainGoal>(&goal))
        {
            const auto pick = SelectExplainCase(*c.Structures.Explain, s.Explain, b.FamilyId, seed, at);
            if (!pick.Ok()) return P2CRunnerIssue(pick.Error);
            return P2CRunnerIssue(BeginExplainExposure(s.Explain, *c.Structures.Explain, pick.CaseId, *g, skill).Error);
        }
        break;
    case SchoolStructure::Investigate:
        if (const auto* g = std::get_if<InvestigateGoal>(&goal))
        {
            const auto pick = SelectInvestigateCase(*c.Structures.Investigate, s.Investigate, b.FamilyId, seed, at);
            if (!pick.Ok()) return P2CRunnerIssue(pick.Error);
            return P2CRunnerIssue(BeginInvestigateExposure(s.Investigate, *c.Structures.Investigate, pick.CaseId, *g, skill).Error);
        }
        break;
    case SchoolStructure::Coordinate:
        if (const auto* g = std::get_if<CoordinateGoal>(&goal))
        {
            const auto pick = SelectCoordinateCase(*c.Structures.Coordinate, s.Coordinate, b.FamilyId, seed, at);
            if (!pick.Ok()) return P2CRunnerIssue(pick.Error);
            return P2CRunnerIssue(BeginCoordinateExposure(s.Coordinate, *c.Structures.Coordinate, pick.CaseId, *g, skill).Error);
        }
        break;
    }
    return P2CSessionIssue("wrong_goal");
}
SchoolIssue P2CRoutineLesson(SchoolSessionsState& s, const SchoolSessionCatalogues& c, const SchoolLessonBinding& b,
    SchoolLessonOutcome& outcome)
{
    if (b.Structure == SchoolStructure::Model)
    {
        const auto result = ResolveModelRoutine(s.Model, *c.Structures.Model, b.FamilyId);
        if (result.Ok()) outcome = result.Outcome;
        return P2CRunnerIssue(result.Error);
    }
    StructureRoutineResult result;
    switch (b.Structure)
    {
    case SchoolStructure::Explain: result = ResolveExplainRoutine(s.Explain, *c.Structures.Explain, b.FamilyId); break;
    case SchoolStructure::Investigate: result = ResolveInvestigateRoutine(s.Investigate, *c.Structures.Investigate, b.FamilyId); break;
    case SchoolStructure::Coordinate: result = ResolveCoordinateRoutine(s.Coordinate, *c.Structures.Coordinate, b.FamilyId); break;
    default: return P2CSessionIssue("bad_binding");
    }
    if (result.Ok()) outcome = result.Outcome;
    return P2CRunnerIssue(result.Error);
}
SchoolIssue P2CCancelLesson(SchoolSessionsState& s, SchoolStructure kind)
{
    switch (kind)
    {
    case SchoolStructure::Model: return P2CRunnerIssue(CancelModelExposure(s.Model));
    case SchoolStructure::Explain: return P2CRunnerIssue(CancelExplainExposure(s.Explain));
    case SchoolStructure::Investigate: return P2CRunnerIssue(CancelInvestigateExposure(s.Investigate));
    case SchoolStructure::Coordinate: return P2CRunnerIssue(CancelCoordinateExposure(s.Coordinate));
    }
    return P2CSessionIssue("bad_binding");
}
} // namespace

SchoolIssue ValidateSchoolLessonBinding(const SchoolLessonBinding& b, const SchoolSessionCatalogues& c)
{
    if (!IsReceiptIdWellFormed(b.Id) || !IsReceiptIdWellFormed(b.FamilyId) ||
        !IsReceiptIdWellFormed(b.TitleTextKey) || !IsReceiptIdWellFormed(b.BriefTextKey) ||
        !IsReceiptIdWellFormed(b.RoleTextKey)) return P2CSessionIssue("bad_binding");
    if (!c.Day.School || !c.Day.Calendar || !c.Day.Activities) return P2CSessionIssue("missing_catalogue");
    const auto& school = *c.Day.School;
    if (!ValidateBellSchedule(school).empty() || c.Day.Calendar->InstitutionId != school.InstitutionId ||
        c.Day.Calendar->StageId != school.StageId || b.Stream != school.Stream ||
        b.Period < 0 || b.Period >= BellPeriodCount(school)) return P2CSessionIssue("bad_binding");
    const auto* activity = c.Day.Activities->Find(AttendanceClassActivityId);
    std::int32_t period = 0;
    for (const auto& block : school.Blocks)
        if (block.Kind == BellBlockKind::Period && period++ == b.Period)
            if (block.Subject != b.Subject || !activity || block.EndMinute - block.StartMinute != activity->DurationMinutes)
                return P2CSessionIssue("period_content_mismatch");
    bool family = false;
    switch (b.Structure)
    {
    case SchoolStructure::Model: family = c.Structures.Model && c.Structures.Model->FindFamily(b.FamilyId); break;
    case SchoolStructure::Explain: family = c.Structures.Explain && c.Structures.Explain->FindFamily(b.FamilyId); break;
    case SchoolStructure::Investigate: family = c.Structures.Investigate && c.Structures.Investigate->FindFamily(b.FamilyId); break;
    case SchoolStructure::Coordinate: family = c.Structures.Coordinate && c.Structures.Coordinate->FindFamily(b.FamilyId); break;
    }
    return family ? SchoolIssue{} : P2CSessionIssue("unknown_family");
}

SchoolPeriodView QuerySchoolPeriod(const SchoolSessionsState& s, const DayLoopState& day,
    const SchoolLessonBinding& b, const SchoolSessionCatalogues& c)
{
    SchoolPeriodView v;
    v.Issue = ValidateSchoolLessonBinding(b, c);
    const auto refuse = [&](std::string_view reason) { v.Issue = P2CSessionIssue(reason); v.ReasonTextKey = v.Issue.Code; return v; };
    if (!v.Issue.Ok()) { v.ReasonTextKey = v.Issue.Code; return v; }
    if (!IsDayLoopStateValid(day) || !IsStructureModelStateValid(s.Model) || !IsExplainStateValid(s.Explain) ||
        !IsInvestigateStateValid(s.Investigate) || !IsCoordinateStateValid(s.Coordinate)) return refuse("invalid_state");
    v.Day = GameClock::DayNumber(day.Now) - GameClock::DayNumber(0);
    if (!AttendanceIsSchoolDay(c.Day.Calendar, v.Day)) return refuse("closed_day");
    v.Period = b.Period;
    std::int32_t period = 0;
    for (const auto& block : c.Day.School->Blocks)
        if (block.Kind == BellBlockKind::Period && period++ == b.Period)
        { v.StartMinute = block.StartMinute; v.EndMinute = block.EndMinute; v.Room = block.Room; v.Subject = block.Subject; }
    for (const auto& r : s.Periods)
    {
        if (r.Day == v.Day && r.Mode == SchoolLessonMode::Foreground) ++v.ForegroundUsed;
        if (r.Day == v.Day && r.Period == b.Period) return refuse("period_used");
    }
    if (s.Active || day.Runner.Active() || day.HasCommitment || day.Asleep || !day.Planned) return refuse("busy");
    if (s.Periods.size() >= SchoolPeriodHistoryLimit || s.NextSession == std::numeric_limits<std::uint64_t>::max())
        return refuse("history_full");
    if (day.Now - GameClock::StartOfDay(day.Now) != v.StartMinute) return refuse("not_at_bell");
    const auto found = std::find_if(day.Attendance.Days.begin(), day.Attendance.Days.end(),
        [&](const AttendanceDay& d) { return d.Day == v.Day; });
    if (found == day.Attendance.Days.end() || !(found->PeriodMask & (std::uint32_t{1} << b.Period)) ||
        day.SchoolUntil < GameClock::StartOfDay(day.Now) + v.EndMinute) return refuse("not_attending");
    v.AttendAvailable = true;
    v.ForegroundAvailable = v.ForegroundUsed < SchoolForegroundLimit;
    v.RoutineAvailable = P2CSecure(s, b);
    return v;
}

SchoolLessonView QuerySchoolLesson(const SchoolSessionsState& s)
{
    return {s.Active, s.Model, s.Explain, s.Investigate, s.Coordinate};
}

SchoolSessionResult OpenSchoolLesson(SchoolSessionsState& state, DayLoopState& day, const SchoolSessionCatalogues& c,
    const SchoolLessonBinding& b, SchoolLessonMode mode, const SchoolLessonGoal& goal, std::int32_t skill,
    std::uint64_t seed, const std::vector<ReceiptCandidate>& alternatives)
{
    SchoolSessionResult result;
    const auto period = QuerySchoolPeriod(state, day, b, c);
    result.Issue = period.Issue;
    if (!result.Ok()) return result;
    if (mode != SchoolLessonMode::Foreground && mode != SchoolLessonMode::Routine && mode != SchoolLessonMode::Attend)
        { result.Issue = P2CSessionIssue("bad_mode"); return result; }
    if (mode == SchoolLessonMode::Foreground && !period.ForegroundAvailable)
        { result.Issue = P2CSessionIssue("foreground_limit"); return result; }
    if (mode == SchoolLessonMode::Routine && !period.RoutineAvailable)
        { result.Issue = P2CSessionIssue("not_secure"); return result; }
    auto next = state;
    auto nextDay = day;
    SchoolLessonRun run{next.NextSession++, b, mode, day.Now, mode != SchoolLessonMode::Foreground, {}};
    if (mode == SchoolLessonMode::Foreground) result.Issue = P2CBeginLesson(next, c, b, goal, skill, seed, day.Now);
    // Routine mastery is resolved at Close so cancellation grants no exposure.
    if (!result.Ok()) return result;
    DayCommand command;
    command.Kind = DayCommandKind::Commit;
    command.ActivityId = AttendanceClassActivityId;
    command.PlaceKind = "classroom";
    command.Alternatives = alternatives;
    auto applied = ApplyDayCommand(nextDay, c.Day, command);
    if (applied.Ok()) { command = {}; command.Kind = DayCommandKind::Start; applied = ApplyDayCommand(nextDay, c.Day, command); }
    if (!applied.Ok()) { result.Issue = {std::string(DayLoopErrorCode(applied.Error)), applied.Detail}; return result; }
    next.Periods.push_back({period.Day, b.Period, b.Id, mode, false, false, nextDay.Runner.Instance, {}});
    next.Active = run;
    state = std::move(next);
    day = std::move(nextDay);
    result.Session = run.Session;
    result.ReadyToClose = run.ReadyToClose;
    return result;
}

SchoolLessonQuote QuoteSchoolLessonAction(const SchoolSessionsState& s, const SchoolSessionCatalogues& c,
    std::uint64_t session, const SchoolLessonAction& action)
{
    SchoolLessonQuote result;
    if (!s.Active || s.Active->Session != session) { result.Issue = P2CSessionIssue("stale_session"); return result; }
    result.Issue = ValidateSchoolLessonBinding(s.Active->Binding, c);
    if (!result.Issue.Ok()) return result;
    if (s.Active->ReadyToClose) { result.Issue = P2CSessionIssue("ready_to_close"); return result; }
    result.Issue = P2CSessionIssue("wrong_action");
    const auto copy = [&](const auto& q) { result.Issue = P2CRunnerIssue(q.Error); result.Units = q.Units; result.UnitsAfter = q.UnitsAfter; };
    switch (s.Active->Binding.Structure)
    {
    case SchoolStructure::Model: if (const auto* a = std::get_if<ModelAction>(&action)) copy(QuoteModelAction(s.Model, *c.Structures.Model, *a)); break;
    case SchoolStructure::Explain: if (const auto* a = std::get_if<ExplainAction>(&action)) copy(QuoteExplainAction(s.Explain, *c.Structures.Explain, *a)); break;
    case SchoolStructure::Investigate: if (const auto* a = std::get_if<InvestigateAction>(&action)) copy(QuoteInvestigateAction(s.Investigate, *c.Structures.Investigate, *a)); break;
    case SchoolStructure::Coordinate: if (const auto* a = std::get_if<CoordinateAction>(&action)) copy(QuoteCoordinateAction(s.Coordinate, *c.Structures.Coordinate, *a)); break;
    }
    return result;
}

SchoolSessionResult ChooseSchoolLesson(SchoolSessionsState& state, const SchoolSessionCatalogues& c,
    std::uint64_t session, const SchoolLessonAction& action)
{
    SchoolSessionResult result;
    result.Issue = QuoteSchoolLessonAction(state, c, session, action).Issue;
    if (!result.Ok()) return result;
    auto next = state;
    const auto copy = [&](const auto& r)
    {
        result.Issue = P2CRunnerIssue(r.Error);
        result.ChargedUnits = r.Charged;
        next.Active->ReadyToClose = r.Finished;
        if (r.Finished) next.Active->Outcome = r.Outcome;
    };
    switch (next.Active->Binding.Structure)
    {
    case SchoolStructure::Model: if (const auto* a = std::get_if<ModelAction>(&action)) copy(ApplyModelAction(next.Model, *c.Structures.Model, *a)); break;
    case SchoolStructure::Explain: if (const auto* a = std::get_if<ExplainAction>(&action)) copy(ApplyExplainAction(next.Explain, *c.Structures.Explain, *a)); break;
    case SchoolStructure::Investigate: if (const auto* a = std::get_if<InvestigateAction>(&action)) copy(ApplyInvestigateAction(next.Investigate, *c.Structures.Investigate, *a)); break;
    case SchoolStructure::Coordinate: if (const auto* a = std::get_if<CoordinateAction>(&action)) copy(ApplyCoordinateAction(next.Coordinate, *c.Structures.Coordinate, *a)); break;
    }
    if (!result.Ok()) return result;
    result.Session = session;
    result.ReadyToClose = next.Active->ReadyToClose;
    state = std::move(next);
    return result;
}

SchoolSessionResult CloseSchoolLesson(SchoolSessionsState& state, DayLoopState& day, const SchoolSessionCatalogues& c,
    std::uint64_t session, bool cancel, std::int32_t elapsed)
{
    SchoolSessionResult result;
    if (!state.Active || state.Active->Session != session) { result.Issue = P2CSessionIssue("stale_session"); return result; }
    result.Issue = ValidateSchoolLessonBinding(state.Active->Binding, c);
    if (!result.Ok()) return result;
    if (state.Periods.empty() || day.Now != state.Active->OpenedAt ||
        day.Runner.Instance != state.Periods.back().ActivityInstance || day.Runner.ActivityId != AttendanceClassActivityId)
        { result.Issue = P2CSessionIssue("invalid_state"); return result; }
    if (!cancel && !state.Active->ReadyToClose) { result.Issue = P2CSessionIssue("unfinished"); return result; }
    // A completed foreground outcome already changed mastery; its only exit is paid Close.
    if (cancel && state.Active->ReadyToClose && state.Active->Mode == SchoolLessonMode::Foreground)
        { result.Issue = P2CSessionIssue("already_resolved"); return result; }
    if (!cancel && elapsed != 0) { result.Issue = P2CSessionIssue("bad_elapsed"); return result; }
    auto next = state;
    auto nextDay = day;
    if (cancel && next.Active->Mode == SchoolLessonMode::Foreground) result.Issue = P2CCancelLesson(next, next.Active->Binding.Structure);
    if (!cancel && next.Active->Mode == SchoolLessonMode::Routine)
        result.Issue = P2CRoutineLesson(next, c, next.Active->Binding, next.Active->Outcome);
    if (!result.Ok()) return result;
    DayCommand command;
    command.Kind = cancel ? DayCommandKind::Cancel : DayCommandKind::Complete;
    command.Minutes = elapsed;
    const auto applied = ApplyDayCommand(nextDay, c.Day, command);
    if (!applied.Ok()) { result.Issue = {std::string(DayLoopErrorCode(applied.Error)), applied.Detail}; return result; }
    auto& record = next.Periods.back();
    record.Completed = !cancel;
    record.Cancelled = cancel;
    record.Outcome = next.Active->Outcome;
    next.Active.reset();
    state = std::move(next);
    day = std::move(nextDay);
    result.Session = session;
    result.Events = applied.Events;
    return result;
}
} // namespace PerfectWorld
