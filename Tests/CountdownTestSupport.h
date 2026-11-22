#pragma once

// Shared by the countdown tests (ROADMAP M4-03).
#include "Countdown.h"
#include "WorldHash.h"
#include "WorldReducer.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

extern int CdChecks;

inline void CdExpect(bool condition, const char* message)
{
    ++CdChecks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

/** The real activity catalogue, the science stream's bell schedule and the real 2042-43 calendar, loaded once by main. */
const PerfectWorld::DayLoopCatalogues& CdCats();

/** The world on Awakening Day at 18:00: in step, awake, rested, the attendance record starting after Awakening Day. */
PerfectWorld::WorldState CdStartWorld(std::uint64_t seed = 4243);

/** One person in the player's life, with the relationship values the tests need. */
struct CdTie
{
    std::string Role;
    std::string Person;
    std::int32_t Trust = 0;
    std::int32_t Warmth = 0;
    std::int32_t Respect = 0;
};

/** The ties every strategy starts with: a close friend, a teacher who respects the player, a neighbour, a rival, a sibling. */
std::vector<CdTie> CdDefaultTies();

/** A relationship book holding exactly these pairs (built directly: the test needs exact values, not the act history). */
PerfectWorld::RelationshipBook CdBook(const std::vector<CdTie>& ties);

/** A context with the ties as contacts and `book` as the relationships. */
PerfectWorld::CountdownContext CdContext(const PerfectWorld::RelationshipBook* book, const std::vector<CdTie>& ties);

/** Opens the countdown on the start world; stops the test if it is refused. */
struct CdSession
{
    PerfectWorld::WorldState World;
    PerfectWorld::CountdownState State;
};
CdSession CdOpen(const PerfectWorld::CountdownContext& context, std::uint64_t seed = 4243);

/** How a day is filled around the committed blocks. */
struct CdStyle
{
    /** Study sessions of two hours, in the order of the day (at most 5). */
    int Study = 2;
    bool Run = false;
    bool Chores = false;
    /** A rest of an hour in the evening. */
    bool Rest = false;
    /** Minute of the day sleep begins; it lasts until 06:00 the next morning. */
    int SleepStart = 1320;
};

/** The player's plan for one day of the span (0 to 9) around the state's reserved blocks, with nothing before `notBefore`. */
PerfectWorld::WeekDayPlan CdDay(const PerfectWorld::CountdownState& state, int day, const CdStyle& style, PerfectWorld::GameInstant notBefore);

/** The plans for the days from `fromDay` to the last day (9), the first beginning no earlier than `notBefore`. */
std::vector<PerfectWorld::WeekDayPlan> CdDays(const PerfectWorld::CountdownState& state, int fromDay, const CdStyle& style,
                                              PerfectWorld::GameInstant notBefore);

/** The span day (0 to 10) the clock is in. */
int CdSpanDay(const PerfectWorld::WorldState& world);

/** Resolves every remaining day in the style, one call at a time (the news stops a call), until the countdown closes. */
void CdFinish(CdSession& session, const CdStyle& style, int maxDaysPerCall = 7);

/** Takes an opportunity, stops the test on a refusal. */
void CdTake(CdSession& session, const PerfectWorld::CountdownContext& context, const char* id);

void TestCountdownSpanAndOpen();
void TestCountdownContent();
void TestCountdownExclusion();
void TestCountdownDisclosure();
void TestCountdownObligations();
void TestCountdownEvent();
void TestCountdownReload();
void TestCountdownSave();
void TestCountdownStrategies();
void TestCountdownClose();
