#include "WorldSave.h"

#include "CreationSaveSerialization.h"
#include "GateSites.h"
#include "Json.h"
#include "WorldHash.h"
#include "WorldSaveInternal.h"

#include <cctype>
#include <utility>

namespace PerfectWorld
{
namespace
{
std::string WorldSaveHex(std::uint64_t value)
{
    static const char digits[] = "0123456789abcdef";
    std::string text(18, '0');
    text[1] = 'x';
    for (int index = 0; index < 16; ++index)
        text[static_cast<std::size_t>(17 - index)] = digits[(value >> (4 * index)) & 0xfU];
    return text;
}

} // namespace

/** Quotes and escapes; control bytes are escaped, so module text round-trips exactly. */
std::string WorldSaveQuoted(std::string_view value)
{
    static const char digits[] = "0123456789abcdef";
    std::string out = "\"";
    for (const char raw : value)
    {
        const unsigned char c = static_cast<unsigned char>(raw);
        if (raw == '"')
            out += "\\\"";
        else if (raw == '\\')
            out += "\\\\";
        else if (raw == '\n')
            out += "\\n"; // the day loop's envelope is lines of text; keep it readable
        else if (c < 0x20)
        {
            out += "\\u00";
            out.push_back(digits[c >> 4]);
            out.push_back(digits[c & 0xf]);
        }
        else
            out.push_back(raw);
    }
    out.push_back('"');
    return out;
}

namespace
{
std::string WorldSaveHexQuoted(std::uint64_t value)
{
    return "\"" + WorldSaveHex(value) + "\"";
}

WorldSaveResult WorldSaveFromCreationError(const OperationResult& problem)
{
    return WorldSaveFail(problem.Error == CreationError::MalformedSaveText        ? WorldSaveError::Malformed
                         : problem.Error == CreationError::UnsupportedSaveVersion ? WorldSaveError::UnsupportedVersion
                                                                                  : WorldSaveError::InvalidCreation,
                         problem.Message);
}

WorldSaveResult WorldSaveReadEnvelope(std::string_view text, const JsonValue& doc, WorldSaveData& out)
{
    const JsonValue* format = doc.Find("format");
    if (!format->IsString() || format->String != WorldSaveFormatId)
        return WorldSaveMalformed("it is not a Perfect World save.");
    std::int64_t version = 0;
    if (!WorldSaveAsInt(doc.Find("schemaVersion"), version))
        return WorldSaveMalformed("the schema version is not a whole number.");
    if (version < WorldSaveLastWithoutDayLoopVersion || version > WorldSaveSchemaVersion)
        return WorldSaveFail(WorldSaveError::UnsupportedVersion, "This save was written by a different version of the game and cannot be opened.");
    if (!WorldSaveExactKeys(doc, {"format", "schemaVersion", "creation", "world", "player", "hashVersion", "stateHash"}))
        return WorldSaveMalformed("it is missing a section or has an unexpected one.");

    CharacterCreationSave creation;
    if (const OperationResult parsed = DeserializeCreationSave(WorldSaveCreationSpan(text), creation); !parsed)
        return WorldSaveFromCreationError(parsed);
    CharacterCreator creator;
    if (const OperationResult restored = creator.RestoreSave(creation); !restored)
        return WorldSaveFromCreationError(restored);

    WorldSaveData data;
    data.Creation = creator.CaptureSave();
    // A v6 world record has no day loop: the player gets a fresh day at the clock's instant.
    if (WorldSaveResult world = WorldSaveReadWorld(*doc.Find("world"), data.World, static_cast<std::int32_t>(version)); !world)
        return world;
    if (WorldSaveResult player = WorldSaveReadPlayer(*doc.Find("player"), data.Player); !player)
        return player;

    std::int64_t hashVersion = 0;
    std::uint64_t storedHash = 0;
    if (!WorldSaveAsIntInRange(doc.Find("hashVersion"), 1, INT32_MAX, hashVersion) || !WorldSaveAsHex(doc.Find("stateHash"), storedHash))
        return WorldSaveMalformed("the world hash is missing or malformed.");
    WorldSaveResult result;
    result.SourceVersion = static_cast<std::int32_t>(version);
    if (version == WorldSaveSchemaVersion && hashVersion == static_cast<std::int64_t>(WorldHashVersion))
    {
        if (HashWorldState(data.World) != storedHash)
            return WorldSaveFail(WorldSaveError::HashMismatch, "This save file is damaged: its world does not match its checksum.");
        result.HashVerified = true;
    }
    out = std::move(data);
    return result;
}

/** FNV-1a 64 of the file's bytes: the world seed of a migrated life, so migrating twice gives the same world. */
std::uint64_t WorldSaveLegacySeed(std::string_view text)
{
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    for (const char c : text)
    {
        hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(c));
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

WorldSaveResult WorldSaveMigrateCreationOnly(std::string_view text, WorldSaveData& out)
{
    CharacterCreationSave creation;
    if (const OperationResult parsed = DeserializeCreationSave(text, creation); !parsed)
    {
        // A cut-off v6 file fails the creation reader on its first key; say what actually happened.
        if (text.find(WorldSaveFormatId) != std::string_view::npos)
            return WorldSaveMalformed("it is incomplete or damaged.");
        return WorldSaveFromCreationError(parsed);
    }
    WorldSaveData data;
    WorldSaveResult begun = BeginWorldFromCreation(creation, WorldSaveLegacySeed(text), data);
    if (!begun)
        return begun;
    begun.Migrated = true;
    begun.SourceVersion = creation.SchemaVersion;
    out = std::move(data);
    return begun;
}
} // namespace

std::string_view CameraModeName(CameraMode mode) noexcept
{
    switch (mode)
    {
    case CameraMode::FirstPerson: return "first_person";
    case CameraMode::ThirdPerson: return "third_person";
    }
    return {};
}

std::optional<CameraMode> ParseCameraMode(std::string_view text) noexcept
{
    if (text == "first_person")
        return CameraMode::FirstPerson;
    if (text == "third_person")
        return CameraMode::ThirdPerson;
    return std::nullopt;
}

std::string OriginHomeZoneId(std::string_view homeKitId)
{
    return "zone.home." + std::string(homeKitId);
}

WorldSaveResult BeginWorldFromCreation(const CharacterCreationSave& creation, std::uint64_t worldSeed, WorldSaveData& out)
{
    CharacterCreator creator;
    if (const OperationResult restored = creator.RestoreSave(creation); !restored)
        return WorldSaveFromCreationError(restored);
    WorldSaveData data;
    data.Creation = creator.CaptureSave();
    data.World = MakeWorldState(worldSeed);
    data.Player.ZoneId = OriginHomeZoneId(creator.ResolveOpeningHome().HomeKitId);
    out = std::move(data);
    WorldSaveResult result;
    result.SourceVersion = WorldSaveSchemaVersion;
    return result;
}

std::string SerializeWorldSave(const WorldSaveData& data)
{
    const WorldState& world = data.World;
    const std::string lane = WorldSaveWriteLane(world);
    if (lane.empty())
        return {};
    std::string creation = SerializeCreationSave(data.Creation);
    while (!creation.empty() && std::isspace(static_cast<unsigned char>(creation.back())) != 0)
        creation.pop_back();

    std::string out;
    out.reserve(4096);
    out += "{\n  \"format\": " + WorldSaveQuoted(WorldSaveFormatId) + ",\n  \"schemaVersion\": " + std::to_string(WorldSaveSchemaVersion);
    out += ",\n  \"creation\": " + creation;
    out += ",\n  \"world\": {\n    \"seed\": " + WorldSaveHexQuoted(world.WorldSeed) + ",\n    \"seq\": " + WorldSaveHexQuoted(world.Seq);

    const GameClockSave clock = world.Clock.CaptureSave();
    out += ",\n    \"clock\": { \"now\": " + std::to_string(clock.Now) + ", \"nextHandle\": " + std::to_string(clock.NextHandle) + ", \"pending\": [";
    for (std::size_t index = 0; index < clock.Pending.size(); ++index)
    {
        const ScheduledEvent& event = clock.Pending[index];
        out += index == 0 ? "\n" : ",\n";
        out += "      { \"handle\": " + std::to_string(event.Handle) + ", \"when\": " + std::to_string(event.When) +
               ", \"id\": " + WorldSaveQuoted(event.Id) + " }";
    }
    out += clock.Pending.empty() ? "] }" : "\n    ] }";

    out += ",\n    \"flags\": {";
    bool first = true;
    for (const auto& [id, value] : world.Flags)
    {
        out += first ? "\n" : ",\n";
        first = false;
        out += "      " + WorldSaveQuoted(id) + ": " + (value ? "true" : "false");
    }
    out += first ? "}" : "\n    }";

    out += ",\n    \"gates\": {";
    first = true;
    for (const auto& [id, gate] : world.Gates)
    {
        out += first ? "\n" : ",\n";
        first = false;
        out += "      " + WorldSaveQuoted(id) + ": { \"rank\": " + WorldSaveQuoted(GateRankName(gate.Rank)) +
               ", \"status\": " + WorldSaveQuoted(GateStatusName(gate.Status)) + ", \"since\": " + std::to_string(gate.StatusSince) +
               ", \"onsets\": " + std::to_string(gate.OnsetCount) + " }";
    }
    out += first ? "}" : "\n    }";

    out += ",\n    \"rng\": [";
    for (std::size_t index = 0; index < world.Rng.size(); ++index)
    {
        const RandomStreamState& stream = world.Rng[index];
        out += index == 0 ? "\n" : ",\n";
        out += "      { \"stream\": " + WorldSaveQuoted(RandomStreamName(static_cast<RandomStreamId>(index))) + ", \"words\": [";
        for (std::size_t word = 0; word < stream.Words.size(); ++word)
            out += (word == 0 ? "" : ", ") + WorldSaveHexQuoted(stream.Words[word]);
        out += "], \"draws\": " + WorldSaveHexQuoted(stream.Draws) + " }";
    }
    // The player's day is its own envelope text, hash line included, carried as one string.
    out += "\n    ],\n    \"dayloop\": " + WorldSaveQuoted(SerialiseDayLoop(world.DayLoop)) + lane + "\n  }";

    const PlayerPlacement& player = data.Player;
    out += ",\n  \"player\": { \"zone\": " + WorldSaveQuoted(player.ZoneId) + ", \"posCm\": [" + std::to_string(player.PositionCm[0]) + ", " +
           std::to_string(player.PositionCm[1]) + ", " + std::to_string(player.PositionCm[2]) + "], \"yawCdeg\": " +
           std::to_string(player.YawCdeg) + ", \"pitchCdeg\": " + std::to_string(player.PitchCdeg) +
           ", \"camera\": " + WorldSaveQuoted(CameraModeName(player.Camera)) + " }";
    out += ",\n  \"hashVersion\": " + std::to_string(WorldHashVersion) + ",\n  \"stateHash\": " + WorldSaveHexQuoted(HashWorldState(world));
    // No trailing newline: a proper prefix of this text can never be a whole document.
    out += "\n}";
    return out;
}

WorldSaveResult DeserializeWorldSave(std::string_view text, WorldSaveData& out)
{
    const JsonParseResult parsed = ParseJson(text);
    if (parsed.Value && parsed.Value->IsObject() && parsed.Value->Find("format") != nullptr)
        return WorldSaveReadEnvelope(text, *parsed.Value, out);
    return WorldSaveMigrateCreationOnly(text, out);
}

SaveValidator WorldSaveValidator()
{
    return [](std::string_view text, std::string& error)
    {
        WorldSaveData data;
        const WorldSaveResult result = DeserializeWorldSave(text, data);
        if (!result)
            error = result.Message;
        return result.Succeeded();
    };
}
} // namespace PerfectWorld
