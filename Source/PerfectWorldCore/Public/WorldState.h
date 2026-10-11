#pragma once

#include "DayLoop.h"
#include "GameClock.h"
#include "RandomStreams.h"

#include <cstdint>
#include <map>
#include <string>
#include <variant>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/** Gate rank, lowest to highest. The numeric order is the rank order. */
enum class GateRank : std::uint8_t
{
    F,
    E,
    D,
    C,
    B,
    A,
    S
};

/** Content data's "open" is Active here. Contained means the NGA holds it. */
enum class GateStatus : std::uint8_t
{
    Contained,
    Active,
    Sealed
};

/** One Gate site (ROADMAP M1-35). Changed only by the gate system, through commands and scheduled onsets. */
struct GateSiteState
{
    GateRank Rank = GateRank::F;
    GateStatus Status = GateStatus::Contained;
    /** The instant the current status began. */
    GameInstant StatusSince = 0;
    /** Scheduled onsets that have fired for this Gate. */
    std::int32_t OnsetCount = 0;
};

/**
 * The authoritative world (SYSTEMS.md §2). Each member is one save section,
 * mutated only by the reducer through its owning system. This version holds the
 * clock, world flags, Gate sites, the RNG streams and the player's day; systems
 * add their slices as they arrive.
 */
struct WorldState
{
    std::uint64_t WorldSeed = 0;
    /** Accepted commands so far. */
    std::uint64_t Seq = 0;
    GameClock Clock;
    std::map<std::string, bool, std::less<>> Flags;
    /** Gate sites by stable ID (the gate system's slice). */
    std::map<std::string, GateSiteState, std::less<>> Gates;
    /**
     * The persisted state of the four RNG streams (M1-02), indexed by RandomStreamId.
     * WorldSim seeds it from WorldSeed; whatever draws from a stream writes the
     * captured state back here, so a reload continues the sequence instead of re-rolling it.
     */
    RandomStreams::Snapshot Rng = RandomStreams().Capture();
    /**
     * The player's day (M2-12): condition, the running activity, practice, receipts, attendance,
     * the plan and the commitment. It keeps its own Now, but there is one clock: in a valid world
     * DayLoop.Now equals Clock.Now() (IsWorldDayInStep). The world clock is authoritative.
     * AdvanceTime moves both; the day loop's own commands move its Now, and a world that routes
     * them must move Clock to match. A fresh world is a fresh life at the clock's instant.
     */
    DayLoopState DayLoop = NewDayLoopState(0);
};

/** A fresh world: the clock at the campaign epoch (2 Jun 2042 06:00), no flags or Gates, RNG streams seeded from the world seed, a fresh day. */
inline WorldState MakeWorldState(std::uint64_t worldSeed)
{
    WorldState state;
    state.WorldSeed = worldSeed;
    state.Rng = RandomStreams(worldSeed).Capture();
    state.DayLoop = NewDayLoopState(state.Clock.Now());
    return state;
}

/**
 * True when the player's day is a valid day loop and its clock is the world clock. The save
 * reader requires it, and the reducer keeps it true: a time advance moves both clocks.
 */
[[nodiscard]] PERFECTWORLDCORE_API bool IsWorldDayInStep(const WorldState& state);

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

struct GateRegistered
{
    std::string GateId;
    GateRank Rank = GateRank::F;
    GateStatus Status = GateStatus::Contained;
};

/** A scheduled onset fired and the Gate is now active. Emitted right after that event's ScheduledEventFired. */
struct GateOnset
{
    std::string GateId;
    GateRank Rank = GateRank::F;
    GateStatus Previous = GateStatus::Contained;
    /** Onsets fired for this Gate, this one included. */
    std::int32_t OnsetCount = 0;
};

/** A scheduled onset fired at a sealed Gate: the seal held and the Gate is unchanged. */
struct GateOnsetHeld
{
    std::string GateId;
};

struct GateContained
{
    std::string GateId;
};

struct GateSealed
{
    std::string GateId;
};

using EventPayload = std::variant<TimeAdvanced, DayStarted, ScheduledEventFired, WorldFlagChanged, GateRegistered, GateOnset,
                                  GateOnsetHeld, GateContained, GateSealed>;

struct DomainEvent
{
    std::uint64_t Seq = 0;
    GameInstant At = 0;
    EventPayload Payload;
};
} // namespace PerfectWorld
