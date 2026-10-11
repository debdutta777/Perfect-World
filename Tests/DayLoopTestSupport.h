#pragma once

// Shared by DayLoopTests.cpp (the scripted day and the refusals), DayLoopSaveTests.cpp (the envelope,
// resuming at every boundary, checkpoints) and DayLoopRandomTests.cpp (random sequences), ROADMAP M2-12.
#include "DayLoop.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

extern int DayLoopChecks;

inline void Check(bool condition, const char* message)
{
    ++DayLoopChecks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

/** The real activity catalogue and the science stream's bell schedule, loaded once by main. */
const PerfectWorld::DayLoopCatalogues& DayLoopCats();

/** A rested player on the opening morning: 06:00 on Monday 2 June 2042 with the energy of a good night's sleep. */
inline PerfectWorld::DayLoopState MorningState()
{
    PerfectWorld::DayLoopState state = PerfectWorld::NewDayLoopState();
    state.Condition.Energy = PerfectWorld::MeterMax;
    return state;
}

inline PerfectWorld::DayCommand PlanCmd(std::vector<PerfectWorld::DayPlanItem> items)
{
    PerfectWorld::DayCommand c;
    c.Kind = PerfectWorld::DayCommandKind::PlanDay;
    c.Plan = std::move(items);
    return c;
}

inline PerfectWorld::DayCommand CommitCmd(std::string activity, std::string place,
                                          std::vector<PerfectWorld::ReceiptCandidate> alternatives = {})
{
    PerfectWorld::DayCommand c;
    c.Kind = PerfectWorld::DayCommandKind::Commit;
    c.ActivityId = std::move(activity);
    c.PlaceKind = std::move(place);
    c.Alternatives = std::move(alternatives);
    return c;
}

inline PerfectWorld::DayCommand SimpleCmd(PerfectWorld::DayCommandKind kind, std::int64_t minutes = 0)
{
    PerfectWorld::DayCommand c;
    c.Kind = kind;
    c.Minutes = minutes;
    return c;
}

inline PerfectWorld::DayCommand PassCmd(std::int64_t minutes, PerfectWorld::DayPassKind kind)
{
    PerfectWorld::DayCommand c = SimpleCmd(PerfectWorld::DayCommandKind::Pass, minutes);
    c.Pass = kind;
    return c;
}

inline PerfectWorld::ReceiptCandidate Candidate(std::string id, PerfectWorld::GameInstant known, PerfectWorld::GameInstant start,
                                                std::int32_t minutes, bool available, std::int32_t learning)
{
    PerfectWorld::ReceiptCandidate a;
    a.Id = std::move(id);
    a.Source = "source.friend";
    a.KnownAt = known;
    a.Start = start;
    a.DurationMinutes = minutes;
    a.Available = available;
    a.Forecast.LearningMilliXp = learning;
    return a;
}

/**
 * The scripted school day of the roadmap test: 29 commands from the morning plan to the next waking.
 * `dayIndex` shifts the alternatives' absolute times to that day, so the same day can be played again.
 */
std::vector<PerfectWorld::DayCommand> ScriptedDay(std::int64_t dayIndex = 0);

/** Applies and requires acceptance. */
inline PerfectWorld::DayLoopResult MustApply(PerfectWorld::DayLoopState& state, const PerfectWorld::DayCommand& command,
                                             const char* message)
{
    PerfectWorld::DayLoopResult result = PerfectWorld::ApplyDayCommand(state, DayLoopCats(), command);
    if (!result.Ok())
        std::cerr << "refused: " << PerfectWorld::DayLoopErrorCode(result.Error) << ' ' << result.Detail << '\n';
    Check(result.Ok(), message);
    return result;
}

/** A small deterministic generator (xorshift64*), as the other suites use. */
struct DayLoopRng
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
};

void TestDayLoopRefusals();
void TestDayLoopSave();
void TestDayLoopRandom();
// DayLoopWorldTests.cpp: the scripted day inside the world state, the world hash and the v7 world save.
void TestDayLoopWorld();
// DayLoopCalendarTests.cpp: takes the real 2042-43 calendar.
void TestDayLoopCalendar(const PerfectWorld::SchoolCalendar& calendar);
