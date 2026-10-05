#pragma once

// Builders and comparison helpers for WorldSaveTests.cpp. Included by that file only; it relies on its Check().

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

WorldSaveData RichSave(std::uint64_t seed, int steps)
{
    std::mt19937_64 random(seed);
    WorldSim sim(seed);
    for (int i = 0; i < steps; ++i)
        (void)sim.Submit(RandomCommand(random, sim));
    // Make sure the interesting parts are present whatever the dice said.
    (void)sim.Submit(SystemCommand(ScheduleWorldEvent{sim.State().Clock.Now() + 700, "sys/kind/pinned"}));
    (void)sim.Submit({CommandSource::Player, SetWorldFlag{"pinned.flag", true}});
    (void)sim.Submit(SystemCommand(GateRegister{"gate.pinned", GateRank::C, GateStatus::Contained}));
    WorldSaveData data = SaveOf(sim, MakeCreation("grandparent_household"));
    DrawFromStreams(data.World, random);
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
    clock.Now = Min64;
    clock.NextHandle = Max64;
    clock.Pending = {{Max64 - 1, Max64, std::string("nul\0inside/\xff\x80/key", 17)}, {1, Min64, ""}};
    Check(w.Clock.RestoreSave(clock), "the edge clock is consistent");
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
