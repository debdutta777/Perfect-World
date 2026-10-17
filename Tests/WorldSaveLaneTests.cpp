#include "WorldSave.h"

#include "WorldHash.h"
#include "WorldReducer.h"

#include <string>

using namespace PerfectWorld;

#include "WorldSaveTestSupport.h"

namespace
{
constexpr std::string_view LaneFarida = "person.farida_qadri";

std::string LaneQuoted(std::string_view text)
{
    std::string out = "\"";
    for (const char c : text)
    {
        if (c == '\n') out += "\\n";
        else if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    return out + '"';
}

void LaneRoundTripAndReplay()
{
    WorldSim live(81);
    const std::string person(HouseholdAdultId);
    Check(live.Submit({CommandSource::Player, AdvanceTime{720}}).Accepted, "advance to the evening conversation window");
    Check(live.Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted,
          "open household session through reducer");
    Check(live.Submit({CommandSource::Player, ConversationChoose{person, "choice.household_adult.talk_rota", 1}}).Accepted,
          "charge the evening discussion before saving its live reply");
    const auto choices = VisibleConversationChoices(HouseholdConversationCatalogue(), live.State().Conversations.at(person));
    Check(!choices.empty(), "session has choices");
    const std::string door(AuthoredDoors().front().Id);
    Check(live.Submit({CommandSource::System, DoorSettle{door, true, -1}}).Accepted, "settle an open negative-side door");
    const std::string secondDoor(AuthoredDoors()[2].Id);
    Check(live.Submit({CommandSource::System, DoorSettle{secondDoor, true, 1}}).Accepted, "settle a positive-side door");
    WorldSaveData data = SaveOf(live, MakeCreation("parent_household"));
    WorldSaveData loaded;
    const auto result = DeserializeWorldSave(SerializeWorldSave(data), loaded);
    Check(result.Succeeded() && result.SourceVersion == 8 && result.HashVerified, "v8 mid-session round trip is certified");
    CheckSame(data, loaded, "all persisted fields survive the lane round trip");
    WorldSim resumed(loaded.World);
    for (WorldSim* sim : {&live, &resumed})
    {
        Check(sim->Submit({CommandSource::Player, ConversationChoose{person, std::string(choices.front().Id), 1}}).Accepted,
              "the same choice resumes after reload");
        if (sim->State().Conversations.at(person).Open)
            Check(sim->Submit({CommandSource::Player, ConversationLeave{person}}).Accepted, "leave resumed session");
        Check(sim->Submit({CommandSource::Player, AdvanceTime{17}}).Accepted, "advance continued world");
    }
    Check(HashWorldState(live.State()) == HashWorldState(resumed.State()), "reload continuation equals uninterrupted run");
    const auto replay = ReplayCommandLog(81, live.Log());
    Check(replay.Complete && replay.Hash == HashWorldState(live.State()), "lane command replay is equivalent");
}

void LaneMigration()
{
    const WorldSaveData data = RichSave(82, 5);
    std::string old = SerializeWorldSave(data);
    const auto begin = old.find(",\n    \"conversations\":");
    old.erase(begin, old.find("\n  }", begin) - begin);
    old = ReplaceOnce(old, "\"schemaVersion\": 8", "\"schemaVersion\": 7");
    old = ReplaceOnce(old, "\"hashVersion\": 5", "\"hashVersion\": 4");
    // Historical v4 cannot be certified by v5. Use the independently pinned v4 reference value as a stale hash.
    const auto hash = old.find("\"stateHash\": ") + 13;
    old.replace(hash, 20, "\"0x6172068cb2eb0ae3\"");
    WorldSaveData loaded;
    const auto result = DeserializeWorldSave(old, loaded);
    Check(result.Succeeded() && result.SourceVersion == 7 && !result.HashVerified && !result.Migrated,
          "historical world loads unverified; Migrated remains the creation-only marker");
    CheckSame(data, loaded, "v7 retains old authority and receives empty conversations and authored doors");
    Check(loaded.World.Conversations.empty() && loaded.World.Doors == MakeInitialDoorState(), "honest lane defaults");
    // Even a relabelled historical record carrying today's hash must not be certified as a v8 snapshot.
    std::string relabelled = ReplaceOnce(old, "\"hashVersion\": 4", "\"hashVersion\": 5");
    const std::string current = SerializeWorldSave(data);
    const auto currentHash = current.find("\"stateHash\": ") + 13;
    relabelled.replace(relabelled.find("\"stateHash\": ") + 13, 20, current.substr(currentHash, 20));
    WorldSaveData relabelledOut;
    const auto relabelledResult = DeserializeWorldSave(relabelled, relabelledOut);
    Check(relabelledResult.Succeeded() && !relabelledResult.HashVerified,
          "historical schema cannot claim current hash certification");
    for (std::size_t cut = 0; cut < old.size(); ++cut)
        Check(ErrorOf(old.substr(0, cut)) != WorldSaveError::None, "every v7 prefix is refused atomically");
    WorldSaveData again;
    Check(DeserializeWorldSave(SerializeWorldSave(loaded), again).HashVerified, "v7 resave becomes certified v8");
    Check(ErrorOf(ReplaceOnce(old, "\"rng\":", "\"conversations\": {}, \"rng\":")) == WorldSaveError::Malformed,
          "v7 refuses new sections");
}

void LaneReceiptsAndCatalogues()
{
    WorldSaveData data = RichSave(84, 0);
    const auto household = HouseholdConversationCatalogue();
    const auto care = CareConversationCatalogue();
    const auto farida = FaridaConversationCatalogue();
    auto& state = data.World.Conversations[std::string(HouseholdAdultId)];
    Check(OpenConversation(household, state, HouseholdAdultId, 1080, PersonAvailability::Awake) == ConversationError::None,
          "open evening household conversation");
    Check(ChooseConversationChoice(household, state, "choice.household_adult.talk_rota").Ok() && state.Open && state.Receipts.size() == 1,
          "save a paid transaction receipt in a live reply node");
    for (const auto catalogue : {care, farida})
    {
        const std::string person(catalogue.Nodes.front().PersonId);
        auto& other = data.World.Conversations[person];
        Check(OpenConversation(catalogue, other, person, 600, PersonAvailability::Awake) == ConversationError::None,
              "open other authored person");
        Check(LeaveConversation(other) == ConversationError::None, "persist closed session history too");
    }
    const std::string text = SerializeWorldSave(data);
    Check(text.find("\"person.care_worker\":") < text.find("\"person.farida_qadri\":") &&
              text.find("\"person.farida_qadri\":") < text.find("\"person.household_adult\":"), "writer orders person keys");
    WorldSaveData loaded;
    Check(DeserializeWorldSave(text, loaded).HashVerified, "all catalogues and a live receipt round trip");
    CheckSame(data, loaded, "facts, receipts, sessions, and closed histories all survive");
    Check(WasChoiceChargedThisSession(loaded.World.Conversations.at(std::string(HouseholdAdultId)), "choice.household_adult.talk_rota"),
          "the persisted receipt still guards the charge");
    const std::string householdText = SerialiseConversationState(household, state);
    Check(ErrorOf(ReplaceOnce(text, LaneQuoted(householdText), LaneQuoted(householdText.substr(0, householdText.size() - 1)))) ==
              WorldSaveError::InvalidWorld, "a module envelope truncated inside valid JSON is refused");
    WorldSaveData changed = data;
    changed.World.Conversations.at(std::string(HouseholdAdultId)).Facts = {std::string(FactEveningRotaAgreed)};
    const auto changedText = SerialiseConversationState(household, changed.World.Conversations.at(std::string(HouseholdAdultId)));
    Check(!changedText.empty(), "changed fact fixture is valid");
    Check(ErrorOf(ReplaceOnce(text, LaneQuoted(householdText), LaneQuoted(changedText))) == WorldSaveError::HashMismatch,
          "a valid conversation fact mutation fails the world hash");
    changed = data;
    const std::string originalDoors = SerialiseDoorState(data.World.Doors);
    Check(ApplyDoorSettlement(changed.World.Doors, changed.World.Doors.Doors.front().Id, true, -1) == DoorResult::Ok,
          "make valid replacement door envelope");
    Check(ErrorOf(ReplaceOnce(text, LaneQuoted(originalDoors), LaneQuoted(SerialiseDoorState(changed.World.Doors)))) == WorldSaveError::HashMismatch,
          "a valid door mutation fails the world hash");
    Check(ErrorOf(ReplaceOnce(text, LaneQuoted(originalDoors), "5")) == WorldSaveError::InvalidWorld, "door envelope must be text");
    Check(ErrorOf(ReplaceOnce(text, LaneQuoted(originalDoors), LaneQuoted(originalDoors + "x"))) == WorldSaveError::InvalidWorld,
          "trailing door envelope bytes refused");
    Check(ErrorOf(ReplaceOnce(text, LaneQuoted(householdText), LaneQuoted(householdText + "x"))) == WorldSaveError::InvalidWorld,
          "trailing conversation envelope bytes refused");
    Check(ErrorOf(ReplaceOnce(text, LaneQuoted(originalDoors), LaneQuoted(originalDoors.substr(0, originalDoors.size() - 1)))) ==
              WorldSaveError::InvalidWorld, "a truncated door envelope inside valid JSON is refused");
}

void LaneRefusals()
{
    WorldSaveData data = RichSave(83, 3);
    auto& session = data.World.Conversations[std::string(HouseholdAdultId)];
    Check(OpenConversation(session, HouseholdAdultId, 360, PersonAvailability::Awake) == ConversationError::None, "open fixture session");
    const std::string text = SerializeWorldSave(data);
    for (std::size_t cut = 0; cut < text.size(); ++cut)
        Check(ErrorOf(text.substr(0, cut)) != WorldSaveError::None, "every v8 lane prefix is refused atomically");
    Check(ErrorOf(ReplaceOnce(text, "\"conversations\"", "\"conversationX\"")) == WorldSaveError::Malformed, "required conversations key");
    Check(ErrorOf(ReplaceOnce(text, "\"doors\"", "\"doorsX\"")) == WorldSaveError::Malformed, "required doors key");
    std::string wrongType = text;
    const auto mapBegin = wrongType.find("\"conversations\": {");
    const auto mapEnd = wrongType.find("\n    }", mapBegin) + 6;
    wrongType.replace(mapBegin, mapEnd - mapBegin, "\"conversations\": []");
    Check(ErrorOf(wrongType) == WorldSaveError::InvalidWorld, "wrong conversation type refused");
    Check(ErrorOf(ReplaceOnce(text, "\"person.household_adult\":", "\"person.household_adult\": \"\", \"person.household_adult\":")) ==
              WorldSaveError::Malformed, "duplicate person key refused");
    Check(ErrorOf(ReplaceOnce(text, "pw.doors/2", "pw.doors/1")) == WorldSaveError::InvalidWorld, "v8 requires doors v2");
    Check(ErrorOf(ReplaceOnce(text, "pw.conversation/2", "pw.conversation/1")) == WorldSaveError::InvalidWorld, "v8 requires conversation v2");
    Check(ErrorOf(ReplaceOnce(text, "\"person.household_adult\":", "\"person.unknown\":")) == WorldSaveError::InvalidWorld, "unknown map person refused");
    Check(ErrorOf(ReplaceOnce(text, "pw.conversation/2\\n", "pw.conversation/2\\ninvalid\\n")) == WorldSaveError::InvalidWorld, "corrupt module text refused");
    Check(ErrorOf(ReplaceOnce(text, "pw.doors/2\\n", "pw.doors/2\\ninvalid\\n")) == WorldSaveError::InvalidWorld, "corrupt door text refused");
    const auto care = CareConversationCatalogue();
    const std::string carePerson(care.Nodes.front().PersonId);
    Check(ErrorOf(ReplaceOnce(text, "\"person.household_adult\":", LaneQuoted(carePerson) + ':')) == WorldSaveError::InvalidWorld,
          "a session cannot belong to a different map person");
    ConversationState farida;
    Check(OpenConversation(FaridaConversationCatalogue(), farida, LaneFarida, 600, PersonAvailability::Awake) == ConversationError::None,
          "second fixture session is valid alone");
    const std::string extra = LaneQuoted(LaneFarida) + ": " + LaneQuoted(SerialiseConversationState(FaridaConversationCatalogue(), farida)) + ',';
    Check(ErrorOf(ReplaceOnce(text, "\"conversations\": {", "\"conversations\": {" + extra)) == WorldSaveError::InvalidWorld,
          "two globally open sessions refused");
    data.World.Conversations.emplace(std::string(LaneFarida), farida);
    Check(SerializeWorldSave(data).empty(), "writer refuses two open sessions");
    data.World.Conversations.clear();
    data.World.Conversations["person.unknown"] = {};
    Check(SerializeWorldSave(data).empty(), "writer refuses unknown person");
    data.World.Conversations.clear();
    data.World.Conversations[std::string(HouseholdAdultId)].Facts = {"fact.unknown"};
    Check(SerializeWorldSave(data).empty(), "writer refuses invalid conversation state");
    data.World.Conversations.clear();
    data.World.Doors.Doors.front().SwingSide = 9;
    Check(SerializeWorldSave(data).empty(), "writer refuses invalid door pair");
}
} // namespace

void TestWorldSaveLane()
{
    LaneRoundTripAndReplay();
    LaneMigration();
    LaneReceiptsAndCatalogues();
    LaneRefusals();
}
