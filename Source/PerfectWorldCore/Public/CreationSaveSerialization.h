#pragma once

#include "CharacterCreation.h"

#include <string>
#include <string_view>

namespace PerfectWorld
{
/**
 * Text serialization for creation records.
 *
 * Trait ranks are written by stable trait ID, never by array position, so
 * reordering or inserting traits cannot silently reinterpret an existing file.
 * The format is a small deliberate subset of JSON: objects, strings, integers
 * and booleans. It has no dependency on Unreal, so the round trip and every
 * corruption case are covered by the portable CTest suite.
 */

/** Human-readable slot file name, e.g. slot index 0 -> "creation_slot_1.json". */
[[nodiscard]] PERFECTWORLDCORE_API std::string SaveSlotFileName(std::int32_t slotIndex);

/** True when the slot index names one of the SaveSlotCount supported slots. */
[[nodiscard]] PERFECTWORLDCORE_API bool IsValidSaveSlot(std::int32_t slotIndex) noexcept;

/** Serialize a record. Always emits the current schema version. */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerializeCreationSave(const CharacterCreationSave& save);

/**
 * Parse a record written by SerializeCreationSave. On any failure `outSave` is
 * left untouched and the result carries a message suitable for showing the
 * player, so a corrupt file is recoverable rather than fatal.
 */
[[nodiscard]] PERFECTWORLDCORE_API OperationResult DeserializeCreationSave(std::string_view text,
                                                                           CharacterCreationSave& outSave);
} // namespace PerfectWorld
