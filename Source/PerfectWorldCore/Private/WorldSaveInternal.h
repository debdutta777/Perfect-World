#pragma once

#include "Json.h"
#include "WorldSave.h"

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

// Shared by WorldSave.cpp (envelope, writer, migration) and WorldSaveReader.cpp (JSON to state). Not part of the public API.
namespace PerfectWorld
{
WorldSaveResult WorldSaveFail(WorldSaveError error, std::string message);
/** "This save file could not be read: <detail>" */
WorldSaveResult WorldSaveMalformed(const std::string& detail);
/** "This save file holds an impossible world: <detail>" */
WorldSaveResult WorldSaveInvalid(const std::string& detail);

bool WorldSaveParseHex(std::string_view text, std::uint64_t& out);
/** A JSON integer (no fraction or exponent), or false. */
bool WorldSaveAsInt(const JsonValue* value, std::int64_t& out);
/** A JSON integer within [low, high]. */
bool WorldSaveAsIntInRange(const JsonValue* value, std::int64_t low, std::int64_t high, std::int64_t& out);
bool WorldSaveAsHex(const JsonValue* value, std::uint64_t& out);
bool WorldSaveAsText(const JsonValue* value, std::string& out);
/** The object has exactly these keys: all present, none extra. (The parser already refused duplicates.) */
bool WorldSaveExactKeys(const JsonValue& object, std::initializer_list<std::string_view> keys);

/** The text of the top-level "creation" value of a document that has already parsed, or empty. */
std::string_view WorldSaveCreationSpan(std::string_view text);

/**
 * Reads the "world" and "player" objects. Success leaves the result default; a failure carries the refusal.
 * v6 gets a fresh day; v7 requires dayloop; v8 also requires conversations and doors; v9 journal; v10 commerce (read separately).
 * Every version refuses keys from later versions.
 */
WorldSaveResult WorldSaveReadWorld(const JsonValue& value, WorldState& world, std::int32_t version);
/** Canonical lane sections, including their module text envelopes. Empty for invalid lane authority. */
std::string WorldSaveWriteLane(const WorldState& world);
WorldSaveResult WorldSaveReadLane(const JsonValue& value, WorldState& world);
std::string WorldSaveQuoted(std::string_view value);
WorldSaveResult WorldSaveReadPlayer(const JsonValue& value, PlayerPlacement& player);
/** R8-04 (WorldSaveCommerce.cpp): the writer's "commerce" member text, or empty when commerce cannot be saved with this creation. */
std::string WorldSaveWriteCommerce(const WorldState& world, const CharacterCreationSave& creation);
/** v10 reads and validates pw.commerce/1; v6 to v9 receive InitialiseCommerce(householdRank, saved instant) once. The clock must be read. */
WorldSaveResult WorldSaveReadCommerce(const JsonValue& value, WorldState& world, std::int32_t version, std::int32_t householdRank);
/** S04 (WorldSaveSchool.cpp): the writer's "school" member text, or empty when the school slices break a rule. */
std::string WorldSaveWriteSchool(const WorldState& world);
/** v11 reads and validates pw.school/1 against the world's clock and day; v6 to v10 start with empty school slices. */
WorldSaveResult WorldSaveReadSchool(const JsonValue& value, WorldState& world, std::int32_t version);
/** S04 pw.school/1 stable names, indexed by SchoolLessonMode and by ModelGrade / StructureGrade (shared by writer and reader). */
inline constexpr std::string_view WorldSaveSchoolModeNames[] = {"foreground", "routine", "attend"};
inline constexpr std::string_view WorldSaveSchoolGradeNames[] = {"sound", "partial", "unsound"};
/** S04: a pw.school/1 text field: printable ASCII without spaces or commas, possibly empty. */
bool WorldSaveSchoolSafeText(std::string_view text);
/** S04 (WorldSaveSchool.cpp): the envelope without the world rules; empty when a field or native state cannot be written. */
std::string WorldSaveSchoolEncode(const SchoolSessionsState& sessions, const SchoolWorkState& work, const SchoolWeekState& week);
/** S04 (WorldSaveSchoolRules.cpp): the semantic rules of decoded school slices inside `world` (its clock and day loop). */
bool WorldSaveSchoolValid(const SchoolSessionsState& sessions, const SchoolWorkState& work, const SchoolWeekState& week,
                          const WorldState& world);

/** R9-04: the four Awakening slices one pw.awakening/1 envelope carries. */
struct WorldSaveAwakeningSlices
{
    AwakeningState Awakening;
    TutorialDungeonState Tutorial;
    std::string Countdown;
    std::optional<AwakenedProfile> Live;
};
/** R9-04 (WorldSaveAwakening.cpp): the writer's "awakening" member text, or empty when the slices break a rule. */
std::string WorldSaveWriteAwakening(const WorldState& world);
/** v12 reads and validates pw.awakening/1 against the world (clock, day, Gates); v6 to v11 start with all four empty. */
WorldSaveResult WorldSaveReadAwakening(const JsonValue& value, WorldState& world, std::int32_t version);
/** R9-04: a pw.awakening/1 text field: printable ASCII without spaces or commas, possibly empty. */
bool WorldSaveAwakeningSafeText(std::string_view text);
/** R9-04: the canonical comma-joined fields of a profile (the live line; also how two profiles are compared). */
std::string WorldSaveAwakeningProfileText(const AwakenedProfile& profile);
/** R9-04: the envelope without the world rules; empty when a field or native ledger cannot be written. */
std::string WorldSaveAwakeningEncode(const WorldSaveAwakeningSlices& slices);
WorldSaveAwakeningSlices WorldSaveAwakeningOf(const WorldState& world);
/** R9-04 (WorldSaveAwakeningDecode.cpp): strict parse, derived grant fields filled; false (out unspecified) otherwise. */
bool WorldSaveAwakeningDecode(std::string_view text, WorldSaveAwakeningSlices& out);
/** R9-04 (WorldSaveAwakeningRules.cpp): the semantic rules of the world's Awakening slices inside that world. */
bool WorldSaveAwakeningValid(const WorldState& world);
} // namespace PerfectWorld
