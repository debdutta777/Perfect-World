// R9-04: pw.awakening/1 persistence - save round trips at every Awakening state (unseeded, seeded, mid-session, granted
// with the countdown open, after practice, after the deadline), uninterrupted and resumed runs reaching the same world,
// canonical-only and rule-checked decoding, the frozen v11 fixture (written by the unchanged generator built against
// e995956, schema 11, hash 8) migrating to empty Awakening slices, and a fresh-process restart reaching the same bytes.
// world_v11_doors_29_migrated_v12.json is the expected migrated result; PW_WRITE_MIGRATED=<path> writes it (only when the
// migration is meant to change).
#include "WorldSave.h"

#include "Doors.h"
#include "R94AwakeningTestSupport.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>

using namespace PerfectWorld;

namespace
{
const GameInstant R94SAppointment = R94At(2043, 1, 23, 9, 0);

std::string R94SRead(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    P2CExpect(static_cast<bool>(in), "a test file is readable");
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void R94SWrite(const std::string& path, const std::string& text)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
    P2CExpect(static_cast<bool>(out), "a test file is written");
}

std::string R94SReplace(std::string text, const std::string& from, const std::string& to)
{
    const std::size_t at = text.find(from);
    if (at == std::string::npos)
        std::cerr << "missing: " << from << '\n';
    P2CExpect(at != std::string::npos, "the test text contains the edited part");
    return text.replace(at, from.size(), to);
}

WorldSaveError R94SErrorOf(const std::string& text)
{
    WorldSaveData out;
    return DeserializeWorldSave(text, out).Error;
}

/** Saves, loads certified v12, and requires the Awakening slices and the whole world back exactly, and the same bytes again. */
WorldSaveData R94SRoundTrip(const R94World& w, const WorldState& world, const char* what)
{
    const std::string text = SerializeWorldSave(w.Save(world));
    P2CExpect(!text.empty() && text.find("\"schemaVersion\": 12,") != std::string::npos && text.find("\"hashVersion\": 9,") != std::string::npos &&
                  text.find("\"awakening\": \"pw.awakening/1\\n") != std::string::npos,
              what);
    WorldSaveData loaded;
    const WorldSaveResult result = DeserializeWorldSave(text, loaded);
    P2CExpect(result.Succeeded() && result.HashVerified && result.SourceVersion == 12 && !result.Migrated, what);
    P2CExpect(SerialiseAwakeningState(loaded.World) == SerialiseAwakeningState(world) && loaded.World.CountdownText == world.CountdownText &&
                  loaded.World.Tutorial == world.Tutorial && HashWorldState(loaded.World) == HashWorldState(world),
              what);
    P2CExpect(SerializeWorldSave(loaded) == text, what);
    return loaded;
}

/** The continuation every resumed run plays: finish any session, cross the evening, practise if possible, then a day. */
void R94SScript(const R94World& w, WorldSim& sim)
{
    (void)w;
    if (sim.State().Tutorial.Phase == TutorialDungeonPhase::Active)
        R94PlayTutorial(sim, 2);
    const GameInstant evening = CountdownStartInstant() + 30;
    if (sim.State().Clock.Now() < evening)
        S04Accept(sim, AdvanceTime{evening - sim.State().Clock.Now()}, "across the opening");
    const auto offer = sim.PreviewCountdownPractice(PowerDimension::Range, 45);
    if (offer.Available())
        S04Accept(sim, CountdownPracticeConfirm{offer}, "optional practice");
    S04Accept(sim, AdvanceTime{MinutesPerDay}, "a day of ordinary life");
}

struct R94SStates
{
    WorldState Unseeded, Seeded, Active, Granted, Practised, Closed;
};

R94SStates R94SBuild(const R94World& w)
{
    R94SStates s;
    s.Unseeded = MakeWorldState(0x9410, w.School.Creator);
    s.Seeded = w.Seeded(R94SAppointment, 0x9411);
    // The countdown opens with no teacher contact (so no standing obligation); practice later borrows a teacher.
    WorldSim sim = w.Bound(s.Seeded, false);
    S04Accept(sim, TutorialEnter{}, "entry");
    const auto& first = TutorialSituations()[0];
    S04Accept(sim, TutorialAct{std::string(first.StationId), std::string(first.Choices[2].Id)}, "one situation");
    s.Active = sim.State();
    R94PlayTutorial(sim, 0);
    S04Accept(sim, AdvanceTime{CountdownStartInstant() + 60 - sim.State().Clock.Now()}, "the evening");
    s.Granted = sim.State();
    WorldSim supervised = w.Bound(sim.State(), true);
    S04Accept(supervised, CountdownPracticeConfirm{supervised.PreviewCountdownPractice(PowerDimension::Output, 60)}, "practice");
    s.Practised = supervised.State();
    WorldSim free = w.Bound(supervised.State(), false);
    S04Accept(free, AdvanceTime{R94At(2043, 2, 2, 6, 0) + 120 - free.State().Clock.Now()}, "past the deadline");
    s.Closed = free.State();
    P2CExpect(!s.Granted.CountdownText.empty() && s.Granted.LiveAwakenedProfile && ParseCountdown(s.Closed.CountdownText) &&
                  ParseCountdown(s.Closed.CountdownText)->Closed,
              "the six test states are what they claim");
    return s;
}

void R94SRoundTrips(const R94World& w, const R94SStates& s)
{
    R94SRoundTrip(w, s.Unseeded, "an unseeded life round-trips");
    R94SRoundTrip(w, s.Seeded, "a seeded life before its appointment round-trips");
    R94SRoundTrip(w, s.Active, "a mid-session save round-trips at its paused instant");
    R94SRoundTrip(w, s.Granted, "a granted life with the countdown open round-trips");
    R94SRoundTrip(w, s.Practised, "a practised live profile round-trips");
    R94SRoundTrip(w, s.Closed, "a closed countdown round-trips");
    // The empty envelope, pinned literally apart from the two native ledgers: what every new and migrated world hashes.
    const std::string aptitude = SerialiseAptitude({});
    const std::string conduct = SerialiseConductLedger({});
    P2CExpect(SerialiseAwakeningState(s.Unseeded) ==
                  "pw.awakening/1\nseeded=0,0\nattributes=0,0,0,0,0,0\nskills=0\nevidence=0,0,0,0,0,0\nprojection=" +
                      std::to_string(aptitude.size()) + "\n" + aptitude + "applied=0\nconduct=" + std::to_string(conduct.size()) + "\n" +
                      conduct + "appointment=" + std::to_string(R94SAppointment) + "," + std::to_string(R94At(2043, 2, 2, 6, 0)) +
                      "\nmakeup=0,\ngrant=none\ntutorial=0,0,0,0,,,\ncountdown=none\nlive=none\nend\n",
              "the empty pw.awakening/1 envelope is exactly its documented form");
    // Uninterrupted and resumed runs reach the same world from every live state.
    for (const WorldState* state : {&s.Seeded, &s.Active, &s.Granted})
    {
        WorldSim straight = w.Bound(*state, true);
        R94SScript(w, straight);
        WorldSim resumed = w.Bound(R94SRoundTrip(w, *state, "resume").World, true);
        R94SScript(w, resumed);
        P2CExpect(HashWorldState(straight.State()) == HashWorldState(resumed.State()) &&
                      SerialiseAwakeningState(straight.State()) == SerialiseAwakeningState(resumed.State()),
                  "a reload changes nothing about what happens next");
    }
}

void R94STamper(const R94World& w, const R94SStates& s)
{
    const std::string granted = SerialiseAwakeningState(s.Granted);
    WorldState probe = s.Granted;
    P2CExpect(!granted.empty() && DeserialiseAwakeningState(granted, probe) && HashWorldState(probe) == HashWorldState(s.Granted),
              "the public decoder accepts the canonical text");
    const auto refused = [&](const std::string& text, const char* what) {
        WorldState target = s.Granted;
        const std::uint64_t before = HashWorldState(target);
        P2CExpect(!DeserialiseAwakeningState(text, target) && HashWorldState(target) == before, what);
    };
    const std::string seed = "seeded=1," + std::to_string(s.Granted.WorldSeed) + "\n";
    refused(R94SReplace(granted, seed, "seeded=1,0" + std::to_string(s.Granted.WorldSeed) + "\n"), "a non-canonical number is refused");
    refused(R94SReplace(granted, seed, "seeded=1," + std::to_string(s.Granted.WorldSeed + 1) + "\n"), "another world's seed is refused");
    refused(granted.substr(0, granted.find("live=")) + "live=none\nend\n", "a grant without its live profile is refused");
    refused(granted.substr(0, granted.find("countdown=")) + "countdown=none\n" + granted.substr(granted.find("live=")),
            "a countdown whose Gate history exists cannot be dropped");
    refused(R94SReplace(granted, "\nend\n", "\nend\nextra\n"), "trailing text is refused");
    refused(granted + "x", "a trailing byte is refused");
    for (std::size_t cut = 0; cut < granted.size(); cut += 7)
        refused(granted.substr(0, cut), "a truncated envelope is refused");

    WorldState noLive = s.Granted;
    noLive.LiveAwakenedProfile.reset();
    P2CExpect(SerialiseAwakeningState(noLive).empty() && SerializeWorldSave(w.Save(noLive)).empty(), "the writer refuses a broken state too");
    WorldState stray = s.Unseeded;
    stray.CountdownText = s.Granted.CountdownText;
    P2CExpect(!IsAwakeningStateValidInWorld(stray), "an unseeded life cannot hold a countdown");
    WorldState future = s.Granted;
    future.Awakening.AppliedActivityInstances.push_back(future.DayLoop.Runner.Instance + 1);
    P2CExpect(!IsAwakeningStateValidInWorld(future), "a consumed instance the runner never started is refused");
    WorldState rewritten = s.Granted;
    rewritten.Awakening.Granted->StartingProfile.Rating[0] += 1;
    P2CExpect(!IsAwakeningStateValidInWorld(rewritten), "a starting profile that is not its result's is refused");

    const std::string text = SerializeWorldSave(w.Save(s.Granted));
    const std::string bumped = R94SReplace(text, "\\nmakeup=0,\\n", "\\nmakeup=1,\\n");
    P2CExpect(R94SErrorOf(bumped) != WorldSaveError::None, "a plausible Awakening edit is refused");
    const std::size_t key = text.find(",\n    \"awakening\": ");
    P2CExpect(R94SErrorOf(text.substr(0, key) + text.substr(text.find("\n  }", key))) == WorldSaveError::Malformed,
              "a v12 world must have its awakening");
    P2CExpect(R94SErrorOf(R94SReplace(text, "\"awakening\": \"pw", "\"awakening\": 7, \"x\": \"pw")) != WorldSaveError::None,
              "the awakening member must be text");
}

void R94SLegacy(const R94World& w)
{
    const std::string dir = PW_AWAKENING_FIXTURE_DIR;
    const std::string text = R94SRead(dir + "/world_v11_doors_29.json");
    P2CExpect(text.size() == 5429 && text.find("\"schemaVersion\": 11,") != std::string::npos &&
                  text.find("\"hashVersion\": 8,") != std::string::npos && text.find("pw.awakening/1") == std::string::npos &&
                  text.find("\"stateHash\": \"0xece29e9ddd41d0a5\"") != std::string::npos,
              "the frozen v11 fixture is the untouched historical writer's output");
    WorldSaveData loaded;
    const WorldSaveResult result = DeserializeWorldSave(text, loaded);
    P2CExpect(result.Succeeded() && result.SourceVersion == 11 && !result.HashVerified && !result.Migrated,
              "frozen v11 authenticates its hash-8 checksum and loads honestly unverified");
    P2CExpect(!loaded.World.Awakening.Seeded && loaded.World.Tutorial == TutorialDungeonState{} && loaded.World.CountdownText.empty() &&
                  !loaded.World.LiveAwakenedProfile && IsAwakeningStateValidInWorld(loaded.World),
              "a v11 world migrates to empty Awakening slices");
    const std::string migrated = SerializeWorldSave(loaded);
    if (const char* path = std::getenv("PW_WRITE_MIGRATED"); path != nullptr && path[0] != '\0')
        R94SWrite(path, migrated);
    const std::string frozen = R94SRead(dir + "/world_v11_doors_29_migrated_v12.json");
    P2CExpect(migrated == frozen, "the migration produces exactly the frozen expected v12 save");
    WorldSaveData frozenLoaded;
    const WorldSaveResult frozenResult = DeserializeWorldSave(frozen, frozenLoaded);
    P2CExpect(frozenResult.Succeeded() && frozenResult.HashVerified && frozenResult.SourceVersion == 12 &&
                  HashWorldState(frozenLoaded.World) == HashWorldState(loaded.World),
              "the expected migrated save is a certified v12 world");
    for (std::size_t cut = 0; cut < text.size(); cut += 3)
        P2CExpect(R94SErrorOf(text.substr(0, cut)) != WorldSaveError::None, "a prefix of the v11 fixture is refused");
    P2CExpect(R94SErrorOf(R94SReplace(text, "home_front,1,-1", "home_front,1,1")) == WorldSaveError::HashMismatch &&
                  R94SErrorOf(R94SReplace(text, "\"stateHash\": \"0xece29e9ddd41d0a5\"", "\"stateHash\": \"0xece29e9ddd41d0a6\"")) ==
                      WorldSaveError::HashMismatch &&
                  R94SErrorOf(R94SReplace(text, "next_session=1", "next_session=2")) == WorldSaveError::HashMismatch,
              "the historical world, school included, is authenticated before migrating");
    P2CExpect(R94SErrorOf(R94SReplace(text, "\"school\": ", "\"awakening\": \"pw.awakening/1\\n\",\n    \"school\": ")) == WorldSaveError::Malformed,
              "a v11 record cannot smuggle in Awakening state");
    P2CExpect(R94SErrorOf(R94SReplace(text, "\"schemaVersion\": 11,", "\"schemaVersion\": 12,")) == WorldSaveError::Malformed,
              "a v11 record relabelled v12 lacks its awakening");
    WorldSaveData relabelled;
    const WorldSaveResult relabelledResult = DeserializeWorldSave(R94SReplace(text, "\"hashVersion\": 8,", "\"hashVersion\": 9,"), relabelled);
    P2CExpect(relabelledResult.Succeeded() && !relabelledResult.HashVerified, "relabelling the hash version never earns certification");
    WorldSim continued = w.Bound(loaded.World);
    S04Accept(continued, AdvanceTime{60}, "the migrated life continues");

    // A v11 save of a lived, awakened life: re-encoded as v11 (hash 8, no awakening), it loads with EMPTY slices.
    const WorldState lived = R94SBuild(w).Practised;
    std::string v11 = SerializeWorldSave(w.Save(lived));
    const std::size_t key = v11.find(",\n    \"awakening\": ");
    v11 = v11.substr(0, key) + v11.substr(v11.find("\n  }", key));
    v11 = R94SReplace(v11, "\"schemaVersion\": 12,", "\"schemaVersion\": 11,");
    v11 = R94SReplace(v11, "\"hashVersion\": 9,", "\"hashVersion\": 8,");
    const std::uint64_t old = HashWorldStateWithDoorsText(lived, SerialiseDoorState(lived.Doors), 8);
    v11 = R94SReplace(v11, "\"stateHash\": \"" + R94Hex(HashWorldState(lived)) + "\"", "\"stateHash\": \"" + R94Hex(old) + "\"");
    WorldSaveData migratedLife;
    const WorldSaveResult lifeResult = DeserializeWorldSave(v11, migratedLife);
    P2CExpect(lifeResult.Succeeded() && !lifeResult.HashVerified && !migratedLife.World.Awakening.Seeded &&
                  migratedLife.World.CountdownText.empty() && migratedLife.World.DayLoop == lived.DayLoop,
              "no past session, grant or countdown is invented for a migrated life");
    P2CExpect(DeserializeWorldSave(SerializeWorldSave(migratedLife), migratedLife).HashVerified, "its first v12 save is certified");
    WorldSim after = w.Bound(migratedLife.World);
    S04Accept(after, AdvanceTime{120}, "the migrated life continues past the evening without a countdown");
    P2CExpect(after.State().CountdownText.empty(), "a world already past 18:00 is never bootstrapped by the boundary");
}

void R94SHashSection(const R94World& w, const R94SStates& s)
{
    const auto a = HashWorldSections(s.Granted);
    WorldState changed = s.Granted;
    changed.LiveAwakenedProfile->SupervisedPracticeMinutes += 1;
    const auto b = HashWorldSections(changed);
    P2CExpect(a.size() == 12 && a.back().Name == "awakening" && WorldHashVersion == 9, "awakening is the twelfth section, hash v9");
    for (std::size_t i = 0; i < a.size(); ++i)
        P2CExpect((a[i].Hash != b[i].Hash) == (a[i].Name == "awakening"), "an Awakening change moves only its section");
    for (std::uint32_t version : {5u, 6u, 7u, 8u})
        P2CExpect(HashWorldStateWithDoorsText(s.Granted, SerialiseDoorState(s.Granted.Doors), version) ==
                      HashWorldStateWithDoorsText(changed, SerialiseDoorState(changed.Doors), version),
                  "historical hash versions never cover the awakening");
    (void)w;
}

void R94SFreshProcess(const R94World& w, const R94SStates& s, const char* self, const char* contentRoot)
{
    R94SWrite("r94_restart.save", SerializeWorldSave(w.Save(s.Active)));
    WorldSaveData here;
    P2CExpect(DeserializeWorldSave(R94SRead("r94_restart.save"), here).HashVerified, "the restart save loads here");
    WorldSim continued = w.Bound(here.World, true);
    R94SScript(w, continued);
    here.World = continued.State();
    const std::string expected = SerializeWorldSave(here);
    P2CExpect(!expected.empty(), "the in-process continuation saves");
    std::remove("r94_restart.out");
    std::string command = "\"\"" + std::string(self) + "\" \"" + contentRoot + "\" --continue r94_restart.save r94_restart.out\"";
#ifndef _WIN32
    command = command.substr(1, command.size() - 2);
#endif
    P2CExpect(std::system(command.c_str()) == 0, "the fresh process continues the save");
    P2CExpect(R94SRead("r94_restart.out") == expected, "a fresh process reaches exactly the same world and bytes");
}

int R94SChild(const R94World& w, const char* in, const char* out)
{
    WorldSaveData data;
    P2CExpect(DeserializeWorldSave(R94SRead(in), data).HashVerified, "the child loads the restart save");
    WorldSim sim = w.Bound(data.World, true);
    R94SScript(w, sim);
    data.World = sim.State();
    R94SWrite(out, SerializeWorldSave(data));
    return 0;
}
} // namespace

int main(int argc, char** argv)
{
    P2CExpect(argc == 2 || (argc == 5 && std::string(argv[2]) == "--continue"), "content root [--continue <in> <out>]");
    const P2CFixture fixture(argv[1]);
    const R94World world(fixture);
    if (argc == 5)
        return R94SChild(world, argv[3], argv[4]);
    const R94SStates states = R94SBuild(world);
    R94SHashSection(world, states);
    R94SRoundTrips(world, states);
    R94STamper(world, states);
    R94SLegacy(world);
    R94SFreshProcess(world, states, argv[0], argv[1]);
    std::cout << "R9-04 WorldSaveAwakening passed\n";
    return 0;
}
