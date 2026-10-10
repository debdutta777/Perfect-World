#include "OpportunityReceipts.h"

#include <charconv>
#include <initializer_list>

// The text form of ReceiptState (ROADMAP M2-11). Integers and tokens only; the
// parser is strict, so a truncated, reordered or edited text is refused, and
// the only text that parses is the one SerialiseReceipts would write for the
// state it decodes to.
namespace PerfectWorld
{
namespace
{
constexpr std::string_view ReceiptTextEnd = "end";

/** The next line without its newline. False when no newline remains, which is what a truncated text looks like. */
bool ReceiptNextLine(std::string_view text, std::size_t& position, std::string_view& line)
{
    const std::size_t newline = text.find('\n', position);
    if (newline == std::string_view::npos)
        return false;
    line = text.substr(position, newline - position);
    position = newline + 1;
    return true;
}

/** Canonical decimal only: an optional '-', no '+', no leading zeros, no "-0", nothing trailing. */
template <typename Int>
bool ReceiptParseInt(std::string_view text, Int& out)
{
    if (text.empty())
        return false;
    const std::string_view digits = text.front() == '-' ? text.substr(1) : text;
    if (digits.empty() || (digits.size() > 1 && digits.front() == '0') || (text.front() == '-' && digits == "0"))
        return false;
    for (const char c : digits)
    {
        if (c < '0' || c > '9')
            return false;
    }
    Int value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc() || end != text.data() + text.size())
        return false;
    out = value;
    return true;
}

/**
 * Splits "tag k1=v1 k2=v2 ..." into its values, requiring exactly this tag and
 * these keys in this order, single spaces, non-empty values and nothing after.
 */
bool ReceiptFields(std::string_view line, std::string_view tag, std::initializer_list<std::string_view> keys,
                   std::string_view* values)
{
    if (line.substr(0, tag.size()) != tag)
        return false;
    std::size_t position = tag.size();
    for (const std::string_view key : keys)
    {
        if (position >= line.size() || line[position] != ' ')
            return false;
        ++position;
        if (line.substr(position, key.size()) != key)
            return false;
        position += key.size();
        if (position >= line.size() || line[position] != '=')
            return false;
        ++position;
        const std::size_t end = line.find(' ', position);
        const std::size_t stop = end == std::string_view::npos ? line.size() : end;
        if (stop == position)
            return false;
        *values++ = line.substr(position, stop - position);
        position = stop;
    }
    return position == line.size();
}

constexpr std::initializer_list<std::string_view> ReceiptTotalsKeys = {
    "commitments", "committed_minutes", "learning", "condition", "displaced", "overlap", "displaced_learning"};

void ReceiptAppendTotals(std::string& text, const ReceiptTotals& totals)
{
    text += " commitments=" + std::to_string(totals.Commitments);
    text += " committed_minutes=" + std::to_string(totals.CommittedMinutes);
    text += " learning=" + std::to_string(totals.LearningMilliXp);
    text += " condition=" + std::to_string(totals.ConditionCostMilli);
    text += " displaced=" + std::to_string(totals.Displaced);
    text += " overlap=" + std::to_string(totals.DisplacedOverlapMinutes);
    text += " displaced_learning=" + std::to_string(totals.DisplacedLearningMilliXp);
}

bool ReceiptReadTotals(const std::string_view* v, ReceiptTotals& totals)
{
    return ReceiptParseInt(v[0], totals.Commitments) && ReceiptParseInt(v[1], totals.CommittedMinutes) &&
           ReceiptParseInt(v[2], totals.LearningMilliXp) && ReceiptParseInt(v[3], totals.ConditionCostMilli) &&
           ReceiptParseInt(v[4], totals.Displaced) && ReceiptParseInt(v[5], totals.DisplacedOverlapMinutes) &&
           ReceiptParseInt(v[6], totals.DisplacedLearningMilliXp);
}

ReceiptParse ReceiptRefuse(std::string message)
{
    ReceiptParse result;
    result.Error = std::move(message);
    return result;
}

/** Reads "key=count" and refuses a count above `limit` before anything is allocated. */
bool ReceiptReadCount(std::string_view text, std::size_t& position, std::string_view key, std::size_t limit,
                      std::size_t& count)
{
    std::string_view line;
    std::string_view value;
    if (!ReceiptNextLine(text, position, line))
        return false;
    if (line.size() <= key.size() + 1 || line.substr(0, key.size()) != key || line[key.size()] != '=')
        return false;
    value = line.substr(key.size() + 1);
    std::uint64_t parsed = 0;
    if (!ReceiptParseInt(value, parsed) || parsed > limit)
        return false;
    count = static_cast<std::size_t>(parsed);
    return true;
}
} // namespace

std::string SerialiseReceipts(const ReceiptState& state)
{
    if (!IsReceiptStateValid(state))
        return {};
    std::string text(ReceiptTextHeader);
    text += "\nstate next_seq=" + std::to_string(state.NextSeq);
    text += " last_decided=" + std::to_string(state.LastDecidedAt);
    text += " folded_before=" + std::to_string(state.FoldedBeforeDay);
    text += "\nentries=" + std::to_string(state.Entries.size()) + "\n";
    for (const ReceiptEntry& entry : state.Entries)
    {
        text += "e seq=" + std::to_string(entry.Seq);
        text += " activity=" + entry.ActivityId;
        text += " decided=" + std::to_string(entry.DecidedAt);
        text += " start=" + std::to_string(entry.Start);
        text += " minutes=" + std::to_string(entry.DurationMinutes);
        text += " learning=" + std::to_string(entry.Forecast.LearningMilliXp);
        text += " condition=" + std::to_string(entry.Forecast.ConditionCostMilli);
        text += " omitted=" + std::to_string(entry.OmittedCount);
        text += " omitted_overlap=" + std::to_string(entry.OmittedOverlapMinutes);
        text += " omitted_learning=" + std::to_string(entry.OmittedLearningMilliXp);
        text += " displaced=" + std::to_string(entry.Displaced.size()) + "\n";
        for (const ReceiptDisplaced& line : entry.Displaced)
        {
            text += "d id=" + line.Id;
            text += " source=" + line.Source;
            text += " known=" + std::to_string(line.KnownAt);
            text += " start=" + std::to_string(line.Start);
            text += " minutes=" + std::to_string(line.DurationMinutes);
            text += " overlap=" + std::to_string(line.OverlapMinutes);
            text += " learning=" + std::to_string(line.Forecast.LearningMilliXp);
            text += " condition=" + std::to_string(line.Forecast.ConditionCostMilli) + "\n";
        }
    }
    text += "overflow=" + std::to_string(state.Overflow.size()) + "\n";
    for (const ReceiptOverflow& overflow : state.Overflow)
    {
        text += "o day=" + std::to_string(overflow.Day);
        ReceiptAppendTotals(text, overflow.Totals);
        text += "\n";
    }
    text += "weeks=" + std::to_string(state.Weeks.size()) + "\n";
    for (const ReceiptWeek& week : state.Weeks)
    {
        text += "w start=" + std::to_string(week.StartDay);
        ReceiptAppendTotals(text, week.Totals);
        text += "\n";
    }
    text += ReceiptTextEnd;
    text += "\n";
    return text;
}

ReceiptParse DeserialiseReceipts(std::string_view text)
{
    std::size_t position = 0;
    std::string_view line;
    std::string_view v[12];
    if (!ReceiptNextLine(text, position, line) || line != ReceiptTextHeader)
        return ReceiptRefuse("The receipts text has no recognised header.");

    ReceiptState state;
    if (!ReceiptNextLine(text, position, line) ||
        !ReceiptFields(line, "state", {"next_seq", "last_decided", "folded_before"}, v) ||
        !ReceiptParseInt(v[0], state.NextSeq) || !ReceiptParseInt(v[1], state.LastDecidedAt) ||
        !ReceiptParseInt(v[2], state.FoldedBeforeDay))
        return ReceiptRefuse("The receipts text has no valid state line.");

    std::size_t entryCount = 0;
    if (!ReceiptReadCount(text, position, "entries", ReceiptMaxEntries, entryCount))
        return ReceiptRefuse("The receipts text has no valid entries count.");
    for (std::size_t i = 0; i < entryCount; ++i)
    {
        ReceiptEntry entry;
        std::size_t displacedCount = 0;
        if (!ReceiptNextLine(text, position, line) ||
            !ReceiptFields(line, "e",
                           {"seq", "activity", "decided", "start", "minutes", "learning", "condition", "omitted",
                            "omitted_overlap", "omitted_learning", "displaced"},
                           v) ||
            !ReceiptParseInt(v[0], entry.Seq) || !ReceiptParseInt(v[2], entry.DecidedAt) ||
            !ReceiptParseInt(v[3], entry.Start) || !ReceiptParseInt(v[4], entry.DurationMinutes) ||
            !ReceiptParseInt(v[5], entry.Forecast.LearningMilliXp) ||
            !ReceiptParseInt(v[6], entry.Forecast.ConditionCostMilli) || !ReceiptParseInt(v[7], entry.OmittedCount) ||
            !ReceiptParseInt(v[8], entry.OmittedOverlapMinutes) || !ReceiptParseInt(v[9], entry.OmittedLearningMilliXp) ||
            !ReceiptParseInt(v[10], displacedCount) || displacedCount > static_cast<std::size_t>(ReceiptMaxDisplacedPerCommitment))
            return ReceiptRefuse("The receipts text has a malformed entry line.");
        entry.ActivityId = std::string(v[1]);
        for (std::size_t d = 0; d < displacedCount; ++d)
        {
            ReceiptDisplaced displaced;
            if (!ReceiptNextLine(text, position, line) ||
                !ReceiptFields(line, "d",
                               {"id", "source", "known", "start", "minutes", "overlap", "learning", "condition"}, v) ||
                !ReceiptParseInt(v[2], displaced.KnownAt) || !ReceiptParseInt(v[3], displaced.Start) ||
                !ReceiptParseInt(v[4], displaced.DurationMinutes) || !ReceiptParseInt(v[5], displaced.OverlapMinutes) ||
                !ReceiptParseInt(v[6], displaced.Forecast.LearningMilliXp) ||
                !ReceiptParseInt(v[7], displaced.Forecast.ConditionCostMilli))
                return ReceiptRefuse("The receipts text has a malformed displaced line.");
            displaced.Id = std::string(v[0]);
            displaced.Source = std::string(v[1]);
            entry.Displaced.push_back(std::move(displaced));
        }
        state.Entries.push_back(std::move(entry));
    }

    std::size_t overflowCount = 0;
    if (!ReceiptReadCount(text, position, "overflow", ReceiptMaxOverflowDays, overflowCount))
        return ReceiptRefuse("The receipts text has no valid overflow count.");
    for (std::size_t i = 0; i < overflowCount; ++i)
    {
        ReceiptOverflow overflow;
        if (!ReceiptNextLine(text, position, line) ||
            !ReceiptFields(line, "o",
                           {"day", "commitments", "committed_minutes", "learning", "condition", "displaced", "overlap",
                            "displaced_learning"},
                           v) ||
            !ReceiptParseInt(v[0], overflow.Day) || !ReceiptReadTotals(v + 1, overflow.Totals))
            return ReceiptRefuse("The receipts text has a malformed overflow line.");
        state.Overflow.push_back(overflow);
    }

    std::size_t weekCount = 0;
    if (!ReceiptReadCount(text, position, "weeks", ReceiptMaxWeeks, weekCount))
        return ReceiptRefuse("The receipts text has no valid weeks count.");
    for (std::size_t i = 0; i < weekCount; ++i)
    {
        ReceiptWeek week;
        if (!ReceiptNextLine(text, position, line) ||
            !ReceiptFields(line, "w",
                           {"start", "commitments", "committed_minutes", "learning", "condition", "displaced",
                            "overlap", "displaced_learning"},
                           v) ||
            !ReceiptParseInt(v[0], week.StartDay) || !ReceiptReadTotals(v + 1, week.Totals))
            return ReceiptRefuse("The receipts text has a malformed week line.");
        state.Weeks.push_back(week);
    }

    if (!ReceiptNextLine(text, position, line) || line != ReceiptTextEnd)
        return ReceiptRefuse("The receipts text is truncated or has no end line.");
    if (position != text.size())
        return ReceiptRefuse("The receipts text has data after the end line.");
    if (!IsReceiptStateValid(state))
        return ReceiptRefuse("The receipts text describes an invalid state.");

    ReceiptParse result;
    result.State = std::move(state);
    return result;
}
} // namespace PerfectWorld
