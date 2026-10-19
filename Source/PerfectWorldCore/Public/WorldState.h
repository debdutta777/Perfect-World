#pragma once

#include "Conversation.h"
#include "DayLoop.h"
#include "Doors.h"
#include "GameClock.h"
#include "LaneJournal.h"
#include "RandomStreams.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
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
    /**
     * The lane's conversation memory, one ConversationState (Conversation.h) per person spoken to, keyed by person id:
     * that person's open session, remembered facts, bOnceEver choices and the current session's paid-choice receipts
     * (ChoiceId, Session). Each state is valid under its person's own catalogue (ConversationCatalogueForPerson), so the
     * household, care and Farida content never share facts or session numbers. At most one state is Open in the world
     * (OpenConversationPersonId). An entry exists once its person has been spoken to. Changed only by the conversation.*
     * commands (WorldConversation.cpp), which charge a paid choice's minutes through the world clock in the same command.
     * Since world v8 each entry is saved in key order as the person id plus its "pw.conversation/2" text and hashed.
     * Unknown people and invalid states are refused. Earlier saves start with no entries. Remembered journal agreements
     * are projected from these current facts, never persisted as a second copy.
     */
    std::map<std::string, ConversationState, std::less<>> Conversations;
    /** Dated successful exchanges, pw.lane_journal/1 in world v9. Older saves have no dated evidence. */
    LaneJournalState Journal;
    /**
     * The lane's doors (Doors.h): one physical entry (open, swing side) per authored door, in table order. Changed
     * only by the door.set and door.settle commands. Since world v8 persisted as "pw.doors/2" and hashed;
     * earlier worlds start from the authored defaults, as here. Historical door rosters migrate explicitly.
     */
    DoorState Doors = MakeInitialDoorState();
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

/**
 * True when nothing is due at the current instant (LANE_LIFE_IMPLEMENTATION §A item 5). An event-capped advance can
 * stop with events still due now; submit time.settle (SettleDueEvents) until this holds before saving or before the
 * next player command. The conversation.* and door.* commands are refused ("world.unsettled") until it does.
 */
[[nodiscard]] PERFECTWORLDCORE_API bool IsWorldQuiescent(const WorldState& state);

/**
 * The conversation catalogue that speaks for a person (WorldConversation.cpp): the first of the household, care and
 * Farida catalogues with a node for that person id, or empty for a person none of them knows. Until the composed lane
 * catalogue (CAT order) exists, this is the one person-to-content routing rule; the save order uses it too.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::optional<ConversationCatalogue> ConversationCatalogueForPerson(std::string_view personId) noexcept;

/** The person whose conversation session is open, or empty when none is. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view OpenConversationPersonId(const WorldState& state) noexcept;

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

/** A conversation session opened at the world clock's minute of day. */
struct ConversationOpened
{
    std::string PersonId;
    std::string NodeId;
    std::uint32_t Session = 0;
};

/**
 * One authored conversation choice was accepted: its facts are applied and, when CostMinutes > 0, exactly that many
 * minutes follow in the same command's TimeAdvanced (the charge for the receipt (ChoiceId, Session)). Leave emits only
 * ConversationEnded.
 */
struct ConversationChoiceAccepted
{
    std::string PersonId;
    std::string ChoiceId;
    std::uint32_t Session = 0;
    std::int32_t CostMinutes = 0;
    std::string FactSet;
    std::string FactCleared;
    /** Empty when the session ended. */
    std::string NextNodeId;
};

/** The open session ended (Leave, or a choice with no next node). */
struct ConversationEnded
{
    std::string PersonId;
    std::uint32_t Session = 0;
};

/** A door's physical state changed: through access rules (door.set) or a reported settlement (door.settle). */
struct DoorChanged
{
    std::string DoorId;
    bool Open = false;
    std::int8_t SwingSide = 0;
    bool Settlement = false;
};

using EventPayload = std::variant<TimeAdvanced, DayStarted, ScheduledEventFired, WorldFlagChanged, GateRegistered, GateOnset,
                                  GateOnsetHeld, GateContained, GateSealed, ConversationOpened, ConversationChoiceAccepted,
                                  ConversationEnded, DoorChanged>;

struct DomainEvent
{
    std::uint64_t Seq = 0;
    GameInstant At = 0;
    EventPayload Payload;
};
} // namespace PerfectWorld
