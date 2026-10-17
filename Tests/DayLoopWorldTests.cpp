// The player's day inside the world (ROADMAP M1-03, M1-05, M2-12): the scripted school day with real content,
// carried by WorldState, covered by the world hash and saved in the v7 world save.
#include "DayLoopTestSupport.h"

#include "CharacterCreation.h"
#include "WorldHash.h"
#include "WorldReducer.h"
#include "WorldSave.h"

#include <set>

using namespace PerfectWorld;

namespace
{
CharacterCreationSave WorldDayCreation()
{
    CharacterCreator creator;
    Check(creator.ApplyPreset("balanced_scholar").Succeeded(), "preset applies");
    Check(creator.SetHouseholdTemplate("parent_household").Succeeded(), "household applies");
    Check(creator.SetIdentity({"Ananya", "Sen", "she/her", "Kolkata", "Bengali"}).Succeeded(), "identity applies");
    Check(creator.StartLife().Succeeded(), "life starts");
    return creator.CaptureSave();
}

/** The world a life begins in, with the rested morning player the day-loop tests use. */
WorldState MorningWorld()
{
    WorldState world = MakeWorldState(77);
    world.DayLoop = MorningState();
    Check(IsWorldDayInStep(world), "the morning world's day is in step with its clock");
    return world;
}

/**
 * One day command inside the world: the day loop applies it and the world clock follows to wherever it put its Now.
 * This is the routing a later step builds into the reducer; until then the caller keeps the clocks together, as here.
 */
void ApplyInWorld(WorldState& world, const DayCommand& command)
{
    const DayLoopResult result = MustApply(world.DayLoop, command, "a scripted command is accepted in the world");
    (void)result;
    if (world.DayLoop.Now > world.Clock.Now())
        Check(!world.Clock.AdvanceTo(world.DayLoop.Now).Rejected, "the world clock follows the day");
    Check(IsWorldDayInStep(world), "the day and the world clock agree after every command");
}

WorldSaveData SaveOfWorld(const WorldState& world)
{
    WorldSaveData data;
    Check(BeginWorldFromCreation(WorldDayCreation(), 77, data).Succeeded(), "a life begins");
    data.World = world;
    return data;
}

WorldSaveData LoadMustSucceed(const std::string& text, bool verified)
{
    WorldSaveData loaded;
    const WorldSaveResult result = DeserializeWorldSave(text, loaded);
    if (!result.Succeeded())
        std::cerr << result.Message << '\n';
    Check(result.Succeeded() && !result.Migrated && result.SourceVersion == WorldSaveSchemaVersion && result.HashVerified == verified,
          "the world save loads as the current schema");
    return loaded;
}

void CheckEveryPrefixRefused(const std::string& text, const char* what)
{
    for (std::size_t cut = 0; cut < text.size(); ++cut)
    {
        WorldSaveData out;
        out.Player.ZoneId = "zone.sentinel";
        if (DeserializeWorldSave(std::string_view(text).substr(0, cut), out).Succeeded() || out.Player.ZoneId != "zone.sentinel")
        {
            std::cerr << "prefix of " << cut << " of " << text.size() << " bytes was not refused\n";
            Check(false, what);
        }
    }
}

void TestScriptedDayInWorld()
{
    const std::vector<DayCommand> script = ScriptedDay();

    // The uninterrupted day, with the world after every command.
    std::vector<WorldState> boundary;
    WorldState world = MorningWorld();
    boundary.push_back(world);
    for (const DayCommand& command : script)
    {
        ApplyInWorld(world, command);
        boundary.push_back(world);
    }
    Check(boundary.size() == 30 && world.Clock.Now() == 1440 && world.DayLoop.Now == 1440 && world.DayLoop.Wakes == 1 && world.DayLoop.Seq == 29,
          "the scripted day ends at the next morning, with both clocks at 06:00");
    std::set<std::uint64_t> hashes;
    for (const WorldState& w : boundary)
        hashes.insert(HashWorldState(w));
    Check(hashes.size() == boundary.size(), "every command of the day changes the world hash");
    // Hash v5 appends conversations and doors after dayloop, which remains at index 5.
    Check(HashWorldSections(boundary[0])[5].Name == "dayloop" &&
              HashWorldSections(boundary[0])[5].Hash != HashWorldSections(boundary[29])[5].Hash &&
              HashWorldSections(boundary[0])[0].Hash == HashWorldSections(boundary[29])[0].Hash,
          "the day moves the dayloop section, and nothing else of the meta section");
    const std::uint64_t finalHash = HashWorldState(world);

    // Saving at every command boundary and resuming from the load reaches the same final hash.
    std::vector<std::string> saves;
    for (std::size_t i = 0; i < boundary.size(); ++i)
    {
        const std::string text = SerializeWorldSave(SaveOfWorld(boundary[i]));
        saves.push_back(text);
        const WorldSaveData loaded = LoadMustSucceed(text, true);
        Check(IsWorldDayInStep(loaded.World) && loaded.World.Clock.Now() == boundary[i].Clock.Now() &&
                  loaded.World.DayLoop.Now == boundary[i].DayLoop.Now,
              "the world clock and the day's clock agree after a load");
        Check(loaded.World.DayLoop == boundary[i].DayLoop && SerialiseDayLoop(loaded.World.DayLoop) == SerialiseDayLoop(boundary[i].DayLoop),
              "the day round-trips exactly");
        Check(HashWorldState(loaded.World) == HashWorldState(boundary[i]), "the loaded hash equals the saved hash");
        Check(SerializeWorldSave(SaveOfWorld(loaded.World)) == text, "saving what was loaded gives byte-identical text");

        WorldState resumed = loaded.World;
        for (std::size_t next = i; next < script.size(); ++next)
            ApplyInWorld(resumed, script[next]);
        Check(HashWorldState(resumed) == finalHash, "resuming from a save at any boundary reaches the same final hash");
    }

    // A perfect copy of the day's end, cut at every byte, is refused; so are the running activity's and the sleeper's.
    const std::size_t running = 3;  // after the first Start
    const std::size_t asleep = 28;  // after GoToSleep
    Check(boundary[running].DayLoop.Runner.Active() && boundary[asleep].DayLoop.Asleep && !boundary[29].DayLoop.Asleep,
          "the chosen boundaries are an activity running, the player asleep, and the next morning");
    for (const std::size_t at : {running, asleep, std::size_t{29}})
        CheckEveryPrefixRefused(saves[at], "every proper prefix of a v7 save with a day in it is refused");

    // A damaged byte is refused or harmless, across the whole file including the day.
    {
        const std::string& text = saves[29];
        std::size_t refused = 0;
        for (std::size_t at = 0; at < text.size(); ++at)
        {
            if (text[at] == '#')
                continue;
            std::string damaged = text;
            damaged[at] = '#';
            WorldSaveData out;
            if (DeserializeWorldSave(damaged, out).Succeeded())
                Check(HashWorldState(out.World) == finalHash, "a damaged byte never changes the world without being noticed");
            else
                ++refused;
        }
        Check(refused > text.size() / 2, "most damaged bytes are refused");
    }

    // The same day as an older v6 save would have it: no day loop. The player gets a fresh day at the world clock's instant.
    {
        std::string v6 = saves[29];
        const std::size_t key = v6.find(",\n    \"dayloop\": ");
        const std::size_t end = v6.find("\n  }", key);
        Check(key != std::string::npos && end != std::string::npos, "the day's save has a dayloop member");
        v6.erase(key, end - key); // the dayloop, conversations and doors members, none of which v6 had
        const std::string schema = "\"schemaVersion\": " + std::to_string(WorldSaveSchemaVersion);
        const std::string hash = "\"hashVersion\": " + std::to_string(WorldHashVersion);
        Check(v6.find(schema) != std::string::npos && v6.find(hash) != std::string::npos,
              "the save names the current schema and hash versions");
        v6.replace(v6.find(schema), schema.size(), "\"schemaVersion\": 6");
        v6.replace(v6.find(hash), hash.size(), "\"hashVersion\": 3");
        WorldSaveData loaded;
        const WorldSaveResult result = DeserializeWorldSave(v6, loaded);
        Check(result.Succeeded() && result.SourceVersion == 6 && !result.HashVerified && !result.Migrated, "the old save loads, unverified");
        Check(loaded.World.Clock.Now() == 1440 && loaded.World.DayLoop == NewDayLoopState(1440) && IsWorldDayInStep(loaded.World) &&
                  loaded.World.DayLoop.Receipts.Entries.empty() && loaded.World.DayLoop.Wakes == 0,
              "it has a fresh day at the clock's 06:00 on day two, and none of the day it never recorded");
    }
}

void TestWorldClockWithDay()
{
    const std::vector<DayCommand> script = ScriptedDay();

    // While an activity runs, or the player sleeps, a bare advance is refused and nothing changes.
    WorldState world = MorningWorld();
    for (std::size_t i = 0; i < script.size(); ++i)
    {
        ApplyInWorld(world, script[i]);
        const bool running = world.DayLoop.Runner.Active();
        const bool sleeping = world.DayLoop.Asleep;
        WorldSim sim(world);
        const std::uint64_t before = HashWorldState(sim.State());
        const CommandResult step = sim.Submit({CommandSource::Player, AdvanceTime{15}});
        if (running || sleeping)
        {
            Check(!step.Accepted && step.Why.Code == (running ? "time.activity_running" : "time.asleep") && HashWorldState(sim.State()) == before &&
                      sim.Log().empty(),
                  "time does not pass behind a running activity or a sleeping player");
        }
        else
        {
            Check(step.Accepted && sim.State().Clock.Now() == world.Clock.Now() + 15 && sim.State().DayLoop.Now == world.Clock.Now() + 15 &&
                      IsWorldDayInStep(sim.State()),
                  "otherwise an advance moves the world clock and the day together");
            const DayLoopState& day = sim.State().DayLoop;
            Check(day.Phase == DayLoopPhaseOf(day) && day.Seq == world.DayLoop.Seq && day.Condition == world.DayLoop.Condition &&
                      day.Receipts == world.DayLoop.Receipts && day.Runner == world.DayLoop.Runner,
                  "a bare advance moves only the day's clock and phase: condition, receipts and the sequence are the day loop's to change");
        }
    }

    // After the day, the next morning: unplanned, and the clocks are one.
    Check(world.DayLoop.Phase == DayPhase::Plan && !world.DayLoop.Planned, "the next morning needs a plan");
    WorldSim morning(world);
    Check(morning.Submit({CommandSource::Player, AdvanceTime{180}}).Accepted && morning.State().Clock.Now() == 1620 &&
              morning.State().DayLoop.Now == 1620 && morning.State().DayLoop.Phase == DayPhase::Plan,
          "time passing before the morning plan leaves the day in its plan phase");

    // A planned morning that waits past the evening line becomes the evening: the phase follows the shared clock.
    WorldState planned = MorningWorld();
    ApplyInWorld(planned, script[0]);
    WorldSim waiting(planned);
    Check(waiting.Submit({CommandSource::Player, AdvanceTime{13 * 60}}).Accepted && waiting.State().DayLoop.Phase == DayPhase::Evening &&
              IsWorldDayInStep(waiting.State()),
          "a planned day advanced to 19:00 is in the evening");
}
} // namespace

void TestDayLoopWorld()
{
    TestScriptedDayInWorld();
    TestWorldClockWithDay();
}
