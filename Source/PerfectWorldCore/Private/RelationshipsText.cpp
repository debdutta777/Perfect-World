#include "IdentityRegistry.h"
#include "IdentityText.h"
#include "Relationships.h"

#include <algorithm>
#include <utility>

namespace PerfectWorld
{
namespace
{
namespace Text = IdentityText;

constexpr std::string_view RelationshipHeader = "pw_relationships";
constexpr std::uint64_t RelationshipHashTag = 0x505752454C415445ULL; // "PWRELATE"
constexpr std::uint32_t RelationshipKnownActs = (1u << RelationshipActKindCount) - 1;

void RelationshipHashPair(Text::HashWriter& out, const Relationship& pair)
{
    out.Text(pair.Low);
    out.Text(pair.High);
    out.Unsigned(static_cast<std::uint64_t>(pair.Stage));
    out.Signed(pair.Trust);
    out.Signed(pair.Warmth);
    out.Signed(pair.Respect);
    out.Signed(pair.Owed);
    out.Unsigned(pair.FirstActs);
    out.Unsigned(pair.LastSeq);
    out.Unsigned(pair.Memories.size());
    for (const RelationshipMemory& memory : pair.Memories)
    {
        out.Unsigned(memory.EventSeq);
        out.Signed(memory.At);
        out.Text(memory.Type);
        out.Unsigned(static_cast<std::uint64_t>(memory.Kind));
        out.Signed(memory.Salience);
    }
}

std::string RelationshipPairLine(const Relationship& pair)
{
    std::string line = "pair ";
    line += pair.Low;
    line += ' ';
    line += pair.High;
    line += ' ';
    line += RelationshipStageName(pair.Stage);
    for (const std::int32_t value : {pair.Trust, pair.Warmth, pair.Respect, pair.Owed})
    {
        line += ' ';
        line += std::to_string(value);
    }
    line += ' ';
    line += std::to_string(pair.FirstActs);
    line += ' ';
    line += std::to_string(pair.LastSeq);
    line += ' ';
    line += std::to_string(pair.Memories.size());
    line += '\n';
    return line;
}

std::string RelationshipMemoryLine(const RelationshipMemory& memory)
{
    std::string line = "mem ";
    line += std::to_string(memory.EventSeq);
    line += ' ';
    line += std::to_string(memory.At);
    line += ' ';
    line += memory.Type;
    line += ' ';
    line += RelationshipActKindName(memory.Kind);
    line += ' ';
    line += std::to_string(memory.Salience);
    line += '\n';
    return line;
}

std::optional<std::int32_t> RelationshipParseMilli(std::string_view text)
{
    const std::optional<std::int64_t> value = Text::ParseSigned(text);
    if (!value || *value < RelationshipMilliMin || *value > RelationshipMilliMax)
        return std::nullopt;
    return static_cast<std::int32_t>(*value);
}

std::optional<RelationshipMemory> RelationshipParseMemory(std::string_view line)
{
    const auto fields = Text::SplitFields(line);
    if (!fields || fields->size() != 6 || (*fields)[0] != "mem")
        return std::nullopt;
    const std::optional<std::uint64_t> seq = Text::ParseUnsigned((*fields)[1]);
    const std::optional<std::int64_t> at = Text::ParseSigned((*fields)[2]);
    const std::optional<RelationshipActKind> kind = ParseRelationshipActKind((*fields)[4]);
    const std::optional<std::int64_t> salience = Text::ParseSigned((*fields)[5]);
    if (!seq || *seq == 0 || !at || *at < 0 || !kind || !salience || *salience < 0 || *salience > RelationshipSalienceMax)
        return std::nullopt;
    if (!IsRelationshipEventTypeFair((*fields)[3]))
        return std::nullopt;
    RelationshipMemory memory;
    memory.EventSeq = *seq;
    memory.At = *at;
    memory.Type = std::string((*fields)[3]);
    memory.Kind = *kind;
    memory.Salience = static_cast<std::int32_t>(*salience);
    return memory;
}

// Parses a "pair" line (without its memories) and returns the memory count.
std::optional<std::pair<Relationship, std::uint64_t>> RelationshipParsePair(std::string_view line)
{
    const auto fields = Text::SplitFields(line);
    if (!fields || fields->size() != 11 || (*fields)[0] != "pair")
        return std::nullopt;
    const std::optional<RelationshipStage> stage = ParseRelationshipStage((*fields)[3]);
    const std::optional<std::int32_t> trust = RelationshipParseMilli((*fields)[4]);
    const std::optional<std::int32_t> warmth = RelationshipParseMilli((*fields)[5]);
    const std::optional<std::int32_t> respect = RelationshipParseMilli((*fields)[6]);
    const std::optional<std::int32_t> owed = RelationshipParseMilli((*fields)[7]);
    const std::optional<std::uint64_t> mask = Text::ParseUnsigned((*fields)[8]);
    const std::optional<std::uint64_t> lastSeq = Text::ParseUnsigned((*fields)[9]);
    const std::optional<std::uint64_t> memoryCount = Text::ParseUnsigned((*fields)[10]);
    if (!stage || !trust || !warmth || !respect || !owed || !mask || !lastSeq || !memoryCount)
        return std::nullopt;
    if (*mask == 0 || (*mask & ~static_cast<std::uint64_t>(RelationshipKnownActs)) != 0 || *lastSeq == 0 || *memoryCount > RelationshipMaxMemories)
        return std::nullopt;
    if (!IsValidIdentityId((*fields)[1]) || !IsValidIdentityId((*fields)[2]) || !((*fields)[1] < (*fields)[2]))
        return std::nullopt;
    // A pair is a memorial exactly when a bereavement has applied.
    const bool bereaved = (*mask & (1u << static_cast<unsigned>(RelationshipActKind::Bereave))) != 0;
    if (bereaved != (*stage == RelationshipStage::Memorial))
        return std::nullopt;
    Relationship pair;
    pair.Low = std::string((*fields)[1]);
    pair.High = std::string((*fields)[2]);
    pair.Stage = *stage;
    pair.Trust = *trust;
    pair.Warmth = *warmth;
    pair.Respect = *respect;
    pair.Owed = *owed;
    pair.FirstActs = static_cast<std::uint32_t>(*mask);
    pair.LastSeq = *lastSeq;
    return std::make_pair(std::move(pair), *memoryCount);
}
} // namespace

bool RelationshipsMatchLog(const RelationshipBook& book, const EventLog& log)
{
    for (const auto& [key, pair] : book.Pairs())
    {
        if (pair.LastSeq > log.Size())
            return false;
        for (const RelationshipMemory& memory : pair.Memories)
        {
            const EventLogEntry* entry = log.Find(memory.EventSeq);
            if (entry == nullptr || entry->At != memory.At || entry->Type != memory.Type)
                return false;
            const auto has = [entry](const std::string& id) {
                return std::find(entry->Parties.begin(), entry->Parties.end(), id) != entry->Parties.end();
            };
            if (!has(pair.Low) || !has(pair.High))
                return false;
        }
    }
    return true;
}

std::uint64_t HashRelationships(const RelationshipBook& book)
{
    Text::HashWriter out;
    out.Unsigned(RelationshipHashTag);
    out.Unsigned(RelationshipFormatVersion);
    out.Unsigned(book.Size());
    for (const auto& entry : book.Pairs())
        RelationshipHashPair(out, entry.second);
    return out.Result();
}

std::string SerialiseRelationships(const RelationshipBook& book)
{
    std::string out;
    out += RelationshipHeader;
    out += ' ';
    out += std::to_string(RelationshipFormatVersion);
    out += "\ncount ";
    out += std::to_string(book.Size());
    out += '\n';
    for (const auto& entry : book.Pairs())
    {
        out += RelationshipPairLine(entry.second);
        for (const RelationshipMemory& memory : entry.second.Memories)
            out += RelationshipMemoryLine(memory);
    }
    out += "end ";
    out += std::to_string(HashRelationships(book));
    out += '\n';
    return out;
}

std::optional<RelationshipBook> ParseRelationships(std::string_view text)
{
    const std::optional<std::vector<std::string_view>> lines = Text::SplitLines(text);
    if (!lines || lines->size() < 3)
        return std::nullopt;
    const auto header = Text::SplitFields((*lines)[0]);
    const auto countLine = Text::SplitFields((*lines)[1]);
    if (!header || header->size() != 2 || (*header)[0] != RelationshipHeader || (*header)[1] != std::to_string(RelationshipFormatVersion))
        return std::nullopt;
    if (!countLine || countLine->size() != 2 || (*countLine)[0] != "count")
        return std::nullopt;
    const std::optional<std::uint64_t> count = Text::ParseUnsigned((*countLine)[1]);
    if (!count || *count > RelationshipMaxPairs)
        return std::nullopt;

    RelationshipBook book;
    std::size_t cursor = 2;
    const std::size_t lastLine = lines->size() - 1; // the "end" line
    for (std::uint64_t index = 0; index < *count; ++index)
    {
        if (cursor >= lastLine)
            return std::nullopt;
        auto parsed = RelationshipParsePair((*lines)[cursor++]);
        if (!parsed || cursor + parsed->second > lastLine)
            return std::nullopt;
        Relationship& pair = parsed->first;
        for (std::uint64_t m = 0; m < parsed->second; ++m)
        {
            auto memory = RelationshipParseMemory((*lines)[cursor++]);
            if (!memory)
                return std::nullopt;
            const bool ascending = pair.Memories.empty() || pair.Memories.back().EventSeq < memory->EventSeq;
            const bool kindApplied = (pair.FirstActs & (1u << static_cast<unsigned>(memory->Kind))) != 0;
            if (!ascending || !kindApplied || memory->EventSeq > pair.LastSeq)
                return std::nullopt;
            pair.Memories.push_back(std::move(*memory));
        }
        if (!book.RestorePair(std::move(pair)))
            return std::nullopt;
    }
    if (cursor != lastLine)
        return std::nullopt;

    const auto endLine = Text::SplitFields((*lines)[lastLine]);
    if (!endLine || endLine->size() != 2 || (*endLine)[0] != "end")
        return std::nullopt;
    const std::optional<std::uint64_t> hash = Text::ParseUnsigned((*endLine)[1]);
    if (!hash || *hash != HashRelationships(book))
        return std::nullopt;
    return book;
}
} // namespace PerfectWorld
