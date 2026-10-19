#include "WorldSave.h"

#include "WorldHash.h"
#include "WorldReducer.h"

#include <fstream>
#include <iterator>
#include <string>
#include <utility>

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

void LaneStationeryEgress()
{
    WorldSim live(85);
    const std::string door = "door.nabapara.stationery_front";
    Check(live.Submit({CommandSource::Player, AdvanceTime{810}}).Accepted, "reach stationery closing time at 19:30");
    const auto before = HashWorldState(live.State());
    const auto refused = live.Submit({CommandSource::Player, DoorSet{door, true, {DoorDirection::Entry}}});
    Check(!refused.Accepted && refused.Why.Code == "door.closed_hours" && HashWorldState(live.State()) == before,
          "closed-hours entry is refused atomically through the reducer");
    Check(live.Submit({CommandSource::Player, DoorSet{door, true, {DoorDirection::Exit}}}).Accepted,
          "a person inside opens the stationery door after closing");
    const auto data = SaveOf(live, MakeCreation("parent_household"));
    WorldSaveData loaded;
    Check(DeserializeWorldSave(SerializeWorldSave(data), loaded).HashVerified, "stationery open state survives certified v8 save");
    CheckSame(data, loaded, "stationery state and remaining world survive save and load");
    Check(QueryDoorOpen(loaded.World.Doors, door) == true, "saved stationery door is open");
    WorldSim resumed(loaded.World);
    Check(!resumed.Submit({CommandSource::Player, DoorSet{door, true, {DoorDirection::Entry}}}).Accepted,
          "an open leaf after reload does not grant entry after closing");
    Check(resumed.Submit({CommandSource::Player, DoorSet{door, false, {DoorDirection::Exit}}}).Accepted,
          "a person leaving may close the door after reload");
    Check(ReplayCommandLog(85, live.Log()).Hash == HashWorldState(live.State()), "exit command replays deterministically");
}

// Historical v5 byte layout, independent of the door writer's current authored table.
std::uint64_t LaneHistoricalHash(const WorldState& world, std::string_view doors)
{
    const auto number = [](std::uint64_t value) {
        std::string bytes;
        for (int shift = 0; shift < 64; shift += 8)
            bytes += static_cast<char>(value >> shift);
        return bytes;
    };
    const auto text = [&](std::string_view value) {
        return "s" + number(value.size()) + std::string(value);
    };
    const auto fnv = [](std::string_view bytes) {
        std::uint64_t hash = 0xcbf29ce484222325ULL;
        for (const unsigned char byte : bytes)
            hash = (hash ^ byte) * 0x100000001b3ULL;
        return hash;
    };
    std::string bytes = "u" + number(5);
    for (const auto& section : HashWorldSections(world))
        bytes += text(section.Name) + "u" + number(section.Name == "doors" ? fnv(text(doors)) : section.Hash);
    return fnv(bytes);
}

void LaneTenDoorV8Migration()
{
    constexpr std::string_view oldDoors =
        "pw.doors/2\ncount=10\n"
        "door=door.nabapara.home_front,1,-1\n"
        "door=door.nabapara.home_living,1,1\n"
        "door=door.nabapara.home_bedroom,0,0\n"
        "door=door.nabapara.home_kitchen,1,1\n"
        "door=door.nabapara.home_washroom,0,0\n"
        "door=door.nabapara.shanti_chai_shutter,0,0\n"
        "door=door.nabapara.care_front,0,0\n"
        "door=door.nabapara.care_dormitory,0,0\n"
        "door=door.nabapara.care_common,1,1\n"
        "door=door.nabapara.care_washroom,0,0\nend\n";
    auto data = RichSave(86, 0);
    Check(ApplyDoorSettlement(data.World.Doors, "door.nabapara.home_front", true, -1) == DoorResult::Ok,
          "historical fixture has a nondefault negative swing");
    std::string old = ReplaceOnce(SerializeWorldSave(data), LaneQuoted(SerialiseDoorState(data.World.Doors)), LaneQuoted(oldDoors));
    const auto hashAt = old.find("\"stateHash\": ") + 13;
    const auto hex = [](std::uint64_t hash) {
        std::string value = "\"0x0000000000000000\"";
        for (int digit = 0; digit < 16; ++digit)
            value[18 - digit] = "0123456789abcdef"[(hash >> (digit * 4)) & 15];
        return value;
    };
    old.replace(hashAt, 20, hex(LaneHistoricalHash(data.World, oldDoors)));
    WorldSaveData loaded;
    const auto result = DeserializeWorldSave(old, loaded);
    Check(result.Succeeded() && result.SourceVersion == 8 && !result.HashVerified,
          "historical ten-door v8 authenticates its original hash and reports changed state honestly");
    CheckSame(data, loaded, "historical door facts survive and stationery receives its closed authored default");
    Check(QueryDoorOpen(loaded.World.Doors, "door.nabapara.stationery_front") == false, "new business starts closed");
    WorldSaveData again;
    Check(DeserializeWorldSave(SerializeWorldSave(loaded), again).HashVerified, "migrated world resaves as certified current v8");
    CheckSame(loaded, again, "migration is idempotent after resave");
    Check(ErrorOf(ReplaceOnce(old, "home_front,1,-1", "home_front,1,1")) == WorldSaveError::HashMismatch,
          "migration refuses tampered historical door facts");
    Check(ErrorOf(ReplaceOnce(old, "\"seed\": \"0x0000000000000056\"", "\"seed\": \"0x0000000000000057\"")) == WorldSaveError::HashMismatch,
          "migration authenticates the rest of the historical world too");
}

// NL-01: the reader takes exactly the rosters some writer produced. Every other prefix, a duplicate, an unknown door or
// a gap is refused; a historical roster keeps its facts and gains the authored defaults.
std::string LaneDoorText(std::string_view header, const DoorState& state, std::size_t count, bool sides)
{
    std::string text = std::string(header) + "\ncount=" + std::to_string(count) + "\n";
    for (std::size_t i = 0; i < count; ++i)
    {
        const DoorEntry& entry = state.Doors[i];
        text += "door=" + entry.Id + "," + (entry.Open ? "1" : "0") + (sides ? "," + std::to_string(entry.SwingSide) : std::string{}) + "\n";
    }
    return text + "end\n";
}

void LaneDoorRosters()
{
    DoorState played = MakeInitialDoorState();
    Check(ApplyDoorSettlement(played, "door.nabapara.home_front", true, -1) == DoorResult::Ok &&
              ApplyDoorSettlement(played, "door.nabapara.stationery_front", true, -1) == DoorResult::Ok &&
              ApplyDoorSettlement(played, "door.nabapara.care_common", false, 0) == DoorResult::Ok, "played roster");
    const auto editions = DoorRosterEditions();
    Check(editions.size() == 7 && editions.back().Header == "pw.doors/2" && editions.back().Count == 20, "seven editions end at today's twenty");
    const DoorState initial = MakeInitialDoorState();
    for (std::size_t count = 0; count <= AuthoredDoors().size() + 1; ++count)
    {
        for (const bool legacy : {false, true})
        {
            const std::size_t lines = count < AuthoredDoors().size() ? count : AuthoredDoors().size();
            std::string text = LaneDoorText(legacy ? "pw.doors/1" : "pw.doors/2", played, lines, !legacy);
            if (count != lines)
                text = ReplaceOnce(text, "count=" + std::to_string(lines), "count=" + std::to_string(count));
            const auto parsed = DeserialiseDoorState(text);
            const bool known = legacy ? (count == 5 || count == 10 || count == 11) : (count == 10 || count == 11 || count == 16 || count == 20);
            Check(parsed.Ok() == known, "only the historical roster lengths are accepted");
            if (!known)
            {
                Check(parsed.State == DoorState{} && !parsed.bMigrated, "a refused roster publishes nothing");
                continue;
            }
            Check(parsed.bMigrated == (legacy || count != 20) && IsDoorStateValid(parsed.State), "historical rosters migrate");
            for (std::size_t i = 0; i < parsed.State.Doors.size(); ++i)
            {
                const DoorEntry& expected = i < count ? played.Doors[i] : initial.Doors[i];
                Check(parsed.State.Doors[i].Id == expected.Id && parsed.State.Doors[i].Open == expected.Open, "facts kept, defaults appended");
                Check(legacy || parsed.State.Doors[i].SwingSide == expected.SwingSide, "sides kept");
            }
        }
    }
    const std::string eleven = LaneDoorText("pw.doors/2", played, 11, true);
    Check(!DeserialiseDoorState(ReplaceOnce(eleven, "stationery_front", "caretaker_flat_front")).Ok(), "a gap in the roster is refused");
    Check(!DeserialiseDoorState(ReplaceOnce(eleven, "stationery_front", "care_washroom")).Ok(), "a duplicate door is refused");
    Check(!DeserialiseDoorState(ReplaceOnce(eleven, "stationery_front", "stationery_back")).Ok(), "an unknown door is refused");
    const std::string sixteen = LaneDoorText("pw.doors/2", played, 16, true);
    Check(DeserialiseDoorState(sixteen).bMigrated && !DeserialiseDoorState(ReplaceOnce(sixteen, "tea_row_house_front", "old_house_front")).Ok(),
          "sixteen is a historical roster; a later door in its place is refused");
    const std::string twenty = LaneDoorText("pw.doors/2", played, 20, true);
    Check(DeserialiseDoorState(twenty).State == played && !DeserialiseDoorState(twenty).bMigrated, "today's roster round trips");
    Check(!DeserialiseDoorState(ReplaceOnce(twenty, "tall_flats_unit_a", "tall_flats_unit_b")).Ok(), "duplicate unit door refused");
}

#ifdef PW_DOORS_FIXTURE_DIR
std::string LaneFixture(const char* name)
{
    std::ifstream in(std::string(PW_DOORS_FIXTURE_DIR) + "/" + name, std::ios::binary);
    Check(static_cast<bool>(in), "the frozen fixture is readable");
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

/**
 * Frozen v8 saves written by the real historical writers (Tests/Fixtures/world_v8_doors_*.json): the ten-door file by the
 * core at a902a39, the eleven-door file by the core at 8bdd8b6, the sixteen-door file by the core at ec27cc7, all from
 * one generator program (Tools/DoorFixtures: Start Life, evening talk, door settlements with both swing signs; world seed
 * 0x4e4c3031). They must authenticate over their stored door text before any default is appended, and refuse every
 * tampering.
 */
void LaneFrozenDoorFixtures()
{
    for (const auto& [name, count] : {std::pair{"world_v8_doors_10.json", std::size_t{10}}, std::pair{"world_v8_doors_11.json", std::size_t{11}},
                                      std::pair{"world_v8_doors_16.json", std::size_t{16}}})
    {
        const std::string text = LaneFixture(name);
        WorldSaveData loaded;
        const auto result = DeserializeWorldSave(text, loaded);
        Check(result.Succeeded() && result.SourceVersion == 8 && !result.HashVerified && !result.Migrated, "historical v8 loads, honestly unverified");
        const std::size_t at = text.find("pw.doors/2");
        const std::string escaped = text.substr(at, text.find('"', at) - at);
        std::string storedDoors;
        for (std::size_t i = 0; i < escaped.size(); ++i)
        {
            const bool newline = escaped[i] == '\\' && i + 1 < escaped.size() && escaped[i + 1] == 'n';
            storedDoors += newline ? '\n' : escaped[i];
            i += newline ? 1 : 0;
        }
        const auto storedParse = DeserialiseDoorState(storedDoors);
        Check(storedParse.Ok() && storedParse.bMigrated && storedParse.State == loaded.World.Doors, "stored roster is historical");
        Check(storedDoors.find("count=" + std::to_string(count) + "\n") != std::string::npos, "fixture holds its roster");
        const std::uint64_t storedHash = std::stoull(text.substr(text.find("\"stateHash\": ") + 14, 18), nullptr, 16);
        Check(HashWorldStateWithDoorsText(loaded.World, storedDoors) == storedHash && HashWorldState(loaded.World) != storedHash,
              "the checksum covers the stored door text, not the appended defaults");
        Check(QueryDoorOpen(loaded.World.Doors, "door.nabapara.home_front") == true && loaded.World.Doors.Doors[0].SwingSide == -1 &&
                  loaded.World.Doors.Doors[2].SwingSide == 1 && QueryDoorOpen(loaded.World.Doors, "door.nabapara.care_common") == false,
              "historical physical facts survive");
        Check(QueryDoorOpen(loaded.World.Doors, "door.nabapara.stationery_front") == (count >= 11), "stationery: stored fact or default");
        Check(QueryDoorOpen(loaded.World.Doors, "door.nabapara.tall_flats_front") == (count == 16) &&
                  loaded.World.Doors.Doors[12].SwingSide == (count == 16 ? -1 : 0) && loaded.World.Doors.Doors[15].SwingSide == (count == 16 ? 1 : 0),
              "NL-01 doors: stored facts (open outward, open inward) or defaults");
        for (std::size_t i = count; i < AuthoredDoors().size(); ++i)
            Check(loaded.World.Doors.Doors[i] == MakeInitialDoorState().Doors[i], "doors newer than the save start at their authored defaults");
        WorldSaveData again;
        Check(DeserializeWorldSave(SerializeWorldSave(loaded), again).HashVerified, "first resave is certified current v8");
        CheckSame(loaded, again, "migration is idempotent after resave");
        Check(ErrorOf(ReplaceOnce(text, "home_front,1,-1", "home_front,1,1")) == WorldSaveError::HashMismatch, "tampered door fact refused");
        Check(ErrorOf(ReplaceOnce(text, "home_living,0,0", "home_living,1,1")) == WorldSaveError::HashMismatch, "tampered closed leaf refused");
        Check(ErrorOf(ReplaceOnce(text, "\"seed\": \"0x000000004e4c3031\"", "\"seed\": \"0x000000004e4c3032\"")) == WorldSaveError::HashMismatch,
              "the rest of the historical world is authenticated too");
        Check(ErrorOf(ReplaceOnce(text, "care_washroom,0,0", "care_common,0,0")) == WorldSaveError::InvalidWorld, "duplicate door refused");
        Check(ErrorOf(ReplaceOnce(text, "care_washroom,0,0", "care_laundry,0,0")) == WorldSaveError::InvalidWorld, "unknown door refused");
        // Drop the last door: ten from eleven is a known roster that fails the stored checksum; nine and fifteen are no roster.
        const std::string lastDoor = count == 16 ? "\\ndoor=door.nabapara.tea_row_house_front,1,1"
                                     : count == 11 ? "\\ndoor=door.nabapara.stationery_front,1,-1" : "\\ndoor=door.nabapara.care_washroom,0,0";
        const std::string shortened =
            ReplaceOnce(ReplaceOnce(text, "pw.doors/2\\ncount=" + std::to_string(count), "pw.doors/2\\ncount=" + std::to_string(count - 1)), lastDoor, "");
        Check(ErrorOf(shortened) == (count == 11 ? WorldSaveError::HashMismatch : WorldSaveError::InvalidWorld), "a shortened roster is refused");
        // A roster padded with the newer doors' defaults is the current edition, so it is checked as a current save and fails.
        if (count == 16)
            Check(ErrorOf(ReplaceOnce(ReplaceOnce(text, "count=16", "count=20"), lastDoor, lastDoor +
                      "\\ndoor=door.nabapara.repair_workshop_front,0,0\\ndoor=door.nabapara.hardware_store_front,0,0"
                      "\\ndoor=door.nabapara.old_house_front,0,0\\ndoor=door.nabapara.tall_flats_upper_a,0,0")) == WorldSaveError::HashMismatch,
                  "a sixteen-door save padded to twenty cannot pass as current");
    }
}
#endif
} // namespace

void TestWorldSaveLane()
{
    LaneRoundTripAndReplay();
    LaneMigration();
    LaneReceiptsAndCatalogues();
    LaneRefusals();
    LaneStationeryEgress();
    LaneTenDoorV8Migration();
    LaneDoorRosters();
#ifdef PW_DOORS_FIXTURE_DIR
    LaneFrozenDoorFixtures();
#else
    Check(false, "PW_DOORS_FIXTURE_DIR must be defined by CMakeLists.txt for the frozen door fixtures");
#endif
}
