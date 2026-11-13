#include "WorldSaveInternal.h"

#include "SchoolWeek.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

// S04: the semantic rules of the school slices (IsSchoolStateValidInWorld, WorldSave.h). The save reader applies them to a
// decoded pw.school/1 envelope and the writer refuses a world that breaks them, so neither path accepts history that the
// SchoolSessions, SchoolWork and SchoolWeek contracts could not have produced at the world's clock and day.
namespace PerfectWorld
{
namespace
{
bool S04RModeValid(SchoolLessonMode mode)
{
    return mode == SchoolLessonMode::Foreground || mode == SchoolLessonMode::Routine || mode == SchoolLessonMode::Attend;
}

bool S04RStructureValid(SchoolStructure structure)
{
    return static_cast<std::size_t>(structure) < AssessmentStructureNames.size();
}

bool S04REmpty(const SchoolLessonOutcome& outcome)
{
    return std::holds_alternative<std::monostate>(outcome);
}

/** A resolved outcome must be the structure's own type: Model's for model lessons, else the structure's or routine's. */
bool S04ROutcomeFits(const SchoolLessonOutcome& outcome, SchoolStructure structure, SchoolLessonMode mode)
{
    if (structure == SchoolStructure::Model)
        return std::holds_alternative<ModelOutcome>(outcome);
    if (mode == SchoolLessonMode::Routine)
        return std::holds_alternative<StructureOutcome>(outcome);
    switch (structure)
    {
    case SchoolStructure::Explain: return std::holds_alternative<ExplainOutcome>(outcome);
    case SchoolStructure::Investigate: return std::holds_alternative<InvestigateOutcome>(outcome);
    case SchoolStructure::Coordinate: return std::holds_alternative<CoordinateOutcome>(outcome);
    default: return false;
    }
}

std::int64_t S04RCampaignDay(GameInstant at)
{
    return GameClock::DayNumber(at) - GameClock::DayNumber(0);
}

bool S04RExposureActive(const SchoolSessionsState& s, SchoolStructure structure)
{
    switch (structure)
    {
    case SchoolStructure::Model: return s.Model.Exposure.Active();
    case SchoolStructure::Explain: return s.Explain.Exposure.Active();
    case SchoolStructure::Investigate: return s.Investigate.Exposure.Active();
    case SchoolStructure::Coordinate: return s.Coordinate.Exposure.Active();
    }
    return false;
}

bool S04RNativeValid(const SchoolSessionsState& s)
{
    return IsStructureModelStateValid(s.Model) && IsExplainStateValid(s.Explain) && IsInvestigateStateValid(s.Investigate) &&
           IsCoordinateStateValid(s.Coordinate);
}

bool S04RPeriodsValid(const SchoolSessionsState& s, const WorldState& world)
{
    if (s.Periods.size() > SchoolPeriodHistoryLimit)
        return false;
    const std::int64_t today = S04RCampaignDay(world.Clock.Now());
    std::set<std::pair<std::int64_t, std::int32_t>> used;
    std::map<std::int64_t, std::int32_t> foreground;
    std::uint64_t lastInstance = 0;
    std::int64_t lastDay = 0;
    for (std::size_t index = 0; index < s.Periods.size(); ++index)
    {
        const SchoolPeriodRecord& r = s.Periods[index];
        const bool pending = !r.Completed && !r.Cancelled;
        if (r.Day < lastDay || r.Day > today || r.Period < 0 || r.Period >= 32 || !IsReceiptIdWellFormed(r.BindingId) ||
            !S04RModeValid(r.Mode) || (r.Completed && r.Cancelled) || r.ActivityInstance <= lastInstance ||
            !used.insert({r.Day, r.Period}).second)
            return false;
        if (r.Mode == SchoolLessonMode::Foreground && ++foreground[r.Day] > SchoolForegroundLimit)
            return false;
        // Only the last record can still be open, and only while its lesson is (checked against Active by the caller).
        if (pending && index + 1 != s.Periods.size())
            return false;
        // A typed outcome only where a foreground or routine lesson was recorded; Attend, cancellations and pending have none.
        const bool resolved = r.Completed && r.Mode != SchoolLessonMode::Attend;
        if (resolved == S04REmpty(r.Outcome))
            return false;
        lastInstance = r.ActivityInstance;
        lastDay = r.Day;
    }
    return true;
}

bool S04RActiveValid(const SchoolSessionsState& s, const WorldState& world)
{
    const bool pending = !s.Periods.empty() && !s.Periods.back().Completed && !s.Periods.back().Cancelled;
    std::int32_t exposures = 0;
    for (const SchoolStructure structure : {SchoolStructure::Model, SchoolStructure::Explain, SchoolStructure::Investigate,
                                            SchoolStructure::Coordinate})
        exposures += S04RExposureActive(s, structure) ? 1 : 0;
    if (!s.Active)
        return !pending && exposures == 0;
    const SchoolLessonRun& run = *s.Active;
    const SchoolLessonBinding& b = run.Binding;
    if (!pending || run.Session == 0 || run.Session >= s.NextSession || !S04RModeValid(run.Mode) || !S04RStructureValid(b.Structure) ||
        !IsReceiptIdWellFormed(b.Id) || b.Period < 0 || b.Period >= 32 || run.OpenedAt != world.Clock.Now())
        return false;
    // Foreground is ready exactly when its runner resolved (typed outcome, exposure ended); Routine and Attend are always
    // ready and resolve nothing until Close.
    const bool foreground = run.Mode == SchoolLessonMode::Foreground;
    if (foreground ? run.ReadyToClose == S04REmpty(run.Outcome) : (!run.ReadyToClose || !S04REmpty(run.Outcome)))
        return false;
    if (!S04REmpty(run.Outcome) && !S04ROutcomeFits(run.Outcome, b.Structure, run.Mode))
        return false;
    const bool exposing = foreground && !run.ReadyToClose;
    if (exposures != (exposing ? 1 : 0) || (exposing && !S04RExposureActive(s, b.Structure)))
        return false;
    const SchoolPeriodRecord& record = s.Periods.back();
    const ActivityRunnerState& runner = world.DayLoop.Runner;
    return record.BindingId == b.Id && record.Mode == run.Mode && record.Period == b.Period &&
           record.Day == S04RCampaignDay(run.OpenedAt) && runner.Active() && runner.Instance == record.ActivityInstance &&
           runner.ActivityId == AttendanceClassActivityId;
}

bool S04RRecordOutcomesFit(const SchoolSessionsState& s)
{
    // A record keeps no structure, but a typed outcome must at least be one of the five native kinds (the decoder's tags).
    return std::all_of(s.Periods.begin(), s.Periods.end(), [](const SchoolPeriodRecord& r) {
        return r.Mode != SchoolLessonMode::Routine || !r.Completed || std::holds_alternative<ModelOutcome>(r.Outcome) ||
               std::holds_alternative<StructureOutcome>(r.Outcome);
    });
}

bool S04RWorkValid(const SchoolWorkState& w, const WorldState& world)
{
    if (w.Records.size() > SchoolWorkLimit)
        return false;
    const GameInstant now = world.Clock.Now();
    std::int32_t open = 0;
    for (std::size_t index = 0; index < w.Records.size(); ++index)
    {
        const SchoolWorkRecord& r = w.Records[index];
        const SchoolWorkBrief& b = r.Brief;
        if ((index > 0 && !(w.Records[index - 1].Brief.Id < b.Id)) || !IsReceiptIdWellFormed(b.Id) || b.PaperId.empty() ||
            !IsReceiptIdWellFormed(b.TitleTextKey) || !IsReceiptIdWellFormed(b.BriefTextKey) ||
            !IsReceiptIdWellFormed(b.CriteriaTextKey) || !IsReceiptIdWellFormed(b.RecoveryTextKey) ||
            (b.Kind != SchoolWorkKind::Homework && b.Kind != SchoolWorkKind::AcademicAssessment) || r.IssuedAt < 0 ||
            r.IssuedAt > b.AvailableAt || b.AvailableAt > b.DueAt || b.DueAt > DayLoopMaxInstant || r.IssuedAt > now ||
            !IsAssessmentStateValid(r.Paper))
            return false;
        if (!r.StartedAt)
        {
            // Idle (never begun, or a draft set aside): nothing sent, no paper in hand.
            if (r.SubmittedAt || !r.Paper.PaperId.empty())
                return false;
            continue;
        }
        if (*r.StartedAt < r.IssuedAt || *r.StartedAt < b.AvailableAt || r.Paper.PaperId != b.PaperId)
            return false;
        if (r.SubmittedAt)
        {
            if (*r.SubmittedAt != *r.StartedAt + AssessmentPaperMinutes || *r.SubmittedAt > now || !r.Paper.Finished)
                return false;
        }
        else if (++open > 1 || *r.StartedAt != now || r.Paper.Finished)
            return false;
    }
    return true;
}

bool S04RWeekValid(const SchoolWeekState& k)
{
    SchoolWeekState rebuilt;
    for (const SchoolStandingOrder& order : k.Orders)
    {
        if (!SetSchoolStandingOrder(rebuilt, order).Ok())
            return false;
    }
    if (rebuilt.Orders.size() != k.Orders.size())
        return false;
    for (std::size_t index = 0; index < k.Orders.size(); ++index)
    {
        const SchoolStandingOrder& a = rebuilt.Orders[index];
        const SchoolStandingOrder& b = k.Orders[index];
        if (a.Id != b.Id || a.Weekdays != b.Weekdays || a.AttendSchool != b.AttendSchool || a.LessonMode != b.LessonMode)
            return false;
    }
    return k.SceneDays.size() <= 400 && std::is_sorted(k.SceneDays.begin(), k.SceneDays.end()) &&
           std::adjacent_find(k.SceneDays.begin(), k.SceneDays.end()) == k.SceneDays.end();
}
} // namespace

bool WorldSaveSchoolValid(const SchoolSessionsState& sessions, const SchoolWorkState& work, const SchoolWeekState& week,
                          const WorldState& world)
{
    return sessions.NextSession >= 1 && S04RNativeValid(sessions) && S04RPeriodsValid(sessions, world) &&
           S04RRecordOutcomesFit(sessions) && S04RActiveValid(sessions, world) && S04RWorkValid(work, world) &&
           S04RWeekValid(week);
}
} // namespace PerfectWorld
