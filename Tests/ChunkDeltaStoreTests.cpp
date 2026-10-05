#include "ChunkDeltaStore.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace PerfectWorld;

int Checks = 0;
void Check(bool condition, const char* message)
{
    ++Checks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

namespace
{
const RealmChunk ChunkA{1, 4, 2};
const RealmChunk ChunkB{2, 0, 0};

/** The hand-written canonical text of the store built by BuildSmallStore. */
const std::string SmallDocument =
    "{\"format\":\"pw.chunk_deltas/1\",\"chunkCount\":2,\"editCount\":3,\"chunks\":["
    "{\"floor\":1,\"x\":4,\"y\":2,\"lastChangedDay\":9,\"pinned\":false,\"editCount\":2,\"edits\":[[1,0,-3,0],[0,1,5,17]]},"
    "{\"floor\":2,\"x\":0,\"y\":0,\"lastChangedDay\":3,\"pinned\":true,\"editCount\":1,\"edits\":[[31,31,0,65535]]}"
    "],\"end\":\"pw.chunk_deltas.end\"}";

ChunkDeltaStore BuildSmallStore()
{
    ChunkDeltaStore store;
    // Deliberately not in sorted order: order of insertion must not show.
    Check(store.SetBlock(ChunkB, 31, 31, 0, 65535, 3) == ChunkDeltaEditResult::Changed, "set in chunk B");
    Check(store.SetBlock(ChunkA, 0, 1, 5, 17, 9) == ChunkDeltaEditResult::Changed, "set in chunk A");
    Check(store.SetBlock(ChunkA, 1, 0, -3, ChunkDeltaAirKind, 9) == ChunkDeltaEditResult::Changed, "remove a block in chunk A");
    Check(store.SetPinned(ChunkB, true), "pin chunk B");
    return store;
}

bool Refused(const std::string& text)
{
    const ChunkDeltaParseResult result = ParseChunkDeltas(text);
    return !result.Store.has_value() && !result.Error.empty();
}

/** A copy of the small document with one exact fragment swapped; the fragment must be there. */
std::string Mutated(const std::string& from, const std::string& to)
{
    std::string text = SmallDocument;
    const std::size_t at = text.find(from);
    Check(at != std::string::npos, "mutation target exists in the small document");
    text.replace(at, from.size(), to);
    return text;
}

/** Small deterministic integer generator for building scrambled stores. */
struct TestRandom
{
    std::uint64_t State = 0x0DE17A5ULL;
    std::uint32_t Next(std::uint32_t bound)
    {
        State = State * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<std::uint32_t>((State >> 33) % bound);
    }
};

/** Puts `count` edits in a chunk, all stamped with `day`. */
void Fill(ChunkDeltaStore& store, const RealmChunk& chunk, int count, std::int32_t day)
{
    for (int i = 0; i < count; ++i)
    {
        Check(store.SetBlock(chunk, i % 32, i / 32, 0, 1, day) == ChunkDeltaEditResult::Changed, "fill edit");
    }
}
} // namespace

int main()
{
    // The documented text format, byte for byte.
    {
        const ChunkDeltaStore store = BuildSmallStore();
        Check(SerializeChunkDeltas(store) == SmallDocument, "the store serialises to the documented canonical text");
        Check(store.ChunkCount() == 2 && store.EditCount() == 3, "counts are chunks and edits");
        Check(*store.EditedBlock(ChunkA, 1, 0, -3) == ChunkDeltaAirKind, "a removed block is an edit of kind 0");
        Check(*store.EditedBlock(ChunkA, 0, 1, 5) == 17, "an edited block reads back");
        Check(!store.EditedBlock(ChunkA, 0, 1, 6).has_value(), "an unedited block is the generated one");
        Check(!store.EditedBlock(RealmChunk{1, 9, 9}, 0, 0, 0).has_value(), "an untouched chunk has no edits");
        Check(store.Find(ChunkB)->Pinned && !store.Find(ChunkA)->Pinned, "pins are per chunk");
        Check(store.Find(ChunkA)->LastChangedDay == 9, "the last-changed day is kept");

        const ChunkDeltaParseResult parsed = ParseChunkDeltas(SmallDocument);
        Check(parsed.Store.has_value() && parsed.Error.empty(), "the documented text parses");
        Check(*parsed.Store == store, "parse gives back the same store");
        Check(SerializeChunkDeltas(*parsed.Store) == SmallDocument, "round trip is byte-identical");
    }

    // Set replaces, clear restores, an unchanged call changes nothing, and the day follows real changes only.
    {
        ChunkDeltaStore store;
        Check(store.SetBlock(ChunkA, 5, 6, 7, 3, 10) == ChunkDeltaEditResult::Changed, "first set");
        Check(store.SetBlock(ChunkA, 5, 6, 7, 3, 20) == ChunkDeltaEditResult::Unchanged, "same set again is unchanged");
        Check(store.Find(ChunkA)->LastChangedDay == 10, "an unchanged set does not stamp the day");
        Check(store.SetBlock(ChunkA, 5, 6, 7, 8, 30) == ChunkDeltaEditResult::Changed, "set replaces the kind");
        Check(store.EditCount() == 1 && *store.EditedBlock(ChunkA, 5, 6, 7) == 8, "replacing does not add an edit");
        Check(store.Find(ChunkA)->LastChangedDay == 30, "a real change stamps the day");
        Check(store.ClearBlock(ChunkA, 4, 6, 7, 31) == ChunkDeltaEditResult::Unchanged, "clearing an unedited block is unchanged");
        Check(store.Find(ChunkA)->LastChangedDay == 30, "an unchanged clear does not stamp the day");
        Check(store.ClearBlock(RealmChunk{1, 77, 77}, 0, 0, 0, 40) == ChunkDeltaEditResult::Unchanged, "clearing in an untouched chunk");
        Check(store.ChunkCount() == 1, "clearing in an untouched chunk creates nothing");
        Check(store.ClearBlock(ChunkA, 5, 6, 7, 32) == ChunkDeltaEditResult::Changed, "clear restores the generated block");
        Check(!store.EditedBlock(ChunkA, 5, 6, 7).has_value(), "the block is generated again");
        Check(store.Empty() && store.ChunkCount() == 0 && store.EditCount() == 0, "a chunk with no edits disappears");
        Check(!store.SetPinned(ChunkA, true), "a chunk with no edits cannot be pinned");
        Check(!store.RevertChunk(ChunkA), "reverting an absent chunk reports false");
    }

    // An empty store equals an all-cleared store, and serialises to the same bytes.
    {
        const ChunkDeltaStore empty;
        ChunkDeltaStore cleared;
        TestRandom random;
        std::vector<BlockEdit> placed;
        for (int i = 0; i < 400; ++i)
        {
            const RealmChunk chunk{1 + static_cast<std::int32_t>(random.Next(3)), random.Next(5), random.Next(5)};
            const BlockEdit edit{static_cast<std::int32_t>(random.Next(32)), static_cast<std::int32_t>(random.Next(32)),
                                 static_cast<std::int32_t>(random.Next(11)) - 5, static_cast<std::uint16_t>(random.Next(9))};
            if (cleared.SetBlock(chunk, edit.X, edit.Y, edit.Z, edit.Kind, 1 + i) != ChunkDeltaEditResult::Refused)
            {
                placed.push_back(edit);
            }
        }
        Check(!cleared.Empty(), "the scrambled store has content");
        // Clear every block of every chunk it could be in; unrelated clears must be harmless.
        for (std::int32_t floor = 1; floor <= 3; ++floor)
        {
            for (std::int64_t y = 0; y < 5; ++y)
            {
                for (std::int64_t x = 0; x < 5; ++x)
                {
                    for (const BlockEdit& edit : placed)
                    {
                        cleared.ClearBlock(RealmChunk{floor, x, y}, edit.X, edit.Y, edit.Z, 9'999);
                    }
                }
            }
        }
        Check(cleared.Empty() && cleared.EditCount() == 0, "every edit cleared leaves nothing");
        Check(cleared == empty, "an all-cleared store equals an empty store");
        Check(SerializeChunkDeltas(cleared) == SerializeChunkDeltas(empty), "and serialises to the same bytes");
        const ChunkDeltaParseResult parsed = ParseChunkDeltas(SerializeChunkDeltas(empty));
        Check(parsed.Store.has_value() && parsed.Store->Empty(), "an empty store round trips");
        Check(SerializeChunkDeltas(*parsed.Store) == SerializeChunkDeltas(empty), "the empty document is byte-identical");
        // Every prefix of the empty document is refused too.
        const std::string document = SerializeChunkDeltas(empty);
        for (std::size_t length = 0; length < document.size(); ++length)
        {
            Check(Refused(document.substr(0, length)), "a prefix of the empty document is refused");
        }
    }

    // Refusals at the edges of the valid range.
    {
        ChunkDeltaStore store;
        const RealmChunk last{12, RealmFloorChunksPerEdge(12) - 1, RealmFloorChunksPerEdge(12) - 1};
        Check(store.SetBlock(last, 31, 31, ChunkDeltaMaxAbsZ, 1, 0) == ChunkDeltaEditResult::Changed, "the far corner chunk, top block");
        Check(store.SetBlock(last, 0, 0, -ChunkDeltaMaxAbsZ, 1, 0) == ChunkDeltaEditResult::Changed, "the deepest block");
        const std::int64_t beyond = RealmFloorChunksPerEdge(12);
        const std::int64_t before = store.EditCount();
        Check(store.SetBlock(RealmChunk{12, beyond, 0}, 0, 0, 0, 1, 0) == ChunkDeltaEditResult::Refused, "chunk past the floor's east edge");
        Check(store.SetBlock(RealmChunk{12, 0, beyond}, 0, 0, 0, 1, 0) == ChunkDeltaEditResult::Refused, "chunk past the floor's north edge");
        Check(store.SetBlock(RealmChunk{12, -1, 0}, 0, 0, 0, 1, 0) == ChunkDeltaEditResult::Refused, "chunk before the floor");
        Check(store.SetBlock(RealmChunk{0, 0, 0}, 0, 0, 0, 1, 0) == ChunkDeltaEditResult::Refused, "floor 0");
        Check(store.SetBlock(RealmChunk{RealmFloorCount + 1, 0, 0}, 0, 0, 0, 1, 0) == ChunkDeltaEditResult::Refused, "floor past the stack");
        Check(store.SetBlock(last, 32, 0, 0, 1, 0) == ChunkDeltaEditResult::Refused, "local x 32");
        Check(store.SetBlock(last, -1, 0, 0, 1, 0) == ChunkDeltaEditResult::Refused, "local x -1");
        Check(store.SetBlock(last, 0, 32, 0, 1, 0) == ChunkDeltaEditResult::Refused, "local y 32");
        Check(store.SetBlock(last, 0, -1, 0, 1, 0) == ChunkDeltaEditResult::Refused, "local y -1");
        Check(store.SetBlock(last, 0, 0, ChunkDeltaMaxAbsZ + 1, 1, 0) == ChunkDeltaEditResult::Refused, "z too high");
        Check(store.SetBlock(last, 0, 0, -ChunkDeltaMaxAbsZ - 1, 1, 0) == ChunkDeltaEditResult::Refused, "z too low");
        Check(store.SetBlock(last, 1, 1, 1, 1, -1) == ChunkDeltaEditResult::Refused, "negative day");
        Check(store.ClearBlock(last, 32, 0, 0, 0) == ChunkDeltaEditResult::Refused, "clear refuses a bad block too");
        Check(store.ClearBlock(RealmChunk{12, beyond, 0}, 0, 0, 0, 0) == ChunkDeltaEditResult::Refused, "clear refuses a bad chunk too");
        Check(store.EditCount() == before, "refusals change nothing");
        const ChunkDeltaParseResult parsed = ParseChunkDeltas(SerializeChunkDeltas(store));
        Check(parsed.Store.has_value() && *parsed.Store == store, "the extreme edits round trip");
    }

    // Insertion order never shows, and a larger scrambled store round trips byte for byte.
    {
        TestRandom random;
        std::vector<std::pair<RealmChunk, BlockEdit>> operations;
        for (int i = 0; i < 600; ++i)
        {
            const RealmChunk chunk{1 + static_cast<std::int32_t>(random.Next(12)), random.Next(1'000), random.Next(1'000)};
            const BlockEdit edit{static_cast<std::int32_t>(random.Next(32)), static_cast<std::int32_t>(random.Next(32)),
                                 static_cast<std::int32_t>(random.Next(2'001)) - 1'000, static_cast<std::uint16_t>(random.Next(65'536))};
            operations.emplace_back(chunk, edit);
        }
        ChunkDeltaStore forward;
        ChunkDeltaStore backward;
        for (std::size_t i = 0; i < operations.size(); ++i)
        {
            const auto& [chunk, edit] = operations[i];
            forward.SetBlock(chunk, edit.X, edit.Y, edit.Z, edit.Kind, 50);
            const auto& [chunk2, edit2] = operations[operations.size() - 1 - i];
            backward.SetBlock(chunk2, edit2.X, edit2.Y, edit2.Z, edit2.Kind, 50);
        }
        // Where two operations hit one block, the later one wins, so the two orders can differ there.
        // Rebuild backward from the final values to compare like with like.
        ChunkDeltaStore rebuilt;
        for (const auto& [chunk, delta] : forward.AllChunks())
        {
            for (std::size_t i = delta.Edits.size(); i > 0; --i)
            {
                const BlockEdit& edit = delta.Edits[i - 1];
                rebuilt.SetBlock(chunk, edit.X, edit.Y, edit.Z, edit.Kind, 50);
            }
        }
        Check(rebuilt == forward, "insertion order does not show in the store");
        Check(SerializeChunkDeltas(rebuilt) == SerializeChunkDeltas(forward), "nor in the saved text");
        Check(forward.EditCount() > 500, "the scrambled store is substantial");
        const std::string text = SerializeChunkDeltas(forward);
        const ChunkDeltaParseResult parsed = ParseChunkDeltas(text);
        Check(parsed.Store.has_value(), "the scrambled store parses");
        Check(*parsed.Store == forward && parsed.Store->EditCount() == forward.EditCount(), "and equals the original");
        Check(SerializeChunkDeltas(*parsed.Store) == text, "and re-serialises byte-identically");
        Check(text.find('\n') == std::string::npos && text.back() == '}', "the text has no trailing whitespace");
    }

    // Every proper prefix of a valid document is refused.
    {
        for (std::size_t length = 0; length < SmallDocument.size(); ++length)
        {
            Check(Refused(SmallDocument.substr(0, length)), "a prefix of a valid document is refused");
        }
        ChunkDeltaStore store = BuildSmallStore();
        Fill(store, RealmChunk{3, 1, 1}, 40, 12);
        const std::string document = SerializeChunkDeltas(store);
        for (std::size_t length = 0; length < document.size(); ++length)
        {
            Check(Refused(document.substr(0, length)), "a prefix of a larger valid document is refused");
        }
        Check(ParseChunkDeltas(document).Store.has_value(), "the full larger document is accepted");
        Check(Refused(SmallDocument + " "), "trailing whitespace is refused");
        Check(Refused(SmallDocument + "\n"), "a trailing newline is refused");
        Check(Refused(SmallDocument + "x"), "trailing garbage is refused");
        Check(Refused(" " + SmallDocument), "leading whitespace is refused");
    }

    // Bad versions, counts, ranges, shapes and order.
    {
        Check(ParseChunkDeltas(SmallDocument).Store.has_value(), "the unmutated document is valid");
        Check(Refused(Mutated("pw.chunk_deltas/1", "pw.chunk_deltas/2")), "a later version is refused");
        Check(Refused(Mutated("pw.chunk_deltas/1", "pw.chunk_deltas/0")), "version 0 is refused");
        Check(Refused(Mutated("pw.chunk_deltas/1", "pw.world_deltas/1")), "a foreign format name is refused");
        Check(Refused(Mutated("\"chunkCount\":2", "\"chunkCount\":3")), "chunk count too high");
        Check(Refused(Mutated("\"chunkCount\":2", "\"chunkCount\":1")), "chunk count too low");
        Check(Refused(Mutated("\"chunkCount\":2", "\"chunkCount\":-2")), "negative chunk count");
        Check(Refused(Mutated("\"editCount\":3", "\"editCount\":4")), "edit total too high");
        Check(Refused(Mutated("\"editCount\":3", "\"editCount\":2")), "edit total too low");
        Check(Refused(Mutated("\"editCount\":2,\"edits\"", "\"editCount\":3,\"edits\"")), "chunk edit count too high");
        Check(Refused(Mutated("\"editCount\":1,\"edits\"", "\"editCount\":0,\"edits\"")), "chunk edit count too low");
        Check(Refused(Mutated("\"chunkCount\":2", "\"chunkCount\":2.0")), "a fractional count is refused");
        Check(Refused(Mutated("\"chunkCount\":2", "\"chunkCount\":\"2\"")), "a string count is refused");
        Check(Refused(Mutated("[31,31,0,65535]", "[32,31,0,65535]")), "local x 32");
        Check(Refused(Mutated("[31,31,0,65535]", "[-1,31,0,65535]")), "local x -1");
        Check(Refused(Mutated("[31,31,0,65535]", "[31,32,0,65535]")), "local y 32");
        Check(Refused(Mutated("[31,31,0,65535]", "[31,-1,0,65535]")), "local y -1");
        Check(Refused(Mutated("[31,31,0,65535]", "[31,31,2000001,65535]")), "z too high");
        Check(Refused(Mutated("[31,31,0,65535]", "[31,31,-2000001,65535]")), "z too low");
        Check(Refused(Mutated("[31,31,0,65535]", "[31,31,0,65536]")), "kind 65536");
        Check(Refused(Mutated("[31,31,0,65535]", "[31,31,0,-1]")), "kind -1");
        Check(Refused(Mutated("[31,31,0,65535]", "[31,31,0.5,65535]")), "fractional z");
        Check(Refused(Mutated("[31,31,0,65535]", "[31,31,0]")), "an edit with three fields");
        Check(Refused(Mutated("[31,31,0,65535]", "[31,31,0,65535,1]")), "an edit with five fields");
        Check(Refused(Mutated("[31,31,0,65535]", "{\"x\":31}")), "an edit that is not an array");
        Check(Refused(Mutated("\"floor\":2", "\"floor\":13")), "floor past the stack");
        Check(Refused(Mutated("\"floor\":2", "\"floor\":0")), "floor 0");
        Check(Refused(Mutated("\"floor\":1,\"x\":4", "\"floor\":1,\"x\":2232320")), "chunk x past floor 1's edge (2,180 sectors of 1,024 chunks)");
        Check(Refused(Mutated("\"floor\":2,\"x\":0,\"y\":0", "\"floor\":2,\"x\":0,\"y\":1116160")), "chunk y past floor 2's edge");
        Check(ParseChunkDeltas(Mutated("\"floor\":1,\"x\":4", "\"floor\":1,\"x\":2232319")).Store.has_value(), "the last chunk of floor 1 is accepted");
        Check(Refused(Mutated("\"floor\":2,\"x\":0", "\"floor\":2,\"x\":-1")), "negative chunk x");
        Check(Refused(Mutated("\"lastChangedDay\":3", "\"lastChangedDay\":-3")), "negative day");
        Check(Refused(Mutated("\"pinned\":true", "\"pinned\":1")), "pinned as a number");
        Check(Refused(Mutated("\"pinned\":true", "\"pinned\":\"true\"")), "pinned as a string");
        Check(Refused(Mutated("\"pinned\":true,", "")), "a missing member");
        Check(Refused(Mutated("\"pinned\":true,", "\"pinned\":true,\"extra\":1,")), "an unknown member");
        Check(Refused(Mutated("\"pinned\":true,", "\"pinned\":true,\"pinned\":true,")), "a duplicate member");
        Check(Refused(Mutated("\"end\":\"pw.chunk_deltas.end\"", "\"end\":\"pw.chunk_deltas.fin\"")), "a wrong end marker");
        Check(Refused(Mutated(",\"end\":\"pw.chunk_deltas.end\"", "")), "a missing end marker");
        Check(Refused(Mutated("\"chunks\":[", "\"chunks\":{\"a\":[")), "chunks as an object");
        Check(Refused("[]") && Refused("null") && Refused("{}") && Refused(""), "wrong top-level shapes");
        // Order and repetition: the only accepted form is the canonical one.
        Check(Refused(Mutated("\"floor\":2,\"x\":0,\"y\":0", "\"floor\":1,\"x\":4,\"y\":2")), "a repeated chunk");
        Check(Refused(Mutated("\"floor\":1,\"x\":4,\"y\":2", "\"floor\":3,\"x\":4,\"y\":2")), "chunks out of order");
        Check(Refused(Mutated("[[1,0,-3,0],[0,1,5,17]]", "[[0,1,5,17],[1,0,-3,0]]")), "edits out of order");
        Check(Refused(Mutated("[[1,0,-3,0],[0,1,5,17]]", "[[1,0,-3,0],[1,0,-3,5]]")), "two edits at one block");
        Check(Refused(Mutated("[[1,0,-3,0],[0,1,5,17]]", "[[1,0,-3,0],[1,0,-4,5]]")), "z out of order at one column");
        Check(Refused(Mutated("\"editCount\":2,\"edits\":[[1,0,-3,0],[0,1,5,17]]", "\"editCount\":0,\"edits\":[]")) &&
                  Refused(Mutated("\"editCount\":1,\"edits\":[[31,31,0,65535]]", "\"editCount\":0,\"edits\":[]")),
              "a chunk with no edits is refused");
        // Reordered members are valid JSON but not the canonical form.
        const ChunkDeltaParseResult shuffled = ParseChunkDeltas(
            "{\"end\":\"pw.chunk_deltas.end\",\"format\":\"pw.chunk_deltas/1\",\"chunkCount\":0,\"editCount\":0,\"chunks\":[]}");
        if (shuffled.Store.has_value())
        {
            // Accepting member order is allowed so long as the result re-serialises canonically.
            Check(SerializeChunkDeltas(*shuffled.Store).starts_with("{\"format\""), "accepted documents re-serialise canonically");
        }
        // The loader enforces the same invariants for callers that build chunks themselves.
        ChunkDeltaStore loader;
        Check(!loader.LoadChunk(ChunkA, ChunkDelta{}), "an empty chunk cannot be loaded");
        Check(!loader.LoadChunk(RealmChunk{1, 0, 99'999'999}, ChunkDelta{{BlockEdit{0, 0, 0, 1}}, 0, false}), "a chunk off its floor cannot be loaded");
        Check(!loader.LoadChunk(ChunkA, ChunkDelta{{BlockEdit{0, 0, 0, 1}}, -1, false}), "a negative day cannot be loaded");
        Check(!loader.LoadChunk(ChunkA, ChunkDelta{{BlockEdit{1, 0, 0, 1}, BlockEdit{0, 0, 0, 1}}, 0, false}), "unsorted edits cannot be loaded");
        Check(loader.LoadChunk(ChunkA, ChunkDelta{{BlockEdit{0, 0, 0, 1}}, 4, true}), "a good chunk loads");
        Check(!loader.LoadChunk(ChunkA, ChunkDelta{{BlockEdit{0, 0, 0, 1}}, 4, true}), "a chunk cannot be loaded twice");
        Check(loader.EditCount() == 1 && loader.ChunkCount() == 1, "failed loads change nothing");
    }

    // The cap reverts exactly the documented chunks, in the documented order, and never a pinned one.
    {
        const RealmChunk c1{1, 5, 5};  // day 10, 4 edits
        const RealmChunk c2{1, 1, 9};  // day 10, 5 edits (ties with c1, y 9 after y 5)
        const RealmChunk c3{1, 3, 5};  // day 10, 6 edits (ties; same y as c1, x 3 before x 5)
        const RealmChunk c4{2, 0, 0};  // day 10, 7 edits (ties; floor 2 after floor 1)
        const RealmChunk c5{1, 0, 0};  // day 20, 8 edits
        const RealmChunk p1{1, 8, 8};  // day 1, 50 edits, pinned: oldest of all but immune
        const RealmChunk c6{3, 0, 0};  // day 30, 9 edits
        const auto build = [&]() {
            ChunkDeltaStore store;
            Fill(store, c1, 4, 10);
            Fill(store, c2, 5, 10);
            Fill(store, c3, 6, 10);
            Fill(store, c4, 7, 10);
            Fill(store, c5, 8, 20);
            Fill(store, p1, 50, 1);
            Fill(store, c6, 9, 30);
            Check(store.SetPinned(p1, true), "pin the oldest chunk");
            return store;
        };
        // Tie order among day 10 is c3 (1,y5,x3), c1 (1,y5,x5), c2 (1,y9,x1), c4 (2,y0,x0).
        const ChunkDeltaStore full = build();
        Check(full.EditCount() == 4 + 5 + 6 + 7 + 8 + 50 + 9, "the cap scenario holds 89 edits");

        {
            ChunkDeltaStore store = build();
            const ChunkDeltaCapReport report = store.ApplyEditCap(89);
            Check(report.RevertedChunks.empty() && report.EditsReverted == 0 && report.CapMet && report.EditsRemaining == 89,
                  "a store at the cap is left alone");
            Check(store == full, "and is unchanged");
            Check(store.ApplyEditCap(1'000).RevertedChunks.empty(), "a store under the cap is left alone");
        }
        {
            ChunkDeltaStore store = build();
            const ChunkDeltaCapReport report = store.ApplyEditCap(88);
            Check(report.RevertedChunks == std::vector<RealmChunk>{c3}, "one over the cap reverts only c3, the first of the day-10 ties");
            Check(report.EditsReverted == 6 && report.EditsRemaining == 83 && report.CapMet, "6 edits drop; a coarser fit than 1 is accepted");
            Check(!store.Find(c3) && store.Find(c1) && store.Find(c5), "the others stay");
        }
        {
            ChunkDeltaStore store = build();
            const ChunkDeltaCapReport report = store.ApplyEditCap(80);
            Check(report.RevertedChunks == (std::vector<RealmChunk>{c3, c1}), "cap 80: c3 then c1 (83 is still over, 79 is under)");
            Check(report.EditsReverted == 10 && report.EditsRemaining == 79 && report.CapMet, "79 edits remain");
        }
        {
            ChunkDeltaStore store = build();
            const ChunkDeltaCapReport report = store.ApplyEditCap(60);
            Check(report.RevertedChunks == (std::vector<RealmChunk>{c3, c1, c2, c4, c5}),
                  "cap 60: the day-10 chunks in floor, y, x order, then the day-20 chunk (67 is still over)");
            Check(report.EditsReverted == 30 && report.EditsRemaining == 59 && report.CapMet, "59 remain and the cap is met");
            Check(store.Find(p1) != nullptr && store.Find(p1)->Pinned && store.Find(c6) != nullptr, "the pinned chunk and the newest stay");
        }
        {
            // The pinned chunk alone (50) fits a cap of 55 only with c6 gone: 59 -> 50.
            ChunkDeltaStore store = build();
            const ChunkDeltaCapReport report = store.ApplyEditCap(55);
            Check(report.RevertedChunks == (std::vector<RealmChunk>{c3, c1, c2, c4, c5, c6}), "cap 55 reverts every unpinned chunk, oldest first");
            Check(report.EditsReverted == 39 && report.EditsRemaining == 50 && report.CapMet, "only the pinned 50 edits remain");
        }
        {
            // A cap the pinned chunk alone exceeds: everything unpinned goes, the pinned one never does.
            ChunkDeltaStore store = build();
            const ChunkDeltaCapReport report = store.ApplyEditCap(10);
            Check(report.RevertedChunks.size() == 6 && report.EditsRemaining == 50 && !report.CapMet, "pinned chunks may leave the cap unmet");
            Check(store.ChunkCount() == 1 && store.Find(p1) != nullptr, "the pinned chunk survives");
            Check(store.ApplyEditCap(-5).RevertedChunks.empty(), "a negative cap counts as 0, and pinned chunks still stay");
            Check(store.EditCount() == 50, "nothing pinned was lost");
        }
        {
            // Unpinning makes it eligible, and the pinned chunk was the oldest (day 1).
            ChunkDeltaStore store = build();
            Check(store.SetPinned(p1, false), "unpin");
            const ChunkDeltaCapReport report = store.ApplyEditCap(60);
            Check(!report.RevertedChunks.empty() && report.RevertedChunks.front() == p1, "an unpinned oldest chunk reverts first");
            Check(report.EditsRemaining == 39 && report.CapMet, "50 gone leaves 39");
        }
        {
            // The cap is explicit: a store over the cap still serialises and parses whole.
            const ChunkDeltaStore store = build();
            const ChunkDeltaParseResult parsed = ParseChunkDeltas(SerializeChunkDeltas(store));
            Check(parsed.Store.has_value() && *parsed.Store == store && parsed.Store->EditCount() == 89, "no implicit cap on save or load");
        }
    }

    std::cout << "PASS: " << Checks << " chunk delta checks\n";
    return 0;
}
