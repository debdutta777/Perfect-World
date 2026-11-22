// ROADMAP M3-02: two hundred random plans, each compressed and played step by step, must agree on
// every quantity the equivalence rule names; the rules compression keeps hold on every one of them.
#include "WeekTestSupport.h"

using namespace PerfectWorld;

namespace
{
struct WeekRandomKind
{
    const char* Id;
    const char* Place;
};

constexpr WeekRandomKind WeekRandomKinds[] = {
    {"activity.eat_meal", "dining"},         {"activity.attend_class", "classroom"}, {"activity.study_at_home", "home"},
    {"activity.household_chores", "home"},   {"activity.exercise_run", "outdoors"},  {"activity.rest", "any"},
    {"activity.sleep_night", "home"},        {"activity.attend_class", "classroom"}, {"activity.study_at_home", "home"}};

constexpr const char* WeekRandomOrders[] = {"order.study_after_school", "order.walk_with_friend", "order.sleep_at_ten",
                                            "order.same_as_yesterday"};

/** A rested or tired player some time into the first week: a random start the plan must fit. */
WorldState WeekRandomStart(WeekRng& rng)
{
    WorldState world = WeekMorningWorld(rng.Next());
    world.DayLoop.Condition.Energy = static_cast<std::int32_t>(rng.Pick(20000, 100000));
    world.DayLoop.Condition.Hunger = static_cast<std::int32_t>(rng.Pick(0, 70000));
    world.DayLoop.Condition.Stress = static_cast<std::int32_t>(rng.Pick(0, 50000));
    const std::int64_t wait = rng.Chance(500) ? rng.Pick(0, 4000) : 0;
    if (wait > 0)
    {
        WorldSim sim(world);
        WeekExpect(sim.Submit({CommandSource::Player, AdvanceTime{wait}}).Accepted, "the random start moves on");
        world = sim.State();
    }
    return world;
}

WeekEntry WeekRandomEntry(WeekRng& rng, std::int32_t& activities, bool schoolDay, bool& schooled)
{
    const std::int64_t roll = rng.Pick(0, 99);
    if (roll < 50 && activities < 20)
    {
        const WeekRandomKind& kind = WeekRandomKinds[rng.Pick(0, 8)];
        ++activities;
        return WeekActivity(0, kind.Id, kind.Place);
    }
    if (roll < 62 && schoolDay && !schooled)
    {
        schooled = true;
        WeekEntry school = WeekSchool(0);
        if (rng.Chance(100))
            school.DepartureMinute = static_cast<std::int32_t>(rng.Pick(600, 960));
        return school;
    }
    if (roll < 70)
        return WeekSleep(0, static_cast<std::int32_t>(rng.Pick(30, 720)));
    return WeekSpend(0, static_cast<std::int32_t>(rng.Pick(1, 150)), static_cast<DayPassKind>(rng.Pick(0, 2)));
}

void WeekRandomFlags(WeekRng& rng, WeekEntry& entry)
{
    if (rng.Chance(20))
        entry.FirstOfKind = true;
    if (rng.Chance(30))
        entry.NeedsChoice = true;
    else if (rng.Chance(80))
    {
        entry.DecidedByOrder = true;
        entry.OrderId = WeekRandomOrders[rng.Pick(0, 3)];
    }
}

/** A sound plan for the world: entries in time order inside up to seven days, school only on weekdays and once a day. */
WeekPlan WeekRandomPlan(WeekRng& rng, const WorldState& world)
{
    WeekPlan plan;
    plan.Seed = rng.Next();
    plan.InterruptsUsed = rng.Chance(150) ? 1 : 0;
    const GameInstant firstDayStart = GameClock::StartOfDay(world.DayLoop.Now);
    GameInstant cursor = world.DayLoop.Now;
    const std::int64_t days = rng.Pick(1, 7);
    for (std::int64_t d = 0; d < days; ++d)
    {
        const GameInstant dayStart = firstDayStart + d * MinutesPerDay;
        const Weekday weekday = GameClock::ToCalendar(dayStart + 720).DayOfWeek;
        const bool schoolDay = weekday != Weekday::Saturday && weekday != Weekday::Sunday;
        WeekDayPlan day;
        std::int32_t activities = 0;
        bool schooled = false;
        while (day.Entries.size() < 16)
        {
            cursor += rng.Pick(0, 150) + (rng.Chance(120) ? rng.Pick(0, 600) : 0);
            cursor = std::max(cursor, dayStart);
            if (cursor >= dayStart + MinutesPerDay)
                break;
            WeekEntry entry = WeekRandomEntry(rng, activities, schoolDay, schooled);
            entry.StartMinute = static_cast<std::int32_t>(cursor - dayStart);
            WeekRandomFlags(rng, entry);
            cursor += entry.Kind == WeekEntryKind::Activity ? WeekCats().Activities->Find(entry.ActivityId)->DurationMinutes
                      : entry.Kind == WeekEntryKind::School ? 0
                                                             : entry.Minutes;
            day.Entries.push_back(std::move(entry));
        }
        plan.Days.push_back(std::move(day));
    }
    return plan;
}

const WeekEntry& WeekEntryAtFlat(const WeekPlan& plan, std::int32_t flat)
{
    for (const WeekDayPlan& day : plan.Days)
    {
        if (flat < static_cast<std::int32_t>(day.Entries.size()))
            return day.Entries[static_cast<std::size_t>(flat)];
        flat -= static_cast<std::int32_t>(day.Entries.size());
    }
    return plan.Days.front().Entries.front();
}

std::int32_t WeekTotalEntries(const WeekPlan& plan)
{
    std::int32_t total = 0;
    for (const WeekDayPlan& day : plan.Days)
        total += static_cast<std::int32_t>(day.Entries.size());
    return total;
}

struct WeekRandomTally
{
    int Whole = 0;
    int Choice = 0;
    int First = 0;
    int Budget = 0;
    int Disclosed = 0;
    int Empty = 0;
};

void WeekCheckRandomRules(const WeekPlan& plan, const WeekResult& compressed, WeekRandomTally& tally)
{
    // Firsts never auto-resolve; a consequential choice no order covers is never resolved for the player.
    for (std::int32_t flat = 0; flat < compressed.EntriesResolved; ++flat)
    {
        const WeekEntry& entry = WeekEntryAtFlat(plan, flat);
        WeekExpect(!entry.FirstOfKind && (!entry.NeedsChoice || entry.DecidedByOrder), "no resolved entry was a first or an open choice");
    }
    WeekExpect(compressed.InterruptsUsed >= plan.InterruptsUsed && compressed.InterruptsUsed <= WeekMaxInterrupts,
               "a week never has more than one interrupt");
    std::int32_t ordered = 0;
    for (std::int32_t flat = 0; flat < compressed.EntriesResolved; ++flat)
        ordered += WeekEntryAtFlat(plan, flat).DecidedByOrder ? 1 : 0;
    WeekExpect(ordered == compressed.Ledger.DecidedByOrder && ordered == static_cast<std::int32_t>(compressed.Disclosures.size()),
               "every choice made on the player's behalf is counted and disclosed");
    const std::string receipt = RenderWeekReceipt(compressed);
    for (const WeekDisclosure& disclosure : compressed.Disclosures)
        WeekExpect(receipt.find(disclosure.OrderId) != std::string::npos, "the receipt names each order");
    tally.Disclosed += ordered > 0 ? 1 : 0;

    switch (compressed.Stop)
    {
    case WeekStop::None:
        WeekExpect(compressed.EntriesResolved == WeekTotalEntries(plan), "an unstopped week resolved every entry");
        ++tally.Whole;
        break;
    case WeekStop::FirstNeedsPlay:
        WeekExpect(WeekEntryAtFlat(plan, compressed.EntriesResolved).FirstOfKind && compressed.InterruptsUsed == plan.InterruptsUsed,
                   "a stop for a first is at a first and does not use the interrupt");
        ++tally.First;
        break;
    case WeekStop::ChoiceNeeded:
        WeekExpect(WeekEntryAtFlat(plan, compressed.EntriesResolved).NeedsChoice && plan.InterruptsUsed == 0 &&
                       compressed.InterruptsUsed == 1,
                   "the one interrupt is an open choice");
        ++tally.Choice;
        break;
    case WeekStop::InterruptBudgetSpent:
        WeekExpect(WeekEntryAtFlat(plan, compressed.EntriesResolved).NeedsChoice && plan.InterruptsUsed == 1 &&
                       compressed.InterruptsUsed == 1,
                   "a spent budget is not spent twice");
        ++tally.Budget;
        break;
    }
    tally.Empty += compressed.EntriesResolved == 0 ? 1 : 0;
}

/** The week as the time contract sees it costs what the day loop charged, at every granularity. */
void WeekCheckRandomTimeContract(const WorldState& world, const WeekPlan& plan, const WeekResult& compressed, const WeekResult& stepped)
{
    const WeekTimePlanResult prefix = BuildWeekTimePlan(world, plan, WeekCats(), compressed.EntriesResolved);
    WeekExpect(prefix.Ok(), "the resolved part of a random week is a sound time plan");
    if (prefix.Plan.SpanMinutes < 1)
        return;
    const TimePlanResult scenes = ResolvePlan(prefix.Plan, TimeGranularity::Scenes);
    if (scenes.Ok() && !CompareWeekToTimeLedger(stepped, scenes.Ledger).empty())
        std::cerr << "ledger differs on " << CompareWeekToTimeLedger(stepped, scenes.Ledger) << " span " << prefix.Plan.SpanMinutes
                  << " items " << prefix.Plan.Items.size() << " completed " << stepped.Ledger.Completed << " vs "
                  << scenes.Ledger.Completed << " total " << stepped.Ledger.TotalCharged << " vs " << scenes.Ledger.TotalCharged
                  << " order " << stepped.Ledger.DecidedByOrder << " vs " << scenes.Ledger.DecidedByOrder << '\n';
    WeekExpect(scenes.Ok() && CompareWeekToTimeLedger(stepped, scenes.Ledger).empty(), "the played week costs what the contract says");
    for (const TimeGranularity granularity : {TimeGranularity::Day, TimeGranularity::Week, TimeGranularity::Span})
    {
        const TimePlanResult coarse = ResolvePlan(prefix.Plan, granularity);
        WeekExpect(coarse.Ok() && CompareWeekToTimeLedger(compressed, coarse.Ledger).empty(),
                   "the compressed week costs what the contract says at every granularity");
    }
    WeekExpect(CompareGranularities(prefix.Plan).Equivalent(), "the contract's own equivalence holds for the resolved part");
}
} // namespace

void TestWeekRandom()
{
    WeekRandomTally tally;
    for (int i = 0; i < 200; ++i)
    {
        WeekRng rng{0x9E3779B97F4A7C15ULL * static_cast<std::uint64_t>(i + 1)};
        const WorldState world = WeekRandomStart(rng);
        const WeekPlan plan = WeekRandomPlan(rng, world);
        const WeekPlanIssue issue = ValidateWeekPlan(world, plan, WeekCats());
        if (!issue.Ok())
            std::cerr << "plan " << i << ": " << WeekErrorCode(issue.Error) << " day " << issue.Day << " entry " << issue.Entry << ' '
                      << issue.Detail << '\n';
        WeekExpect(issue.Ok(), "a random plan is sound");

        const WeekResult compressed = CompressWeek(world, plan, WeekCats());
        if (!compressed.Ok())
            std::cerr << "plan " << i << " refused: " << WeekErrorCode(compressed.Issue.Error) << " day " << compressed.Issue.Day
                      << " entry " << compressed.Issue.Entry << ' ' << compressed.Issue.Detail << '\n';
        WeekExpect(compressed.Ok(), "a sound random plan compresses");
        WeekCheckRandomRules(plan, compressed, tally);

        static constexpr std::int64_t chunks[] = {5, 15, 15, 30, 60, 240};
        WeekStepOptions options;
        options.ChunkMinutes = plan.Days.size() <= 2 && i % 3 == 0 ? 1 : chunks[rng.Pick(0, 5)];
        options.EntryLimit = compressed.EntriesResolved;
        const WeekResult stepped = PlayWeekStepByStep(world, plan, WeekCats(), options);
        WeekExpect(stepped.Ok() && stepped.EntriesResolved == compressed.EntriesResolved, "the reference resolves the same entries");
        if (!CompareWeekResults(compressed, stepped).Equivalent())
            std::cerr << "plan " << i << " chunk " << options.ChunkMinutes << '\n';
        WeekExpectEquivalent(compressed, stepped, "a random plan compressed equals the same plan step by step");
        WeekCheckRandomTimeContract(world, plan, compressed, stepped);

        if (i % 10 == 0)
        {
            const WeekResult again = CompressWeek(world, plan, WeekCats());
            WeekExpect(HashWorldState(again.World) == HashWorldState(compressed.World) && again.Ledger == compressed.Ledger &&
                           again.Disclosures == compressed.Disclosures,
                       "a random plan compresses to the same thing every time");
        }
    }
    std::cout << "  random plans: " << tally.Whole << " whole, " << tally.Choice << " stopped at a choice, " << tally.First
              << " at a first, " << tally.Budget << " at a spent budget, " << tally.Disclosed << " with decisions disclosed\n";
    WeekExpect(tally.Whole >= 10 && tally.Choice >= 10 && tally.First >= 10 && tally.Budget >= 5 && tally.Disclosed >= 30,
               "the random plans cover whole weeks, every kind of stop and decisions made for the player");
}
