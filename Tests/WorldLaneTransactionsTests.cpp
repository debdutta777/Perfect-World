#include "WorldReducer.h"

#include <cstdlib>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

// Order CK+TX (LANE_LIFE_DISCUSSION_2026-10-14): due-now draining, cap preservation, chronological events, split-advance
// equivalence, and the atomic, exactly-once conversation charge.
using namespace PerfectWorld;

int Checks = 0;
void Check(bool condition, const char* message)
{
    ++Checks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

namespace
{
constexpr std::string_view Farida = "person.farida_qadri";
constexpr std::string_view SitWithTea = "choice.farida_qadri.sit_with_tea";
constexpr std::string_view DeclineTea = "choice.farida_qadri.decline_tea";

Command Advance(std::int64_t minutes)
{
    return {CommandSource::Player, AdvanceTime{minutes}};
}
Command Schedule(GameInstant when, std::string id)
{
    return {CommandSource::System, ScheduleWorldEvent{when, std::move(id)}};
}
Command Settle()
{
    return {CommandSource::System, SettleDueEvents{}};
}
Command Open(std::string_view person, PersonAvailability availability = PersonAvailability::Awake)
{
    return {CommandSource::Player, ConversationOpen{std::string(person), availability}};
}
Command Choose(std::string_view person, std::string_view choice, std::uint32_t session)
{
    return {CommandSource::Player, ConversationChoose{std::string(person), std::string(choice), session}};
}
Command Leave(std::string_view person)
{
    return {CommandSource::Player, ConversationLeave{std::string(person)}};
}

/** Everything observable about a world except the command count, as text. */
std::string WorldText(const WorldState& s)
{
    std::ostringstream out;
    out << s.Clock.Now() << '|' << s.DayLoop.Now << '|';
    for (const ScheduledEvent& e : s.Clock.Pending())
        out << e.Handle << '@' << e.When << ':' << e.Id << ',';
    out << '|';
    for (const auto& [id, value] : s.Flags)
        out << id << '=' << value << ',';
    out << '|';
    for (const auto& [person, conversation] : s.Conversations)
        out << person << '{' << SerialiseConversationState(*ConversationCatalogueForPerson(person), conversation) << '}';
    out << '|' << SerialiseDoorState(s.Doors);
    return out.str();
}

std::string Snapshot(const WorldSim& sim)
{
    return WorldText(sim.State()) + '|' + std::to_string(sim.State().Seq) + '|' + std::to_string(sim.Log().size());
}

/** The fired events and midnights of a result, as "f<handle>@<at>" and "d<day>@<at>". */
void History(const CommandResult& r, std::vector<std::string>& out)
{
    for (const DomainEvent& e : r.Events)
    {
        if (const auto* f = std::get_if<ScheduledEventFired>(&e.Payload))
            out.push_back("f" + std::to_string(f->Handle) + "@" + std::to_string(e.At));
        else if (const auto* d = std::get_if<DayStarted>(&e.Payload))
            out.push_back("d" + std::to_string(d->Day) + "@" + std::to_string(e.At));
    }
}

bool Chronological(const CommandResult& r)
{
    GameInstant last = r.From;
    for (const DomainEvent& e : r.Events)
    {
        if (e.At < last || e.At > r.To)
            return false;
        last = e.At;
    }
    return true;
}

std::size_t CountFired(const CommandResult& r)
{
    std::size_t n = 0;
    for (const DomainEvent& e : r.Events)
        n += std::holds_alternative<ScheduledEventFired>(e.Payload) ? 1 : 0;
    return n;
}

void TestDueNow()
{
    WorldSim sim(1);
    Check(sim.Submit(Advance(100)).Accepted, "advance to 100");
    Check(sim.Submit(Settle()).Why.Code == "time.nothing_due", "settling with nothing due is refused");
    Check(sim.Submit(Schedule(100, "test/now/a")).Accepted && sim.Submit(Schedule(101, "test/next/b")).Accepted, "schedule now and next");
    Check(!IsWorldQuiescent(sim.State()), "an event for this instant leaves the world unsettled");
    const CommandResult settled = sim.Submit(Settle());
    Check(settled.Accepted && settled.From == 100 && settled.To == 100 && sim.State().Clock.Now() == 100 && sim.State().DayLoop.Now == 100,
          "settling fires without moving time");
    Check(CountFired(settled) == 1 && settled.Events.front().At == 100 && IsWorldQuiescent(sim.State()), "the due event fired at its own instant");

    // Through an advance: the due event fires at Now, not Now + 1, before anything later.
    Check(sim.Submit(Schedule(100, "test/now/c")).Accepted, "schedule another for now");
    const CommandResult step = sim.Submit(Advance(1));
    Check(step.Accepted && CountFired(step) == 2 && Chronological(step), "the advance fires the due event and the next one");
    Check(std::holds_alternative<TimeAdvanced>(step.Events[0].Payload) && step.Events[1].At == 100 && step.Events[2].At == 101,
          "the due-now event is stamped at its instant, before the next minute's");
}

void TestCapPreservation()
{
    WorldSim sim(2, 3);
    for (int i = 0; i < 5; ++i)
        Check(sim.Submit(Schedule(100, "test/burst/" + std::to_string(i))).Accepted, "schedule a burst");
    Check(sim.Submit(Schedule(200, "test/after/x")).Accepted && sim.Submit(Schedule(200, "test/after/y")).Accepted, "schedule later");
    const CommandResult first = sim.Submit(Advance(1'000));
    Check(first.Accepted && first.Partial && first.To == 100 && CountFired(first) == 3, "the cap stops the advance at the burst's instant");
    Check(sim.State().Clock.Pending().size() == 4 && !IsWorldQuiescent(sim.State()), "the unfired burst events stay pending and due");
    Check(sim.State().DayLoop.Now == 100, "the day stops with the clock");
    Check(sim.Submit(Open(Farida)).Why.Code == "world.unsettled", "no conversation while the world is unsettled");

    const CommandResult settle = sim.Submit(Settle());
    Check(settle.Accepted && !settle.Partial && settle.To == 100 && CountFired(settle) == 2 && IsWorldQuiescent(sim.State()),
          "the continuation drains the rest at the same instant");
    const CommandResult rest = sim.Submit(Advance(900));
    Check(rest.Accepted && !rest.Partial && rest.To == 1'000 && CountFired(rest) == 2 && sim.State().Clock.Pending().empty(),
          "the remaining time fires the later events and reaches the target");

    // A spent cap stops the advance at the last fired event even when the rest is quiet; nothing is due there, and the
    // resubmitted remainder reaches the target.
    WorldSim exact(3, 2);
    Check(exact.Submit(Schedule(10, "test/a/1")).Accepted && exact.Submit(Schedule(20, "test/a/2")).Accepted, "two events");
    const CommandResult spent = exact.Submit(Advance(500));
    Check(spent.Accepted && spent.Partial && spent.To == 20 && IsWorldQuiescent(exact.State()), "a spent cap stops at the last fired event");
    const CommandResult remainder = exact.Submit(Advance(500 - exact.State().Clock.Now()));
    Check(remainder.Accepted && !remainder.Partial && remainder.To == 500 && CountFired(remainder) == 0, "the remainder reaches the target");

    // Midnight is announced once, even when the cap splits the events at midnight.
    WorldSim night(4, 1);
    const GameInstant midnight = GameClock::StartOfDay(0) + MinutesPerDay;
    Check(night.Submit(Schedule(midnight, "test/mid/1")).Accepted && night.Submit(Schedule(midnight, "test/mid/2")).Accepted, "two at midnight");
    std::vector<std::string> history;
    History(night.Submit(Advance(midnight + 10)), history);
    while (!IsWorldQuiescent(night.State()))
        History(night.Submit(Settle()), history);
    History(night.Submit(Advance(midnight + 10 - night.State().Clock.Now())), history);
    Check(history == std::vector<std::string>({"d" + std::to_string(GameClock::DayNumber(midnight)) + "@" + std::to_string(midnight),
                                               "f1@" + std::to_string(midnight), "f2@" + std::to_string(midnight)}),
          "one DayStarted, then both midnight events, across the continuation");
}

/** Random schedules; `whole` advances in one command, `split` in random pieces under a small cap. */
void TestSplitEquivalenceAndOrder()
{
    std::mt19937_64 random(2042);
    for (int trial = 0; trial < 60; ++trial)
    {
        WorldSim whole(11);
        WorldSim split(11, 1 + random() % 3);
        const GameInstant target = 3 * MinutesPerDay + static_cast<GameInstant>(random() % 600);
        for (int i = 0; i < 25; ++i)
        {
            const GameInstant when = static_cast<GameInstant>(random() % static_cast<std::uint64_t>(target + 1));
            const std::string id = "test/rand/" + std::to_string(i);
            Check(whole.Submit(Schedule(when, id)).Accepted && split.Submit(Schedule(when, id)).Accepted, "schedule the same");
        }
        std::vector<std::string> one;
        std::vector<std::string> many;
        const CommandResult once = whole.Submit(Advance(target));
        Check(once.Accepted && !once.Partial && Chronological(once), "one advance is complete and chronological");
        History(once, one);
        int guard = 0;
        while ((split.State().Clock.Now() < target || !IsWorldQuiescent(split.State())) && ++guard < 10'000)
        {
            const GameInstant left = target - split.State().Clock.Now();
            const CommandResult piece = left > 0 ? split.Submit(Advance(1 + static_cast<std::int64_t>(random() % static_cast<std::uint64_t>(left))))
                                                 : split.Submit(Settle());
            Check(piece.Accepted && Chronological(piece), "every piece is accepted and chronological");
            Check(split.State().DayLoop.Now == split.State().Clock.Now(), "the day follows the clock after every piece");
            History(piece, many);
        }
        Check(one == many, "the split advance fires the same events and midnights at the same instants, in the same order");
        Check(WorldText(whole.State()) == WorldText(split.State()), "and ends in the same world");
    }
}

void TestConversationCharge()
{
    WorldSim sim(5);
    // The epoch is 06:00: Farida is serving her first pot.
    const CommandResult opened = sim.Submit(Open(Farida));
    Check(opened.Accepted && OpenConversationPersonId(sim.State()) == Farida, "the conversation opens");
    const auto* event = std::get_if<ConversationOpened>(&opened.Events.at(0).Payload);
    Check(event != nullptr && event->Session == 1 && event->NodeId == "node.farida_qadri.serving_morning", "at her morning node, session 1");
    Check(sim.Submit(Advance(5)).Why.Code == "time.conversation_open", "reading a conversation passes no time");
    Check(sim.Submit(Open(HouseholdAdultId)).Why.Code == "conversation.already_open", "one session at a time");

    Check(sim.Submit(Choose(Farida, DeclineTea, 1)).Accepted && sim.State().Clock.Now() == 0, "a free choice costs nothing");
    const std::string beforePaid = Snapshot(sim);
    const CommandResult paid = sim.Submit(Choose(Farida, SitWithTea, 1));
    Check(paid.Accepted && paid.From == 0 && paid.To == 10 && sim.State().Clock.Now() == 10 && sim.State().DayLoop.Now == 10,
          "the paid choice charges its ten minutes through the clock");
    const auto* accepted = std::get_if<ConversationChoiceAccepted>(&paid.Events.at(0).Payload);
    const auto* advanced = std::get_if<TimeAdvanced>(&paid.Events.at(1).Payload);
    Check(accepted != nullptr && accepted->ChoiceId == SitWithTea && accepted->Session == 1 && accepted->CostMinutes == 10 &&
              advanced != nullptr && advanced->From == 0 && advanced->To == 10,
          "the acceptance and its charge are one command's events");
    Check(WasChoiceChargedThisSession(sim.State().Conversations.at(std::string(Farida)), SitWithTea), "the receipt is in world state");
    Check(Snapshot(sim) != beforePaid, "the world changed once");

    const std::string afterPaid = Snapshot(sim);
    for (int i = 0; i < 5; ++i)
    {
        const CommandResult again = sim.Submit(Choose(Farida, SitWithTea, 1));
        Check(!again.Accepted && (again.Why.Code == "conversation.stale_choice" || again.Why.Code == "conversation.already_charged"),
              "a duplicate submit is refused by the conversation module");
        Check(again.Events.empty() && Snapshot(sim) == afterPaid, "and changes nothing, so the receipt is charged exactly once");
    }

    Check(sim.Submit(Leave(Farida)).Accepted && OpenConversationPersonId(sim.State()).empty(), "leaving is free");
    Check(sim.State().Clock.Now() == 10, "leaving spent no time");
    Check(sim.Submit(Choose(Farida, SitWithTea, 1)).Why.Code == "conversation.not_open", "after Leave the old choice is refused");
    Check(sim.Submit(Open(Farida)).Accepted && sim.State().Conversations.at(std::string(Farida)).SessionCount == 2, "reopen: session 2");
    const std::string reopened = Snapshot(sim);
    Check(sim.Submit(Choose(Farida, SitWithTea, 1)).Why.Code == "conversation.stale_session" && Snapshot(sim) == reopened,
          "a replay of the session-1 command cannot charge in session 2");
    const CommandResult second = sim.Submit(Choose(Farida, SitWithTea, 2));
    Check(second.Accepted && sim.State().Clock.Now() == 20, "a new session may sit for tea again: a new receipt, a new charge");

    // Replay: the accepted log rebuilds the identical world, receipts included.
    WorldSim replay(5);
    for (const Command& command : sim.Log())
        Check(replay.Submit(command).Accepted, "every logged command replays");
    Check(Snapshot(replay) == Snapshot(sim), "the replay reaches the same world");

    // "choice.leave" through Choose ends the session free.
    const CommandResult left = sim.Submit(Choose(Farida, ConversationLeaveChoiceId, 2));
    Check(left.Accepted && left.Events.size() == 1 && std::holds_alternative<ConversationEnded>(left.Events[0].Payload) &&
              sim.State().Clock.Now() == 20,
          "Leave as a choice ends the session at no cost");

    // People keep separate memories, each valid under their own catalogue.
    Check(sim.Submit(Open(HouseholdAdultId)).Accepted && sim.Submit(Leave(HouseholdAdultId)).Accepted, "talk to the household adult");
    Check(sim.State().Conversations.size() == 2, "two people, two states");
    for (const auto& [person, conversation] : sim.State().Conversations)
        Check(IsConversationStateValid(*ConversationCatalogueForPerson(person), conversation), "each state is valid under its catalogue");
}

void TestRejectionAtomicity()
{
    // The event cap would stop the charge short: the whole choice is refused, nothing changes.
    WorldSim capped(6, 2);
    Check(capped.Submit(Open(Farida)).Accepted && capped.Submit(Choose(Farida, DeclineTea, 1)).Accepted, "open and decline");
    for (int i = 0; i < 3; ++i)
        Check(capped.Submit(Schedule(5, "test/during/" + std::to_string(i))).Accepted, "three events inside the ten minutes");
    const std::string before = Snapshot(capped);
    const CommandResult refused = capped.Submit(Choose(Farida, SitWithTea, 1));
    Check(!refused.Accepted && refused.Why.Code == "conversation.charge_interrupted" && refused.Events.empty() && refused.To == refused.From,
          "a charge the cap would cut short is refused");
    Check(Snapshot(capped) == before, "with no facts, receipt, time or fired events left behind");

    WorldSim roomy(6);
    Check(roomy.Submit(Open(Farida)).Accepted && roomy.Submit(Choose(Farida, DeclineTea, 1)).Accepted, "open and decline");
    for (int i = 0; i < 3; ++i)
        Check(roomy.Submit(Schedule(5, "test/during/" + std::to_string(i))).Accepted, "three events inside the ten minutes");
    const CommandResult paid = roomy.Submit(Choose(Farida, SitWithTea, 1));
    Check(paid.Accepted && CountFired(paid) == 3 && paid.To == 10 && Chronological(paid), "with room, the events fire inside the charge");

    // Every other refusal leaves the world identical.
    WorldSim sim(7);
    const std::string fresh = Snapshot(sim);
    struct Case
    {
        Command command;
        const char* code;
    };
    const Case cases[] = {
        {Open("person.nobody"), "conversation.unknown_person"},
        {Open(""), "conversation.unknown_person"},
        {Choose(Farida, SitWithTea, 0), "conversation.not_open"},
        {Leave(Farida), "conversation.not_open"},
        {{CommandSource::Player, DoorSet{"door.nowhere", true, {}}}, "door.unknown_door"},
        {{CommandSource::Player, DoorSet{"door.nabapara.home_front", true, {}}}, "door.private"},
        {{CommandSource::Player, DoorSet{"door.nabapara.home_living", true, {}}}, "door.no_change"},
        {{CommandSource::Player, DoorSettle{"door.nabapara.home_front", true, 1}}, "door.source"},
        {{CommandSource::Presentation, DoorSettle{"door.nabapara.home_front", false, 0}}, "door.no_change"},
    };
    for (const Case& c : cases)
    {
        const CommandResult r = sim.Submit(c.command);
        Check(!r.Accepted && r.Why.Code == c.code && r.Events.empty(), "each invalid lane command is refused with its code");
        Check(Snapshot(sim) == fresh, "and leaves the world identical");
    }
    Check(sim.Submit(Open(Farida)).Accepted, "open");
    const std::string open = Snapshot(sim);
    Check(sim.Submit(Choose(Farida, "choice.farida_qadri.not_offered", 1)).Why.Code == "conversation.stale_choice" && Snapshot(sim) == open,
          "a choice not offered is refused without a trace");
    Check(sim.Submit(Choose(Farida, SitWithTea, 2)).Why.Code == "conversation.stale_session" && Snapshot(sim) == open,
          "a session number not yet open is refused");

    // A paid choice while the player is asleep (a loaded state) is refused by the shared time checks.
    WorldState asleep = sim.State();
    asleep.DayLoop.Asleep = true;
    WorldSim sleeper(asleep);
    const std::string sleeping = Snapshot(sleeper);
    Check(sleeper.Submit(Choose(Farida, SitWithTea, 1)).Why.Code == "time.asleep" && Snapshot(sleeper) == sleeping,
          "the charge is checked before anything changes");
    Check(sleeper.Submit(Choose(Farida, DeclineTea, 1)).Accepted && sleeper.State().Clock.Now() == 0, "a free choice needs no time");
    WorldState asleepClosed = MakeWorldState(8);
    asleepClosed.DayLoop.Asleep = true;
    Check(WorldSim(asleepClosed).Submit(Open(Farida)).Why.Code == "conversation.player_asleep", "a sleeping player opens no conversation");

    // Doors through their rules.
    WorldSim doors(9);
    DoorUseContext resident;
    resident.IsResident = true;
    const CommandResult opened = doors.Submit({CommandSource::Player, DoorSet{"door.nabapara.home_front", true, resident}});
    const auto* changed = opened.Events.empty() ? nullptr : std::get_if<DoorChanged>(&opened.Events[0].Payload);
    Check(opened.Accepted && changed != nullptr && changed->Open && !changed->Settlement && QueryDoorOpen(doors.State().Doors, "door.nabapara.home_front") == true,
          "a resident opens the front door");
    const CommandResult settled = doors.Submit({CommandSource::Presentation, DoorSettle{"door.nabapara.home_front", false, 0}});
    Check(settled.Accepted && QueryDoorOpen(doors.State().Doors, "door.nabapara.home_front") == false, "the presentation reports it swung shut");
}

void TestRouting()
{
    const std::string_view people[] = {HouseholdAdultId, "person.care_worker", Farida};
    for (const std::string_view person : people)
    {
        const std::optional<ConversationCatalogue> catalogue = ConversationCatalogueForPerson(person);
        Check(catalogue.has_value() && ValidateConversationCatalogue(*catalogue), "each lane person has a valid catalogue");
        bool speaks = false;
        for (const ConversationNodeDef& node : catalogue->Nodes)
            speaks = speaks || node.PersonId == person;
        Check(speaks, "and it is the catalogue that speaks for them");
    }
    Check(!ConversationCatalogueForPerson("person.unknown") && !ConversationCatalogueForPerson(""), "an unknown person has none");
    Check(IsLaneCommand(ConversationOpen{}) && IsLaneCommand(DoorSettle{}) && !IsLaneCommand(AdvanceTime{}), "lane commands are recognised");
}
} // namespace

int main()
{
    TestDueNow();
    TestCapPreservation();
    TestSplitEquivalenceAndOrder();
    TestConversationCharge();
    TestRejectionAtomicity();
    TestRouting();
    std::cout << "PASS: " << Checks << " world lane transaction checks\n";
    return 0;
}
