#pragma once

#include "WorldSave.h"
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
 * The persistent-state diff (ROADMAP M3-02; SYSTEMS "Determinism and replay": the
 * replay tool reports the first section that diverges, and the granularity
 * equivalence test needs to say exactly what differs). A pure comparison of two
 * worlds, or two world saves, section by section and field by field. It reads the
 * public state only and changes nothing.
 *
 * SECTIONS follow the world hash (WorldHash.h), in its order:
 *   meta, clock, flags, gates, rng, dayloop
 * and, for whole saves, player (the placement) and creation (the creation record).
 * The dayloop section is split by the day-loop envelope into its core lines
 * (state, condition, last, plan, commitment) and the four module texts (runner,
 * practice, receipts, attendance), and those texts are compared line by line,
 * field by field.
 *
 * PATHS are stable and readable, "<section>.<part>.<line>.<field>":
 *   meta.seq                              dayloop.core.state.seq
 *   clock.pending[7].when                 dayloop.core.condition.energy
 *   flags.door.open                       dayloop.receipts.e[seq=3].minutes
 *   gates.gate.a.one.status               dayloop.attendance.totals.on_time
 *   rng.opportunity.draws                 dayloop.practice.skill[skill.academics]
 * A repeated line is told apart by an index "[n]" (its position among lines with the
 * same tag), or by its identity where it has one (a receipt entry's seq, a family or
 * skill id, an attendance day). A line present on one side only is one entry whose
 * other value is "(absent)". Hash and "end" lines of the module texts are derived
 * from the fields and are not reported.
 *
 * If a section's hash differs but no field was found different (and nothing was
 * ignored), the diff itself has a gap; it reports "<section>.hash" so the gap
 * cannot hide.
 */

/** The value shown for a field, line or entry that one side does not have. */
inline constexpr std::string_view StateDiffAbsent = "(absent)";

struct StateDiffEntry
{
    /** The world-hash section name, or "player" or "creation" for a save. */
    std::string Section;
    std::string Path;
    /** The value in the first state, as text; StateDiffAbsent when it has none. */
    std::string A;
    std::string B;

    friend bool operator==(const StateDiffEntry&, const StateDiffEntry&) = default;
};

struct StateDiffOptions
{
    /**
     * Paths not to compare: a path equal to one, or below it ("dayloop.receipts" covers every receipt field).
     * The granularity-equivalence comparison ignores the command counters (WeekCompression.h).
     */
    std::vector<std::string> IgnorePaths;
};

struct StateDiffReport
{
    /** In section order, then in the order the fields were compared. Deterministic. */
    std::vector<StateDiffEntry> Entries;
    /** HashWorldState of each side (0 when a save could not be read). */
    std::uint64_t HashA = 0;
    std::uint64_t HashB = 0;

    /** True when no field differs (outside the ignored paths). */
    [[nodiscard]] bool Identical() const noexcept { return Entries.empty(); }
    /** The sections that have a difference, in section order, each once. */
    [[nodiscard]] PERFECTWORLDCORE_API std::vector<std::string> SectionNames() const;
    /** The entries of one section. */
    [[nodiscard]] PERFECTWORLDCORE_API std::vector<StateDiffEntry> InSection(std::string_view section) const;
};

/** Compares two worlds: meta, clock, flags, gates, rng and dayloop. */
[[nodiscard]] PERFECTWORLDCORE_API StateDiffReport DiffWorldStates(const WorldState& a, const WorldState& b,
                                                                   const StateDiffOptions& options = {});

/** Compares two whole saves: the world, then the player placement, then the creation record. */
[[nodiscard]] PERFECTWORLDCORE_API StateDiffReport DiffWorldSaves(const WorldSaveData& a, const WorldSaveData& b,
                                                                  const StateDiffOptions& options = {});

struct SaveTextDiff
{
    /** Empty on success; otherwise which side could not be read and why, for example "a: the save is damaged". */
    std::string Error;
    StateDiffReport Report;

    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/** Reads two save files' text (v7, or a v6 save with a fresh day) and compares them. A save that does not load is an Error, not a difference. */
[[nodiscard]] PERFECTWORLDCORE_API SaveTextDiff DiffWorldSaveTexts(std::string_view textA, std::string_view textB,
                                                                   const StateDiffOptions& options = {});

/**
 * The report as text, stable byte for byte:
 *   pwstatediff: identical (world hash 0x...)
 * or
 *   pwstatediff: 3 differences in 2 sections (world hash a=0x... b=0x...)
 *   section meta
 *     meta.seq: 4 -> 5
 *   section dayloop
 *     dayloop.core.state.now: 1440 -> 1450
 * Values containing spaces are shown as they are; "(absent)" marks a missing side.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string RenderStateDiff(const StateDiffReport& report);

/** The paths a report's entries name, in order (a convenience for tests and tools). */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<std::string> StateDiffPaths(const StateDiffReport& report);
} // namespace PerfectWorld
