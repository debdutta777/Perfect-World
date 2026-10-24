#include "StateDiff.h"

#include "StateDiffInternal.h"

#include <algorithm>
#include <map>
#include <utility>

// The day loop's sections in the diff (ROADMAP M3-02). The state is written as its pw.dayloop/1
// envelope and cut into its core lines and the four module texts; each text is a list of lines
// of the form "tag key=value ..." or "key=value", and a line is compared field by field.
namespace PerfectWorld
{
namespace
{
using StateDiffFields = std::vector<std::pair<std::string, std::string>>;

struct StateDiffLine
{
    std::string Tag;
    /** The line's identity where it has one (a receipt's seq, a family id, an attendance day); else empty. */
    std::string Identity;
    std::size_t Ordinal = 0;
    StateDiffFields Fields;
    std::string Raw;
};

std::vector<std::string_view> StateDiffSplit(std::string_view text, char separator)
{
    std::vector<std::string_view> pieces;
    std::size_t start = 0;
    while (start <= text.size())
    {
        std::size_t end = text.find(separator, start);
        if (end == std::string_view::npos)
            end = text.size();
        if (end > start)
            pieces.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return pieces;
}

/** Names for the positional tokens of the attendance text; empty for every other line. */
std::vector<std::string_view> StateDiffPositional(std::string_view part, std::string_view tag)
{
    if (part != "attendance")
        return {};
    if (tag == "range")
        return {"first_day", "next_day"};
    if (tag == "totals")
        return {"school_days", "on_time", "late", "very_late", "absent", "periods_held", "periods_attended"};
    if (tag == "days")
        return {"count"};
    if (tag == "day")
        return {"day", "outcome", "arrival", "periods_held", "period_mask"};
    return {};
}

/** Lines whose value is derived from the others, so reporting it would only repeat a difference. */
bool StateDiffDerivedLine(std::string_view tag)
{
    return tag == "end" || tag == "hash";
}

bool StateDiffParseLine(std::string_view part, std::string_view line, StateDiffLine& out)
{
    const std::size_t space = line.find(' ');
    const std::string_view first = line.substr(0, space);
    const std::size_t equals = first.find('=');
    out.Raw = std::string(line);
    if (equals != std::string_view::npos)
    {
        // "key=value": the whole remainder is the value; a family or skill id leads its value.
        out.Tag = std::string(first.substr(0, equals));
        std::string value(line.substr(equals + 1));
        if (out.Tag == "family" || out.Tag == "skill")
        {
            const std::size_t comma = value.find(',');
            out.Identity = value.substr(0, comma);
            value = comma == std::string::npos ? std::string() : value.substr(comma + 1);
        }
        out.Fields.emplace_back(std::string(), std::move(value));
        return !StateDiffDerivedLine(out.Tag);
    }
    out.Tag = std::string(first);
    if (StateDiffDerivedLine(out.Tag))
        return false;
    const std::vector<std::string_view> names = StateDiffPositional(part, out.Tag);
    std::size_t position = 0;
    if (space != std::string_view::npos)
    {
        for (const std::string_view token : StateDiffSplit(line.substr(space + 1), ' '))
        {
            const std::size_t eq = token.find('=');
            if (eq != std::string_view::npos)
            {
                out.Fields.emplace_back(std::string(token.substr(0, eq)), std::string(token.substr(eq + 1)));
            }
            else
            {
                const std::string name = position < names.size() ? std::string(names[position]) : "f" + std::to_string(position);
                out.Fields.emplace_back(name, std::string(token));
            }
            ++position;
        }
    }
    for (const auto& [name, value] : out.Fields)
    {
        if ((out.Tag == "e" && name == "seq") || (out.Tag == "day" && name == "day"))
            out.Identity = (out.Tag == "e" ? "seq=" : "") + value;
    }
    return true;
}

std::vector<StateDiffLine> StateDiffParseText(std::string_view part, std::string_view text)
{
    std::vector<StateDiffLine> lines;
    std::map<std::string, std::size_t> seen;
    for (const std::string_view raw : StateDiffSplit(text, '\n'))
    {
        StateDiffLine line;
        if (!StateDiffParseLine(part, raw, line))
            continue;
        line.Ordinal = seen[line.Tag]++;
        lines.push_back(std::move(line));
    }
    return lines;
}

std::map<std::string, std::size_t> StateDiffTagCounts(const std::vector<StateDiffLine>& lines)
{
    std::map<std::string, std::size_t> counts;
    for (const StateDiffLine& line : lines)
        ++counts[line.Tag];
    return counts;
}

std::size_t StateDiffCountOf(const std::map<std::string, std::size_t>& counts, const std::string& tag)
{
    const auto found = counts.find(tag);
    return found == counts.end() ? 0 : found->second;
}

/** "tag", "tag[ordinal]" for a repeated tag, or "tag[identity]". */
std::string StateDiffLineKey(const StateDiffLine& line, bool repeated)
{
    if (!line.Identity.empty())
        return line.Tag + "[" + line.Identity + "]";
    return repeated ? line.Tag + "[" + std::to_string(line.Ordinal) + "]" : line.Tag;
}

const std::string* StateDiffFindField(const StateDiffFields& fields, const std::string& name)
{
    for (const auto& [fieldName, value] : fields)
    {
        if (fieldName == name)
            return &value;
    }
    return nullptr;
}

void StateDiffCompareLine(StateDiffSink& sink, const std::string& path, const StateDiffLine& a, const StateDiffLine& b)
{
    for (const auto& [name, value] : a.Fields)
    {
        const std::string fieldPath = name.empty() ? path : path + "." + name;
        const std::string* other = StateDiffFindField(b.Fields, name);
        sink.Field("dayloop", fieldPath, value, other == nullptr ? std::string(StateDiffAbsent) : *other);
    }
    for (const auto& [name, value] : b.Fields)
    {
        if (StateDiffFindField(a.Fields, name) == nullptr)
            sink.Add("dayloop", name.empty() ? path : path + "." + name, std::string(StateDiffAbsent), value);
    }
}

void StateDiffCompareText(StateDiffSink& sink, std::string_view part, std::string_view textA, std::string_view textB)
{
    const std::vector<StateDiffLine> linesA = StateDiffParseText(part, textA);
    const std::vector<StateDiffLine> linesB = StateDiffParseText(part, textB);
    const std::string prefix = "dayloop." + std::string(part) + ".";

    const std::map<std::string, std::size_t> countsA = StateDiffTagCounts(linesA);
    const std::map<std::string, std::size_t> countsB = StateDiffTagCounts(linesB);
    const auto isRepeated = [&](const std::string& tag) {
        return StateDiffCountOf(countsA, tag) > 1 || StateDiffCountOf(countsB, tag) > 1;
    };
    std::map<std::string, const StateDiffLine*> byKeyB;
    for (const StateDiffLine& line : linesB)
        byKeyB.emplace(StateDiffLineKey(line, isRepeated(line.Tag)), &line);
    for (const StateDiffLine& line : linesA)
    {
        const bool repeated = isRepeated(line.Tag);
        const std::string key = StateDiffLineKey(line, repeated);
        const auto other = byKeyB.find(key);
        if (other == byKeyB.end())
        {
            sink.Add("dayloop", prefix + key, line.Raw, std::string(StateDiffAbsent));
            continue;
        }
        StateDiffCompareLine(sink, prefix + key, line, *other->second);
        byKeyB.erase(other);
    }
    for (const StateDiffLine& line : linesB)
    {
        const std::string key = StateDiffLineKey(line, isRepeated(line.Tag));
        if (byKeyB.count(key) != 0)
            sink.Add("dayloop", prefix + key, std::string(StateDiffAbsent), line.Raw);
    }
}

/** The envelope cut into its core lines and the four named module texts. */
struct StateDiffEnvelope
{
    std::vector<std::pair<std::string, std::string>> Parts;
};

StateDiffEnvelope StateDiffCut(const std::string& envelope)
{
    StateDiffEnvelope cut;
    const std::string_view text = envelope;
    std::size_t position = text.find('\n');
    if (position == std::string_view::npos)
        return cut;
    ++position;
    const std::size_t firstSection = text.find("\nsection ", position == 0 ? 0 : position - 1);
    const std::size_t coreEnd = firstSection == std::string_view::npos ? text.size() : firstSection + 1;
    cut.Parts.emplace_back("core", std::string(text.substr(position, coreEnd - position)));
    position = coreEnd;
    while (text.compare(position, 8, "section ") == 0)
    {
        const std::size_t lineEnd = text.find('\n', position);
        if (lineEnd == std::string_view::npos)
            break;
        const std::vector<std::string_view> head = StateDiffSplit(text.substr(position, lineEnd - position), ' ');
        if (head.size() != 3)
            break;
        std::size_t length = 0;
        for (const char c : head[2])
            length = length * 10 + static_cast<std::size_t>(c - '0');
        if (lineEnd + 1 + length > text.size())
            break;
        cut.Parts.emplace_back(std::string(head[1]), std::string(text.substr(lineEnd + 1, length)));
        position = lineEnd + 1 + length;
    }
    return cut;
}
} // namespace

void StateDiffDayLoop(StateDiffSink& sink, const DayLoopState& a, const DayLoopState& b)
{
    const std::string textA = SerialiseDayLoop(a);
    const std::string textB = SerialiseDayLoop(b);
    if (textA.empty() || textB.empty())
    {
        // An invalid day has no text; it never reaches a world or a save, but a diff must not hide it.
        sink.Field("dayloop", "dayloop.valid", textA.empty() ? "false" : "true", textB.empty() ? "false" : "true");
        return;
    }
    const StateDiffEnvelope cutA = StateDiffCut(textA);
    const StateDiffEnvelope cutB = StateDiffCut(textB);
    for (const auto& [name, partA] : cutA.Parts)
    {
        std::string_view partB;
        for (const auto& [otherName, text] : cutB.Parts)
        {
            if (otherName == name)
                partB = text;
        }
        StateDiffCompareText(sink, name, partA, partB);
    }
}
} // namespace PerfectWorld
