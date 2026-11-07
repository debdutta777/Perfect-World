#include "LaneJournal.h"

#include "WorldHash.h"
#include "WorldSave.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>

using namespace PerfectWorld;

namespace
{
void R4JExpect(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

void R4JSubmit(WorldSim& sim, CommandPayload payload)
{
    const CommandResult result = sim.Submit({CommandSource::System, std::move(payload)});
    if (!result.Accepted) { std::cerr << "refused: " << result.Why.Code << " " << result.Why.Message << '\n'; }
    R4JExpect(result.Accepted, "journal command accepted");
}

void R4JOpen(WorldSim& sim, const std::string& person, PersonAvailability availability = PersonAvailability::Awake)
{
    R4JSubmit(sim, ConversationOpen{person, availability});
}

void R4JChoose(WorldSim& sim, const std::string& person, const std::string& choice)
{
    R4JSubmit(sim, ConversationChoose{person, choice, sim.State().Conversations.at(person).SessionCount});
}

void R4JTalk(WorldSim& sim, const std::string& person, const std::string& choice)
{
    R4JOpen(sim, person);
    R4JChoose(sim, person, choice);
    R4JSubmit(sim, ConversationLeave{person});
}

void R4JAt(WorldSim& sim, GameInstant when)
{
    R4JSubmit(sim, AdvanceTime{when - sim.State().Clock.Now()});
}

// The household rota is talked about from 11:00 (node.household_adult.rota_open); a new life starts at breakfast.
void R4JMidday(WorldSim& sim)
{
    R4JAt(sim, GameClock::StartOfDay(sim.State().Clock.Now()) + 720);
}

WorldSaveData R4JSave(const WorldState& world)
{
    CharacterCreator creator;
    // R8-04: a Household Resources rank-0 life, matching the seed-only sims (and replays) whose worlds it saves.
    R4JExpect(creator.ApplyPreset("independent_generalist").Succeeded(), "preset applies");
    R4JExpect(creator.StartLife().Succeeded(), "life starts");
    WorldSaveData data;
    R4JExpect(BeginWorldFromCreation(creator.CaptureSave(), world.WorldSeed, data).Succeeded(), "save begins");
    data.World = world;
    return data;
}

std::string R4JReplace(std::string text, std::string_view from, std::string_view to)
{
    const auto at = text.find(from);
    R4JExpect(at != std::string::npos, "replacement target exists");
    text.replace(at, from.size(), to);
    return text;
}

std::string R4JHex(std::uint64_t value)
{
    std::string text = "0x0000000000000000";
    for (int i = 0; i < 16; ++i) text[17 - i] = "0123456789abcdef"[(value >> (4 * i)) & 15];
    return text;
}

void R4JDatesAndGreetings()
{
    WorldSim sim(404);
    const std::string parent(HouseholdAdultId);
    const auto day = GameClock::DayNumber(sim.State().Clock.Now());
    R4JExpect(day != sim.State().Clock.Now() / MinutesPerDay, "day index is the clock's calendar index, not epoch-minute division");
    R4JExpect(LaneJournalToday(sim.State(), day).PeopleMetToday.empty(), "new life is empty");
    R4JOpen(sim, parent);
    R4JSubmit(sim, ConversationLeave{parent});
    R4JExpect(sim.State().Journal.Encounters.empty(), "opening and leaving is not an encounter");
    for (const auto availability : {PersonAvailability::Asleep, PersonAvailability::Busy})
    {
        R4JOpen(sim, parent, availability);
        R4JExpect(!sim.Submit({CommandSource::Player, ConversationChoose{parent, "choice.household_adult.back_unsure",
                   sim.State().Conversations.at(parent).SessionCount}}).Accepted, "decline node refuses authored choice");
        R4JChoose(sim, parent, std::string(ConversationLeaveChoiceId));
    }
    R4JExpect(sim.State().Journal.Encounters.empty(), "declines do not record dates");
    const GameInstant first = sim.State().Clock.Now();
    R4JTalk(sim, parent, "choice.household_adult.back_unsure");
    R4JSubmit(sim, AdvanceTime{1});
    R4JTalk(sim, parent, "choice.household_adult.back_before_dark");
    auto today = LaneJournalToday(sim.State(), day);
    R4JExpect(today.PeopleMetToday.size() == 1 && today.PeopleMetToday[0].PersonId == parent &&
              today.PeopleMetToday[0].DisplayName == HouseholdAdultLabel && today.PeopleMetToday[0].FirstMetMinute == first,
              "repeated free greetings preserve one first encounter");
    const auto midnight = GameClock::StartOfDay(first) + MinutesPerDay;
    R4JAt(sim, midnight - 1);
    // Ambient catalogue has an awake authored choice at all hours; its two-minute charge crosses midnight.
    R4JTalk(sim, "person.arun_paul", "choice.arun_paul.round");
    R4JExpect(sim.State().Clock.Now() == midnight + 1, "paid conversation crosses midnight through game clock");
    R4JExpect(LaneJournalToday(sim.State(), day).PeopleMetToday.size() == 2 &&
              LaneJournalToday(sim.State(), day + 1).PeopleMetToday.empty(), "crossing choice belongs only to its start day");
    R4JTalk(sim, "person.arun_paul", "choice.arun_paul.round");
    R4JExpect(sim.State().Journal.Encounters.size() == 3 && LaneJournalToday(sim.State(), day + 1).PeopleMetToday.size() == 1,
              "same person on two days retains two dated rows");
    R4JExpect(IsLaneJournalValid(sim.State()), "dated world remains valid");
    WorldSaveData resumed;
    R4JExpect(DeserializeWorldSave(SerializeWorldSave(R4JSave(sim.State())), resumed).HashVerified &&
              resumed.World.Journal == sim.State().Journal &&
              LaneJournalToday(resumed.World, day) == LaneJournalToday(sim.State(), day) &&
              LaneJournalToday(resumed.World, day + 1) == LaneJournalToday(sim.State(), day + 1),
              "two dates and multiple people survive a certified round trip identically");
    const auto yesterday = LaneJournalToday(sim.State(), day);
    R4JExpect(yesterday.PeopleMetToday[0].PersonId == "person.arun_paul" && yesterday.PeopleMetToday[1].PersonId == parent,
              "projection is ordered by stable person ID, not time or insertion");
    const auto replay = ReplayCommandLog(404, sim.Log());
    R4JExpect(replay.Complete && replay.Hash == HashWorldState(sim.State()), "dated state replays deterministically");
}

void R4JReplacedAgreements()
{
    WorldSim sim(405);
    const std::string parent(HouseholdAdultId);
    R4JMidday(sim);
    R4JOpen(sim, parent);
    R4JChoose(sim, parent, "choice.household_adult.talk_rota");
    R4JChoose(sim, parent, "choice.household_adult.propose_dishes");
    R4JSubmit(sim, ConversationLeave{parent});
    const auto day = GameClock::DayNumber(sim.State().Clock.Now());
    auto view = LaneJournalToday(sim.State(), day);
    R4JExpect(view.AgreementsRemembered.size() == 1 && view.AgreementsRemembered[0].AgreementId == FactRotaDishesWeekdays,
              "specific arrangement suppresses implied generic agreement");
    R4JOpen(sim, parent);
    R4JChoose(sim, parent, "choice.household_adult.revisit_rota");
    R4JChoose(sim, parent, "choice.household_adult.propose_alternate");
    R4JSubmit(sim, ConversationLeave{parent});
    view = LaneJournalToday(sim.State(), day + 10);
    R4JExpect(view.PeopleMetToday.empty() && view.AgreementsRemembered.size() == 1 &&
              view.AgreementsRemembered[0].AgreementId == FactRotaAlternateDays &&
              view.AgreementsRemembered[0].ShortTextKey == "journal.agreement.evening_rota_alternate_days",
              "projection reads replacement facts across all days without a duplicate agreement store");
    R4JAt(sim, GameClock::StartOfDay(sim.State().Clock.Now()) + 960); // 16:00, Farida serving the afternoon.
    R4JTalk(sim, "person.farida_qadri", "choice.farida_qadri.ask_bench");
    R4JExpect(LaneJournalToday(sim.State(), day).AgreementsRemembered.size() == 1, "asked permission is not an agreement");
    R4JAt(sim, GameClock::StartOfDay(sim.State().Clock.Now()) + 1440 + 540); // next day 09:00, both proprietors' public windows.
    R4JTalk(sim, "person.mahua_saha", "choice.mahua_saha.promise_sort");
    R4JTalk(sim, "person.debashis_pal", "choice.debashis_pal.promise_bring");
    view = LaneJournalToday(sim.State(), day);
    R4JExpect(view.AgreementsRemembered.size() == 3 && view.AgreementsRemembered[0].PersonId == "person.debashis_pal" &&
              view.AgreementsRemembered[1].PersonId == parent && view.AgreementsRemembered[2].PersonId == "person.mahua_saha",
              "Round 4 agreements are derived and sorted by person");
}

void R4JRollback()
{
    WorldSim sim(406, 1);
    R4JMidday(sim);
    const GameInstant now = sim.State().Clock.Now();
    R4JSubmit(sim, ScheduleWorldEvent{now + 1, "test/journal/a"});
    R4JSubmit(sim, ScheduleWorldEvent{now + 2, "test/journal/b"});
    R4JOpen(sim, std::string(HouseholdAdultId));
    const auto before = HashWorldState(sim.State());
    const auto result = sim.Submit({CommandSource::Player, ConversationChoose{std::string(HouseholdAdultId),
        "choice.household_adult.talk_rota", 1}});
    R4JExpect(result.Why.Code != "conversation.stale_choice", "the paid choice is offered, so only the event cap refuses it");
    R4JExpect(!result.Accepted && sim.State().Journal.Encounters.empty() && HashWorldState(sim.State()) == before,
              "interrupted paid choice rolls back encounter, facts, receipts and clock atomically");
}

void R4JRoundTripAndDamage()
{
    WorldSim sim(407);
    const std::string parent(HouseholdAdultId);
    R4JTalk(sim, parent, "choice.household_adult.back_unsure");
    const auto data = R4JSave(sim.State());
    const std::string text = SerializeWorldSave(data);
    WorldSaveData loaded;
    const auto result = DeserializeWorldSave(text, loaded);
    R4JExpect(result.Succeeded() && result.SourceVersion == WorldSaveSchemaVersion && result.HashVerified, "current-schema round trip authenticates");
    R4JExpect(loaded.World.Journal == sim.State().Journal && HashWorldState(loaded.World) == HashWorldState(sim.State()) &&
              SerializeWorldSave(loaded) == text, "journal round trip is canonical and hash stable");
    const auto day = GameClock::DayNumber(0);
    const std::string row = "encounter=" + std::to_string(day) + ",0," + parent;
    const std::string journal = SerialiseLaneJournal(sim.State());
    WorldState target = sim.State();
    for (std::size_t cut = 0; cut < journal.size(); ++cut)
    {
        R4JExpect(!DeserialiseLaneJournal(journal.substr(0, cut), target), "every truncated journal envelope refused");
        R4JExpect(target.Journal == sim.State().Journal, "journal parse refusal leaves target untouched");
    }
    for (const auto& damaged : {
        R4JReplace(journal, "/1\n", "/2\n"), journal + "x",
        R4JReplace(journal, "count=1", "count=2"),
        R4JReplace(journal, "count=1", "count=01"),
        R4JReplace(journal, parent, "person.unknown"),
        R4JReplace(R4JReplace(journal, row, row + "\n" + row), "count=1", "count=2"),
        R4JReplace(journal, row, "encounter=" + std::to_string(day + 1) + ",0," + parent),
        R4JReplace(journal, row, "encounter=" + std::to_string(day) + ",-1," + parent),
        R4JReplace(journal, row, "encounter=" + std::to_string(day) + ",1," + parent)})
        R4JExpect(!DeserialiseLaneJournal(damaged, target) && target.Journal == sim.State().Journal, "invalid journal refused atomically");
    const auto R4JRefuse = [&](const std::string& damaged, WorldSaveError wanted)
    {
        WorldSaveData out = loaded;
        R4JExpect(DeserializeWorldSave(damaged, out).Error == wanted && SerializeWorldSave(out) == text, "damaged save refused atomically");
    };
    R4JRefuse(R4JReplace(text, "\"journal\":", "\"journalX\":"), WorldSaveError::Malformed);
    R4JRefuse(R4JReplace(text, "pw.lane_journal/1", "pw.lane_journal/2"), WorldSaveError::InvalidWorld);
    R4JRefuse(R4JReplace(text, "end\\n\"\n  }", "end\"\n  }"), WorldSaveError::InvalidWorld);
    // Removing a valid row leaves a valid empty envelope but must fail the world hash.
    auto changed = R4JReplace(text, "count=1\\n" + row + "\\n", "count=0\\n");
    R4JRefuse(changed, WorldSaveError::HashMismatch);
    for (std::size_t cut = 0; cut < text.size(); ++cut)
        R4JExpect(!DeserializeWorldSave(text.substr(0, cut), loaded).Succeeded(), "every truncated world save refused");
    const auto baseline = HashWorldState(data.World);
    for (int field = 0; field < 4; ++field)
    {
        auto edited = data.World;
        if (field == 0) edited.Journal.Encounters.clear();
        if (field == 1) ++edited.Journal.Encounters[0].Day;
        if (field == 2) edited.Journal.Encounters[0].PersonId = "person.farida_qadri";
        if (field == 3) ++edited.Journal.Encounters[0].FirstMetMinute;
        R4JExpect(HashWorldState(edited) != baseline, "every journal field contributes to world hash");
    }
    auto invalid = data;
    invalid.World.Journal.Encounters.push_back(invalid.World.Journal.Encounters.front());
    R4JExpect(SerializeWorldSave(invalid).empty(), "writer refuses duplicate dated encounters");
}

void R4JMigration()
{
    WorldSim sim(408);
    const std::string parent(HouseholdAdultId);
    R4JMidday(sim);
    R4JOpen(sim, parent);
    R4JChoose(sim, parent, "choice.household_adult.talk_rota");
    R4JChoose(sim, parent, "choice.household_adult.propose_dishes");
    R4JSubmit(sim, ConversationLeave{parent});
    const auto data = R4JSave(sim.State());
    std::string old = SerializeWorldSave(data);
    const auto start = old.find(",\n    \"journal\":");
    old.erase(start, old.find("\n  }", start) - start);
    old = R4JReplace(old, "\"schemaVersion\": " + std::to_string(WorldSaveSchemaVersion), "\"schemaVersion\": 8");
    old = R4JReplace(old, "\"hashVersion\": " + std::to_string(WorldHashVersion), "\"hashVersion\": 5");
    old = R4JReplace(old, R4JHex(HashWorldState(data.World)),
        R4JHex(HashWorldStateWithDoorsText(data.World, SerialiseDoorState(data.World.Doors), 5)));
    WorldSaveData loaded;
    const auto result = DeserializeWorldSave(old, loaded);
    R4JExpect(result.Succeeded() && result.SourceVersion == 8 && !result.HashVerified && loaded.World.Journal.Encounters.empty(),
              "v8 migration authenticates original state and invents no encounter dates");
    const auto day = GameClock::DayNumber(sim.State().Clock.Now());
    R4JExpect(LaneJournalToday(loaded.World, day).AgreementsRemembered == LaneJournalToday(sim.State(), day).AgreementsRemembered,
              "older save retains agreements derived from facts");
    R4JExpect(DeserializeWorldSave(SerializeWorldSave(loaded), loaded).HashVerified, "migrated world becomes certified on current-schema resave");
    WorldSim resumed(loaded.World);
    R4JAt(resumed, GameClock::StartOfDay(resumed.State().Clock.Now()) + 960); // 16:00, Farida serving the afternoon.
    R4JTalk(resumed, "person.farida_qadri", "choice.farida_qadri.decline_tea");
    R4JExpect(resumed.State().Journal.Encounters.size() == 1, "only subsequent real choices add dates after migration");
    R4JExpect(!DeserializeWorldSave(R4JReplace(old, "\"doors\":", "\"journal\": \"pw.lane_journal/1\\ncount=0\\nend\\n\", \"doors\":"), loaded).Succeeded(),
              "historical schema refuses journal fields");
    for (const char* name : {"world_v8_doors_16.json", "world_v8_doors_20.json"})
    {
        std::ifstream in(std::string(PW_JOURNAL_FIXTURE_DIR) + "/" + name, std::ios::binary);
        R4JExpect(static_cast<bool>(in), "frozen fixture readable");
        const std::string frozen{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        R4JExpect(DeserializeWorldSave(frozen, loaded).Succeeded() && loaded.World.Journal.Encounters.empty() &&
                  !loaded.World.Conversations.empty(), "frozen v8 door migrations retain facts without dates");
    }
}
} // namespace

int main()
{
    R4JDatesAndGreetings();
    R4JReplacedAgreements();
    R4JRollback();
    R4JRoundTripAndDamage();
    R4JMigration();
    std::cout << "Lane journal tests passed\n";
}
