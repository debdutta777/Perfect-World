#include "Conversation.h"

#include <charconv>
#include <utility>

// The text form of a ConversationState. Integers and stable ids only; the parser is strict, so a truncated or edited
// text is refused rather than half read.
namespace PerfectWorld
{
namespace
{
constexpr std::string_view ConversationHeader = "pw.conversation/1";
constexpr std::string_view ConversationEnd = "end";

bool NextLine(std::string_view text, std::size_t& position, std::string_view& line)
{
    const std::size_t newline = text.find('\n', position);
    if (newline == std::string_view::npos)
        return false; // no newline left: what a truncated text looks like
    line = text.substr(position, newline - position);
    position = newline + 1;
    return true;
}

/** "key=value"; the value may be empty. */
bool Field(std::string_view text, std::size_t& position, std::string_view key, std::string_view& value)
{
    std::string_view line;
    if (!NextLine(text, position, line) || line.size() <= key.size() || line.substr(0, key.size()) != key || line[key.size()] != '=')
        return false;
    value = line.substr(key.size() + 1);
    return true;
}

/** Canonical non-negative decimal only: no sign, no leading zeros, nothing trailing. */
bool ParseCount(std::string_view text, std::size_t& out)
{
    if (text.empty() || text.size() > 4 || (text.size() > 1 && text.front() == '0'))
        return false;
    for (const char c : text)
    {
        if (c < '0' || c > '9')
            return false;
    }
    std::size_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc() || end != text.data() + text.size())
        return false;
    out = value;
    return true;
}

bool ReadList(std::string_view text, std::size_t& position, std::string_view countKey, std::string_view itemKey, std::size_t maxCount,
              std::vector<std::string>& out)
{
    std::string_view value;
    std::size_t count = 0;
    if (!Field(text, position, countKey, value) || !ParseCount(value, count) || count > maxCount)
        return false;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!Field(text, position, itemKey, value) || value.empty())
            return false;
        out.emplace_back(value);
    }
    return true;
}
} // namespace

std::string SerialiseConversationState(const ConversationState& state)
{
    if (!IsConversationStateValid(state))
        return {};
    std::string text(ConversationHeader);
    text += "\nopen=" + std::string(state.Open ? "1" : "0");
    text += "\nperson=" + state.PersonId;
    text += "\nnode=" + state.NodeId;
    text += "\ntaken=" + std::to_string(state.TakenChoices.size());
    for (const std::string& id : state.TakenChoices)
        text += "\nchoice=" + id;
    text += "\nfacts=" + std::to_string(state.Facts.size());
    for (const std::string& id : state.Facts)
        text += "\nfact=" + id;
    text += "\n";
    text += ConversationEnd;
    text += "\n";
    return text;
}

ConversationStateParse DeserialiseConversationState(std::string_view text)
{
    ConversationStateParse result;
    const auto refuse = [&](const char* message) {
        result.Error = message;
        result.State = ConversationState{};
        return result;
    };
    std::size_t position = 0;
    std::string_view line;
    std::string_view value;
    if (!NextLine(text, position, line) || line != ConversationHeader)
        return refuse("The conversation text has no recognised header.");
    ConversationState state;
    if (!Field(text, position, "open", value) || (value != "0" && value != "1"))
        return refuse("The conversation text has no valid open line.");
    state.Open = value == "1";
    if (!Field(text, position, "person", value))
        return refuse("The conversation text has no person line.");
    state.PersonId = std::string(value);
    if (!Field(text, position, "node", value))
        return refuse("The conversation text has no node line.");
    state.NodeId = std::string(value);
    if (!ReadList(text, position, "taken", "choice", 64, state.TakenChoices))
        return refuse("The conversation text has an invalid taken-choices list.");
    if (!ReadList(text, position, "facts", "fact", 64, state.Facts))
        return refuse("The conversation text has an invalid facts list.");
    if (!NextLine(text, position, line) || line != ConversationEnd)
        return refuse("The conversation text is truncated or has no end line.");
    if (position != text.size())
        return refuse("The conversation text has data after the end line.");
    if (!IsConversationStateValid(state))
        return refuse("The conversation text describes an inconsistent state.");
    result.State = std::move(state);
    return result;
}
} // namespace PerfectWorld
