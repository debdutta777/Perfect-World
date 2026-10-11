#pragma once

#include "Commands.h"
#include "WorldState.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
struct Rejection
{
    /** Stable, machine-readable, e.g. "time.non_positive". */
    std::string Code;
    std::string Message;
};

struct CommandResult
{
    bool Accepted = false;
    Rejection Why;
    std::uint64_t Seq = 0;
    GameInstant From = 0;
    GameInstant To = 0;
    /** The event cap was reached before the target; submit the remaining time again. */
    bool Partial = false;
    std::vector<DomainEvent> Events;
};

/** Scheduled events one advance may fire before it stops and reports Partial. */
constexpr std::size_t MaxEventsPerAdvance = 10'000;

/**
 * The command reducer (SYSTEMS.md §2.2, ROADMAP M1-01). Submit is the only
 * mutation entry point: validate (pure; a rejection changes nothing), apply,
 * advance time event by event, commit. Unreal actors submit commands and
 * present the returned events; they never change state themselves.
 */
class PERFECTWORLDCORE_API WorldSim
{
public:
    /** `eventCap` exists for tests; the game always uses MaxEventsPerAdvance. */
    explicit WorldSim(std::uint64_t worldSeed, std::size_t eventCap = MaxEventsPerAdvance);
    /**
     * Continue a loaded world (DeserializeWorldSave's World). The state must be in step
     * (IsWorldDayInStep); the command log starts empty, so it holds the commands since the load.
     */
    explicit WorldSim(WorldState state, std::size_t eventCap = MaxEventsPerAdvance);

    CommandResult Submit(const Command& command);
    const WorldState& State() const noexcept { return State_; }
    /** Accepted commands in order: the replay log (ROADMAP M1-03). */
    const std::vector<Command>& Log() const noexcept { return Log_; }

private:
    std::optional<Rejection> Validate(const Command& command) const;
    void Apply(const Command& command, CommandResult& result);
    void RunTimeTo(GameInstant target, CommandResult& result);

    WorldState State_;
    std::vector<Command> Log_;
    std::size_t EventCap_ = MaxEventsPerAdvance;
};
} // namespace PerfectWorld
