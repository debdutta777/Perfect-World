// ROADMAP M3-02: the persistent-state diff. This file: identical states, the report's shape and order,
// symmetry, determinism and the ignored paths. Single mutations are in StateDiffMutationTests.cpp.
#include "WeekTestSupport.h"

#include <algorithm>

using namespace PerfectWorld;

namespace
{
std::string HexOf(std::uint64_t value)
{
    constexpr const char* digits = "0123456789abcdef";
    std::string text = "0x";
    for (int shift = 60; shift >= 0; shift -= 4)
        text += digits[(value >> shift) & 0xF];
    return text;
}

void TestIdentical()
{
    const WorldState world = WeekMorningWorld();
    const WorldState copy = world;
    const StateDiffReport report = DiffWorldStates(world, copy);
    WeekExpect(report.Identical() && report.Entries.empty() && report.SectionNames().empty(), "a state compared with its copy differs nowhere");
    WeekExpect(report.HashA == HashWorldState(world) && report.HashB == report.HashA, "the report carries the world hashes");
    WeekExpect(RenderStateDiff(report) == "pwstatediff: identical (world hash " + HexOf(report.HashA) + ")\n",
               "the report of identical states says so, with the hash");

    // A state with everything in it: a resolved week, a Gate, pending events, flags.
    const WeekResult week = CompressWeek(world, WeekScriptedSchoolWeek(), WeekCats());
    WeekExpect(week.Ok(), "the week resolves");
    WeekExpect(DiffWorldStates(week.World, WorldState(week.World)).Identical(), "a busy world compared with its copy differs nowhere");
    WeekExpect(DiffWorldStates(week.World, CompressWeek(world, WeekScriptedSchoolWeek(), WeekCats()).World).Identical(),
               "two resolutions of the same week are the same state, command counters included");
    WeekExpect(!DiffWorldStates(world, week.World).Identical(), "a week later is not the same state");
}

WorldState WeekDiffBusy()
{
    WorldState a = WeekMorningWorld();
    a.Flags["door.open"] = true;
    a.Gates["gate.a.one"] = GateSiteState{GateRank::C, GateStatus::Active, 100, 1};
    a.Rng[1].Draws = 40;
    return a;
}

void TestShapeAndOrder()
{
    // Differences in every section at once: reported in the order of the world hash, each section once.
    const WorldState a = WeekDiffBusy();
    WorldState b = a;
    b.DayLoop.Condition.Energy -= 5;
    b.Rng[1].Draws = 41;
    b.Gates["gate.a.one"].Status = GateStatus::Sealed;
    b.Flags["door.open"] = false;
    (void)b.Clock.Schedule(b.Clock.Now() + 100, "sys/kind/x");
    b.Seq = 9;
    const StateDiffReport report = DiffWorldStates(a, b);
    WeekExpect(report.SectionNames() == std::vector<std::string>({"meta", "clock", "flags", "gates", "rng", "dayloop"}),
               "sections are reported in the world hash's order");
    WeekExpect(StateDiffPaths(report) == std::vector<std::string>({"meta.seq", "clock.next_handle", "clock.pending[1]", "flags.door.open",
                                                                    "gates.gate.a.one.status", "rng.tower.draws",
                                                                    "dayloop.core.condition.energy"}),
               "paths are stable and readable");
    WeekExpect(report.InSection("rng").size() == 1 && report.InSection("rng")[0].A == "40" && report.InSection("rng")[0].B == "41" &&
                   report.InSection("nothing").empty(),
               "a section's entries can be asked for");
    const std::string text = RenderStateDiff(report);
    const std::string expected = "pwstatediff: 7 differences in 6 sections (world hash a=" + HexOf(report.HashA) + " b=" + HexOf(report.HashB) +
                                 ")\nsection meta\n  meta.seq: 0 -> 9\nsection clock\n  clock.next_handle: 1 -> 2\n"
                                 "  clock.pending[1]: (absent) -> when=" + std::to_string(b.Clock.Now() + 100) +
                                 " id=sys/kind/x\nsection flags\n  flags.door.open: true -> false\nsection gates\n"
                                 "  gates.gate.a.one.status: active -> sealed\nsection rng\n  rng.tower.draws: 40 -> 41\nsection dayloop\n"
                                 "  dayloop.core.condition.energy: 100000 -> 99995\n";
    if (text != expected)
        std::cerr << "got:\n" << text << "expected:\n" << expected;
    WeekExpect(text == expected, "the report text is exactly as documented");
    WeekExpect(RenderStateDiff(DiffWorldStates(a, b)) == text, "and the same every time");

    // Swapping the two states swaps the values and nothing else.
    const StateDiffReport swapped = DiffWorldStates(b, a);
    WeekExpect(StateDiffPaths(swapped) == StateDiffPaths(report) && swapped.Entries.size() == report.Entries.size(),
               "comparing the other way round reports the same fields");
    for (std::size_t i = 0; i < report.Entries.size(); ++i)
        WeekExpect(swapped.Entries[i].A == report.Entries[i].B && swapped.Entries[i].B == report.Entries[i].A, "with the values swapped");
    WeekExpect(swapped.HashA == report.HashB && swapped.HashB == report.HashA, "and the hashes");
}

void TestIgnored()
{
    const WorldState a = WeekMorningWorld();
    WorldState b = a;
    b.Seq = 3;
    b.DayLoop.Seq = 1;
    b.DayLoop.HasLastAction = true;
    b.DayLoop.Last = {DayCommandKind::PlanDay, 0, ""};
    b.DayLoop.Condition.Energy -= 2;
    b.DayLoop.Condition.Hunger += 2;
    StateDiffOptions options;
    options.IgnorePaths = {"meta.seq", "dayloop.core.state.seq"};
    StateDiffReport report = DiffWorldStates(a, b, options);
    WeekExpect(StateDiffPaths(report) == std::vector<std::string>({"dayloop.core.condition.energy", "dayloop.core.condition.hunger",
                                                                    "dayloop.core.last.kind"}),
               "ignored paths are left out and nothing else");
    options.IgnorePaths.push_back("dayloop.core.condition");
    options.IgnorePaths.push_back("dayloop.core.last");
    report = DiffWorldStates(a, b, options);
    WeekExpect(report.Identical(), "an ignored path covers everything below it");
    WeekExpect(report.HashA != report.HashB &&
                   RenderStateDiff(report) == "pwstatediff: no differences outside the ignored fields (world hash a=" + HexOf(report.HashA) +
                                                  " b=" + HexOf(report.HashB) + ")\n",
               "and the report says the hashes still differ");
    options.IgnorePaths = {"dayloop.core.state"};
    report = DiffWorldStates(a, b, options);
    WeekExpect(StateDiffPaths(report) == std::vector<std::string>({"meta.seq", "dayloop.core.condition.energy",
                                                                    "dayloop.core.condition.hunger", "dayloop.core.last.kind"}),
               "ignoring a line leaves its neighbours");
    options.IgnorePaths = {"dayloop.core.stat"};
    WeekExpect(DiffWorldStates(a, b, options).Entries.size() == 5, "an ignored path matches whole path pieces only");
}
} // namespace

void TestStateDiff()
{
    TestIdentical();
    TestShapeAndOrder();
    TestIgnored();
}
