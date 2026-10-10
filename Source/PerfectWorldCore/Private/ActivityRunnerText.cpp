#include "Activities.h"

#include <charconv>

// The text form of ActivityRunnerState (ROADMAP M2-03): what a save taken
// mid-activity carries. Integers and one stable ID only; the parser is strict,
// so a truncated or edited save is refused rather than half-read.
namespace PerfectWorld
{
namespace
{
constexpr std::string_view ActivityTextHeader = "pw.activity_runner/1";
constexpr std::string_view ActivityTextEnd = "end";
constexpr std::string_view ActivityTextIdle = "-";
constexpr std::size_t ActivityMaxIdLength = 128;

bool ActivityIdWellFormed(std::string_view id)
{
    if (id.empty() || id.size() > ActivityMaxIdLength)
        return false;
    for (const char c : id)
    {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
        if (!ok)
            return false;
    }
    return true;
}

/** The next line without its newline. False when no newline remains, which is what a truncated text looks like. */
bool ActivityNextLine(std::string_view text, std::size_t& position, std::string_view& line)
{
    const std::size_t newline = text.find('\n', position);
    if (newline == std::string_view::npos)
        return false;
    line = text.substr(position, newline - position);
    position = newline + 1;
    return true;
}

/** The value of a "key=value" line, or false when the key differs. */
bool ActivityKeyValue(std::string_view line, std::string_view key, std::string_view& value)
{
    if (line.size() <= key.size() || line.substr(0, key.size()) != key || line[key.size()] != '=')
        return false;
    value = line.substr(key.size() + 1);
    return true;
}

/** Canonical decimal only: no sign other than '-', no '+', no leading zeros, nothing trailing. */
template <typename Int>
bool ActivityParseInt(std::string_view text, Int& out)
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

ActivityRunnerParse ActivityRefuseText(std::string message)
{
    ActivityRunnerParse result;
    result.Error = std::move(message);
    return result;
}
} // namespace

bool IsActivityRunnerStateValid(const ActivityRunnerState& state)
{
    if (state.Instance < state.AppliedInstance || state.Instance - state.AppliedInstance > 1)
        return false;
    const bool running = state.Instance > state.AppliedInstance;
    if (running)
        return ActivityIdWellFormed(state.ActivityId) && state.StartedAt >= 0;
    return state.ActivityId.empty() && state.StartedAt == 0;
}

std::string SerialiseActivityRunner(const ActivityRunnerState& state)
{
    if (!IsActivityRunnerStateValid(state))
        return {};
    std::string text(ActivityTextHeader);
    text += "\ninstance=" + std::to_string(state.Instance);
    text += "\napplied=" + std::to_string(state.AppliedInstance);
    text += "\nstarted_at=" + std::to_string(state.StartedAt);
    text += "\nactivity=" + (state.Active() ? state.ActivityId : std::string(ActivityTextIdle));
    text += "\n";
    text += ActivityTextEnd;
    text += "\n";
    return text;
}

ActivityRunnerParse DeserialiseActivityRunner(std::string_view text)
{
    std::size_t position = 0;
    std::string_view line;
    if (!ActivityNextLine(text, position, line) || line != ActivityTextHeader)
        return ActivityRefuseText("The activity runner text has no recognised header.");

    ActivityRunnerState state;
    std::string_view value;
    if (!ActivityNextLine(text, position, line) || !ActivityKeyValue(line, "instance", value) ||
        !ActivityParseInt(value, state.Instance))
        return ActivityRefuseText("The activity runner text has no valid instance line.");
    if (!ActivityNextLine(text, position, line) || !ActivityKeyValue(line, "applied", value) ||
        !ActivityParseInt(value, state.AppliedInstance))
        return ActivityRefuseText("The activity runner text has no valid applied line.");
    if (!ActivityNextLine(text, position, line) || !ActivityKeyValue(line, "started_at", value) ||
        !ActivityParseInt(value, state.StartedAt))
        return ActivityRefuseText("The activity runner text has no valid started_at line.");
    if (!ActivityNextLine(text, position, line) || !ActivityKeyValue(line, "activity", value))
        return ActivityRefuseText("The activity runner text has no activity line.");
    if (value != ActivityTextIdle)
        state.ActivityId = std::string(value);
    if (!ActivityNextLine(text, position, line) || line != ActivityTextEnd)
        return ActivityRefuseText("The activity runner text is truncated or has no end line.");
    if (position != text.size())
        return ActivityRefuseText("The activity runner text has data after the end line.");
    if (!IsActivityRunnerStateValid(state))
        return ActivityRefuseText("The activity runner text describes an inconsistent state.");

    ActivityRunnerParse result;
    result.State = std::move(state);
    return result;
}
} // namespace PerfectWorld
