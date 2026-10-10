#include "Schedules.h"

#include "IdentityRegistry.h"
#include "ScheduleInternal.h"

#include <algorithm>
#include <set>

// Validation, the travel table and the routine checks (ROADMAP M2-06). The rules
// are documented in Schedules.h.
namespace PerfectWorld
{
namespace
{
std::string ScheduleProblem(std::string_view what, std::size_t index, std::string_view why)
{
    std::string out(what);
    out += ' ';
    out += std::to_string(index);
    out += ' ';
    out += why;
    return out;
}

bool ScheduleDoingKindValid(ScheduleDoingKind kind)
{
    return kind == ScheduleDoingKind::Activity || kind == ScheduleDoingKind::Role;
}

std::string ScheduleCheckEntry(const ScheduleEntry& entry, std::size_t index)
{
    if (entry.WeekdayMask == 0 || (entry.WeekdayMask & ~ScheduleAllWeekdays) != 0)
        return ScheduleProblem("routine entry", index, "has an empty or out-of-range weekday mask");
    if (entry.StartMinute < 0 || entry.EndMinute > MinutesPerDay || entry.StartMinute >= entry.EndMinute)
        return ScheduleProblem("routine entry", index, "needs 0 <= start < end <= 1440");
    if (!IsStableIdToken(entry.Zone) || !IsStableIdToken(entry.Doing))
        return ScheduleProblem("routine entry", index, "needs a lowercase-token zone and activity or role");
    if (!ScheduleDoingKindValid(entry.Kind))
        return ScheduleProblem("routine entry", index, "has an unknown kind");
    if (entry.Priority < 0 || entry.Priority > ScheduleMaxPriority)
        return ScheduleProblem("routine entry", index, "has a priority outside 0..1000");
    return {};
}

std::string ScheduleCheckException(const ScheduleException& exception, std::size_t index)
{
    if (!IsStableIdToken(exception.Id) || exception.Id.compare(0, ScheduleRoutineSourcePrefix.size(), ScheduleRoutineSourcePrefix) == 0)
        return ScheduleProblem("exception", index, "needs a lowercase-token ID that does not start with \"routine:\"");
    if (exception.From < 0 || exception.From >= exception.To || exception.To > ScheduleMaxInstant)
        return ScheduleProblem("exception", index, "needs 0 <= from < to <= ScheduleMaxInstant");
    if (!IsStableIdToken(exception.Zone) || !IsStableIdToken(exception.Doing))
        return ScheduleProblem("exception", index, "needs a lowercase-token zone and activity or role");
    if (!ScheduleDoingKindValid(exception.Kind))
        return ScheduleProblem("exception", index, "has an unknown kind");
    if (exception.Priority < 0 || exception.Priority > ScheduleMaxPriority)
        return ScheduleProblem("exception", index, "has a priority outside 0..1000");
    return {};
}

bool ScheduleLegLess(const ScheduleTravelLeg& leg, const std::pair<std::string_view, std::string_view>& key)
{
    const int from = std::string_view(leg.From).compare(key.first);
    return from != 0 ? from < 0 : std::string_view(leg.To) < key.second;
}

const ScheduleTravelLeg* ScheduleFindLeg(const ScheduleTravelTable& table, std::string_view from, std::string_view to)
{
    const std::pair<std::string_view, std::string_view> key{from, to};
    const auto it = std::lower_bound(table.Legs.begin(), table.Legs.end(), key, ScheduleLegLess);
    return it != table.Legs.end() && it->From == from && it->To == to ? &*it : nullptr;
}
} // namespace

std::string ValidatePersonSchedule(const PersonSchedule& person)
{
    if (!IsValidIdentityId(person.PersonId))
        return "the person ID is not a valid identity ID";
    if (!IsStableIdToken(person.HomeZone))
        return "the home zone is not a lowercase token";
    if (person.Routine.size() > ScheduleMaxRoutineEntries)
        return "the routine has too many entries";
    if (person.Exceptions.size() > ScheduleMaxExceptions)
        return "there are too many exceptions";

    for (std::size_t i = 0; i < person.Routine.size(); ++i)
    {
        std::string problem = ScheduleCheckEntry(person.Routine[i], i);
        if (!problem.empty())
            return problem;
    }
    // Overlap on a shared weekday is resolved by priority, but equal priorities are ambiguous.
    for (std::size_t i = 0; i < person.Routine.size(); ++i)
    {
        for (std::size_t j = i + 1; j < person.Routine.size(); ++j)
        {
            const ScheduleEntry& a = person.Routine[i];
            const ScheduleEntry& b = person.Routine[j];
            if ((a.WeekdayMask & b.WeekdayMask) != 0 && a.StartMinute < b.EndMinute && b.StartMinute < a.EndMinute &&
                a.Priority == b.Priority)
                return ScheduleProblem("routine entries", i, "and " + std::to_string(j) + " overlap with equal priority");
        }
    }

    std::set<std::string_view> ids;
    for (std::size_t i = 0; i < person.Exceptions.size(); ++i)
    {
        std::string problem = ScheduleCheckException(person.Exceptions[i], i);
        if (!problem.empty())
            return problem;
        if (!ids.insert(person.Exceptions[i].Id).second)
            return ScheduleProblem("exception", i, "repeats an earlier exception ID");
    }
    return {};
}

std::string ValidateScheduleTravel(const ScheduleTravelTable& table)
{
    if (table.Legs.size() > ScheduleMaxTravelLegs)
        return "the travel table has too many legs";
    for (std::size_t i = 0; i < table.Legs.size(); ++i)
    {
        const ScheduleTravelLeg& leg = table.Legs[i];
        if (!IsStableIdToken(leg.From) || !IsStableIdToken(leg.To) || leg.From == leg.To)
            return ScheduleProblem("travel leg", i, "needs two different lowercase-token zones");
        if (leg.Minutes < 1 || leg.Minutes > ScheduleMaxTravelMinutes)
            return ScheduleProblem("travel leg", i, "needs 1..720 minutes");
        if (i > 0)
        {
            const ScheduleTravelLeg& earlier = table.Legs[i - 1];
            if (!(earlier.From < leg.From || (earlier.From == leg.From && earlier.To < leg.To)))
                return ScheduleProblem("travel leg", i, "is not in strictly ascending (from, to) order");
        }
    }
    return {};
}

std::string ValidateScheduleTravelCoverage(const PersonSchedule& person, const ScheduleTravelTable& table)
{
    std::set<std::string_view> zones = {person.HomeZone};
    for (const ScheduleEntry& entry : person.Routine)
        zones.insert(entry.Zone);
    for (const ScheduleException& exception : person.Exceptions)
        zones.insert(exception.Zone);
    for (auto a = zones.begin(); a != zones.end(); ++a)
    {
        for (auto b = std::next(a); b != zones.end(); ++b)
        {
            if (ScheduleFindLeg(table, *a, *b) == nullptr && ScheduleFindLeg(table, *b, *a) == nullptr)
                return "no travel leg between " + std::string(*a) + " and " + std::string(*b);
        }
    }
    return {};
}

std::string ValidateScheduleRoutineTravel(const PersonSchedule& person, const ScheduleTravelTable& table)
{
    PersonSchedule routineOnly = person;
    routineOnly.Exceptions.clear();
    // One plan spans 17 days, so every consecutive pair of a weekly routine, wrap-around included, appears in it.
    const ScheduleInternal::Plan plan = ScheduleInternal::BuildPlan(routineOnly, table, 10);
    for (const ScheduleInternal::Segment& segment : plan.Segments)
    {
        if (segment.Squeezed)
            return "the trip to " + segment.SourceId + " does not fit in the gap the routine leaves for it";
    }
    return {};
}

bool ScheduleCoversWholeWeek(const PersonSchedule& person)
{
    for (unsigned weekday = 0; weekday < 7; ++weekday)
    {
        std::vector<std::pair<std::int32_t, std::int32_t>> spans;
        for (const ScheduleEntry& entry : person.Routine)
        {
            if ((entry.WeekdayMask >> weekday & 1u) != 0 && entry.StartMinute < entry.EndMinute)
                spans.emplace_back(entry.StartMinute, entry.EndMinute);
        }
        std::sort(spans.begin(), spans.end());
        std::int32_t covered = 0;
        for (const auto& [start, end] : spans)
        {
            if (start > covered)
                break;
            covered = std::max(covered, end);
        }
        if (covered < MinutesPerDay)
            return false;
    }
    return true;
}

std::int32_t ScheduleTravelMinutes(const ScheduleTravelTable& table, std::string_view from, std::string_view to)
{
    if (from == to)
        return 0;
    if (const ScheduleTravelLeg* leg = ScheduleFindLeg(table, from, to))
        return leg->Minutes;
    if (const ScheduleTravelLeg* leg = ScheduleFindLeg(table, to, from))
        return leg->Minutes;
    return ScheduleFallbackTravelMinutes;
}
} // namespace PerfectWorld
