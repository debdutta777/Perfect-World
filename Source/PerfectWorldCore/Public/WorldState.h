#pragma once

#include "GameClock.h"

#include <cstdint>
#include <map>
#include <string>
#include <variant>

namespace PerfectWorld
{
/**
 * The authoritative world (SYSTEMS.md §2). Each member is one save section,
 * mutated only by the reducer through its owning system. This first version
 * holds the clock and world flags; systems add their slices as they arrive.
 */
struct WorldState
{
    std::uint64_t WorldSeed = 0;
    /** Accepted commands so far. */
    std::uint64_t Seq = 0;
    GameClock Clock;
    std::map<std::string, bool, std::less<>> Flags;
};

/** Things that happened, in the order they happened. Presentation rebuilds from these. */
struct TimeAdvanced
{
    GameInstant From = 0;
    GameInstant To = 0;
};

/** A new game day has begun (midnight). Fires before any scheduled event at the same instant. */
struct DayStarted
{
    std::int64_t Day = 0;
};

struct ScheduledEventFired
{
    std::int64_t Handle = 0;
    std::string EventId;
};

struct WorldFlagChanged
{
    std::string FlagId;
    bool Value = false;
};

using EventPayload = std::variant<TimeAdvanced, DayStarted, ScheduledEventFired, WorldFlagChanged>;

struct DomainEvent
{
    std::uint64_t Seq = 0;
    GameInstant At = 0;
    EventPayload Payload;
};
} // namespace PerfectWorld
