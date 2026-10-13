#include "Conversation.h"

#include <algorithm>

namespace PerfectWorld
{
namespace
{
constexpr std::array<std::string_view, PersonAvailabilityCount> AvailabilityIds{"awake", "busy", "asleep"};

bool StartsWith(std::string_view text, std::string_view prefix) noexcept
{
    return text.substr(0, prefix.size()) == prefix;
}

bool IsKnownFact(std::string_view factId) noexcept
{
    for (const std::string_view fact : ConversationFactIds())
    {
        if (fact == factId)
            return true;
    }
    return false;
}

bool InWindow(const ConversationNodeDef& node, std::int32_t minute) noexcept
{
    if (node.FromMinute < node.ToMinute)
        return minute >= node.FromMinute && minute < node.ToMinute;
    return minute >= node.FromMinute || minute < node.ToMinute; // wraps past midnight
}

bool FactsAllow(const ConversationNodeDef& node, const ConversationState& state) noexcept
{
    return (node.RequiresFact.empty() || HasConversationFact(state, node.RequiresFact)) &&
           (node.ForbidsFact.empty() || !HasConversationFact(state, node.ForbidsFact));
}

bool PersonKnown(std::string_view personId) noexcept
{
    for (const ConversationNodeDef& node : ConversationNodes())
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

void EndSession(ConversationState& state)
{
    state.Open = false;
    state.PersonId.clear();
    state.NodeId.clear();
}

bool StrictlyAscending(const std::vector<std::string>& ids) noexcept
{
    for (std::size_t i = 1; i < ids.size(); ++i)
    {
        if (!(ids[i - 1] < ids[i]))
            return false;
    }
    return true;
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
    }
    return {};
}

const ConversationNodeDef* FindConversationNode(std::string_view nodeId) noexcept
{
    for (const ConversationNodeDef& node : ConversationNodes())
    {
        if (node.Id == nodeId)
            return &node;
    }
    return nullptr;
}

const ConversationChoiceDef* FindConversationChoice(std::string_view choiceId) noexcept
{
    for (const ConversationNodeDef& node : ConversationNodes())
    {
        for (std::size_t i = 0; i < node.ChoiceCount; ++i)
        {
            if (node.Choices[i].Id == choiceId)
                return &node.Choices[i];
        }
    }
    return nullptr;
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

const ConversationNodeDef* SelectConversationNode(std::string_view personId, std::int32_t minuteOfDay, const ConversationState& state,
                                                  PersonAvailability availability) noexcept
{
    if (minuteOfDay < 0 || minuteOfDay >= ConversationMinutesPerDay)
        return nullptr;
    for (const ConversationNodeDef& node : ConversationNodes())
    {
        if (node.bSelectable && node.PersonId == personId && node.Availability == availability && InWindow(node, minuteOfDay) &&
            FactsAllow(node, state))
            return &node;
    }
    return nullptr;
}

ConversationError OpenConversation(ConversationState& state, std::string_view personId, std::int32_t minuteOfDay,
                                   PersonAvailability availability)
{
    if (state.Open)
        return ConversationError::AlreadyOpen;
    if (!PersonKnown(personId))
        return ConversationError::UnknownPerson;
    if (minuteOfDay < 0 || minuteOfDay >= ConversationMinutesPerDay)
        return ConversationError::InvalidTime;
    const ConversationNodeDef* node = SelectConversationNode(personId, minuteOfDay, state, availability);
    if (node == nullptr)
        return ConversationError::UnknownPerson; // unreachable for a validated catalogue; refuse rather than open nothing
    state.Open = true;
    state.PersonId = std::string(personId);
    state.NodeId = std::string(node->Id);
    return ConversationError::None;
}

std::vector<ConversationChoiceView> VisibleConversationChoices(const ConversationState& state)
{
    std::vector<ConversationChoiceView> views;
    if (!state.Open)
        return views;
    const ConversationNodeDef* node = FindConversationNode(state.NodeId);
    if (node != nullptr)
    {
        for (std::size_t i = 0; i < node->ChoiceCount; ++i)
        {
            const ConversationChoiceDef& choice = node->Choices[i];
            if (choice.bOnce && WasChoiceTaken(state, choice.Id))
                continue;
            views.push_back(ConversationChoiceView{choice.Id, choice.Text, choice.CostMinutes, false});
        }
    }
    views.push_back(ConversationChoiceView{ConversationLeaveChoiceId, "Leave", 0, true});
    return views;
}

ConversationChooseResult ChooseConversationChoice(ConversationState& state, std::string_view choiceId)
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
    const ConversationNodeDef* node = FindConversationNode(state.NodeId);
    const ConversationChoiceDef* choice = nullptr;
    if (node != nullptr)
    {
        for (std::size_t i = 0; i < node->ChoiceCount; ++i)
        {
            if (node->Choices[i].Id == choiceId)
                choice = &node->Choices[i];
        }
    }
    if (choice == nullptr)
    {
        result.Error = ConversationError::StaleChoice;
        return result;
    }
    if (choice->bOnce && WasChoiceTaken(state, choice->Id))
    {
        result.Error = ConversationError::AlreadyTaken;
        return result;
    }
    result.CostMinutes = choice->CostMinutes;
    if (choice->bOnce)
        InsertSorted(state.TakenChoices, choice->Id);
    if (!choice->SetsFact.empty() && !HasConversationFact(state, choice->SetsFact))
    {
        InsertSorted(state.Facts, choice->SetsFact);
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

ConversationError LeaveConversation(ConversationState& state)
{
    if (!state.Open)
        return ConversationError::NotOpen;
    EndSession(state);
    return ConversationError::None;
}

bool IsConversationStateValid(const ConversationState& state) noexcept
{
    if (!StrictlyAscending(state.Facts) || !StrictlyAscending(state.TakenChoices))
        return false;
    for (const std::string& fact : state.Facts)
    {
        if (!IsKnownFact(fact))
            return false;
    }
    // Every taken choice is a known once-choice, and its fact is set; every fact was set by a taken choice.
    for (const std::string& taken : state.TakenChoices)
    {
        const ConversationChoiceDef* choice = FindConversationChoice(taken);
        if (choice == nullptr || !choice->bOnce || (!choice->SetsFact.empty() && !HasConversationFact(state, choice->SetsFact)))
            return false;
    }
    for (const std::string& fact : state.Facts)
    {
        bool setByTaken = false;
        for (const std::string& taken : state.TakenChoices)
        {
            const ConversationChoiceDef* choice = FindConversationChoice(taken);
            setByTaken = setByTaken || (choice != nullptr && choice->SetsFact == fact);
        }
        if (!setByTaken)
            return false;
    }
    // The arrangement can be kept only after it was agreed.
    if (HasConversationFact(state, FactEveningRotaKept) && !HasConversationFact(state, FactEveningRotaAgreed))
        return false;
    if (!state.Open)
        return state.PersonId.empty() && state.NodeId.empty();
    const ConversationNodeDef* node = FindConversationNode(state.NodeId);
    return node != nullptr && node->PersonId == state.PersonId;
}

bool ValidateConversationCatalogue() noexcept
{
    const std::span<const ConversationNodeDef> nodes = ConversationNodes();
    std::vector<std::string_view> choiceIds;
    std::vector<std::string_view> factSetters;
    for (std::size_t n = 0; n < nodes.size(); ++n)
    {
        const ConversationNodeDef& node = nodes[n];
        if (!StartsWith(node.Id, "node.") || node.PersonId != HouseholdAdultId || node.Line.empty() ||
            static_cast<std::size_t>(node.Availability) >= PersonAvailabilityCount)
            return false;
        for (std::size_t j = 0; j < n; ++j)
        {
            if (nodes[j].Id == node.Id)
                return false;
        }
        if (node.FromMinute < 0 || node.FromMinute >= ConversationMinutesPerDay || node.ToMinute < 0 || node.ToMinute > ConversationMinutesPerDay)
            return false;
        if ((!node.RequiresFact.empty() && !IsKnownFact(node.RequiresFact)) || (!node.ForbidsFact.empty() && !IsKnownFact(node.ForbidsFact)))
            return false;
        if (node.ChoiceCount > ConversationMaxAuthoredChoices)
            return false;
        if (node.bDeclines && (node.ChoiceCount != 0 || node.Availability == PersonAvailability::Awake))
            return false;
        if (node.Availability != PersonAvailability::Awake && !node.bDeclines && node.bSelectable)
            return false;
        for (std::size_t i = 0; i < node.ChoiceCount; ++i)
        {
            const ConversationChoiceDef& choice = node.Choices[i];
            if (!StartsWith(choice.Id, "choice.") || choice.Id == ConversationLeaveChoiceId || choice.Text.empty() ||
                choice.CostMinutes < 0 || choice.CostMinutes > ConversationMaxCostMinutes)
                return false;
            if ((choice.CostMinutes > 0 || !choice.SetsFact.empty()) && !choice.bOnce)
                return false; // a charge or a reward may happen only once
            if (!choice.SetsFact.empty())
            {
                if (!IsKnownFact(choice.SetsFact) || std::find(factSetters.begin(), factSetters.end(), choice.SetsFact) != factSetters.end())
                    return false;
                factSetters.push_back(choice.SetsFact);
            }
            if (!choice.NextNode.empty() && FindConversationNode(choice.NextNode) == nullptr)
                return false;
            if (std::find(choiceIds.begin(), choiceIds.end(), choice.Id) != choiceIds.end())
                return false;
            choiceIds.push_back(choice.Id);
        }
    }
    if (factSetters.size() != ConversationFactIds().size())
        return false; // a fact nothing can set
    // Every situation selects exactly one node: each availability, each minute, each reachable set of facts.
    const std::array<std::vector<std::string>, 3> factSets{std::vector<std::string>{},
                                                           std::vector<std::string>{std::string(FactEveningRotaAgreed)},
                                                           std::vector<std::string>{std::string(FactEveningRotaAgreed), std::string(FactEveningRotaKept)}};
    for (const std::vector<std::string>& facts : factSets)
    {
        ConversationState state;
        state.Facts = facts;
        for (std::size_t a = 0; a < PersonAvailabilityCount; ++a)
        {
            for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; ++minute)
            {
                int matches = 0;
                for (const ConversationNodeDef& node : nodes)
                {
                    matches += (node.bSelectable && node.Availability == static_cast<PersonAvailability>(a) && InWindow(node, minute) &&
                                FactsAllow(node, state))
                                   ? 1
                                   : 0;
                }
                if (matches != 1)
                    return false;
            }
        }
    }
    return true;
}
} // namespace PerfectWorld
