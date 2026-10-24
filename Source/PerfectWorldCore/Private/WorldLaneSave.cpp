#include "WorldSaveInternal.h"

#include "WorldReducer.h"

#include <utility>

namespace PerfectWorld
{
std::string WorldSaveWriteLane(const WorldState& world)
{
    const std::string doors = SerialiseDoorState(world.Doors);
    const std::string journal = SerialiseLaneJournal(world);
    if (doors.empty() || journal.empty())
        return {};
    std::string out = ",\n    \"conversations\": {";
    bool first = true;
    bool open = false;
    for (const auto& [person, state] : world.Conversations)
    {
        const auto catalogue = ConversationCatalogueForPerson(person);
        if (!catalogue || (state.Open && (open || state.PersonId != person)))
            return {};
        const std::string text = SerialiseConversationState(*catalogue, state);
        if (text.empty())
            return {};
        open = open || state.Open;
        out += first ? "\n" : ",\n";
        first = false;
        out += "      " + WorldSaveQuoted(person) + ": " + WorldSaveQuoted(text);
    }
    out += first ? "}" : "\n    }";
    out += ",\n    \"doors\": " + WorldSaveQuoted(doors);
    out += ",\n    \"journal\": " + WorldSaveQuoted(journal);
    return out;
}

WorldSaveResult WorldSaveReadLane(const JsonValue& value, WorldState& world)
{
    const JsonValue* conversations = value.Find("conversations");
    const JsonValue* doors = value.Find("doors");
    if (conversations == nullptr || !conversations->IsObject() || doors == nullptr || !doors->IsString())
        return WorldSaveInvalid("the conversation or door section has the wrong type.");
    // A v8 file always uses current module envelopes; older module forms belong to their standalone migrations.
    if (!doors->String.starts_with("pw.doors/2\n"))
        return WorldSaveInvalid("the door envelope version is not supported in this world save.");
    DoorStateParse parsedDoors = DeserialiseDoorState(doors->String);
    if (!parsedDoors.Ok())
        return WorldSaveInvalid("the door state is invalid: " + parsedDoors.Error);
    bool open = false;
    for (const auto& [person, item] : conversations->Object)
    {
        const auto catalogue = ConversationCatalogueForPerson(person);
        if (!catalogue || !item.IsString() || !item.String.starts_with("pw.conversation/2\n"))
            return WorldSaveInvalid("a conversation names an unknown person or has an invalid envelope.");
        ConversationStateParse parsed = DeserialiseConversationState(*catalogue, item.String);
        if (!parsed.Ok() || (parsed.State.Open && (open || parsed.State.PersonId != person)))
            return WorldSaveInvalid("a conversation is invalid, belongs to another person, or a second session is open.");
        open = open || parsed.State.Open;
        world.Conversations.emplace(person, std::move(parsed.State));
    }
    world.Doors = std::move(parsedDoors.State);
    // WorldSaveReadWorld requires this field only for v9; v8 migration leaves the log empty.
    if (const JsonValue* journal = value.Find("journal"))
    {
        if (!journal->IsString() || !DeserialiseLaneJournal(journal->String, world))
            return WorldSaveInvalid("the journal envelope is invalid.");
    }
    return {};
}
} // namespace PerfectWorld
