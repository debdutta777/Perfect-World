#include "Conversation.h"

#include <algorithm>
#include <limits>

// The conversation runtime: selection, opening, choosing and state validity, over any ConversationCatalogue.
// Catalogue validation and the text form are in ConversationText.cpp; the authored household content is in ConversationContent.cpp.
namespace PerfectWorld
{
namespace
{
constexpr std::array<std::string_view, PersonAvailabilityCount> AvailabilityIds{"awake", "busy", "asleep"};

bool InWindow(const ConversationNodeDef& node, std::int32_t minute) noexcept
{
    if (node.FromMinute < node.ToMinute)
        return minute >= node.FromMinute && minute < node.ToMinute;
    return minute >= node.FromMinute || minute < node.ToMinute; // wraps past midnight
}

bool FactsAllow(const ConversationNodeDef& node, const ConversationState& state) noexcept
{
    for (const std::string_view fact : node.RequiresFacts)
    {
        if (!fact.empty() && !HasConversationFact(state, fact))
            return false;
    }
    for (const std::string_view fact : node.ForbidsFacts)
    {
        if (!fact.empty() && HasConversationFact(state, fact))
            return false;
    }
    return true;
}

bool ChoiceConditionsHold(const ConversationChoiceDef& choice, const ConversationState& state) noexcept
{
    return (choice.RequiresFact.empty() || HasConversationFact(state, choice.RequiresFact)) &&
           (choice.ForbidsFact.empty() || !HasConversationFact(state, choice.ForbidsFact));
}

bool PersonKnown(const ConversationCatalogue& catalogue, std::string_view personId) noexcept
{
    for (const ConversationNodeDef& node : catalogue.Nodes)
    {
        if (node.PersonId == personId)
            return true;
    }
    return false;
}

void InsertSorted(std::vector<std::string>& ids, std::string_view id)
{
    const auto at = std::lower_bound(ids.begin(), ids.end(), id,
                                     [](const std::string& left, std::string_view right) { return std::string_view(left) < right; });
    if (at == ids.end() || *at != id)
        ids.insert(at, std::string(id));
}

bool EraseSorted(std::vector<std::string>& ids, std::string_view id)
{
    const auto at = std::lower_bound(ids.begin(), ids.end(), id,
                                     [](const std::string& left, std::string_view right) { return std::string_view(left) < right; });
    if (at == ids.end() || *at != id)
        return false;
    ids.erase(at);
    return true;
}

void EndSession(ConversationState& state)
{
    state.Open = false;
    state.PersonId.clear();
    state.NodeId.clear();
}

template <typename Items, typename Key>
bool StrictlyAscending(const Items& items, Key key) noexcept
{
    for (std::size_t i = 1; i < items.size(); ++i)
    {
        if (!(key(items[i - 1]) < key(items[i])))
            return false;
    }
    return true;
}

/** Sets a fact: clears the others of its group, then the fact, then what it implies (bounded by the fact count, so a loop cannot hang). */
void SetFact(const ConversationCatalogue& catalogue, ConversationState& state, std::string_view factId, std::string_view& cleared)
{
    std::string_view current = factId;
    for (std::size_t step = 0; step <= catalogue.Facts.size() && !current.empty(); ++step)
    {
        const ConversationFactDef* def = FindConversationFact(catalogue, current);
        if (def == nullptr)
            return;
        if (!def->Group.empty())
        {
            for (const ConversationFactDef& other : catalogue.Facts)
            {
                if (other.Id != def->Id && other.Group == def->Group && EraseSorted(state.Facts, other.Id) && cleared.empty())
                    cleared = other.Id;
            }
        }
        InsertSorted(state.Facts, def->Id);
        current = def->Implies;
    }
}
} // namespace

std::string_view PersonAvailabilityId(PersonAvailability availability) noexcept
{
    const std::size_t index = static_cast<std::size_t>(availability);
    return index < PersonAvailabilityCount ? AvailabilityIds[index] : std::string_view{};
}

std::string_view ConversationErrorCode(ConversationError error) noexcept
{
    switch (error)
    {
    case ConversationError::None: return "none";
    case ConversationError::UnknownPerson: return "unknown_person";
    case ConversationError::InvalidTime: return "invalid_time";
    case ConversationError::AlreadyOpen: return "already_open";
    case ConversationError::NotOpen: return "not_open";
    case ConversationError::StaleChoice: return "stale_choice";
    case ConversationError::AlreadyTaken: return "already_taken";
    case ConversationError::AlreadyCharged: return "already_charged";
    case ConversationError::SessionLimit: return "session_limit";
    }
    return {};
}

const ConversationNodeDef* FindConversationNode(const ConversationCatalogue& catalogue, std::string_view nodeId) noexcept
{
    for (const ConversationNodeDef& node : catalogue.Nodes)
    {
        if (node.Id == nodeId)
            return &node;
    }
    return nullptr;
}

const ConversationChoiceDef* FindConversationChoice(const ConversationCatalogue& catalogue, std::string_view choiceId) noexcept
{
    for (const ConversationNodeDef& node : catalogue.Nodes)
    {
        for (std::size_t i = 0; i < node.ChoiceCount; ++i)
        {
            if (node.Choices[i].Id == choiceId)
                return &node.Choices[i];
        }
    }
    return nullptr;
}

const ConversationFactDef* FindConversationFact(const ConversationCatalogue& catalogue, std::string_view factId) noexcept
{
    for (const ConversationFactDef& fact : catalogue.Facts)
    {
        if (fact.Id == factId)
            return &fact;
    }
    return nullptr;
}

const ConversationNodeDef* FindConversationNode(std::string_view nodeId) noexcept
{
    return FindConversationNode(DefaultConversationCatalogue(), nodeId);
}

const ConversationChoiceDef* FindConversationChoice(std::string_view choiceId) noexcept
{
    return FindConversationChoice(DefaultConversationCatalogue(), choiceId);
}

std::span<const ConversationNodeDef> ConversationNodes() noexcept
{
    return DefaultConversationCatalogue().Nodes;
}

bool HasConversationFact(const ConversationState& state, std::string_view factId) noexcept
{
    return std::binary_search(state.Facts.begin(), state.Facts.end(), factId,
                              [](std::string_view left, std::string_view right) { return left < right; });
}

bool WasChoiceTaken(const ConversationState& state, std::string_view choiceId) noexcept
{
    return std::binary_search(state.TakenChoices.begin(), state.TakenChoices.end(), choiceId,
                              [](std::string_view left, std::string_view right) { return left < right; });
}

bool WasChoiceChargedThisSession(const ConversationState& state, std::string_view choiceId) noexcept
{
    for (const ConversationReceipt& receipt : state.Receipts)
    {
        if (receipt.ChoiceId == choiceId && receipt.Session == state.SessionCount)
            return true;
    }
    return false;
}

bool ConversationNodeServes(const ConversationNodeDef& node, std::int32_t minuteOfDay, const ConversationState& state,
                            PersonAvailability availability) noexcept
{
    return node.bSelectable && node.Availability == availability && InWindow(node, minuteOfDay) && FactsAllow(node, state);
}

const ConversationNodeDef* SelectConversationNode(const ConversationCatalogue& catalogue, std::string_view personId, std::int32_t minuteOfDay,
                                                  const ConversationState& state, PersonAvailability availability) noexcept
{
    if (minuteOfDay < 0 || minuteOfDay >= ConversationMinutesPerDay)
        return nullptr;
    for (const ConversationNodeDef& node : catalogue.Nodes)
    {
        if (node.PersonId == personId && ConversationNodeServes(node, minuteOfDay, state, availability))
            return &node;
    }
    return nullptr;
}

const ConversationNodeDef* SelectConversationNode(std::string_view personId, std::int32_t minuteOfDay, const ConversationState& state,
                                                  PersonAvailability availability) noexcept
{
    return SelectConversationNode(DefaultConversationCatalogue(), personId, minuteOfDay, state, availability);
}

ConversationError OpenConversation(const ConversationCatalogue& catalogue, ConversationState& state, std::string_view personId,
                                   std::int32_t minuteOfDay, PersonAvailability availability)
{
    if (state.Open)
        return ConversationError::AlreadyOpen;
    if (!PersonKnown(catalogue, personId))
        return ConversationError::UnknownPerson;
    if (minuteOfDay < 0 || minuteOfDay >= ConversationMinutesPerDay)
        return ConversationError::InvalidTime;
    if (state.SessionCount == std::numeric_limits<std::uint32_t>::max())
        return ConversationError::SessionLimit;
    const ConversationNodeDef* node = SelectConversationNode(catalogue, personId, minuteOfDay, state, availability);
    if (node == nullptr)
        return ConversationError::UnknownPerson; // unreachable for a validated catalogue; refuse rather than open nothing
    state.Open = true;
    state.PersonId = std::string(personId);
    state.NodeId = std::string(node->Id);
    ++state.SessionCount;
    state.Receipts.clear(); // receipts guard one session; the new session starts with none
    return ConversationError::None;
}

ConversationError OpenConversation(ConversationState& state, std::string_view personId, std::int32_t minuteOfDay, PersonAvailability availability)
{
    return OpenConversation(DefaultConversationCatalogue(), state, personId, minuteOfDay, availability);
}

std::vector<ConversationChoiceView> VisibleConversationChoices(const ConversationCatalogue& catalogue, const ConversationState& state)
{
    std::vector<ConversationChoiceView> views;
    if (!state.Open)
        return views;
    const ConversationNodeDef* node = FindConversationNode(catalogue, state.NodeId);
    if (node != nullptr)
    {
        for (std::size_t i = 0; i < node->ChoiceCount; ++i)
        {
            const ConversationChoiceDef& choice = node->Choices[i];
            if (!ChoiceConditionsHold(choice, state) || (choice.bOnceEver && WasChoiceTaken(state, choice.Id)) ||
                (choice.CostMinutes > 0 && WasChoiceChargedThisSession(state, choice.Id)))
                continue;
            views.push_back(ConversationChoiceView{choice.Id, choice.Text, choice.CostMinutes, false});
        }
    }
    views.push_back(ConversationChoiceView{ConversationLeaveChoiceId, "Leave", 0, true});
    return views;
}

std::vector<ConversationChoiceView> VisibleConversationChoices(const ConversationState& state)
{
    return VisibleConversationChoices(DefaultConversationCatalogue(), state);
}

ConversationChooseResult ChooseConversationChoice(const ConversationCatalogue& catalogue, ConversationState& state, std::string_view choiceId)
{
    ConversationChooseResult result;
    if (!state.Open)
    {
        result.Error = ConversationError::NotOpen;
        return result;
    }
    if (choiceId == ConversationLeaveChoiceId)
    {
        EndSession(state);
        result.bEnded = true;
        return result;
    }
    const ConversationNodeDef* node = FindConversationNode(catalogue, state.NodeId);
    const ConversationChoiceDef* choice = nullptr;
    if (node != nullptr)
    {
        for (std::size_t i = 0; i < node->ChoiceCount; ++i)
        {
            if (node->Choices[i].Id == choiceId)
                choice = &node->Choices[i];
        }
    }
    if (choice == nullptr || !ChoiceConditionsHold(*choice, state))
    {
        result.Error = ConversationError::StaleChoice;
        return result;
    }
    if (choice->bOnceEver && WasChoiceTaken(state, choice->Id))
    {
        result.Error = ConversationError::AlreadyTaken;
        return result;
    }
    if (choice->CostMinutes > 0 && WasChoiceChargedThisSession(state, choice->Id))
    {
        result.Error = ConversationError::AlreadyCharged;
        return result;
    }
    // Accepted: nothing below can fail, so the state changes as one step.
    result.ChoiceId = choice->Id;
    result.Session = state.SessionCount;
    result.CostMinutes = choice->CostMinutes;
    if (choice->CostMinutes > 0)
    {
        const auto at = std::lower_bound(state.Receipts.begin(), state.Receipts.end(), choice->Id,
                                         [](const ConversationReceipt& left, std::string_view right) { return std::string_view(left.ChoiceId) < right; });
        state.Receipts.insert(at, ConversationReceipt{std::string(choice->Id), state.SessionCount});
    }
    if (choice->bOnceEver)
        InsertSorted(state.TakenChoices, choice->Id);
    if (!choice->ClearsFact.empty() && EraseSorted(state.Facts, choice->ClearsFact))
        result.FactCleared = choice->ClearsFact;
    if (!choice->SetsFact.empty())
    {
        const bool wasSet = HasConversationFact(state, choice->SetsFact);
        SetFact(catalogue, state, choice->SetsFact, result.FactCleared);
        if (!wasSet)
            result.FactSet = choice->SetsFact;
    }
    if (choice->NextNode.empty())
    {
        EndSession(state);
        result.bEnded = true;
    }
    else
    {
        state.NodeId = std::string(choice->NextNode);
        result.NextNodeId = choice->NextNode;
    }
    return result;
}

ConversationChooseResult ChooseConversationChoice(ConversationState& state, std::string_view choiceId)
{
    return ChooseConversationChoice(DefaultConversationCatalogue(), state, choiceId);
}

ConversationError LeaveConversation(ConversationState& state)
{
    if (!state.Open)
        return ConversationError::NotOpen;
    EndSession(state);
    return ConversationError::None;
}

bool IsConversationStateValid(const ConversationCatalogue& catalogue, const ConversationState& state) noexcept
{
    if (!StrictlyAscending(state.Facts, [](const std::string& id) { return std::string_view(id); }) ||
        !StrictlyAscending(state.TakenChoices, [](const std::string& id) { return std::string_view(id); }) ||
        !StrictlyAscending(state.Receipts, [](const ConversationReceipt& receipt) { return std::string_view(receipt.ChoiceId); }))
        return false;
    for (const std::string& fact : state.Facts)
    {
        const ConversationFactDef* def = FindConversationFact(catalogue, fact);
        if (def == nullptr || (!def->Implies.empty() && !HasConversationFact(state, def->Implies)))
            return false; // unknown, or present without what it implies
        if (def->Group.empty())
            continue;
        for (const std::string& other : state.Facts)
        {
            const ConversationFactDef* otherDef = FindConversationFact(catalogue, other);
            if (other != fact && otherDef != nullptr && otherDef->Group == def->Group)
                return false; // two facts of an exclusive group
        }
    }
    for (const std::string& taken : state.TakenChoices)
    {
        const ConversationChoiceDef* choice = FindConversationChoice(catalogue, taken);
        if (choice == nullptr || !choice->bOnceEver)
            return false;
    }
    for (const ConversationReceipt& receipt : state.Receipts)
    {
        const ConversationChoiceDef* choice = FindConversationChoice(catalogue, receipt.ChoiceId);
        if (choice == nullptr || choice->CostMinutes <= 0 || state.SessionCount == 0 || receipt.Session != state.SessionCount)
            return false;
    }
    if (!state.Open)
        return state.PersonId.empty() && state.NodeId.empty();
    if (state.SessionCount == 0)
        return false;
    const ConversationNodeDef* node = FindConversationNode(catalogue, state.NodeId);
    return node != nullptr && node->PersonId == state.PersonId;
}

bool IsConversationStateValid(const ConversationState& state) noexcept
{
    return IsConversationStateValid(DefaultConversationCatalogue(), state);
}
} // namespace PerfectWorld
