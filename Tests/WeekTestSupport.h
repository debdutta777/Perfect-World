#pragma once

// Shared by the week compression and state diff tests (ROADMAP M3-02).
#include "StateDiff.h"
#include "WeekCompression.h"
#include "WorldHash.h"
#include "WorldReducer.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

extern int WeekChecks;

inline void WeekExpect(bool condition, const char* message)
{
    ++WeekChecks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

/** The real activity catalogue and the science stream's bell schedule, loaded once by main. */
const PerfectWorld::DayLoopCatalogues& WeekCats();

/** A fresh life on the opening morning (06:00 on Monday 2 June 2042) with the rested player the day-loop tests use. */
inline PerfectWorld::WorldState WeekMorningWorld(std::uint64_t seed = 77)
{
    PerfectWorld::WorldState world = PerfectWorld::MakeWorldState(seed);
    world.DayLoop = PerfectWorld::NewDayLoopState(world.Clock.Now());
    world.DayLoop.Condition.Energy = PerfectWorld::MeterMax;
    return world;
}

inline PerfectWorld::WeekEntry WeekActivity(std::int32_t minute, const char* id, const char* place)
{
    PerfectWorld::WeekEntry entry;
    entry.Kind = PerfectWorld::WeekEntryKind::Activity;
    entry.StartMinute = minute;
    entry.ActivityId = id;
    entry.PlaceKind = place;
    return entry;
}

inline PerfectWorld::WeekEntry WeekSchool(std::int32_t minute)
{
    PerfectWorld::WeekEntry entry;
    entry.Kind = PerfectWorld::WeekEntryKind::School;
    entry.StartMinute = minute;
    return entry;
}

inline PerfectWorld::WeekEntry WeekSleep(std::int32_t minute, std::int32_t minutes)
{
    PerfectWorld::WeekEntry entry;
    entry.Kind = PerfectWorld::WeekEntryKind::Sleep;
    entry.StartMinute = minute;
    entry.Minutes = minutes;
    return entry;
}

inline PerfectWorld::WeekEntry WeekSpend(std::int32_t minute, std::int32_t minutes, PerfectWorld::DayPassKind kind)
{
    PerfectWorld::WeekEntry entry;
    entry.Kind = PerfectWorld::WeekEntryKind::Spend;
    entry.StartMinute = minute;
    entry.Minutes = minutes;
    entry.Pass = kind;
    return entry;
}

/** A school day from the scripted day of the roadmap test: breakfast, the walk, school, three classes, home, sleep. */
std::vector<PerfectWorld::WeekEntry> WeekSchoolDay();
/** A day at home: late breakfast, study, chores, a run, supper, sleep. */
std::vector<PerfectWorld::WeekEntry> WeekHomeDay();
/** Monday to Friday at school and the weekend at home, from 06:00 on Monday 2 June 2042. */
PerfectWorld::WeekPlan WeekScriptedSchoolWeek();

/** Everything the equivalence rule compares, with a message naming the first difference. */
inline void WeekExpectEquivalent(const PerfectWorld::WeekResult& a, const PerfectWorld::WeekResult& b, const char* message)
{
    const PerfectWorld::WeekEquivalence report = PerfectWorld::CompareWeekResults(a, b);
    if (!report.Equivalent())
    {
        std::cerr << "not equivalent (" << message << "):\n";
        for (const std::string& difference : report.Differences)
            std::cerr << "  " << difference << '\n';
    }
    WeekExpect(report.Equivalent(), message);
}

/** A small deterministic generator (xorshift64*), as the other suites use. */
struct WeekRng
{
    std::uint64_t S;
    std::uint64_t Next()
    {
        S ^= S >> 12;
        S ^= S << 25;
        S ^= S >> 27;
        return (S * 2685821657736338717ULL) >> 16;
    }
    std::int64_t Pick(std::int64_t low, std::int64_t high)
    {
        return low + static_cast<std::int64_t>(Next() % static_cast<std::uint64_t>(high - low + 1));
    }
    bool Chance(std::int64_t permille) { return Pick(0, 999) < permille; }
};

void TestWeekScripted();
void TestWeekRefusals();
void TestWeekRules();
void TestWeekRandom();
void TestStateDiff();
void TestStateDiffMutations();
void TestStateDiffSaves(const char* toolPath);
