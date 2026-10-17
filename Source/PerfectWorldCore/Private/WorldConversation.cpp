#include "WorldReducer.h"

#include <algorithm>
#include <array>

// The lane's conversation and door commands (LANE_LIFE_DISCUSSION_2026-10-14 order TX). The core Conversation and Doors
// modules decide; this file only routes a command to the right content, keeps the world's copy of their state, and
// charges a paid choice's declared minutes through the one world clock in the same command. Validation runs each module
// operation on a copy, so a refusal never touches the world; Apply then repeats it on the world's state, where it cannot
// fail, except that a paid choice's charge can still be stopped short by the event cap, which makes Submit roll back.
namespace PerfectWorld
{
namespace
{
Rejection LaneReject(std::string code, std::string message)
{
    return Rejection{std::move(code), std::move(message)};
}

Rejection ConversationRefusal(ConversationError error)
{
    return LaneReject("conversation." + std::string(ConversationErrorCode(error)), "The conversation module refused that.");
}

Rejection DoorRefusal(DoorResult result)
{
    return LaneReject("door." + std::string(DoorResultCode(result)), "The door rules refused that.");
}

std::int32_t MinuteOfDayNow(const WorldState& state)
{
    return DoorMinuteOfDay(state.Clock.Now());
}

/** The person's state as stored, or a fresh one for a person never spoken to. */
ConversationState StateOrFresh(const WorldState& state, std::string_view personId)
{
    const auto found = state.Conversations.find(personId);
    return found == state.Conversations.end() ? ConversationState{} : found->second;
}

const ConversationState* OpenStateOf(const WorldState& state, std::string_view personId)
{
    const auto found = state.Conversations.find(personId);
    return found != state.Conversations.end() && found->second.Open ? &found->second : nullptr;
}

const DoorEntry* FindDoorEntry(const DoorState& doors, std::string_view doorId)
{
    for (const DoorEntry& entry : doors.Doors)
    {
        if (entry.Id == doorId)
            return &entry;
    }
    return nullptr;
}

bool MayReportSettlement(CommandSource source)
{
    return source == CommandSource::Presentation || source == CommandSource::System || source == CommandSource::Debug;
}
} // namespace

std::optional<ConversationCatalogue> ConversationCatalogueForPerson(std::string_view personId) noexcept
{
    if (personId.empty())
        return std::nullopt;
    const std::array<ConversationCatalogue, 5> catalogues = {HouseholdConversationCatalogue(), CareConversationCatalogue(),
                                                             FaridaConversationCatalogue(), AninditaConversationCatalogue(),
                                                             MiraConversationCatalogue()};
    for (const ConversationCatalogue& catalogue : catalogues)
    {
        const bool speaks = std::any_of(catalogue.Nodes.begin(), catalogue.Nodes.end(),
                                        [personId](const ConversationNodeDef& node) { return node.PersonId == personId; });
        if (speaks)
            return catalogue;
    }
    return std::nullopt;
}

std::string_view OpenConversationPersonId(const WorldState& state) noexcept
{
    for (const auto& [personId, conversation] : state.Conversations)
    {
        if (conversation.Open)
            return personId;
    }
    return {};
}

bool IsLaneCommand(const CommandPayload& payload) noexcept
{
    return std::holds_alternative<ConversationOpen>(payload) || std::holds_alternative<ConversationChoose>(payload) ||
           std::holds_alternative<ConversationLeave>(payload) || std::holds_alternative<DoorSet>(payload) ||
           std::holds_alternative<DoorSettle>(payload);
}

bool WorldSim::NeedsRollbackSnapshot(const Command& command) const
{
    // Only a conversation choice can be refused after it has started to apply (its charge stopped by the event cap).
    return std::holds_alternative<ConversationChoose>(command.Payload);
}

std::optional<Rejection> WorldSim::ValidateLaneCommand(const Command& command) const
{
    if (const auto* open = std::get_if<ConversationOpen>(&command.Payload))
    {
        if (!IsWorldQuiescent(State_))
            return LaneReject("world.unsettled", "Events are still due at this instant; settle them first.");
        if (State_.DayLoop.Asleep)
            return LaneReject("conversation.player_asleep", "The player is asleep.");
        if (State_.DayLoop.Runner.Active())
            return LaneReject("conversation.activity_running", "An activity is running; finish or cancel it first.");
        if (!OpenConversationPersonId(State_).empty())
            return ConversationRefusal(ConversationError::AlreadyOpen);
        const std::optional<ConversationCatalogue> catalogue = ConversationCatalogueForPerson(open->PersonId);
        if (!catalogue)
            return ConversationRefusal(ConversationError::UnknownPerson);
        ConversationState trial = StateOrFresh(State_, open->PersonId);
        const ConversationError error = OpenConversation(*catalogue, trial, open->PersonId, MinuteOfDayNow(State_), open->Availability);
        if (error != ConversationError::None)
            return ConversationRefusal(error);
        return std::nullopt;
    }
    if (const auto* choose = std::get_if<ConversationChoose>(&command.Payload))
    {
        if (!IsWorldQuiescent(State_))
            return LaneReject("world.unsettled", "Events are still due at this instant; settle them first.");
        const std::optional<ConversationCatalogue> catalogue = ConversationCatalogueForPerson(choose->PersonId);
        if (!catalogue)
            return ConversationRefusal(ConversationError::UnknownPerson);
        const ConversationState* current = OpenStateOf(State_, choose->PersonId);
        if (current == nullptr)
            return ConversationRefusal(ConversationError::NotOpen);
        if (choose->Session != current->SessionCount)
            return LaneReject("conversation.stale_session", "That choice belongs to a session that has ended.");
        ConversationState trial = *current;
        const ConversationChooseResult chosen = ChooseConversationChoice(*catalogue, trial, choose->ChoiceId);
        if (!chosen.Ok())
            return ConversationRefusal(chosen.Error);
        if (chosen.CostMinutes > 0)
            return ValidateTimeCharge(chosen.CostMinutes);
        return std::nullopt;
    }
    if (const auto* leave = std::get_if<ConversationLeave>(&command.Payload))
    {
        if (OpenStateOf(State_, leave->PersonId) == nullptr)
            return ConversationRefusal(ConversationError::NotOpen);
        return std::nullopt;
    }
    if (const auto* set = std::get_if<DoorSet>(&command.Payload))
    {
        DoorState trial = State_.Doors;
        const DoorResult result = TrySetDoorOpen(trial, set->DoorId, set->Open, MinuteOfDayNow(State_), set->Context);
        if (result != DoorResult::Ok)
            return DoorRefusal(result);
        return std::nullopt;
    }
    if (const auto* settle = std::get_if<DoorSettle>(&command.Payload))
    {
        if (!MayReportSettlement(command.Source))
            return LaneReject("door.source", "Only the presentation, systems and debug report how a leaf settled.");
        DoorState trial = State_.Doors;
        const DoorResult result = ApplyDoorSettlement(trial, settle->DoorId, settle->IsOpenNow, settle->SwingSide);
        if (result != DoorResult::Ok)
            return DoorRefusal(result);
        return std::nullopt;
    }
    return LaneReject("command.unknown", "Unknown command.");
}

std::optional<Rejection> WorldSim::ApplyLaneCommand(const Command& command, CommandResult& result)
{
    if (const auto* open = std::get_if<ConversationOpen>(&command.Payload))
    {
        const ConversationCatalogue catalogue = *ConversationCatalogueForPerson(open->PersonId);
        ConversationState& conversation = State_.Conversations[open->PersonId];
        (void)OpenConversation(catalogue, conversation, open->PersonId, MinuteOfDayNow(State_), open->Availability);
        result.Events.push_back({0, State_.Clock.Now(), ConversationOpened{open->PersonId, conversation.NodeId, conversation.SessionCount}});
    }
    else if (const auto* choose = std::get_if<ConversationChoose>(&command.Payload))
    {
        const ConversationCatalogue catalogue = *ConversationCatalogueForPerson(choose->PersonId);
        ConversationState& conversation = State_.Conversations.find(choose->PersonId)->second;
        const std::uint32_t session = conversation.SessionCount;
        const ConversationChooseResult chosen = ChooseConversationChoice(catalogue, conversation, choose->ChoiceId);
        if (!chosen.ChoiceId.empty())
        {
            result.Events.push_back({0, State_.Clock.Now(),
                                     ConversationChoiceAccepted{choose->PersonId, std::string(chosen.ChoiceId), chosen.Session, chosen.CostMinutes,
                                                                std::string(chosen.FactSet), std::string(chosen.FactCleared),
                                                                std::string(chosen.NextNodeId)}});
        }
        if (chosen.CostMinutes > 0)
        {
            // The charge for receipt (ChoiceId, Session), through the same path as every other minute. All or nothing.
            ChargeTime(chosen.CostMinutes, result);
            if (result.Partial)
                return LaneReject("conversation.charge_interrupted",
                                  "Too many scheduled events fall inside this choice's time; settle the world and choose again.");
        }
        if (chosen.bEnded)
            result.Events.push_back({0, State_.Clock.Now(), ConversationEnded{choose->PersonId, session}});
    }
    else if (const auto* leave = std::get_if<ConversationLeave>(&command.Payload))
    {
        ConversationState& conversation = State_.Conversations.find(leave->PersonId)->second;
        (void)LeaveConversation(conversation);
        result.Events.push_back({0, State_.Clock.Now(), ConversationEnded{leave->PersonId, conversation.SessionCount}});
    }
    else if (const auto* set = std::get_if<DoorSet>(&command.Payload))
    {
        (void)TrySetDoorOpen(State_.Doors, set->DoorId, set->Open, MinuteOfDayNow(State_), set->Context);
        if (const DoorEntry* entry = FindDoorEntry(State_.Doors, set->DoorId))
            result.Events.push_back({0, State_.Clock.Now(), DoorChanged{entry->Id, entry->Open, entry->SwingSide, false}});
    }
    else if (const auto* settle = std::get_if<DoorSettle>(&command.Payload))
    {
        (void)ApplyDoorSettlement(State_.Doors, settle->DoorId, settle->IsOpenNow, settle->SwingSide);
        if (const DoorEntry* entry = FindDoorEntry(State_.Doors, settle->DoorId))
            result.Events.push_back({0, State_.Clock.Now(), DoorChanged{entry->Id, entry->Open, entry->SwingSide, true}});
    }
    return std::nullopt;
}
} // namespace PerfectWorld
