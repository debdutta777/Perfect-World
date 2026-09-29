#include "WorldReducer.h"

#include <algorithm>

namespace PerfectWorld
{
namespace
{
bool IsLowerIdChar(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
}

/** "system/kind/key": three non-empty parts of stable-ID characters. */
bool IsScheduledEventId(std::string_view id)
{
    int parts = 1;
    std::size_t partLength = 0;
    for (const char c : id)
    {
        if (c == '/')
        {
            if (partLength == 0)
                return false;
            ++parts;
            partLength = 0;
        }
        else if (IsLowerIdChar(c))
        {
            ++partLength;
        }
        else
        {
            return false;
        }
    }
    return parts == 3 && partLength > 0;
}

bool IsFlagId(std::string_view id)
{
    return !id.empty() && id.size() <= 128 && std::all_of(id.begin(), id.end(), IsLowerIdChar) && id.front() != '.' && id.back() != '.';
}

Rejection Reject(std::string code, std::string message)
{
    return Rejection{std::move(code), std::move(message)};
}
} // namespace

WorldSim::WorldSim(std::uint64_t worldSeed, std::size_t eventCap) : EventCap_(eventCap == 0 ? MaxEventsPerAdvance : eventCap)
{
    State_.WorldSeed = worldSeed;
}

std::optional<Rejection> WorldSim::Validate(const Command& command) const
{
    if (const auto* advance = std::get_if<AdvanceTime>(&command.Payload))
    {
        if (advance->Minutes <= 0)
            return Reject("time.non_positive", "Time can only move forward, by at least one minute.");
        if (advance->Minutes > MaxSingleAdvanceMinutes)
            return Reject("time.too_long", "A single advance may not exceed ten years.");
        return std::nullopt;
    }
    if (const auto* schedule = std::get_if<ScheduleWorldEvent>(&command.Payload))
    {
        if (command.Source != CommandSource::System && command.Source != CommandSource::Debug)
            return Reject("schedule.source", "Only systems schedule world events.");
        if (!IsScheduledEventId(schedule->EventId))
            return Reject("schedule.bad_id", "Scheduled event IDs are \"system/kind/key\".");
        if (schedule->When < State_.Clock.Now())
            return Reject("schedule.in_past", "An event cannot be scheduled in the past.");
        return std::nullopt;
    }
    if (const auto* flag = std::get_if<SetWorldFlag>(&command.Payload))
    {
        if (!IsFlagId(flag->FlagId))
            return Reject("flag.bad_id", "Flag IDs are lowercase letters, digits, '_', '-' and '.'.");
        return std::nullopt;
    }
    return Reject("command.unknown", "Unknown command.");
}

void WorldSim::RunTimeTo(GameInstant target, CommandResult& result)
{
    std::size_t fired = 0;
    while (State_.Clock.Now() < target)
    {
        if (fired >= EventCap_)
        {
            result.Partial = true;
            break;
        }
        // Step to whichever comes first: the next scheduled event, the next
        // midnight, or the target. Midnight's DayStarted comes before any
        // event scheduled for that same instant.
        const GameInstant now = State_.Clock.Now();
        const GameInstant nextMidnight = GameClock::StartOfDay(now) + MinutesPerDay;
        GameInstant step = std::min(target, nextMidnight);
        const GameInstant nextEvent = State_.Clock.NextEventInstant();
        if (nextEvent >= 0 && nextEvent < step)
        {
            step = std::max(nextEvent, now + 1);
        }
        const AdvanceResult advanced = State_.Clock.AdvanceTo(step);
        // Chronological order: anything due before this instant (an event
        // scheduled at the starting instant), then midnight, then the rest.
        bool dayAnnounced = step != nextMidnight;
        for (const ScheduledEvent& event : advanced.Fired)
        {
            if (!dayAnnounced && event.When >= step)
            {
                result.Events.push_back({0, step, DayStarted{GameClock::DayNumber(step)}});
                dayAnnounced = true;
            }
            result.Events.push_back({0, event.When, ScheduledEventFired{event.Handle, event.Id}});
            ++fired;
        }
        if (!dayAnnounced)
        {
            result.Events.push_back({0, step, DayStarted{GameClock::DayNumber(step)}});
        }
    }
}

void WorldSim::Apply(const Command& command, CommandResult& result)
{
    if (const auto* advance = std::get_if<AdvanceTime>(&command.Payload))
    {
        const GameInstant from = State_.Clock.Now();
        const std::size_t firstEvent = result.Events.size();
        RunTimeTo(from + advance->Minutes, result);
        result.Events.insert(result.Events.begin() + static_cast<std::ptrdiff_t>(firstEvent),
                             DomainEvent{0, from, TimeAdvanced{from, State_.Clock.Now()}});
    }
    else if (const auto* schedule = std::get_if<ScheduleWorldEvent>(&command.Payload))
    {
        (void)State_.Clock.Schedule(schedule->When, schedule->EventId);
    }
    else if (const auto* flag = std::get_if<SetWorldFlag>(&command.Payload))
    {
        const auto existing = State_.Flags.find(flag->FlagId);
        if (existing == State_.Flags.end() || existing->second != flag->Value)
        {
            State_.Flags[flag->FlagId] = flag->Value;
            result.Events.push_back({0, State_.Clock.Now(), WorldFlagChanged{flag->FlagId, flag->Value}});
        }
    }
}

CommandResult WorldSim::Submit(const Command& command)
{
    CommandResult result;
    result.From = State_.Clock.Now();
    result.To = result.From;
    result.Seq = State_.Seq;
    if (const std::optional<Rejection> rejection = Validate(command))
    {
        result.Why = *rejection;
        return result;
    }
    Apply(command, result);
    State_.Seq += 1;
    Log_.push_back(command);
    result.Accepted = true;
    result.Seq = State_.Seq;
    result.To = State_.Clock.Now();
    for (DomainEvent& event : result.Events)
    {
        event.Seq = State_.Seq;
    }
    return result;
}
} // namespace PerfectWorld
