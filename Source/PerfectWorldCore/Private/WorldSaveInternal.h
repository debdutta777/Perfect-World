#pragma once

#include "Json.h"
#include "WorldSave.h"

#include <cstdint>
#include <initializer_list>
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
 * `withDayLoop` is true for the v7 record (a "dayloop" key is required) and false for v6 (the key is refused
 * and the world gets a fresh day).
 */
WorldSaveResult WorldSaveReadWorld(const JsonValue& value, WorldState& world, bool withDayLoop);
WorldSaveResult WorldSaveReadPlayer(const JsonValue& value, PlayerPlacement& player);
} // namespace PerfectWorld
