// ROADMAP M3-02: a single change to a state is reported as exactly the field that changed, in the section it belongs to.
#include "WeekTestSupport.h"

#include "GateSites.h"

#include <algorithm>

using namespace PerfectWorld;

namespace
{
using Paths = std::vector<std::string>;

Paths WeekDiffPaths(const WorldState& a, const WorldState& b)
{
    return StateDiffPaths(DiffWorldStates(a, b));
}

bool WeekHasPath(const Paths& paths, const std::string& path)
{
    return std::find(paths.begin(), paths.end(), path) != paths.end();
}

bool WeekAllUnder(const Paths& paths, const std::string& prefix)
{
    return std::all_of(paths.begin(), paths.end(), [&](const std::string& path) { return path.rfind(prefix, 0) == 0; });
}

void WeekExpectPaths(const WorldState& base, const WorldState& changed, const Paths& expected, const char* message)
{
    const Paths got = WeekDiffPaths(base, changed);
    if (got != expected)
    {
        std::cerr << message << ":\n  got:";
        for (const std::string& path : got)
            std::cerr << ' ' << path;
        std::cerr << "\n  expected:";
        for (const std::string& path : expected)
            std::cerr << ' ' << path;
        std::cerr << '\n';
    }
    WeekExpect(got == expected, message);
}

/** A world with something in every part: a Gate, flags, pending events and two resolved days (a school day and a home day). */
WorldState WeekRichWorld()
{
    WorldSim sim(WeekMorningWorld(5));
    const auto submit = [&sim](CommandPayload payload, CommandSource source) {
        WeekExpect(sim.Submit({source, std::move(payload)}).Accepted, "a world command is accepted");
    };
    submit(GateRegister{"gate.a.one", GateRank::E, GateStatus::Contained}, CommandSource::System);
    submit(GateScheduleOnset{"gate.a.one", 50000}, CommandSource::System);
    submit(ScheduleWorldEvent{60000, "sys/kind/later"}, CommandSource::System);
    submit(SetWorldFlag{"week.flag", true}, CommandSource::Player);
    WeekPlan plan;
    plan.Days.push_back({WeekSchoolDay()});
    plan.Days.push_back({WeekHomeDay()});
    const WeekResult result = CompressWeek(sim.State(), plan, WeekCats());
    WeekExpect(result.Ok(), "two days resolve");
    return result.World;
}

DayCommand WeekDiffCommand(DayCommandKind kind, std::int64_t minutes = 0)
{
    DayCommand command;
    command.Kind = kind;
    command.Minutes = minutes;
    return command;
}

void WeekApplyDay(WorldState& world, const DayCommand& command)
{
    const DayLoopResult result = ApplyDayCommand(world.DayLoop, WeekCats(), command);
    if (!result.Ok())
        std::cerr << "refused: " << DayLoopErrorCode(result.Error) << ' ' << result.Detail << '\n';
    WeekExpect(result.Ok(), "a day command is accepted");
}

void TestWorldSections(const WorldState& rich)
{
    WorldState c = rich;
    c.WorldSeed += 1;
    WeekExpectPaths(rich, c, {"meta.seed"}, "the seed");
    c = rich;
    c.Seq += 1;
    WeekExpectPaths(rich, c, {"meta.seq"}, "the command count");

    c = rich;
    (void)c.Clock.Advance(1);
    WeekExpectPaths(rich, c, {"clock.now"}, "the clock");
    WeekExpect(DiffWorldStates(rich, c).Entries[0].A == std::to_string(rich.Clock.Now()) &&
                   DiffWorldStates(rich, c).Entries[0].B == std::to_string(rich.Clock.Now() + 1),
               "with the two instants");
    const std::int64_t handle = rich.Clock.CaptureSave().NextHandle;
    c = rich;
    (void)c.Clock.Schedule(rich.Clock.Now() + 7000, "sys/kind/new");
    WeekExpectPaths(rich, c, {"clock.next_handle", "clock.pending[" + std::to_string(handle) + "]"}, "a scheduled event");
    const std::int64_t existing = rich.Clock.Pending().front().Handle;
    c = rich;
    WeekExpect(c.Clock.Cancel(existing), "an event is cancelled");
    WeekExpectPaths(rich, c, {"clock.pending[" + std::to_string(existing) + "]"}, "a cancelled event");

    c = rich;
    c.Flags["new.flag"] = true;
    WeekExpectPaths(rich, c, {"flags.new.flag"}, "a new flag");
    WeekExpect(DiffWorldStates(rich, c).Entries[0].A == "(absent)" && DiffWorldStates(rich, c).Entries[0].B == "true", "absent to true");
    c = rich;
    c.Flags["week.flag"] = false;
    WeekExpectPaths(rich, c, {"flags.week.flag"}, "a flipped flag");
    c = rich;
    c.Flags.erase("week.flag");
    WeekExpectPaths(rich, c, {"flags.week.flag"}, "a removed flag");

    const GateSiteState gate = rich.Gates.at("gate.a.one");
    c = rich;
    c.Gates["gate.a.one"].Rank = GateRank::S;
    WeekExpectPaths(rich, c, {"gates.gate.a.one.rank"}, "a Gate's rank");
    c = rich;
    c.Gates["gate.a.one"].Status = GateStatus::Active;
    WeekExpectPaths(rich, c, {"gates.gate.a.one.status"}, "a Gate's status");
    c = rich;
    c.Gates["gate.a.one"].StatusSince += 5;
    WeekExpectPaths(rich, c, {"gates.gate.a.one.since"}, "when the status began");
    c = rich;
    c.Gates["gate.a.one"].OnsetCount += 1;
    WeekExpectPaths(rich, c, {"gates.gate.a.one.onsets"}, "the onset count");
    c = rich;
    c.Gates["gate.b.two"] = gate;
    WeekExpectPaths(rich, c, {"gates.gate.b.two"}, "a new Gate");
    WeekExpect(DiffWorldStates(rich, c).Entries[0].B == "rank=E status=contained since=0 onsets=0", "is shown whole");
    c = rich;
    c.Gates.clear();
    WeekExpectPaths(rich, c, {"gates.gate.a.one"}, "a removed Gate");

    c = rich;
    c.Rng[static_cast<std::size_t>(RandomStreamId::Tower)].Words[2] ^= 1;
    WeekExpectPaths(rich, c, {"rng.tower.word2"}, "a stream's state word");
    c = rich;
    c.Rng[static_cast<std::size_t>(RandomStreamId::Opportunity)].Draws += 1;
    c.Rng[static_cast<std::size_t>(RandomStreamId::Crystallisation)].Draws += 1;
    WeekExpectPaths(rich, c, {"rng.opportunity.draws", "rng.crystallisation.draws"}, "two streams' draws, in stream order");
}

void TestDayFields(const WorldState& rich)
{
    const std::pair<const char*, std::int32_t ConditionState::*> meters[] = {
        {"dayloop.core.condition.energy", &ConditionState::Energy},
        {"dayloop.core.condition.hunger", &ConditionState::Hunger},
        {"dayloop.core.condition.health", &ConditionState::Health},
        {"dayloop.core.condition.stress", &ConditionState::Stress}};
    for (const auto& [path, member] : meters)
    {
        WorldState c = rich;
        c.DayLoop.Condition.*member += c.DayLoop.Condition.*member > 0 ? -1 : 1;
        WeekExpectPaths(rich, c, {path}, "one condition meter");
    }
    WorldState c = rich;
    c.DayLoop.Seq += 1;
    WeekExpectPaths(rich, c, {"dayloop.core.state.seq"}, "the day loop's command count");
    c = rich;
    c.DayLoop.Wakes += 1;
    WeekExpectPaths(rich, c, {"dayloop.core.state.wakes"}, "the wake count");
    c = rich;
    c.DayLoop.Last.Kind = DayCommandKind::Pass;
    WeekExpectPaths(rich, c, {"dayloop.core.last.kind"}, "the last action");
    c = rich;
    c.DayLoop.Phase = DayPhase::Sleep; // not a state a day can be in
    const StateDiffReport invalid = DiffWorldStates(rich, c);
    WeekExpect(StateDiffPaths(invalid) == Paths({"dayloop.valid"}) && invalid.Entries[0].A == "true" && invalid.Entries[0].B == "false",
               "a day that is not valid is shown as such, not hidden");
}

void TestDayCommands(const WorldState& rich)
{
    WeekExpect(!rich.DayLoop.Planned && !rich.DayLoop.Asleep && rich.DayLoop.Phase == DayPhase::Plan, "the rich world wakes unplanned");
    WorldState planned = rich;
    WeekApplyDay(planned, WeekDiffCommand(DayCommandKind::PlanDay));
    WeekExpectPaths(rich, planned,
                    {"dayloop.core.state.phase", "dayloop.core.state.seq", "dayloop.core.state.planned", "dayloop.core.last.kind"},
                    "planning the day changes the phase, the count, the flag and the last action");

    WorldState twoItems = rich;
    DayCommand plan = WeekDiffCommand(DayCommandKind::PlanDay);
    plan.Plan = {{360, "activity.eat_meal"}, {480, "activity.study_at_home"}};
    WeekApplyDay(twoItems, plan);
    const StateDiffReport items = DiffWorldStates(planned, twoItems);
    WeekExpect(StateDiffPaths(items) == Paths({"dayloop.core.plan", "dayloop.core.p[0]", "dayloop.core.p[1]"}) &&
                   items.Entries[0].A == "0" && items.Entries[0].B == "2" && items.Entries[1].A == "(absent)" &&
                   items.Entries[1].B == "p start=360 activity=activity.eat_meal",
               "plan items present on one side only are lines, with the plan's count beside them");

    WorldState committed = planned;
    DayCommand commit = WeekDiffCommand(DayCommandKind::Commit);
    commit.ActivityId = "activity.eat_meal";
    commit.PlaceKind = "dining";
    WeekApplyDay(committed, commit);
    const StateDiffReport commitment = DiffWorldStates(planned, committed);
    WeekExpect(StateDiffPaths(commitment) == Paths({"dayloop.core.state.seq", "dayloop.core.last.kind", "dayloop.core.last.activity",
                                                    "dayloop.core.commitment", "dayloop.core.c"}) &&
                   commitment.Entries[4].A == "(absent)" &&
                   commitment.Entries[4].B == "c activity=activity.eat_meal place=dining decided=" + std::to_string(rich.DayLoop.Now) +
                                                  " alternatives=0",
               "a commitment changes the core lines and adds its own");

    WorldState started = committed;
    WeekApplyDay(started, WeekDiffCommand(DayCommandKind::Start));
    Paths paths = WeekDiffPaths(committed, started);
    WeekExpect(WeekHasPath(paths, "dayloop.runner.instance") && WeekHasPath(paths, "dayloop.runner.activity") &&
                   WeekHasPath(paths, "dayloop.runner.started_at") && !WeekHasPath(paths, "dayloop.runner.applied") &&
                   WeekHasPath(paths, "dayloop.core.state.seq") && !WeekHasPath(paths, "dayloop.core.state.now"),
               "starting an activity changes the runner and not the clock");

    WorldState done = started;
    WeekApplyDay(done, WeekDiffCommand(DayCommandKind::Complete));
    paths = WeekDiffPaths(started, done);
    WeekExpect(WeekHasPath(paths, "dayloop.runner.applied") && WeekHasPath(paths, "dayloop.core.state.now") &&
                   WeekHasPath(paths, "dayloop.core.condition.hunger") &&
                   std::any_of(paths.begin(), paths.end(), [](const std::string& p) { return p.rfind("dayloop.receipts.e[seq=", 0) == 0; }),
               "completing it ends the runner, moves the clock, feeds the body and writes the receipt");
    WeekExpect(WeekAllUnder(paths, "dayloop."), "and touches nothing outside the day (the world clock is the caller's to move)");

    // Studying feeds the practice window; the family's line is told apart by its id.
    WorldState study = planned;
    DayCommand studyCommit = WeekDiffCommand(DayCommandKind::Commit);
    studyCommit.ActivityId = "activity.study_at_home";
    studyCommit.PlaceKind = "home";
    WeekApplyDay(study, studyCommit);
    WeekApplyDay(study, WeekDiffCommand(DayCommandKind::Start));
    WorldState studied = study;
    WeekApplyDay(studied, WeekDiffCommand(DayCommandKind::Complete));
    paths = WeekDiffPaths(study, studied);
    WeekExpect(WeekHasPath(paths, "dayloop.practice.last_at") && WeekHasPath(paths, "dayloop.practice.family[family.studied]"),
               "studying changes the practice window, the family by id");

    // Arriving at school changes the attendance record, field by field.
    WorldState arrived = planned;
    WeekApplyDay(arrived, WeekDiffCommand(DayCommandKind::ArriveSchool));
    const StateDiffReport school = DiffWorldStates(planned, arrived);
    const AttendanceRecord& before = planned.DayLoop.Attendance;
    bool schoolDays = false;
    bool dayCount = false;
    for (const StateDiffEntry& entry : school.Entries)
    {
        schoolDays = schoolDays || (entry.Path == "dayloop.attendance.totals.school_days" && entry.A == std::to_string(before.SchoolDays) &&
                                    entry.B == std::to_string(before.SchoolDays + 1));
        dayCount = dayCount || (entry.Path == "dayloop.attendance.days.count" && entry.A == std::to_string(before.Days.size()) &&
                                entry.B == std::to_string(before.Days.size() + 1));
    }
    WeekExpect(schoolDays && dayCount && school.InSection("dayloop").size() == school.Entries.size(),
               "arriving at school changes the totals and the day count, named by what they are");
    WeekExpect(std::none_of(school.Entries.begin(), school.Entries.end(),
                            [](const StateDiffEntry& e) { return e.Path.find("attendance.end") != std::string::npos; }),
               "the record's own hash line is not reported");
}
} // namespace

void TestStateDiffMutations()
{
    const WorldState rich = WeekRichWorld();
    WeekExpect(DiffWorldStates(rich, WorldState(rich)).Identical(), "the rich world equals its copy");
    TestWorldSections(rich);
    TestDayFields(rich);
    TestDayCommands(rich);
}
