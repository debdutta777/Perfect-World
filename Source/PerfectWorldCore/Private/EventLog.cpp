#include "EventLog.h"

#include "IdentityRegistry.h"
#include "IdentityText.h"

#include <algorithm>
#include <utility>

namespace PerfectWorld
{
namespace
{
namespace Text = IdentityText;

constexpr std::string_view EventLogHeader = "pw_events";
constexpr std::uint64_t EventLogHashTag = 0x50574556544C4F47ULL; // "PWEVTLOG"

void EventLogHashEntry(Text::HashWriter& out, const EventLogEntry& entry)
{
    out.Unsigned(entry.Seq);
    out.Signed(entry.At);
    out.Text(entry.Type);
    out.Unsigned(entry.CauseSeq);
    out.Unsigned(entry.Parties.size());
    for (const std::string& party : entry.Parties)
        out.Text(party);
}

std::string EventLogEntryLine(const EventLogEntry& entry)
{
    std::string line = "event ";
    line += std::to_string(entry.Seq);
    line += ' ';
    line += std::to_string(entry.At);
    line += ' ';
    line += entry.Type;
    line += ' ';
    line += std::to_string(entry.CauseSeq);
    line += ' ';
    line += std::to_string(entry.Parties.size());
    for (const std::string& party : entry.Parties)
    {
        line += ' ';
        line += party;
    }
    line += '\n';
    return line;
}

/** Parses an "event ..." line into a draft, requiring the line's Seq to equal expectedSeq. */
std::optional<EventLogDraft> EventLogParseEntry(const std::vector<std::string_view>& fields, std::uint64_t expectedSeq)
{
    if (fields.size() < 6 || fields[0] != "event")
        return std::nullopt;
    const std::optional<std::uint64_t> seq = Text::ParseUnsigned(fields[1]);
    const std::optional<std::int64_t> at = Text::ParseSigned(fields[2]);
    const std::optional<std::uint64_t> cause = Text::ParseUnsigned(fields[4]);
    const std::optional<std::uint64_t> partyCount = Text::ParseUnsigned(fields[5]);
    if (!seq || *seq != expectedSeq || !at || !cause || !partyCount || *partyCount != fields.size() - 6)
        return std::nullopt;
    EventLogDraft draft;
    draft.At = *at;
    draft.Type = std::string(fields[3]);
    draft.CauseSeq = *cause;
    for (std::size_t i = 6; i < fields.size(); ++i)
        draft.Parties.emplace_back(fields[i]);
    return draft;
}
} // namespace

EventLogAppendResult EventLog::Append(const EventLogDraft& draft)
{
    if (Entries_.size() >= EventLogMaxEntries)
        return {EventLogStatus::Full, 0};
    if (!IsStableIdToken(draft.Type))
        return {EventLogStatus::BadType, 0};
    if (draft.At < 0 || (!Entries_.empty() && draft.At < Entries_.back().At))
        return {EventLogStatus::BadTime, 0};
    // The next Seq is size + 1, so a cause must be one of the entries already held.
    if (draft.CauseSeq > Entries_.size())
        return {EventLogStatus::BadCause, 0};
    if (draft.Parties.size() > EventLogMaxParties)
        return {EventLogStatus::BadParties, 0};
    for (std::size_t i = 0; i < draft.Parties.size(); ++i)
    {
        const bool repeated = std::find(draft.Parties.begin(), draft.Parties.begin() + static_cast<std::ptrdiff_t>(i), draft.Parties[i]) !=
                              draft.Parties.begin() + static_cast<std::ptrdiff_t>(i);
        if (repeated || !IsValidIdentityId(draft.Parties[i]))
            return {EventLogStatus::BadParties, 0};
    }

    EventLogEntry entry;
    entry.Seq = Entries_.size() + 1;
    entry.At = draft.At;
    entry.Type = draft.Type;
    entry.CauseSeq = draft.CauseSeq;
    entry.Parties = draft.Parties;
    Entries_.push_back(std::move(entry));
    return {EventLogStatus::Appended, Entries_.back().Seq};
}

const EventLogEntry* EventLog::Find(std::uint64_t seq) const
{
    return seq >= 1 && seq <= Entries_.size() ? &Entries_[static_cast<std::size_t>(seq - 1)] : nullptr;
}

std::uint64_t HashEventLogPrefix(const EventLog& log, std::size_t count)
{
    Text::HashWriter out;
    out.Unsigned(EventLogHashTag);
    out.Unsigned(EventLogFormatVersion);
    const std::size_t used = std::min(count, log.Size());
    for (std::size_t i = 0; i < used; ++i)
        EventLogHashEntry(out, log.Entries()[i]);
    return out.Result();
}

std::uint64_t HashEventLog(const EventLog& log)
{
    return HashEventLogPrefix(log, log.Size());
}

std::string SerialiseEventLog(const EventLog& log)
{
    std::string out;
    out += EventLogHeader;
    out += ' ';
    out += std::to_string(EventLogFormatVersion);
    out += "\ncount ";
    out += std::to_string(log.Size());
    out += '\n';
    for (const EventLogEntry& entry : log.Entries())
        out += EventLogEntryLine(entry);
    out += "end ";
    out += std::to_string(HashEventLog(log));
    out += '\n';
    return out;
}

std::optional<EventLog> ParseEventLog(std::string_view text)
{
    const std::optional<std::vector<std::string_view>> lines = Text::SplitLines(text);
    if (!lines || lines->size() < 3)
        return std::nullopt;

    const auto header = Text::SplitFields((*lines)[0]);
    const auto countLine = Text::SplitFields((*lines)[1]);
    if (!header || header->size() != 2 || (*header)[0] != EventLogHeader || (*header)[1] != std::to_string(EventLogFormatVersion))
        return std::nullopt;
    if (!countLine || countLine->size() != 2 || (*countLine)[0] != "count")
        return std::nullopt;
    const std::optional<std::uint64_t> count = Text::ParseUnsigned((*countLine)[1]);
    if (!count || *count > EventLogMaxEntries || lines->size() != *count + 3)
        return std::nullopt;

    EventLog log;
    for (std::uint64_t index = 0; index < *count; ++index)
    {
        const auto fields = Text::SplitFields((*lines)[static_cast<std::size_t>(2 + index)]);
        if (!fields)
            return std::nullopt;
        const std::optional<EventLogDraft> draft = EventLogParseEntry(*fields, index + 1);
        if (!draft || !log.Append(*draft).Ok())
            return std::nullopt;
    }

    const auto endLine = Text::SplitFields(lines->back());
    if (!endLine || endLine->size() != 2 || (*endLine)[0] != "end")
        return std::nullopt;
    const std::optional<std::uint64_t> hash = Text::ParseUnsigned((*endLine)[1]);
    if (!hash || *hash != HashEventLog(log))
        return std::nullopt;
    return log;
}
} // namespace PerfectWorld
