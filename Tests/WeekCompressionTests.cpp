// ROADMAP M3-02: week compression under the equivalence rule, and the persistent-state diff tool.
// This file: the test driver, the content, and the scripted school week.
#include "WeekTestSupport.h"

#include "ContentRegistry.h"
#include "SchoolCalendar.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace PerfectWorld;

int WeekChecks = 0;

namespace
{
DayLoopCatalogues Catalogues;
ActivityCatalogue ActivityCat;
BellScheduleSet BellSet;
SchoolCalendarSet CalendarSet;

std::vector<ContentFile> ReadWeekContent(const std::filesystem::path& root)
{
    namespace fs = std::filesystem;
    std::vector<fs::path> paths;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());
    std::vector<ContentFile> files;
    for (const fs::path& path : paths)
    {
        std::ifstream in(path, std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        files.push_back({"/" + fs::relative(path, root).generic_string(), text.str()});
    }
    return files;
}

DayCommand WeekHand(DayCommandKind kind, std::int64_t minutes = 0)
{
    DayCommand command;
    command.Kind = kind;
    command.Minutes = minutes;
    return command;
}

DayCommand WeekHandPass(std::int64_t minutes, DayPassKind kind)
{
    DayCommand command = WeekHand(DayCommandKind::Pass, minutes);
    command.Pass = kind;
    return command;
}

DayCommand WeekHandCommit(const char* activity, const char* place)
{
    DayCommand command = WeekHand(DayCommandKind::Commit);
    command.ActivityId = activity;
    command.PlaceKind = place;
    return command;
}

/** Monday of the scripted week, driven by hand: the day loop's own commands, and the clock moved the way the world tests do. */
WorldState HandDrivenMonday()
{
    WorldState world = WeekMorningWorld();
    const DayCommand start = WeekHand(DayCommandKind::Start);
    const DayCommand complete = WeekHand(DayCommandKind::Complete);
    DayCommand plan = WeekHand(DayCommandKind::PlanDay);
    plan.Plan = {{360, "activity.eat_meal"},         {620, "activity.attend_class"}, {670, "activity.attend_class"},
                 {740, "activity.attend_class"},     {990, "activity.study_at_home"}, {1110, "activity.household_chores"},
                 {1170, "activity.eat_meal"}};
    const std::vector<DayCommand> script = {
        plan,
        WeekHandCommit("activity.eat_meal", "dining"), start, complete,
        WeekHandPass(180, DayPassKind::Wait), WeekHandPass(30, DayPassKind::Walk),
        WeekHand(DayCommandKind::ArriveSchool),
        WeekHandPass(30, DayPassKind::Wait),
        WeekHandCommit("activity.attend_class", "classroom"), start, complete,
        WeekHandPass(5, DayPassKind::Wait),
        WeekHandCommit("activity.attend_class", "classroom"), start, complete,
        WeekHandPass(25, DayPassKind::Wait),
        WeekHandCommit("activity.attend_class", "classroom"), start, complete,
        WeekHandPass(175, DayPassKind::Wait), WeekHandPass(30, DayPassKind::Walk),
        WeekHandCommit("activity.study_at_home", "home"), start, complete,
        WeekHandCommit("activity.household_chores", "home"), start, complete,
        WeekHandCommit("activity.eat_meal", "dining"), start, complete,
        WeekHandPass(130, DayPassKind::Rest),
        WeekHand(DayCommandKind::GoToSleep), WeekHand(DayCommandKind::Wake, 480)};
    for (const DayCommand& command : script)
    {
        const DayLoopResult result = ApplyDayCommand(world.DayLoop, Catalogues, command);
        if (!result.Ok())
            std::cerr << "hand-driven command refused: " << DayLoopErrorCode(result.Error) << ' ' << result.Detail << '\n';
        WeekExpect(result.Ok(), "the hand-driven Monday is accepted");
        if (world.DayLoop.Now > world.Clock.Now())
            WeekExpect(!world.Clock.AdvanceTo(world.DayLoop.Now).Rejected, "the hand-driven clock follows the day");
    }
    WeekExpect(IsWorldDayInStep(world), "the hand-driven Monday ends in step");
    return world;
}

std::int32_t WeekEntryCount(const WeekPlan& plan)
{
    std::int32_t count = 0;
    for (const WeekDayPlan& day : plan.Days)
        count += static_cast<std::int32_t>(day.Entries.size());
    return count;
}

/** The minutes the plan spends in scenes, worked out here from the content's durations rather than by the engine. */
std::int64_t WeekExpectedSceneMinutes(const WeekPlan& plan)
{
    std::int64_t total = 0;
    for (const WeekDayPlan& day : plan.Days)
    {
        for (const WeekEntry& entry : day.Entries)
        {
            if (entry.Kind == WeekEntryKind::Sleep)
                total += entry.Minutes;
            else if (entry.Kind == WeekEntryKind::Activity)
                total += ActivityCat.Find(entry.ActivityId)->DurationMinutes;
        }
    }
    return total;
}

void TestScriptedWeek()
{
    const WorldState world = WeekMorningWorld();
    const WeekPlan plan = WeekScriptedSchoolWeek();
    WeekExpect(plan.Days.size() == 7 && WeekEntryCount(plan) == 5 * 12 + 2 * 8,"the scripted week is five school days and two home days");
    WeekExpect(ValidateWeekPlan(world, plan, WeekCats()).Ok(), "the scripted week validates");

    const WeekResult compressed = CompressWeek(world, plan, WeekCats());
    WeekExpect(compressed.Ok() && compressed.Stop == WeekStop::None, "the week compresses without stopping");
    WeekExpect(compressed.EntriesResolved == WeekEntryCount(plan), "every entry was resolved");
    const WorldState& after = compressed.World;
    WeekExpect(IsWorldDayInStep(after) && after.Clock.Now() == 7 * MinutesPerDay && after.DayLoop.Now == 7 * MinutesPerDay,
               "the week ends at the same minute on the next Monday, with the clocks together");
    WeekExpect(after.DayLoop.Wakes == 7 && !after.DayLoop.Asleep && !after.DayLoop.Planned && after.DayLoop.Phase == DayPhase::Plan,
               "seven nights, woken");
    WeekExpect(after.DayLoop.Attendance.SchoolDays == 5 && after.DayLoop.Attendance.OnTimeDays == 5 &&
                   after.DayLoop.Attendance.PeriodsHeld == 30,
               "five school days, all on time");
    WeekExpect(after.DayLoop.Runner.Instance == 5 * 7 + 2 * 6 && after.DayLoop.Runner.AppliedInstance == after.DayLoop.Runner.Instance,
               "every activity ran once and applied once");

    const WeekLedger& ledger = compressed.Ledger;
    const std::int64_t scene = WeekExpectedSceneMinutes(plan);
    WeekExpect(ledger.EndInstant - ledger.StartInstant == 7 * MinutesPerDay && ledger.TotalCharged == 7 * MinutesPerDay,
               "every minute of the week is charged to something");
    WeekExpect(ledger.ChargedByMode[static_cast<std::size_t>(ClockMode::Scene)] == scene &&
                   ledger.ChargedByMode[static_cast<std::size_t>(ClockMode::FreeRoam)] == 7 * MinutesPerDay - scene,
               "scenes are the activities and the nights; waiting, resting and walking are free roam");
    WeekExpect(ledger.DecidedByOrder == 0 && compressed.Disclosures.empty() && compressed.InterruptsUsed == 0,
               "nothing was decided on the player's behalf");
    WeekExpect(!ledger.Credits.empty() && !ledger.Evidence.empty(), "the week earned learning and evidence for the stats owner");

    // The reference: the same plan at several fine granularities.
    WeekResult first;
    for (const std::int64_t chunk : {15, 1, 7, 60, 360})
    {
        WeekStepOptions options;
        options.ChunkMinutes = chunk;
        const WeekResult stepped = PlayWeekStepByStep(world, plan, WeekCats(), options);
        WeekExpect(stepped.Ok() && stepped.EntriesResolved == compressed.EntriesResolved, "the step-by-step week resolves every entry");
        WeekExpectEquivalent(compressed, stepped, "the compressed week equals the step-by-step week");
        WeekExpect(CompareWeekResults(stepped, compressed).Equivalent(), "equivalence is symmetric");
        if (chunk == 15)
            first = stepped;
    }
    WeekExpect(compressed.DayCommands * 2 < first.DayCommands, "compression uses far fewer commands than quarter-hour steps");
    WeekExpect(compressed.World.Seq != first.World.Seq && compressed.World.DayLoop.Seq != first.World.DayLoop.Seq,
               "the command counts differ, which is the excluded quantity");
    WeekExpect(DiffWorldStates(compressed.World, first.World).Entries.size() >= 2, "an unfiltered diff sees only the command counters");
    WeekExpect(DiffWorldStates(compressed.World, first.World).SectionNames() == std::vector<std::string>({"meta", "dayloop"}),
               "and they are in the meta and dayloop sections");

    // Independent of the engine: Monday driven command by command by hand gives the same day.
    WeekPlan monday;
    monday.Days.push_back(plan.Days[0]);
    const WeekResult mondayCompressed = CompressWeek(world, monday, WeekCats());
    WeekExpect(mondayCompressed.Ok(), "Monday compresses");
    StateDiffOptions ignore;
    for (const std::string_view path : WeekEquivalenceIgnoredPaths)
        ignore.IgnorePaths.emplace_back(path);
    const StateDiffReport handDiff = DiffWorldStates(HandDrivenMonday(), mondayCompressed.World, ignore);
    for (const StateDiffEntry& entry : handDiff.Entries)
        std::cerr << "  " << entry.Path << ": " << entry.A << " -> " << entry.B << '\n';
    WeekExpect(handDiff.Identical(), "Monday resolved by the engine equals Monday driven by hand");

    // The receipt names what happened.
    const std::string receipt = RenderWeekReceipt(compressed);
    WeekExpect(receipt.find("resolved 76 entries, 2042-06-02 06:00 to 2042-06-09 06:00 (10080 minutes)") != std::string::npos &&
                   receipt.find("completed the whole plan") != std::string::npos && receipt.find("decided_by_order: 0") != std::string::npos,
               "the weekly receipt states the span and that nothing was decided for the player");
    WeekExpect(receipt == RenderWeekReceipt(CompressWeek(world, plan, WeekCats())), "the receipt is deterministic");
}

/** With the real 2042-43 calendar in force the same week is still equivalent. */
void TestScriptedWeekWithCalendar()
{
    const SchoolCalendar* calendar = CalendarSet.Find("institution.nabapara_vidyalaya", "stage.higher_secondary");
    WeekExpect(calendar != nullptr, "the Nabapara Class XI calendar exists");
    DayLoopCatalogues withCalendar = Catalogues;
    withCalendar.Calendar = calendar;
    const WorldState world = WeekMorningWorld();
    const WeekPlan plan = WeekScriptedSchoolWeek();
    const WeekResult compressed = CompressWeek(world, plan, withCalendar);
    const WeekResult stepped = PlayWeekStepByStep(world, plan, withCalendar);
    WeekExpect(compressed.Ok() && stepped.Ok(), "the first week of term resolves under the calendar");
    WeekExpectEquivalent(compressed, stepped, "equivalent under the calendar too");
}
} // namespace

const DayLoopCatalogues& WeekCats()
{
    return Catalogues;
}

std::vector<WeekEntry> WeekSchoolDay()
{
    return {WeekActivity(360, "activity.eat_meal", "dining"),
            WeekSpend(560, 30, DayPassKind::Walk),
            WeekSchool(590),
            WeekActivity(620, "activity.attend_class", "classroom"),
            WeekActivity(670, "activity.attend_class", "classroom"),
            WeekActivity(740, "activity.attend_class", "classroom"),
            WeekSpend(960, 30, DayPassKind::Walk),
            WeekActivity(990, "activity.study_at_home", "home"),
            WeekActivity(1110, "activity.household_chores", "home"),
            WeekActivity(1170, "activity.eat_meal", "dining"),
            WeekSpend(1190, 130, DayPassKind::Rest),
            WeekSleep(1320, 480)};
}

std::vector<WeekEntry> WeekHomeDay()
{
    return {WeekActivity(360, "activity.eat_meal", "dining"),
            WeekActivity(480, "activity.study_at_home", "home"),
            WeekActivity(660, "activity.exercise_run", "outdoors"),
            WeekActivity(720, "activity.household_chores", "home"),
            WeekActivity(900, "activity.eat_meal", "dining"),
            WeekActivity(1000, "activity.study_at_home", "home"),
            WeekSpend(1190, 130, DayPassKind::Rest),
            WeekSleep(1320, 480)};
}

WeekPlan WeekScriptedSchoolWeek()
{
    WeekPlan plan;
    plan.Seed = 0x2042'0602ULL;
    for (int day = 0; day < 7; ++day)
        plan.Days.push_back({day < 5 ? WeekSchoolDay() : WeekHomeDay()});
    return plan;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: PerfectWorldWeekTests <path to Data/Content> [path to pwstatediff]\n";
        return 2;
    }
    const std::vector<ContentFile> files = ReadWeekContent(argv[1]);
    static const ContentLoadResult content = LoadContent(files);
    WeekExpect(content.Ok(), "the real content validates");
    static const ActivityCatalogueResult activities = LoadActivityCatalogue(content.Db);
    WeekExpect(activities.Ok(), "the activity catalogue loads");
    ActivityCat = activities.Catalogue;
    static const BellScheduleResult bells = LoadBellSchedules(content.Db);
    WeekExpect(bells.Ok(), "the bell schedules load");
    BellSet = bells.Set;
    const BellSchedule* science = BellSet.FindById("bell_schedule.nabapara_xi_science");
    WeekExpect(science != nullptr, "the science stream's day exists");
    Catalogues.Activities = &ActivityCat;
    Catalogues.School = science;
    static const SchoolCalendarResult calendars = LoadSchoolCalendars(content.Db);
    WeekExpect(calendars.Ok(), "the school calendars load");
    CalendarSet = calendars.Set;

    TestScriptedWeek();
    TestScriptedWeekWithCalendar();
    TestWeekRefusals();
    TestWeekRules();
    TestWeekRandom();
    TestStateDiff();
    TestStateDiffMutations();
    TestStateDiffSaves(argc > 2 ? argv[2] : nullptr);
    std::cout << "PerfectWorld week compression and state diff: " << WeekChecks << " checks passed\n";
    return 0;
}
