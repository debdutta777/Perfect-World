#pragma once

// R9-04: a WorldSim bound to the real school content, the real ability recipes and a trusted countdown context, plus
// helpers that build a seeded life at a chosen instant and drive it only through commands.

#include "S04SchoolTestSupport.h"
#include "WorldCountdown.h"

#include <string>
#include <utility>

struct R94World
{
    const P2CFixture& F;
    S04World School;
    PerfectWorld::AbilityRecipeSet Recipes;
    PerfectWorld::AbilityRecipeSet NoRecipes;

    explicit R94World(const P2CFixture& f) : F(f), School(f)
    {
        auto recipes = PerfectWorld::LoadAbilityRecipes(f.Content);
        P2CExpect(recipes.Ok(), "the real ability recipes load");
        Recipes = std::move(recipes.Set);
    }
    R94World(const R94World&) = delete;
    R94World& operator=(const R94World&) = delete;

    /** The player's countdown context. A teacher contact supervises practice and also owes the hall setup obligation. */
    [[nodiscard]] static PerfectWorld::CountdownContext Context(bool teacher)
    {
        PerfectWorld::CountdownContext context;
        context.Player = "player";
        if (teacher)
            context.Contacts.push_back({"teacher", "npc_teacher"});
        return context;
    }
    [[nodiscard]] PerfectWorld::WorldAwakeningCatalogues Bind(bool teacher = false, bool recipes = true) const
    {
        return {recipes ? &Recipes : &NoRecipes, Context(teacher)};
    }
    [[nodiscard]] PerfectWorld::WorldSim Bound(PerfectWorld::WorldState state, bool teacher = false,
                                               std::size_t cap = PerfectWorld::MaxEventsPerAdvance, bool recipes = true) const
    {
        return PerfectWorld::WorldSim(std::move(state), cap, School.Cats.Day, {}, School.School(), Bind(teacher, recipes));
    }
    /** The same world in a sim with no Awakening binding (what nested helpers and old callers see). */
    [[nodiscard]] PerfectWorld::WorldSim Unbound(PerfectWorld::WorldState state) const { return School.Continue(std::move(state)); }
    /** A life confirmed at creation (SeedAwakeningInputs over its creation record), standing at `at`. */
    [[nodiscard]] PerfectWorld::WorldState Seeded(PerfectWorld::GameInstant at, std::uint64_t seed) const
    {
        using namespace PerfectWorld;
        WorldSim sim = School.Sim(seed);
        if (at > sim.State().Clock.Now())
            S04Accept(sim, AdvanceTime{at - sim.State().Clock.Now()}, "the life reaches its instant", CommandSource::System);
        WorldState world = sim.State();
        world.DayLoop.Condition.Energy = MeterMax;
        const auto seeded = SeedAwakeningInputs(F.Content, School.Creator.CaptureSave(), world.WorldSeed);
        P2CExpect(seeded.Ok() && seeded.State.Seeded, "creation confirmation seeds the Awakening inputs");
        world.Awakening = seeded.State;
        return world;
    }
    [[nodiscard]] PerfectWorld::WorldSaveData Save(const PerfectWorld::WorldState& world) const { return School.Save(world); }
};

inline PerfectWorld::GameInstant R94At(int year, int month, int day, int hour, int minute)
{
    return PerfectWorld::GameClock::FromCalendar(year, month, day, hour, minute);
}

/** Plays the remaining situations with the given choice index each; the last act exits and grants in the same command. */
inline PerfectWorld::CommandResult R94PlayTutorial(PerfectWorld::WorldSim& sim, std::size_t choice = 0)
{
    using namespace PerfectWorld;
    CommandResult last;
    for (std::size_t i = 0; i < TutorialSituationCount && sim.State().Tutorial.Phase == TutorialDungeonPhase::Active; ++i)
    {
        const auto& situation = TutorialSituations()[sim.State().Tutorial.NextSituation];
        last = S04Accept(sim, TutorialAct{std::string(situation.StationId), std::string(situation.Choices[choice].Id)},
                         "a completed tutorial action is accepted");
    }
    return last;
}

inline std::string R94Hex(std::uint64_t value)
{
    static const char digits[] = "0123456789abcdef";
    std::string out = "0x";
    for (int shift = 60; shift >= 0; shift -= 4)
        out += digits[(value >> shift) & 0xF];
    return out;
}

inline std::string R94Profile(const PerfectWorld::AwakenedProfile& p)
{
    std::string out;
    for (const auto v : p.Rating)
        out += std::to_string(v) + ",";
    out += std::to_string(p.SupervisedPracticeMinutes) + "," + std::to_string(static_cast<int>(p.Grade));
    for (const auto v : p.AffinityPercent)
        out += "," + std::to_string(v);
    return out;
}
