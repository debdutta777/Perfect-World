#define _CRT_SECURE_NO_WARNINGS

#include "WorldHash.h"

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

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

#include "WorldHashReference.h"

namespace
{

// ---- Reducer output rendering, as in WorldReducerTests ----

Command Advance(std::int64_t minutes) { return {CommandSource::Player, AdvanceTime{minutes}}; }
Command Schedule(GameInstant when, std::string id, CommandSource source = CommandSource::System)
{
    return {source, ScheduleWorldEvent{when, std::move(id)}};
}
Command Flag(std::string id, bool value) { return {CommandSource::Player, SetWorldFlag{std::move(id), value}}; }

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

/** A mixed bag of valid and invalid commands, from a seed. `now` is the live clock so many schedules are valid. */
Command RandomCommand(std::mt19937_64& random, GameInstant now)
{
    const std::uint64_t pick = random() % 20;
    if (pick < 6)
        return Advance(static_cast<std::int64_t>(random() % 3'000) - 100);
    if (pick == 6)
        return Advance(static_cast<std::int64_t>(random() % 150'000) + 1);
    if (pick == 7 && random() % 8 == 0)
        return Advance(MaxSingleAdvanceMinutes + 1);
    if (pick < 13)
    {
        static const char* const ids[] = {"sys/kind/a", "sys/kind/b", "quest/start/x", "npc.walk/leg/1", "bad id", "Up/Case/Id", "a//b"};
        return Schedule(now + static_cast<std::int64_t>(random() % 5'000) - 50, ids[random() % 7],
                        random() % 5 == 0 ? CommandSource::Player : CommandSource::System);
    }
    static const char* const flagIds[] = {"flag.a", "flag.b", "door.open", "gate-1", "Bad Flag", "x.", ""};
    return Flag(flagIds[random() % 7], random() % 2 == 0);
}

std::size_t RandomCap(std::mt19937_64& random)
{
    static const std::size_t caps[] = {MaxEventsPerAdvance, 1, 5, 50};
    return caps[random() % 4];
}

// ---- Tests ----

void TestReferenceEncoder()
{
    Check(RefFnv1a("") == 0xcbf29ce484222325ULL, "FNV-1a 64 of the empty string");
    Check(RefFnv1a("a") == 0xaf63dc4c8601ec8cULL, "FNV-1a 64 of \"a\"");
    Check(RefFnv1a("foobar") == 0x85944171f73967e8ULL, "FNV-1a 64 of \"foobar\"");

    // Pin the byte layout itself on a tiny world, so the reference is not just a mirror of the code.
    RefBytes meta;
    meta.U(1);
    Check(meta.Data == std::string("u\x01\0\0\0\0\0\0\0", 9), "an unsigned field is a tag and 8 little-endian bytes");
    RefBytes text;
    text.S("ab");
    Check(text.Data == std::string("s\x02\0\0\0\0\0\0\0ab", 11), "a string is a tag, an 8-byte length and the bytes");
    RefBytes flag;
    flag.B(true);
    Check(flag.Data == std::string("b\x01", 2), "a bool is a tag and one byte");
}

void TestGolden()
{
    const WorldState hand = HandBuiltWorld();
    const WorldState edge = EdgeWorld();
    CheckAgainstReference(hand, "the hand-built world matches the reference encoder");
    CheckAgainstReference(edge, "the edge-case world matches the reference encoder");
    CheckAgainstReference(WorldState{}, "an empty world matches the reference encoder");
    if (const char* print = std::getenv("PW_PRINT_GOLDEN"); print != nullptr && print[0] == '1')
    {
        // The reference encoder's values, not the library's: the CheckAgainstReference calls above already
        // proved the two equal, so pinning the reference means the golden never rests on the code under test alone.
        std::cout << std::hex << "GoldenHandBuilt = 0x" << RefWorld(hand) << "ULL\nGoldenEdge = 0x" << RefWorld(edge) << "ULL\n";
        std::exit(0);
    }
    Check(HashWorldState(hand) == GoldenHandBuilt, "the hand-built world hashes to its pinned value");
    Check(HashWorldState(edge) == GoldenEdge, "the edge-case world hashes to its pinned value");
    const std::vector<WorldSectionHash> sections = HashWorldSections(hand);
    Check(sections.size() == 5 && sections[0].Name == "meta" && sections[1].Name == "clock" && sections[2].Name == "flags" &&
              sections[3].Name == "gates" && sections[4].Name == "rng",
          "sections come in the order meta, clock, flags, gates, rng");
    Check(WorldHashVersion == 3, "the hash version is 3 with the rng section");
    Check(HashWorldState(hand) == HashWorldState(HandBuiltWorld()), "hashing is repeatable");
    Check(HashWorldState(hand) != HashWorldState(WorldState{}), "different worlds differ");
}

void EditClock(WorldState& w, const std::function<void(GameClockSave&)>& edit)
{
    GameClockSave save = w.Clock.CaptureSave();
    edit(save);
    Check(w.Clock.RestoreSave(save), "an edited clock save is still consistent");
}

void TestSingleFieldEdits()
{
    struct Edit
    {
        const char* Name;
        const char* Section;
        std::function<void(WorldState&)> Apply;
    };
    const std::vector<Edit> edits = {
        {"seed +1", "meta", [](WorldState& w) { w.WorldSeed += 1; }},
        {"seq +1", "meta", [](WorldState& w) { w.Seq += 1; }},
        {"clock now", "clock", [](WorldState& w) { EditClock(w, [](GameClockSave& s) { s.Now += 1; }); }},
        {"clock next handle", "clock", [](WorldState& w) { EditClock(w, [](GameClockSave& s) { s.NextHandle += 1; }); }},
        {"event handle", "clock", [](WorldState& w) { EditClock(w, [](GameClockSave& s) { s.Pending[2].Handle = 1; }); }},
        {"event when", "clock", [](WorldState& w) { EditClock(w, [](GameClockSave& s) { s.Pending[2].When += 1; }); }},
        {"event id char", "clock", [](WorldState& w) { EditClock(w, [](GameClockSave& s) { s.Pending[2].Id = "sys/x/z"; }); }},
        {"event id length", "clock", [](WorldState& w) { EditClock(w, [](GameClockSave& s) { s.Pending[2].Id += "2"; }); }},
        {"event added", "clock", [](WorldState& w) { EditClock(w, [](GameClockSave& s) { s.Pending.push_back({1, 1400, "a/b/c"}); }); }},
        {"event removed", "clock", [](WorldState& w) { EditClock(w, [](GameClockSave& s) { s.Pending.pop_back(); }); }},
        {"events swap ids", "clock",
         [](WorldState& w) { EditClock(w, [](GameClockSave& s) { std::swap(s.Pending[0].Id, s.Pending[1].Id); }); }},
        {"flag value", "flags", [](WorldState& w) { w.Flags["door.open"] = false; }},
        {"other flag value", "flags", [](WorldState& w) { w.Flags["gate.cordoned"] = true; }},
        {"flag renamed", "flags",
         [](WorldState& w) {
             w.Flags.erase("door.open");
             w.Flags["door.shut"] = true;
         }},
        {"flag added", "flags", [](WorldState& w) { w.Flags["new.flag"] = false; }},
        {"flag removed", "flags", [](WorldState& w) { w.Flags.erase("gate.cordoned"); }},
        {"all flags removed", "flags", [](WorldState& w) { w.Flags.clear(); }},
        {"gate added", "gates", [](WorldState& w) { w.Gates["gate.new"] = GateSiteState{}; }},
        {"rng opportunity word 0", "rng", [](WorldState& w) { w.Rng[0].Words[0] += 1; }},
        {"rng opportunity word 3", "rng", [](WorldState& w) { w.Rng[0].Words[3] ^= 0x8000000000000000ULL; }},
        {"rng opportunity draws", "rng", [](WorldState& w) { w.Rng[0].Draws += 1; }},
        {"rng tower word 1", "rng", [](WorldState& w) { w.Rng[1].Words[1] += 1; }},
        {"rng ambience word 2", "rng", [](WorldState& w) { w.Rng[2].Words[2] += 1; }},
        {"rng crystallisation word 0", "rng", [](WorldState& w) { w.Rng[3].Words[0] += 1; }},
        {"rng crystallisation draws", "rng", [](WorldState& w) { w.Rng[3].Draws += 1; }},
        {"rng two streams swapped", "rng", [](WorldState& w) { std::swap(w.Rng[0], w.Rng[1]); }},
    };
    const WorldState base = HandBuiltWorld();
    const std::uint64_t baseWorld = HashWorldState(base);
    const std::vector<WorldSectionHash> baseSections = HashWorldSections(base);
    std::set<std::uint64_t> seen{baseWorld};
    for (const Edit& edit : edits)
    {
        WorldState edited = base;
        edit.Apply(edited);
        const std::vector<WorldSectionHash> sections = HashWorldSections(edited);
        Check(HashWorldState(edited) != baseWorld, edit.Name);
        Check(sections.size() == baseSections.size(), edit.Name);
        for (std::size_t i = 0; i < sections.size() && i < baseSections.size(); ++i)
        {
            const bool target = baseSections[i].Name == edit.Section;
            Check((sections[i].Hash != baseSections[i].Hash) == target, edit.Name);
        }
        CheckAgainstReference(edited, edit.Name);
        seen.insert(HashWorldState(edited));
    }
    Check(seen.size() == edits.size() + 1, "every distinct single-field edit gives a distinct world hash");

    // Every bit of the seed and sequence number matters.
    for (int bit = 0; bit < 64; ++bit)
    {
        WorldState a = base;
        a.WorldSeed ^= (1ULL << bit);
        WorldState b = base;
        b.Seq ^= (1ULL << bit);
        Check(HashWorldState(a) != baseWorld && HashWorldState(b) != baseWorld && HashWorldState(a) != HashWorldState(b),
              "flipping any single bit of the seed or Seq changes the hash");
    }

    // Field boundaries are unambiguous: moving a character between neighbouring items changes the hash.
    GameClockSave clock;
    clock.NextHandle = 3;
    clock.Pending = {{1, 5, "ab"}, {2, 5, "c"}};
    GameClockSave moved = clock;
    moved.Pending = {{1, 5, "a"}, {2, 5, "bc"}};
    Check(HashWorldState(MakeWorld(1, 2, clock, {})) != HashWorldState(MakeWorld(1, 2, moved, {})), "event IDs cannot bleed together");
    Check(HashWorldState(MakeWorld(1, 0, {}, {{"ab", true}, {"c", true}})) != HashWorldState(MakeWorld(1, 0, {}, {{"a", true}, {"bc", true}})),
          "flag IDs cannot bleed together");
    Check(HashWorldState(MakeWorld(1, 2, {}, {})) != HashWorldState(MakeWorld(2, 1, {}, {})), "seed and Seq are not interchangeable");
}

void TestFlagOrder()
{
    std::vector<std::pair<std::string, bool>> flags;
    for (int i = 0; i < 12; ++i)
        flags.push_back({"flag." + std::to_string(i), i % 3 == 0});
    GameClockSave clock;
    const std::uint64_t want = HashWorldState(MakeWorld(5, 5, clock, flags));
    std::mt19937_64 random(7);
    for (int round = 0; round < 50; ++round)
    {
        for (std::size_t i = flags.size() - 1; i > 0; --i)
            std::swap(flags[i], flags[random() % (i + 1)]);
        Check(HashWorldState(MakeWorld(5, 5, clock, flags)) == want, "flag insertion order does not change the hash");
    }
    // The same holds through the reducer: two orders of the same flag commands reach the same flags section.
    WorldSim forward(1);
    WorldSim backward(1);
    for (int i = 0; i < 8; ++i)
    {
        forward.Submit(Flag("flag." + std::to_string(i), true));
        backward.Submit(Flag("flag." + std::to_string(7 - i), true));
    }
    Check(HashWorldSections(forward.State())[2].Hash == HashWorldSections(backward.State())[2].Hash &&
              HashWorldState(forward.State()) == HashWorldState(backward.State()),
          "flag commands in a different order reach the same hash");
}

void TestRandomReplay()
{
    constexpr int Sequences = 10'000;
    std::set<std::uint64_t> finals;
    for (int index = 0; index < Sequences; ++index)
    {
        std::mt19937_64 random(1'000'003ULL * static_cast<std::uint64_t>(index + 1));
        const std::uint64_t worldSeed = random();
        const std::size_t cap = RandomCap(random);
        const int length = 10 + static_cast<int>(random() % 40);
        WorldSim a(worldSeed, cap);
        WorldSim b(worldSeed, cap);
        std::vector<std::string> acceptedRenders;
        std::vector<ReplayCheckpoint> expectedCheckpoints;
        bool anyRefused = false;
        for (int i = 0; i < length; ++i)
        {
            const Command command = RandomCommand(random, a.State().Clock.Now());
            const std::uint64_t before = HashWorldState(a.State());
            const CommandResult ra = a.Submit(command);
            const CommandResult rb = b.Submit(command);
            const std::string rendered = Render(ra);
            Check(rendered == Render(rb), "two worlds fed the same commands give identical results");
            const std::uint64_t after = HashWorldState(a.State());
            Check(after == HashWorldState(b.State()), "two worlds fed the same commands give identical hashes");
            Check((after != before) == ra.Accepted, "a command changes the hash if and only if it was accepted");
            if (ra.Accepted)
            {
                acceptedRenders.push_back(rendered);
                if (a.State().Seq % 5 == 0)
                    expectedCheckpoints.push_back({a.State().Seq, after});
            }
            else
            {
                anyRefused = true;
            }
        }
        const std::uint64_t finalHash = HashWorldState(a.State());
        finals.insert(finalHash);
        if (index % 100 == 0)
            CheckAgainstReference(a.State(), "a random world matches the reference encoder");

        ReplayOptions options;
        options.EventCap = cap;
        options.CheckpointEvery = 5;
        options.KeepResults = true;
        const ReplayResult replay = ReplayCommandLog(worldSeed, a.Log(), options);
        Check(replay.Complete && replay.Applied == a.Log().size(), "the accepted-command log replays completely");
        Check(replay.Hash == finalHash && replay.Hash == HashWorldState(b.State()), "the replay reaches the identical hash");
        Check(replay.Results.size() == acceptedRenders.size(), "replay keeps one result per command");
        for (std::size_t i = 0; i < replay.Results.size() && i < acceptedRenders.size(); ++i)
            Check(Render(replay.Results[i]) == acceptedRenders[i], "replay results equal the original results");
        Check(replay.Checkpoints.size() == expectedCheckpoints.size(), "replay records the expected checkpoints");
        for (std::size_t i = 0; i < replay.Checkpoints.size() && i < expectedCheckpoints.size(); ++i)
            Check(replay.Checkpoints[i].Seq == expectedCheckpoints[i].Seq && replay.Checkpoints[i].Hash == expectedCheckpoints[i].Hash,
                  "replay checkpoints match the live hashes");
        (void)anyRefused;
    }
    Check(finals.size() == static_cast<std::size_t>(Sequences), "10,000 different runs end in 10,000 different hashes");
}

void TestReplayCommandLog()
{
    // Empty log: complete, and the hash is that of a fresh world.
    {
        const ReplayResult empty = ReplayCommandLog(77, {});
        const WorldState fresh = MakeWorldState(77); // WorldSim seeds the RNG streams from the world seed
        Check(fresh.WorldSeed == 77 && fresh.Rng == RandomStreams(77).Capture(), "a fresh world seeds its streams from its seed");
        Check(empty.Complete && empty.Applied == 0 && empty.Hash == HashWorldState(fresh) && empty.Checkpoints.empty() &&
                  empty.Results.empty(),
              "an empty log replays to a fresh world");
    }

    // Refusal: replay stops at the first refused command and says which.
    {
        const std::vector<Command> log = {Flag("a.b", true), Advance(90), Advance(-5), Flag("never.applied", true)};
        ReplayOptions options;
        options.KeepResults = true;
        const ReplayResult replay = ReplayCommandLog(5, log, options);
        Check(!replay.Complete && replay.Applied == 2 && replay.Refusal.Code == "time.non_positive" && !replay.Refusal.Message.empty(),
              "replay stops at the first refused command and reports it");
        Check(replay.Results.size() == 3 && !replay.Results.back().Accepted && replay.Results[0].Accepted,
              "the refused command's result is kept last");
        WorldSim prefix(5);
        prefix.Submit(log[0]);
        prefix.Submit(log[1]);
        Check(replay.Hash == HashWorldState(prefix.State()), "the hash reported is that of the world before the refused command");
        Check(ReplayCommandLog(5, log).Results.empty(), "results are not kept unless asked for");
    }

    // Checkpoints: every Nth accepted command, and they agree with live hashes.
    {
        std::vector<Command> log;
        for (int i = 0; i < 10; ++i)
            log.push_back(i % 2 == 0 ? Flag("f." + std::to_string(i), true) : Advance(i * 100));
        ReplayOptions options;
        options.CheckpointEvery = 3;
        const ReplayResult replay = ReplayCommandLog(11, log, options);
        Check(replay.Complete && replay.Checkpoints.size() == 3, "ten commands, every third: three checkpoints");
        WorldSim live(11);
        std::size_t next = 0;
        for (const Command& c : log)
        {
            live.Submit(c);
            if (live.State().Seq % 3 == 0)
            {
                Check(next < replay.Checkpoints.size() && replay.Checkpoints[next].Seq == live.State().Seq &&
                          replay.Checkpoints[next].Hash == HashWorldState(live.State()),
                      "each checkpoint is the live hash at that Seq");
                ++next;
            }
        }
        Check(replay.Checkpoints[0].Hash != replay.Checkpoints[1].Hash && replay.Hash != replay.Checkpoints[2].Hash,
              "checkpoints advance with the world");
        Check(ReplayCommandLog(11, log).Checkpoints.empty(), "no checkpoints unless asked for");
    }

    // Event-cap mismatch: the log still replays, but to a different world, and the hash shows it.
    {
        WorldSim capped(3, 5);
        for (GameInstant t = 1; t <= 12; ++t)
            capped.Submit(Schedule(t * 10, "test/many/" + std::to_string(t)));
        const CommandResult partial = capped.Submit(Advance(1'000));
        Check(partial.Partial, "the capped advance was partial");
        const std::uint64_t liveHash = HashWorldState(capped.State());
        ReplayOptions same;
        same.EventCap = 5;
        Check(ReplayCommandLog(3, capped.Log(), same).Hash == liveHash, "replay with the matching cap reproduces the hash");
        const ReplayResult wrong = ReplayCommandLog(3, capped.Log());
        Check(wrong.Complete && wrong.Hash != liveHash, "replay with a different cap completes but its hash differs");
        Check(ReplayCommandLog(4, capped.Log(), same).Hash != liveHash, "replay with a different world seed differs");
    }
}
} // namespace

int main()
{
    TestReferenceEncoder();
    TestGolden();
    TestSingleFieldEdits();
    TestFlagOrder();
    TestReplayCommandLog();
    TestRandomReplay();
    std::cout << "PASS: " << Checks << " world hash checks\n";
    return 0;
}
