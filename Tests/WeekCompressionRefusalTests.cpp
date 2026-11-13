// ROADMAP M3-02: a refused week changes nothing, whichever way it is resolved and wherever it is refused.
#include "WeekTestSupport.h"

#include <algorithm>

using namespace PerfectWorld;

namespace
{
WeekPlan WeekOneDay(std::vector<WeekEntry> entries)
{
    WeekPlan plan;
    plan.Days.push_back({std::move(entries)});
    return plan;
}

/** Both ways refuse alike with the expected error at the expected place, leave the world exactly as given, and report nothing resolved. */
void WeekExpectRefused(const WorldState& world, const WeekPlan& plan, const DayLoopCatalogues& cats, WeekError expected,
                       std::int32_t day, std::int32_t entry, const char* message)
{
    const std::uint64_t hashBefore = HashWorldState(world);
    const WeekResult compressed = CompressWeek(world, plan, cats);
    const WeekResult stepped = PlayWeekStepByStep(world, plan, cats);
    if (compressed.Issue.Error != expected)
        std::cerr << message << ": got " << WeekErrorCode(compressed.Issue.Error) << " (" << compressed.Issue.Detail << ")\n";
    WeekExpect(!compressed.Ok() && compressed.Issue.Error == expected, message);
    WeekExpect(compressed.Issue.Day == day && compressed.Issue.Entry == entry, "the refusal names the day and entry");
    WeekExpect(!stepped.Ok() && stepped.Issue.Error == expected && stepped.Issue.Day == day && stepped.Issue.Entry == entry,
               "the step-by-step reference refuses the same way");
    for (const WeekResult* result : {&compressed, &stepped})
    {
        WeekExpect(DiffWorldStates(world, result->World).Identical() && HashWorldState(result->World) == hashBefore,
                   "a refused week leaves the world exactly as it was given");
        WeekExpect(result->EntriesResolved == 0 && result->Outcomes.empty() && result->Disclosures.empty() &&
                       result->Ledger.TotalCharged == 0 && result->Stop == WeekStop::None && result->DayCommands == 0,
                   "a refused week resolves nothing");
    }
    WeekExpect(ValidateWeekPlan(world, plan, cats).Error == expected || expected == WeekError::DayCommandRefused,
               "validation reports the static refusals before anything runs");
    WeekExpect(CompareWeekResults(compressed, stepped).Equivalent(), "both refusals are the same outcome");
    const std::string receipt = RenderWeekReceipt(compressed);
    WeekExpect(receipt.find("refused: " + std::string(WeekErrorCode(expected))) != std::string::npos &&
                   receipt.find("nothing was changed") != std::string::npos,
               "the receipt says it was refused and nothing changed");
}

WorldState WeekAsleepWorld()
{
    WorldState world = WeekMorningWorld();
    DayCommand plan;
    plan.Kind = DayCommandKind::PlanDay;
    WeekExpect(ApplyDayCommand(world.DayLoop, WeekCats(), plan).Ok(), "the day is planned");
    DayCommand sleep;
    sleep.Kind = DayCommandKind::GoToSleep;
    WeekExpect(ApplyDayCommand(world.DayLoop, WeekCats(), sleep).Ok(), "the player goes to sleep");
    WeekExpect(IsWorldDayInStep(world) && world.DayLoop.Asleep, "an asleep world is still in step");
    return world;
}

WorldState WeekCommittedWorld(bool started)
{
    WorldState world = WeekMorningWorld();
    DayCommand plan;
    plan.Kind = DayCommandKind::PlanDay;
    WeekExpect(ApplyDayCommand(world.DayLoop, WeekCats(), plan).Ok(), "the day is planned");
    DayCommand commit;
    commit.Kind = DayCommandKind::Commit;
    commit.ActivityId = "activity.eat_meal";
    commit.PlaceKind = "dining";
    WeekExpect(ApplyDayCommand(world.DayLoop, WeekCats(), commit).Ok(), "a meal is committed to");
    if (started)
    {
        DayCommand start;
        start.Kind = DayCommandKind::Start;
        WeekExpect(ApplyDayCommand(world.DayLoop, WeekCats(), start).Ok(), "the meal starts");
    }
    WeekExpect(IsWorldDayInStep(world), "a committed world is in step");
    return world;
}

void TestEntryRefusals(const WorldState& world, const DayLoopCatalogues& cats)
{
    WeekExpectRefused(world, WeekPlan{}, cats, WeekError::NoDays, -1, -1, "a plan with no days is refused");
    WeekPlan eight;
    eight.Days.resize(8);
    WeekExpectRefused(world, eight, cats, WeekError::TooManyDays, -1, -1, "eight days is more than a week");

    std::vector<WeekEntry> crowded;
    for (std::int32_t i = 0; i < 49; ++i)
        crowded.push_back(WeekSpend(360 + i, 1, DayPassKind::Wait));
    WeekExpectRefused(world, WeekOneDay(crowded), cats, WeekError::TooManyEntries, 0, -1, "forty-nine entries in a day is too many");
    std::vector<WeekEntry> twentyFive;
    for (std::int32_t i = 0; i < 25; ++i)
        twentyFive.push_back(WeekActivity(360 + 20 * i, "activity.rest", "home"));
    WeekExpectRefused(world, WeekOneDay(twentyFive), cats, WeekError::TooManyEntries, 0, -1, "twenty-five activities exceed the day plan");

    WeekExpectRefused(world, WeekOneDay({WeekSpend(360, 10, DayPassKind::Wait), WeekActivity(400, "activity.nope", "home")}), cats,
                      WeekError::UnknownActivity, 0, 1, "an unknown activity is refused");
    WeekExpectRefused(world, WeekOneDay({WeekActivity(360, "activity.eat_meal", "classroom")}), cats, WeekError::WrongPlace, 0, 0,
                      "a meal is not eaten in a classroom");
    WeekExpectRefused(world, WeekOneDay({WeekSpend(1440, 10, DayPassKind::Wait)}), cats, WeekError::BadMinute, 0, 0,
                      "minute 1440 is not in the day");
    WeekExpectRefused(world, WeekOneDay({WeekSpend(-1, 10, DayPassKind::Wait)}), cats, WeekError::BadMinute, 0, 0,
                      "a negative minute is refused");
    WeekExpectRefused(world, WeekOneDay({WeekSleep(400, 0)}), cats, WeekError::BadEntry, 0, 0, "sleeping no time is refused");
    WeekExpectRefused(world, WeekOneDay({WeekSleep(400, 1441)}), cats, WeekError::BadEntry, 0, 0, "sleeping more than a day is refused");
    WeekExpectRefused(world, WeekOneDay({WeekSpend(400, 1441, DayPassKind::Rest)}), cats, WeekError::BadEntry, 0, 0,
                      "spending more than a day is refused");
    WeekEntry unknownPass = WeekSpend(400, 10, DayPassKind::Wait);
    unknownPass.Pass = static_cast<DayPassKind>(9);
    WeekExpectRefused(world, WeekOneDay({unknownPass}), cats, WeekError::BadEntry, 0, 0, "an unknown pass kind is refused");
    WeekEntry late = WeekSchool(590);
    late.DepartureMinute = 1441;
    WeekExpectRefused(world, WeekOneDay({late}), cats, WeekError::BadEntry, 0, 0, "a departure outside the day is refused");
    WeekExpectRefused(world, WeekOneDay({WeekActivity(360, "activity.eat_meal", "BAD PLACE")}), cats, WeekError::BadEntry, 0, 0,
                      "a place kind must be a token");
    WeekEntry shown = WeekActivity(360, "activity.eat_meal", "dining");
    shown.Alternatives.push_back(ReceiptCandidate{});
    WeekExpectRefused(world, WeekOneDay({shown}), cats, WeekError::BadEntry, 0, 0, "an invalid alternative is refused");

    WeekExpectRefused(world, WeekOneDay({WeekActivity(360, "activity.eat_meal", "dining"), WeekSpend(370, 10, DayPassKind::Wait)}), cats,
                      WeekError::Overlap, 0, 1, "an entry that starts before the meal ends overlaps it");
    WeekExpectRefused(world, WeekOneDay({WeekSpend(300, 10, DayPassKind::Wait)}), cats, WeekError::Overlap, 0, 0,
                      "an entry before the clock's now is refused");
    WeekPlan backwards;
    backwards.Days.push_back({{WeekSleep(1320, 480)}});
    backwards.Days.push_back({{WeekSpend(300, 10, DayPassKind::Wait)}});
    WeekExpectRefused(world, backwards, cats, WeekError::Overlap, 1, 0, "a night's sleep runs into the next day's first entry");

    WeekEntry both = WeekSpend(400, 10, DayPassKind::Wait);
    both.NeedsChoice = true;
    both.DecidedByOrder = true;
    both.OrderId = "order.x";
    WeekExpectRefused(world, WeekOneDay({both}), cats, WeekError::BadFlags, 0, 0, "a choice an order covers is not left open");
    WeekEntry noId = WeekSpend(400, 10, DayPassKind::Wait);
    noId.DecidedByOrder = true;
    WeekExpectRefused(world, WeekOneDay({noId}), cats, WeekError::BadFlags, 0, 0, "a standing order needs its id");
    WeekEntry strayId = WeekSpend(400, 10, DayPassKind::Wait);
    strayId.OrderId = "order.x";
    WeekExpectRefused(world, WeekOneDay({strayId}), cats, WeekError::BadFlags, 0, 0, "an order id without an order is refused");
    WeekEntry badId = noId;
    badId.OrderId = "Order X";
    WeekExpectRefused(world, WeekOneDay({badId}), cats, WeekError::BadFlags, 0, 0, "an order id must be a token");

    WeekPlan twice = WeekOneDay({WeekSpend(400, 10, DayPassKind::Wait)});
    twice.InterruptsUsed = 2;
    WeekExpectRefused(world, twice, cats, WeekError::BadInterruptCount, -1, -1, "a week has at most one interrupt");
    twice.InterruptsUsed = -1;
    WeekExpectRefused(world, twice, cats, WeekError::BadInterruptCount, -1, -1, "and not fewer than none");
}

void TestCatalogueRefusals(const WorldState& world)
{
    DayLoopCatalogues noSchool = WeekCats();
    noSchool.School = nullptr;
    WeekExpectRefused(world, WeekOneDay({WeekSchool(590)}), noSchool, WeekError::NoSchool, 0, 0, "school needs a bell schedule");
    DayLoopCatalogues noActivities = WeekCats();
    noActivities.Activities = nullptr;
    WeekExpectRefused(world, WeekOneDay({WeekSpend(400, 10, DayPassKind::Wait)}), noActivities, WeekError::MissingCatalogue, -1, -1,
                      "a week needs the activity catalogue");

    // The time contract: a mission or a compressed span is not a routine part of a week.
    for (const ActivityClockMode mode : {ActivityClockMode::Mission, ActivityClockMode::Compressed})
    {
        ActivityCatalogue edited = *WeekCats().Activities;
        for (ActivityDef& def : edited.Defs)
        {
            if (def.Id == "activity.study_at_home")
            {
                def.Clock = mode;
                def.Interrupt = ActivityInterrupt::None;
            }
        }
        DayLoopCatalogues cats = WeekCats();
        cats.Activities = &edited;
        WeekExpectRefused(world, WeekOneDay({WeekActivity(360, "activity.study_at_home", "home")}), cats, WeekError::NotRoutine, -1, -1,
                          "the time contract refuses a mission or compressed activity in a routine week");
        WeekExpect(CompressWeek(world, WeekOneDay({WeekActivity(360, "activity.study_at_home", "home")}), cats).Issue.Detail.rfind("time.", 0) == 0,
                   "and says which time-contract rule it broke");
    }
}

void TestWorldRefusals(const DayLoopCatalogues& cats)
{
    const WeekPlan simple = WeekOneDay({WeekSpend(480, 10, DayPassKind::Wait)});
    WeekExpectRefused(WeekAsleepWorld(), simple, cats, WeekError::WorldBusy, -1, -1, "a sleeping player has no week to plan");
    WeekExpectRefused(WeekCommittedWorld(false), simple, cats, WeekError::WorldBusy, -1, -1, "an open commitment must be finished first");
    WeekExpectRefused(WeekCommittedWorld(true), simple, cats, WeekError::WorldBusy, -1, -1, "a running activity must be finished first");

    WorldState outOfStep = WeekMorningWorld();
    outOfStep.DayLoop.Now = 5;
    WeekExpectRefused(outOfStep, simple, cats, WeekError::InvalidWorld, -1, -1, "a day that is not on the world clock is refused");

    // A life's calendar ends a hundred years in: a week that would pass it is refused whole.
    WorldState late = WeekMorningWorld();
    const GameInstant nearEnd = DayLoopMaxInstant - 100;
    for (GameInstant now = 0; now < nearEnd;)
    {
        const GameInstant target = std::min<GameInstant>(nearEnd, now + MaxSingleAdvanceMinutes);
        WeekExpect(!late.Clock.AdvanceTo(target).Rejected, "the clock reaches the end of the calendar");
        now = target;
    }
    late.DayLoop.Now = late.Clock.Now();
    late.DayLoop.Phase = DayLoopPhaseOf(late.DayLoop);
    if (IsWorldDayInStep(late))
        WeekExpectRefused(late, WeekOneDay({WeekSleep(static_cast<std::int32_t>((nearEnd + 360) % MinutesPerDay), 1440)}), cats,
                          WeekError::BeyondCalendar, 0, 0, "a week past the end of the calendar is refused");
}

/** Refused by the day loop mid-run, after earlier days were resolved: the whole week is undone. */
void TestRuntimeRefusals(const WorldState& world, const DayLoopCatalogues& cats)
{
    WeekPlan saturday;
    for (int day = 0; day < 5; ++day)
        saturday.Days.push_back({WeekSchoolDay()});
    saturday.Days.push_back({{WeekSchool(590)}});
    const WeekResult compressed = CompressWeek(world, saturday, cats);
    WeekExpect(ValidateWeekPlan(world, saturday, cats).Ok(), "school on Saturday is a sound plan; the calendar refuses it as it runs");
    WeekExpect(!compressed.Ok() && compressed.Issue.Error == WeekError::DayCommandRefused && compressed.Issue.Day == 5 &&
                   compressed.Issue.Entry == 0 && compressed.Issue.Detail.find("arrive_school: dayloop.attendance_refused") == 0,
               "the day loop refuses arriving at school on a Saturday, naming the day and entry");
    WeekExpectRefused(world, saturday, cats, WeekError::DayCommandRefused, 5, 0, "a week refused on its sixth day changes nothing");

    WeekPlan twice = WeekOneDay({WeekSchool(590), WeekSchool(600)});
    WeekExpectRefused(world, twice, cats, WeekError::DayCommandRefused, 0, 1, "arriving at school twice in a day is refused");

    // Nothing hidden is left behind: a corrected plan from the same world gives what a fresh world gives.
    WorldState fresh = WeekMorningWorld();
    const WeekPlan fixed = WeekOneDay({WeekSchool(590), WeekActivity(620, "activity.attend_class", "classroom")});
    const WeekResult once = CompressWeek(world, fixed, cats);
    const WeekResult again = CompressWeek(fresh, fixed, cats);
    WeekExpect(once.Ok() && again.Ok() && DiffWorldStates(once.World, again.World).Identical() && once.Ledger == again.Ledger,
               "a refusal leaves no trace for the next plan");
}
} // namespace

void TestWeekRefusals()
{
    const WorldState world = WeekMorningWorld();
    TestEntryRefusals(world, WeekCats());
    TestCatalogueRefusals(world);
    TestWorldRefusals(WeekCats());
    TestRuntimeRefusals(world, WeekCats());
}
