#include "Schedules.h"

#include "IdentityRegistry.h"
#include "ScheduleInternal.h"

// Hashes and the strict text forms for schedules, travel tables and person
// state (ROADMAP M2-06). Layouts are documented in Schedules.h.
namespace PerfectWorld
{
namespace
{
namespace Text = ScheduleInternal;

constexpr std::string_view ScheduleHeader = "pw.schedule";
constexpr std::string_view ScheduleTravelHeader = "pw.schedule_travel";
constexpr std::string_view ScheduleStateHeader = "pw.schedule_state";

// Mixed into each hash so one module's hash never equals another's of equal bytes.
constexpr std::uint64_t ScheduleHashTag = 0x50575343484544ULL;
constexpr std::uint64_t ScheduleTravelHashTag = 0x5057534348545256ULL;
constexpr std::uint64_t ScheduleStateHashTag = 0x5057534348535454ULL;

std::string_view ScheduleKindName(ScheduleDoingKind kind)
{
    return kind == ScheduleDoingKind::Activity ? "activity" : "role";
}

std::optional<ScheduleDoingKind> ScheduleParseKind(std::string_view name)
{
    if (name == "activity")
        return ScheduleDoingKind::Activity;
    if (name == "role")
        return ScheduleDoingKind::Role;
    return std::nullopt;
}

void ScheduleAppendInt(std::string& out, std::int64_t value)
{
    out += ' ';
    out += std::to_string(value);
}

void ScheduleAppendText(std::string& out, std::string_view value)
{
    out += ' ';
    out += value;
}

/** The count line "<word> <n>" at `index`, checked against the bound. */
std::optional<std::size_t> ScheduleParseCount(const std::vector<std::string_view>& lines, std::size_t index,
                                              std::string_view word, std::size_t limit)
{
    if (index >= lines.size())
        return std::nullopt;
    const auto fields = Text::SplitFields(lines[index]);
    if (!fields || fields->size() != 2 || (*fields)[0] != word)
        return std::nullopt;
    const std::optional<std::int64_t> count = Text::ParseNonNegative((*fields)[1]);
    if (!count || static_cast<std::uint64_t>(*count) > limit)
        return std::nullopt;
    return static_cast<std::size_t>(*count);
}

/** The line "<word> <value>" at `index`, split into its value. */
std::optional<std::string_view> ScheduleParseKeyed(const std::vector<std::string_view>& lines, std::size_t index,
                                                   std::string_view word)
{
    if (index >= lines.size())
        return std::nullopt;
    const auto fields = Text::SplitFields(lines[index]);
    if (!fields || fields->size() != 2 || (*fields)[0] != word)
        return std::nullopt;
    return (*fields)[1];
}

/** "<header> <version>" on the first line. */
bool ScheduleHeaderMatches(const std::vector<std::string_view>& lines, std::string_view header)
{
    const auto fields = Text::SplitFields(lines[0]);
    return fields && fields->size() == 2 && (*fields)[0] == header && (*fields)[1] == std::to_string(ScheduleFormatVersion);
}

/** The closing "end <hash>" line must be the last and must equal the recomputed hash. */
bool ScheduleEndMatches(const std::vector<std::string_view>& lines, std::size_t expectedIndex, std::uint64_t hash)
{
    if (lines.size() != expectedIndex + 1)
        return false;
    const std::optional<std::string_view> value = ScheduleParseKeyed(lines, expectedIndex, "end");
    if (!value)
        return false;
    const std::optional<std::uint64_t> parsed = Text::ParseUnsigned(*value);
    return parsed && *parsed == hash;
}

void ScheduleHashEntry(Text::HashWriter& out, const ScheduleEntry& entry)
{
    out.Unsigned(entry.WeekdayMask);
    out.Signed(entry.StartMinute);
    out.Signed(entry.EndMinute);
    out.Signed(entry.Priority);
    out.Unsigned(static_cast<std::uint8_t>(entry.Kind));
    out.Text(entry.Zone);
    out.Text(entry.Doing);
}

void ScheduleHashException(Text::HashWriter& out, const ScheduleException& exception)
{
    out.Text(exception.Id);
    out.Signed(exception.From);
    out.Signed(exception.To);
    out.Signed(exception.Priority);
    out.Unsigned(static_cast<std::uint8_t>(exception.Kind));
    out.Text(exception.Zone);
    out.Text(exception.Doing);
}
} // namespace

std::uint64_t HashPersonSchedule(const PersonSchedule& person)
{
    Text::HashWriter out;
    out.Unsigned(ScheduleHashTag);
    out.Unsigned(ScheduleFormatVersion);
    out.Text(person.PersonId);
    out.Text(person.HomeZone);
    out.Unsigned(person.Routine.size());
    for (const ScheduleEntry& entry : person.Routine)
        ScheduleHashEntry(out, entry);
    out.Unsigned(person.Exceptions.size());
    for (const ScheduleException& exception : person.Exceptions)
        ScheduleHashException(out, exception);
    return out.Result();
}

std::uint64_t HashScheduleTravel(const ScheduleTravelTable& table)
{
    Text::HashWriter out;
    out.Unsigned(ScheduleTravelHashTag);
    out.Unsigned(ScheduleFormatVersion);
    out.Unsigned(table.Legs.size());
    for (const ScheduleTravelLeg& leg : table.Legs)
    {
        out.Text(leg.From);
        out.Text(leg.To);
        out.Signed(leg.Minutes);
    }
    return out.Result();
}

std::uint64_t HashSchedulePersonState(const SchedulePersonState& state)
{
    Text::HashWriter out;
    out.Unsigned(ScheduleStateHashTag);
    out.Unsigned(ScheduleFormatVersion);
    out.Text(state.PersonId);
    out.Signed(state.At);
    out.Unsigned(state.Tally.size());
    for (const auto& [id, tally] : state.Tally)
    {
        out.Text(id);
        out.Signed(tally.Sessions);
        out.Signed(tally.Minutes);
    }
    return out.Result();
}

std::string SerialisePersonSchedule(const PersonSchedule& person)
{
    std::string out(ScheduleHeader);
    out += ' ' + std::to_string(ScheduleFormatVersion) + "\nperson ";
    out += person.PersonId;
    out += "\nhome ";
    out += person.HomeZone;
    out += "\nroutine " + std::to_string(person.Routine.size()) + '\n';
    for (const ScheduleEntry& entry : person.Routine)
    {
        out += "entry";
        ScheduleAppendInt(out, entry.WeekdayMask);
        ScheduleAppendInt(out, entry.StartMinute);
        ScheduleAppendInt(out, entry.EndMinute);
        ScheduleAppendInt(out, entry.Priority);
        ScheduleAppendText(out, ScheduleKindName(entry.Kind));
        ScheduleAppendText(out, entry.Zone);
        ScheduleAppendText(out, entry.Doing);
        out += '\n';
    }
    out += "exceptions " + std::to_string(person.Exceptions.size()) + '\n';
    for (const ScheduleException& exception : person.Exceptions)
    {
        out += "exception";
        ScheduleAppendText(out, exception.Id);
        ScheduleAppendInt(out, exception.From);
        ScheduleAppendInt(out, exception.To);
        ScheduleAppendInt(out, exception.Priority);
        ScheduleAppendText(out, ScheduleKindName(exception.Kind));
        ScheduleAppendText(out, exception.Zone);
        ScheduleAppendText(out, exception.Doing);
        out += '\n';
    }
    out += "end " + std::to_string(HashPersonSchedule(person)) + '\n';
    return out;
}

std::optional<PersonSchedule> ParsePersonSchedule(std::string_view text)
{
    const auto lines = Text::SplitLines(text);
    if (!lines || lines->size() < 6 || !ScheduleHeaderMatches(*lines, ScheduleHeader))
        return std::nullopt;
    const auto personId = ScheduleParseKeyed(*lines, 1, "person");
    const auto home = ScheduleParseKeyed(*lines, 2, "home");
    const auto routineCount = ScheduleParseCount(*lines, 3, "routine", ScheduleMaxRoutineEntries);
    if (!personId || !home || !routineCount)
        return std::nullopt;

    PersonSchedule person;
    person.PersonId = std::string(*personId);
    person.HomeZone = std::string(*home);
    std::size_t index = 4;
    for (std::size_t i = 0; i < *routineCount; ++i, ++index)
    {
        if (index >= lines->size())
            return std::nullopt;
        const auto fields = Text::SplitFields((*lines)[index]);
        if (!fields || fields->size() != 8 || (*fields)[0] != "entry")
            return std::nullopt;
        const auto mask = Text::ParseNonNegative((*fields)[1]);
        const auto start = Text::ParseNonNegative((*fields)[2]);
        const auto end = Text::ParseNonNegative((*fields)[3]);
        const auto priority = Text::ParseNonNegative((*fields)[4]);
        const auto kind = ScheduleParseKind((*fields)[5]);
        if (!mask || *mask > 255 || !start || *start > MinutesPerDay || !end || *end > MinutesPerDay || !priority ||
            *priority > ScheduleMaxPriority || !kind)
            return std::nullopt;
        ScheduleEntry entry;
        entry.WeekdayMask = static_cast<std::uint8_t>(*mask);
        entry.StartMinute = static_cast<std::int32_t>(*start);
        entry.EndMinute = static_cast<std::int32_t>(*end);
        entry.Priority = static_cast<std::int32_t>(*priority);
        entry.Kind = *kind;
        entry.Zone = std::string((*fields)[6]);
        entry.Doing = std::string((*fields)[7]);
        person.Routine.push_back(std::move(entry));
    }
    const auto exceptionCount = ScheduleParseCount(*lines, index, "exceptions", ScheduleMaxExceptions);
    if (!exceptionCount)
        return std::nullopt;
    ++index;
    for (std::size_t i = 0; i < *exceptionCount; ++i, ++index)
    {
        if (index >= lines->size())
            return std::nullopt;
        const auto fields = Text::SplitFields((*lines)[index]);
        if (!fields || fields->size() != 8 || (*fields)[0] != "exception")
            return std::nullopt;
        const auto from = Text::ParseNonNegative((*fields)[2]);
        const auto to = Text::ParseNonNegative((*fields)[3]);
        const auto priority = Text::ParseNonNegative((*fields)[4]);
        const auto kind = ScheduleParseKind((*fields)[5]);
        if (!from || !to || !priority || *priority > ScheduleMaxPriority || !kind)
            return std::nullopt;
        ScheduleException exception;
        exception.Id = std::string((*fields)[1]);
        exception.From = *from;
        exception.To = *to;
        exception.Priority = static_cast<std::int32_t>(*priority);
        exception.Kind = *kind;
        exception.Zone = std::string((*fields)[6]);
        exception.Doing = std::string((*fields)[7]);
        person.Exceptions.push_back(std::move(exception));
    }
    if (!ScheduleEndMatches(*lines, index, HashPersonSchedule(person)) || !ValidatePersonSchedule(person).empty())
        return std::nullopt;
    return person;
}

std::string SerialiseScheduleTravel(const ScheduleTravelTable& table)
{
    std::string out(ScheduleTravelHeader);
    out += ' ' + std::to_string(ScheduleFormatVersion) + "\nlegs " + std::to_string(table.Legs.size()) + '\n';
    for (const ScheduleTravelLeg& leg : table.Legs)
    {
        out += "leg";
        ScheduleAppendText(out, leg.From);
        ScheduleAppendText(out, leg.To);
        ScheduleAppendInt(out, leg.Minutes);
        out += '\n';
    }
    out += "end " + std::to_string(HashScheduleTravel(table)) + '\n';
    return out;
}

std::optional<ScheduleTravelTable> ParseScheduleTravel(std::string_view text)
{
    const auto lines = Text::SplitLines(text);
    if (!lines || lines->size() < 3 || !ScheduleHeaderMatches(*lines, ScheduleTravelHeader))
        return std::nullopt;
    const auto count = ScheduleParseCount(*lines, 1, "legs", ScheduleMaxTravelLegs);
    if (!count)
        return std::nullopt;
    ScheduleTravelTable table;
    std::size_t index = 2;
    for (std::size_t i = 0; i < *count; ++i, ++index)
    {
        if (index >= lines->size())
            return std::nullopt;
        const auto fields = Text::SplitFields((*lines)[index]);
        if (!fields || fields->size() != 4 || (*fields)[0] != "leg")
            return std::nullopt;
        const auto minutes = Text::ParseNonNegative((*fields)[3]);
        if (!minutes || *minutes > ScheduleMaxTravelMinutes)
            return std::nullopt;
        table.Legs.push_back({std::string((*fields)[1]), std::string((*fields)[2]), static_cast<std::int32_t>(*minutes)});
    }
    if (!ScheduleEndMatches(*lines, index, HashScheduleTravel(table)) || !ValidateScheduleTravel(table).empty())
        return std::nullopt;
    return table;
}

std::string SerialiseSchedulePersonState(const SchedulePersonState& state)
{
    std::string out(ScheduleStateHeader);
    out += ' ' + std::to_string(ScheduleFormatVersion) + "\nperson ";
    out += state.PersonId;
    out += "\nat " + std::to_string(state.At) + "\ntally " + std::to_string(state.Tally.size()) + '\n';
    for (const auto& [id, tally] : state.Tally)
    {
        out += "activity";
        ScheduleAppendText(out, id);
        ScheduleAppendInt(out, tally.Sessions);
        ScheduleAppendInt(out, tally.Minutes);
        out += '\n';
    }
    out += "end " + std::to_string(HashSchedulePersonState(state)) + '\n';
    return out;
}

std::optional<SchedulePersonState> ParseSchedulePersonState(std::string_view text)
{
    const auto lines = Text::SplitLines(text);
    if (!lines || lines->size() < 5 || !ScheduleHeaderMatches(*lines, ScheduleStateHeader))
        return std::nullopt;
    const auto personId = ScheduleParseKeyed(*lines, 1, "person");
    const auto at = ScheduleParseKeyed(*lines, 2, "at");
    const auto count = ScheduleParseCount(*lines, 3, "tally", ScheduleMaxRoutineEntries + ScheduleMaxExceptions);
    if (!personId || !at || !count)
        return std::nullopt;
    const auto atValue = Text::ParseNonNegative(*at);
    if (!atValue || *atValue > ScheduleMaxInstant || !IsValidIdentityId(*personId))
        return std::nullopt;

    SchedulePersonState state;
    state.PersonId = std::string(*personId);
    state.At = *atValue;
    std::size_t index = 4;
    for (std::size_t i = 0; i < *count; ++i, ++index)
    {
        if (index >= lines->size())
            return std::nullopt;
        const auto fields = Text::SplitFields((*lines)[index]);
        if (!fields || fields->size() != 4 || (*fields)[0] != "activity")
            return std::nullopt;
        const auto sessions = Text::ParseNonNegative((*fields)[2]);
        const auto minutes = Text::ParseNonNegative((*fields)[3]);
        // Canonical order is strictly ascending by ID, which also refuses duplicates.
        if (!sessions || !minutes || !IsStableIdToken((*fields)[1]) || (!state.Tally.empty() && !(state.Tally.rbegin()->first < (*fields)[1])))
            return std::nullopt;
        state.Tally.emplace(std::string((*fields)[1]), ScheduleActivityTally{*sessions, *minutes});
    }
    if (!ScheduleEndMatches(*lines, index, HashSchedulePersonState(state)))
        return std::nullopt;
    return state;
}
} // namespace PerfectWorld
