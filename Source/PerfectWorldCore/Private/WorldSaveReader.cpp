#include "WorldSaveInternal.h"

#include "GateSites.h"

#include <cctype>
#include <utility>

namespace PerfectWorld
{
namespace
{
constexpr std::size_t WorldSaveReaderNpos = std::string_view::npos;

bool WorldSaveReaderIsZoneId(std::string_view id)
{
    if (id.empty() || id.size() > WorldSaveMaxZoneIdLength || id.front() == '.' || id.back() == '.')
        return false;
    for (const char c : id)
    {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
        if (!ok)
            return false;
    }
    return true;
}

/** End offset of the JSON value starting at `start`, or npos. The document has already parsed, so this only finds extents. */
std::size_t WorldSaveReaderValueEnd(std::string_view text, std::size_t start)
{
    int depth = 0;
    bool inString = false;
    for (std::size_t pos = start; pos < text.size(); ++pos)
    {
        const char c = text[pos];
        if (inString)
        {
            if (c == '\\')
                ++pos;
            else if (c == '"')
            {
                inString = false;
                if (depth == 0)
                    return pos + 1;
            }
            continue;
        }
        if (c == '"')
            inString = true;
        else if (c == '{' || c == '[')
            ++depth;
        else if (c == '}' || c == ']')
        {
            if (depth == 0)
                return pos > start ? pos : WorldSaveReaderNpos;
            if (--depth == 0)
                return pos + 1;
        }
        else if (depth == 0 && (c == ',' || std::isspace(static_cast<unsigned char>(c)) != 0))
            return pos > start ? pos : WorldSaveReaderNpos;
    }
    return WorldSaveReaderNpos;
}

bool WorldSaveReaderClock(const JsonValue& value, WorldState& world, std::string& error)
{
    GameClockSave save;
    const JsonValue* pending = value.IsObject() ? value.Find("pending") : nullptr;
    if (!WorldSaveExactKeys(value, {"now", "nextHandle", "pending"}) || !WorldSaveAsInt(value.Find("now"), save.Now) ||
        !WorldSaveAsInt(value.Find("nextHandle"), save.NextHandle) || pending == nullptr || !pending->IsArray())
    {
        error = "the clock record is incomplete.";
        return false;
    }
    for (const JsonValue& item : pending->Array)
    {
        ScheduledEvent event;
        if (!WorldSaveExactKeys(item, {"handle", "when", "id"}) || !WorldSaveAsInt(item.Find("handle"), event.Handle) ||
            !WorldSaveAsInt(item.Find("when"), event.When) || !WorldSaveAsText(item.Find("id"), event.Id))
        {
            error = "a scheduled event is incomplete.";
            return false;
        }
        save.Pending.push_back(std::move(event));
    }
    if (!world.Clock.RestoreSave(save))
    {
        error = "the clock and its scheduled events contradict each other.";
        return false;
    }
    return true;
}

bool WorldSaveReaderFlags(const JsonValue& value, WorldState& world, std::string& error)
{
    if (!value.IsObject())
    {
        error = "the world flags are not a record.";
        return false;
    }
    for (const auto& [id, flag] : value.Object)
    {
        if (flag.Type != JsonValue::Kind::Bool)
        {
            error = "a world flag is not true or false.";
            return false;
        }
        world.Flags[id] = flag.Bool;
    }
    return true;
}

bool WorldSaveReaderGates(const JsonValue& value, WorldState& world, std::string& error)
{
    if (!value.IsObject())
    {
        error = "the Gate sites are not a record.";
        return false;
    }
    for (const auto& [id, gate] : value.Object)
    {
        GateSiteState site;
        std::string rank;
        std::string status;
        std::int64_t onsets = 0;
        if (id.empty() || !WorldSaveExactKeys(gate, {"rank", "status", "since", "onsets"}) || !WorldSaveAsText(gate.Find("rank"), rank) ||
            !WorldSaveAsText(gate.Find("status"), status) || !WorldSaveAsInt(gate.Find("since"), site.StatusSince) ||
            !WorldSaveAsIntInRange(gate.Find("onsets"), 0, INT32_MAX, onsets))
        {
            error = "a Gate site is incomplete.";
            return false;
        }
        const std::optional<GateRank> parsedRank = ParseGateRank(rank);
        const std::optional<GateStatus> parsedStatus = ParseGateStatus(status);
        if (!parsedRank || !parsedStatus)
        {
            error = "a Gate site has an unknown rank or status.";
            return false;
        }
        site.Rank = *parsedRank;
        site.Status = *parsedStatus;
        site.OnsetCount = static_cast<std::int32_t>(onsets);
        world.Gates[id] = site;
    }
    return true;
}

bool WorldSaveReaderRng(const JsonValue& value, WorldState& world, std::string& error)
{
    if (!value.IsArray())
    {
        error = "the random streams are not a list.";
        return false;
    }
    // A stream this file does not mention (one added by a later build) starts from the world seed, as a new life would.
    std::array<bool, RandomStreamCount> seen{};
    for (std::size_t index = 0; index < RandomStreamCount; ++index)
        world.Rng[index] = RandomStream(DeriveRandomStreamSeed(world.WorldSeed, static_cast<RandomStreamId>(index))).Capture();
    for (const JsonValue& item : value.Array)
    {
        std::string name;
        RandomStreamState state;
        const JsonValue* words = item.IsObject() ? item.Find("words") : nullptr;
        if (!WorldSaveExactKeys(item, {"stream", "words", "draws"}) || !WorldSaveAsText(item.Find("stream"), name) || words == nullptr ||
            !words->IsArray() || words->Array.size() != state.Words.size() || !WorldSaveAsHex(item.Find("draws"), state.Draws))
        {
            error = "a random stream is incomplete.";
            return false;
        }
        for (std::size_t word = 0; word < state.Words.size(); ++word)
        {
            if (!WorldSaveAsHex(&words->Array[word], state.Words[word]))
            {
                error = "a random stream word is not a 64-bit hex value.";
                return false;
            }
        }
        const std::optional<RandomStreamId> id = ParseRandomStreamId(name);
        if (!id)
        {
            error = "it names an unknown random stream: " + name;
            return false;
        }
        const std::size_t slot = static_cast<std::size_t>(*id);
        if (seen[slot] || !IsValidRandomStreamState(state))
        {
            error = "a random stream is repeated or has an impossible state.";
            return false;
        }
        seen[slot] = true;
        world.Rng[slot] = state;
    }
    return true;
}

/** v7: the day loop's envelope text, which must be canonical and in step with the world clock. */
bool WorldSaveReaderDayLoop(const JsonValue& value, WorldState& world, std::string& error)
{
    std::string text;
    if (!WorldSaveAsText(&value, text))
    {
        error = "the player's day is not text.";
        return false;
    }
    DayLoopParse parsed = ParseDayLoop(text);
    if (!parsed.Ok())
    {
        error = "the player's day is damaged: " + parsed.Error;
        return false;
    }
    if (parsed.State.Now != world.Clock.Now())
    {
        error = "the player's day and the world clock disagree about the time.";
        return false;
    }
    world.DayLoop = std::move(parsed.State);
    return true;
}

/** v6: no day loop was saved. A fresh day opens at the clock's instant; a clock outside the day loop's calendar cannot hold one. */
bool WorldSaveReaderFreshDay(WorldState& world, std::string& error)
{
    world.DayLoop = NewDayLoopState(world.Clock.Now());
    if (!IsWorldDayInStep(world))
    {
        error = "the world clock is outside the calendar a life can use.";
        return false;
    }
    return true;
}
} // namespace

WorldSaveResult WorldSaveFail(WorldSaveError error, std::string message)
{
    WorldSaveResult result;
    result.Error = error;
    result.Message = std::move(message);
    return result;
}

WorldSaveResult WorldSaveMalformed(const std::string& detail)
{
    return WorldSaveFail(WorldSaveError::Malformed, "This save file could not be read: " + detail);
}

WorldSaveResult WorldSaveInvalid(const std::string& detail)
{
    return WorldSaveFail(WorldSaveError::InvalidWorld, "This save file holds an impossible world: " + detail);
}

bool WorldSaveParseHex(std::string_view text, std::uint64_t& out)
{
    if (text.size() != 18 || text[0] != '0' || text[1] != 'x')
        return false;
    std::uint64_t value = 0;
    for (std::size_t index = 2; index < text.size(); ++index)
    {
        const char c = text[index];
        const int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        if (digit < 0)
            return false;
        value = (value << 4) | static_cast<std::uint64_t>(digit);
    }
    out = value;
    return true;
}

bool WorldSaveAsInt(const JsonValue* value, std::int64_t& out)
{
    if (value == nullptr || value->Type != JsonValue::Kind::Number || !value->Integer)
        return false;
    out = *value->Integer;
    return true;
}

bool WorldSaveAsIntInRange(const JsonValue* value, std::int64_t low, std::int64_t high, std::int64_t& out)
{
    return WorldSaveAsInt(value, out) && out >= low && out <= high;
}

bool WorldSaveAsHex(const JsonValue* value, std::uint64_t& out)
{
    return value != nullptr && value->IsString() && WorldSaveParseHex(value->String, out);
}

bool WorldSaveAsText(const JsonValue* value, std::string& out)
{
    if (value == nullptr || !value->IsString())
        return false;
    out = value->String;
    return true;
}

bool WorldSaveExactKeys(const JsonValue& object, std::initializer_list<std::string_view> keys)
{
    if (!object.IsObject() || object.Object.size() != keys.size())
        return false;
    for (const std::string_view key : keys)
    {
        if (object.Find(key) == nullptr)
            return false;
    }
    return true;
}

std::string_view WorldSaveCreationSpan(std::string_view text)
{
    std::size_t at = 0;
    const auto skip = [&]
    {
        while (at < text.size() && std::isspace(static_cast<unsigned char>(text[at])) != 0)
            ++at;
    };
    skip();
    if (at >= text.size() || text[at] != '{')
        return {};
    ++at;
    while (true)
    {
        skip();
        if (at >= text.size() || text[at] != '"')
            return {};
        const std::size_t keyStart = ++at;
        while (at < text.size() && text[at] != '"')
            at += text[at] == '\\' ? 2 : 1;
        if (at >= text.size())
            return {};
        const std::string_view key = text.substr(keyStart, at - keyStart);
        ++at;
        skip();
        if (at >= text.size() || text[at] != ':')
            return {};
        ++at;
        skip();
        const std::size_t end = WorldSaveReaderValueEnd(text, at);
        if (end == WorldSaveReaderNpos)
            return {};
        if (key == "creation")
            return text.substr(at, end - at);
        at = end;
        skip();
        if (at < text.size() && text[at] == ',')
        {
            ++at;
            continue;
        }
        return {};
    }
}

WorldSaveResult WorldSaveReadWorld(const JsonValue& value, WorldState& world, bool withDayLoop)
{
    std::string error;
    const JsonValue* clock = value.IsObject() ? value.Find("clock") : nullptr;
    const bool keys = withDayLoop ? WorldSaveExactKeys(value, {"seed", "seq", "clock", "flags", "gates", "rng", "dayloop"})
                                  : WorldSaveExactKeys(value, {"seed", "seq", "clock", "flags", "gates", "rng"});
    if (!keys || clock == nullptr || !WorldSaveAsHex(value.Find("seed"), world.WorldSeed) || !WorldSaveAsHex(value.Find("seq"), world.Seq))
        return WorldSaveMalformed("the world record is incomplete.");
    // The clock is read first: the day loop must agree with it.
    if (!WorldSaveReaderClock(*clock, world, error) || !WorldSaveReaderFlags(*value.Find("flags"), world, error) ||
        !WorldSaveReaderGates(*value.Find("gates"), world, error) || !WorldSaveReaderRng(*value.Find("rng"), world, error) ||
        !(withDayLoop ? WorldSaveReaderDayLoop(*value.Find("dayloop"), world, error) : WorldSaveReaderFreshDay(world, error)))
        return WorldSaveInvalid(error);
    return {};
}

WorldSaveResult WorldSaveReadPlayer(const JsonValue& value, PlayerPlacement& player)
{
    std::string camera;
    std::int64_t yaw = 0;
    std::int64_t pitch = 0;
    const JsonValue* position = value.IsObject() ? value.Find("posCm") : nullptr;
    if (!WorldSaveExactKeys(value, {"zone", "posCm", "yawCdeg", "pitchCdeg", "camera"}) || !WorldSaveAsText(value.Find("zone"), player.ZoneId) ||
        position == nullptr || !position->IsArray() || position->Array.size() != player.PositionCm.size() ||
        !WorldSaveAsText(value.Find("camera"), camera) || !WorldSaveAsInt(value.Find("yawCdeg"), yaw) ||
        !WorldSaveAsInt(value.Find("pitchCdeg"), pitch))
        return WorldSaveMalformed("the player record is incomplete.");
    if (!WorldSaveReaderIsZoneId(player.ZoneId))
        return WorldSaveInvalid("the player's zone is not a valid zone ID.");
    for (std::size_t axis = 0; axis < player.PositionCm.size(); ++axis)
    {
        std::int64_t coordinate = 0;
        if (!WorldSaveAsInt(&position->Array[axis], coordinate))
            return WorldSaveMalformed("a position coordinate is not a whole number.");
        if (coordinate < -WorldSavePositionLimitCm || coordinate > WorldSavePositionLimitCm)
            return WorldSaveInvalid("the player's position is outside the world.");
        player.PositionCm[axis] = coordinate;
    }
    if (yaw < 0 || yaw >= WorldSaveFullTurnCdeg || pitch < -WorldSaveMaxPitchCdeg || pitch > WorldSaveMaxPitchCdeg)
        return WorldSaveInvalid("the player's facing is out of range.");
    const std::optional<CameraMode> mode = ParseCameraMode(camera);
    if (!mode)
        return WorldSaveInvalid("the camera mode is unknown: " + camera);
    player.YawCdeg = static_cast<std::int32_t>(yaw);
    player.PitchCdeg = static_cast<std::int32_t>(pitch);
    player.Camera = *mode;
    return {};
}
} // namespace PerfectWorld
