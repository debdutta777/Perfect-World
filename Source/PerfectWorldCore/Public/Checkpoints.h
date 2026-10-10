#pragma once

#include "DayLoop.h"
#include "SaveStore.h"

#include <cstdint>
#include <string>
#include <string_view>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * Recovery checkpoints, separate from life slots (ROADMAP M2-12; SYSTEMS
 * "Life slots and recovery checkpoints": slots are lives, checkpoints are a way
 * back within one life, never offered as slots).
 *
 * A life slot's file is SaveSlotFileName(i), "creation_slot_<n>.json". A
 * checkpoint's file is "checkpoint_<n>.txt" for n in 1..CheckpointCount. The two
 * families can never be equal: different stems and different extensions, and the
 * store's ".bak" and ".tmp" suffixes keep them apart. Writing reuses
 * WriteSaveFile (temp file, read back, validate, then rename), with the day-loop
 * envelope parser as the validator, so a checkpoint that does not parse is
 * never written over a good one.
 *
 * Rotation. The n-th wake is checkpoint ((n - 1) mod 3) + 1: the three most
 * recent wakes are always on disk, so "return to an earlier point" offers up to
 * three days back. ApplyDayCommand reports the index with CheckpointDue.
 *
 * The folder is the life's folder (SYSTEMS: Saved/SaveGames/slot_N/), given as
 * a UTF-8 path. The Unreal side owns choosing it.
 */

constexpr std::int32_t CheckpointCount = DayLoopCheckpointCount;

/** True for the index of one of the CheckpointCount checkpoints (zero based). */
[[nodiscard]] PERFECTWORLDCORE_API bool IsValidCheckpointIndex(std::int32_t index) noexcept;
/** "checkpoint_<index + 1>.txt"; empty for an index out of range. */
[[nodiscard]] PERFECTWORLDCORE_API std::string CheckpointFileName(std::int32_t index);
/** Whether a bare file name has the checkpoint form, with or without the store's ".bak" or ".tmp" suffix. */
[[nodiscard]] PERFECTWORLDCORE_API bool IsCheckpointFileName(std::string_view name);
/** The folder joined to the checkpoint's file name with '/'; empty for an index out of range. */
[[nodiscard]] PERFECTWORLDCORE_API std::string CheckpointPath(const std::string& folder, std::int32_t index);

/** Accepts only text that ParseDayLoop accepts. */
[[nodiscard]] PERFECTWORLDCORE_API SaveValidator DayLoopSaveValidator();

/** Writes the state's envelope as checkpoint `index`. A refusal leaves every file as it was. */
[[nodiscard]] PERFECTWORLDCORE_API SaveOperationResult WriteDayLoopCheckpoint(const std::string& folder,
                                                                              std::int32_t index,
                                                                              const DayLoopState& state);

struct DayLoopCheckpointRead
{
    SaveReadStatus Status = SaveReadStatus::Missing;
    /** Plain-sentence note for the player; empty for a plain Loaded. */
    std::string Message;
    /** Meaningful only when Succeeded(). */
    DayLoopState State;

    [[nodiscard]] bool Succeeded() const
    {
        return Status == SaveReadStatus::Loaded || Status == SaveReadStatus::RecoveredFromBackup;
    }
};

/** Reads checkpoint `index`, falling back to its ".bak" as the store does. */
[[nodiscard]] PERFECTWORLDCORE_API DayLoopCheckpointRead ReadDayLoopCheckpoint(const std::string& folder,
                                                                               std::int32_t index);
} // namespace PerfectWorld
