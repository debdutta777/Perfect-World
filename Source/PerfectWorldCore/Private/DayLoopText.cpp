#include "DayLoop.h"

#include <charconv>
#include <initializer_list>
#include <utility>

// The pw.dayloop/1 envelope and the combined hash (ROADMAP M2-12). The module
// sections are embedded as their own text, length-prefixed, so each module's
// strict parser has the final say on its bytes. Only the canonical text parses:
// after decoding, the state is written again and must equal the input.
namespace PerfectWorld
{
namespace
{
constexpr std::uint64_t DayLoopFnvOffset = 0xcbf29ce484222325ULL;
constexpr std::uint64_t DayLoopFnvPrime = 0x100000001b3ULL;
constexpr std::string_view DayLoopNoActivity = "-";

void DayLoopFnvBytes(std::uint64_t& hash, std::string_view bytes)
{
    for (const char c : bytes)
    {
        hash ^= static_cast<unsigned char>(c);
        hash *= DayLoopFnvPrime;
    }
}

void DayLoopFnvLength(std::uint64_t& hash, std::uint64_t value)
{
    for (int i = 0; i < 8; ++i)
    {
        hash ^= (value >> (8 * i)) & 0xFFu;
        hash *= DayLoopFnvPrime;
    }
}

struct DayLoopSections
{
    std::string Core;
    std::string Runner;
    std::string Practice;
    std::string Receipts;
    std::string Attendance;
};

void DayLoopAppendKey(std::string& text, std::string_view tag, std::initializer_list<std::pair<std::string_view, std::string>> fields)
{
    text += tag;
    for (const auto& [key, value] : fields)
    {
        text += ' ';
        text += key;
        text += '=';
        text += value;
    }
    text += '\n';
}

std::string DayLoopCoreText(const DayLoopState& s)
{
    std::string t;
    DayLoopAppendKey(t, "state",
                     {{"now", std::to_string(s.Now)}, {"phase", std::string(DayPhaseName(s.Phase))},
                      {"seq", std::to_string(s.Seq)}, {"wakes", std::to_string(s.Wakes)},
                      {"planned", s.Planned ? "1" : "0"}, {"asleep", s.Asleep ? "1" : "0"},
                      {"slept_at", std::to_string(s.SleptAt)}, {"school_until", std::to_string(s.SchoolUntil)}});
    DayLoopAppendKey(t, "condition",
                     {{"energy", std::to_string(s.Condition.Energy)}, {"hunger", std::to_string(s.Condition.Hunger)},
                      {"health", std::to_string(s.Condition.Health)}, {"stress", std::to_string(s.Condition.Stress)}});
    DayLoopAppendKey(t, "last",
                     {{"kind", s.HasLastAction ? std::string(DayCommandName(s.Last.Kind)) : std::string("none")},
                      {"at", std::to_string(s.HasLastAction ? s.Last.At : 0)},
                      {"activity", s.HasLastAction && !s.Last.ActivityId.empty() ? s.Last.ActivityId
                                                                                   : std::string(DayLoopNoActivity)}});
    t += "plan=" + std::to_string(s.Plan.size()) + '\n';
    for (const DayPlanItem& item : s.Plan)
        DayLoopAppendKey(t, "p", {{"start", std::to_string(item.StartMinute)}, {"activity", item.ActivityId}});
    t += std::string("commitment=") + (s.HasCommitment ? "1" : "0") + '\n';
    if (s.HasCommitment)
    {
        const DayLoopCommitment& c = s.Commitment;
        DayLoopAppendKey(t, "c",
                         {{"activity", c.ActivityId}, {"place", c.PlaceKind}, {"decided", std::to_string(c.DecidedAt)},
                          {"alternatives", std::to_string(c.Alternatives.size())}});
        for (const ReceiptCandidate& a : c.Alternatives)
            DayLoopAppendKey(t, "a",
                             {{"id", a.Id}, {"source", a.Source}, {"known", std::to_string(a.KnownAt)},
                              {"start", std::to_string(a.Start)}, {"minutes", std::to_string(a.DurationMinutes)},
                              {"available", a.Available ? "1" : "0"},
                              {"learning", std::to_string(a.Forecast.LearningMilliXp)},
                              {"condition", std::to_string(a.Forecast.ConditionCostMilli)}});
    }
    return t;
}

bool DayLoopBuildSections(const DayLoopState& state, DayLoopSections& sections)
{
    if (!IsDayLoopStateValid(state))
        return false;
    sections.Core = DayLoopCoreText(state);
    sections.Runner = SerialiseActivityRunner(state.Runner);
    sections.Practice = SerialisePractice(state.Practice);
    sections.Receipts = SerialiseReceipts(state.Receipts);
    sections.Attendance = SerialiseAttendanceRecord(state.Attendance);
    return !sections.Runner.empty() && !sections.Practice.empty() && !sections.Receipts.empty() &&
           !sections.Attendance.empty();
}

std::uint64_t DayLoopHashSections(const DayLoopSections& sections)
{
    std::uint64_t hash = DayLoopFnvOffset;
    const std::pair<std::string_view, const std::string*> framed[] = {
        {"core", &sections.Core}, {"runner", &sections.Runner}, {"practice", &sections.Practice},
        {"receipts", &sections.Receipts}, {"attendance", &sections.Attendance}};
    for (const auto& [name, payload] : framed)
    {
        DayLoopFnvBytes(hash, name);
        DayLoopFnvLength(hash, 0);
        DayLoopFnvLength(hash, payload->size());
        DayLoopFnvBytes(hash, *payload);
    }
    return hash;
}

std::string DayLoopHex(std::uint64_t value)
{
    constexpr std::string_view digits = "0123456789abcdef";
    std::string text(16, '0');
    for (int i = 15; i >= 0; --i)
    {
        text[static_cast<std::size_t>(i)] = digits[value & 0xFu];
        value >>= 4;
    }
    return text;
}

// ---- reading ----

bool DayLoopNextLine(std::string_view text, std::size_t& position, std::string_view& line)
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
bool DayLoopParseInt(std::string_view text, Int& out)
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

bool DayLoopParseBool(std::string_view text, bool& out)
{
    if (text != "0" && text != "1")
        return false;
    out = text == "1";
    return true;
}

/** "tag k1=v1 k2=v2 ...": exactly this tag and these keys in this order, single spaces, non-empty values. */
bool DayLoopFields(std::string_view line, std::string_view tag, std::initializer_list<std::string_view> keys,
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

class DayLoopReader
{
public:
    explicit DayLoopReader(std::string_view text) : Text_(text) {}

    bool Line(std::string_view& line) { return DayLoopNextLine(Text_, Position_, line); }

    bool Fields(std::string_view tag, std::initializer_list<std::string_view> keys, std::string_view* values)
    {
        std::string_view line;
        return Line(line) && DayLoopFields(line, tag, keys, values);
    }

    /** "key=count" with the count at most `limit`. */
    bool Count(std::string_view key, std::size_t limit, std::size_t& count)
    {
        std::string_view line;
        if (!Line(line) || line.size() <= key.size() + 1 || line.substr(0, key.size()) != key || line[key.size()] != '=')
            return false;
        std::uint64_t value = 0;
        if (!DayLoopParseInt(line.substr(key.size() + 1), value) || value > limit)
            return false;
        count = static_cast<std::size_t>(value);
        return true;
    }

    /** "section <name> <bytes>" and then exactly that many bytes. */
    bool Section(std::string_view name, std::string_view& payload)
    {
        std::string_view line;
        if (!Line(line))
            return false;
        const std::string head = "section " + std::string(name) + ' ';
        if (line.substr(0, head.size()) != head)
            return false;
        std::uint64_t bytes = 0;
        if (!DayLoopParseInt(line.substr(head.size()), bytes) || bytes > Text_.size() - Position_)
            return false;
        payload = Text_.substr(Position_, static_cast<std::size_t>(bytes));
        Position_ += static_cast<std::size_t>(bytes);
        return true;
    }

    [[nodiscard]] bool AtEnd() const { return Position_ == Text_.size(); }

private:
    std::string_view Text_;
    std::size_t Position_ = 0;
};

DayLoopParse DayLoopRefuse(std::string message)
{
    DayLoopParse result;
    result.Error = std::move(message);
    return result;
}

std::optional<DayPhase> DayLoopPhaseFromName(std::string_view name)
{
    for (const DayPhase phase : {DayPhase::Plan, DayPhase::School, DayPhase::FreeBlock, DayPhase::Evening, DayPhase::Sleep})
    {
        if (DayPhaseName(phase) == name)
            return phase;
    }
    return std::nullopt;
}

bool DayLoopReadCore(DayLoopReader& in, DayLoopState& s)
{
    std::string_view v[8];
    if (!in.Fields("state", {"now", "phase", "seq", "wakes", "planned", "asleep", "slept_at", "school_until"}, v))
        return false;
    const std::optional<DayPhase> phase = DayLoopPhaseFromName(v[1]);
    if (!DayLoopParseInt(v[0], s.Now) || !phase || !DayLoopParseInt(v[2], s.Seq) || !DayLoopParseInt(v[3], s.Wakes) ||
        !DayLoopParseBool(v[4], s.Planned) || !DayLoopParseBool(v[5], s.Asleep) || !DayLoopParseInt(v[6], s.SleptAt) ||
        !DayLoopParseInt(v[7], s.SchoolUntil))
        return false;
    s.Phase = *phase;

    if (!in.Fields("condition", {"energy", "hunger", "health", "stress"}, v) ||
        !DayLoopParseInt(v[0], s.Condition.Energy) || !DayLoopParseInt(v[1], s.Condition.Hunger) ||
        !DayLoopParseInt(v[2], s.Condition.Health) || !DayLoopParseInt(v[3], s.Condition.Stress))
        return false;

    if (!in.Fields("last", {"kind", "at", "activity"}, v))
        return false;
    if (v[0] == "none")
    {
        if (v[1] != "0" || v[2] != DayLoopNoActivity)
            return false;
    }
    else
    {
        const std::optional<DayCommandKind> kind = DayCommandFromName(v[0]);
        if (!kind || !DayLoopParseInt(v[1], s.Last.At))
            return false;
        s.HasLastAction = true;
        s.Last.Kind = *kind;
        if (v[2] != DayLoopNoActivity)
            s.Last.ActivityId = std::string(v[2]);
    }

    std::size_t count = 0;
    if (!in.Count("plan", DayLoopMaxPlanItems, count))
        return false;
    for (std::size_t i = 0; i < count; ++i)
    {
        DayPlanItem item;
        if (!in.Fields("p", {"start", "activity"}, v) || !DayLoopParseInt(v[0], item.StartMinute))
            return false;
        item.ActivityId = std::string(v[1]);
        s.Plan.push_back(std::move(item));
    }

    if (!in.Count("commitment", 1, count))
        return false;
    s.HasCommitment = count == 1;
    if (!s.HasCommitment)
        return true;
    DayLoopCommitment& c = s.Commitment;
    std::string_view cv[4];
    if (!in.Fields("c", {"activity", "place", "decided", "alternatives"}, cv) || !DayLoopParseInt(cv[2], c.DecidedAt))
        return false;
    c.ActivityId = std::string(cv[0]);
    c.PlaceKind = std::string(cv[1]);
    if (!DayLoopParseInt(cv[3], count) || count > DayLoopMaxAlternatives)
        return false;
    for (std::size_t i = 0; i < count; ++i)
    {
        std::string_view av[8];
        ReceiptCandidate a;
        if (!in.Fields("a", {"id", "source", "known", "start", "minutes", "available", "learning", "condition"}, av) ||
            !DayLoopParseInt(av[2], a.KnownAt) || !DayLoopParseInt(av[3], a.Start) ||
            !DayLoopParseInt(av[4], a.DurationMinutes) || !DayLoopParseBool(av[5], a.Available) ||
            !DayLoopParseInt(av[6], a.Forecast.LearningMilliXp) || !DayLoopParseInt(av[7], a.Forecast.ConditionCostMilli))
            return false;
        a.Id = std::string(av[0]);
        a.Source = std::string(av[1]);
        c.Alternatives.push_back(std::move(a));
    }
    return true;
}
} // namespace

std::string SerialiseDayLoop(const DayLoopState& state)
{
    DayLoopSections sections;
    if (!DayLoopBuildSections(state, sections))
        return {};
    std::string text(DayLoopTextHeader);
    text += '\n';
    text += sections.Core;
    const std::pair<std::string_view, const std::string*> modules[] = {
        {"runner", &sections.Runner}, {"practice", &sections.Practice},
        {"receipts", &sections.Receipts}, {"attendance", &sections.Attendance}};
    for (const auto& [name, payload] : modules)
    {
        text += "section ";
        text += name;
        text += ' ' + std::to_string(payload->size()) + '\n';
        text += *payload;
    }
    text += "hash " + DayLoopHex(DayLoopHashSections(sections)) + "\nend\n";
    return text;
}

std::uint64_t HashDayLoop(const DayLoopState& state)
{
    DayLoopSections sections;
    return DayLoopBuildSections(state, sections) ? DayLoopHashSections(sections) : 0;
}

DayLoopParse ParseDayLoop(std::string_view text)
{
    DayLoopReader in(text);
    std::string_view line;
    if (!in.Line(line) || line != DayLoopTextHeader)
        return DayLoopRefuse("not a pw.dayloop/1 envelope");

    DayLoopState state;
    if (!DayLoopReadCore(in, state))
        return DayLoopRefuse("the core lines are truncated or malformed");

    std::string_view payload;
    if (!in.Section("runner", payload))
        return DayLoopRefuse("the runner section is truncated");
    const ActivityRunnerParse runner = DeserialiseActivityRunner(payload);
    if (!runner.Ok())
        return DayLoopRefuse("runner section: " + runner.Error);
    state.Runner = runner.State;

    if (!in.Section("practice", payload))
        return DayLoopRefuse("the practice section is truncated");
    const PracticeParse practice = DeserialisePractice(payload);
    if (!practice.Ok())
        return DayLoopRefuse("practice section: " + practice.Error);
    state.Practice = practice.State;

    if (!in.Section("receipts", payload))
        return DayLoopRefuse("the receipts section is truncated");
    const ReceiptParse receipts = DeserialiseReceipts(payload);
    if (!receipts.Ok())
        return DayLoopRefuse("receipts section: " + receipts.Error);
    state.Receipts = receipts.State;

    if (!in.Section("attendance", payload))
        return DayLoopRefuse("the attendance section is truncated");
    const std::optional<AttendanceRecord> attendance = ParseAttendanceRecord(payload);
    if (!attendance)
        return DayLoopRefuse("attendance section: the record is refused");
    state.Attendance = *attendance;

    std::string_view hashLine;
    std::string_view end;
    if (!in.Line(hashLine) || !in.Line(end) || end != "end" || !in.AtEnd())
        return DayLoopRefuse("the hash line or the end is missing, or text follows the end");
    if (!IsDayLoopStateValid(state))
        return DayLoopRefuse("the state is not valid");
    if (hashLine != "hash " + DayLoopHex(HashDayLoop(state)))
        return DayLoopRefuse("the hash does not match the sections");
    if (SerialiseDayLoop(state) != text)
        return DayLoopRefuse("the text is not the canonical form");

    DayLoopParse result;
    result.State = std::move(state);
    return result;
}

std::optional<DayLoopSlotSummary> ReadDayLoopSlotSummary(std::string_view text)
{
    DayLoopParse parsed = ParseDayLoop(text);
    if (!parsed.Ok())
        return std::nullopt;
    return SummariseDayLoop(parsed.State);
}
} // namespace PerfectWorld
