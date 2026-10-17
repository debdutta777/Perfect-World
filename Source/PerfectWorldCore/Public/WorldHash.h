#pragma once

#include "Commands.h"
#include "WorldReducer.h"
#include "WorldState.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * State hash and command-log replay (SYSTEMS.md §2, ROADMAP M1-03).
 *
 * The hash is a fingerprint of the whole authoritative world, built from
 * explicit, tagged fields rather than memory layout, so it is identical on
 * every platform and compiler. Each save section hashes separately so a
 * desync can be traced to the section that diverged; the world hash then
 * combines the section hashes in a fixed order. Integer arithmetic only.
 *
 * Bump WorldHashVersion whenever the encoding or the set of sections changes;
 * the version is part of the world hash, so old and new hashes never compare equal.
 */
/**
 * 1: meta, clock, flags. 2 (2026-10-05): adds the gates section (ROADMAP M1-35).
 * 3 (2026-10-05): adds the rng section, the four stream states (ROADMAP M1-05).
 * 4 (2026-10-11): adds the dayloop section, HashDayLoop of the player's day (ROADMAP M2-12).
 * 5 (2026-10-17): adds conversations (person ID and canonical module text) and doors (canonical module text).
 */
constexpr std::uint32_t WorldHashVersion = 5;

struct WorldSectionHash
{
    std::string Name;
    std::uint64_t Hash = 0;
};

/** Fixed order: meta, clock, flags, gates, rng, dayloop, conversations, doors. */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<WorldSectionHash> HashWorldSections(const WorldState& state);

/** The fingerprint of the whole world. */
[[nodiscard]] PERFECTWORLDCORE_API std::uint64_t HashWorldState(const WorldState& state);

/**
 * HashWorldState with the doors section taken over `doorsText`, the pw.doors text as a save stored it. A save whose door
 * set migrated on load authenticates its original checksum with this; for unmigrated doors it equals HashWorldState.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::uint64_t HashWorldStateWithDoorsText(const WorldState& state, std::string_view doorsText);

struct ReplayOptions
{
    /** Passed to the replay world; must equal the cap of the run that produced the log. */
    std::size_t EventCap = MaxEventsPerAdvance;
    /** When non-zero, record the hash after every this-many accepted commands. */
    std::size_t CheckpointEvery = 0;
    /** Keep every command's result (events included); off by default to save memory. */
    bool KeepResults = false;
};

struct ReplayCheckpoint
{
    /** Accepted commands applied when the hash was taken. */
    std::uint64_t Seq = 0;
    std::uint64_t Hash = 0;
};

struct ReplayResult
{
    /** Every command in the log was accepted. */
    bool Complete = false;
    /** Commands accepted before replay stopped. */
    std::size_t Applied = 0;
    /** Why the first refused command was refused; meaningful only when !Complete. Its index is Applied. */
    Rejection Refusal;
    /** The world hash after the last applied command. */
    std::uint64_t Hash = 0;
    std::vector<ReplayCheckpoint> Checkpoints;
    /** One result per applied command (plus the refused one), when KeepResults is set. */
    std::vector<CommandResult> Results;
};

/**
 * Rebuild a world from its seed and an accepted-command log (WorldSim::Log()).
 * Stops at the first refused command and reports it: a log that replays
 * incompletely is corrupt, or was made with a different event cap.
 */
[[nodiscard]] PERFECTWORLDCORE_API ReplayResult ReplayCommandLog(std::uint64_t worldSeed,
                                                                  const std::vector<Command>& log,
                                                                  const ReplayOptions& options = {});
} // namespace PerfectWorld
