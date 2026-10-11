#pragma once

// The scripted non-combat personas for "every Tattva can be reached by at least 2 non-combat personas"
// (ROADMAP M4-02). Each is a Life (AwakeningTestSupport.h): evidence, not a name. "Non-combat" is the
// Aptitude Projection's own reading: no persona's life points at the Hunter path (its band stays below
// Plausible), and no persona's dungeon conduct is anything but ordinary conduct.
#include "AwakeningTestSupport.h"

struct Persona
{
    const char* Name;
    PerfectWorld::Tattva Target;
    Life Evidence;
};

inline std::vector<Persona> NonCombatPersonas()
{
    using namespace PerfectWorld;
    using A = ConductAction;
    using F = AptitudeFacet;
    using S = AptitudeFacetState;
    // Growth order: Body, Agility, Intelligence, Perception, Willpower, Charisma.
    return {
        {"ward volunteer", Tattva::Prithvi,
         {{6'000, 0, 0, 0, 18'000, 0},
          {{"skill.self_management", 70'000}, {"skill.logistics", 40'000}, {"skill.household_tasks", 30'000}},
          {{F::Steadiness, S::Established}, {F::Scheduling, S::Shown}},
          {{A::Hold, 2}, {A::Assist, 2}, {A::Hold, 1}}}},
        {"accounts keeper", Tattva::Prithvi,
         {{0, 0, 0, 0, 14'000, 0},
          {{"skill.commerce", 70'000}, {"skill.self_management", 60'000}, {"skill.logistics", 40'000}},
          {{F::Accounts, S::Established}, {F::Steadiness, S::Shown}},
          {{A::Hold, 2}, {A::Hold, 1}, {A::Observe, 1}}}},

        {"negotiator", Tattva::Jala,
         {{0, 0, 0, 15'000, 0, 24'000},
          {{"skill.communication", 80'000}, {"skill.commerce", 20'000}},
          {{F::Teamwork, S::Established}, {F::Interpretation, S::Shown}},
          {{A::Assist, 3}, {A::Improvise, 1}, {A::Assist, 2}}}},
        {"community organiser", Tattva::Jala,
         {{0, 0, 0, 8'000, 0, 20'000},
          {{"skill.communication", 60'000}, {"skill.household_tasks", 20'000}},
          {{F::Teamwork, S::Shown}, {F::Steadiness, S::Shown}},
          {{A::Assist, 2}, {A::Improvise, 2}, {A::Assist, 1}}}},

        {"repairer", Tattva::Agni,
         {{0, 0, 0, 8'000, 0, 0},
          {{"skill.practical_repair", 80'000}, {"skill.household_tasks", 50'000}, {"skill.computing", 20'000}},
          {{F::HandsOn, S::Established}, {F::FaultFinding, S::Shown}},
          {{A::Improvise, 2}, {A::Hold, 1}, {A::Improvise, 1}}}},
        {"maker", Tattva::Agni,
         {{0, 0, 0, 0, 0, 0},
          {{"skill.creative", 80'000}, {"skill.practical_repair", 40'000}},
          {{F::Creative, S::Established}, {F::HandsOn, S::Shown}},
          {{A::Improvise, 2}, {A::Observe, 1}, {A::Improvise, 1}}}},

        {"courier", Tattva::Vayu,
         {{10'000, 12'000, 0, 0, 0, 0},
          {{"skill.logistics", 70'000}, {"skill.fitness", 40'000}},
          {{F::Scheduling, S::Established}, {F::Accounts, S::Shown}},
          {{A::Evade, 2}, {A::Improvise, 2}, {A::Evade, 1}}}},
        {"runner", Tattva::Vayu,
         {{14'000, 20'000, 0, 0, 0, 0},
          {{"skill.fitness", 80'000}},
          {{F::Physical, S::Established}},
          {{A::Improvise, 2}, {A::Evade, 1}, {A::Improvise, 1}}}},

        {"scholar", Tattva::Akasha,
         {{0, 0, 26'000, 0, 0, 0},
          {{"skill.academics", 80'000}, {"skill.computing", 40'000}},
          {{F::Analysis, S::Established}, {F::Interpretation, S::Shown}},
          {{A::Observe, 2}, {A::Withdraw, 1}, {A::Observe, 1}}}},
        {"surveyor", Tattva::Akasha,
         {{0, 0, 12'000, 22'000, 0, 0},
          {{"skill.academics", 40'000}},
          {{F::FaultFinding, S::Shown}, {F::Analysis, S::Shown}},
          {{A::Observe, 2}, {A::Observe, 1}, {A::Withdraw, 2}}}},
    };
}
