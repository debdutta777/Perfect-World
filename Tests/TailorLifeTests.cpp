#include "ConversationTailor.h"

#include "ContentRegistry.h"
#include "Doors.h"
#include "LaneFrontageCatalogue.h"
#include "LaneJournal.h"
#include "NpcBehaviour.h"
#include "NpcScheduleContent.h"
#include "WorldHash.h"
#include "WorldReducer.h"
#include "WorldSave.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>

using namespace PerfectWorld;
namespace
{
constexpr std::string_view R66Person = "person.satyajit_dutta";
constexpr std::string_view R66Fact = "fact.satyajit_dutta.bring_garment_agreed";
int R66Assertions = 0;
void R66Require(bool condition, const char* message)
{
    ++R66Assertions;
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
GameInstant R66At(int day, int minute) { return (day + 7) * MinutesPerDay + minute - 360; }

void R66Conversation()
{
    const auto cat = TailorConversationCatalogue();
    R66Require(ValidateConversationCatalogue(cat) && cat.Facts.size() == 1 && cat.Facts.front().Id == R66Fact,
        "catalogue has one agreement, no purchase fact");
    for (bool remembered : {false, true})
    {
        ConversationState memory;
        if (remembered) memory.Facts.push_back(std::string(R66Fact));
        for (int availability = 0; availability < 3; ++availability)
            for (int minute = 0; minute < 1440; ++minute)
            {
                int count = 0;
                for (const auto& node : cat.Nodes)
                    count += ConversationNodeServes(node, minute, memory, static_cast<PersonAvailability>(availability));
                R66Require(count == 1, "exactly one greeting or explained refusal at every minute");
            }
    }
    ConversationState talk;
    R66Require(OpenConversation(cat, talk, R66Person, 610, PersonAvailability::Awake) == ConversationError::None,
        "service greeting opens at a safe pause");
    R66Require(ChooseConversationChoice(cat, talk, "choice.satyajit_dutta.talk").Ok() && talk.Facts.empty(),
        "service Talk does not create an agreement or sale");
    R66Require(ChooseConversationChoice(cat, talk, "choice.satyajit_dutta.agree").Ok() &&
        HasConversationFact(talk, R66Fact) && talk.Receipts.empty(), "free agreement records only the stated intention");
    R66Require(LeaveConversation(talk) == ConversationError::None, "Leave closes");
    const auto saved = SerialiseConversationState(cat, talk);
    const auto parsed = DeserialiseConversationState(cat, saved);
    R66Require(parsed.Ok() && HasConversationFact(parsed.State, R66Fact), "agreement survives conversation round trip");
    ConversationState loaded = parsed.State;
    R66Require(OpenConversation(cat, loaded, R66Person, 900, PersonAvailability::Awake) == ConversationError::None &&
        loaded.NodeId == "node.satyajit_dutta.afternoon_remembered", "later greeting remembers agreement");
    R66Require(!ChooseConversationChoice(cat, loaded, "choice.satyajit_dutta.purchase").Ok(), "invented purchase refused");
    for (const auto availability : {PersonAvailability::Busy, PersonAvailability::Asleep})
    {
        ConversationState state;
        R66Require(OpenConversation(cat, state, R66Person, 610, availability) == ConversationError::None,
            "explained unavailability opens");
        const auto options = VisibleConversationChoices(cat, state);
        R66Require(options.size() == 1 && options.front().bIsLeave, "unavailable offers only Leave");
    }
}

void R66Geometry()
{
    namespace LI = PerfectWorld::LaneInterior;
    const auto* row = LI::FindFrontageById("building.nabapara.tailor_shop");
    R66Require(row != nullptr && row->Furniture == LI::FurnitureSet::Workshop, "existing Workshop frontage");
    const auto layout = LI::GenerateInteriorLayout(LI::MakeFrontageInput(*row, 15, 300));
    R66Require(layout.Ok(), "tailor layout resolves");
    for (const char* suffix : {"staff_counter", "customer_counter", "workbench", "finishing_table", "front_door"})
        R66Require(LI::FindAnchor(layout, layout.BuildingId + ".anchor." + suffix) != nullptr, "real work/customer anchor");
    const std::array<std::pair<const char*, int>, 3> routes{{
        {"counter_to_door", 1182}, {"counter_to_workbench", 747}, {"counter_to_finishing_table", 1137}}};
    for (const auto& [suffix, expected] : routes)
    {
        const auto* route = LI::FindRoute(layout, layout.BuildingId + ".route." + suffix);
        R66Require(route != nullptr, "kit route exists");
        int cm = 0;
        for (std::size_t i = 1; i < route->Points.size(); ++i)
            cm += std::abs(route->Points[i].U - route->Points[i - 1].U) + std::abs(route->Points[i].V - route->Points[i - 1].V);
        R66Require(cm == expected, "route length matches actual staff polyline");
    }
    const auto* bench = LI::FindAnchor(layout, layout.BuildingId + ".anchor.workbench");
    const auto* finish = LI::FindAnchor(layout, layout.BuildingId + ".anchor.finishing_table");
    R66Require(std::abs(bench->U - finish->U) + std::abs(bench->V - finish->V) == 390, "derived finishing subpath length");
}

struct R66PhaseCase { int Minute; NpcWorkPhase Phase; const char* Action; const char* Pause; };
constexpr R66PhaseCase R66Cases[] = {
    {610, NpcWorkPhase::Measure, "work.measure", "npc.pause.measure_recorded"},
    {680, NpcWorkPhase::Cut, "work.cut", "npc.pause.shears_down"},
    {740, NpcWorkPhase::Stitch, "work.stitch", "npc.pause.needle_secured"},
    {1100, NpcWorkPhase::Press, "work.press", "npc.pause.iron_stood"}};

void R66Work(const PersonSchedule& person, const ScheduleTravelTable& travel)
{
    for (const auto& sample : R66Cases)
    {
        const auto block = ResolveSchedule(person, travel, R66At(0, sample.Minute));
        const auto at = block.SegmentStart + 1;
        const auto ordinary = ResolveNpcDecisionAt(person, travel, at);
        R66Require(ordinary.Ok() && ordinary.Work.Phase == sample.Phase && ordinary.Intent.WorkActionId == sample.Action &&
            ordinary.Work.SafePauseKey == sample.Pause, "tailor phase/action/safe pause mapping");
        for (auto state : {NpcState::Chat, NpcState::Yield})
        {
            std::array windows{NpcInterruptionWindow{state, at, {}}};
            const auto pending = ResolveNpcDecisionAt(person, travel, at, windows);
            R66Require(pending.Ok() && pending.DeferredUntil == at + 2 && pending.Intent.State == NpcState::Work,
                "Chat and Yield defer to portable safe pause");
            const auto accepted = ResolveNpcDecisionAt(person, travel, at + 2, windows);
            R66Require(accepted.Ok() && accepted.Intent.State == state && accepted.Work.CompletedSteps == 1 &&
                (state != NpcState::Chat || accepted.Intent.Availability == PersonAvailability::Awake), "acceptance finishes exactly one step");
            const auto held = ResolveNpcDecisionAt(person, travel, at + 5, windows);
            R66Require(held.Work.CompletedSteps == 1, "no work completion while held");
            windows[0].ReleasedAt = at + 5;
            const auto resumed = ResolveNpcDecisionAt(person, travel, at + 5, windows);
            R66Require(resumed.Intent.State == NpcState::Work && resumed.Work.Ordinal == 1 && resumed.Work.Phase == sample.Phase,
                "release resumes next unfinished step");
            R66Require(ResolveNpcDecisionAt(person, travel, at + 8, windows).Work.CompletedSteps == 2, "next completion after resumed work");
            const auto all = NpcWorkEventsBetween(person, travel, block.SegmentStart, block.SegmentEnd, windows);
            std::vector<NpcWorkEvent> parts;
            for (auto cursor = block.SegmentStart; cursor < block.SegmentEnd;)
            {
                const auto end = std::min(block.SegmentEnd, cursor + 7);
                const auto part = NpcWorkEventsBetween(person, travel, cursor, end, windows);
                R66Require(part.Ok(), "partitioned work observations valid");
                parts.insert(parts.end(), part.Events.begin(), part.Events.end());
                cursor = end;
            }
            R66Require(all.Ok() && all.Events == parts && !all.Events.empty() &&
                all.Events.back().Outcome == NpcWorkOutcome::Abandoned, "split/long observations agree and account for unfinished suffix");
            windows[0].ReleasedAt = at + 1;
            R66Require(ResolveNpcDecisionAt(person, travel, at + 2, windows).Work.CompletedSteps == 1, "pre-acceptance cancellation adds no pause");
        }
    }
}

void R66Week(const NpcSchedule& schedule)
{
    const auto* door = FindDoor("door.nabapara.tailor_shop_front");
    R66Require(door != nullptr && door->OpeningWindows.size() == 2, "posted split hours exist");
    for (int day = 0; day < 7; ++day)
        for (int minute = 0; minute < 1440; ++minute)
        {
            const auto at = R66At(day, minute);
            const auto r = ResolveSchedule(schedule.Person, schedule.Travel, at);
            R66Require(r.Valid && !r.Squeezed && r.Status != ScheduleStatus::Idle, "full unsqueezed seven-day coverage");
            const bool open = std::any_of(door->OpeningWindows.begin(), door->OpeningWindows.end(), [minute](const auto& w)
                { return minute >= w.OpenMinute && minute < w.CloseMinute; });
            const auto d = ResolveNpcDecisionAt(schedule.Person, schedule.Travel, at);
            R66Require(d.Ok(), "every decision resolves");
            const bool work = d.Work.Phase != NpcWorkPhase::None;
            const bool internalWalk = r.Status == ScheduleStatus::Travel && r.FromZone.starts_with("anchor.tailor_shop.") &&
                r.Zone.starts_with("anchor.tailor_shop.");
            R66Require(open == (work || internalWalk), "work or internal work travel exactly covers posted hours");
            if (minute >= 780 && minute < 900)
                R66Require(d.Needs.Priority == (minute < 800 ? NpcNeedPriority::Meal : NpcNeedPriority::Rest), "midday closure meal then rest");
        }
    std::array windows{NpcInterruptionWindow{NpcState::Chat, R66At(0, 779), R66At(0, 805)}};
    const auto boundary = ResolveNpcDecisionAt(schedule.Person, schedule.Travel, R66At(0, 805), windows);
    R66Require(boundary.Ok() && boundary.ScheduleChanged && boundary.Work.Phase == NpcWorkPhase::None &&
        boundary.Needs.Priority == NpcNeedPriority::Rest, "Leave after closure selects current break, never old work");
    const auto asleep = ResolveNpcDecisionAt(schedule.Person, schedule.Travel, R66At(0, 1400));
    R66Require(asleep.Ok() && asleep.Intent.State == NpcState::Sleep && asleep.Needs.Priority == NpcNeedPriority::Sleep, "home sleep priority");
    R66Work(schedule.Person, schedule.Travel);
}

void R66World()
{
    const auto cat = ConversationCatalogueForPerson(R66Person);
    R66Require(cat && cat->Nodes.data() == TailorConversationCatalogue().Nodes.data(), "integrator catalogue routing installed");
    CharacterCreator creator;
    R66Require(creator.ApplyPreset("balanced_scholar").Succeeded() && creator.StartLife().Succeeded(), "character starts");
    WorldSaveData data;
    R66Require(BeginWorldFromCreation(creator.CaptureSave(), 6606, data).Succeeded(), "world starts");
    WorldSim sim(data.World);
    const std::string person(R66Person);
    R66Require(sim.Submit({CommandSource::Player, AdvanceTime{250}}).Accepted &&
        sim.Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted,
        "world conversation opens during service");
    const auto before = sim.State().Clock.Now();
    R66Require(sim.Submit({CommandSource::Player, ConversationChoose{person, "choice.satyajit_dutta.agree", 1}}).Accepted &&
        sim.Submit({CommandSource::Player, ConversationLeave{person}}).Accepted && sim.State().Clock.Now() == before,
        "agreement and Leave are free");
    data.World = sim.State();
    const auto text = SerializeWorldSave(data);
    WorldSaveData loaded;
    R66Require(!text.empty() && DeserializeWorldSave(text, loaded).HashVerified &&
        HashWorldState(loaded.World) == HashWorldState(sim.State()), "world round trip preserves canonical state");
    const auto& memory = loaded.World.Conversations.at(person);
    R66Require(memory.Facts == std::vector<std::string>{std::string(R66Fact)} && memory.Receipts.empty(), "exactly one intention, no purchase receipt");
    const auto journal = LaneJournalToday(loaded.World, GameClock::DayNumber(before));
    R66Require(journal.PeopleMetToday.size() == 1 && journal.PeopleMetToday.front().DisplayName == "Satyajit Dutta" &&
        journal.AgreementsRemembered.size() == 1 && journal.AgreementsRemembered.front().AgreementId == R66Fact,
        "journal integration names tailor and actual agreement");
    WorldSim resumed(loaded.World);
    R66Require(resumed.Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted &&
        resumed.State().Conversations.at(person).NodeId == "node.satyajit_dutta.morning_remembered", "remembered greeting after world reload");
}
}

int main(int argc, char** argv)
{
    R66Conversation();
    R66Geometry();
    R66Require(argc == 2, "supply tailor schedule JSON");
    std::ifstream input(argv[1]);
    R66Require(input.good(), "schedule file exists");
    const std::string json{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const auto content = LoadContent({{"/npc_schedule/npc_schedule.satyajit_dutta.json", json}});
    for (const auto& issue : content.Issues) std::cerr << issue.Message << '\n';
    R66Require(content.Ok(), "content schema and integrator frozen schedule contract");
    const auto loaded = LoadNpcSchedule(content.Db, "npc_schedule.satyajit_dutta");
    if (!loaded.Ok()) std::cerr << loaded.Error << '\n';
    R66Require(loaded.Ok(), "integrator AllSpecs registration installed");
    const auto& schedule = *loaded.Value;
    for (std::size_t i = 1; i < schedule.Travel.Legs.size(); ++i)
        R66Require(std::pair{schedule.Travel.Legs[i - 1].From, schedule.Travel.Legs[i - 1].To} <
            std::pair{schedule.Travel.Legs[i].From, schedule.Travel.Legs[i].To}, "strictly ascending travel pairs");
    auto malformed = schedule;
    malformed.Person.HomeZone = "location.nabapara.other";
    R66Require(!ValidateNpcScheduleContent(malformed).empty(), "wrong home refused");
    malformed = schedule;
    malformed.Routes.front().LengthCm = 1;
    R66Require(!ValidateNpcScheduleContent(malformed).empty(), "shortened route refused");
    R66Week(schedule);
    R66World();
    std::cout << "TailorLifeTests passed (" << R66Assertions << " assertions)\n";
}
