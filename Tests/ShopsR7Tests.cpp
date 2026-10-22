#include "ShopsR7.h"

#include "ConversationShopsR7.h"
#include "LaneJournal.h"
#include "NpcScheduleContent.h"
#include "WorldHash.h"
#include "WorldSave.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <tuple>

using namespace PerfectWorld;
namespace
{
int R74Assertions = 0;
void R74Require(bool condition, const char* message)
{
    ++R74Assertions;
    if (!condition) { std::cerr << "R74 FAIL: " << message << '\n'; std::exit(1); }
}
GameInstant R74At(int minute, int day = 0) { return (day + 7) * MinutesPerDay + minute - 360; }
std::string R74Short(const ShopStaffR7& staff) { return std::string(staff.PersonId.substr(7)); }
std::string R74Choice(const ShopStaffR7& staff, std::string_view suffix)
{
    return "choice." + R74Short(staff) + "." + std::string(suffix);
}
JsonValue& R74Field(JsonValue& value, std::string_view field)
{
    for (auto& [key, item] : value.Object) { if (key == field) { return item; } }
    R74Require(false, "fixture field exists");
    std::exit(1);
}
ContentDb R74Db(const ShopStaffR7& staff, const JsonValue& root)
{
    ContentDb db;
    db.Records.emplace(staff.ScheduleId, ContentRecord{"npc_schedule", std::string(staff.ScheduleId), "R74.json", root, {}});
    return db;
}
NpcSchedule R74Load(const ShopStaffR7& staff, const std::filesystem::path& dir)
{
    const auto path = dir / (std::string(staff.ScheduleId) + ".json");
    std::ifstream file(path);
    R74Require(file.good(), "authored schedule file opens");
    const std::string text{std::istreambuf_iterator<char>(file), {}};
    const auto parsed = ParseJson(text);
    R74Require(parsed.Value.has_value(), "authored JSON parses");
    const auto db = R74Db(staff, *parsed.Value);
    const auto result = LoadNpcSchedule(db, staff.ScheduleId);
    if (!result.Ok()) { std::cerr << staff.ScheduleId << ": " << result.Error << '\n'; }
    R74Require(result.Ok(), "frozen typed registration loads (integrator AllSpecs required)");
    const auto registered = LoadContent({{path.string(), text}});
    R74Require(registered.Ok() && registered.Db.CountOfType("npc_schedule") == 1,
               "registered shape and semantic dispatch accept the schedule");
    auto bad = *parsed.Value;
    R74Field(bad, "person").String = "person.unknown";
    R74Require(!LoadNpcSchedule(R74Db(staff, bad), staff.ScheduleId).Ok(), "identity mutation refused");
    bad = *parsed.Value;
    R74Field(bad, "home").String = "location.nabapara.other_home";
    R74Require(!LoadNpcSchedule(R74Db(staff, bad), staff.ScheduleId).Ok(), "home mutation refused");
    bad = *parsed.Value;
    R74Field(bad, "door").String = "door.nabapara.fake_shop";
    R74Require(!LoadNpcSchedule(R74Db(staff, bad), staff.ScheduleId).Ok(), "invented door refused");
    bad = *parsed.Value;
    auto& paths = R74Field(bad, "travel").Array;
    std::swap(paths[0], paths[1]);
    R74Require(!LoadNpcSchedule(R74Db(staff, bad), staff.ScheduleId).Ok(), "unordered travel refused");
    bad = *parsed.Value;
    R74Field(R74Field(bad, "travel").Array.back(), "minutes").Integer = 1;
    R74Require(!LoadNpcSchedule(R74Db(staff, bad), staff.ScheduleId).Ok(), "short journey refused");
    bad = *parsed.Value;
    R74Field(bad, "routes").Array.pop_back();
    R74Require(!LoadNpcSchedule(R74Db(staff, bad), staff.ScheduleId).Ok(), "missing approved home route refused");
    return *result.Value;
}

void R74Geometry(const ShopStaffR7& staff, const NeighbourhoodPlan& plan)
{
    const auto* entrance = FindNeighbourhoodFeature(plan, staff.EntranceId);
    const auto* counter = FindNeighbourhoodFeature(plan, staff.PlanCounterId);
    R74Require(entrance && counter && entrance->Geometry.size() == 1 && counter->Geometry.size() == 1,
               "R71 entrance and counter are real point features");
    R74Require(entrance->RelatedId.empty(), "planned shop entrance introduces no save door");
    const auto e = entrance->Geometry.front(), c = counter->Geometry.front();
    const auto s = staff.CounterPosition;
    R74Require(s.Z == NeighbourhoodFootpathZCm && s.Y == c.Y && s.X >= c.X && s.X <= c.X + 150,
               "staff stand at the plan destination or declared assistant offset, absolute Z");
    R74Require(std::abs(e.X - s.X) + std::abs(e.Y - s.Y) == staff.EntranceRouteCm,
               "counter route equals its axis-aligned polyline length");
    R74Require(RouteNeighbourhood(plan, "walk.door.nabapara.home_front", counter->LinkNodeId).Found,
               "shop destination reachable from the preserved home route graph");
}

void R74Week(const ShopStaffR7& staff, const NpcSchedule& schedule)
{
    R74Require(ValidateNpcScheduleContent(schedule).empty(), "typed schedule revalidates");
    R74Require(schedule.Person.PersonId == staff.PersonId && schedule.Person.HomeZone == staff.HomeId,
               "frozen staff and home match");
    R74Require(schedule.Paths.size() == 3 && schedule.Routes.size() == 2, "complete three-zone all-pairs geometry");
    for (std::size_t i = 0; i < schedule.Paths.size(); ++i)
    {
        const auto& path = schedule.Paths[i];
        if (i != 0)
        {
            const auto& prev = schedule.Paths[i - 1];
            R74Require(std::tie(prev.From, prev.To) < std::tie(path.From, path.To), "strictly ascending travel pairs");
        }
        int length = 0;
        auto zone = path.From;
        for (const auto& id : path.Routes)
        {
            const auto route = std::find_if(schedule.Routes.begin(), schedule.Routes.end(),
                [&](const auto& r) { return r.Id == id; });
            R74Require(route != schedule.Routes.end() && (route->From == zone || route->To == zone), "connected route path");
            zone = route->From == zone ? route->To : route->From;
            length += route->LengthCm;
        }
        R74Require(zone == path.To && path.Minutes == (length + 139) / 140, "exact ceil(length / 140) journey");
    }
    std::set<NpcWorkPhase> phases;
    for (int day = 0; day < 7; ++day)
    {
        for (int minute = 0; minute < 1440; ++minute)
        {
            const auto at = R74At(minute, day);
            const auto block = ResolveSchedule(schedule.Person, schedule.Travel, at);
            R74Require(block.Valid && !block.Squeezed && block.Status != ScheduleStatus::Idle,
                       "every minute of all seven days is explicit and unsqueezed");
            const auto intent = ResolveNpcIntent(block, false, false);
            R74Require(intent.Ok(), "legacy role intent remains valid");
            const bool open = IsShopOpenR7(staff, minute);
            if (open || (block.Status == ScheduleStatus::Stay && block.Zone == staff.CounterId))
            {
                R74Require(block.Status == ScheduleStatus::Stay && block.Zone == staff.CounterId,
                           "at the staffed counter during every open minute");
                R74Require(open == (block.Doing == "serving" || block.Doing == "cooking" || block.Doing == "stocking"),
                           "work only in posted windows, breaks close this counter");
                const auto decision = ResolveNpcDecisionAt(schedule.Person, schedule.Travel, at);
                R74Require(decision.Ok(), "shop work/needs decision resolves");
                if (open)
                {
                    R74Require(decision.Work.Phase != NpcWorkPhase::None && !decision.Work.SafePauseKey.empty(),
                               "every work block has a safe-pause phase");
                    phases.insert(decision.Work.Phase);
                }
                else
                {
                    R74Require(decision.Work.Phase == NpcWorkPhase::None && decision.Needs.Priority >= NpcNeedPriority::Rest,
                               "closed break prioritises meal or rest and earns no work");
                }
            }
            if (block.Doing == "sleeping")
            {
                R74Require(block.Zone == staff.HomeId && !intent.Intent.bTalkable, "sleep at home is unavailable");
            }
        }
    }
    R74Require(phases.size() == 2 && phases.contains(NpcWorkPhase::Serve), "each person serves and has a distinct preparation/stock phase");
    auto whole = StartSchedulePerson(staff.PersonId, R74At(0));
    auto split = whole;
    const auto full = AdvanceSchedulePerson(schedule.Person, schedule.Travel, whole, R74At(0, 7));
    std::vector<ScheduleEvent> events;
    while (split.At < whole.At)
    {
        const auto part = AdvanceSchedulePerson(schedule.Person, schedule.Travel, split, std::min(whole.At, split.At + 137));
        R74Require(part.Ok, "split schedule advance succeeds");
        events.insert(events.end(), part.Events.begin(), part.Events.end());
    }
    R74Require(full.Ok && whole == split && full.Events == events, "week skip equals split schedule events");
}

void R74Interruptions(const NpcSchedule& schedule)
{
    for (const auto& entry : schedule.Person.Routine)
    {
        const auto start = R74At(entry.StartMinute);
        const auto block = ResolveSchedule(schedule.Person, schedule.Travel, start);
        const auto phase = ShopWorkAtR7(schedule.Person, block);
        if (phase.Phase == NpcWorkPhase::None) { continue; }
        const auto before = ResolveNpcDecisionAt(schedule.Person, schedule.Travel, start + 1);
        R74Require(before.Work.Phase == phase.Phase && before.Intent.WorkActionId == phase.Action &&
                   before.Work.SafePauseKey == phase.Pause, "exact action and safe pause mapping");
        for (const auto state : {NpcState::Chat, NpcState::Yield})
        {
            const NpcInterruptionWindow window{state, start + 1, start + 8};
            const std::span<const NpcInterruptionWindow> windows{&window, 1};
            const auto pending = ResolveNpcDecisionAt(schedule.Person, schedule.Travel, start + 1, windows);
            R74Require(pending.DeferredUntil == start + 3 && pending.Reason == NpcDecisionReason::PausePending,
                       "unsafe interruption waits for bounded safe point");
            R74Require(NextNpcDecisionBoundary(schedule.Person, schedule.Travel, start + 1, windows) == start + 3,
                       "next boundary is the safe point");
            const auto paused = ResolveNpcDecisionAt(schedule.Person, schedule.Travel, start + 3, windows);
            const auto held = ResolveNpcDecisionAt(schedule.Person, schedule.Travel, start + 7, windows);
            R74Require(paused.Intent.State == state && held.Intent.State == state &&
                       paused.Work.CompletedSteps == held.Work.CompletedSteps && held.Intent.ResumeWorkActionId == phase.Action,
                       "Chat/Yield retains phase without earning paused work");
            const auto resumed = ResolveNpcDecisionAt(schedule.Person, schedule.Travel, start + 8, windows);
            R74Require(resumed.Intent.State == NpcState::Work && resumed.Work.CompletedSteps == 1,
                       "release resumes the next unfinished step");
            const NpcInterruptionWindow cancelled{state, start + 1, start + 2};
            R74Require(ResolveNpcDecisionAt(schedule.Person, schedule.Travel, start + 2, {&cancelled, 1}).Intent ==
                       ResolveNpcDecisionAt(schedule.Person, schedule.Travel, start + 2).Intent,
                       "release before the safe point cancels the request");
        }
        const auto end = block.SegmentEnd;
        const NpcInterruptionWindow longPause{NpcState::Chat, start + 1, end + 2};
        const auto changed = ResolveNpcDecisionAt(schedule.Person, schedule.Travel, end, {&longPause, 1});
        R74Require(changed.Ok() && changed.ScheduleChanged && changed.Work.BlockStart != start,
                   "schedule boundary replaces expired work while talking");
        const auto released = ResolveNpcDecisionAt(schedule.Person, schedule.Travel, end + 2, {&longPause, 1});
        const auto current = ResolveNpcDecisionAt(schedule.Person, schedule.Travel, end + 2);
        R74Require(released.Ok() && released.Intent == current.Intent && released.Work.Phase == current.Work.Phase,
                   "release across boundary resumes current schedule rather than expired work");
        const auto all = NpcWorkEventsBetween(schedule.Person, schedule.Travel, start, end + 5, {&longPause, 1});
        const auto a = NpcWorkEventsBetween(schedule.Person, schedule.Travel, start, start + 7, {&longPause, 1});
        const auto b = NpcWorkEventsBetween(schedule.Person, schedule.Travel, start + 7, end + 5, {&longPause, 1});
        auto joined = a.Events;
        joined.insert(joined.end(), b.Events.begin(), b.Events.end());
        R74Require(all.Ok() && a.Ok() && b.Ok() && all.Events == joined, "long and split work observations match exactly");
        R74Require(std::any_of(all.Events.begin(), all.Events.end(), [](const auto& e)
            { return e.Outcome == NpcWorkOutcome::Abandoned; }), "unfinished block suffix is abandoned");
    }
}

void R74Conversations(const ShopStaffR7& staff)
{
    const auto cat = ShopsR7ConversationCatalogue();
    ConversationState state;
    const auto minute = staff.Opening.front().Open;
    R74Require(OpenConversation(cat, state, staff.PersonId, minute, PersonAvailability::Awake) == ConversationError::None,
               "staff greeting opens");
    const auto freshNode = state.NodeId;
    const auto choices = VisibleConversationChoices(cat, state);
    R74Require(choices.size() == 4 && choices.back().bIsLeave, "three free topics plus Leave");
    for (const auto& choice : choices) { R74Require(choice.CostMinutes == 0, "no conversation cost or sale"); }
    const auto agreement = ChooseConversationChoice(cat, state, R74Choice(staff, "agree"));
    R74Require(agreement.Ok() && agreement.CostMinutes == 0 && state.Facts.size() == 1 && state.Receipts.empty(),
               "one free remembered agreement, no receipt");
    const auto saved = SerialiseConversationState(cat, state);
    const auto loaded = DeserialiseConversationState(cat, saved);
    R74Require(loaded.Ok() && loaded.State == state, "open agreement round trip");
    state = loaded.State;
    R74Require(LeaveConversation(state) == ConversationError::None, "Leave after agreement");
    R74Require(OpenConversation(cat, state, staff.PersonId, minute, PersonAvailability::Awake) == ConversationError::None &&
               state.NodeId != freshNode, "remembered greeting after reload");
    const auto remembered = VisibleConversationChoices(cat, state);
    R74Require(remembered.size() == 3, "already remembered agreement withdrawn");
    R74Require(ChooseConversationChoice(cat, state, R74Choice(staff, "service")).Ok(), "service explanation reachable");
    const auto* service = FindConversationNode(cat, state.NodeId);
    R74Require(service && service->Line.find("Purchasing") != std::string_view::npos,
               "service explicitly explains unavailable purchasing");
    R74Require(LeaveConversation(state) == ConversationError::None, "Leave after service explanation");
    R74Require(OpenConversation(cat, state, staff.PersonId, minute, PersonAvailability::Awake) == ConversationError::None &&
               ChooseConversationChoice(cat, state, R74Choice(staff, "para")).Ok(), "para small talk reachable");
    R74Require(LeaveConversation(state) == ConversationError::None, "Leave after small talk");
    for (const auto availability : {PersonAvailability::Busy, PersonAvailability::Asleep})
    {
        R74Require(OpenConversation(cat, state, staff.PersonId, minute, availability) == ConversationError::None,
                   "busy/asleep explanation selectable");
        R74Require(VisibleConversationChoices(cat, state).size() == 1, "unavailable offers only Leave");
        R74Require(LeaveConversation(state) == ConversationError::None, "Leave unavailable staff");
    }
    for (int at = 0; at < MinutesPerDay; ++at)
    {
        const auto* fresh = SelectConversationNode(cat, staff.PersonId, at, {}, PersonAvailability::Awake);
        const auto* rememberedNode = SelectConversationNode(cat, staff.PersonId, at, state, PersonAvailability::Awake);
        R74Require(fresh && rememberedNode && !fresh->Line.empty() && !rememberedNode->Line.empty(),
                   "every opening/break/closing minute has an explanation with or without memory");
        const auto expectedChoices = IsShopOpenR7(staff, at) ? 3U : 0U;
        R74Require(fresh->ChoiceCount == expectedChoices && rememberedNode->ChoiceCount == expectedChoices,
                   "topics and agreement available only during posted service hours");
    }
    R74Require(!SelectConversationNode(cat, staff.PersonId, -1, state, PersonAvailability::Awake), "bad minute refused");
}

void R74PhaseIsolation(const ShopStaffR7& staff, const NpcSchedule& schedule)
{
    const auto block = ResolveSchedule(schedule.Person, schedule.Travel, R74At(staff.Opening.front().Open));
    auto otherPerson = schedule.Person;
    otherPerson.PersonId = "person.unknown";
    R74Require(ShopWorkAtR7(otherPerson, block).Phase == NpcWorkPhase::None, "unknown identity cannot acquire a shop phase");
    auto home = block;
    home.Zone = std::string(staff.HomeId);
    R74Require(ShopWorkAtR7(schedule.Person, home).Phase == NpcWorkPhase::None, "home roles cannot acquire a shop phase");
    auto rest = block;
    rest.Doing = "resting";
    R74Require(ShopWorkAtR7(schedule.Person, rest).Phase == NpcWorkPhase::None, "counter rest never acquires a work phase");
}

void R74WorldMemory()
{
    WorldSim live(74);
    R74Require(live.Submit({CommandSource::Player, AdvanceTime{600}}).Accepted, "advance to all shops' afternoon hours");
    for (const auto& staff : ShopStaffR7Definitions())
    {
        const std::string person(staff.PersonId);
        const auto clock = live.State().Clock.Now();
        R74Require(live.Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted,
                   "world catalogue registration opens staff");
        const auto session = live.State().Conversations.at(person).SessionCount;
        R74Require(live.Submit({CommandSource::Player, ConversationChoose{person, R74Choice(staff, "agree"), session}}).Accepted,
                   "agreement through reducer");
        R74Require(live.State().Clock.Now() == clock, "agreement advances no time");
        R74Require(live.Submit({CommandSource::Player, ConversationLeave{person}}).Accepted, "world Leave");
    }
    const auto journal = LaneJournalToday(live.State(), GameClock::DayNumber(live.State().Clock.Now()));
    R74Require(journal.AgreementsRemembered.size() == 5 && journal.PeopleMetToday.size() == 5,
               "five journal agreement and identity mappings integrated");
    CharacterCreator creator;
    R74Require(creator.ApplyPreset("balanced_scholar").Succeeded() && creator.StartLife().Succeeded(), "valid creation for save");
    WorldSaveData data;
    data.Creation = creator.CaptureSave();
    data.World = live.State();
    data.Player.ZoneId = "zone.street.nabapara_lane";
    WorldSaveData loaded;
    const auto saved = SerializeWorldSave(data);
    R74Require(!saved.empty() && DeserializeWorldSave(saved, loaded).HashVerified, "certified world save round trip");
    R74Require(HashWorldState(data.World) == HashWorldState(loaded.World) &&
               data.World.Conversations == loaded.World.Conversations && data.World.Journal == loaded.World.Journal,
               "all staff memory and dated encounters survive");
    WorldSim resumed(loaded.World);
    for (const auto& staff : ShopStaffR7Definitions())
    {
        const std::string person(staff.PersonId);
        R74Require(resumed.Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted,
                   "reloaded staff session opens");
        R74Require(resumed.State().Conversations.at(person).NodeId.find("remembered") != std::string::npos,
                   "world reload remembers each agreement");
        R74Require(resumed.Submit({CommandSource::Player, ConversationLeave{person}}).Accepted, "leave reloaded staff");
    }
    const auto replay = ReplayCommandLog(74, live.Log());
    R74Require(replay.Complete && replay.Hash == HashWorldState(live.State()), "staff agreements replay deterministically");
}
} // namespace

int main(int argc, char** argv)
{
    R74Require(argc == 2, "npc_schedule directory argument supplied");
    R74Require(ValidateConversationCatalogue(ShopsR7ConversationCatalogue()), "all minute/availability/fact combinations have one entry");
    R74Require(!IsShopStaffR7("person.debashis_pal") && !IsShopStaffR7("person.unknown"), "shop opt-in does not capture other people");
    const auto plan = BuildNeighbourhoodPlan(74);
    std::vector<NpcSchedule> schedules;
    std::set<int> heights;
    for (const auto& staff : ShopStaffR7Definitions())
    {
        R74Require(heights.insert(staff.HeightCm).second && IsShopStaffR7(staff.PersonId), "distinct staff height and identity");
        R74Geometry(staff, plan);
        auto schedule = R74Load(staff, argv[1]);
        R74Week(staff, schedule);
        R74Interruptions(schedule);
        R74PhaseIsolation(staff, schedule);
        R74Conversations(staff);
        schedules.push_back(std::move(schedule));
    }
    for (int minute = 420; minute < 1260; ++minute)
    {
        if (minute >= 810 && minute < 930) { continue; }
        const auto owner = ResolveSchedule(schedules[0].Person, schedules[0].Travel, R74At(minute));
        const auto assistant = ResolveSchedule(schedules[1].Person, schedules[1].Travel, R74At(minute));
        R74Require(owner.Doing == "serving" || assistant.Doing == "serving", "sweet shop always has a serving counter in open hours");
    }
    R74WorldMemory();
    std::cout << "R74: " << R74Assertions << " assertions passed\n";
}
