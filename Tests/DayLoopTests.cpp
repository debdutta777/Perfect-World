#include "DayLoopTestSupport.h"

#include "ContentRegistry.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <tuple>

using namespace PerfectWorld;

int DayLoopChecks = 0;

namespace
{
DayLoopCatalogues Catalogues;
ActivityCatalogue ActivityCat;
BellScheduleSet BellSet;

std::vector<ContentFile> ReadContentDirectory(const std::filesystem::path& root)
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

std::int64_t SumCredits(const std::vector<SkillCredit>& credits)
{
    std::int64_t total = 0;
    for (const SkillCredit& credit : credits)
        total += credit.MilliXp;
    return total;
}

// The expected clock and phase after each of the 29 commands (instants are minutes from 06:00 on Monday).
struct Step
{
    GameInstant Now;
    DayPhase Phase;
};
constexpr Step Expected[29] = {
    {0, DayPhase::FreeBlock},   {0, DayPhase::FreeBlock},   {0, DayPhase::FreeBlock},   {20, DayPhase::FreeBlock},
    {190, DayPhase::FreeBlock}, {230, DayPhase::FreeBlock}, {230, DayPhase::School},    {260, DayPhase::School},
    {260, DayPhase::School},    {260, DayPhase::School},    {305, DayPhase::School},    {310, DayPhase::School},
    {310, DayPhase::School},    {310, DayPhase::School},    {355, DayPhase::School},    {590, DayPhase::FreeBlock},
    {630, DayPhase::FreeBlock}, {630, DayPhase::FreeBlock}, {630, DayPhase::FreeBlock}, {750, DayPhase::Evening},
    {750, DayPhase::Evening},   {750, DayPhase::Evening},   {810, DayPhase::Evening},   {810, DayPhase::Evening},
    {810, DayPhase::Evening},   {830, DayPhase::Evening},   {960, DayPhase::Evening},   {960, DayPhase::Sleep},
    {1440, DayPhase::Plan}};

void TestScriptedDay()
{
    const std::vector<DayCommand> script = ScriptedDay();
    Check(script.size() == 29, "the scripted day has 29 commands");
    DayLoopState state = MorningState();
    Check(IsDayLoopStateValid(state) && state.Phase == DayPhase::Plan && state.Seq == 0, "a new life is awake and unplanned");

    std::vector<DayLoopEvent> completed;
    DayLoopState beforeSleep;
    DayLoopResult last;
    for (std::size_t i = 0; i < script.size(); ++i)
    {
        // The forecast shown before the player commits equals what is credited when the activity completes.
        DayActivityForecast forecast;
        if (script[i].Kind == DayCommandKind::Start)
            forecast = ForecastDayActivity(state, ActivityCat, state.Commitment.ActivityId, state.Now);
        last = MustApply(state, script[i], "each scripted command is accepted");
        Check(state.Seq == i + 1, "the sequence number is dense");
        Check(state.Now == Expected[i].Now, "the clock is where the script puts it");
        Check(state.Phase == Expected[i].Phase, "the phase follows the clock and the school day");
        Check(IsDayLoopStateValid(state), "every accepted command leaves a valid state");
        for (const DayLoopEvent& event : last.Events)
        {
            if (event.Kind == DayLoopEventKind::Completed)
                completed.push_back(event);
        }
        if (script[i].Kind == DayCommandKind::Start)
        {
            const DayLoopSlotSummary summary = SummariseDayLoop(state);
            Check(summary.HasCommitment && summary.CommitmentStarted && summary.LastKind == DayCommandKind::Start &&
                      summary.LastActivityId == state.Commitment.ActivityId,
                  "the slot summary shows the running commitment and the last action");
            Check(forecast.Ok, "the forecast is available before the commit");
            // Checked against the next Complete below.
            const DayLoopSlotSummary again = SummariseDayLoop(state);
            Check(again == summary, "the summary is a pure function of the state");
            DayLoopState trial = state;
            const DayLoopResult done = MustApply(trial, SimpleCmd(DayCommandKind::Complete), "the activity completes");
            Check(SumCredits(done.Events.front().Credited) == forecast.Receipt.LearningMilliXp,
                  "the forecast equals the credit");
        }
        if (i == 26)
            beforeSleep = state;
    }

    // The end of the day.
    Check(last.CheckpointDue && last.CheckpointIndex == 0, "waking reports a checkpoint due, the first of the three");
    Check(last.Events.size() == 2 && last.Events[0].Kind == DayLoopEventKind::Woke &&
              last.Events[1].Kind == DayLoopEventKind::CheckpointDue,
          "waking emits Woke then CheckpointDue");
    Check(state.Wakes == 1 && state.Seq == 29 && !state.Asleep && !state.Planned && state.Plan.empty(),
          "the next morning is awake, unplanned and counted");
    Check(state.Condition.Energy == 100000 && state.Condition.Hunger == 19600 && state.Condition.Health == 100000 &&
              state.Condition.Stress == 0,
          "eight hours of sleep restore the energy; hunger and stress are as the rates say");
    Check(state.Last == DayLastAction{DayCommandKind::Wake, 1440, ""}, "the last action is the wake");
    Check(state.Runner.Instance == 6 && state.Runner.AppliedInstance == 6 && !state.Runner.Active(),
          "six activities ran and each applied once");

    const ConditionState atBed = beforeSleep.Condition;
    Check(atBed.Energy == 29100 && atBed.Hunger == 5200 && atBed.Health == 100000 && atBed.Stress == 4730,
          "at 22:00 the condition is the sum of the day's rates (energy 29100, hunger 5200)");
    Check(beforeSleep.Phase == DayPhase::Evening && beforeSleep.Now == 960, "22:00 is the evening");

    // Attendance: on time, all six periods.
    const AttendanceRecord& attendance = state.Attendance;
    Check(attendance.SchoolDays == 1 && attendance.OnTimeDays == 1 && attendance.LateDays == 0 &&
              attendance.PeriodsHeld == 6 && attendance.PeriodsAttended == 6 && attendance.Days.size() == 1,
          "the walk beat the bell: one on-time day, six of six periods");
    Check(attendance.Days[0].ArrivalMinute == 590 && state.SchoolUntil == 590, "arrived 09:50; dismissal 15:50");

    // The practice window: two attended periods in one family, the second at half.
    Check(completed.size() == 6, "six activities completed");
    Check(SumCredits(completed[1].Credited) == 1500 && completed[1].Label == PracticeLabel::UsefulPractice,
          "the first period is useful practice (1200 academics, 300 communication)");
    Check(SumCredits(completed[2].Credited) == 750 && completed[2].Label == PracticeLabel::Consolidation,
          "the second period in the window is consolidation at half (600 and 150)");
    Check(completed[2].Credited.size() == 2 && completed[2].Credited[0].SkillId == "skill.academics" &&
              completed[2].Credited[0].MilliXp == 600 && completed[2].Credited[1].MilliXp == 150,
          "the half credit is split as the allowance is");
    Check(SumCredits(completed[3].Credited) == 3000 && completed[3].Outcome == "studied", "the study block is worth 3000 milli");
    Check(SumCredits(completed[4].Credited) == 1000 && SumCredits(completed[0].Credited) == 0 && SumCredits(completed[5].Credited) == 0,
          "chores teach 1000; the two meals teach nothing");
    Check(PracticeBudgetRemaining(beforeSleep.Practice, "skill.academics", 830) == 3800,
          "academics has spent 4200 of its 8000 daily budget");
    Check(beforeSleep.Practice.Families.size() == 3 && beforeSleep.Practice.Families[0].FamilyId == "family.attended_class" &&
              beforeSleep.Practice.Families[1].FamilyId == "family.chore_done" &&
              beforeSleep.Practice.Families[2].FamilyId == "family.studied",
          "the default practice families come from the outcomes");
    Check(!PracticeIsConsolidated(beforeSleep.Practice, "skill.academics", 830), "one day does not consolidate");

    // The receipt: six commitments; the day's totals equal the command log.
    const ReceiptDay day = ReceiptDayQuery(state.Receipts, ReceiptDayOf(0));
    Check(day.Entries.size() == 6 && day.Totals.Commitments == 6 && day.Totals.CommittedMinutes == 310,
          "six commitments, 310 committed minutes");
    std::int64_t loggedMinutes = 0;
    std::int64_t loggedLearning = 0;
    for (const DayLoopEvent& event : completed)
    {
        loggedMinutes += event.Minutes;
        loggedLearning += SumCredits(event.Credited);
    }
    Check(day.Totals.CommittedMinutes == loggedMinutes && day.Totals.LearningMilliXp == loggedLearning &&
              loggedLearning == 6250,
          "the receipt's totals equal the sum over the command log");
    Check(day.Totals.ConditionCostMilli == 49400, "condition cost is energy lost plus hunger gained over the minutes");
    Check(day.Totals.Displaced == 2 && day.Totals.DisplacedOverlapMinutes == 110 && day.Totals.DisplacedLearningMilliXp == 800,
          "two shown alternatives were displaced (20 and 90 minutes, 800 milli of learning)");
    Check(day.Entries[0].Displaced.size() == 1 && day.Entries[0].Displaced[0].Id == "alt.early_walk" &&
              day.Entries[0].Displaced[0].OverlapMinutes == 20,
          "the first meal displaced the early walk; the unknown, unavailable and back-to-back offers were not listed");
    Check(day.Entries[4].Displaced.empty() && day.Entries[3].Displaced.size() == 1 && day.Entries[3].Displaced[0].Id == "alt.football",
          "the study block displaced the football game; the chores displaced nothing");
    Check(completed[0].ReceiptSeq == 1 && completed[5].ReceiptSeq == 6, "receipt entries are numbered in order");

    // Saving at any point and the next morning agree on the day's hash.
    Check(HashDayLoop(state) == HashDayLoop(ParseDayLoop(SerialiseDayLoop(state)).State), "the final hash survives a save");
}

DayLoopState PlannedState()
{
    DayLoopState state = MorningState();
    MustApply(state, PlanCmd({}), "an empty plan is a plan");
    return state;
}

void TestArrivals()
{
    // 10:03 is within the five-minute grace: late. 10:06 is very late and misses the assembly.
    for (const auto& [wait, walk, outcome] : {std::tuple<int, int, AttendanceOutcome>{150, 40, AttendanceOutcome::OnTime},
                                              {200, 43, AttendanceOutcome::Late}, {200, 46, AttendanceOutcome::VeryLate}})
    {
        DayLoopState state = PlannedState();
        MustApply(state, PassCmd(wait, DayPassKind::Wait), "wait");
        MustApply(state, PassCmd(walk, DayPassKind::Walk), "walk to school");
        const DayLoopResult result = MustApply(state, SimpleCmd(DayCommandKind::ArriveSchool), "arrive");
        Check(result.Events.front().Attendance == outcome, "the arrival is classified by the bell");
        Check(state.Phase == DayPhase::School, "an attended day is the school phase");
        Check(state.Attendance.LateDays + state.Attendance.VeryLateDays == (outcome == AttendanceOutcome::OnTime ? 0 : 1),
              "a late arrival is a late mark");
    }

    // Arriving after dismissal attends nothing: absent, and the day is not a school phase.
    DayLoopState late = PlannedState();
    MustApply(late, PassCmd(620, DayPassKind::Wait), "wait until 16:20");
    const DayLoopResult absent = MustApply(late, SimpleCmd(DayCommandKind::ArriveSchool), "arrive after dismissal");
    Check(absent.Events.front().Attendance == AttendanceOutcome::Absent && late.Phase == DayPhase::FreeBlock &&
              late.SchoolUntil == 0 && late.Attendance.AbsentDays == 1,
          "arriving after the last bell is an absence");

    // Leaving early ends the school phase at the departure minute.
    DayLoopState early = PlannedState();
    MustApply(early, PassCmd(200, DayPassKind::Wait), "wait");
    MustApply(early, PassCmd(30, DayPassKind::Walk), "walk");
    DayCommand leave = SimpleCmd(DayCommandKind::ArriveSchool);
    leave.DepartureMinute = 720;
    MustApply(early, leave, "arrive and leave at noon");
    Check(early.SchoolUntil == 720 - 360 && early.Phase == DayPhase::School, "the school phase ends at the departure");
    MustApply(early, PassCmd(130, DayPassKind::Wait), "wait past the departure");
    Check(early.Phase == DayPhase::FreeBlock && early.Attendance.PeriodsAttended == 2,
          "after the departure the free block opens; only the two periods that ended before noon were attended");

    // Skipping school: the next morning's wake closes the day as absent, and nothing was lost.
    DayLoopState skipped = PlannedState();
    MustApply(skipped, PassCmd(960, DayPassKind::Wait), "the whole day passes");
    MustApply(skipped, SimpleCmd(DayCommandKind::GoToSleep), "sleep");
    MustApply(skipped, SimpleCmd(DayCommandKind::Wake, 480), "wake");
    Check(skipped.Attendance.AbsentDays == 1 && skipped.Attendance.SchoolDays == 1 && skipped.Attendance.PeriodsAttended == 0,
          "waking closes the unrecorded school day as absent");
}

void TestOtherRules()
{
    // Two half-hour passes equal one hour (additivity through Condition).
    DayLoopState split = PlannedState();
    DayLoopState whole = PlannedState();
    MustApply(split, PassCmd(30, DayPassKind::Walk), "first half hour");
    MustApply(split, PassCmd(30, DayPassKind::Walk), "second half hour");
    MustApply(whole, PassCmd(60, DayPassKind::Walk), "one hour");
    Check(split.Condition == whole.Condition && split.Now == whole.Now, "an hour passes the same in one step or two");

    // Cancelling a running activity charges the elapsed minutes and credits and records nothing.
    DayLoopState cancel = PlannedState();
    MustApply(cancel, CommitCmd("activity.study_at_home", "home"), "commit");
    MustApply(cancel, SimpleCmd(DayCommandKind::Start), "start");
    const DayLoopResult cancelled = MustApply(cancel, SimpleCmd(DayCommandKind::Cancel, 30), "cancel after half an hour");
    Check(cancelled.Events.front().Kind == DayLoopEventKind::Cancelled && cancelled.Events.front().Minutes == 30 &&
              cancel.Now == 30 && !cancel.HasCommitment && !cancel.Runner.Active() && cancel.Runner.AppliedInstance == 1,
          "cancel moves the clock by the elapsed minutes and frees the runner");
    Check(cancel.Practice.Families.empty() && cancel.Receipts.Entries.empty() && cancel.Condition.Energy == 100000 - 30 * 80,
          "an unfinished session teaches nothing and has no receipt; its minutes are charged");

    // Dropping a commitment that has not started costs nothing.
    DayLoopState drop = PlannedState();
    MustApply(drop, CommitCmd("activity.rest", "garden"), "commit to rest anywhere");
    const ConditionState before = drop.Condition;
    MustApply(drop, SimpleCmd(DayCommandKind::Cancel), "change one's mind");
    Check(drop.Condition == before && drop.Now == 0 && !drop.HasCommitment, "cancelling before the start is free");

    // The practice window through the loop: a fourth study session within 24 hours teaches nothing.
    DayLoopState grind = PlannedState();
    std::int64_t credited[4] = {};
    for (int i = 0; i < 4; ++i)
    {
        MustApply(grind, CommitCmd("activity.study_at_home", "home"), "commit");
        MustApply(grind, SimpleCmd(DayCommandKind::Start), "start");
        const DayLoopResult done = MustApply(grind, SimpleCmd(DayCommandKind::Complete), "complete");
        credited[i] = SumCredits(done.Events.front().Credited);
        if (i == 3)
            Check(done.Events.front().Label == PracticeLabel::LittleNewLearning, "the fourth session is little new learning");
    }
    Check(credited[0] == 3000 && credited[1] == 1500 && credited[2] == 300 && credited[3] == 0,
          "credit falls 3000, 1500, 300, 0 within 24 hours (full, half, tenth, nothing)");
    Check(grind.Receipts.Entries.size() == 4, "every session is on the receipt, whatever it earned");

    // After waking, a new plan is required; the previous plan is gone.
    DayLoopState next = PlannedState();
    MustApply(next, SimpleCmd(DayCommandKind::GoToSleep), "sleep");
    MustApply(next, SimpleCmd(DayCommandKind::Wake, 600), "wake");
    const DayLoopResult refused = ApplyDayCommand(next, DayLoopCats(), PassCmd(10, DayPassKind::Wait));
    Check(refused.Error == DayLoopError::PlanFirst && next.Phase == DayPhase::Plan, "a new morning needs a new plan");
}
} // namespace

const DayLoopCatalogues& DayLoopCats()
{
    return Catalogues;
}

std::vector<DayCommand> ScriptedDay(std::int64_t dayIndex)
{
    const GameInstant base = dayIndex * MinutesPerDay;
    const DayCommand start = SimpleCmd(DayCommandKind::Start);
    const DayCommand complete = SimpleCmd(DayCommandKind::Complete);
    return {
        PlanCmd({{360, "activity.eat_meal"}, {620, "activity.attend_class"}, {670, "activity.attend_class"},
                 {990, "activity.study_at_home"}, {1110, "activity.household_chores"}, {1190, "activity.eat_meal"}}),
        CommitCmd("activity.eat_meal", "dining",
                  {Candidate("alt.early_walk", base, base, 30, true, 0), Candidate("alt.future_offer", base + 100, base, 60, true, 500),
                   Candidate("alt.closed", base, base, 60, false, 900)}),
        start, complete,
        PassCmd(170, DayPassKind::Wait), PassCmd(40, DayPassKind::Walk), SimpleCmd(DayCommandKind::ArriveSchool),
        PassCmd(30, DayPassKind::Wait),
        CommitCmd("activity.attend_class", "classroom"), start, complete,
        PassCmd(5, DayPassKind::Wait),
        CommitCmd("activity.attend_class", "classroom"), start, complete,
        PassCmd(235, DayPassKind::Wait), PassCmd(40, DayPassKind::Walk),
        CommitCmd("activity.study_at_home", "home", {Candidate("alt.football", base + 300, base + 630, 90, true, 800)}),
        start, complete,
        CommitCmd("activity.household_chores", "home", {Candidate("alt.library", base + 300, base + 810, 60, true, 400)}),
        start, complete,
        CommitCmd("activity.eat_meal", "dining"), start, complete,
        PassCmd(130, DayPassKind::Rest), SimpleCmd(DayCommandKind::GoToSleep), SimpleCmd(DayCommandKind::Wake, 480)};
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: PerfectWorldDayLoopTests <path to Data/Content>\n";
        return 2;
    }
    const std::vector<ContentFile> files = ReadContentDirectory(argv[1]);
    static const ContentLoadResult content = LoadContent(files);
    Check(content.Ok(), "the real content validates");
    static const ActivityCatalogueResult activities = LoadActivityCatalogue(content.Db);
    Check(activities.Ok(), "the activity catalogue loads");
    ActivityCat = activities.Catalogue;
    static const BellScheduleResult bells = LoadBellSchedules(content.Db);
    Check(bells.Ok(), "the bell schedules load");
    BellSet = bells.Set;
    const BellSchedule* science = BellSet.FindById("bell_schedule.nabapara_xi_science");
    Check(science != nullptr, "the science stream's day exists");
    Catalogues.Activities = &ActivityCat;
    Catalogues.School = science;

    TestScriptedDay();
    TestArrivals();
    TestOtherRules();
    TestDayLoopRefusals();
    TestDayLoopSave();
    TestDayLoopRandom();
    std::cout << "PerfectWorld day loop: " << DayLoopChecks << " checks passed\n";
    return 0;
}
