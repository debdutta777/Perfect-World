#pragma once

#include "WorldSave.h"

#include <optional>
#include <string>
#include <string_view>

class APlayerController;

/** Storage only. All calls run synchronously on the game thread with one writer per slot.
 * Paths are absolute UTF-8, using the creator's existing creation_slot_N.json names.
 * These functions never access actors, subsystems, panels or the live creator.
 */
namespace PWLaneLifePersistence
{
/** Engine entry seam, implemented beside the origin resolver in GameMode.cpp.
 * Only the built lane can resume coordinates; the current origin's zero marker resolves at home.
 * A blocked/unsupported resume uses safe origin entry without rewriting the slot.
 */
bool PlacePlayerAtSavedPlacement(APlayerController& Player, const PerfectWorld::PlayerPlacement& Saved,
    bool& UsedOrigin);
struct CaptureResult
{
    /** Present only when the complete serialised snapshot passes the core reader. */
    std::optional<PerfectWorld::WorldSaveData> Data;
    std::string Text;
    PerfectWorld::WorldSaveResult Validation;

    [[nodiscard]] bool Succeeded() const { return Data.has_value() && !Text.empty() && Validation.Succeeded(); }
};

/** Copy the live creator record, WorldSim::State() and integer placement, then serialise
 * and validate the WHOLE envelope. Refusal has no Data and no Text; never write it.
 * Capture between commands on the game thread, after door motion has settled and
 * time.settle has reached quiescence. An unsettled world is refused.
 * Angles are centidegrees and positions are whole cm, per PlayerPlacement.
 */
[[nodiscard]] CaptureResult CaptureWorldSave(const PerfectWorld::CharacterCreationSave& Creation,
    const PerfectWorld::WorldState& World, const PerfectWorld::PlayerPlacement& Player);

/** Refuse empty/invalid/non-v8 text before disk access. Reuses the portable flushed-temp,
 * read-back, validation, backup and rename protocol. Interrupted temps are never loaded.
 * A recovered backup is protected against rotation of a damaged primary over it.
 */
[[nodiscard]] PerfectWorld::SaveOperationResult WriteWorldSlot(const std::string& AbsolutePath, std::string_view Text);

struct ReadResult
{
    /** Detached candidate only. No live state has changed, even on success. */
    std::optional<PerfectWorld::WorldSaveData> Data;
    PerfectWorld::SaveReadStatus Status = PerfectWorld::SaveReadStatus::Missing;
    /** SourceVersion, HashVerified and Migrated refer to the original file, before restore. */
    PerfectWorld::WorldSaveResult Validation;
    /** Player-facing refusal or backup recovery notice; never contains a path. */
    std::string Message;

    [[nodiscard]] bool Succeeded() const { return Data.has_value() && Validation.Succeeded(); }
};

/** Fully validates primary or .bak with DeserializeWorldSave, including core migrations.
 * Reads never rewrite a legacy file. The saved open conversation remains in the candidate;
 * the integrator must close it on a detached WorldSim before the restore commit.
 */
[[nodiscard]] ReadResult ReadWorldSlot(const std::string& AbsolutePath);
} // namespace PWLaneLifePersistence
