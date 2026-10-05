#include "ChunkDeltaStore.h"

#include "Json.h"

#include <algorithm>
#include <initializer_list>
#include <limits>
#include <utility>

namespace PerfectWorld
{
namespace
{
bool ChunkDeltaEditInRange(const BlockEdit& edit)
{
    return edit.X >= 0 && edit.X < ChunkDeltaBlocksPerEdge && edit.Y >= 0 && edit.Y < ChunkDeltaBlocksPerEdge &&
           edit.Z >= -ChunkDeltaMaxAbsZ && edit.Z <= ChunkDeltaMaxAbsZ;
}

bool ChunkDeltaSamePlace(const BlockEdit& a, const BlockEdit& b)
{
    return a.X == b.X && a.Y == b.Y && a.Z == b.Z;
}

std::vector<BlockEdit>::iterator ChunkDeltaLowerBound(std::vector<BlockEdit>& edits, const BlockEdit& probe)
{
    return std::lower_bound(edits.begin(), edits.end(), probe, ChunkDeltaEditLess);
}

std::vector<BlockEdit>::const_iterator ChunkDeltaLowerBound(const std::vector<BlockEdit>& edits, const BlockEdit& probe)
{
    return std::lower_bound(edits.begin(), edits.end(), probe, ChunkDeltaEditLess);
}

// ---- Reader helpers --------------------------------------------------------

bool ChunkDeltaFail(std::string& error, std::string message)
{
    error = std::move(message);
    return false;
}

bool ChunkDeltaAsInteger(const JsonValue& value, std::int64_t minimum, std::int64_t maximum, std::int64_t& out)
{
    if (value.Type != JsonValue::Kind::Number || !value.Integer.has_value())
    {
        return false;
    }
    if (*value.Integer < minimum || *value.Integer > maximum)
    {
        return false;
    }
    out = *value.Integer;
    return true;
}

/** The object must have exactly these members: no more, no fewer (duplicates are the parser's job). */
bool ChunkDeltaExpectKeys(const JsonValue& object, std::initializer_list<std::string_view> keys, const std::string& where,
                          std::string& error)
{
    if (!object.IsObject())
    {
        return ChunkDeltaFail(error, where + " must be an object");
    }
    if (object.Object.size() != keys.size())
    {
        return ChunkDeltaFail(error, where + " must have exactly its documented members");
    }
    for (const std::string_view key : keys)
    {
        if (object.Find(key) == nullptr)
        {
            return ChunkDeltaFail(error, where + " is missing \"" + std::string(key) + "\"");
        }
    }
    return true;
}

bool ChunkDeltaReadInteger(const JsonValue& object, std::string_view key, std::int64_t minimum, std::int64_t maximum,
                           const std::string& where, std::int64_t& out, std::string& error)
{
    const JsonValue* value = object.Find(key);
    if (value == nullptr || !ChunkDeltaAsInteger(*value, minimum, maximum, out))
    {
        return ChunkDeltaFail(error, where + ": \"" + std::string(key) + "\" must be a whole number from " +
                                         std::to_string(minimum) + " to " + std::to_string(maximum));
    }
    return true;
}

bool ChunkDeltaReadEdit(const JsonValue& value, const std::string& where, BlockEdit& edit, std::string& error)
{
    if (!value.IsArray() || value.Array.size() != 4)
    {
        return ChunkDeltaFail(error, where + " must be [x, y, z, kind]");
    }
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t z = 0;
    std::int64_t kind = 0;
    if (!ChunkDeltaAsInteger(value.Array[0], 0, ChunkDeltaBlocksPerEdge - 1, x) ||
        !ChunkDeltaAsInteger(value.Array[1], 0, ChunkDeltaBlocksPerEdge - 1, y))
    {
        return ChunkDeltaFail(error, where + ": local x and y must be whole numbers from 0 to 31");
    }
    if (!ChunkDeltaAsInteger(value.Array[2], -ChunkDeltaMaxAbsZ, ChunkDeltaMaxAbsZ, z))
    {
        return ChunkDeltaFail(error, where + ": z is out of range");
    }
    if (!ChunkDeltaAsInteger(value.Array[3], 0, std::numeric_limits<std::uint16_t>::max(), kind))
    {
        return ChunkDeltaFail(error, where + ": block kind must be a whole number from 0 to 65535");
    }
    edit.X = static_cast<std::int32_t>(x);
    edit.Y = static_cast<std::int32_t>(y);
    edit.Z = static_cast<std::int32_t>(z);
    edit.Kind = static_cast<std::uint16_t>(kind);
    return true;
}

bool ChunkDeltaReadChunk(const JsonValue& value, std::size_t index, RealmChunk& chunk, ChunkDelta& delta, std::string& error)
{
    const std::string where = "chunk " + std::to_string(index);
    if (!ChunkDeltaExpectKeys(value, {"floor", "x", "y", "lastChangedDay", "pinned", "editCount", "edits"}, where, error))
    {
        return false;
    }
    std::int64_t floor = 0;
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t day = 0;
    std::int64_t editCount = 0;
    constexpr std::int64_t coordinateLimit = std::numeric_limits<std::int64_t>::max();
    if (!ChunkDeltaReadInteger(value, "floor", 1, RealmFloorCount, where, floor, error) ||
        !ChunkDeltaReadInteger(value, "x", 0, coordinateLimit, where, x, error) ||
        !ChunkDeltaReadInteger(value, "y", 0, coordinateLimit, where, y, error) ||
        !ChunkDeltaReadInteger(value, "lastChangedDay", 0, std::numeric_limits<std::int32_t>::max(), where, day, error) ||
        !ChunkDeltaReadInteger(value, "editCount", 1, std::numeric_limits<std::int32_t>::max(), where, editCount, error))
    {
        return false;
    }
    chunk = RealmChunk{static_cast<std::int32_t>(floor), x, y};
    if (!IsValidRealmChunk(chunk))
    {
        return ChunkDeltaFail(error, where + " is off its floor");
    }
    const JsonValue* pinned = value.Find("pinned");
    if (pinned->Type != JsonValue::Kind::Bool)
    {
        return ChunkDeltaFail(error, where + ": \"pinned\" must be true or false");
    }
    const JsonValue* edits = value.Find("edits");
    if (!edits->IsArray() || edits->Array.size() != static_cast<std::size_t>(editCount))
    {
        return ChunkDeltaFail(error, where + ": \"edits\" must be an array of exactly editCount entries");
    }
    delta.LastChangedDay = static_cast<std::int32_t>(day);
    delta.Pinned = pinned->Bool;
    delta.Edits.clear();
    delta.Edits.reserve(edits->Array.size());
    for (std::size_t i = 0; i < edits->Array.size(); ++i)
    {
        BlockEdit edit;
        if (!ChunkDeltaReadEdit(edits->Array[i], where + " edit " + std::to_string(i), edit, error))
        {
            return false;
        }
        delta.Edits.push_back(edit);
    }
    return true;
}
} // namespace

bool IsValidChunkDeltaBlock(const RealmChunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z)
{
    return IsValidRealmChunk(chunk) && ChunkDeltaEditInRange(BlockEdit{x, y, z, ChunkDeltaAirKind});
}

ChunkDeltaEditResult ChunkDeltaStore::SetBlock(
    const RealmChunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z, std::uint16_t kind, std::int32_t day)
{
    if (!IsValidChunkDeltaBlock(chunk, x, y, z) || day < 0)
    {
        return ChunkDeltaEditResult::Refused;
    }
    const BlockEdit edit{x, y, z, kind};
    const auto found = Chunks.find(chunk);
    if (found == Chunks.end())
    {
        ChunkDelta delta;
        delta.Edits.push_back(edit);
        delta.LastChangedDay = day;
        Chunks.emplace(chunk, std::move(delta));
        ++TotalEdits;
        return ChunkDeltaEditResult::Changed;
    }
    ChunkDelta& delta = found->second;
    const auto at = ChunkDeltaLowerBound(delta.Edits, edit);
    if (at != delta.Edits.end() && ChunkDeltaSamePlace(*at, edit))
    {
        if (at->Kind == kind)
        {
            return ChunkDeltaEditResult::Unchanged;
        }
        at->Kind = kind;
    }
    else
    {
        delta.Edits.insert(at, edit);
        ++TotalEdits;
    }
    delta.LastChangedDay = day;
    return ChunkDeltaEditResult::Changed;
}

ChunkDeltaEditResult ChunkDeltaStore::ClearBlock(
    const RealmChunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t day)
{
    if (!IsValidChunkDeltaBlock(chunk, x, y, z) || day < 0)
    {
        return ChunkDeltaEditResult::Refused;
    }
    const auto found = Chunks.find(chunk);
    if (found == Chunks.end())
    {
        return ChunkDeltaEditResult::Unchanged;
    }
    ChunkDelta& delta = found->second;
    const BlockEdit probe{x, y, z, ChunkDeltaAirKind};
    const auto at = ChunkDeltaLowerBound(delta.Edits, probe);
    if (at == delta.Edits.end() || !ChunkDeltaSamePlace(*at, probe))
    {
        return ChunkDeltaEditResult::Unchanged;
    }
    delta.Edits.erase(at);
    --TotalEdits;
    if (delta.Edits.empty())
    {
        Chunks.erase(found);
    }
    else
    {
        delta.LastChangedDay = day;
    }
    return ChunkDeltaEditResult::Changed;
}

bool ChunkDeltaStore::SetPinned(const RealmChunk& chunk, bool pinned)
{
    const auto found = Chunks.find(chunk);
    if (found == Chunks.end())
    {
        return false;
    }
    found->second.Pinned = pinned;
    return true;
}

bool ChunkDeltaStore::RevertChunk(const RealmChunk& chunk)
{
    const auto found = Chunks.find(chunk);
    if (found == Chunks.end())
    {
        return false;
    }
    TotalEdits -= static_cast<std::int64_t>(found->second.Edits.size());
    Chunks.erase(found);
    return true;
}

std::optional<std::uint16_t> ChunkDeltaStore::EditedBlock(
    const RealmChunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) const
{
    const ChunkDelta* delta = Find(chunk);
    if (delta == nullptr)
    {
        return std::nullopt;
    }
    const BlockEdit probe{x, y, z, ChunkDeltaAirKind};
    const auto at = ChunkDeltaLowerBound(delta->Edits, probe);
    if (at == delta->Edits.end() || !ChunkDeltaSamePlace(*at, probe))
    {
        return std::nullopt;
    }
    return at->Kind;
}

const ChunkDelta* ChunkDeltaStore::Find(const RealmChunk& chunk) const
{
    const auto found = Chunks.find(chunk);
    return found == Chunks.end() ? nullptr : &found->second;
}

ChunkDeltaCapReport ChunkDeltaStore::ApplyEditCap(std::int64_t cap)
{
    ChunkDeltaCapReport report;
    const std::int64_t limit = cap < 0 ? 0 : cap;
    if (TotalEdits > limit)
    {
        struct Candidate
        {
            std::int32_t Day;
            RealmChunk Chunk;
        };
        std::vector<Candidate> candidates;
        for (const auto& [chunk, delta] : Chunks)
        {
            if (!delta.Pinned)
            {
                candidates.push_back(Candidate{delta.LastChangedDay, chunk});
            }
        }
        // Chunk keys are unique, so this is a strict total order and the sort is deterministic.
        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            return a.Day != b.Day ? a.Day < b.Day : ChunkDeltaChunkLess{}(a.Chunk, b.Chunk);
        });
        for (const Candidate& candidate : candidates)
        {
            if (TotalEdits <= limit)
            {
                break;
            }
            const std::int64_t before = TotalEdits;
            RevertChunk(candidate.Chunk);
            report.EditsReverted += before - TotalEdits;
            report.RevertedChunks.push_back(candidate.Chunk);
        }
    }
    report.EditsRemaining = TotalEdits;
    report.CapMet = TotalEdits <= limit;
    return report;
}

bool ChunkDeltaStore::LoadChunk(const RealmChunk& chunk, ChunkDelta delta)
{
    if (!IsValidRealmChunk(chunk) || Chunks.contains(chunk) || delta.Edits.empty() || delta.LastChangedDay < 0)
    {
        return false;
    }
    for (std::size_t i = 0; i < delta.Edits.size(); ++i)
    {
        if (!ChunkDeltaEditInRange(delta.Edits[i]) || (i > 0 && !ChunkDeltaEditLess(delta.Edits[i - 1], delta.Edits[i])))
        {
            return false;
        }
    }
    TotalEdits += static_cast<std::int64_t>(delta.Edits.size());
    Chunks.emplace(chunk, std::move(delta));
    return true;
}

std::string SerializeChunkDeltas(const ChunkDeltaStore& store)
{
    std::string text;
    text += "{\"format\":\"";
    text += ChunkDeltaFormatName;
    text += "\",\"chunkCount\":" + std::to_string(store.ChunkCount());
    text += ",\"editCount\":" + std::to_string(store.EditCount());
    text += ",\"chunks\":[";
    bool firstChunk = true;
    for (const auto& [chunk, delta] : store.AllChunks())
    {
        text += firstChunk ? "{" : ",{";
        firstChunk = false;
        text += "\"floor\":" + std::to_string(chunk.Floor);
        text += ",\"x\":" + std::to_string(chunk.X);
        text += ",\"y\":" + std::to_string(chunk.Y);
        text += ",\"lastChangedDay\":" + std::to_string(delta.LastChangedDay);
        text += delta.Pinned ? ",\"pinned\":true" : ",\"pinned\":false";
        text += ",\"editCount\":" + std::to_string(delta.Edits.size());
        text += ",\"edits\":[";
        bool firstEdit = true;
        for (const BlockEdit& edit : delta.Edits)
        {
            text += firstEdit ? "[" : ",[";
            firstEdit = false;
            text += std::to_string(edit.X) + "," + std::to_string(edit.Y) + "," + std::to_string(edit.Z) + "," +
                    std::to_string(edit.Kind) + "]";
        }
        text += "]}";
    }
    text += "],\"end\":\"";
    text += ChunkDeltaEndMarker;
    text += "\"}";
    return text;
}

ChunkDeltaParseResult ParseChunkDeltas(std::string_view text)
{
    ChunkDeltaParseResult result;
    std::string& error = result.Error;

    // The canonical text has no whitespace around it, so none is accepted.
    if (text.empty() || text.front() != '{' || text.back() != '}')
    {
        error = "not a chunk delta document: it must begin with '{' and end with '}' with no whitespace around it";
        return result;
    }
    const JsonParseResult parsed = ParseJson(text);
    if (!parsed.Value.has_value())
    {
        error = "not valid JSON: " + parsed.Error;
        return result;
    }
    const JsonValue& root = *parsed.Value;
    if (!ChunkDeltaExpectKeys(root, {"format", "chunkCount", "editCount", "chunks", "end"}, "the document", error))
    {
        return result;
    }
    const JsonValue* format = root.Find("format");
    if (!format->IsString() || format->String != ChunkDeltaFormatName)
    {
        error = "unsupported format or version; expected \"" + std::string(ChunkDeltaFormatName) + "\"";
        return result;
    }
    const JsonValue* end = root.Find("end");
    if (!end->IsString() || end->String != ChunkDeltaEndMarker)
    {
        error = "missing or wrong end marker";
        return result;
    }
    std::int64_t chunkCount = 0;
    std::int64_t editCount = 0;
    constexpr std::int64_t countLimit = std::numeric_limits<std::int32_t>::max();
    if (!ChunkDeltaReadInteger(root, "chunkCount", 0, countLimit, "the document", chunkCount, error) ||
        !ChunkDeltaReadInteger(root, "editCount", 0, countLimit, "the document", editCount, error))
    {
        return result;
    }
    const JsonValue* chunks = root.Find("chunks");
    if (!chunks->IsArray() || chunks->Array.size() != static_cast<std::size_t>(chunkCount))
    {
        error = "\"chunks\" must be an array of exactly chunkCount entries";
        return result;
    }

    ChunkDeltaStore store;
    RealmChunk previous;
    for (std::size_t i = 0; i < chunks->Array.size(); ++i)
    {
        RealmChunk chunk;
        ChunkDelta delta;
        if (!ChunkDeltaReadChunk(chunks->Array[i], i, chunk, delta, error))
        {
            return result;
        }
        if (i > 0 && !ChunkDeltaChunkLess{}(previous, chunk))
        {
            error = "chunk " + std::to_string(i) + " is out of order or repeated (order is floor, y, x)";
            return result;
        }
        previous = chunk;
        if (!store.LoadChunk(chunk, std::move(delta)))
        {
            error = "chunk " + std::to_string(i) + ": edits are out of order or repeated (order is y, x, z)";
            return result;
        }
    }
    if (store.EditCount() != editCount)
    {
        error = "editCount does not match the edits in the document";
        return result;
    }
    result.Store = std::move(store);
    return result;
}
} // namespace PerfectWorld
