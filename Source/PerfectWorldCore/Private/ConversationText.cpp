#include "Conversation.h"

#include <algorithm>
#include <charconv>
#include <limits>
#include <utility>

// Two things that read a catalogue and a ConversationState as data: the validity of a whole catalogue, and the text form
// of a state ("pw.conversation/2", reading "/1" too). Integers and stable ids only; the parser is strict, so a truncated
// or edited text is refused rather than half read.
namespace PerfectWorld
{
namespace
{
constexpr std::string_view ConversationHeaderV1 = "pw.conversation/1";
constexpr std::string_view ConversationHeaderV2 = "pw.conversation/2";
constexpr std::string_view ConversationEnd = "end";

// What "pw.conversation/1" could contain: three once-choices and two facts, each fact tied to its choice.
constexpr std::string_view LegacyTalkRota = "choice.household_adult.talk_rota";
constexpr std::string_view LegacyKeepRota = "choice.household_adult.keep_rota";
constexpr std::string_view LegacyRevisitRota = "choice.household_adult.revisit_rota";

bool StartsWith(std::string_view text, std::string_view prefix) noexcept
{
    return text.substr(0, prefix.size()) == prefix;
}

bool SameChoice(const ConversationChoiceDef& a, const ConversationChoiceDef& b) noexcept
{
    return a.Id == b.Id && a.Text == b.Text && a.CostMinutes == b.CostMinutes && a.bOnceEver == b.bOnceEver && a.SetsFact == b.SetsFact &&
           a.ClearsFact == b.ClearsFact && a.NextNode == b.NextNode && a.RequiresFact == b.RequiresFact && a.ForbidsFact == b.ForbidsFact;
}

bool KnownOrEmpty(const ConversationCatalogue& catalogue, std::string_view factId) noexcept
{
    return factId.empty() || FindConversationFact(catalogue, factId) != nullptr;
}

bool FactsWellFormed(const ConversationCatalogue& catalogue) noexcept
{
    for (std::size_t i = 0; i < catalogue.Facts.size(); ++i)
    {
        const ConversationFactDef& fact = catalogue.Facts[i];
        if (!StartsWith(fact.Id, "fact.") || FindConversationFact(catalogue, fact.Id) != &fact || fact.Implies == fact.Id ||
            !KnownOrEmpty(catalogue, fact.Implies))
            return false;
        std::string_view next = fact.Implies; // the chain must end
        for (std::size_t step = 0; !next.empty(); ++step)
        {
            if (step > catalogue.Facts.size() || next == fact.Id)
                return false;
            next = FindConversationFact(catalogue, next)->Implies;
        }
        if (!fact.Group.empty())
        {
            for (const ConversationFactDef& other : catalogue.Facts)
            {
                if (other.Implies == fact.Id)
                    return false; // an exclusive-group fact cannot be implied: it could be set without clearing its group
            }
        }
    }
    return true;
}

bool IsImplied(const ConversationCatalogue& catalogue, std::string_view factId) noexcept
{
    for (const ConversationFactDef& other : catalogue.Facts)
    {
        if (other.Implies == factId)
            return true;
    }
    return false;
}

/** Whether a node is the next node of a choice. */
bool IsTargeted(const ConversationCatalogue& catalogue, std::string_view nodeId) noexcept
{
    for (const ConversationNodeDef& node : catalogue.Nodes)
    {
        for (std::size_t i = 0; i < node.ChoiceCount; ++i)
        {
            if (node.Choices[i].NextNode == nodeId)
                return true;
        }
    }
    return false;
}

bool NodeWellFormed(const ConversationCatalogue& catalogue, const ConversationNodeDef& node) noexcept
{
    if (!StartsWith(node.Id, "node.") || !StartsWith(node.PersonId, "person.") || node.Line.empty() ||
        static_cast<std::size_t>(node.Availability) >= PersonAvailabilityCount || FindConversationNode(catalogue, node.Id) != &node)
        return false;
    if (node.FromMinute < 0 || node.FromMinute >= ConversationMinutesPerDay || node.ToMinute < 0 || node.ToMinute > ConversationMinutesPerDay)
        return false;
    for (std::size_t i = 0; i < ConversationMaxNodeFactConditions; ++i)
    {
        if (!KnownOrEmpty(catalogue, node.RequiresFacts[i]) || !KnownOrEmpty(catalogue, node.ForbidsFacts[i]))
            return false;
    }
    if (node.ChoiceCount > ConversationMaxAuthoredChoices)
        return false;
    if (node.bDeclines && (node.ChoiceCount != 0 || node.Availability == PersonAvailability::Awake || !node.bSelectable))
        return false;
    if (node.Availability != PersonAvailability::Awake && !node.bDeclines && node.bSelectable)
        return false;
    if (!node.bSelectable && !IsTargeted(catalogue, node.Id))
        return false; // a reply nothing leads to
    for (std::size_t i = 0; i < node.ChoiceCount; ++i)
    {
        const ConversationChoiceDef& choice = node.Choices[i];
        if (!StartsWith(choice.Id, "choice.") || choice.Id == ConversationLeaveChoiceId || choice.Text.empty() || choice.CostMinutes < 0 ||
            choice.CostMinutes > ConversationMaxCostMinutes)
            return false;
        for (std::size_t j = 0; j < i; ++j)
        {
            if (node.Choices[j].Id == choice.Id)
                return false; // unique within a node
        }
        const ConversationChoiceDef* first = FindConversationChoice(catalogue, choice.Id);
        if (first == nullptr || !SameChoice(*first, choice))
            return false; // an id offered from several nodes means the same choice everywhere
        if (!KnownOrEmpty(catalogue, choice.SetsFact) || !KnownOrEmpty(catalogue, choice.ClearsFact) ||
            !KnownOrEmpty(catalogue, choice.RequiresFact) || !KnownOrEmpty(catalogue, choice.ForbidsFact))
            return false;
        if (!choice.ClearsFact.empty() && IsImplied(catalogue, choice.ClearsFact))
            return false; // clearing it would break what implies it
        if (!choice.NextNode.empty())
        {
            const ConversationNodeDef* next = FindConversationNode(catalogue, choice.NextNode);
            if (next == nullptr || next->PersonId != node.PersonId)
                return false;
        }
    }
    return true;
}

/** Every fact must be reachable: some choice sets it, or sets a fact that implies it. */
bool EveryFactSettable(const ConversationCatalogue& catalogue) noexcept
{
    for (const ConversationFactDef& fact : catalogue.Facts)
    {
        bool settable = false;
        for (const ConversationNodeDef& node : catalogue.Nodes)
        {
            for (std::size_t i = 0; i < node.ChoiceCount && !settable; ++i)
            {
                std::string_view reached = node.Choices[i].SetsFact;
                for (std::size_t step = 0; !reached.empty() && step <= catalogue.Facts.size() && !settable; ++step)
                {
                    settable = reached == fact.Id;
                    reached = FindConversationFact(catalogue, reached)->Implies;
                }
            }
        }
        if (!settable)
            return false;
    }
    return true;
}

/** The facts a person's selectable nodes look at. False when there are too many to sweep. */
bool RelevantFacts(const ConversationCatalogue& catalogue, std::string_view personId, std::vector<std::string_view>& out)
{
    for (const ConversationNodeDef& node : catalogue.Nodes)
    {
        if (node.PersonId != personId || !node.bSelectable)
            continue;
        for (std::size_t i = 0; i < ConversationMaxNodeFactConditions; ++i)
        {
            for (const std::string_view fact : {node.RequiresFacts[i], node.ForbidsFacts[i]})
            {
                if (!fact.empty() && std::find(out.begin(), out.end(), fact) == out.end())
                    out.push_back(fact);
            }
        }
    }
    return out.size() <= ConversationMaxRelevantFacts;
}

/** Could a state hold exactly this subset of the relevant facts? Implications must hold (where they land on a relevant fact) and groups stay exclusive. */
bool SubsetConsistent(const ConversationCatalogue& catalogue, const std::vector<std::string_view>& relevant, std::uint32_t mask) noexcept
{
    const auto inSubset = [&](std::string_view id) {
        for (std::size_t i = 0; i < relevant.size(); ++i)
        {
            if (relevant[i] == id)
                return ((mask >> i) & 1u) != 0;
        }
        return true; // not looked at by any node: assume it can be present, so it constrains nothing
    };
    for (std::size_t i = 0; i < relevant.size(); ++i)
    {
        if (((mask >> i) & 1u) == 0)
            continue;
        const ConversationFactDef* fact = FindConversationFact(catalogue, relevant[i]);
        for (std::string_view next = fact->Implies; !next.empty(); next = FindConversationFact(catalogue, next)->Implies)
        {
            if (!inSubset(next))
                return false;
        }
        for (std::size_t j = i + 1; j < relevant.size() && !fact->Group.empty(); ++j)
        {
            if (((mask >> j) & 1u) != 0 && FindConversationFact(catalogue, relevant[j])->Group == fact->Group)
                return false;
        }
    }
    return true;
}

/** Every availability, every segment of the day and every consistent set of facts selects exactly one node for this person. */
bool PersonCovered(const ConversationCatalogue& catalogue, std::string_view personId)
{
    std::vector<std::string_view> relevant;
    if (!RelevantFacts(catalogue, personId, relevant))
        return false;
    std::vector<std::int32_t> starts{0}; // every window edge is a breakpoint, so one minute per segment tests the segment
    for (const ConversationNodeDef& node : catalogue.Nodes)
    {
        if (node.PersonId == personId && node.bSelectable)
        {
            starts.push_back(node.FromMinute);
            if (node.ToMinute < ConversationMinutesPerDay)
                starts.push_back(node.ToMinute);
        }
    }
    std::sort(starts.begin(), starts.end());
    starts.erase(std::unique(starts.begin(), starts.end()), starts.end());
    for (std::uint32_t mask = 0; mask < (1u << relevant.size()); ++mask)
    {
        if (!SubsetConsistent(catalogue, relevant, mask))
            continue;
        ConversationState state;
        for (std::size_t i = 0; i < relevant.size(); ++i)
        {
            if (((mask >> i) & 1u) != 0)
                state.Facts.emplace_back(relevant[i]);
        }
        std::sort(state.Facts.begin(), state.Facts.end());
        for (std::size_t a = 0; a < PersonAvailabilityCount; ++a)
        {
            for (const std::int32_t minute : starts)
            {
                int matches = 0;
                for (const ConversationNodeDef& node : catalogue.Nodes)
                    matches += (node.PersonId == personId && ConversationNodeServes(node, minute, state, static_cast<PersonAvailability>(a))) ? 1 : 0;
                if (matches != 1)
                    return false;
            }
        }
    }
    return true;
}

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

/** Canonical non-negative decimal only: no sign, no leading zeros, nothing trailing, at most maxDigits digits. */
bool ParseNumber(std::string_view text, std::size_t maxDigits, std::uint64_t& out)
{
    if (text.empty() || text.size() > maxDigits || (text.size() > 1 && text.front() == '0'))
        return false;
    for (const char c : text)
    {
        if (c < '0' || c > '9')
            return false;
    }
    std::uint64_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc() || end != text.data() + text.size())
        return false;
    out = value;
    return true;
}

bool ReadCount(std::string_view text, std::size_t& position, std::string_view countKey, std::size_t maxCount, std::size_t& count)
{
    std::string_view value;
    std::uint64_t parsed = 0;
    if (!Field(text, position, countKey, value) || !ParseNumber(value, 4, parsed) || parsed > maxCount)
        return false;
    count = static_cast<std::size_t>(parsed);
    return true;
}

bool ReadList(std::string_view text, std::size_t& position, std::string_view countKey, std::string_view itemKey, std::size_t maxCount,
              std::vector<std::string>& out)
{
    std::size_t count = 0;
    std::string_view value;
    if (!ReadCount(text, position, countKey, maxCount, count))
        return false;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!Field(text, position, itemKey, value) || value.empty())
            return false;
        out.emplace_back(value);
    }
    return true;
}

bool ReadReceipts(std::string_view text, std::size_t& position, std::vector<ConversationReceipt>& out)
{
    std::size_t count = 0;
    std::string_view value;
    if (!ReadCount(text, position, "receipts", 64, count))
        return false;
    for (std::size_t i = 0; i < count; ++i)
    {
        std::uint64_t session = 0;
        if (!Field(text, position, "receipt", value))
            return false;
        const std::size_t at = value.rfind('@');
        if (at == std::string_view::npos || at == 0 || !ParseNumber(value.substr(at + 1), 10, session) ||
            session > std::numeric_limits<std::uint32_t>::max())
            return false;
        out.push_back(ConversationReceipt{std::string(value.substr(0, at)), static_cast<std::uint32_t>(session)});
    }
    return true;
}

bool Contains(const std::vector<std::string>& ids, std::string_view id) noexcept
{
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

/** The taken and fact lines of a "/1" text, as /1 could have written them. */
bool LegacyConsistent(const ConversationState& state) noexcept
{
    const bool ascending = std::is_sorted(state.TakenChoices.begin(), state.TakenChoices.end()) &&
                           std::adjacent_find(state.TakenChoices.begin(), state.TakenChoices.end()) == state.TakenChoices.end() &&
                           std::is_sorted(state.Facts.begin(), state.Facts.end()) &&
                           std::adjacent_find(state.Facts.begin(), state.Facts.end()) == state.Facts.end();
    if (!ascending)
        return false;
    for (const std::string& taken : state.TakenChoices)
    {
        if (taken != LegacyTalkRota && taken != LegacyKeepRota && taken != LegacyRevisitRota)
            return false;
    }
    for (const std::string& fact : state.Facts)
    {
        if (fact != FactEveningRotaAgreed && fact != FactEveningRotaKept)
            return false;
    }
    return Contains(state.Facts, FactEveningRotaAgreed) == Contains(state.TakenChoices, LegacyTalkRota) &&
           Contains(state.Facts, FactEveningRotaKept) == Contains(state.TakenChoices, LegacyKeepRota) &&
           (!Contains(state.Facts, FactEveningRotaKept) || Contains(state.Facts, FactEveningRotaAgreed));
}
} // namespace

bool ValidateConversationCatalogue(const ConversationCatalogue& catalogue) noexcept
{
    if (catalogue.Nodes.empty() || !FactsWellFormed(catalogue))
        return false;
    std::vector<std::string_view> persons;
    for (const ConversationNodeDef& node : catalogue.Nodes)
    {
        if (!NodeWellFormed(catalogue, node))
            return false;
        if (std::find(persons.begin(), persons.end(), node.PersonId) == persons.end())
            persons.push_back(node.PersonId);
    }
    if (!EveryFactSettable(catalogue))
        return false; // a fact nothing can set
    for (const std::string_view person : persons)
    {
        if (!PersonCovered(catalogue, person))
            return false;
    }
    return true;
}

bool ValidateConversationCatalogue() noexcept
{
    return ValidateConversationCatalogue(DefaultConversationCatalogue());
}

std::string SerialiseConversationState(const ConversationCatalogue& catalogue, const ConversationState& state)
{
    if (!IsConversationStateValid(catalogue, state))
        return {};
    std::string text(ConversationHeaderV2);
    text += "\nopen=" + std::string(state.Open ? "1" : "0");
    text += "\nperson=" + state.PersonId;
    text += "\nnode=" + state.NodeId;
    text += "\nsessions=" + std::to_string(state.SessionCount);
    text += "\ntaken=" + std::to_string(state.TakenChoices.size());
    for (const std::string& id : state.TakenChoices)
        text += "\nchoice=" + id;
    text += "\nreceipts=" + std::to_string(state.Receipts.size());
    for (const ConversationReceipt& receipt : state.Receipts)
        text += "\nreceipt=" + receipt.ChoiceId + "@" + std::to_string(receipt.Session);
    text += "\nfacts=" + std::to_string(state.Facts.size());
    for (const std::string& id : state.Facts)
        text += "\nfact=" + id;
    text += "\n";
    text += ConversationEnd;
    text += "\n";
    return text;
}

std::string SerialiseConversationState(const ConversationState& state)
{
    return SerialiseConversationState(DefaultConversationCatalogue(), state);
}

ConversationStateParse DeserialiseConversationState(const ConversationCatalogue& catalogue, std::string_view text)
{
    ConversationStateParse result;
    const auto refuse = [&](const char* message) {
        result.Error = message;
        result.State = ConversationState{};
        result.bMigrated = false;
        return result;
    };
    std::size_t position = 0;
    std::string_view line;
    std::string_view value;
    if (!NextLine(text, position, line) || (line != ConversationHeaderV1 && line != ConversationHeaderV2))
        return refuse("The conversation text has no recognised header.");
    const bool legacy = line == ConversationHeaderV1;
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
    if (!legacy)
    {
        std::uint64_t sessions = 0;
        if (!Field(text, position, "sessions", value) || !ParseNumber(value, 10, sessions) || sessions > std::numeric_limits<std::uint32_t>::max())
            return refuse("The conversation text has no valid sessions line.");
        state.SessionCount = static_cast<std::uint32_t>(sessions);
    }
    if (!ReadList(text, position, "taken", "choice", 64, state.TakenChoices))
        return refuse("The conversation text has an invalid taken-choices list.");
    if (!legacy && !ReadReceipts(text, position, state.Receipts))
        return refuse("The conversation text has an invalid receipts list.");
    if (!ReadList(text, position, "facts", "fact", 64, state.Facts))
        return refuse("The conversation text has an invalid facts list.");
    if (!NextLine(text, position, line) || line != ConversationEnd)
        return refuse("The conversation text is truncated or has no end line.");
    if (position != text.size())
        return refuse("The conversation text has data after the end line.");
    if (legacy)
    {
        // Honest migration: keep what the player agreed and kept; drop the once-only record (a cost is now guarded by a
        // per-session receipt, a fact by itself); the agreement stays unnamed; a session left open at a line that no longer
        // exists is closed, which is free.
        if (!LegacyConsistent(state))
            return refuse("The old conversation text is not one the old version could have written.");
        state.TakenChoices.clear();
        state.SessionCount = state.Open ? 1 : 0;
        if (state.Open && FindConversationNode(catalogue, state.NodeId) == nullptr)
        {
            state.Open = false;
            state.PersonId.clear();
            state.NodeId.clear();
        }
        result.bMigrated = true;
    }
    if (!IsConversationStateValid(catalogue, state))
        return refuse("The conversation text describes an inconsistent state.");
    result.State = std::move(state);
    return result;
}

ConversationStateParse DeserialiseConversationState(std::string_view text)
{
    return DeserialiseConversationState(DefaultConversationCatalogue(), text);
}
} // namespace PerfectWorld
