#pragma once

// Builders and comparison helpers shared by the world-save tests; relies on their Check().

#include "CharacterCreation.h"
#include "CreationSaveSerialization.h"
#include "GateSites.h"
#include "WorldHash.h"
#include "WorldSave.h"

#include <array>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <utility>

void Check(bool condition, const char* message);
/** WorldSaveDayTests.cpp: the day loop in the v7 save, and the v6 save without it. */
void TestWorldSaveDay();
void TestWorldSaveLane();

namespace
{
constexpr std::int64_t Max64 = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t Min64 = std::numeric_limits<std::int64_t>::min();

// ---- Builders ----

CharacterCreationSave MakeCreation(const char* household, const char* givenName = "Ananya \"Ana\"")
{
    CharacterCreator creator;
    Check(creator.ApplyPreset("balanced_scholar").Succeeded(), "preset applies");
    Check(creator.SetHouseholdTemplate(household).Succeeded(), "household applies");
    Check(creator.SetIdentity({givenName, "Sen", "she/her", "Kolkata", "Bengali"}).Succeeded(), "identity applies");
    AppearanceRecord look = AppearanceRecord::Default();
    look.FaceShapeId = "face_heart";
    look.HairStyleId = "hair_long_braid";
    look.BuildId = "build_sturdy";
    look.UniformVariantId = "uniform_casual";
    look.SkinTone = {123, 84, 61};
    look.HairColor = {20, 16, 14};
    look.EyeColor = {90, 120, 60};
    Check(creator.SetAppearance(look).Succeeded(), "appearance applies");
    Check(creator.StartLife().Succeeded(), "life starts");
    return creator.CaptureSave();
}

/** The restored creator of a record: a world for that life begins with its household's commerce (R8-04). */
CharacterCreator CreatorOf(const CharacterCreationSave& creation)
{
    CharacterCreator creator;
    Check(creator.RestoreSave(creation).Succeeded(), "the creation record restores");
    return creator;
}

/** Seed-and-creation replay: the same starting snapshot as WorldSim(seed, CreatorOf(creation)), household funding included. */
ReplayResult ReplayLife(std::uint64_t seed, const CharacterCreationSave& creation, const std::vector<Command>& log)
{
    return ReplayCommandLogFromSnapshot(MakeWorldState(seed, CreatorOf(creation)), log);
}

/** What a pre-v10 save of `data` loads as: everything kept, commerce initialised once at the saved instant (R8-04). */
WorldSaveData MigratedCommerce(WorldSaveData data)
{
    data.World.Commerce = InitialiseCommerce(CreatorOf(data.Creation).GetTraitRank(HouseholdResourcesTraitId), data.World.Clock.Now()).State;
    return data;
}

Command SystemCommand(CommandPayload payload) { return {CommandSource::System, std::move(payload)}; }

Command RandomCommand(std::mt19937_64& random, const WorldSim& sim)
{
    const GameInstant now = sim.State().Clock.Now();
    static const char* const gates[] = {"gate.a.one", "gate.a.two", "gate.b", "gate.bad id", "nogate"};
    static const char* const events[] = {"sys/kind/a", "sys/kind/b", "quest/start/x", "bad id"};
    static const char* const flags[] = {"flag.a", "flag.b", "door.open", "Bad Flag"};
    const std::string gate = gates[random() % 5];
    switch (random() % 9)
    {
    case 0:
    case 1: return {CommandSource::Player, AdvanceTime{static_cast<std::int64_t>(random() % 3000) + 1}};
    case 2: return {CommandSource::Player, AdvanceTime{static_cast<std::int64_t>(random() % 90000) + 1}};
    case 3:
    case 4: return SystemCommand(ScheduleWorldEvent{now + static_cast<std::int64_t>(random() % 5000), events[random() % 4]});
    case 5: return {CommandSource::Player, SetWorldFlag{flags[random() % 4], random() % 2 == 0}};
    case 6: return SystemCommand(GateRegister{gate, static_cast<GateRank>(random() % 7), static_cast<GateStatus>(random() % 3)});
    case 7: return SystemCommand(GateScheduleOnset{gate, now + static_cast<std::int64_t>(random() % 4000)});
    default: return random() % 2 == 0 ? SystemCommand(GateContain{gate}) : SystemCommand(GateSeal{gate});
    }
}

WorldSaveData SaveOf(const WorldSim& sim, const CharacterCreationSave& creation)
{
    WorldSaveData data;
    data.Creation = creation;
    data.World = sim.State();
    data.Player.ZoneId = "zone.street.nabapara_lane";
    data.Player.PositionCm = {120, -3400, 20};
    data.Player.YawCdeg = 9000;
    return data;
}

/** Draws from the streams a few times and writes the states back into the world, as an RNG-using system would. */
void DrawFromStreams(WorldState& world, std::mt19937_64& random)
{
    RandomStreams streams;
    Check(streams.Restore(world.Rng), "the world's stream states are valid");
    for (std::size_t i = 0; i < RandomStreamCount; ++i)
    {
        for (std::uint64_t n = random() % 20; n > 0; --n)
            (void)streams.Stream(static_cast<RandomStreamId>(i)).NextBelow(1 + random() % 1000);
    }
    world.Rng = streams.Capture();
}

/** A day with something in it that costs no time: planned, tired, a plan, three commands and a wake counted. The clock is untouched. */
void BusyDay(WorldState& world)
{
    DayLoopState& d = world.DayLoop;
    d.Condition.Energy -= 1234;
    d.Condition.Hunger += 321;
    d.Condition.Stress += 17;
    d.Planned = true;
    d.Plan = {{360, "activity.eat_meal"}, {620, "activity.attend_class"}};
    d.Seq = 3;
    d.Wakes = 1;
    d.HasLastAction = true;
    d.Last = {DayCommandKind::PlanDay, d.Now, ""};
    d.Phase = DayLoopPhaseOf(d);
    Check(IsWorldDayInStep(world), "a busy day is valid and in step with the clock");
}

WorldSaveData RichSave(std::uint64_t seed, int steps)
{
    std::mt19937_64 random(seed);
    const CharacterCreationSave creation = MakeCreation("grandparent_household");
    WorldSim sim(seed, CreatorOf(creation));
    for (int i = 0; i < steps; ++i)
        (void)sim.Submit(RandomCommand(random, sim));
    // Make sure the interesting parts are present whatever the dice said.
    (void)sim.Submit(SystemCommand(ScheduleWorldEvent{sim.State().Clock.Now() + 700, "sys/kind/pinned"}));
    (void)sim.Submit({CommandSource::Player, SetWorldFlag{"pinned.flag", true}});
    (void)sim.Submit(SystemCommand(GateRegister{"gate.pinned", GateRank::C, GateStatus::Contained}));
    WorldSaveData data = SaveOf(sim, creation);
    DrawFromStreams(data.World, random);
    BusyDay(data.World);
    data.Player.Camera = CameraMode::ThirdPerson;
    data.Player.PitchCdeg = -1234;
    return data;
}

WorldSaveData EdgeSave()
{
    WorldSaveData data;
    data.Creation = MakeCreation("appointed_guardian");
    WorldState& w = data.World;
    w.WorldSeed = std::numeric_limits<std::uint64_t>::max();
    w.Seq = std::numeric_limits<std::uint64_t>::max() - 1;
    GameClockSave clock;
    // The clock sits at the last instant a day can use (a save with a clock outside that range is refused);
    // the extremes live in the scheduled events and the other 64-bit fields.
    clock.Now = DayLoopMaxInstant;
    clock.NextHandle = Max64;
    clock.Pending = {{Max64 - 1, Max64, std::string("nul\0inside/\xff\x80/key", 17)}, {1, DayLoopMaxInstant, ""}};
    Check(w.Clock.RestoreSave(clock), "the edge clock is consistent");
    w.DayLoop = NewDayLoopState(DayLoopMaxInstant);
    w.Commerce = InitialiseCommerce(CreatorOf(data.Creation).GetTraitRank(HouseholdResourcesTraitId), DayLoopMaxInstant).State;
    BusyDay(w);
    w.Flags = {{"", false}, {std::string("a\0b", 3), true}, {"\xc3\xa9.unicode", true}, {"quote\"back\\slash", false}};
    w.Gates["gate.edge"] = GateSiteState{GateRank::S, GateStatus::Sealed, Min64, std::numeric_limits<std::int32_t>::max()};
    constexpr std::uint64_t Top = std::numeric_limits<std::uint64_t>::max();
    for (std::size_t i = 0; i < RandomStreamCount; ++i)
    {
        w.Rng[i].Words = {Top - i, 0x8000000000000000ULL, 1, 0x0123456789abcdefULL};
        w.Rng[i].Draws = Top - 3 * i;
    }
    data.Player.ZoneId = std::string(WorldSaveMaxZoneIdLength, 'z');
    data.Player.PositionCm = {WorldSavePositionLimitCm, -WorldSavePositionLimitCm, 0};
    data.Player.YawCdeg = WorldSaveFullTurnCdeg - 1;
    data.Player.PitchCdeg = -WorldSaveMaxPitchCdeg;
    data.Player.Camera = CameraMode::ThirdPerson;
    return data;
}

// ---- Comparison ----

bool SameClock(const GameClock& a, const GameClock& b)
{
    const GameClockSave x = a.CaptureSave();
    const GameClockSave y = b.CaptureSave();
    if (x.Now != y.Now || x.NextHandle != y.NextHandle || x.Pending.size() != y.Pending.size())
        return false;
    for (std::size_t i = 0; i < x.Pending.size(); ++i)
    {
        if (x.Pending[i].Handle != y.Pending[i].Handle || x.Pending[i].When != y.Pending[i].When || x.Pending[i].Id != y.Pending[i].Id)
            return false;
    }
    return true;
}

bool SameGates(const WorldState& a, const WorldState& b)
{
    if (a.Gates.size() != b.Gates.size())
        return false;
    for (const auto& [id, gate] : a.Gates)
    {
        const auto found = b.Gates.find(id);
        if (found == b.Gates.end() || found->second.Rank != gate.Rank || found->second.Status != gate.Status ||
            found->second.StatusSince != gate.StatusSince || found->second.OnsetCount != gate.OnsetCount)
            return false;
    }
    return true;
}

/** Field by field, independently of the hash. */
void CheckSame(const WorldSaveData& a, const WorldSaveData& b, const char* what)
{
    Check(a.World.WorldSeed == b.World.WorldSeed && a.World.Seq == b.World.Seq, what);
    Check(SameClock(a.World.Clock, b.World.Clock), what);
    Check(a.World.Flags == b.World.Flags, what);
    Check(SameGates(a.World, b.World), what);
    Check(a.World.Rng == b.World.Rng, what);
    Check(a.World.Conversations == b.World.Conversations && a.World.Doors == b.World.Doors, what);
    Check(a.World.Commerce == b.World.Commerce && a.World.Journal.Encounters == b.World.Journal.Encounters, what);
    Check(a.World.DayLoop == b.World.DayLoop && SerialiseDayLoop(a.World.DayLoop) == SerialiseDayLoop(b.World.DayLoop), what);
    Check(IsWorldDayInStep(a.World) == IsWorldDayInStep(b.World) && a.World.DayLoop.Now == a.World.Clock.Now(), what);
    Check(a.Player == b.Player, what);
    Check(SerializeCreationSave(a.Creation) == SerializeCreationSave(b.Creation), what);
    Check(HashWorldState(a.World) == HashWorldState(b.World), what);
}

WorldSaveData Sentinel()
{
    WorldSaveData data;
    data.World = MakeWorldState(424242);
    data.Player.ZoneId = "zone.sentinel";
    return data;
}

bool Untouched(const WorldSaveData& data)
{
    return data.World.WorldSeed == 424242 && data.Player.ZoneId == "zone.sentinel" && data.World.Flags.empty();
}

std::string ReplaceOnce(std::string text, const std::string& from, const std::string& to, std::size_t after = 0)
{
    const std::size_t at = text.find(from, after);
    Check(at != std::string::npos, ("test text contains: " + from).c_str());
    text.replace(at, from.size(), to);
    return text;
}

WorldSaveError ErrorOf(const std::string& text)
{
    WorldSaveData out = Sentinel();
    const WorldSaveResult result = DeserializeWorldSave(text, out);
    Check(result.Succeeded() == (result.Error == WorldSaveError::None), "success and error agree");
    Check(result.Succeeded() || (Untouched(out) && !result.Message.empty()), "a refusal leaves the target untouched and says why");
    return result.Error;
}

} // namespace
