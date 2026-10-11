#pragma once

// Shared by the TimeContract test files (ROADMAP M3-01).
#include "ContentRegistry.h"
#include "TimeContract.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

inline int TimeTestChecks = 0;

inline void Expect(bool condition, const char* message)
{
    ++TimeTestChecks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

inline std::vector<PerfectWorld::ContentFile> TimeTestReadContent(const std::filesystem::path& root)
{
    namespace fs = std::filesystem;
    std::vector<fs::path> paths;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());
    std::vector<PerfectWorld::ContentFile> files;
    for (const fs::path& path : paths)
    {
        std::ifstream in(path, std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        files.push_back({"/" + fs::relative(path, root).generic_string(), text.str()});
    }
    return files;
}

/** A small deterministic generator (splitmix64) so the random tests replay exactly. */
struct TimeTestRng
{
    std::uint64_t State = 1;

    explicit TimeTestRng(std::uint64_t seed) : State(seed) {}

    std::uint64_t Next()
    {
        State += 0x9e3779b97f4a7c15ULL;
        std::uint64_t x = State;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
        return x ^ (x >> 31);
    }

    /** Uniform in [lo, hi]. */
    std::int64_t Range(std::int64_t lo, std::int64_t hi) { return lo + static_cast<std::int64_t>(Next() % static_cast<std::uint64_t>(hi - lo + 1)); }
};

inline PerfectWorld::InteractionDeclaration MakeDecl(const char* id, PerfectWorld::ClockMode mode, std::int32_t cost,
                                                     PerfectWorld::InterruptPolicy interrupt, PerfectWorld::ResolutionBoundary resolve,
                                                     std::int32_t beats = 1)
{
    PerfectWorld::InteractionDeclaration d;
    d.Id = id;
    d.Mode = mode;
    d.CostMinutes = cost;
    d.Interrupt = interrupt;
    d.Resolve = resolve;
    d.Beats = beats;
    return d;
}

inline PerfectWorld::TimeEvent Progress(std::int64_t minutes) { return {PerfectWorld::TimeEventKind::Progress, minutes}; }
inline PerfectWorld::TimeEvent InterruptEvent() { return {PerfectWorld::TimeEventKind::Interrupt, 0}; }
inline PerfectWorld::TimeEvent FinishEvent() { return {PerfectWorld::TimeEventKind::Finish, 0}; }
inline PerfectWorld::TimeEvent CancelEvent() { return {PerfectWorld::TimeEventKind::Cancel, 0}; }

/** Everything that must be identical between two resolutions of the same input (the events used are excluded). */
inline bool SameResolution(const PerfectWorld::TimeResolution& a, const PerfectWorld::TimeResolution& b)
{
    return a.Error == b.Error && a.Outcome == b.Outcome && a.ElapsedMinutes == b.ElapsedMinutes && a.ChargedMinutes == b.ChargedMinutes &&
           a.BeatsCompleted == b.BeatsCompleted && a.OutcomeBeats == b.OutcomeBeats && a.InterruptsHeld == b.InterruptsHeld;
}

/** A random declaration that is valid by construction, of any of the seven modes. */
PerfectWorld::InteractionDeclaration TimeTestRandomDeclaration(TimeTestRng& rng);
/** A random event sequence for a declaration: mostly progress, some interrupts, finishes and cancels. */
std::vector<PerfectWorld::TimeEvent> TimeTestRandomEvents(TimeTestRng& rng);

void TestModeRules();
void TestDeclarations();
void TestController();
void TestText();
void TestResolveBasics();
void TestInterruptionPolicies();
void TestMissionClock();
void TestRandomResolution();
void TestContent(const std::vector<PerfectWorld::ContentFile>& files);
void TestEquivalence();
void TestRandomEquivalence();
