#include "Threads.h"

#include "IdentityText.h"
#include "ThreadsInternal.h"

#include <set>

// The fingerprint and the text form of a ThreadSet (ROADMAP M3-06). The layout is documented in Threads.h.
namespace PerfectWorld
{
namespace
{
namespace Text = IdentityText;

constexpr std::string_view ThreadHeader = "pw_threads";
constexpr std::uint64_t ThreadHashTag = 0x5057544852454144ULL; // "PWTHREAD"
constexpr std::size_t ThreadMaxPeopleOnLine = ThreadMaxRoles;
constexpr std::uint64_t ThreadMaxFiredMask = (1ULL << ThreadMaxTriggers) - 1;

void ThrHashPeople(Text::HashWriter& out, const std::vector<std::string>& people)
{
    out.Unsigned(people.size());
    for (const std::string& person : people)
        out.Text(person);
}

void ThrHashInstance(Text::HashWriter& out, const ThreadInstance& live)
{
    out.Text(live.Id);
    out.Text(live.Template);
    out.Signed(live.Stage);
    out.Signed(live.OpenedAt);
    out.Signed(live.StageEnteredAt);
    out.Signed(live.Now);
    out.Unsigned(live.Cursor);
    out.Unsigned(live.Fired);
    ThrHashPeople(out, live.People);
    out.Unsigned(live.Counts.size());
    for (const std::int32_t count : live.Counts)
        out.Signed(count);
}

void ThrAppendPeople(std::string& line, const std::vector<std::string>& people)
{
    line += ' ';
    line += std::to_string(people.size());
    for (const std::string& person : people)
    {
        line += ' ';
        line += person;
    }
}

std::string ThrInstanceLine(const ThreadInstance& live)
{
    std::string line = "thread " + live.Id + ' ' + live.Template + ' ' + std::to_string(live.Stage) + ' ' + std::to_string(live.OpenedAt) + ' ' +
                       std::to_string(live.StageEnteredAt) + ' ' + std::to_string(live.Now) + ' ' + std::to_string(live.Cursor) + ' ' +
                       std::to_string(live.Fired);
    ThrAppendPeople(line, live.People);
    line += ' ';
    line += std::to_string(live.Counts.size());
    for (const std::int32_t count : live.Counts)
    {
        line += ' ';
        line += std::to_string(count);
    }
    line += '\n';
    return line;
}

std::string ThrResolvedLine(const ThreadResolved& done)
{
    std::string line = "done " + done.Id + ' ' + done.Template + ' ' + done.Resolution + ' ' + std::to_string(done.At);
    ThrAppendPeople(line, done.People);
    line += '\n';
    return line;
}

/** "thr_" and a canonical positive number below `next`, as that number. */
std::optional<std::uint64_t> ThrParseInstanceId(std::string_view id, std::uint64_t next)
{
    if (id.substr(0, 4) != "thr_")
        return std::nullopt;
    const std::optional<std::uint64_t> number = Text::ParseUnsigned(id.substr(4));
    if (!number || *number == 0 || *number >= next)
        return std::nullopt;
    return number;
}

/** `count` then that many distinct valid identity IDs, from fields[at...]; advances `at`. */
std::optional<std::vector<std::string>> ThrParsePeople(const std::vector<std::string_view>& fields, std::size_t& at)
{
    if (at >= fields.size())
        return std::nullopt;
    const std::optional<std::uint64_t> count = Text::ParseUnsigned(fields[at++]);
    if (!count || *count < ThreadMinRoles || *count > ThreadMaxPeopleOnLine || at + *count > fields.size())
        return std::nullopt;
    std::vector<std::string> people;
    std::set<std::string> seen;
    for (std::uint64_t i = 0; i < *count; ++i)
    {
        const std::string_view person = fields[at++];
        if (!IsValidIdentityId(person) || !seen.insert(std::string(person)).second)
            return std::nullopt;
        people.emplace_back(person);
    }
    return people;
}

std::optional<ThreadInstance> ThrParseInstance(std::string_view line, std::uint64_t next)
{
    const auto fields = Text::SplitFields(line);
    if (!fields || fields->size() < 13 || (*fields)[0] != "thread")
        return std::nullopt;
    const std::optional<std::uint64_t> number = ThrParseInstanceId((*fields)[1], next);
    const std::optional<std::int64_t> stage = Text::ParseSigned((*fields)[3]);
    const std::optional<std::int64_t> opened = Text::ParseSigned((*fields)[4]);
    const std::optional<std::int64_t> entered = Text::ParseSigned((*fields)[5]);
    const std::optional<std::int64_t> now = Text::ParseSigned((*fields)[6]);
    const std::optional<std::uint64_t> cursor = Text::ParseUnsigned((*fields)[7]);
    const std::optional<std::uint64_t> fired = Text::ParseUnsigned((*fields)[8]);
    if (!number || !IsStableIdToken((*fields)[2]) || !stage || !opened || !entered || !now || !cursor || !fired)
        return std::nullopt;
    if (*stage < 0 || *stage >= static_cast<std::int64_t>(ThreadMaxStages) || *opened < 0 || *entered < *opened || *now < *entered ||
        *fired > ThreadMaxFiredMask)
        return std::nullopt;
    std::size_t at = 9;
    auto people = ThrParsePeople(*fields, at);
    if (!people || at >= fields->size())
        return std::nullopt;
    const std::optional<std::uint64_t> countTotal = Text::ParseUnsigned((*fields)[at++]);
    if (!countTotal || *countTotal > ThreadMaxEventSlots || at + *countTotal != fields->size())
        return std::nullopt;
    ThreadInstance live;
    live.Id = std::string((*fields)[1]);
    live.Template = std::string((*fields)[2]);
    live.People = std::move(*people);
    live.Stage = static_cast<std::int32_t>(*stage);
    live.OpenedAt = *opened;
    live.StageEnteredAt = *entered;
    live.Now = *now;
    live.Cursor = *cursor;
    live.Fired = static_cast<std::uint32_t>(*fired);
    for (std::uint64_t i = 0; i < *countTotal; ++i)
    {
        const std::optional<std::uint64_t> count = Text::ParseUnsigned((*fields)[at++]);
        if (!count || *count > static_cast<std::uint64_t>(ThreadMaxEventCount))
            return std::nullopt;
        live.Counts.push_back(static_cast<std::int32_t>(*count));
    }
    return live;
}

std::optional<ThreadResolved> ThrParseResolved(std::string_view line, std::uint64_t next)
{
    const auto fields = Text::SplitFields(line);
    if (!fields || fields->size() < 8 || (*fields)[0] != "done")
        return std::nullopt;
    const std::optional<std::uint64_t> number = ThrParseInstanceId((*fields)[1], next);
    const std::optional<std::int64_t> at = Text::ParseSigned((*fields)[4]);
    if (!number || !IsStableIdToken((*fields)[2]) || !IsStableIdToken((*fields)[3]) || !at || *at < 0)
        return std::nullopt;
    std::size_t cursor = 5;
    auto people = ThrParsePeople(*fields, cursor);
    if (!people || cursor != fields->size())
        return std::nullopt;
    ThreadResolved done;
    done.Id = std::string((*fields)[1]);
    done.Template = std::string((*fields)[2]);
    done.Resolution = std::string((*fields)[3]);
    done.At = *at;
    done.People = std::move(*people);
    return done;
}

/** The header line "<keyword> <canonical unsigned>", or nothing. */
std::optional<std::uint64_t> ThrParseCounter(std::string_view line, std::string_view keyword)
{
    const auto fields = Text::SplitFields(line);
    if (!fields || fields->size() != 2 || (*fields)[0] != keyword)
        return std::nullopt;
    return Text::ParseUnsigned((*fields)[1]);
}
} // namespace

std::uint64_t HashThreads(const ThreadSet& set)
{
    Text::HashWriter out;
    out.Unsigned(ThreadHashTag);
    out.Unsigned(ThreadFormatVersion);
    out.Unsigned(set.NextInstance);
    out.Unsigned(set.Active.size());
    for (const ThreadInstance& live : set.Active)
        ThrHashInstance(out, live);
    out.Unsigned(set.Resolved.size());
    for (const ThreadResolved& done : set.Resolved)
    {
        out.Text(done.Id);
        out.Text(done.Template);
        out.Text(done.Resolution);
        out.Signed(done.At);
        ThrHashPeople(out, done.People);
    }
    return out.Result();
}

std::string SerialiseThreads(const ThreadSet& set)
{
    std::string out(ThreadHeader);
    out += ' ';
    out += std::to_string(ThreadFormatVersion);
    out += "\nnext " + std::to_string(set.NextInstance) + "\nactive " + std::to_string(set.Active.size()) + '\n';
    for (const ThreadInstance& live : set.Active)
        out += ThrInstanceLine(live);
    out += "resolved " + std::to_string(set.Resolved.size()) + '\n';
    for (const ThreadResolved& done : set.Resolved)
        out += ThrResolvedLine(done);
    out += "end " + std::to_string(HashThreads(set)) + '\n';
    return out;
}

std::optional<ThreadSet> ParseThreads(std::string_view text)
{
    const std::optional<std::vector<std::string_view>> lines = Text::SplitLines(text);
    if (!lines || lines->size() < 5)
        return std::nullopt;
    const auto header = Text::SplitFields((*lines)[0]);
    if (!header || header->size() != 2 || (*header)[0] != ThreadHeader || (*header)[1] != std::to_string(ThreadFormatVersion))
        return std::nullopt;
    const std::optional<std::uint64_t> next = ThrParseCounter((*lines)[1], "next");
    const std::optional<std::uint64_t> activeCount = ThrParseCounter((*lines)[2], "active");
    if (!next || *next == 0 || *next > ThreadMaxInstances + 1 || !activeCount || *activeCount > ThreadMaxActive)
        return std::nullopt;

    ThreadSet set;
    set.NextInstance = *next;
    std::size_t cursor = 3;
    const std::size_t lastLine = lines->size() - 1; // the "end" line
    std::set<std::string> ids;
    std::uint64_t previous = 0;
    for (std::uint64_t i = 0; i < *activeCount; ++i)
    {
        if (cursor >= lastLine)
            return std::nullopt;
        std::optional<ThreadInstance> live = ThrParseInstance((*lines)[cursor++], *next);
        if (!live)
            return std::nullopt;
        const std::uint64_t number = *ThrParseInstanceId(live->Id, *next);
        if (number <= previous || !ids.insert(live->Id).second)
            return std::nullopt;
        previous = number;
        set.Active.push_back(std::move(*live));
    }
    if (cursor >= lastLine)
        return std::nullopt;
    const std::optional<std::uint64_t> resolvedCount = ThrParseCounter((*lines)[cursor++], "resolved");
    if (!resolvedCount || *resolvedCount > ThreadMaxInstances)
        return std::nullopt;
    for (std::uint64_t i = 0; i < *resolvedCount; ++i)
    {
        if (cursor >= lastLine)
            return std::nullopt;
        std::optional<ThreadResolved> done = ThrParseResolved((*lines)[cursor++], *next);
        if (!done || !ids.insert(done->Id).second || (!set.Resolved.empty() && ThreadInternal::ResolvedBefore(*done, set.Resolved.back())))
            return std::nullopt;
        set.Resolved.push_back(std::move(*done));
    }
    if (cursor != lastLine)
        return std::nullopt;
    const auto endLine = Text::SplitFields((*lines)[lastLine]);
    if (!endLine || endLine->size() != 2 || (*endLine)[0] != "end")
        return std::nullopt;
    const std::optional<std::uint64_t> hash = Text::ParseUnsigned((*endLine)[1]);
    if (!hash || *hash != HashThreads(set))
        return std::nullopt;
    return set;
}
} // namespace PerfectWorld
