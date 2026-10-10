#include "ConductLedger.h"

#include "IdentityText.h"

#include <utility>

namespace PerfectWorld
{
namespace
{
namespace Text = IdentityText;

constexpr std::string_view ConductHeader = "pw_conduct";
constexpr std::uint64_t ConductHashTag = 0x5057434F4E445543ULL; // "PWCONDUC"
// "test" + id + choice + action + stakes + at + parties + one field per dimension.
constexpr std::size_t ConductTestFieldCount = 7 + ConductDimensionCount;

std::string ConductPartiesField(const std::vector<std::string>& parties)
{
    if (parties.empty())
        return "-";
    std::string out;
    for (std::size_t i = 0; i < parties.size(); ++i)
    {
        if (i > 0)
            out += ',';
        out += parties[i];
    }
    return out;
}

std::string ConductTestLine(const ConductEntry& entry)
{
    std::string line = "test ";
    line += entry.Record.TestId;
    line += ' ';
    line += entry.Record.ChoiceId;
    line += ' ';
    line += ConductActionName(entry.Record.Action);
    line += ' ';
    line += std::to_string(entry.Record.Stakes);
    line += ' ';
    line += std::to_string(entry.Record.At);
    line += ' ';
    line += ConductPartiesField(entry.Record.Parties);
    for (const std::int32_t value : entry.Points)
    {
        line += ' ';
        line += std::to_string(value);
    }
    line += '\n';
    return line;
}

// Splits a comma-joined party field. Any empty piece refuses it; "-" means none.
std::optional<std::vector<std::string>> ConductSplitParties(std::string_view field)
{
    std::vector<std::string> parties;
    if (field == "-")
        return parties;
    std::size_t start = 0;
    while (true)
    {
        const std::size_t end = field.find(',', start);
        const std::string_view piece = field.substr(start, end == std::string_view::npos ? end : end - start);
        if (piece.empty())
            return std::nullopt;
        parties.emplace_back(piece);
        if (end == std::string_view::npos)
            return parties;
        start = end + 1;
    }
}

// Parses one "test" line into an entry that is not yet validated against a ledger.
std::optional<ConductEntry> ConductParseTest(std::string_view line)
{
    const auto fields = Text::SplitFields(line);
    if (!fields || fields->size() != ConductTestFieldCount || (*fields)[0] != "test")
        return std::nullopt;
    const std::optional<ConductAction> action = ParseConductAction((*fields)[3]);
    const std::optional<std::int64_t> stakes = Text::ParseSigned((*fields)[4]);
    const std::optional<std::int64_t> at = Text::ParseSigned((*fields)[5]);
    auto parties = ConductSplitParties((*fields)[6]);
    if (!action || !stakes || *stakes < ConductMinStakes || *stakes > ConductMaxStakes || !at || *at < 0 || !parties)
        return std::nullopt;
    ConductEntry entry;
    entry.Record.TestId = std::string((*fields)[1]);
    entry.Record.ChoiceId = std::string((*fields)[2]);
    entry.Record.Action = *action;
    entry.Record.Stakes = static_cast<std::int32_t>(*stakes);
    entry.Record.At = *at;
    entry.Record.Parties = std::move(*parties);
    for (std::size_t d = 0; d < ConductDimensionCount; ++d)
    {
        const std::optional<std::int64_t> value = Text::ParseSigned((*fields)[7 + d]);
        if (!value || *value < 0 || *value > ConductTestDimensionCap)
            return std::nullopt;
        entry.Points[d] = static_cast<std::int32_t>(*value);
    }
    return entry;
}
} // namespace

std::uint64_t HashConductLedger(const ConductLedger& ledger)
{
    Text::HashWriter out;
    out.Unsigned(ConductHashTag);
    out.Unsigned(ConductLedgerFormatVersion);
    out.Unsigned(ledger.Entries.size());
    for (const ConductEntry& entry : ledger.Entries)
    {
        out.Text(entry.Record.TestId);
        out.Text(entry.Record.ChoiceId);
        out.Unsigned(static_cast<std::uint64_t>(entry.Record.Action));
        out.Signed(entry.Record.Stakes);
        out.Signed(entry.Record.At);
        out.Unsigned(entry.Record.Parties.size());
        for (const std::string& party : entry.Record.Parties)
            out.Text(party);
        for (const std::int32_t value : entry.Points)
            out.Signed(value);
    }
    return out.Result();
}

std::string SerialiseConductLedger(const ConductLedger& ledger)
{
    std::string out;
    out += ConductHeader;
    out += ' ';
    out += std::to_string(ConductLedgerFormatVersion);
    out += "\ncount ";
    out += std::to_string(ledger.Entries.size());
    out += '\n';
    for (const ConductEntry& entry : ledger.Entries)
        out += ConductTestLine(entry);
    out += "end ";
    out += std::to_string(HashConductLedger(ledger));
    out += '\n';
    return out;
}

std::optional<ConductLedger> ParseConductLedger(std::string_view text)
{
    const std::optional<std::vector<std::string_view>> lines = Text::SplitLines(text);
    if (!lines || lines->size() < 3)
        return std::nullopt;
    const auto header = Text::SplitFields((*lines)[0]);
    const auto countLine = Text::SplitFields((*lines)[1]);
    if (!header || header->size() != 2 || (*header)[0] != ConductHeader || (*header)[1] != std::to_string(ConductLedgerFormatVersion))
        return std::nullopt;
    if (!countLine || countLine->size() != 2 || (*countLine)[0] != "count")
        return std::nullopt;
    const std::optional<std::uint64_t> count = Text::ParseUnsigned((*countLine)[1]);
    if (!count || *count > ConductMaxTests || lines->size() != 3 + *count)
        return std::nullopt;

    // Each line is replayed through RecordConduct, so every ledger rule (replay, order, fairness, party shape,
    // table evidence) is enforced exactly as at record time. The stored copy must then match what was recorded.
    ConductLedger ledger;
    for (std::uint64_t i = 0; i < *count; ++i)
    {
        const std::optional<ConductEntry> parsed = ConductParseTest((*lines)[2 + i]);
        if (!parsed || !RecordConduct(ledger, parsed->Record).Ok() || ledger.Entries.back() != *parsed)
            return std::nullopt;
    }

    const auto endLine = Text::SplitFields((*lines)[2 + *count]);
    if (!endLine || endLine->size() != 2 || (*endLine)[0] != "end")
        return std::nullopt;
    const std::optional<std::uint64_t> hash = Text::ParseUnsigned((*endLine)[1]);
    if (!hash || *hash != HashConductLedger(ledger))
        return std::nullopt;
    return ledger;
}
} // namespace PerfectWorld
