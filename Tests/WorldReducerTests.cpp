#include "WorldReducer.h"

#include <cstdlib>
#include <iostream>
#include <random>
#include <sstream>

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
Command Advance(std::int64_t minutes)
{
    return {CommandSource::Player, AdvanceTime{minutes}};
}
Command Schedule(GameInstant when, std::string id, CommandSource source = CommandSource::System)
{
    return {source, ScheduleWorldEvent{when, std::move(id)}};
}
Command Flag(std::string id, bool value)
{
    return {CommandSource::Player, SetWorldFlag{std::move(id), value}};
}

/** Everything observable about a world, as text, so two worlds can be compared exactly. */
std::string Snapshot(const WorldSim& sim)
{
    std::ostringstream out;
    const WorldState& s = sim.State();
    out << s.WorldSeed << '|' << s.Seq << '|' << s.Clock.Now() << '|';
    for (const ScheduledEvent& e : s.Clock.Pending())
        out << e.Handle << '@' << e.When << ':' << e.Id << ',';
    out << '|';
    for (const auto& [id, value] : s.Flags)
        out << id << '=' << value << ',';
    out << '|' << sim.Log().size();
    return out.str();
}

std::string Render(const CommandResult& r)
{
    std::ostringstream out;
    out << r.Accepted << ' ' << r.Why.Code << ' ' << r.Seq << ' ' << r.From << ' ' << r.To << ' ' << r.Partial << ':';
    for (const DomainEvent& e : r.Events)
    {
        out << " [" << e.Seq << '@' << e.At << ' ';
        if (const auto* t = std::get_if<TimeAdvanced>(&e.Payload))
            out << "time " << t->From << "->" << t->To;
        else if (const auto* d = std::get_if<DayStarted>(&e.Payload))
            out << "day " << d->Day;
        else if (const auto* f = std::get_if<ScheduledEventFired>(&e.Payload))
            out << "fired " << f->Handle << ' ' << f->EventId;
        else if (const auto* w = std::get_if<WorldFlagChanged>(&e.Payload))
            out << "flag " << w->FlagId << '=' << w->Value;
        out << ']';
    }
    return out.str();
}
} // namespace

int main()
{
    // A refused command changes nothing at all and says why, with a stable code.
    {
        WorldSim sim(42);
        sim.Submit(Schedule(500, "test/kind/key"));
        sim.Submit(Flag("door.open", true));
        sim.Submit(Advance(30));
        const std::string before = Snapshot(sim);
        struct Case
        {
            Command command;
            const char* code;
        };
        const Case cases[] = {
            {Advance(0), "time.non_positive"},
            {Advance(-5), "time.non_positive"},
            {Advance(MaxSingleAdvanceMinutes + 1), "time.too_long"},
            {Schedule(600, "test/kind/key", CommandSource::Player), "schedule.source"},
            {Schedule(600, "not-three-parts"), "schedule.bad_id"},
            {Schedule(600, "Test/Kind/Key"), "schedule.bad_id"},
            {Schedule(600, "a//b"), "schedule.bad_id"},
            {Schedule(10, "test/kind/past"), "schedule.in_past"},
            {Flag("", true), "flag.bad_id"},
            {Flag("Door Open", true), "flag.bad_id"},
            {Flag("door.", true), "flag.bad_id"},
        };
        for (const Case& c : cases)
        {
            const CommandResult r = sim.Submit(c.command);
            Check(!r.Accepted && r.Why.Code == c.code, "each invalid command is refused with its code");
            Check(!r.Why.Message.empty() && r.Events.empty(), "a refusal explains itself and emits nothing");
            Check(Snapshot(sim) == before, "a refused command leaves the state identical");
        }
    }

    // Time: AdvanceTime is the only way the clock moves, and it reports
    // midnights and scheduled events in chronological order.
    {
        WorldSim sim(7);
        const GameInstant firstMidnight = GameClock::StartOfDay(0) + MinutesPerDay;
        Check(sim.Submit(Schedule(firstMidnight + 90, "test/late/b")).Accepted, "schedule after midnight");
        Check(sim.Submit(Schedule(45, "test/early/a")).Accepted, "schedule before midnight");
        Check(sim.Submit(Schedule(firstMidnight, "test/midnight/c")).Accepted, "schedule exactly at midnight");
        Check(sim.State().Clock.Now() == 0, "scheduling does not move the clock");

        const CommandResult r = sim.Submit(Advance(3 * MinutesPerDay));
        Check(r.Accepted && r.From == 0 && r.To == 3 * MinutesPerDay && !r.Partial, "the advance reaches its target");
        Check(std::holds_alternative<TimeAdvanced>(r.Events.front().Payload), "the time event comes first");
        std::vector<std::string> order;
        GameInstant last = -1;
        std::int64_t days = 0;
        for (const DomainEvent& e : r.Events)
        {
            Check(e.At >= last, "events are in chronological order");
            last = e.At;
            Check(e.Seq == r.Seq, "events carry the command's sequence number");
            if (const auto* f = std::get_if<ScheduledEventFired>(&e.Payload))
                order.push_back(f->EventId);
            if (const auto* d = std::get_if<DayStarted>(&e.Payload))
            {
                ++days;
                if (days == 1)
                    order.push_back("midnight");
            }
        }
        Check(days == 3, "three midnights crossed in three days");
        Check(order == std::vector<std::string>({"test/early/a", "midnight", "test/midnight/c", "test/late/b"}),
              "an event at midnight fires after the day starts, and everything else in time order");
        Check(sim.State().Clock.Pending().empty(), "every event fired exactly once");
        Check(sim.Submit(Advance(MinutesPerDay)).Events.size() == 2, "a later day fires nothing twice (time + one midnight)");
    }

    // An event scheduled at the current instant fires on the next advance,
    // before a midnight one minute later.
    {
        WorldSim sim(1);
        const GameInstant almostMidnight = GameClock::StartOfDay(0) + MinutesPerDay - 1;
        sim.Submit(Advance(almostMidnight));
        sim.Submit(Schedule(almostMidnight, "test/now/x"));
        const CommandResult r = sim.Submit(Advance(1));
        Check(r.Events.size() == 3, "time, the due event and midnight");
        Check(std::holds_alternative<ScheduledEventFired>(r.Events[1].Payload) && std::holds_alternative<DayStarted>(r.Events[2].Payload),
              "the event due now precedes the midnight after it");
    }

    // The event cap: a long advance stops, reports Partial, and loses nothing.
    {
        WorldSim sim(3, 5);
        for (GameInstant t = 1; t <= 12; ++t)
            sim.Submit(Schedule(t * 10, "test/many/" + std::to_string(t)));
        const CommandResult first = sim.Submit(Advance(1'000));
        Check(first.Accepted && first.Partial, "the cap marks the advance partial");
        Check(first.To == 50 && sim.State().Clock.Pending().size() == 7, "it stopped after five events, at the fifth");
        std::size_t fired = 5;
        while (!sim.State().Clock.Pending().empty())
        {
            const CommandResult more = sim.Submit(Advance(1'000 - sim.State().Clock.Now()));
            for (const DomainEvent& e : more.Events)
                fired += std::holds_alternative<ScheduledEventFired>(e.Payload) ? 1 : 0;
        }
        Check(fired == 12, "resubmitting fires every remaining event exactly once");
    }

    // Flags: a change emits an event; setting the same value is accepted and silent.
    {
        WorldSim sim(9);
        Check(sim.Submit(Flag("gate.canal.cordoned", true)).Events.size() == 1, "a new flag emits a change");
        Check(sim.Submit(Flag("gate.canal.cordoned", true)).Events.empty(), "the same value emits nothing");
        Check(sim.Submit(Flag("gate.canal.cordoned", false)).Events.size() == 1, "toggling emits a change");
        Check(sim.State().Flags.at("gate.canal.cordoned") == false, "the flag holds its last value");
        Check(sim.State().Seq == 3 && sim.Log().size() == 3, "every accepted command advances Seq and the log");
    }

    // Determinism: the same commands give the same results and the same state,
    // including thousands of random ones, valid and invalid.
    {
        auto run = [](std::uint64_t seed) {
            WorldSim sim(99, 50);
            std::mt19937_64 random(seed);
            std::ostringstream transcript;
            for (int i = 0; i < 3'000; ++i)
            {
                const std::uint64_t pick = random() % 10;
                Command c;
                if (pick < 4)
                    c = Advance(static_cast<std::int64_t>(random() % 3'000) - 100);
                else if (pick < 8)
                    c = Schedule(sim.State().Clock.Now() + static_cast<std::int64_t>(random() % 5'000) - 50,
                                 "sys/kind/" + std::to_string(random() % 100), random() % 5 == 0 ? CommandSource::Player : CommandSource::System);
                else
                    c = Flag("flag." + std::to_string(random() % 20), random() % 2 == 0);
                transcript << Render(sim.Submit(c)) << '\n';
            }
            return transcript.str() + Snapshot(sim);
        };
        const std::string a = run(2042);
        Check(a == run(2042), "the same input gives the same output");
        Check(a != run(2043), "different input gives different output");
    }

    std::cout << "PASS: " << Checks << " world reducer checks\n";
    return 0;
}
