#include "ScheduleInternal.h"

#include <algorithm>

// The resolved day (ROADMAP M2-06). The rules are in Schedules.h; this file turns a
// schedule, its exceptions and a travel table into contiguous segments and
// answers the three questions asked of them: where is the person at an instant,
// when does that next change, and what did an advance pass through.
namespace PerfectWorld::ScheduleInternal
{
namespace
{
/** One candidate block: a routine occurrence on a particular day, or an exception. */
struct ScheduleItem
{
    GameInstant Start = 0;
    GameInstant End = 0;
    std::int32_t Priority = 0;
    bool IsException = false;
    std::int32_t Index = 0;
    const std::string* Zone = nullptr;
    const std::string* Doing = nullptr;
    const std::string* Id = nullptr;
    ScheduleDoingKind Kind = ScheduleDoingKind::Role;
};

/** The documented precedence: priority, then exception over routine, then later-starting exception, then smaller ID. */
bool ScheduleBeats(const ScheduleItem& a, const ScheduleItem& b)
{
    if (a.Priority != b.Priority)
        return a.Priority > b.Priority;
    if (a.IsException != b.IsException)
        return a.IsException;
    if (a.IsException)
    {
        if (a.Start != b.Start)
            return a.Start > b.Start;
        return *a.Id < *b.Id;
    }
    // Equal-priority routine overlap is refused by validation; this keeps resolution total.
    return a.Index < b.Index;
}

bool ScheduleItemLess(const ScheduleItem& a, const ScheduleItem& b)
{
    if (a.Start != b.Start)
        return a.Start < b.Start;
    if (a.IsException != b.IsException)
        return a.IsException < b.IsException;
    if (a.Index != b.Index)
        return a.Index < b.Index;
    return a.End < b.End;
}

/** A winning item over one stretch of the window. */
struct ScheduleRun
{
    const ScheduleItem* Item = nullptr;
    GameInstant Start = 0;
    GameInstant End = 0;
};

/** A stay while the plan is being laid out; its bounds may still move to make room for travel. */
struct ScheduleStay
{
    const ScheduleItem* Item = nullptr;
    GameInstant Start = 0;
    GameInstant End = 0;
    /** The stay was already under way when the window opened, so it has no trip into it. */
    bool Continuing = false;
    /** This stay's own bounds were shortened for a trip. */
    bool Squeezed = false;
    /** The trip into this stay needed a neighbour to be shortened. */
    bool TripSqueezed = false;
};

void ScheduleCollectItems(const PersonSchedule& person, std::int64_t day, GameInstant windowStart, GameInstant windowEnd,
                          std::vector<ScheduleItem>& items)
{
    for (std::int64_t d = day - ScheduleHorizonDays; d <= day + ScheduleHorizonDays; ++d)
    {
        const unsigned bit = 1u << WeekdayOfDay(d);
        const GameInstant base = DayStart(d);
        for (std::size_t i = 0; i < person.Routine.size(); ++i)
        {
            const ScheduleEntry& entry = person.Routine[i];
            // Malformed entries are ignored rather than trusted; validation is the place they are refused.
            if ((entry.WeekdayMask & ScheduleAllWeekdays) == 0 || entry.StartMinute < 0 || entry.EndMinute > MinutesPerDay ||
                entry.StartMinute >= entry.EndMinute || (entry.WeekdayMask & bit) == 0)
                continue;
            ScheduleItem item;
            item.Start = base + entry.StartMinute;
            item.End = base + entry.EndMinute;
            item.Priority = entry.Priority;
            item.Index = static_cast<std::int32_t>(i);
            item.Zone = &entry.Zone;
            item.Doing = &entry.Doing;
            item.Kind = entry.Kind;
            items.push_back(item);
        }
    }
    for (std::size_t i = 0; i < person.Exceptions.size(); ++i)
    {
        const ScheduleException& exception = person.Exceptions[i];
        if (exception.From < 0 || exception.From >= exception.To || exception.To > ScheduleMaxInstant ||
            exception.To <= windowStart || exception.From >= windowEnd)
            continue;
        ScheduleItem item;
        item.Start = exception.From;
        item.End = exception.To;
        item.Priority = exception.Priority;
        item.IsException = true;
        item.Index = static_cast<std::int32_t>(i);
        item.Zone = &exception.Zone;
        item.Doing = &exception.Doing;
        item.Id = &exception.Id;
        item.Kind = exception.Kind;
        items.push_back(item);
    }
    std::sort(items.begin(), items.end(), ScheduleItemLess);
}

/** The winning item over each stretch of the window, adjacent stretches of one item merged. */
std::vector<ScheduleRun> ScheduleSweep(const std::vector<ScheduleItem>& items, GameInstant windowStart, GameInstant windowEnd)
{
    std::vector<GameInstant> points = {windowStart, windowEnd};
    for (const ScheduleItem& item : items)
    {
        points.push_back(std::clamp(item.Start, windowStart, windowEnd));
        points.push_back(std::clamp(item.End, windowStart, windowEnd));
    }
    std::sort(points.begin(), points.end());
    points.erase(std::unique(points.begin(), points.end()), points.end());

    std::vector<ScheduleRun> runs;
    std::vector<std::size_t> active;
    std::size_t next = 0;
    for (std::size_t i = 0; i + 1 < points.size(); ++i)
    {
        const GameInstant from = points[i];
        const GameInstant to = points[i + 1];
        while (next < items.size() && items[next].Start <= from)
            active.push_back(next++);
        active.erase(std::remove_if(active.begin(), active.end(), [&](std::size_t index) { return items[index].End <= from; }),
                     active.end());
        const ScheduleItem* winner = nullptr;
        for (const std::size_t index : active)
        {
            if (winner == nullptr || ScheduleBeats(items[index], *winner))
                winner = &items[index];
        }
        if (winner == nullptr)
            continue;
        if (!runs.empty() && runs.back().Item == winner && runs.back().End == from)
            runs.back().End = to;
        else
            runs.push_back({winner, from, to});
    }
    return runs;
}

std::string ScheduleSourceId(const ScheduleItem& item)
{
    if (item.IsException)
        return *item.Id;
    return std::string(ScheduleRoutineSourcePrefix) + std::to_string(item.Index);
}

Segment ScheduleMakeSegment(ScheduleStatus status, GameInstant start, GameInstant end, const std::string& zone)
{
    Segment segment;
    segment.Status = status;
    segment.Start = start;
    segment.End = end;
    segment.Zone = zone;
    return segment;
}
} // namespace

Plan BuildPlan(const PersonSchedule& person, const ScheduleTravelTable& travel, std::int64_t day)
{
    const GameInstant windowStart = DayStart(day - ScheduleHorizonDays);
    const GameInstant windowEnd = DayStart(day + ScheduleHorizonDays + 1);
    std::vector<ScheduleItem> items;
    ScheduleCollectItems(person, day, windowStart, windowEnd, items);
    const std::vector<ScheduleRun> runs = ScheduleSweep(items, windowStart, windowEnd);

    // Pass 1: make room for travel. A stack, because a stay squeezed to nothing hands the check to the one before.
    std::vector<ScheduleStay> stays;
    for (const ScheduleRun& run : runs)
    {
        const ScheduleItem& item = *run.Item;
        ScheduleStay next;
        next.Item = &item;
        // A run clipped by the window keeps its item's real bounds.
        next.Continuing = run.Start == windowStart && item.Start < windowStart;
        next.Start = next.Continuing ? item.Start : run.Start;
        next.End = run.End == windowEnd && item.End > windowEnd ? item.End : run.End;
        bool dropped = false;
        while (!stays.empty() && !next.Continuing)
        {
            ScheduleStay& previous = stays.back();
            const std::int32_t trip = ScheduleTravelMinutes(travel, *previous.Item->Zone, *item.Zone);
            const std::int64_t deficit = trip - (next.Start - previous.End);
            if (deficit <= 0)
                break;
            if (previous.Item->Priority <= item.Priority)
            {
                if (previous.End - deficit <= previous.Start)
                {
                    stays.pop_back();
                    continue;
                }
                previous.End -= deficit;
                previous.Squeezed = true;
                next.TripSqueezed = true;
            }
            else
            {
                next.TripSqueezed = true;
                next.Start += deficit;
                next.Squeezed = true;
                dropped = next.Start >= next.End;
            }
            break;
        }
        if (!dropped)
            stays.push_back(next);
    }

    // Pass 2: lay out idle time, trips and stays.
    Plan plan;
    plan.Day = day;
    const std::string* previousZone = &person.HomeZone;
    GameInstant previousEnd = windowStart;
    for (const ScheduleStay& stay : stays)
    {
        const ScheduleItem& item = *stay.Item;
        const std::string& zone = *item.Zone;
        const std::string sourceId = ScheduleSourceId(item);
        const std::int64_t nominal = item.End - item.Start;

        // The first stay of the window has no predecessor: a trip only if there is room for it, never squeezed.
        const std::int32_t trip = stay.Continuing ? 0 : ScheduleTravelMinutes(travel, *previousZone, zone);
        const bool hasTrip = trip > 0 && stay.Start - previousEnd >= trip;
        const GameInstant tripStart = hasTrip ? stay.Start - trip : stay.Start;
        if (!stay.Continuing && tripStart > previousEnd)
            plan.Segments.push_back(ScheduleMakeSegment(ScheduleStatus::Idle, previousEnd, tripStart, *previousZone));
        if (hasTrip)
        {
            Segment leg = ScheduleMakeSegment(ScheduleStatus::Travel, tripStart, stay.Start, zone);
            leg.FromZone = *previousZone;
            leg.Kind = item.Kind;
            leg.Doing = *item.Doing;
            leg.SourceId = sourceId;
            leg.Squeezed = stay.TripSqueezed;
            leg.Nominal = nominal;
            plan.Segments.push_back(std::move(leg));
        }
        Segment piece = ScheduleMakeSegment(ScheduleStatus::Stay, stay.Start, stay.End, zone);
        piece.Kind = item.Kind;
        piece.Doing = *item.Doing;
        piece.SourceId = sourceId;
        piece.Squeezed = stay.Squeezed;
        piece.Nominal = nominal;
        plan.Segments.push_back(std::move(piece));

        previousZone = &zone;
        previousEnd = stay.End;
    }
    if (previousEnd < windowEnd)
        plan.Segments.push_back(ScheduleMakeSegment(ScheduleStatus::Idle, previousEnd, windowEnd, *previousZone));
    return plan;
}

const Segment* FindSegment(const Plan& plan, GameInstant at)
{
    const auto found = std::upper_bound(plan.Segments.begin(), plan.Segments.end(), at,
                                        [](GameInstant instant, const Segment& segment) { return instant < segment.Start; });
    if (found == plan.Segments.begin())
        return nullptr;
    const Segment& candidate = *(found - 1);
    return at < candidate.End ? &candidate : nullptr;
}

bool SameIdentity(const Segment& a, const Segment& b)
{
    return a.Status == b.Status && a.Zone == b.Zone && a.FromZone == b.FromZone && a.Kind == b.Kind && a.Doing == b.Doing &&
           a.SourceId == b.SourceId && a.Squeezed == b.Squeezed;
}

std::vector<Boundary> DayBoundaries(const Plan& plan, const Plan* previous)
{
    std::vector<Boundary> boundaries;
    const GameInstant dayStart = DayStart(plan.Day);
    const GameInstant dayEnd = DayStart(plan.Day + 1);
    if (previous != nullptr)
    {
        const Segment* before = FindSegment(*previous, dayStart - 1);
        const Segment* after = FindSegment(plan, dayStart);
        if (before != nullptr && after != nullptr && !SameIdentity(*before, *after))
            boundaries.push_back({dayStart, before, after});
    }
    for (const Segment& segment : plan.Segments)
    {
        if (segment.Start <= dayStart || segment.Start >= dayEnd)
            continue;
        const Segment* before = FindSegment(plan, segment.Start - 1);
        if (before != nullptr && !SameIdentity(*before, segment))
            boundaries.push_back({segment.Start, before, &segment});
    }
    return boundaries;
}

ScheduleResolution ToResolution(const Segment& segment, GameInstant at)
{
    ScheduleResolution out;
    out.Valid = true;
    out.Status = segment.Status;
    out.Zone = segment.Zone;
    out.FromZone = segment.FromZone;
    out.Kind = segment.Kind;
    out.Doing = segment.Doing;
    out.SourceId = segment.SourceId;
    out.SegmentStart = segment.Start;
    out.SegmentEnd = segment.End;
    const std::int64_t length = segment.End - segment.Start;
    out.ProgressPermille = length > 0 ? static_cast<std::int32_t>((at - segment.Start) * 1000 / length) : 0;
    out.Squeezed = segment.Squeezed;
    out.NominalMinutes = segment.Status == ScheduleStatus::Stay ? segment.Nominal : 0;
    return out;
}
} // namespace PerfectWorld::ScheduleInternal

namespace PerfectWorld
{
namespace
{
using namespace ScheduleInternal;

bool ScheduleInstantInRange(GameInstant at) { return at >= 0 && at <= ScheduleMaxInstant; }

ScheduleEvent ScheduleBeganEvent(GameInstant at, const Segment& segment)
{
    ScheduleEvent event;
    event.At = at;
    switch (segment.Status)
    {
    case ScheduleStatus::Stay: event.Kind = ScheduleEventKind::StayBegan; break;
    case ScheduleStatus::Travel: event.Kind = ScheduleEventKind::TravelBegan; break;
    case ScheduleStatus::Idle: event.Kind = ScheduleEventKind::IdleBegan; break;
    }
    event.Zone = segment.Zone;
    event.FromZone = segment.FromZone;
    event.DoingKind = segment.Kind;
    event.Doing = segment.Doing;
    event.SourceId = segment.SourceId;
    event.Squeezed = segment.Squeezed;
    return event;
}
} // namespace

ScheduleResolution ResolveSchedule(const PersonSchedule& person, const ScheduleTravelTable& travel, GameInstant at)
{
    if (!ScheduleInstantInRange(at))
        return ScheduleResolution{};
    const Plan plan = BuildPlan(person, travel, RelativeDay(at));
    const Segment* segment = FindSegment(plan, at);
    return segment == nullptr ? ScheduleResolution{} : ToResolution(*segment, at);
}

std::optional<GameInstant> NextScheduleBoundary(const PersonSchedule& person, const ScheduleTravelTable& travel,
                                                GameInstant after)
{
    if (!ScheduleInstantInRange(after))
        return std::nullopt;
    Plan previous;
    bool havePrevious = false;
    const std::int64_t firstDay = RelativeDay(after);
    for (std::int64_t day = firstDay; day <= firstDay + ScheduleBoundaryScanDays; ++day)
    {
        Plan plan = BuildPlan(person, travel, day);
        for (const Boundary& boundary : DayBoundaries(plan, havePrevious ? &previous : nullptr))
        {
            if (boundary.At > after)
                return boundary.At;
        }
        previous = std::move(plan);
        havePrevious = true;
    }
    return std::nullopt;
}

SchedulePersonState StartSchedulePerson(std::string_view personId, GameInstant at)
{
    SchedulePersonState state;
    state.PersonId = std::string(personId);
    state.At = at;
    return state;
}

ScheduleAdvanceResult AdvanceSchedulePerson(const PersonSchedule& person, const ScheduleTravelTable& travel,
                                            SchedulePersonState& state, GameInstant to)
{
    ScheduleAdvanceResult result;
    if (state.PersonId != person.PersonId)
        result.Error = "the state belongs to a different person";
    else if (!ScheduleInstantInRange(state.At) || !ScheduleInstantInRange(to))
        result.Error = "an instant lies outside 0..ScheduleMaxInstant";
    else if (to < state.At)
        result.Error = "time cannot run backwards";
    else if (to - state.At > MaxSingleAdvanceMinutes)
        result.Error = "the advance exceeds MaxSingleAdvanceMinutes";
    if (!result.Error.empty())
        return result;

    std::map<std::string, ScheduleActivityTally> tally = state.Tally;
    Plan previous;
    bool havePrevious = false;
    const std::int64_t lastDay = RelativeDay(to);
    for (std::int64_t day = RelativeDay(state.At); day <= lastDay && to > state.At; ++day)
    {
        Plan plan = BuildPlan(person, travel, day);
        for (const Boundary& boundary : DayBoundaries(plan, havePrevious ? &previous : nullptr))
        {
            if (boundary.At <= state.At || boundary.At > to)
                continue;
            const Segment& before = *boundary.Before;
            if (before.Status == ScheduleStatus::Stay && before.Kind == ScheduleDoingKind::Activity)
            {
                ScheduleEvent ended = ScheduleBeganEvent(boundary.At, before);
                ended.Kind = ScheduleEventKind::ActivityEnded;
                ended.Squeezed = before.Squeezed;
                ended.Minutes = boundary.At - before.Start;
                ended.NominalMinutes = before.Nominal;
                ended.Counted = ended.Minutes * 1000 >= ended.NominalMinutes * ScheduleSessionMinPartsPerThousand;
                ScheduleActivityTally& entry = tally[before.Doing];
                entry.Minutes += ended.Minutes;
                if (ended.Counted)
                    ++entry.Sessions;
                result.Events.push_back(std::move(ended));
            }
            result.Events.push_back(ScheduleBeganEvent(boundary.At, *boundary.After));
        }
        previous = std::move(plan);
        havePrevious = true;
    }
    state.Tally = std::move(tally);
    state.At = to;
    result.Ok = true;
    return result;
}
} // namespace PerfectWorld
