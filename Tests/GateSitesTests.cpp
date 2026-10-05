#include "GateSites.h"
#include "WorldHash.h"

#include <functional>
#include <cstdlib>
#include <iostream>
#include <set>
#include <sstream>
#include <type_traits>

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
Command Sys(CommandPayload payload, CommandSource source = CommandSource::System)
{
    return {source, std::move(payload)};
}
Command Reg(const std::string& id, GateRank rank, GateStatus status)
{
    return Sys(GateRegister{id, rank, status});
}
Command Onset(const std::string& id, GameInstant when)
{
    return Sys(GateScheduleOnset{id, when});
}
Command Advance(std::int64_t minutes)
{
    return {CommandSource::Player, AdvanceTime{minutes}};
}

std::string Snapshot(const WorldSim& sim)
{
    std::ostringstream out;
    const WorldState& s = sim.State();
    out << s.Seq << '|' << s.Clock.Now() << '|';
    for (const ScheduledEvent& e : s.Clock.Pending())
        out << e.Handle << '@' << e.When << ':' << e.Id << ',';
    out << '|';
    for (const auto& [id, g] : s.Gates)
        out << id << '=' << GateRankName(g.Rank) << GateStatusName(g.Status) << '@' << g.StatusSince << 'x' << g.OnsetCount << ',';
    return out.str() + '|' + std::to_string(sim.Log().size());
}

std::string RenderEvent(const DomainEvent& e)
{
    std::ostringstream out;
    out << " [" << e.Seq << '@' << e.At << ' ';
    if (const auto* t = std::get_if<TimeAdvanced>(&e.Payload))
        out << "time " << t->From << "->" << t->To;
    else if (const auto* d = std::get_if<DayStarted>(&e.Payload))
        out << "day " << d->Day;
    else if (const auto* f = std::get_if<ScheduledEventFired>(&e.Payload))
        out << "fired " << f->Handle << ' ' << f->EventId;
    else if (const auto* w = std::get_if<WorldFlagChanged>(&e.Payload))
        out << "flag " << w->FlagId << '=' << w->Value;
    else if (const auto* r = std::get_if<GateRegistered>(&e.Payload))
        out << "registered " << r->GateId << GateRankName(r->Rank) << GateStatusName(r->Status);
    else if (const auto* o = std::get_if<GateOnset>(&e.Payload))
        out << "onset " << o->GateId << ' ' << GateStatusName(o->Previous) << " #" << o->OnsetCount;
    else if (const auto* h = std::get_if<GateOnsetHeld>(&e.Payload))
        out << "held " << h->GateId;
    else if (const auto* c = std::get_if<GateContained>(&e.Payload))
        out << "contained " << c->GateId;
    else if (const auto* s = std::get_if<GateSealed>(&e.Payload))
        out << "sealed " << s->GateId;
    return out.str() + ']';
}

std::string Render(const CommandResult& r)
{
    std::ostringstream out;
    out << r.Accepted << ' ' << r.Why.Code << ' ' << r.Seq << ' ' << r.From << ' ' << r.To << ' ' << r.Partial << ':';
    for (const DomainEvent& e : r.Events)
        out << RenderEvent(e);
    return out.str();
}

int CountOnsets(const CommandResult& r, const std::string& id = "")
{
    int n = 0;
    for (const DomainEvent& e : r.Events)
    {
        const auto* o = std::get_if<GateOnset>(&e.Payload);
        n += (o != nullptr && (id.empty() || o->GateId == id)) ? 1 : 0;
    }
    return n;
}

/** Submits a command that must be refused with `code`, and checks nothing at all changed. */
void ExpectRefused(WorldSim& sim, const Command& command, const char* code, const char* message)
{
    const std::string before = Snapshot(sim);
    const CommandResult result = sim.Submit(command);
    Check(!result.Accepted && result.Why.Code == code && result.Events.empty(), message);
    Check(Snapshot(sim) == before, message);
}

const GateSiteState& Gate(const WorldSim& sim, const std::string& id)
{
    return sim.State().Gates.at(id);
}

struct Lcg
{
    std::uint64_t S;
    std::uint64_t Next(std::uint64_t n)
    {
        S = S * 6364136223846793005ULL + 1442695040888963407ULL;
        return (S >> 33) % n;
    }
};

struct Run
{
    std::string Transcript;
    std::string Accepted;
    std::string Final;
    std::vector<Command> Log;
    std::set<std::string> Codes;
    int Onsets = 0;
    int Held = 0;
};

Run RandomRun(std::uint64_t seed, int count)
{
    static const std::string bad[] = {"x.t.0", "gate.", "gate.T", "gate.a/b"};
    static const CommandSource sources[] = {CommandSource::System, CommandSource::System, CommandSource::System, CommandSource::Debug,
                                            CommandSource::Player, CommandSource::Presentation, CommandSource::Performance,
                                            CommandSource::Migration};
    WorldSim sim(seed);
    Lcg rng{seed};
    Run run;
    std::set<std::string> sealed;
    for (int i = 0; i < count; ++i)
    {
        const std::string id = rng.Next(12) == 0 ? bad[rng.Next(4)] : "gate.t." + std::to_string(rng.Next(24));
        const CommandSource source = sources[rng.Next(8)];
        const GameInstant now = sim.State().Clock.Now();
        Command command;
        switch (rng.Next(13))
        {
        case 0:
        case 1:
            command = {source, GateRegister{id, static_cast<GateRank>(rng.Next(8)), static_cast<GateStatus>(rng.Next(4))}};
            break;
        case 2:
        case 3:
        case 4:
            command = {source, GateScheduleOnset{id, now - 600 + static_cast<GameInstant>(rng.Next(3600))}};
            break;
        case 5:
        case 6:
            command = {source, GateContain{id}};
            break;
        case 7:
        case 8:
            command = {source, GateSeal{id}};
            break;
        default:
            command = Advance(1 + static_cast<std::int64_t>(rng.Next(600)));
            break;
        }
        const CommandResult result = sim.Submit(command);
        const std::string text = Render(result);
        run.Transcript += text + '\n';
        if (result.Accepted)
            run.Accepted += text + '\n';
        else
            run.Codes.insert(result.Why.Code);
        for (const DomainEvent& e : result.Events)
        {
            run.Onsets += std::holds_alternative<GateOnset>(e.Payload) ? 1 : 0;
            run.Held += std::holds_alternative<GateOnsetHeld>(e.Payload) ? 1 : 0;
        }
        for (const std::string& s : sealed)
            Check(Gate(sim, s).Status == GateStatus::Sealed, "a sealed Gate never changes status");
        for (const auto& [gid, g] : sim.State().Gates)
        {
            if (g.Status == GateStatus::Sealed)
                sealed.insert(gid);
        }
    }
    run.Final = Snapshot(sim);
    run.Log = sim.Log();
    return run;
}
} // namespace

int main()
{
    static_assert(std::is_const_v<std::remove_reference_t<decltype(std::declval<const WorldSim&>().State())>>, "State is read-only");
    static_assert(std::is_const_v<std::remove_reference_t<decltype(std::declval<WorldSim&>().State())>>, "State is read-only");

    // Names and parsing; data's "open" means active.
    Check(GateRankName(GateRank::F) == "F" && GateRankName(GateRank::S) == "S", "rank names");
    Check(ParseGateRank("D") == GateRank::D && !ParseGateRank("Z") && !ParseGateRank("d") && !ParseGateRank(""), "ranks parse strictly");
    Check(ParseGateStatus("open") == GateStatus::Active && ParseGateStatus("contained") == GateStatus::Contained
              && ParseGateStatus("sealed") == GateStatus::Sealed && ParseGateStatus("active") == GateStatus::Active && !ParseGateStatus("closed"),
          "statuses parse, open being active");
    Check(GateOnsetEventId("gate.x") == "gates/onset/gate.x" && IsGateEventId("gates/onset/gate.x") && !IsGateEventId("test/kind/key"),
          "onset event IDs");

    // Registration, and every refusal with state unchanged.
    {
        WorldSim sim(7);
        const CommandResult ok = sim.Submit(Reg("gate.t.river", GateRank::D, GateStatus::Contained));
        Check(ok.Accepted && ok.Events.size() == 1 && std::holds_alternative<GateRegistered>(ok.Events[0].Payload), "register emits GateRegistered");
        Check(Gate(sim, "gate.t.river").Rank == GateRank::D && Gate(sim, "gate.t.river").Status == GateStatus::Contained
                  && Gate(sim, "gate.t.river").StatusSince == 0 && Gate(sim, "gate.t.river").OnsetCount == 0,
              "a registered Gate holds its rank, status and instant");
        sim.Submit(Advance(100));
        Check(sim.Submit(Reg("gate.t.live", GateRank::A, GateStatus::Active)).Accepted && Gate(sim, "gate.t.live").StatusSince == 100,
              "a Gate may register already active, since now");

        ExpectRefused(sim, Reg("gate.t.river", GateRank::S, GateStatus::Sealed), "gate.duplicate", "duplicate registration");
        ExpectRefused(sim, Reg("river", GateRank::D, GateStatus::Contained), "gate.bad_id", "id without the gate. prefix");
        ExpectRefused(sim, Reg("gate.", GateRank::D, GateStatus::Contained), "gate.bad_id", "empty id after the prefix");
        ExpectRefused(sim, Reg("gate.t.Big", GateRank::D, GateStatus::Contained), "gate.bad_id", "uppercase id");
        ExpectRefused(sim, Reg("gate.t/x", GateRank::D, GateStatus::Contained), "gate.bad_id", "a slash would corrupt the onset event ID");
        ExpectRefused(sim, Reg("gate.t.", GateRank::D, GateStatus::Contained), "gate.bad_id", "id ending in a dot");
        ExpectRefused(sim, Reg("gate." + std::string(100, 'a'), GateRank::D, GateStatus::Contained), "gate.bad_id", "overlong id");
        ExpectRefused(sim, Reg("gate.t.new", static_cast<GateRank>(7), GateStatus::Contained), "gate.bad_rank", "rank out of range");
        ExpectRefused(sim, Reg("gate.t.new", GateRank::D, static_cast<GateStatus>(3)), "gate.bad_status", "status out of range");
        ExpectRefused(sim, Onset("gate.t.nowhere", 500), "gate.unknown", "onset for an unknown Gate");
        ExpectRefused(sim, Sys(GateContain{"gate.t.nowhere"}), "gate.unknown", "contain an unknown Gate");
        ExpectRefused(sim, Sys(GateSeal{"gate.t.nowhere"}), "gate.unknown", "seal an unknown Gate");
        ExpectRefused(sim, Onset("gate.t.river", 99), "gate.in_past", "onset in the past");
        ExpectRefused(sim, Sys(GateContain{"gate.t.river"}), "gate.not_active", "contain a contained Gate");
        ExpectRefused(sim, Sys(GateSeal{"gate.t.live"}), "gate.not_contained", "seal an active Gate");
        ExpectRefused(sim, Onset("bad", 500), "gate.bad_id", "bad id outranks unknown");
        for (const CommandSource source : {CommandSource::Player, CommandSource::Presentation, CommandSource::Performance, CommandSource::Migration})
        {
            ExpectRefused(sim, {source, GateRegister{"gate.t.p", GateRank::F, GateStatus::Contained}}, "gate.source", "register needs a system");
            ExpectRefused(sim, {source, GateScheduleOnset{"gate.t.river", 500}}, "gate.source", "scheduling needs a system");
            ExpectRefused(sim, {source, GateContain{"gate.t.live"}}, "gate.source", "containing needs a system");
            ExpectRefused(sim, {source, GateSeal{"gate.t.river"}}, "gate.source", "sealing needs a system");
        }
        Check(sim.Submit({CommandSource::Debug, GateRegister{"gate.t.dbg", GateRank::F, GateStatus::Contained}}).Accepted, "debug may register");
        Check(sim.State().Gates.size() == 3 && sim.Log().size() == 4, "refusals left no trace");
        Check(sim.Submit(Onset("gate.t.river", 100)).Accepted, "an onset may be scheduled for this very instant");
    }

    // An onset fires exactly once at its instant, across multi-day and event-capped advances.
    {
        WorldSim sim(9);
        sim.Submit(Reg("gate.t.river", GateRank::D, GateStatus::Contained));
        const GameInstant when = 3 * MinutesPerDay + 421;
        const CommandResult scheduled = sim.Submit(Onset("gate.t.river", when));
        Check(scheduled.Accepted && scheduled.Events.empty() && sim.State().Clock.Pending().size() == 1
                  && sim.State().Clock.Pending()[0].Id == "gates/onset/gate.t.river" && sim.State().Clock.Pending()[0].When == when,
              "scheduling puts the onset on the clock and changes no Gate");
        Check(Gate(sim, "gate.t.river").Status == GateStatus::Contained, "scheduled is not onset");

        const CommandResult early = sim.Submit(Advance(2 * MinutesPerDay));
        Check(CountOnsets(early) == 0 && Gate(sim, "gate.t.river").OnsetCount == 0, "nothing before the instant");
        const CommandResult hit = sim.Submit(Advance(5 * MinutesPerDay));
        Check(CountOnsets(hit) == 1, "the onset fires once in a multi-day advance");
        for (std::size_t i = 0; i < hit.Events.size(); ++i)
        {
            if (const auto* o = std::get_if<GateOnset>(&hit.Events[i].Payload))
            {
                const auto* fired = i > 0 ? std::get_if<ScheduledEventFired>(&hit.Events[i - 1].Payload) : nullptr;
                Check(fired != nullptr && fired->EventId == "gates/onset/gate.t.river", "GateOnset follows its ScheduledEventFired");
                Check(hit.Events[i].At == when && hit.Events[i - 1].At == when && o->Previous == GateStatus::Contained && o->OnsetCount == 1
                          && o->Rank == GateRank::D,
                      "the onset is stamped at its instant");
            }
        }
        Check(Gate(sim, "gate.t.river").Status == GateStatus::Active && Gate(sim, "gate.t.river").StatusSince == when
                  && Gate(sim, "gate.t.river").OnsetCount == 1 && sim.State().Clock.Pending().empty(),
              "the Gate is active since the onset");
        Check(CountOnsets(sim.Submit(Advance(10 * MinutesPerDay))) == 0 && Gate(sim, "gate.t.river").OnsetCount == 1, "it never fires again");
    }
    {
        WorldSim sim(9, 1); // one event per advance, so advances stop Partial.
        sim.Submit(Reg("gate.t.a", GateRank::C, GateStatus::Contained));
        sim.Submit(Reg("gate.t.b", GateRank::B, GateStatus::Contained));
        sim.Submit(Onset("gate.t.a", 100));
        sim.Submit(Onset("gate.t.b", 200));
        const CommandResult first = sim.Submit(Advance(1000));
        Check(first.Partial && first.To == 100 && CountOnsets(first, "gate.t.a") == 1 && CountOnsets(first) == 1, "a capped advance stops after the first onset");
        Check(Gate(sim, "gate.t.b").Status == GateStatus::Contained, "the second onset waits");
        const CommandResult second = sim.Submit(Advance(900));
        Check(second.Partial && second.To == 200 && CountOnsets(second, "gate.t.b") == 1 && CountOnsets(second) == 1, "the next advance fires the second");
        const CommandResult rest = sim.Submit(Advance(800));
        Check(!rest.Partial && CountOnsets(rest) == 0 && sim.State().Clock.Now() == 1000, "the rest of the time is quiet");
        Check(Gate(sim, "gate.t.a").OnsetCount == 1 && Gate(sim, "gate.t.b").OnsetCount == 1, "each onset counted once across partial advances");
    }

    // Deterministic order at one instant, and midnight comes first.
    {
        const GameInstant midnight = MinutesPerDay;
        std::string renders[2];
        for (std::string& render : renders)
        {
            WorldSim sim(3);
            sim.Submit(Reg("gate.t.b", GateRank::E, GateStatus::Contained));
            sim.Submit(Reg("gate.t.a", GateRank::F, GateStatus::Contained));
            sim.Submit(Onset("gate.t.b", midnight));
            sim.Submit(Onset("gate.t.a", midnight));
            sim.Submit(Sys(ScheduleWorldEvent{midnight, "test/kind/key"}));
            const CommandResult r = sim.Submit(Advance(MinutesPerDay + 5));
            render = Render(r);
            Check(r.Events.size() == 7, "time, day, three fired and two onsets");
            Check(std::holds_alternative<DayStarted>(r.Events[1].Payload), "DayStarted precedes the onsets at midnight");
            Check(std::get_if<ScheduledEventFired>(&r.Events[2].Payload)->EventId == "gates/onset/gate.t.b"
                      && std::get<GateOnset>(r.Events[3].Payload).GateId == "gate.t.b"
                      && std::get_if<ScheduledEventFired>(&r.Events[4].Payload)->EventId == "gates/onset/gate.t.a"
                      && std::get<GateOnset>(r.Events[5].Payload).GateId == "gate.t.a"
                      && std::get_if<ScheduledEventFired>(&r.Events[6].Payload)->EventId == "test/kind/key",
                  "events at one instant keep schedule order, each onset right after its own fired event; other systems' events are not routed");
        }
        Check(renders[0] == renders[1], "the same commands give the same events");
    }

    // Containment and sealing: transitions, wrong-status refusals, and the sealed hold.
    {
        WorldSim sim(5);
        sim.Submit(Reg("gate.t.c", GateRank::B, GateStatus::Contained));
        sim.Submit(Reg("gate.t.act", GateRank::A, GateStatus::Active));
        sim.Submit(Advance(50));
        const CommandResult contain = sim.Submit(Sys(GateContain{"gate.t.act"}));
        Check(contain.Accepted && contain.Events.size() == 1 && std::holds_alternative<GateContained>(contain.Events[0].Payload)
                  && Gate(sim, "gate.t.act").Status == GateStatus::Contained && Gate(sim, "gate.t.act").StatusSince == 50,
              "active to contained, stamped now");
        ExpectRefused(sim, Sys(GateContain{"gate.t.act"}), "gate.not_active", "contain twice");
        const CommandResult seal = sim.Submit(Sys(GateSeal{"gate.t.c"}));
        Check(seal.Accepted && seal.Events.size() == 1 && std::holds_alternative<GateSealed>(seal.Events[0].Payload)
                  && Gate(sim, "gate.t.c").Status == GateStatus::Sealed && Gate(sim, "gate.t.c").StatusSince == 50,
              "contained to sealed, stamped now");
        ExpectRefused(sim, Sys(GateSeal{"gate.t.c"}), "gate.not_contained", "seal twice");
        ExpectRefused(sim, Sys(GateContain{"gate.t.c"}), "gate.not_active", "a sealed Gate is never contained again");
        ExpectRefused(sim, Onset("gate.t.c", 900), "gate.sealed", "no onset for a sealed Gate");
        Check(Gate(sim, "gate.t.c").OnsetCount == 0, "refusals did not touch the Gate");

        // A contained Gate whose onset arrives breaks out: it becomes active.
        sim.Submit(Onset("gate.t.act", 80));
        const CommandResult breakout = sim.Submit(Advance(40));
        Check(CountOnsets(breakout) == 1 && std::get<GateOnset>(breakout.Events.back().Payload).Previous == GateStatus::Contained
                  && Gate(sim, "gate.t.act").Status == GateStatus::Active && Gate(sim, "gate.t.act").StatusSince == 80,
              "an onset activates a contained Gate");
        // A second onset at an active Gate counts but does not restart its status.
        sim.Submit(Onset("gate.t.act", 100));
        sim.Submit(Advance(20));
        Check(Gate(sim, "gate.t.act").OnsetCount == 2 && Gate(sim, "gate.t.act").StatusSince == 80 && Gate(sim, "gate.t.act").Status == GateStatus::Active,
              "a repeat onset counts without moving the status instant");

        // DESIGN QUESTION: onset scheduled, then sealed. The seal holds; a distinct event says so.
        sim.Submit(Reg("gate.t.d", GateRank::S, GateStatus::Contained));
        sim.Submit(Onset("gate.t.d", 200));
        sim.Submit(Sys(GateSeal{"gate.t.d"}));
        const CommandResult held = sim.Submit(Advance(200));
        Check(CountOnsets(held) == 0 && Gate(sim, "gate.t.d").Status == GateStatus::Sealed && Gate(sim, "gate.t.d").OnsetCount == 0
                  && Gate(sim, "gate.t.d").StatusSince == 110,
              "a sealed Gate is unchanged by an onset scheduled before the seal");
        const DomainEvent& last = held.Events.back();
        Check(std::holds_alternative<GateOnsetHeld>(last.Payload) && std::get<GateOnsetHeld>(last.Payload).GateId == "gate.t.d" && last.At == 200,
              "and GateOnsetHeld records that the seal held");
    }

    // Status moves only through commands and onsets; the clock alone and outsiders change nothing.
    {
        WorldSim sim(11);
        sim.Submit(Reg("gate.t.q", GateRank::F, GateStatus::Contained));
        const GateSiteState before = Gate(sim, "gate.t.q");
        sim.Submit(Advance(5 * MinutesPerDay));
        sim.Submit({CommandSource::Player, SetWorldFlag{"gate.t.q", true}});
        sim.Submit(Sys(ScheduleWorldEvent{sim.State().Clock.Now() + 10, "test/kind/key"}));
        const CommandResult quiet = sim.Submit(Advance(100));
        Check(quiet.Accepted && Gate(sim, "gate.t.q").Status == before.Status && Gate(sim, "gate.t.q").OnsetCount == 0 && CountOnsets(quiet) == 0,
              "time passing and other events leave Gates alone");
        // An onset event for a Gate nobody registered is dropped without events or state.
        sim.Submit(Sys(ScheduleWorldEvent{sim.State().Clock.Now() + 5, "gates/onset/gate.t.ghost"}, CommandSource::Debug));
        sim.Submit(Sys(ScheduleWorldEvent{sim.State().Clock.Now() + 6, "gates/other/gate.t.q"}, CommandSource::Debug));
        const CommandResult ghost = sim.Submit(Advance(50));
        Check(ghost.Accepted && CountOnsets(ghost) == 0 && sim.State().Gates.size() == 1 && Gate(sim, "gate.t.q").Status == GateStatus::Contained,
              "an unknown Gate's or unrecognised gates/ event changes nothing");
        const CommandResult viaDebug = [&] {
            sim.Submit(Sys(ScheduleWorldEvent{sim.State().Clock.Now() + 5, "gates/onset/gate.t.q"}, CommandSource::Debug));
            return sim.Submit(Advance(10));
        }();
        Check(CountOnsets(viaDebug) == 1, "a debug-scheduled onset for a known Gate routes like any other");
    }

    // Registering every Gate site of a city loaded from JSON text, in ID order.
    {
        const std::string city = R"({"type":"city","id":"city.t","name":"T","nation":"nation.t","state":"S","role":"R",
            "districts":[{"id":"district.t.a","name":"A","kind":"authored"}],
            "gateSites":[
              {"id":"gate.t.zeta","name":"Z","district":"district.t.a","rank":"S","status":"sealed","realmAnchor":false},
              {"id":"gate.t.alpha","name":"A","district":"district.t.a","rank":"D","status":"contained","realmAnchor":true},
              {"id":"gate.t.mid","name":"M","district":"district.t.a","rank":"F","status":"open","realmAnchor":false}]})";
        // The nation is deliberately absent: the registry still holds the city's records, and only gate_site records matter here.
        const ContentLoadResult loaded = LoadContent({{"/city/city.t.json", city}});
        Check(loaded.Db.CountOfType("gate_site") == 3, "three Gate sites loaded");
        const std::vector<Command> commands = GateRegistrationCommands(loaded.Db);
        Check(commands.size() == 3, "one command per Gate site");
        const char* order[] = {"gate.t.alpha", "gate.t.mid", "gate.t.zeta"};
        for (std::size_t i = 0; i < 3; ++i)
            Check(commands[i].Source == CommandSource::System && std::get<GateRegister>(commands[i].Payload).Id == order[i], "commands are in ID order");
        WorldSim sim(13);
        Check(RegisterContentGates(sim, loaded.Db) == 3 && sim.Log().size() == 3, "all three register");
        Check(Gate(sim, "gate.t.alpha").Rank == GateRank::D && Gate(sim, "gate.t.alpha").Status == GateStatus::Contained, "contained stays contained");
        Check(Gate(sim, "gate.t.mid").Rank == GateRank::F && Gate(sim, "gate.t.mid").Status == GateStatus::Active, "data's open is active");
        Check(Gate(sim, "gate.t.zeta").Rank == GateRank::S && Gate(sim, "gate.t.zeta").Status == GateStatus::Sealed, "sealed stays sealed");
        Check(RegisterContentGates(sim, loaded.Db) == 0 && sim.Log().size() == 3, "registering again is refused as duplicate and changes nothing");
        WorldSim empty(13);
        Check(RegisterContentGates(empty, ContentDb{}) == 0 && empty.State().Gates.empty(), "no content, no Gates");
    }

    // 2,000 random gate and time commands replay to an identical transcript.
    {
        const Run a = RandomRun(2026, 2000);
        const Run b = RandomRun(2026, 2000);
        Check(a.Transcript == b.Transcript && a.Final == b.Final, "the same seed gives an identical transcript");
        Check(RandomRun(2027, 2000).Transcript != a.Transcript, "a different seed differs");
        Check(a.Onsets > 20 && a.Held > 0, "the run exercised onsets and held seals");
        for (const char* code : {"gate.source", "gate.bad_id", "gate.bad_rank", "gate.bad_status", "gate.duplicate", "gate.unknown", "gate.sealed",
                                 "gate.in_past", "gate.not_active", "gate.not_contained"})
            Check(a.Codes.count(code) == 1, "the run met every rejection code");
        // Replaying the accepted log alone reproduces every accepted result and the final world.
        WorldSim replay(2026);
        std::string accepted;
        for (const Command& command : a.Log)
        {
            const CommandResult r = replay.Submit(command);
            Check(r.Accepted, "every logged command is accepted again");
            accepted += Render(r) + '\n';
        }
        Check(accepted == a.Accepted && Snapshot(replay) == a.Final, "the log replays to an identical transcript and world");
    }

    // The world hash covers the Gate slice: each Gate field moves the
    // "gates" section hash (index 3) and nothing outside meta and gates.
    {
        WorldState w;
        w.Gates["gate.t.h"] = GateSiteState{GateRank::C, GateStatus::Contained, 10, 0};
        const std::vector<WorldSectionHash> base = HashWorldSections(w);
        Check(base.size() == 4 && base[3].Name == "gates", "the hash has a gates section");
        auto changes = [&](const std::function<void(GateSiteState&)>& edit) {
            WorldState e = w;
            edit(e.Gates["gate.t.h"]);
            const std::vector<WorldSectionHash> s = HashWorldSections(e);
            return s[3].Hash != base[3].Hash && s[0].Hash == base[0].Hash && s[1].Hash == base[1].Hash && s[2].Hash == base[2].Hash;
        };
        Check(changes([](GateSiteState& g) { g.Rank = GateRank::B; }), "rank is hashed");
        Check(changes([](GateSiteState& g) { g.Status = GateStatus::Active; }), "status is hashed");
        Check(changes([](GateSiteState& g) { g.StatusSince = 11; }), "status time is hashed");
        Check(changes([](GateSiteState& g) { g.OnsetCount = 1; }), "onset count is hashed");
        WorldState renamed;
        renamed.Gates["gate.t.i"] = w.Gates["gate.t.h"];
        Check(HashWorldState(renamed) != HashWorldState(w), "the Gate ID is hashed");
    }

    std::cout << "PASS: " << Checks << " gate site checks\n";
    return 0;
}
