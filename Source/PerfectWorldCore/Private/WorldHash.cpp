#include "WorldHash.h"

namespace PerfectWorld
{
namespace
{
constexpr std::uint64_t WorldHashFnvOffset = 0xcbf29ce484222325ULL;
constexpr std::uint64_t WorldHashFnvPrime = 0x100000001b3ULL;

/** FNV-1a 64 over explicit, tagged fields. Every integer is written little-endian, byte by byte. */
class WorldHashWriter
{
public:
    void Byte(std::uint8_t value)
    {
        Hash_ ^= value;
        Hash_ *= WorldHashFnvPrime;
    }

    void Unsigned(std::uint64_t value)
    {
        Byte('u');
        Raw64(value);
    }

    void Signed(std::int64_t value)
    {
        Byte('i');
        Raw64(static_cast<std::uint64_t>(value));
    }

    void Bool(bool value)
    {
        Byte('b');
        Byte(value ? 1 : 0);
    }

    void Text(const std::string& value)
    {
        Byte('s');
        Raw64(static_cast<std::uint64_t>(value.size()));
        for (const char c : value)
            Byte(static_cast<std::uint8_t>(c));
    }

    /** Written before every collection, so one section's items cannot bleed into the next field. */
    void Count(std::size_t count)
    {
        Byte('n');
        Raw64(static_cast<std::uint64_t>(count));
    }

    [[nodiscard]] std::uint64_t Result() const { return Hash_; }

private:
    void Raw64(std::uint64_t value)
    {
        for (int shift = 0; shift < 64; shift += 8)
            Byte(static_cast<std::uint8_t>((value >> shift) & 0xffU));
    }

    std::uint64_t Hash_ = WorldHashFnvOffset;
};

std::uint64_t WorldHashMetaSection(const WorldState& state)
{
    WorldHashWriter out;
    out.Unsigned(state.WorldSeed);
    out.Unsigned(state.Seq);
    return out.Result();
}

std::uint64_t WorldHashClockSection(const WorldState& state)
{
    const GameClockSave save = state.Clock.CaptureSave();
    WorldHashWriter out;
    out.Signed(save.Now);
    out.Signed(save.NextHandle);
    out.Count(save.Pending.size());
    for (const ScheduledEvent& event : save.Pending)
    {
        out.Signed(event.Handle);
        out.Signed(event.When);
        out.Text(event.Id);
    }
    return out.Result();
}

std::uint64_t WorldHashFlagsSection(const WorldState& state)
{
    WorldHashWriter out;
    out.Count(state.Flags.size());
    for (const auto& [flagId, value] : state.Flags) // std::map: ID order, so insertion order never matters
    {
        out.Text(flagId);
        out.Bool(value);
    }
    return out.Result();
}

std::uint64_t WorldHashGatesSection(const WorldState& state)
{
    WorldHashWriter out;
    out.Count(state.Gates.size());
    for (const auto& [gateId, gate] : state.Gates) // std::map: ID order
    {
        out.Text(gateId);
        out.Unsigned(static_cast<std::uint64_t>(gate.Rank));
        out.Unsigned(static_cast<std::uint64_t>(gate.Status));
        out.Signed(gate.StatusSince);
        out.Signed(gate.OnsetCount);
    }
    return out.Result();
}

std::uint64_t WorldHashRngSection(const WorldState& state)
{
    WorldHashWriter out;
    out.Count(state.Rng.size());
    for (const RandomStreamState& stream : state.Rng) // indexed by RandomStreamId: stream order is part of the contract
    {
        for (const std::uint64_t word : stream.Words)
            out.Unsigned(word);
        out.Unsigned(stream.Draws);
    }
    return out.Result();
}

/**
 * The player's day: the day loop's own hash (HashDayLoop), which covers its core text and the
 * condition, activity, practice, receipt and attendance texts. A day loop that is not valid
 * hashes as zero, and the save reader and the reducer never let one into a world.
 */
std::uint64_t WorldHashDayLoopSection(const WorldState& state)
{
    WorldHashWriter out;
    out.Unsigned(HashDayLoop(state.DayLoop));
    return out.Result();
}

void WorldHashAddSection(std::vector<WorldSectionHash>& sections, const char* name, std::uint64_t hash)
{
    sections.push_back(WorldSectionHash{name, hash});
}

std::uint64_t WorldHashConversationsSection(const WorldState& state)
{
    WorldHashWriter out;
    out.Count(state.Conversations.size());
    for (const auto& [person, conversation] : state.Conversations)
    {
        out.Text(person);
        const auto catalogue = ConversationCatalogueForPerson(person);
        out.Text(catalogue ? SerialiseConversationState(*catalogue, conversation) : std::string{});
    }
    return out.Result();
}

std::uint64_t WorldHashDoorsSection(const WorldState& state)
{
    WorldHashWriter out;
    out.Text(SerialiseDoorState(state.Doors));
    return out.Result();
}
} // namespace

std::vector<WorldSectionHash> HashWorldSections(const WorldState& state)
{
    std::vector<WorldSectionHash> sections;
    // ADDING A SECTION: every slice of WorldState must be hashed here, or a
    // change to it goes unnoticed by replay checks and desync detection.
    //  1. Write a WorldHash<Name>Section(const WorldState&) above that encodes
    //     every field with the tagged WorldHashWriter calls, collections in a
    //     deterministic order, each collection prefixed by Count().
    //  2. Add exactly one WorldHashAddSection line below, at the end of the list.
    //  3. Bump WorldHashVersion in WorldHash.h and re-pin the golden hashes.
    //  4. Add single-field edits for the new slice to Tests/WorldHashTests.cpp.
    WorldHashAddSection(sections, "meta", WorldHashMetaSection(state));
    WorldHashAddSection(sections, "clock", WorldHashClockSection(state));
    WorldHashAddSection(sections, "flags", WorldHashFlagsSection(state));
    WorldHashAddSection(sections, "gates", WorldHashGatesSection(state));
    WorldHashAddSection(sections, "rng", WorldHashRngSection(state));
    WorldHashAddSection(sections, "dayloop", WorldHashDayLoopSection(state));
    WorldHashAddSection(sections, "conversations", WorldHashConversationsSection(state));
    WorldHashAddSection(sections, "doors", WorldHashDoorsSection(state));
    return sections;
}

std::uint64_t HashWorldState(const WorldState& state)
{
    WorldHashWriter out;
    out.Unsigned(WorldHashVersion);
    for (const WorldSectionHash& section : HashWorldSections(state))
    {
        out.Text(section.Name);
        out.Unsigned(section.Hash);
    }
    return out.Result();
}

std::uint64_t HashWorldStateWithDoorsText(const WorldState& state, std::string_view doorsText)
{
    WorldHashWriter doors;
    doors.Text(std::string(doorsText));
    const std::uint64_t doorsHash = doors.Result();
    WorldHashWriter out;
    out.Unsigned(WorldHashVersion);
    for (const WorldSectionHash& section : HashWorldSections(state))
    {
        out.Text(section.Name);
        out.Unsigned(section.Name == "doors" ? doorsHash : section.Hash);
    }
    return out.Result();
}

ReplayResult ReplayCommandLog(std::uint64_t worldSeed, const std::vector<Command>& log, const ReplayOptions& options)
{
    ReplayResult replay;
    WorldSim sim(worldSeed, options.EventCap);
    replay.Complete = true;
    for (const Command& command : log)
    {
        CommandResult result = sim.Submit(command);
        const bool accepted = result.Accepted;
        if (!accepted)
            replay.Refusal = result.Why;
        if (options.KeepResults)
            replay.Results.push_back(std::move(result));
        if (!accepted)
        {
            replay.Complete = false;
            break;
        }
        ++replay.Applied;
        if (options.CheckpointEvery != 0 && replay.Applied % options.CheckpointEvery == 0)
            replay.Checkpoints.push_back(ReplayCheckpoint{sim.State().Seq, HashWorldState(sim.State())});
    }
    replay.Hash = HashWorldState(sim.State());
    return replay;
}
} // namespace PerfectWorld
