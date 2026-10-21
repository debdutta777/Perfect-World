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
    /**
     * The event cap was reached before the target; submit the remaining time again (To is where it stopped). Events
     * still due at To fire first on that continuation, or through time.settle (SettleDueEvents) until IsWorldQuiescent.
     */
    bool Partial = false;
    std::vector<DomainEvent> Events;
};

/** Scheduled events one advance may fire before it stops and reports Partial. */
constexpr std::size_t MaxEventsPerAdvance = 10'000;

/** Presentation-free school gate projection. All minute fields are local minutes of today's day. */
struct SchoolArrivalView
{
    SchoolDayKind DayKind = SchoolDayKind::OutsideYear;
    bool HasSchool = false;
    bool CanArrive = false;
    std::int32_t GatesOpenMinute = 0;
    /** Inclusive on-time window: GatesOpenMinute through FirstBellMinute. */
    std::int32_t FirstBellMinute = 0;
    std::int32_t DismissalMinute = 0;
    /** Holiday/break ID when present. No translated presentation text. */
    std::string ClosureId;
    /** Forecast assumes staying until dismissal. Recorded is read solely from DayLoop.Attendance. */
    std::optional<AttendanceForecast> Forecast;
    std::optional<AttendanceDay> Recorded;
    std::string ReasonTextKey;
    Rejection Refusal;
};

/** Pure; requires matching validated bell/calendar content, never falls back to weekdays for missing content. */
[[nodiscard]] PERFECTWORLDCORE_API SchoolArrivalView QuerySchoolArrival(const WorldState& state,
                                                                        const DayLoopCatalogues& school);

/**
 * The command reducer (SYSTEMS.md §2.2, ROADMAP M1-01). Submit is the only
 * mutation entry point: validate (pure; a rejection changes nothing), apply,
 * advance time event by event, commit. Unreal actors submit commands and
 * present the returned events; they never change state themselves.
 */
class PERFECTWORLDCORE_API WorldSim
{
public:
    /**
     * `eventCap` exists for tests; the game always uses MaxEventsPerAdvance.
     * School catalogues are borrowed, immutable and must outlive the sim; bind the same content on load/replay.
     */
    explicit WorldSim(std::uint64_t worldSeed, std::size_t eventCap = MaxEventsPerAdvance,
                      DayLoopCatalogues school = {});
    /**
     * Continue a loaded world (DeserializeWorldSave's World). The state must be in step
     * (IsWorldDayInStep); the command log starts empty, so it holds the commands since the load.
     */
    explicit WorldSim(WorldState state, std::size_t eventCap = MaxEventsPerAdvance, DayLoopCatalogues school = {});

    /**
     * The only mutation entry point. Atomic: a rejected command, including a paid conversation choice whose charge
     * could not complete, leaves the state exactly as it was.
     */
    CommandResult Submit(const Command& command);
    const WorldState& State() const noexcept { return State_; }
    /** Accepted commands in order: the replay log (ROADMAP M1-03). */
    const std::vector<Command>& Log() const noexcept { return Log_; }
    [[nodiscard]] SchoolArrivalView SchoolArrivalToday() const { return QuerySchoolArrival(State_, School_); }

private:
    std::optional<Rejection> Validate(const Command& command) const;
    /**
     * A paid conversation choice whose charge the event cap stopped short restores its rollback snapshot.
     * Free school commands stage their day-loop copy before replacing state, so their refusals need no snapshot.
     */
    std::optional<Rejection> Apply(const Command& command, CommandResult& result);
    bool NeedsRollbackSnapshot(const Command& command) const;
    /**
     * Moves the clock to `target` event by event: due-now events first, midnights before same-instant events,
     * the day loop kept in step at every boundary. Stops (Partial) when the event cap is spent before the target,
     * at the instant of the last fired event; everything unfired stays pending (events still due there fire first
     * on the continuation).
     */
    void RunTimeTo(GameInstant target, CommandResult& result);
    /** RunTimeTo(Now + minutes) with its TimeAdvanced placed before the events it caused. Partial when capped. */
    void ChargeTime(std::int64_t minutes, CommandResult& result);
    /** The checks every time-moving command shares (activity running, asleep, calendar end). */
    std::optional<Rejection> ValidateTimeCharge(std::int64_t minutes) const;
    // Conversation and door commands, in WorldConversation.cpp.
    std::optional<Rejection> ValidateLaneCommand(const Command& command) const;
    std::optional<Rejection> ApplyLaneCommand(const Command& command, CommandResult& result);
    // Free planning and arrival only, in WorldSchoolDay.cpp; application commits a successful day-loop copy.
    std::optional<Rejection> ValidateSchoolCommand(const Command& command) const;
    std::optional<Rejection> ApplySchoolCommand(const Command& command, CommandResult& result);

    WorldState State_;
    std::vector<Command> Log_;
    std::size_t EventCap_ = MaxEventsPerAdvance;
    DayLoopCatalogues School_;
};

/** True for the conversation.* and door.* commands (WorldConversation.cpp). */
[[nodiscard]] PERFECTWORLDCORE_API bool IsLaneCommand(const CommandPayload& payload) noexcept;
} // namespace PerfectWorld
