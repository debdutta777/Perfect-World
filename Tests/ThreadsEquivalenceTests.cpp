// ROADMAP M3-06: advancing at event boundaries equals advancing in fine steps, replay stability, and random
// operation sequences. Part of PerfectWorldThreadTests.
#include "RandomStreams.h"
#include "ThreadsTestSupport.h"

#include <algorithm>
#include <set>

namespace ThrTest
{
namespace
{
/** Every event condition of every template: the type and the roles that must be in the entry. */
struct EventKind
{
    std::string Type;
    std::vector<std::string> Roles;
};

std::vector<EventKind> AllEventKinds()
{
    std::vector<EventKind> kinds;
    for (const ThreadTemplate& templ : TheWorld().Templates.Templates)
    {
        for (const ThreadTrigger& trigger : templ.Triggers)
        {
            for (const ThreadCondition& condition : trigger.Conditions)
            {
                if (condition.Kind == ThreadConditionKind::Event)
                    kinds.push_back({condition.EventType, condition.Parties});
            }
        }
    }
    return kinds;
}

std::vector<ThreadEvent> OfInstance(const std::vector<ThreadEvent>& events, const std::string& instance)
{
    std::vector<ThreadEvent> out;
    for (const ThreadEvent& event : events)
    {
        if (event.Instance == instance)
            out.push_back(event);
    }
    return out;
}

/** A world with several threads, a static relationship snapshot and a log of events dated after they opened. */
struct Scenario
{
    Rig Start;
    GameInstant End = 0;
    std::vector<GameInstant> EventTimes;
};

Scenario MakeScenario(std::uint64_t seed)
{
    RandomStream rng(seed);
    const std::vector<EventKind> kinds = AllEventKinds();
    const World& world = TheWorld();
    Scenario scenario;
    Rig& rig = scenario.Start;
    rig.Calendar = world.Calendar;
    const GameInstant t0 = Days(2 + static_cast<std::int64_t>(rng.NextBelow(80))) + static_cast<GameInstant>(rng.NextBelow(1440));
    // A static snapshot of how people feel, dated before anything opens.
    for (std::int64_t i = 0; i < 6; ++i)
    {
        const std::int64_t a = static_cast<std::int64_t>(rng.NextBelow(6));
        const std::int64_t b = (a + 1 + static_cast<std::int64_t>(rng.NextBelow(5))) % 6;
        rig.Act(t0 - 100 + i, a, b, static_cast<RelationshipActKind>(rng.NextBelow(RelationshipActKindCount - 1)));
    }
    std::vector<std::pair<const ThreadTemplate*, std::vector<std::string>>> threads;
    const std::size_t count = 1 + static_cast<std::size_t>(rng.NextBelow(6));
    for (std::size_t i = 0; i < count; ++i)
    {
        const ThreadTemplate& templ = world.Templates.Templates[rng.NextBelow(world.Templates.Templates.size())];
        const std::int64_t first = static_cast<std::int64_t>(rng.NextBelow(4));
        const ThreadOpenResult opened =
            ThreadOpen(rig.Set, world.Templates, templ.Id, Bind(templ, first), world.Registry, rig.MakeFacts(), t0);
        if (opened.Ok())
            threads.push_back({&templ, ThreadFind(rig.Set, opened.Instance)->People});
    }
    const std::int64_t length = 30 + static_cast<std::int64_t>(rng.NextBelow(190));
    scenario.End = t0 + Days(length);
    const std::size_t events = 10 + static_cast<std::size_t>(rng.NextBelow(60));
    for (std::size_t i = 0; i < events; ++i)
        scenario.EventTimes.push_back(t0 + static_cast<GameInstant>(rng.NextBelow(static_cast<std::uint64_t>(Days(length)))));
    std::sort(scenario.EventTimes.begin(), scenario.EventTimes.end());
    for (const GameInstant at : scenario.EventTimes)
    {
        if (threads.empty() || kinds.empty() || rng.NextBelow(10) == 0)
        {
            rig.Log1(at, "noise_event", {Person(0), Person(1)});
            continue;
        }
        const auto& chosen = threads[rng.NextBelow(threads.size())];
        const ThreadTemplate* templ = chosen.first;
        const std::vector<std::string>& people = chosen.second;
        std::vector<EventKind> mine;
        for (const EventKind& kind : kinds)
        {
            const bool fits = std::all_of(kind.Roles.begin(), kind.Roles.end(), [&](const std::string& role) { return ThreadRoleIndex(*templ, role) >= 0; });
            if (fits)
                mine.push_back(kind);
        }
        const EventKind& kind = mine[rng.NextBelow(mine.size())];
        std::vector<std::string> parties;
        for (const std::string& role : kind.Roles)
        {
            const std::int32_t index = ThreadRoleIndex(*templ, role);
            if (index >= 0)
                parties.push_back(people[static_cast<std::size_t>(index)]);
        }
        if (parties.empty())
            parties = {Person(0), Person(1)};
        rig.Log1(at, kind.Type, parties);
    }
    return scenario;
}

struct Outcome
{
    ThreadSet Set;
    std::vector<ThreadEvent> Events;
};

Outcome RunSteps(const Scenario& scenario, const std::vector<GameInstant>& stops)
{
    Rig rig = scenario.Start;
    Outcome outcome;
    for (const GameInstant stop : stops)
    {
        std::vector<ThreadEvent> events = rig.Advance(stop);
        outcome.Events.insert(outcome.Events.end(), events.begin(), events.end());
    }
    outcome.Set = rig.Set;
    return outcome;
}

void SameOutcome(const Outcome& a, const Outcome& b, const std::string& what)
{
    if (!(a.Set == b.Set))
        std::cerr << SerialiseThreads(a.Set) << "-----\n" << SerialiseThreads(b.Set);
    Expect(a.Set == b.Set && HashThreads(a.Set) == HashThreads(b.Set) && SerialiseThreads(a.Set) == SerialiseThreads(b.Set),
           what + ": the same threads in the same state");
    std::set<std::string> ids;
    for (const ThreadEvent& event : a.Events)
        ids.insert(event.Instance);
    for (const ThreadEvent& event : b.Events)
        ids.insert(event.Instance);
    for (const std::string& id : ids)
        Expect(OfInstance(a.Events, id) == OfInstance(b.Events, id), what + ": the same history for " + id);
}
} // namespace

void TestEquivalence()
{
    std::size_t resolved = 0;
    std::size_t escalations = 0;
    for (std::uint64_t seed = 1; seed <= 60; ++seed)
    {
        const Scenario scenario = MakeScenario(seed);
        const GameInstant now = scenario.Start.Set.Active.empty() ? 0 : scenario.Start.Set.Active.front().Now;
        const Outcome whole = RunSteps(scenario, {scenario.End});
        resolved += whole.Set.Resolved.size();
        escalations += CountEvents(whole.Events, ThreadEventKind::Escalated);

        // Fine steps: a minute at a time for the first ten hours, then irregular pieces.
        RandomStream rng(seed * 7919);
        std::vector<GameInstant> fine;
        GameInstant at = now;
        for (int i = 0; i < 600 && at < scenario.End; ++i)
            fine.push_back(++at);
        while (at < scenario.End)
        {
            at = std::min<GameInstant>(scenario.End, at + 1 + static_cast<GameInstant>(rng.NextBelow(6000)));
            fine.push_back(at);
        }
        SameOutcome(whole, RunSteps(scenario, fine), "seed " + std::to_string(seed) + " fine steps");

        // Event boundaries: stop at every logged instant and at every midnight, as a day loop would.
        std::vector<GameInstant> boundaries = scenario.EventTimes;
        for (GameInstant day = (now / MinutesPerDay + 1) * MinutesPerDay; day < scenario.End; day += MinutesPerDay)
            boundaries.push_back(day);
        boundaries.push_back(scenario.End);
        std::sort(boundaries.begin(), boundaries.end());
        boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());
        boundaries.erase(boundaries.begin(), std::lower_bound(boundaries.begin(), boundaries.end(), now));
        SameOutcome(whole, RunSteps(scenario, boundaries), "seed " + std::to_string(seed) + " event boundaries");

        // Replay: the same inputs always give the same evolution.
        SameOutcome(whole, RunSteps(MakeScenario(seed), {scenario.End}), "seed " + std::to_string(seed) + " replay");
    }
    Expect(resolved >= 40 && escalations >= 100, "the random scenarios are rich enough to mean something");
}

namespace
{
std::uint64_t RandomRun(std::uint64_t seed, bool checkEach)
{
    RandomStream rng(seed);
    const World& world = TheWorld();
    const std::vector<EventKind> kinds = AllEventKinds();
    Rig rig;
    rig.Calendar = world.Calendar;
    GameInstant now = Days(static_cast<std::int64_t>(rng.NextBelow(60)));
    GameInstant lastEntry = 0;
    std::uint64_t opened = 0;
    for (int step = 0; step < 90; ++step)
    {
        const std::uint64_t pick = rng.NextBelow(10);
        if (pick < 3)
        {
            const ThreadTemplate& templ = world.Templates.Templates[rng.NextBelow(world.Templates.Templates.size())];
            std::vector<ThreadRoleBinding> bindings = Bind(templ, static_cast<std::int64_t>(rng.NextBelow(5)));
            if (rng.NextBelow(8) == 0)
                bindings[0].Person = bindings[1].Person; // sometimes a refused open
            const ThreadOpenResult result = ThreadOpen(rig.Set, world.Templates, templ.Id, bindings, world.Registry, rig.MakeFacts(), now);
            opened += result.Ok() ? 1 : 0;
            Expect(result.Ok() || result.Status == ThreadStatus::Duplicate || result.Status == ThreadStatus::BadPeople ||
                       result.Status == ThreadStatus::Full,
                   "an open succeeds or is refused for a known reason");
        }
        else if (pick < 6)
        {
            now += 1 + static_cast<GameInstant>(rng.NextBelow(rng.NextBelow(4) == 0 ? Days(40) : Days(3)));
            Expect(rig.Advance(now).size() < 400, "an advance reports a sane number of events");
        }
        else if (pick < 9)
        {
            lastEntry = std::max(lastEntry, now);
            const EventKind& kind = kinds[rng.NextBelow(kinds.size())];
            std::vector<std::string> parties;
            for (std::size_t i = 0; i < kind.Roles.size(); ++i)
                parties.push_back(Person(static_cast<std::int64_t>(rng.NextBelow(5)) + static_cast<std::int64_t>(i) * 0));
            std::sort(parties.begin(), parties.end());
            parties.erase(std::unique(parties.begin(), parties.end()), parties.end());
            rig.Log1(lastEntry, kind.Type, parties);
        }
        else if (!rig.Set.Active.empty())
        {
            const ThreadInstance& victim = rig.Set.Active[rng.NextBelow(rig.Set.Active.size())];
            const ThreadTemplate& templ = *world.Templates.Find(victim.Template);
            const ThreadResolution& resolution = templ.Resolutions[rng.NextBelow(templ.Resolutions.size())];
            const std::string id = victim.Id;
            Expect(ThreadClose(rig.Set, world.Templates, id, resolution.Id, now).Ok(), "a live thread closes with any of its resolutions");
        }
        if (!checkEach)
            continue;
        const ThreadSet& set = rig.Set;
        Expect(set.Active.size() <= ThreadMaxActive && set.NextInstance == opened + 1 && set.Active.size() + set.Resolved.size() == opened,
               "every opened thread is live or resolved");
        std::uint64_t previous = 0;
        for (const ThreadInstance& live : set.Active)
        {
            const ThreadTemplate& templ = *world.Templates.Find(live.Template);
            const std::uint64_t number = std::stoull(live.Id.substr(4));
            Expect(number > previous, "live threads are in instance order");
            previous = number;
            Expect(live.Now == now && live.Cursor <= rig.Log.Size() && live.StageEnteredAt <= live.Now, "a live thread is evolved to the present");
            Expect(ThreadIntensity(templ, live) >= 1 && ThreadIntensity(templ, live) <= ThreadMaxIntensity, "intensity stays in range");
            Expect(live.Now - live.StageEnteredAt < Days(templ.Stages[static_cast<std::size_t>(live.Stage)].DwellDays), "no stage outlives its dwell");
        }
        Expect(ThreadSetMatchesTemplates(set, world.Templates) && ThreadSetMatchesLog(set, rig.Log), "the set is consistent");
        const std::optional<ThreadSet> loaded = ParseThreads(SerialiseThreads(set));
        Expect(loaded && *loaded == set && HashThreads(*loaded) == HashThreads(set), "the set survives a save at every step");
    }
    // Every template resolves: with nothing further to react to, the clock closes whatever is still open.
    const std::size_t stillOpen = rig.Set.Active.size();
    const std::vector<ThreadEvent> end = rig.Advance(now + Days(ThreadMaxClockDays + 1));
    Expect(rig.Set.Active.empty() && CountEvents(end, ThreadEventKind::Resolved) == stillOpen, "every thread resolves within the longest clock");
    return HashThreads(rig.Set);
}
} // namespace

void TestRandomOperations()
{
    for (std::uint64_t seed = 1; seed <= 60; ++seed)
    {
        const std::uint64_t first = RandomRun(seed, true);
        Expect(first == RandomRun(seed, false), "the same operations give the same final hash");
    }
}
} // namespace ThrTest
