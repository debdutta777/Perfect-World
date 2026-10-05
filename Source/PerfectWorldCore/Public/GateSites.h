#pragma once

#include "ContentRegistry.h"
#include "WorldReducer.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * Gate sites in the world state (ROADMAP M1-35, GAME_PLAN §2 "Hazard onset").
 *
 * A Gate's onset cannot be prevented, only scheduled: gate.schedule_onset puts
 * "gates/onset/<id>" on the clock, and when the reducer's time advance fires
 * an event whose ID starts with "gates/" it hands it to RouteGateEvent. Status
 * changes only through the four gate commands (System or Debug source) and a
 * fired onset; contained -> sealed is one way, and a seal holds against onset.
 *
 * Rejection codes (stable): gate.source, gate.bad_id, gate.bad_rank,
 * gate.bad_status, gate.duplicate, gate.unknown, gate.sealed, gate.in_past,
 * gate.not_active, gate.not_contained.
 */
PERFECTWORLDCORE_API std::string_view GateRankName(GateRank rank) noexcept;
PERFECTWORLDCORE_API std::string_view GateStatusName(GateStatus status) noexcept;
/** "F".."S". Anything else is nullopt. */
PERFECTWORLDCORE_API std::optional<GateRank> ParseGateRank(std::string_view text);
/** "contained", "active", "sealed"; content data's "open" means active. */
PERFECTWORLDCORE_API std::optional<GateStatus> ParseGateStatus(std::string_view text);

/** The clock event ID for a Gate's onset: "gates/onset/<id>". */
PERFECTWORLDCORE_API std::string GateOnsetEventId(std::string_view gateId);

/** True for the four gate commands. */
PERFECTWORLDCORE_API bool IsGateCommand(const CommandPayload& payload);
/** Pure. nullopt means the command is acceptable. Called by the reducer for gate commands. */
PERFECTWORLDCORE_API std::optional<Rejection> ValidateGateCommand(const WorldState& state, const Command& command);
/** Applies an already validated gate command, appending its events (Seq is stamped by the reducer). */
PERFECTWORLDCORE_API void ApplyGateCommand(WorldState& state, const Command& command, std::vector<DomainEvent>& events);

/** True for a clock event owned by the gate system (ID starts with "gates/"). */
PERFECTWORLDCORE_API bool IsGateEventId(std::string_view eventId);
/**
 * Handles a fired clock event the reducer routed here. An onset for a Gate
 * that is registered and not sealed makes it active, counts the onset and
 * emits GateOnset; for a sealed Gate it emits GateOnsetHeld and changes
 * nothing; for an unknown Gate or an unrecognised "gates/" event it does nothing.
 */
PERFECTWORLDCORE_API void RouteGateEvent(WorldState& state, const ScheduledEvent& fired, std::vector<DomainEvent>& events);

/**
 * One gate.register command (System source) for every "gate_site" record of
 * the content, in ID order. A record whose rank or status is not understood is skipped.
 */
PERFECTWORLDCORE_API std::vector<Command> GateRegistrationCommands(const ContentDb& db);
/** Submits GateRegistrationCommands to the world and returns how many were accepted. */
PERFECTWORLDCORE_API std::size_t RegisterContentGates(WorldSim& sim, const ContentDb& db);
} // namespace PerfectWorld
