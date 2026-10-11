#pragma once

#include "CharacterCreation.h"
#include "SaveStore.h"
#include "WorldState.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * The world save, schema v7 (ROADMAP M1-05, M2-12).
 *
 * A v7 slot is one JSON document holding the creation record (written and read
 * by CreationSaveSerialization, unchanged) plus a world snapshot: the clock, the
 * four RNG stream states, world flags and Gate sites by stable ID, the player's
 * day (the pw.dayloop/1 envelope text, which carries its own hash), and where
 * the player stands (zone, integer transform, camera mode). Files v1 to v5 held
 * the creation record alone (v1 to v4 exist; v5 arrives with M0-18); they load
 * by migration: the creation record is kept and a new world begins at 2 Jun
 * 2042 06:00 in the origin home. The original file is never rewritten here.
 *
 * v6 was the same document without the day loop. It still loads: it is read with
 * the v6 key set (no "dayloop" key) and the player gets a fresh day at the world
 * clock's instant. Its world hash used hash version 3, which this build cannot
 * recompute, so it loads unverified. Saves are always written as v7. The version
 * is bumped, rather than the "dayloop" key made optional, so every version keeps
 * the rule that every key is required and an unknown key is refused, and so an
 * older build refuses a v7 file as a newer version instead of as damaged.
 *
 * One clock: the day loop's Now must equal the world clock (IsWorldDayInStep),
 * or the save is refused as an impossible world.
 *
 * Everything is stored as integers, booleans and stable-ID strings, never floats.
 * 64-bit unsigned values (seeds, RNG words, the state hash) are "0x" plus 16
 * lowercase hex digits, because JSON numbers cannot hold them exactly.
 *
 * Integrity: the document is JSON that ends at its closing brace with no
 * trailing newline, so no proper prefix of it is a whole document, and it
 * stores the world hash. On load the hash is recomputed and a mismatch refuses
 * the save, so a damaged but still parseable file is never half-loaded. A file
 * whose stored hash version differs from WorldHashVersion skips that one check.
 *
 * Layout (key order as written; readers accept any order, but every key is required and unknown keys are refused):
 *   format "perfectworld.save", schemaVersion 6, creation {...},
 *   world { seed, seq, clock{now,nextHandle,pending[{handle,when,id}]}, flags{id:bool},
 *           gates{id:{rank,status,since,onsets}}, rng[{stream,words[4],draws}], dayloop "<pw.dayloop/1 text>" },
 *   player { zone, posCm[3], yawCdeg, pitchCdeg, camera }, hashVersion, stateHash.
 * The v6 world record is the same without "dayloop".
 */
constexpr std::int32_t WorldSaveSchemaVersion = 7;
/** The last schema whose world record has no day loop; it still loads, and gets a fresh day. */
constexpr std::int32_t WorldSaveLastWithoutDayLoopVersion = 6;
/** Files at or below this version are creation-only and migrate. */
constexpr std::int32_t WorldSaveLastCreationOnlyVersion = 5;
constexpr std::string_view WorldSaveFormatId = "perfectworld.save";

/** Positions are whole centimetres, within +-100,000 km of the world origin. */
constexpr std::int64_t WorldSavePositionLimitCm = 10'000'000'000LL;
/** Yaw is hundredths of a degree in [0, 36000); pitch is in [-9000, 9000]. */
constexpr std::int32_t WorldSaveFullTurnCdeg = 36000;
constexpr std::int32_t WorldSaveMaxPitchCdeg = 9000;
constexpr std::size_t WorldSaveMaxZoneIdLength = 128;

/** First person is the default; the player can switch (GAME_PLAN, product truth). */
enum class CameraMode : std::uint8_t
{
    FirstPerson,
    ThirdPerson
};

/** Stable text: "first_person", "third_person". Empty for a bad value. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view CameraModeName(CameraMode mode) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::optional<CameraMode> ParseCameraMode(std::string_view text) noexcept;

/** Where the player stands, for resume only (SYSTEMS_CATALOGUE: the transform is not authority for anything else). */
struct PlayerPlacement
{
    /** Stable zone ID: lowercase letters, digits, '_', '-' and '.'. */
    std::string ZoneId;
    std::array<std::int64_t, 3> PositionCm{};
    std::int32_t YawCdeg = 0;
    std::int32_t PitchCdeg = 0;
    CameraMode Camera = CameraMode::FirstPerson;

    friend bool operator==(const PlayerPlacement&, const PlayerPlacement&) = default;
};

/** Everything one slot holds. The world hash covers `World` only; the creation record and placement are saved beside it. */
struct WorldSaveData
{
    CharacterCreationSave Creation;
    WorldState World;
    PlayerPlacement Player;
};

enum class WorldSaveError
{
    None,
    Malformed,          // not a readable document: truncated, damaged or not a save at all
    UnsupportedVersion, // written by a newer build, or an envelope version this build never wrote
    InvalidCreation,    // the creation record does not validate
    InvalidWorld,       // the world snapshot or player placement breaks a rule
    HashMismatch        // it parsed, but the stored world hash disagrees with the loaded world
};

struct WorldSaveResult
{
    WorldSaveError Error = WorldSaveError::None;
    /** A plain sentence for the player; never contains a path. */
    std::string Message;
    /** The file was a creation-only v1 to v5 record and a new world was begun for it. (A v6 file is read, not migrated, and has no day loop.) */
    bool Migrated = false;
    /** The schema version of the file read (the creation record's version when Migrated). */
    std::int32_t SourceVersion = 0;
    /** The stored world hash was recomputed and matched. False for a migration, or a different hash version. */
    bool HashVerified = false;

    [[nodiscard]] bool Succeeded() const noexcept { return Error == WorldSaveError::None; }
    explicit operator bool() const noexcept { return Succeeded(); }
};

/** "zone.home.<home kit ID>": where a life opens, e.g. "zone.home.childrens_home_dormitory". */
[[nodiscard]] PERFECTWORLDCORE_API std::string OriginHomeZoneId(std::string_view homeKitId);

/**
 * A new life from a creation record: the record validated and normalised, a
 * fresh world seeded from `worldSeed` (clock at 2 Jun 2042 06:00, RNG streams
 * seeded, no flags or Gates), and the player in the origin home zone (resolved
 * from the household and Household Resources rank) at a zero transform in first
 * person. On failure `out` is untouched.
 */
[[nodiscard]] PERFECTWORLDCORE_API WorldSaveResult BeginWorldFromCreation(const CharacterCreationSave& creation,
                                                                          std::uint64_t worldSeed, WorldSaveData& out);

/** Always the current schema (v7). */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerializeWorldSave(const WorldSaveData& data);

/**
 * Reads a v7 slot (or a v6 one, giving it a fresh day), or migrates a v1 to v5 creation-only record (the migrated
 * world's seed is FNV-1a 64 of the file's bytes, so migrating the same file
 * twice gives the same world). Truncated, corrupt or inconsistent text is
 * refused with a message; `out` is untouched on any failure.
 */
[[nodiscard]] PERFECTWORLDCORE_API WorldSaveResult DeserializeWorldSave(std::string_view text, WorldSaveData& out);

/** For WriteSaveFile and ReadSaveFile: a slot validates when DeserializeWorldSave accepts it. */
[[nodiscard]] PERFECTWORLDCORE_API SaveValidator WorldSaveValidator();
} // namespace PerfectWorld
