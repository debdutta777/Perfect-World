#include "NpcScheduleContent.h"

#include "ContentSchemas.h"
#include "Conversation.h"
#include "Doors.h"
#include "LaneFrontageCatalogue.h"
#include "LaneInteriorLayout.h"
#include "LaneJournal.h"
#include "NpcBehaviour.h"
#include "WorldHash.h"
#include "WorldReducer.h"
#include "WorldSave.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>

// R6-04: Madhabi Das (alley house) and Arup Basu (stair flats). Mirrors ResidentsR5Tests.cpp, with weekday-dependent work
// blocks, the two homes' door visiting windows, and the physical bedside-to-threshold route measured against the portable layouts.
using namespace PerfectWorld;
namespace
{
namespace LI = PerfectWorld::LaneInterior;
int R64Assertions = 0;
void R64Require(bool condition, const char* message)
{
    ++R64Assertions;
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

struct R64Resident
{
    std::string Suffix;
    std::string Building;
    std::string Home;
    std::string Door;
    std::string Away;
    std::string Fact;
    int TalkMinutes;
    int WeekdayAwayMinutes;   // Monday-Friday minutes in the away zone
    int SaturdayAwayMinutes;
    ConversationCatalogue (*Catalogue)() noexcept;
    std::string Person() const { return "person." + Suffix; }
    std::string Choice(std::string_view ending) const { return "choice." + Suffix + "." + std::string(ending); }
    std::string Node(std::string_view ending) const { return "node." + Suffix + "." + std::string(ending); }
};

// Day 0 is the campaign's Monday; instant 0 is 06:00.
GameInstant R64At(int day, int minute) { return (day + 7) * MinutesPerDay + minute - 360; }

// Same host predicate as LaneLifeKnock: home anchor plus Idle/Work, including suspended Chat/Yield states.
bool R64Host(const NpcIntent& intent, std::string_view home)
{
    const auto state = intent.State == NpcState::Chat || intent.State == NpcState::Yield ? intent.ResumeState : intent.State;
    return intent.AnchorId == home && (state == NpcState::Idle || state == NpcState::Work);
}

void R64Conversation(const R64Resident& resident)
{
    const auto cat = resident.Catalogue();
    R64Require(ValidateConversationCatalogue(cat) && cat.Facts.size() == 1 && cat.Facts[0].Id == resident.Fact,
        "valid catalogue with exactly one agreement fact");
    ConversationState unpromised;
    R64Require(OpenConversation(cat, unpromised, resident.Person(), 1080, PersonAvailability::Awake) == ConversationError::None &&
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
                R64Require(count == 1, "one greeting or refusal for every minute, availability and memory state");
            }
        for (int minute : {0, 719, 720, 1079, 1080, 1439})
        {
            ConversationState state = memory;
            R64Require(OpenConversation(cat, state, resident.Person(), minute, PersonAvailability::Awake) == ConversationError::None,
                "all time-of-day windows open");
            const std::string part = minute < 720 ? "morning" : minute < 1080 ? "afternoon" : "evening";
            R64Require(state.NodeId == resident.Node(part + (remembered ? "_remembered" : "")), "time boundary and remembered greeting");
            reached.insert(state.NodeId);
            const auto offers = VisibleConversationChoices(cat, state);
            R64Require(std::any_of(offers.begin(), offers.end(), [](const auto& choice) { return choice.bIsLeave; }), "Leave always offered");
            if (!remembered)
            {
                R64Require(ChooseConversationChoice(cat, state, resident.Choice("agree")).Ok(), "agreement reachable");
                reached.insert(state.NodeId);
            }
            R64Require(ChooseConversationChoice(cat, state, resident.Choice("talk")).Ok(), "Talk reachable");
            reached.insert(state.NodeId);
            R64Require(!ChooseConversationChoice(cat, state, resident.Choice("talk")).Ok(), "paid Talk cannot repeat in a session");
            R64Require(LeaveConversation(state) == ConversationError::None && !state.Open, "Leave closes without further cost");
        }
        for (auto availability : {PersonAvailability::Busy, PersonAvailability::Asleep})
        {
            ConversationState declined = memory;
            R64Require(OpenConversation(cat, declined, resident.Person(), 1080, availability) == ConversationError::None, "refusal opens");
            reached.insert(declined.NodeId);
            const auto offers = VisibleConversationChoices(cat, declined);
            R64Require(offers.size() == 1 && offers.front().bIsLeave &&
                !ChooseConversationChoice(cat, declined, resident.Choice("agree")).Ok() &&
                !ChooseConversationChoice(cat, declined, resident.Choice("talk")).Ok(), "Busy/Asleep offer Leave alone");
            R64Require(LeaveConversation(declined) == ConversationError::None && declined.Facts == memory.Facts,
                "refusal and Leave invent no agreement");
        }
        if (!remembered)
        {
            R64Require(OpenConversation(cat, memory, resident.Person(), 1080, PersonAvailability::Awake) == ConversationError::None,
                "open agreement session");
            R64Require(ChooseConversationChoice(cat, memory, resident.Choice("agree")).Ok() &&
                LeaveConversation(memory) == ConversationError::None, "remember only the freely chosen agreement");
        }
    }
    R64Require(reached.size() == cat.Nodes.size(), "every authored node reachable");
}

void R64Week(const R64Resident& resident, const NpcSchedule& schedule)
{
    const auto* door = FindDoor(resident.Door);
    R64Require(door && door->Access == DoorAccess::Private && door->LocationId == resident.Home, "exact private home and door");
    R64Require(door->VisitingWindows.size() == 1 && door->VisitingWindows[0].OpenMinute == 960 &&
        door->VisitingWindows[0].CloseMinute == 1170, "visiting hours 16:00-19:30 on the home's door");

    for (int day = 0; day < 8; ++day)
    {
        const auto weekday = GameClock::ToCalendar(R64At(day, 600)).DayOfWeek;
        int away = 0, asleep = 0, visiting = 0, cooking = 0;
        for (int minute = 0; minute < 1440; ++minute)
        {
            const auto at = R64At(day, minute);
            const auto resolved = ResolveSchedule(schedule.Person, schedule.Travel, at);
            const auto result = ResolveNpcIntentAt(schedule.Person, schedule.Travel, at, false, false);
            R64Require(resolved.Valid && !resolved.Squeezed && resolved.Status != ScheduleStatus::Idle && result.Ok(),
                "seven complete unsqueezed days plus midnight rollover");
            const bool home = R64Host(result.Intent, resident.Home);
            DoorUseContext context;
            context.IsHostHome = home;
            const auto knock = EvaluateDoorKnock(*door, minute, context);
            away += resolved.Zone == resident.Away;
            asleep += result.Intent.Availability == PersonAvailability::Asleep;
            cooking += resolved.Doing == "cooking";
            if (!home)
                R64Require(knock.Answer == DoorKnockAnswer::NoAnswer, "away, travelling and sleeping hosts cannot admit");
            if (result.Intent.Availability == PersonAvailability::Asleep)
                R64Require(!home && knock.Answer == DoorKnockAnswer::NoAnswer, "a sleeper never answers");
            if (minute >= 960 && minute < 1170)
            {
                R64Require(home && result.Intent.Availability == PersonAvailability::Awake && result.Intent.bTalkable &&
                    knock.Answer == DoorKnockAnswer::Admitted, "awake host available throughout afternoon visiting period");
                context.HasKnocked = true;
                R64Require(EvaluateDoorPassage(*door, minute, context) == DoorResult::Ok, "answered knock permits actual entry");
                ++visiting;
            }
            else if (home)
            {
                R64Require(knock.Answer == DoorKnockAnswer::NotReceiving, "a host at home outside the window is not receiving");
            }
        }
        const bool weekdayWork = weekday >= Weekday::Monday && weekday <= Weekday::Friday;
        const int expectedAway = weekdayWork ? resident.WeekdayAwayMinutes : weekday == Weekday::Saturday ? resident.SaturdayAwayMinutes : 0;
        R64Require(expectedAway == 0 ? away == 0 : away >= expectedAway && away <= expectedAway + 40,
            "away minutes follow the weekday pattern (travel may add a few); the weekend is home");
        R64Require(asleep >= 450 && visiting == 210 && cooking >= 50, "substantial sleep, meals and full visiting coverage");
    }
    for (int minute : {1020, 1180})
    {
        const auto at = R64At(0, minute);
        const auto ordinary = ResolveNpcIntentAt(schedule.Person, schedule.Travel, at, false, false);
        for (const bool chat : {false, true})
        {
            const auto suspended = ResolveNpcIntentAt(schedule.Person, schedule.Travel, at, chat, !chat);
            R64Require(suspended.Ok() && R64Host(suspended.Intent, resident.Home) &&
                suspended.Intent.ResumeState == ordinary.Intent.State, "Chat/Yield retain real home state");
        }
    }
    DoorUseContext bounds;
    bounds.IsHostHome = true;
    R64Require(EvaluateDoorKnock(*door, 959, bounds).Answer == DoorKnockAnswer::NotReceiving &&
        EvaluateDoorKnock(*door, 1170, bounds).Answer == DoorKnockAnswer::NotReceiving &&
        EvaluateDoorKnock(*door, 960, bounds).Answer == DoorKnockAnswer::Admitted &&
        EvaluateDoorKnock(*door, 1169, bounds).Answer == DoorKnockAnswer::Admitted, "visiting bounds are end-exclusive");
    // The first arrival at the away zone and the first return, on a Monday, wait for the authored travel time.
    const auto leaving = ResolveSchedule(schedule.Person, schedule.Travel, R64At(0, 0));
    R64Require(leaving.Valid && leaving.Zone == resident.Home, "midnight at home");

    auto whole = StartSchedulePerson(resident.Person(), R64At(0, 0));
    auto split = whole;
    const auto end = R64At(8, 0);
    const auto full = AdvanceSchedulePerson(schedule.Person, schedule.Travel, whole, end);
    std::vector<ScheduleEvent> events;
    while (split.At < end)
    {
        const auto part = AdvanceSchedulePerson(schedule.Person, schedule.Travel, split, std::min(end, split.At + 37));
        R64Require(part.Ok, "partitioned schedule advance");
        events.insert(events.end(), part.Events.begin(), part.Events.end());
    }
    R64Require(full.Ok && whole == split && full.Events == events, "schedule replay independent of time partitions");
}

void R64World(const R64Resident& resident)
{
    const auto routed = ConversationCatalogueForPerson(resident.Person());
    R64Require(routed && routed->Nodes.data() == resident.Catalogue().Nodes.data(), "world catalogue registration");
    CharacterCreator creator;
    R64Require(creator.ApplyPreset("balanced_scholar").Succeeded() && creator.StartLife().Succeeded(), "started character");
    WorldSaveData data;
    R64Require(BeginWorldFromCreation(creator.CaptureSave(), 9601, data).Succeeded(), "new world");
    const auto initial = data.World;
    WorldSim live(initial);
    const auto person = resident.Person();
    R64Require(live.Submit({CommandSource::Player, AdvanceTime{720}}).Accepted, "advance to 18:00");
    R64Require(live.Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted, "open world session");
    R64Require(live.Submit({CommandSource::Player, ConversationChoose{person, resident.Choice("agree"), 1}}).Accepted, "record agreement");
    const auto before = live.State().Clock.Now();
    R64Require(live.Submit({CommandSource::Player, ConversationChoose{person, resident.Choice("talk"), 1}}).Accepted &&
        live.State().Clock.Now() == before + resident.TalkMinutes, "Talk charges declared minutes once");
    data.World = live.State();
    data.Player = {"zone.street.nabapara_lane", {400, 300, 85}, 12345, -1200, CameraMode::ThirdPerson};
    const auto text = SerializeWorldSave(data);
    WorldSaveData loaded;
    const auto parsed = DeserializeWorldSave(text, loaded);
    R64Require(!text.empty() && parsed.Succeeded() && parsed.HashVerified && parsed.SourceVersion == WorldSaveSchemaVersion,
        "current world save schema round trip with certified hash");
    R64Require(loaded.Player == data.Player && loaded.World.Conversations == data.World.Conversations &&
        HashWorldState(loaded.World) == HashWorldState(live.State()), "placement, agreement and receipts persist");
    const auto journal = LaneJournalToday(loaded.World, GameClock::DayNumber(before));
    R64Require(journal.PeopleMetToday.size() == 1 && journal.PeopleMetToday.front().PersonId == person &&
        journal.AgreementsRemembered.size() == 1 && journal.AgreementsRemembered.front().AgreementId == resident.Fact,
        "journal rows project only the agreement actually made");

    WorldSim replay(initial);
    for (const auto& command : live.Log()) R64Require(replay.Submit(command).Accepted, "accepted command replay");
    R64Require(HashWorldState(replay.State()) == HashWorldState(live.State()) &&
        replay.State().Conversations == live.State().Conversations && replay.State().Journal == live.State().Journal,
        "world replay preserves hash, receipts and dated encounters");

    WorldSim resumed(loaded.World);
    R64Require(!resumed.Submit({CommandSource::Player, ConversationChoose{person, resident.Choice("talk"), 1}}).Accepted &&
        resumed.State().Clock.Now() == before + resident.TalkMinutes, "save mid-session prevents duplicate time charge");
    R64Require(resumed.Submit({CommandSource::Player, ConversationLeave{person}}).Accepted &&
        resumed.Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted &&
        resumed.State().Conversations.at(person).NodeId == resident.Node("evening_remembered"), "later conversation acknowledges agreement");
    auto damaged = loaded;
    damaged.World.Conversations.at(person).Facts.push_back("fact." + resident.Suffix + ".invented");
    R64Require(SerializeWorldSave(damaged).empty(), "invented agreement refused");
}

double R64DistanceToRect(double u, double v, const LI::Rect& rect)
{
    const double du = std::max({0.0, rect.U0 - u, u - rect.U1});
    const double dv = std::max({0.0, rect.V0 - v, v - rect.V1});
    return std::sqrt(du * du + dv * dv);
}

// The engine binding (PerfectWorldResidentsR6Bindings.cpp) walks bed_side -> across the sleeping room to the doorway axis ->
// down the stem through the internal doorway and the front opening -> the threshold 45 cm outside. The same polyline, in
// building-local (U, V), is swept here against the portable layout with the full 34 cm capsule; the engine automation repeats it
// against the built scene. Its visible length is far below the authored off-lane allowance and is never padded.
void R64Layout(const R64Resident& resident, int authoredCm)
{
    const auto* row = LI::FindFrontageById(resident.Building);
    R64Require(row != nullptr && row->Kind == LI::Variant::CompactResidence && row->EntranceDoorId == resident.Door, "catalogue row hangs the front door");
    const auto layout = LI::GenerateInteriorLayout(LI::MakeFrontageInput(*row, 15, 300));
    R64Require(layout.Ok(), "the layout validates");
    for (const auto* suffix : {"seat_1", "seat_2", "bed_side"})
        R64Require(LI::FindAnchor(layout, resident.Building + ".anchor." + suffix) != nullptr, "kit anchor exists");
    const auto* bed = LI::FindAnchor(layout, resident.Building + ".anchor.bed_side");
    R64Require(bed != nullptr && bed->Kind == LI::AnchorKind::Stand, "bed_side is a free standing anchor");
    const LI::Opening* front = nullptr;
    const LI::Opening* internal = nullptr;
    for (const auto& opening : layout.Openings)
    {
        if (opening.DoorId == resident.Door) front = &opening;
        if (opening.Kind == LI::OpeningKind::Doorway && opening.RoomA != "outside") internal = &opening;
    }
    R64Require(front != nullptr && internal != nullptr && front->Kind == LI::OpeningKind::Door, "front door opening and internal doorway exist");
    const double doorU = (front->Plan.U0 + front->Plan.U1) * 0.5;
    const double internalU = (internal->Plan.U0 + internal->Plan.U1) * 0.5;
    R64Require(doorU == internalU, "the front and internal doorways share one axis");
    const std::array<std::pair<double, double>, 5> route{{{double(bed->U), double(bed->V)}, {internalU, double(bed->V)},
        {internalU, internal->Plan.V0 - 65.0}, {doorU, internal->Plan.V0 - 65.0}, {doorU, -45.0}}};
    double length = 0;
    for (std::size_t i = 1; i < route.size(); ++i)
    {
        const double du = route[i].first - route[i - 1].first, dv = route[i].second - route[i - 1].second;
        const double span = std::sqrt(du * du + dv * dv);
        length += span;
        const int samples = std::max(1, static_cast<int>(std::ceil(span / 2)));
        for (int s = 0; s <= samples; ++s)
        {
            const double u = route[i - 1].first + du * s / samples, v = route[i - 1].second + dv * s / samples;
            for (const auto& piece : layout.Pieces)
                if (piece.Solid) R64Require(R64DistanceToRect(u, v, piece.Footprint) >= 34 - 0.01, "route clears solid furniture by the capsule radius");
            if (v < 25) continue; // outside the facade the opening itself is the constraint
            for (const auto& wall : layout.Walls)
                if (wall.W0 < 165 && wall.W1 > 0)
                    R64Require(R64DistanceToRect(u, v, wall.Plan) >= 34 - 0.01, "route clears every wall by the capsule radius");
        }
    }
    R64Require(front->Plan.Width() >= 68 && internal->Plan.Width() >= 68, "both doorways admit the 68 cm body");
    R64Require(length > 600 && length < authoredCm, "visible route is shorter than the authored off-lane allowance, never padded");
    std::cout << resident.Suffix << ": visible route " << length << " cm, authored allowance " << authoredCm << " cm\n";
}

std::string R64Read(const char* path)
{
    std::ifstream input(path);
    R64Require(input.good(), "schedule JSON exists");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
} // namespace

int main(int argc, char** argv)
{
    const std::array<R64Resident, 2> residents{{
        {"madhabi_das", "building.nabapara.alley_house", "location.nabapara.alley_house", "door.nabapara.alley_house_front",
         "location.nabapara.madhabi_work", "fact.madhabi_das.ward_records_agreed", 15, 330, 0, MadhabiConversationCatalogue},
        {"arup_basu", "building.nabapara.stair_flats", "location.nabapara.stair_flats", "door.nabapara.stair_flats_front",
         "location.nabapara.arup_work", "fact.arup_basu.parcel_round_agreed", 10, 510, 180, ArupConversationCatalogue},
    }};
    for (const auto& resident : residents) R64Conversation(resident);
    R64Layout(residents[0], 2100);
    R64Layout(residents[1], 2800);
    R64Require(argc == 3, "supply both schedule paths");
    const auto content = LoadContent({{"/npc_schedule/npc_schedule.madhabi_das.json", R64Read(argv[1])},
                                     {"/npc_schedule/npc_schedule.arup_basu.json", R64Read(argv[2])}});
    for (const auto& issue : content.Issues) std::cerr << issue.Message << '\n';
    R64Require(content.Ok(), "content schema and frozen contracts load");
    for (const auto& resident : residents)
    {
        const auto loaded = LoadNpcSchedule(content.Db, "npc_schedule." + resident.Suffix);
        if (!loaded.Ok()) std::cerr << loaded.Error << '\n';
        R64Require(loaded.Ok() && ValidateNpcScheduleContent(*loaded.Value).empty(), "AllSpecs registration and typed validation");
        auto altered = *loaded.Value;
        altered.Person.HomeZone = &resident == &residents[0] ? residents[1].Home : residents[0].Home;
        R64Require(!ValidateNpcScheduleContent(altered).empty(), "the other resident's home cannot replace this one");
        altered = *loaded.Value;
        altered.Routes[0].LengthCm = 1;
        R64Require(!ValidateNpcScheduleContent(altered).empty(), "frozen travel contract rejects undersized routes");
        const auto& legs = loaded.Value->Travel.Legs;
        for (std::size_t i = 1; i < legs.size(); ++i)
            R64Require(std::pair{legs[i - 1].From, legs[i - 1].To} < std::pair{legs[i].From, legs[i].To}, "strict travel row ordering");
        R64Week(resident, *loaded.Value);
        R64World(resident);
    }
    std::cout << "PerfectWorldResidentsR6Tests passed (" << R64Assertions << " assertions)\n";
}
