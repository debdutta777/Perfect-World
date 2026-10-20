#include "NpcScheduleContent.h"

#include "ContentSchemas.h"
#include "Conversation.h"
#include "Doors.h"
#include "LaneFrontageCatalogue.h"
#include "LaneJournal.h"
#include "LaneTallFlatsLayout.h"
#include "NpcBehaviour.h"
#include "WorldHash.h"
#include "WorldReducer.h"
#include "WorldSave.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>

// Until the integrator adds these identical declarations to Conversation.h.
namespace PerfectWorld
{
[[nodiscard]] PERFECTWORLDCORE_API ConversationCatalogue PrabirConversationCatalogue() noexcept;
[[nodiscard]] PERFECTWORLDCORE_API ConversationCatalogue IshitaConversationCatalogue() noexcept;
}

using namespace PerfectWorld;
namespace
{
int R5RAssertions = 0;
void R5RRequire(bool condition, const char* message)
{
    ++R5RAssertions;
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

struct R5RResident
{
    std::string Suffix;
    std::string Home;
    std::string Door;
    std::string Away;
    std::string Fact;
    int TalkMinutes;
    ConversationCatalogue (*Catalogue)() noexcept;
    std::string Person() const { return "person." + Suffix; }
    std::string Choice(std::string_view ending) const { return "choice." + Suffix + "." + std::string(ending); }
    std::string Node(std::string_view ending) const { return "node." + Suffix + "." + std::string(ending); }
};

GameInstant R5RAt(int day, int minute) { return (day + 7) * MinutesPerDay + minute - 360; }

// Same host predicate as LaneLifeKnock: home anchor plus Idle/Work, including suspended Chat/Yield states.
bool R5RHost(const NpcIntent& intent, std::string_view home)
{
    const auto state = intent.State == NpcState::Chat || intent.State == NpcState::Yield ? intent.ResumeState : intent.State;
    return intent.AnchorId == home && (state == NpcState::Idle || state == NpcState::Work);
}

void R5RConversation(const R5RResident& resident)
{
    const auto cat = resident.Catalogue();
    R5RRequire(ValidateConversationCatalogue(cat) && cat.Facts.size() == 1 && cat.Facts[0].Id == resident.Fact,
        "valid catalogue with exactly one agreement fact");
    ConversationState unpromised;
    R5RRequire(OpenConversation(cat, unpromised, resident.Person(), 1080, PersonAvailability::Awake) == ConversationError::None &&
        ChooseConversationChoice(cat, unpromised, resident.Choice("talk")).Ok() &&
        LeaveConversation(unpromised) == ConversationError::None && unpromised.Facts.empty(),
        "Talk and Leave do not imply an agreement");
    std::set<std::string> reached;
    ConversationState memory;
    for (int remembered = 0; remembered < 2; ++remembered)
    {
        for (int availability = 0; availability < static_cast<int>(PersonAvailabilityCount); ++availability)
            for (int minute = 0; minute < 1440; ++minute)
            {
                int count = 0;
                for (const auto& node : cat.Nodes)
                    count += ConversationNodeServes(node, minute, memory, static_cast<PersonAvailability>(availability));
                R5RRequire(count == 1, "one greeting or refusal for every minute, availability and memory state");
            }
        for (int minute : {0, 719, 720, 1079, 1080, 1439})
        {
            ConversationState state = memory;
            R5RRequire(OpenConversation(cat, state, resident.Person(), minute, PersonAvailability::Awake) == ConversationError::None,
                "all time-of-day windows open");
            const std::string part = minute < 720 ? "morning" : minute < 1080 ? "afternoon" : "evening";
            R5RRequire(state.NodeId == resident.Node(part + (remembered ? "_remembered" : "")), "time boundary and remembered greeting");
            reached.insert(state.NodeId);
            const auto offers = VisibleConversationChoices(cat, state);
            R5RRequire(std::any_of(offers.begin(), offers.end(), [](const auto& choice) { return choice.bIsLeave; }), "Leave always offered");
            if (!remembered)
            {
                R5RRequire(ChooseConversationChoice(cat, state, resident.Choice("agree")).Ok(), "agreement reachable");
                reached.insert(state.NodeId);
            }
            R5RRequire(ChooseConversationChoice(cat, state, resident.Choice("talk")).Ok(), "Talk reachable");
            reached.insert(state.NodeId);
            R5RRequire(!ChooseConversationChoice(cat, state, resident.Choice("talk")).Ok(), "paid Talk cannot repeat in a session");
            R5RRequire(LeaveConversation(state) == ConversationError::None && !state.Open, "Leave closes without further cost");
        }
        for (auto availability : {PersonAvailability::Busy, PersonAvailability::Asleep})
        {
            ConversationState declined = memory;
            R5RRequire(OpenConversation(cat, declined, resident.Person(), 1080, availability) == ConversationError::None, "refusal opens");
            reached.insert(declined.NodeId);
            const auto offers = VisibleConversationChoices(cat, declined);
            R5RRequire(offers.size() == 1 && offers.front().bIsLeave &&
                !ChooseConversationChoice(cat, declined, resident.Choice("agree")).Ok() &&
                !ChooseConversationChoice(cat, declined, resident.Choice("talk")).Ok(), "Busy/Asleep offer Leave alone");
            R5RRequire(LeaveConversation(declined) == ConversationError::None && declined.Facts == memory.Facts,
                "refusal and Leave invent no agreement");
        }
        if (!remembered)
        {
            R5RRequire(OpenConversation(cat, memory, resident.Person(), 1080, PersonAvailability::Awake) == ConversationError::None,
                "open agreement session");
            R5RRequire(ChooseConversationChoice(cat, memory, resident.Choice("agree")).Ok() &&
                LeaveConversation(memory) == ConversationError::None, "remember only the freely chosen agreement");
        }
    }
    R5RRequire(reached.size() == cat.Nodes.size(), "every authored node reachable");
}

void R5RWeek(const R5RResident& resident, const NpcSchedule& schedule)
{
    const auto* door = FindDoor(resident.Door);
    R5RRequire(door && door->Access == DoorAccess::Private && door->LocationId == resident.Home, "exact private home and door");
    const bool oldHouse = resident.Suffix == "prabir_chatterjee";
    if (oldHouse)
        R5RRequire(door->VisitingWindows.size() == 1 && door->VisitingWindows[0].OpenMinute == 960 &&
            door->VisitingWindows[0].CloseMinute == 1170, "old-house visiting hours 16:00-19:30");
    else R5RRequire(door->VisitingWindows.empty(), "upper flat retains existing no-posted-hours policy");

    for (int day = 0; day < 8; ++day)
    {
        int meals = 0, away = 0, asleep = 0, visiting = 0;
        for (int minute = 0; minute < 1440; ++minute)
        {
            const auto at = R5RAt(day, minute);
            const auto resolved = ResolveSchedule(schedule.Person, schedule.Travel, at);
            const auto result = ResolveNpcIntentAt(schedule.Person, schedule.Travel, at, false, false);
            R5RRequire(resolved.Valid && !resolved.Squeezed && resolved.Status != ScheduleStatus::Idle && result.Ok(),
                "seven complete unsqueezed days plus midnight rollover");
            const bool home = R5RHost(result.Intent, resident.Home);
            DoorUseContext context;
            context.IsHostHome = home;
            const auto knock = EvaluateDoorKnock(*door, minute, context);
            meals += resolved.Doing == "break";
            away += resolved.Zone == resident.Away;
            asleep += result.Intent.Availability == PersonAvailability::Asleep;
            if (!home)
                R5RRequire(knock.Answer == DoorKnockAnswer::NoAnswer, "away, travelling and sleeping hosts cannot admit");
            if (minute >= 960 && minute < 1170)
            {
                R5RRequire(home && result.Intent.Availability == PersonAvailability::Awake && result.Intent.bTalkable &&
                    knock.Answer == DoorKnockAnswer::Admitted, "awake host available throughout afternoon visiting period");
                context.HasKnocked = true;
                R5RRequire(EvaluateDoorPassage(*door, minute, context) == DoorResult::Ok, "answered knock permits actual entry");
                ++visiting;
            }
        }
        R5RRequire(meals >= 75 && asleep >= 450 && visiting == 210, "daily meals, substantial sleep and visiting coverage");
        const auto weekday = GameClock::ToCalendar(R5RAt(day, 600)).DayOfWeek;
        R5RRequire(oldHouse ? away >= 90 : weekday == Weekday::Sunday ? away == 0 : away >= 450,
            "errands daily or six clinic shifts with Sunday home");
    }
    for (int minute : {1020, 1180})
    {
        const auto at = R5RAt(0, minute);
        const auto ordinary = ResolveNpcIntentAt(schedule.Person, schedule.Travel, at, false, false);
        for (const bool chat : {false, true})
        {
            const auto suspended = ResolveNpcIntentAt(schedule.Person, schedule.Travel, at, chat, !chat);
            R5RRequire(suspended.Ok() && R5RHost(suspended.Intent, resident.Home) &&
                suspended.Intent.ResumeState == ordinary.Intent.State, "Chat/Yield retain real home state");
        }
    }
    if (oldHouse)
    {
        DoorUseContext context;
        context.IsHostHome = true;
        R5RRequire(EvaluateDoorKnock(*door, 959, context).Answer == DoorKnockAnswer::NotReceiving &&
            EvaluateDoorKnock(*door, 1170, context).Answer == DoorKnockAnswer::NotReceiving, "visiting bounds are end-exclusive");
    }
    auto whole = StartSchedulePerson(resident.Person(), R5RAt(0, 0));
    auto split = whole;
    const auto end = R5RAt(8, 0);
    const auto full = AdvanceSchedulePerson(schedule.Person, schedule.Travel, whole, end);
    std::vector<ScheduleEvent> events;
    while (split.At < end)
    {
        const auto part = AdvanceSchedulePerson(schedule.Person, schedule.Travel, split, std::min(end, split.At + 37));
        R5RRequire(part.Ok, "partitioned schedule advance");
        events.insert(events.end(), part.Events.begin(), part.Events.end());
    }
    R5RRequire(full.Ok && whole == split && full.Events == events, "schedule replay independent of time partitions");
}

void R5RWorld(const R5RResident& resident)
{
    const auto routed = ConversationCatalogueForPerson(resident.Person());
    R5RRequire(routed && routed->Nodes.data() == resident.Catalogue().Nodes.data(), "integrator world catalogue registration");
    CharacterCreator creator;
    R5RRequire(creator.ApplyPreset("balanced_scholar").Succeeded() && creator.StartLife().Succeeded(), "started character");
    WorldSaveData data;
    R5RRequire(BeginWorldFromCreation(creator.CaptureSave(), 9501, data).Succeeded(), "new world");
    const auto initial = data.World;
    WorldSim live(initial);
    const auto person = resident.Person();
    R5RRequire(live.Submit({CommandSource::Player, AdvanceTime{720}}).Accepted, "advance to 18:00");
    R5RRequire(live.Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted, "open world session");
    R5RRequire(live.Submit({CommandSource::Player, ConversationChoose{person, resident.Choice("agree"), 1}}).Accepted, "record agreement");
    const auto before = live.State().Clock.Now();
    R5RRequire(live.Submit({CommandSource::Player, ConversationChoose{person, resident.Choice("talk"), 1}}).Accepted &&
        live.State().Clock.Now() == before + resident.TalkMinutes, "Talk charges declared minutes once");
    data.World = live.State();
    data.Player = {"zone.street.nabapara_lane", {400, 300, 85}, 12345, -1200, CameraMode::ThirdPerson};
    const auto text = SerializeWorldSave(data);
    WorldSaveData loaded;
    const auto parsed = DeserializeWorldSave(text, loaded);
    R5RRequire(!text.empty() && parsed.Succeeded() && parsed.HashVerified && parsed.SourceVersion == WorldSaveSchemaVersion,
        "current world save schema round trip with certified hash");
    R5RRequire(loaded.Player == data.Player && loaded.World.Conversations == data.World.Conversations &&
        HashWorldState(loaded.World) == HashWorldState(live.State()), "placement, agreement and receipts persist");
    const auto journal = LaneJournalToday(loaded.World, GameClock::DayNumber(before));
    R5RRequire(journal.PeopleMetToday.size() == 1 && journal.PeopleMetToday.front().PersonId == person &&
        journal.AgreementsRemembered.size() == 1 && journal.AgreementsRemembered.front().AgreementId == resident.Fact,
        "integrator journal rows project only the agreement actually made");

    WorldSim replay(initial);
    for (const auto& command : live.Log()) R5RRequire(replay.Submit(command).Accepted, "accepted command replay");
    R5RRequire(HashWorldState(replay.State()) == HashWorldState(live.State()) &&
        replay.State().Conversations == live.State().Conversations && replay.State().Journal == live.State().Journal,
        "world replay preserves hash, receipts and dated encounters");

    WorldSim resumed(loaded.World);
    R5RRequire(!resumed.Submit({CommandSource::Player, ConversationChoose{person, resident.Choice("talk"), 1}}).Accepted &&
        resumed.State().Clock.Now() == before + resident.TalkMinutes, "save mid-session prevents duplicate time charge");
    R5RRequire(resumed.Submit({CommandSource::Player, ConversationLeave{person}}).Accepted &&
        resumed.Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted &&
        resumed.State().Conversations.at(person).NodeId == resident.Node("evening_remembered"), "later conversation acknowledges agreement");
    auto damaged = loaded;
    damaged.World.Conversations.at(person).Facts.push_back("fact." + resident.Suffix + ".invented");
    R5RRequire(SerializeWorldSave(damaged).empty(), "invented agreement refused");
}

void R5RAnchors()
{
    namespace LI = PerfectWorld::LaneInterior;
    const auto* row = LI::FindFrontageById("building.nabapara.old_house");
    R5RRequire(row != nullptr, "old-house layout exists");
    const auto old = LI::GenerateInteriorLayout(LI::MakeFrontageInput(*row, 15, 300));
    const auto tall = LaneTallFlats::GenerateLayout();
    R5RRequire(old.Ok() && tall.Ok(), "both homes have generated layouts");
    for (const auto& suffix : {"seat_1", "seat_2", "bed_side"})
    {
        R5RRequire(LI::FindAnchor(old, std::string("building.nabapara.old_house.anchor.") + suffix), "old-house anchor exists");
        R5RRequire(LI::FindAnchor(tall.Upper, std::string("building.nabapara.tall_flats.upper_a.anchor.") + suffix), "upper-home anchor exists");
    }
}

std::string R5RRead(const char* path)
{
    std::ifstream input(path);
    R5RRequire(input.good(), "schedule JSON exists");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
} // namespace

int main(int argc, char** argv)
{
    const std::array<R5RResident, 2> residents{{
        {"prabir_chatterjee", "location.nabapara.old_house", "door.nabapara.old_house_front", "location.nabapara.prabir_errands",
         "fact.prabir_chatterjee.lane_stories_agreed", 10, PrabirConversationCatalogue},
        {"ishita_banerjee", "location.nabapara.tall_flats_upper_a", "door.nabapara.tall_flats_upper_a", "location.nabapara.ishita_work",
         "fact.ishita_banerjee.quiet_evening_agreed", 15, IshitaConversationCatalogue},
    }};
    for (const auto& resident : residents) R5RConversation(resident);
    R5RAnchors();
    if (argc == 2 && std::string_view(argv[1]) == "--catalogues-only")
    {
        std::cout << "Resident catalogues and layout anchors passed (" << R5RAssertions << " assertions); integration NOT RUN\n";
        return 0;
    }
    R5RRequire(argc == 3, "supply both schedule paths, or explicitly request --catalogues-only");
    const auto content = LoadContent({{"/npc_schedule/npc_schedule.prabir_chatterjee.json", R5RRead(argv[1])},
                                     {"/npc_schedule/npc_schedule.ishita_banerjee.json", R5RRead(argv[2])}});
    for (const auto& issue : content.Issues) std::cerr << issue.Message << '\n';
    R5RRequire(content.Ok(), "content schema and frozen contracts load");
    for (const auto& resident : residents)
    {
        const auto loaded = LoadNpcSchedule(content.Db, "npc_schedule." + resident.Suffix);
        if (!loaded.Ok()) std::cerr << loaded.Error << '\n';
        R5RRequire(loaded.Ok() && ValidateNpcScheduleContent(*loaded.Value).empty(), "integrator AllSpecs registration and typed validation");
        auto altered = *loaded.Value;
        altered.Person.HomeZone = resident.Suffix == "prabir_chatterjee" ? residents[1].Home : residents[0].Home;
        R5RRequire(!ValidateNpcScheduleContent(altered).empty(), "the other resident's home cannot replace this one");
        altered = *loaded.Value;
        altered.Routes[0].LengthCm = 1;
        R5RRequire(!ValidateNpcScheduleContent(altered).empty(), "frozen travel contract rejects undersized routes");
        const auto& legs = loaded.Value->Travel.Legs;
        for (std::size_t i = 1; i < legs.size(); ++i)
            R5RRequire(std::pair{legs[i - 1].From, legs[i - 1].To} < std::pair{legs[i].From, legs[i].To}, "strict travel row ordering");
        R5RWeek(resident, *loaded.Value);
        R5RWorld(resident);
    }
    std::cout << "PerfectWorldResidentsR5Tests passed (" << R5RAssertions << " assertions)\n";
}
