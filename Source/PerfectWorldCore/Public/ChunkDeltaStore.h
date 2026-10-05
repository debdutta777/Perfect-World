#pragma once

#include "RealmCoordinate.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * The chunk delta store (WORLD_SCALE.md §4.3, ROADMAP M1-27).
 *
 * The Deep Realm is regenerated from its seed, so a save holds only what the
 * player changed. A change is a block edit inside one chunk: the block at a
 * local column and height is replaced by a block kind, where kind 0 means the
 * block was removed (air). Clearing an edit puts the generated block back, and
 * a chunk left with no edits disappears, so a chunk that was visited but not
 * changed stores nothing, and a chunk whose changes were all undone stores
 * nothing either. An empty store and an all-cleared store are equal and
 * serialise to the same bytes.
 *
 * Order is part of the contract. Chunks are ordered by floor, then chunk Y,
 * then chunk X; edits inside a chunk by local Y, then local X, then Z. Insertion
 * order never shows in the store or in the saved text.
 *
 * Each chunk also keeps the game day it last changed and a pinned flag. Setting
 * or clearing an edit that really changes something stamps the day; a call that
 * changes nothing leaves the day alone.
 *
 * THE CAP AND REVERT RULE (ApplyEditCap). Deltas have their own save budget, so
 * the store is held to an edit cap (DefaultChunkDeltaEditCap unless a caller
 * passes another). When the total number of edits in all chunks exceeds the cap,
 * whole chunks revert, never single edits, in this order:
 *   1. Only unpinned chunks are candidates. A pinned chunk never reverts, even
 *      if that leaves the store over the cap.
 *   2. Candidates go in ascending order of last-changed day (least recently
 *      changed first). Equal days are broken by floor, then chunk Y, then chunk X,
 *      all ascending.
 *   3. Reverting stops at the first chunk after which the total is at or under
 *      the cap. Chunks are never skipped to find a closer fit, so the result is
 *      the shortest prefix of the order above that meets the cap.
 *   4. A negative cap counts as 0.
 * The report names every reverted chunk in revert order, the number of edits
 * dropped and left, and CapMet (false only when pinned chunks alone exceed the
 * cap). A store at or under the cap is left untouched. The cap is never applied
 * implicitly: SetBlock does not revert anything, and ParseChunkDeltas accepts a
 * document over the cap, so that the caller decides when the rule runs.
 *
 * Saved form: "pw.chunk_deltas/1", one compact JSON object with no trailing
 * whitespace, holding explicit chunk and edit counts, the chunks, and an end
 * marker as the last member. Each chunk lists its edits as [x, y, z, kind].
 * The reader is strict: exact member sets, exact counts, whole-number ranges,
 * chunks and edits in canonical order with no repeats, the end marker, and no
 * whitespace before or after the document. Every proper prefix of a valid
 * document is refused. It parses with the strict JSON reader, so whitespace
 * between tokens is tolerated; the store it yields always re-serialises to the
 * canonical text, and the writer's own output reads back byte-identically.
 */

/** Local block coordinates inside a chunk are 0..31 on each horizontal axis. */
constexpr std::int32_t ChunkDeltaBlocksPerEdge = 32;
/** Vertical block coordinate bound: elevation is an int32 of millimetres, so about +-2,147,483 blocks. */
constexpr std::int32_t ChunkDeltaMaxAbsZ = 2'000'000;
/** Block kind 0 is air: the generated block was removed. */
constexpr std::uint16_t ChunkDeltaAirKind = 0;
/** Default edit cap across all chunks (the delta share of the save budget). */
constexpr std::int64_t DefaultChunkDeltaEditCap = 65'536;
constexpr std::string_view ChunkDeltaFormatName = "pw.chunk_deltas/1";
constexpr std::string_view ChunkDeltaEndMarker = "pw.chunk_deltas.end";

struct BlockEdit
{
    std::int32_t X = 0;
    std::int32_t Y = 0;
    std::int32_t Z = 0;
    std::uint16_t Kind = ChunkDeltaAirKind;

    friend bool operator==(const BlockEdit&, const BlockEdit&) = default;
};

struct ChunkDelta
{
    /** Sorted by Y, then X, then Z; at most one edit per block. Never empty inside a store. */
    std::vector<BlockEdit> Edits;
    std::int32_t LastChangedDay = 0;
    bool Pinned = false;

    friend bool operator==(const ChunkDelta&, const ChunkDelta&) = default;
};

/** Chunk order: floor, then Y, then X. A strict total order. */
struct ChunkDeltaChunkLess
{
    bool operator()(const RealmChunk& a, const RealmChunk& b) const
    {
        if (a.Floor != b.Floor)
        {
            return a.Floor < b.Floor;
        }
        return a.Y != b.Y ? a.Y < b.Y : a.X < b.X;
    }
};

/** Edit order inside a chunk: Y, then X, then Z. A strict total order. */
inline bool ChunkDeltaEditLess(const BlockEdit& a, const BlockEdit& b)
{
    if (a.Y != b.Y)
    {
        return a.Y < b.Y;
    }
    return a.X != b.X ? a.X < b.X : a.Z < b.Z;
}

/** True when the chunk is on its floor, x and y are 0..31 and z is within +-ChunkDeltaMaxAbsZ. */
PERFECTWORLDCORE_API bool IsValidChunkDeltaBlock(const RealmChunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z);

enum class ChunkDeltaEditResult : std::uint8_t
{
    /** The store changed. */
    Changed = 0,
    /** Valid, but nothing needed to change. */
    Unchanged,
    /** Off its floor, out of range, or a negative day. The store is untouched. */
    Refused
};

struct ChunkDeltaCapReport
{
    /** In the order they reverted. */
    std::vector<RealmChunk> RevertedChunks;
    std::int64_t EditsReverted = 0;
    std::int64_t EditsRemaining = 0;
    /** True when EditsRemaining is at or under the cap. */
    bool CapMet = true;
};

class PERFECTWORLDCORE_API ChunkDeltaStore
{
public:
    using ChunkMap = std::map<RealmChunk, ChunkDelta, ChunkDeltaChunkLess>;

    /** Replaces the block at (x, y, z) in the chunk with kind (0 = removed). Stamps the chunk's last-changed day on a change. */
    ChunkDeltaEditResult SetBlock(
        const RealmChunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z, std::uint16_t kind, std::int32_t day);
    /** Restores the generated block. A chunk left with no edits is removed, pin and all. */
    ChunkDeltaEditResult ClearBlock(const RealmChunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t day);
    /** Pins or unpins a chunk that has edits. False when the chunk has none (a pin cannot outlive its edits). */
    bool SetPinned(const RealmChunk& chunk, bool pinned);
    /** Drops every edit in one chunk. False when the chunk had none. */
    bool RevertChunk(const RealmChunk& chunk);

    /** The edit at that block, or nullopt when the generated block stands. */
    std::optional<std::uint16_t> EditedBlock(const RealmChunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) const;
    const ChunkDelta* Find(const RealmChunk& chunk) const;

    bool Empty() const { return Chunks.empty(); }
    std::size_t ChunkCount() const { return Chunks.size(); }
    std::int64_t EditCount() const { return TotalEdits; }
    const ChunkMap& AllChunks() const { return Chunks; }

    /** Applies the cap and revert rule documented at the top of this header. */
    ChunkDeltaCapReport ApplyEditCap(std::int64_t cap);

    /**
     * Inserts one whole chunk, as the reader does. Refuses (false) an invalid chunk, a chunk
     * already present, an empty edit list, a negative day, or edits that are out of range, unsorted
     * or duplicated.
     */
    bool LoadChunk(const RealmChunk& chunk, ChunkDelta delta);

    friend bool operator==(const ChunkDeltaStore& a, const ChunkDeltaStore& b) { return a.Chunks == b.Chunks; }

private:
    ChunkMap Chunks;
    std::int64_t TotalEdits = 0;
};

struct ChunkDeltaParseResult
{
    std::optional<ChunkDeltaStore> Store;
    /** Empty on success; otherwise says why the document was refused. */
    std::string Error;
};

/** The canonical text of the store. Deterministic; has no trailing newline. */
PERFECTWORLDCORE_API std::string SerializeChunkDeltas(const ChunkDeltaStore& store);
/** Strict reader: refuses anything but the canonical form of a version 1 document. */
PERFECTWORLDCORE_API ChunkDeltaParseResult ParseChunkDeltas(std::string_view text);
} // namespace PerfectWorld
