#include "SchoolWeek.h"

#include "WorldReducer.h"

#include <algorithm>
#include <utility>

namespace PerfectWorld
{
namespace
{
SchoolIssue P2CWeekIssue(std::string_view reason, std::string detail = {})
{
    return {"school.week." + std::string(reason), std::move(detail)};
}
bool P2CWeekSecure(const SchoolSessionsState& s, const SchoolLessonBinding& b)
{
    switch (b.Structure)
    {
    case SchoolStructure::Model: return ModelFamilySecure(s.Model, b.FamilyId);
    case SchoolStructure::Explain: return ExplainFamilySecure(s.Explain, b.FamilyId);
    case SchoolStructure::Investigate: return InvestigateFamilySecure(s.Investigate, b.FamilyId);
    case SchoolStructure::Coordinate: return CoordinateFamilySecure(s.Coordinate, b.FamilyId);
    }
    return false;
}
WeekPlan P2COneEntry(const WorldState& world, GameInstant at, const WeekEntry& entry, std::int32_t interrupts)
{
    WeekPlan plan;
    plan.Seed = world.WorldSeed;
    plan.InterruptsUsed = interrupts;
    const auto offset = GameClock::DayNumber(at) - GameClock::DayNumber(world.Clock.Now());
    if (offset < 0 || offset >= WeekMaxDays) return plan;
    plan.Days.resize(static_cast<std::size_t>(offset) + 1);
    plan.Days.back().Entries.push_back(entry);
    return plan;
}
} // namespace

SchoolIssue SetSchoolStandingOrder(SchoolWeekState& state, const SchoolStandingOrder& order, bool remove)
{
    if (!IsReceiptIdWellFormed(order.Id)) return P2CWeekIssue("bad_order");
    auto next = state;
    const auto found = std::find_if(next.Orders.begin(), next.Orders.end(), [&](const auto& item) { return item.Id == order.Id; });
    if (remove)
    {
        if (found == next.Orders.end()) return P2CWeekIssue("unknown_order");
        next.Orders.erase(found);
    }
    else
    {
        if (order.Weekdays == 0 || (order.Weekdays & 0x81) != 0 ||
            (order.LessonMode != SchoolLessonMode::Attend && order.LessonMode != SchoolLessonMode::Routine &&
                order.LessonMode != SchoolLessonMode::Foreground)) return P2CWeekIssue("bad_order");
        if (found == next.Orders.end()) next.Orders.push_back(order); else *found = order;
    }
    if (next.Orders.size() > 7) return P2CWeekIssue("too_many_orders");
    std::uint8_t days = 0;
    for (const auto& item : next.Orders)
    {
        if (days & item.Weekdays) return P2CWeekIssue("overlapping_orders");
        days = static_cast<std::uint8_t>(days | item.Weekdays);
    }
    std::sort(next.Orders.begin(), next.Orders.end(), [](const auto& a, const auto& b) { return a.Id < b.Id; });
    state = std::move(next);
    return {};
}

SchoolWeekBuild BuildSchoolWeekPlan(const WorldState& world, const SchoolWeekState& standing,
    const SchoolSessionsState& sessions, const WeekPlan& base, const std::vector<SchoolLessonBinding>& bindings,
    const SchoolSessionCatalogues& c)
{
    SchoolWeekBuild result;
    if (!c.Day.School || !c.Day.Calendar || !c.Day.Activities) { result.Issue = P2CWeekIssue("missing_catalogue"); return result; }
    if (base.Days.empty() || base.Days.size() > WeekMaxDays || standing.SceneDays.size() > 400 ||
        !std::is_sorted(standing.SceneDays.begin(), standing.SceneDays.end()) ||
        std::adjacent_find(standing.SceneDays.begin(), standing.SceneDays.end()) != standing.SceneDays.end())
        { result.Issue = P2CWeekIssue("bad_plan"); return result; }
    SchoolWeekState validStanding;
    for (const auto& order : standing.Orders)
    {
        result.Issue = SetSchoolStandingOrder(validStanding, order);
        if (!result.Ok()) return result;
    }
    result.Plan.Week = base;
    const auto first = GameClock::DayNumber(world.Clock.Now()) - GameClock::DayNumber(0);
    for (std::size_t d = 0; d < base.Days.size(); ++d)
    {
        const auto absoluteDay = first + static_cast<std::int64_t>(d);
        auto& entries = result.Plan.Week.Days[d].Entries;
        const auto weekday = static_cast<std::uint8_t>(1u << static_cast<unsigned>(CalendarWeekdayOf(absoluteDay)));
        for (const auto& order : validStanding.Orders)
        {
            if (!order.AttendSchool || !(weekday & order.Weekdays) || !AttendanceIsSchoolDay(c.Day.Calendar, absoluteDay)) continue;
            if (std::none_of(entries.begin(), entries.end(), [](const auto& e) { return e.Kind == WeekEntryKind::School; }) &&
                (d != 0 || world.DayLoop.SchoolUntil <= world.Clock.Now()))
            {
                WeekEntry arrival;
                arrival.Kind = WeekEntryKind::School;
                arrival.StartMinute = BellFirstBell(*c.Day.School);
                arrival.DecidedByOrder = true;
                arrival.OrderId = order.Id;
                entries.push_back(arrival);
            }
            std::int32_t period = 0;
            for (const auto& block : c.Day.School->Blocks)
            {
                if (block.Kind != BellBlockKind::Period) continue;
                const auto index = period++;
                if (d == 0 && GameClock::StartOfDay(world.Clock.Now()) + block.StartMinute < world.Clock.Now()) continue;
                const auto b = std::find_if(bindings.begin(), bindings.end(), [&](const auto& item)
                    { return item.Stream == c.Day.School->Stream && item.Period == index; });
                if (b == bindings.end()) { result.Issue = P2CWeekIssue("missing_binding"); return result; }
                if (std::count_if(bindings.begin(), bindings.end(), [&](const auto& item)
                    { return item.Stream == b->Stream && item.Period == index; }) != 1)
                    { result.Issue = P2CWeekIssue("duplicate_binding"); return result; }
                result.Issue = ValidateSchoolLessonBinding(*b, c);
                if (!result.Ok()) return result;
                auto entry = std::find_if(entries.begin(), entries.end(), [&](const auto& e)
                    { return e.Kind == WeekEntryKind::Activity && e.ActivityId == AttendanceClassActivityId && e.StartMinute == block.StartMinute; });
                if (entry == entries.end())
                {
                    WeekEntry lesson;
                    lesson.Kind = WeekEntryKind::Activity;
                    lesson.StartMinute = block.StartMinute;
                    lesson.ActivityId = AttendanceClassActivityId;
                    lesson.PlaceKind = "classroom";
                    entries.push_back(lesson);
                    entry = entries.end() - 1;
                }
                if (!entry->NeedsChoice) { entry->DecidedByOrder = true; entry->OrderId = order.Id; }
                const bool firstLesson = std::none_of(sessions.Periods.begin(), sessions.Periods.end(),
                    [&](const auto& r) { return r.BindingId == b->Id && r.Completed; });
                if (firstLesson || order.LessonMode == SchoolLessonMode::Foreground ||
                    (order.LessonMode == SchoolLessonMode::Routine && !P2CWeekSecure(sessions, *b))) entry->FirstOfKind = true;
                result.Plan.Lessons.push_back({static_cast<std::int32_t>(d), 0, *b, order.LessonMode});
            }
        }
        std::stable_sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.StartMinute < b.StartMinute; });
        if (!entries.empty() && std::binary_search(standing.SceneDays.begin(), standing.SceneDays.end(), absoluteDay))
            entries.front().FirstOfKind = true;
        for (auto& lesson : result.Plan.Lessons)
        {
            if (lesson.Day != static_cast<std::int32_t>(d)) continue;
            std::int32_t count = 0;
            for (const auto& block : c.Day.School->Blocks)
                if (block.Kind == BellBlockKind::Period && count++ == lesson.Binding.Period)
                    for (std::size_t e = 0; e < entries.size(); ++e)
                        if (entries[e].Kind == WeekEntryKind::Activity && entries[e].ActivityId == AttendanceClassActivityId &&
                            entries[e].StartMinute == block.StartMinute) lesson.Entry = static_cast<std::int32_t>(e);
        }
    }
    const auto valid = ValidateWeekPlan(world, result.Plan.Week, c.Day);
    if (!valid.Ok()) result.Issue = {std::string(WeekErrorCode(valid.Error)), valid.Detail};
    return result;
}

SchoolWeekResult CompressSchoolWeek(const WorldState& world, const SchoolSessionsState& sessions,
    const SchoolWeekPlan& plan, const SchoolSessionCatalogues& c)
{
    SchoolWeekResult result;
    result.World = world;
    result.Sessions = sessions;
    result.InterruptsUsed = plan.Week.InterruptsUsed;
    const auto fail = [&](SchoolIssue issue)
    {
        SchoolWeekResult refused;
        refused.World = world;
        refused.Sessions = sessions;
        refused.Issue = std::move(issue);
        return refused;
    };
    const auto valid = ValidateWeekPlan(world, plan.Week, c.Day);
    if (!valid.Ok()) return fail({std::string(WeekErrorCode(valid.Error)), valid.Detail});
    if (sessions.Active) return fail(P2CWeekIssue("busy"));
    for (const auto& lesson : plan.Lessons)
    {
        if (lesson.Day < 0 || lesson.Day >= static_cast<std::int32_t>(plan.Week.Days.size()) || lesson.Entry < 0 ||
            lesson.Entry >= static_cast<std::int32_t>(plan.Week.Days[lesson.Day].Entries.size())) return fail(P2CWeekIssue("bad_lesson"));
        const auto& e = plan.Week.Days[lesson.Day].Entries[lesson.Entry];
        if (e.Kind != WeekEntryKind::Activity || e.ActivityId != AttendanceClassActivityId || e.PlaceKind != "classroom")
            return fail(P2CWeekIssue("bad_lesson"));
        const auto issue = ValidateSchoolLessonBinding(lesson.Binding, c);
        if (!issue.Ok()) return fail(issue);
    }
    const auto origin = GameClock::StartOfDay(world.Clock.Now());
    for (std::size_t d = 0; d < plan.Week.Days.size(); ++d)
    for (std::size_t e = 0; e < plan.Week.Days[d].Entries.size(); ++e)
    {
        const auto& entry = plan.Week.Days[d].Entries[e];
        const auto lesson = std::find_if(plan.Lessons.begin(), plan.Lessons.end(), [&](const auto& item)
            { return item.Day == static_cast<std::int32_t>(d) && item.Entry == static_cast<std::int32_t>(e); });
        if (std::count_if(plan.Lessons.begin(), plan.Lessons.end(), [&](const auto& item)
            { return item.Day == static_cast<std::int32_t>(d) && item.Entry == static_cast<std::int32_t>(e); }) > 1)
            return fail(P2CWeekIssue("duplicate_lesson"));
        if (entry.ActivityId == AttendanceClassActivityId && lesson == plan.Lessons.end()) return fail(P2CWeekIssue("missing_binding"));
        const bool needsPlay = entry.FirstOfKind || (lesson != plan.Lessons.end() &&
            (std::none_of(result.Sessions.Periods.begin(), result.Sessions.Periods.end(),
                [&](const auto& r) { return r.BindingId == lesson->Binding.Id && r.Completed; }) ||
             lesson->Mode == SchoolLessonMode::Foreground || (lesson->Mode == SchoolLessonMode::Routine && !P2CWeekSecure(result.Sessions, lesson->Binding))));
        if (needsPlay || entry.NeedsChoice)
        {
            result.Stop = needsPlay ? WeekStop::FirstNeedsPlay : result.InterruptsUsed ? WeekStop::InterruptBudgetSpent : WeekStop::ChoiceNeeded;
            if (result.Stop == WeekStop::ChoiceNeeded) ++result.InterruptsUsed;
            result.StopDay = static_cast<std::int32_t>(d);
            result.StopEntry = static_cast<std::int32_t>(e);
            return result;
        }
        const auto target = origin + static_cast<GameInstant>(d) * MinutesPerDay + entry.StartMinute;
        if (lesson == plan.Lessons.end() && entry.Kind != WeekEntryKind::Activity)
        {
            auto segment = CompressWeek(result.World, P2COneEntry(result.World, target, entry, result.InterruptsUsed), c.Day);
            if (!segment.Ok()) return fail({std::string(WeekErrorCode(segment.Issue.Error)), segment.Issue.Detail});
            result.World = segment.World;
            result.Segments.push_back(std::move(segment));
            continue;
        }
        while (result.World.Clock.Now() < target)
        {
            WeekEntry wait;
            wait.Kind = WeekEntryKind::Spend;
            wait.StartMinute = static_cast<std::int32_t>(result.World.Clock.Now() - GameClock::StartOfDay(result.World.Clock.Now()));
            wait.Minutes = static_cast<std::int32_t>(std::min<GameInstant>(target - result.World.Clock.Now(), DayLoopMaxPassMinutes));
            auto segment = CompressWeek(result.World, P2COneEntry(result.World, result.World.Clock.Now(), wait, result.InterruptsUsed), c.Day);
            if (!segment.Ok()) return fail({std::string(WeekErrorCode(segment.Issue.Error)), segment.Issue.Detail});
            result.World = segment.World;
            result.Segments.push_back(std::move(segment));
        }
        const auto start = result.World.Clock.Now();
        if (!result.World.DayLoop.Planned)
        {
            DayCommand morning; morning.Kind = DayCommandKind::PlanDay;
            morning.Plan.push_back({entry.StartMinute, entry.ActivityId});
            const auto planned = ApplyDayCommand(result.World.DayLoop, c.Day, morning);
            if (!planned.Ok()) return fail({std::string(DayLoopErrorCode(planned.Error)), planned.Detail});
        }
        SchoolSessionResult closed;
        if (lesson != plan.Lessons.end())
        {
            const auto opened = OpenSchoolLesson(result.Sessions, result.World.DayLoop, c, lesson->Binding, lesson->Mode,
                ModelGoal::UnderstandModel, 0, world.WorldSeed, entry.Alternatives);
            if (!opened.Ok()) return fail(opened.Issue);
            closed = CloseSchoolLesson(result.Sessions, result.World.DayLoop, c, opened.Session);
            if (!closed.Ok()) return fail(closed.Issue);
        }
        else
        {
            DayCommand command;
            command.Kind = DayCommandKind::Commit;
            command.ActivityId = entry.ActivityId;
            command.PlaceKind = entry.PlaceKind;
            command.Alternatives = entry.Alternatives;
            auto applied = ApplyDayCommand(result.World.DayLoop, c.Day, command);
            if (applied.Ok()) { command = {}; command.Kind = DayCommandKind::Start; applied = ApplyDayCommand(result.World.DayLoop, c.Day, command); }
            if (applied.Ok()) { command.Kind = DayCommandKind::Complete; applied = ApplyDayCommand(result.World.DayLoop, c.Day, command); }
            if (!applied.Ok()) return fail({std::string(DayLoopErrorCode(applied.Error)), applied.Detail});
            closed.Events = std::move(applied.Events);
        }
        const auto end = result.World.DayLoop.Now;
        // Same sync boundary used by WeekCompressionRun: preserve the already-resolved
        // DayLoop while the existing reducer fires scheduled world consequences.
        WorldSim sim(result.World);
        while (sim.State().Clock.Now() < end)
        {
            const auto moved = sim.Submit({CommandSource::System, AdvanceTime{end - sim.State().Clock.Now()}});
            if (!moved.Accepted) return fail(P2CWeekIssue("clock_refused", moved.Why.Code));
        }
        result.World = sim.State();
        if (!IsWorldDayInStep(result.World)) return fail(P2CWeekIssue("clock_refused"));
        WeekResult actual;
        actual.World = result.World;
        actual.EntriesResolved = 1;
        actual.Ledger.StartInstant = start;
        actual.Ledger.EndInstant = end;
        actual.Ledger.TotalCharged = end - start;
        const auto* activity = c.Day.Activities->Find(entry.ActivityId);
        const auto mode = activity && activity->Clock == ActivityClockMode::FreeRoam ? ClockMode::FreeRoam : ClockMode::Scene;
        actual.Ledger.ChargedByMode[static_cast<std::size_t>(mode)] = end - start;
        actual.Ledger.Completed = 1;
        if (entry.DecidedByOrder)
        {
            actual.Ledger.DecidedByOrder = 1;
            actual.Disclosures.push_back({static_cast<std::int32_t>(d), static_cast<std::int32_t>(e), entry.Kind,
                entry.ActivityId, entry.OrderId, start});
        }
        for (const auto& event : closed.Events)
            if (event.Kind == DayLoopEventKind::Completed)
            { actual.Ledger.Credits = event.Credited; actual.Ledger.Evidence = event.Evidence; }
        actual.Outcomes.push_back({static_cast<std::int32_t>(d), static_cast<std::int32_t>(e), entry.Kind, entry.ActivityId, start, end, 0});
        result.Segments.push_back(std::move(actual));
        for (const auto& event : closed.Events) result.ActivityEvents.push_back(event);
        if (lesson != plan.Lessons.end()) result.Lessons.push_back(std::move(closed));
    }
    return result;
}
} // namespace PerfectWorld
