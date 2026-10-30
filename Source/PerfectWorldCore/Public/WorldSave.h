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
struct CommerceContent;

/**
 * The world save, schema v10 (ROADMAP M1-05, M2-12; lane order SV, R4-04; commerce R8-04).
 *
 * A v10 slot is one JSON document holding the creation record (written and read
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
 * recompute, so it loads unverified. Saves are always written as v9. The version
 * is bumped, rather than the "dayloop" key made optional, so every version keeps
 * the rule that every key is required and an unknown key is refused, and so an
 * older build refuses a v8 file as a newer version instead of as damaged.
 * v8 adds conversations (person ID keys in ascending order, values are canonical
 * pw.conversation/2 text) and doors (pw.doors/2 text). Unknown people, invalid
 * module states, mismatched open-session people and multiple open sessions are
 * refused. V6/v7 have no conversation entries and receive authored door defaults;
 * v7 retains its day. Their historical hashes are not recomputed or certified.
 * v9 adds required journal text (pw.lane_journal/1): dated successful conversation choices, unique per person/day.
 * V8 retains all conversation facts/agreements and migrates to an EMPTY journal; dates are never inferred from facts
 * or session counters. Historical v8/hash-v5 is authenticated over its original door text, then loads unverified
 * because journal/door migration changed the schema. The door and conversation envelopes remain at version 2.
 * v10 adds required commerce text (pw.commerce/1, below): wallet, inventory, funding, allowance cursor, revision,
 * receipts, folded totals and eligibility. It must be valid (IsCommerceStateValid), its funding must be the frozen table
 * row for the creation record's Household Resources rank, and its allowance cursor must equal the world clock.
 * Every earlier world (v6 to v9) receives InitialiseCommerce(creation rank, saved instant) exactly once: the household's
 * starting wallet, an empty inventory and the next allowance seven days after the saved instant, never backpay.
 * Historical v9/hash-v6 is authenticated with its original nine-section encoding over its stored door text first, and
 * loads unverified because the commerce migration changed the world; its first v10 save is certified normally.
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
 * with an unknown historical hash version skips recomputation; v8/hash-v5 and v9/hash-v6 are authenticated using their
 * original encodings.
 *
 * Layout (key order as written; readers accept any order, but every key is required and unknown keys are refused):
 *   format "perfectworld.save", schemaVersion 10, creation {...},
 *   world { seed, seq, clock{now,nextHandle,pending[{handle,when,id}]}, flags{id:bool},
 *           gates{id:{rank,status,since,onsets}}, rng[{stream,words[4],draws}], dayloop "<pw.dayloop/1 text>",
 *           conversations{personId:"<pw.conversation/2 text>"}, doors "<pw.doors/2 text>", journal "<pw.lane_journal/1 text>",
 *           commerce "<pw.commerce/1 text>" },
 *   player { zone, posCm[3], yawCdeg, pitchCdeg, camera }, hashVersion, stateHash.
 * V9 omits commerce; v8 also omits journal; v7 also omits conversations and doors; v6 also omits dayloop.
 *
 * pw.commerce/1 is lines of text, every number in canonical decimal, every field of CommerceState in declaration order:
 *   pw.commerce/1, wallet=<paise>, slot=<itemId>,<quantity> (exactly twelve; an empty slot is "slot=,0"),
 *   funding=<rank>,<startingPaise>,<weeklyPaise>, initialised=<t>, settled=<t>, next_allowance=<t>, revision=<u64>,
 *   receipts=<n>, then n lines receipt=<allowance|purchase>,<revision>,<at>,<shopId>,<itemId>,<quantity>,<unitPaise>,
 *   <totalPaise>,<balanceBefore>,<balanceAfter>, folded=<count>,<throughRevision>,<creditsPaise>,<debitsPaise>,
 *   tea_eligible=<t>, consumption_eligible=<t>, end. Only the exact canonical spelling is accepted.
 */
constexpr std::int32_t WorldSaveSchemaVersion = 10;
/** The last schema without commerce: v6 to v9 receive the one-time household initialisation on load. */
constexpr std::int32_t WorldSaveLastWithoutCommerceVersion = 9;
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
    /** A creation-only v1 to v5 record began a new world. V6/v7 defaults do not set this creation-only marker. */
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

/**
 * Always the current schema (v10). Invalid lane or commerce authority returns an empty string, as does commerce whose funding
 * is not the creation record's Household Resources row or whose allowance cursor is not the world clock.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerializeWorldSave(const WorldSaveData& data);

/**
 * Reads v10, v9 (household commerce initialised at the saved instant), v8 (also an empty journal), v7 (authored lane defaults), v6 (also a fresh day), or migrates a v1 to v5 creation-only record (the migrated
 * world's seed is FNV-1a 64 of the file's bytes, so migrating the same file
 * twice gives the same world). Truncated, corrupt or inconsistent text is
 * refused with a message; `out` is untouched on any failure.
 */
[[nodiscard]] PERFECTWORLDCORE_API WorldSaveResult DeserializeWorldSave(std::string_view text, WorldSaveData& out);

/**
 * DeserializeWorldSave, then every persisted commerce content ID (inventory items, purchase receipts' shops and items) must
 * name an item or shop in `content`; otherwise InvalidWorld and `out` is untouched. The game loads with its validated content.
 */
[[nodiscard]] PERFECTWORLDCORE_API WorldSaveResult DeserializeWorldSave(std::string_view text, WorldSaveData& out,
                                                                        const CommerceContent& content);

/** True when every inventory item and every purchase receipt's shop and item exist in `content`. */
[[nodiscard]] PERFECTWORLDCORE_API bool IsCommerceStateInContent(const CommerceState& state, const CommerceContent& content);

/** The canonical pw.commerce/1 text (format above); empty when the state is not IsCommerceStateValid. */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseCommerceState(const CommerceState& state);
/** Strict, canonical-only parse of pw.commerce/1 into a valid state; false (and `out` untouched) otherwise. */
[[nodiscard]] PERFECTWORLDCORE_API bool DeserialiseCommerceState(std::string_view text, CommerceState& out);

/** For WriteSaveFile and ReadSaveFile: a slot validates when DeserializeWorldSave accepts it. */
[[nodiscard]] PERFECTWORLDCORE_API SaveValidator WorldSaveValidator();
} // namespace PerfectWorld
