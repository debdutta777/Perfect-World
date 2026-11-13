// ROADMAP M3-02: the rules compression keeps. At most one interrupt, firsts never auto-resolve, every
// choice made on the player's behalf is disclosed, the week agrees with the time contract, and the
// outcome is deterministic and brings the world's scheduled events with it.
#include "WeekTestSupport.h"

#include "GateSites.h"

using namespace PerfectWorld;

namespace
{
constexpr std::int32_t SchoolDayEntries = 12;

WeekEntry& WeekAt(WeekPlan& plan, std::int32_t day, std::int32_t entry)
{
    return plan.Days[static_cast<std::size_t>(day)].Entries[static_cast<std::size_t>(entry)];
}

void WeekOrder(WeekEntry& entry, const char* orderId)
{
    entry.DecidedByOrder = true;
    entry.OrderId = orderId;
}

void TestInterrupts(const WorldState& world, const WeekPlan& base)
{
    // Three consequential choices and no standing order: compression stops before the first, which is the week's one interrupt.
    WeekPlan plan = base;
    WeekAt(plan, 2, 4).NeedsChoice = true;
    WeekAt(plan, 3, 1).NeedsChoice = true;
    WeekAt(plan, 4, 0).NeedsChoice = true;
    const WeekResult stopped = CompressWeek(world, plan, WeekCats());
    WeekExpect(stopped.Ok() && stopped.Stop == WeekStop::ChoiceNeeded && stopped.StopDay == 2 && stopped.StopEntry == 4,
               "compression stops before the first uncovered choice");
    WeekExpect(stopped.EntriesResolved == 2 * SchoolDayEntries + 4 && stopped.InterruptsUsed == 1,
               "the entries before it were resolved, and exactly one interrupt is used");
    WeekExpect(stopped.Ledger.EndInstant == stopped.World.Clock.Now() && stopped.World.Clock.Now() < 3 * MinutesPerDay,
               "the world stands where the last resolved entry ended, in the middle of Wednesday");

    WeekStepOptions upToIt;
    upToIt.EntryLimit = stopped.EntriesResolved;
    const WeekResult played = PlayWeekStepByStep(world, plan, WeekCats(), upToIt);
    WeekExpect(played.Ok() && played.Stop == WeekStop::None, "the player, present, plays those same entries");
    WeekExpectEquivalent(stopped, played, "what compression resolved before the stop equals playing it step by step");
    WeekExpect(PlayWeekStepByStep(world, plan, WeekCats()).EntriesResolved == 5 * SchoolDayEntries + 2 * 8,
               "the step-by-step reference never stops: the player makes the choices");
    const std::string receipt = RenderWeekReceipt(stopped);
    WeekExpect(receipt.find("stopped before day 2 entry 4: choice_needed (interrupts used 1 of 1)") != std::string::npos,
               "the receipt says where and why it stopped");

    // The player decides the first two with standing orders: they are disclosed, and compression runs on to the third.
    WeekPlan ordered = plan;
    WeekAt(ordered, 2, 4).NeedsChoice = false;
    WeekOrder(WeekAt(ordered, 2, 4), "order.study_after_school");
    WeekAt(ordered, 3, 1).NeedsChoice = false;
    WeekOrder(WeekAt(ordered, 3, 1), "order.walk_with_friend");
    const WeekResult second = CompressWeek(world, ordered, WeekCats());
    WeekExpect(second.Ok() && second.Stop == WeekStop::ChoiceNeeded && second.StopDay == 4 && second.StopEntry == 0 &&
                   second.EntriesResolved == 4 * SchoolDayEntries && second.InterruptsUsed == 1,
               "covered choices do not stop compression; the next uncovered one does");
    WeekExpect(second.Disclosures.size() == 2 && second.Ledger.DecidedByOrder == 2, "the two decisions made for the player are disclosed");

    // The week's interrupt already used: the next uncovered choice stops the day without a second interrupt.
    WeekPlan spent = plan;
    spent.InterruptsUsed = 1;
    const WeekResult budget = CompressWeek(world, spent, WeekCats());
    WeekExpect(budget.Ok() && budget.Stop == WeekStop::InterruptBudgetSpent && budget.StopDay == 2 && budget.StopEntry == 4 &&
                   budget.InterruptsUsed == 1,
               "a second interrupt is never taken: the day becomes a scene day");
    WeekExpect(budget.EntriesResolved == stopped.EntriesResolved && DiffWorldStates(budget.World, stopped.World).Identical(),
               "and it stops in the same place");
    WeekExpect(RenderWeekReceipt(budget).find("interrupt_budget_spent (interrupts used 1 of 1)") != std::string::npos,
               "the receipt names the spent budget");

    // The player takes the decision (no longer open): the whole week compresses and equals the reference.
    WeekPlan decided = plan;
    for (const std::pair<int, int> at : {std::pair<int, int>{2, 4}, {3, 1}, {4, 0}})
        WeekAt(decided, at.first, at.second).NeedsChoice = false;
    WeekExpectEquivalent(CompressWeek(world, decided, WeekCats()), PlayWeekStepByStep(world, decided, WeekCats()),
                         "once decided, the whole week compresses and equals the reference");

    // Stopping before the first entry resolves nothing and changes nothing, but is not a refusal.
    WeekPlan atOnce = base;
    WeekAt(atOnce, 0, 0).NeedsChoice = true;
    const WeekResult none = CompressWeek(world, atOnce, WeekCats());
    WeekExpect(none.Ok() && none.Stop == WeekStop::ChoiceNeeded && none.EntriesResolved == 0 && none.Outcomes.empty() &&
                   DiffWorldStates(world, none.World).Identical() && none.Ledger.TotalCharged == 0 && none.DayCommands == 0,
               "stopping before the first entry changes nothing");
}

void TestFirsts(const WorldState& world, const WeekPlan& base)
{
    WeekPlan plan = base;
    WeekAt(plan, 1, 3).FirstOfKind = true;
    WeekAt(plan, 1, 3).NeedsChoice = true; // a first outranks an open choice
    const WeekResult stopped = CompressWeek(world, plan, WeekCats());
    WeekExpect(stopped.Ok() && stopped.Stop == WeekStop::FirstNeedsPlay && stopped.StopDay == 1 && stopped.StopEntry == 3 &&
                   stopped.EntriesResolved == SchoolDayEntries + 3,
               "a first is never auto-resolved: compression stops before it");
    WeekExpect(stopped.InterruptsUsed == 0, "a first is a scene to play, not the week's interrupt");
    WeekStepOptions upToIt;
    upToIt.EntryLimit = stopped.EntriesResolved;
    WeekExpectEquivalent(stopped, PlayWeekStepByStep(world, plan, WeekCats(), upToIt), "up to the first, compression equals playing");
    const WeekResult played = PlayWeekStepByStep(world, plan, WeekCats());
    WeekExpect(played.Ok() && played.EntriesResolved == 5 * SchoolDayEntries + 16, "the player plays the first and the rest");

    // The time contract's own helper agrees: a plan holding a first is resolved as scenes only.
    const WeekTimePlanResult timePlan = BuildWeekTimePlan(world, plan, WeekCats());
    WeekExpect(timePlan.Ok(), "the week with a first is a sound time plan");
    WeekExpect(ResolvePlan(timePlan.Plan, TimeGranularity::Scenes).Ok(), "the contract resolves it as scenes");
    for (const TimeGranularity granularity : {TimeGranularity::Day, TimeGranularity::Week, TimeGranularity::Span})
        WeekExpect(ResolvePlan(timePlan.Plan, granularity).Error == TimeContractError::PlanNeedsPlay,
                   "and refuses to resolve a first at any coarser granularity");
    WeekExpect(CompareWeekToTimeLedger(played, ResolvePlan(timePlan.Plan, TimeGranularity::Scenes).Ledger).empty(),
               "the played week costs what the contract says");
}

void TestDisclosure(const WorldState& world, const WeekPlan& base)
{
    WeekPlan plan = base;
    WeekOrder(WeekAt(plan, 0, 1), "order.walk_to_school");   // a spend
    WeekOrder(WeekAt(plan, 0, 2), "order.attend_every_day"); // arriving at school
    WeekOrder(WeekAt(plan, 1, 7), "order.study_after_school"); // an activity
    WeekOrder(WeekAt(plan, 3, 11), "order.sleep_at_ten");    // sleep
    const WeekResult compressed = CompressWeek(world, plan, WeekCats());
    const WeekResult stepped = PlayWeekStepByStep(world, plan, WeekCats());
    WeekExpect(compressed.Ok() && compressed.Ledger.DecidedByOrder == 4 && compressed.Disclosures.size() == 4,
               "four decisions made on the player's behalf are all counted and disclosed");
    const WeekDisclosure& third = compressed.Disclosures[2];
    WeekExpect(third.Day == 1 && third.Entry == 7 && third.Kind == WeekEntryKind::Activity && third.What == "activity.study_at_home" &&
                   third.OrderId == "order.study_after_school" && third.At == MinutesPerDay + (990 - 360),
               "a disclosure names the day, the entry, what was done, the order, and when it began");
    WeekExpect(compressed.Disclosures[0].What == "spend" && compressed.Disclosures[1].What == "school" &&
                   compressed.Disclosures[3].What == "sleep",
               "spend, school and sleep are disclosed by kind");
    WeekExpect(compressed.Disclosures == stepped.Disclosures, "resolving step by step discloses the same decisions");
    WeekExpectEquivalent(compressed, stepped, "decisions by order do not change the outcome");
    const std::string receipt = RenderWeekReceipt(compressed);
    WeekExpect(receipt.find("decided_by_order: 4") != std::string::npos &&
                   receipt.find("day 1 entry 7 activity.study_at_home by order.study_after_school at 2042-06-03 16:30") != std::string::npos &&
                   receipt.find("order.sleep_at_ten") != std::string::npos,
               "the weekly receipt lists every decision and the order behind it");
    std::int64_t viaOutcomes = 0;
    for (const WeekEntryOutcome& outcome : compressed.Outcomes)
        viaOutcomes += outcome.Kind == WeekEntryKind::Activity ? 1 : 0;
    WeekExpect(viaOutcomes == 5 * 7 + 2 * 6 && compressed.Outcomes.size() == static_cast<std::size_t>(compressed.EntriesResolved),
               "every resolved entry has an outcome line");
}

void TestTimeContract(const WorldState& world, const WeekPlan& plan)
{
    const WeekTimePlanResult timePlan = BuildWeekTimePlan(world, plan, WeekCats());
    WeekExpect(timePlan.Ok() && timePlan.Plan.SpanMinutes == 7 * MinutesPerDay, "the week is a sound time plan of seven days");
    WeekExpect(CompareGranularities(timePlan.Plan).Equivalent(), "the contract's helper finds the week equivalent at every granularity");
    const WeekResult compressed = CompressWeek(world, plan, WeekCats());
    for (const TimeGranularity granularity : {TimeGranularity::Scenes, TimeGranularity::Day, TimeGranularity::Week, TimeGranularity::Span})
    {
        const TimePlanResult resolved = ResolvePlan(timePlan.Plan, granularity);
        WeekExpect(resolved.Ok(), "the contract resolves the week");
        WeekExpect(CompareWeekToTimeLedger(compressed, resolved.Ledger).empty(),
                   "what the day loop charged is what the time contract says the week costs");
    }
    // And a damaged ledger is caught.
    TimeLedger wrong = ResolvePlan(timePlan.Plan, TimeGranularity::Week).Ledger;
    wrong.ChargedByMode[static_cast<std::size_t>(ClockMode::Scene)] += 1;
    WeekExpect(CompareWeekToTimeLedger(compressed, wrong) == "charged_by_mode", "a different charge is named");
    wrong = ResolvePlan(timePlan.Plan, TimeGranularity::Week).Ledger;
    wrong.DecidedByOrder = 1;
    WeekExpect(CompareWeekToTimeLedger(compressed, wrong) == "decided_by_order", "a different count of decisions is named");

    // A stopped week is the time plan of its prefix.
    WeekPlan choice = plan;
    WeekAt(choice, 2, 4).NeedsChoice = true;
    const WeekResult stopped = CompressWeek(world, choice, WeekCats());
    const WeekTimePlanResult prefix = BuildWeekTimePlan(world, choice, WeekCats(), stopped.EntriesResolved);
    WeekExpect(prefix.Ok() && prefix.Plan.SpanMinutes == stopped.Ledger.EndInstant - stopped.Ledger.StartInstant,
               "the prefix's time plan ends where the stopped week ended");
    WeekExpect(CompareWeekToTimeLedger(stopped, ResolvePlan(prefix.Plan, TimeGranularity::Week).Ledger).empty(),
               "and costs what the stopped week charged");
}

void TestDeterminism(const WorldState& world, const WeekPlan& plan)
{
    const WeekResult a = CompressWeek(world, plan, WeekCats());
    const WorldState copy = world;
    const WeekResult b = CompressWeek(copy, plan, WeekCats());
    WeekExpect(HashWorldState(a.World) == HashWorldState(b.World) && a.Ledger == b.Ledger && a.Outcomes == b.Outcomes &&
                   a.Disclosures == b.Disclosures && a.DayCommands == b.DayCommands && a.ClockAdvances == b.ClockAdvances,
               "compression is a pure function of the world and the plan");
    WeekExpect(DiffWorldStates(world, copy).Identical(), "and the world given is untouched");
    WeekStepOptions fine;
    fine.ChunkMinutes = 30;
    const WeekResult c = PlayWeekStepByStep(world, plan, WeekCats(), fine);
    const WeekResult d = PlayWeekStepByStep(world, plan, WeekCats(), fine);
    WeekExpect(HashWorldState(c.World) == HashWorldState(d.World) && c.Ledger == d.Ledger, "the reference is deterministic too");
    WeekExpect(HashWorldState(a.World) != HashWorldState(c.World),
               "the world hash includes the command counters, which is why equivalence ignores them and not the hash");
    WeekExpect(a.World.DayLoop.Practice == c.World.DayLoop.Practice && a.World.DayLoop.Receipts.Entries.size() == c.World.DayLoop.Receipts.Entries.size() &&
                   a.World.DayLoop.Condition == c.World.DayLoop.Condition,
               "yet the practice window, the receipts and the condition are exactly equal");
}

WorldState WeekWorldWithGates()
{
    WorldState start = WeekMorningWorld();
    WorldSim sim(start);
    const auto submit = [&sim](CommandPayload payload, CommandSource source) {
        const CommandResult result = sim.Submit({source, std::move(payload)});
        WeekExpect(result.Accepted, "a world command is accepted");
    };
    submit(GateRegister{"gate.a.one", GateRank::E, GateStatus::Contained}, CommandSource::System);
    submit(GateScheduleOnset{"gate.a.one", 3000}, CommandSource::System);
    submit(GateRegister{"gate.b.two", GateRank::D, GateStatus::Contained}, CommandSource::System);
    submit(GateScheduleOnset{"gate.b.two", 20000}, CommandSource::System);
    submit(ScheduleWorldEvent{4000, "sys/kind/midweek"}, CommandSource::System);
    submit(SetWorldFlag{"week.flag", true}, CommandSource::Player);
    return sim.State();
}

void TestWorldEvents(const WeekPlan& plan)
{
    // The world's scheduled consequences fire exactly once, at their instants, whatever the granularity.
    const WorldState world = WeekWorldWithGates();
    const WeekResult compressed = CompressWeek(world, plan, WeekCats());
    WeekExpect(compressed.Ok(), "a week with scheduled events resolves");
    const GateSiteState& gate = compressed.World.Gates.at("gate.a.one");
    WeekExpect(gate.Status == GateStatus::Active && gate.StatusSince == 3000 && gate.OnsetCount == 1,
               "a Gate's onset fires exactly once, at its own instant, in the middle of an activity's span");
    WeekExpect(compressed.World.Gates.at("gate.b.two").Status == GateStatus::Contained &&
                   compressed.World.Clock.Pending().size() == 1 && compressed.World.Clock.Pending().front().When == 20000,
               "an onset after the week is still pending; the one inside it has fired");
    WeekExpect(compressed.World.Flags.at("week.flag"), "world flags are carried through");
    for (const std::int64_t chunk : {1, 15, 90, 1440})
    {
        WeekStepOptions options;
        options.ChunkMinutes = chunk;
        WeekExpectEquivalent(compressed, PlayWeekStepByStep(world, plan, WeekCats(), options),
                             "scheduled events fire identically at every granularity");
    }
    // Stopped part-way, the onset after the stop is still to come, exactly as in the reference.
    WeekPlan choice = plan;
    WeekAt(choice, 1, 0).NeedsChoice = true; // the stop is at 06:00 Tuesday (instant 1440), before the onset at 3000
    const WeekResult stopped = CompressWeek(world, choice, WeekCats());
    WeekExpect(stopped.Ok() && stopped.World.Gates.at("gate.a.one").Status == GateStatus::Contained &&
                   stopped.World.Clock.Pending().size() == 3,
               "an onset after the stop has not fired");
    WeekStepOptions upToIt;
    upToIt.EntryLimit = stopped.EntriesResolved;
    WeekExpectEquivalent(stopped, PlayWeekStepByStep(world, choice, WeekCats(), upToIt), "and the stopped world equals the reference");
}
} // namespace

void TestWeekRules()
{
    const WorldState world = WeekMorningWorld();
    const WeekPlan plan = WeekScriptedSchoolWeek();
    TestInterrupts(world, plan);
    TestFirsts(world, plan);
    TestDisclosure(world, plan);
    TestTimeContract(world, plan);
    TestDeterminism(world, plan);
    TestWorldEvents(plan);
}
