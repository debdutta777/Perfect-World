// ROADMAP M3-01: the equivalence rule between fine-grained and compressed time
// (SYSTEMS §2.5 rule 2, §8 granularity equivalence), the helper M3-02 will use.
#include "TimeContract.h"
#include "TimeContractTestSupport.h"

using namespace PerfectWorld;

namespace
{
using M = ClockMode;
using I = InterruptPolicy;
using R = ResolutionBoundary;

PlannedInteraction Item(const char* id, M mode, std::int32_t cost, I interrupt, R resolve, std::int32_t beats, std::int64_t start,
                        bool byOrder = false)
{
    PlannedInteraction item;
    item.Decl = MakeDecl(id, mode, cost, interrupt, resolve, beats);
    item.StartMinute = start;
    item.DecidedByOrder = byOrder;
    return item;
}

/** Three days: sleep, meal, the phone, a talk the standing orders chose, class, a run, a night across midnight, a walk. */
TimePlan ThreeDayPlan()
{
    TimePlan plan;
    plan.SpanMinutes = 3 * 1440;
    plan.Seed = 0x2042'0602ULL;
    plan.Items = {
        Item("plan.sleep", M::Scene, 480, I::Immediate, R::OnComplete, 1, 0),
        Item("plan.meal", M::Scene, 20, I::None, R::OnComplete, 1, 480),
        Item("plan.phone", M::UiPaused, 0, I::None, R::OnCommit, 1, 500),
        Item("plan.talk", M::Conversation, 10, I::AtBeat, R::OnComplete, 2, 500, true),
        Item("plan.class", M::Scene, 45, I::AtBeat, R::PerBeat, 3, 510),
        Item("plan.run", M::FreeRoam, 30, I::Immediate, R::OnComplete, 1, 600),
        Item("plan.night", M::Scene, 480, I::Immediate, R::OnComplete, 1, 1400),
        Item("plan.walk", M::FreeRoam, 0, I::None, R::OnComplete, 1, 2000),
    };
    return plan;
}

std::int64_t TotalCost(const TimePlan& plan)
{
    std::int64_t total = 0;
    for (const PlannedInteraction& item : plan.Items)
        total += item.Decl.CostMinutes;
    return total;
}

PlannedInteraction RandomItem(TimeTestRng& rng, std::int64_t start, std::int64_t room)
{
    for (int attempt = 0; attempt < 20; ++attempt)
    {
        PlannedInteraction item;
        item.Decl = TimeTestRandomDeclaration(rng);
        if (item.Decl.Mode == M::Mission || item.Decl.Mode == M::Compressed || item.Decl.CostMinutes > room)
            continue;
        item.StartMinute = start;
        item.FirstOfKind = rng.Range(0, 199) == 0;
        item.DecidedByOrder = rng.Range(0, 3) == 0;
        return item;
    }
    return Item("plan.fallback", M::UiPaused, 0, I::None, R::OnCommit, 1, start);
}

TimePlan RandomPlan(TimeTestRng& rng)
{
    TimePlan plan;
    plan.SpanMinutes = rng.Range(1, 28 * 1440);
    plan.Seed = rng.Next();
    std::int64_t cursor = 0;
    for (int count = 0; count < 60; ++count)
    {
        const std::int64_t start = cursor + rng.Range(0, 400);
        if (start >= plan.SpanMinutes)
            break;
        PlannedInteraction item = RandomItem(rng, start, plan.SpanMinutes - start);
        cursor = start + item.Decl.CostMinutes;
        plan.Items.push_back(std::move(item));
    }
    return plan;
}
} // namespace

void TestEquivalence()
{
    const TimePlan plan = ThreeDayPlan();
    Expect(ValidatePlan(plan) == TimeContractError::None, "the hand-built plan is sound");

    const TimePlanResult scenes = ResolvePlan(plan, TimeGranularity::Scenes);
    const TimePlanResult day = ResolvePlan(plan, TimeGranularity::Day);
    const TimePlanResult week = ResolvePlan(plan, TimeGranularity::Week);
    const TimePlanResult span = ResolvePlan(plan, TimeGranularity::Span);
    Expect(scenes.Ok() && day.Ok() && week.Ok() && span.Ok(), "every granularity resolves the plan");

    const TimeLedger& l = scenes.Ledger;
    Expect(l.EndMinute == 4320 && l.Completed == 8 && l.TotalCharged == 1065, "the ledger totals what the plan costs");
    Expect(l.ChargedByMode[static_cast<std::size_t>(M::Scene)] == 1025 && l.ChargedByMode[static_cast<std::size_t>(M::FreeRoam)] == 30 &&
               l.ChargedByMode[static_cast<std::size_t>(M::Conversation)] == 10 && l.ChargedByMode[static_cast<std::size_t>(M::UiPaused)] == 0,
           "charges are split by mode and the phone is free");
    Expect(l.OutcomeBeats == 11 && l.DecidedByOrder == 1, "outcome beats count, and the standing-order choice is disclosed");
    Expect(TotalCost(plan) == l.TotalCharged, "a routine plan is charged exactly its declared costs");

    for (const TimePlanResult* other : {&day, &week, &span})
    {
        Expect(CompareLedgers(scenes.Ledger, other->Ledger).empty(), "scenes, days, weeks and one span give the same persistent outcome");
        Expect(scenes.Ledger.DayDrawHash == other->Ledger.DayDrawHash && scenes.Ledger.InteractionHash == other->Ledger.InteractionHash, "the keyed draws agree");
    }
    Expect(CompareGranularities(plan).Equivalent(), "the helper reports equivalence");

    // The excluded quantities really do change with granularity, so the exclusion list is not vacuous.
    Expect(scenes.Ledger.Steps == 3 + 8 && day.Ledger.Steps == 3 && week.Ledger.Steps == 1 && span.Ledger.Steps == 1, "steps differ with granularity");
    Expect(scenes.Ledger != week.Ledger && TimeEquivalenceExcluded[0] == "steps", "so steps are excluded from equivalence");
    TimeLedger ignoring = week.Ledger;
    ignoring.Steps = 99;
    Expect(CompareLedgers(ignoring, week.Ledger).empty(), "a different step count is not a difference");
    Expect(TimeEquivalenceQuantities.size() == 8, "eight quantities must match");

    // Each must-match quantity is actually compared.
    const auto differs = [&](auto change, std::string_view name) {
        TimeLedger tampered = scenes.Ledger;
        change(tampered);
        Expect(CompareLedgers(scenes.Ledger, tampered) == name, "a changed quantity is named");
        Expect(std::find(TimeEquivalenceQuantities.begin(), TimeEquivalenceQuantities.end(), name) != TimeEquivalenceQuantities.end(),
               "and the name is on the documented list");
    };
    differs([](TimeLedger& t) { ++t.EndMinute; }, "end_minute");
    differs([](TimeLedger& t) { ++t.ChargedByMode[2]; }, "charged_by_mode");
    differs([](TimeLedger& t) { ++t.TotalCharged; }, "total_charged");
    differs([](TimeLedger& t) { ++t.Completed; }, "completed");
    differs([](TimeLedger& t) { ++t.OutcomeBeats; }, "outcome_beats");
    differs([](TimeLedger& t) { ++t.DecidedByOrder; }, "decided_by_order");
    differs([](TimeLedger& t) { ++t.DayDrawHash; }, "day_draw_hash");
    differs([](TimeLedger& t) { ++t.InteractionHash; }, "interaction_hash");

    // A genuinely different plan is not equivalent.
    TimePlan other = plan;
    other.Seed ^= 1;
    const TimeLedger reseeded = ResolvePlan(other, TimeGranularity::Day).Ledger;
    Expect(CompareLedgers(scenes.Ledger, reseeded) == "day_draw_hash", "another seed changes the draws and nothing else");
    other = plan;
    other.Items[5].Decl.CostMinutes = 35;
    Expect(CompareLedgers(scenes.Ledger, ResolvePlan(other, TimeGranularity::Day).Ledger) == "charged_by_mode", "another plan is told apart");

    // Keyed draws are pure functions of their key.
    Expect(TimeKeyedDraw(1, 2, 3) == TimeKeyedDraw(1, 2, 3), "a draw repeats");
    Expect(TimeKeyedDraw(1, 2, 3) != TimeKeyedDraw(2, 2, 3) && TimeKeyedDraw(1, 2, 3) != TimeKeyedDraw(1, 3, 3) &&
               TimeKeyedDraw(1, 2, 3) != TimeKeyedDraw(1, 2, 4) && TimeKeyedDraw(1, 2, 3) != TimeKeyedDraw(1, 3, 2),
           "a draw depends on seed, day and index");
    Expect(TimeKeyedDraw(0x2042'0602ULL, 17, -1) == TimeKeyedDraw(0x2042'0602ULL, 17, -1), "the day draw is keyed, not streamed");

    // A first is never auto-resolved: played scenes may contain it, every compression refuses it.
    TimePlan withFirst = plan;
    withFirst.Items[3].FirstOfKind = true;
    Expect(ResolvePlan(withFirst, TimeGranularity::Scenes).Ok(), "a first can be played as a scene");
    for (const TimeGranularity g : {TimeGranularity::Day, TimeGranularity::Week, TimeGranularity::Span})
        Expect(ResolvePlan(withFirst, g).Error == TimeContractError::PlanNeedsPlay, "compression refuses a first");
    Expect(CompareGranularities(withFirst).Error == TimeContractError::PlanNeedsPlay && !CompareGranularities(withFirst).Equivalent(),
           "the helper reports that the plan needs play");

    // Plans that are not routine or not sound are refused, never half resolved.
    const auto refuses = [&](TimePlan bad, TimeContractError error, const char* what) {
        Expect(ValidatePlan(bad) == error && ResolvePlan(bad, TimeGranularity::Span).Error == error && CompareGranularities(bad).Error == error, what);
    };
    TimePlan bad = plan;
    bad.SpanMinutes = 0;
    refuses(bad, TimeContractError::PlanSpanInvalid, "a plan needs a span");
    bad = plan;
    bad.Items[2] = Item("plan.tower", M::Mission, 60, I::None, R::OnComplete, 1, 500);
    bad.Items[2].Decl.Mission.FixedMinutes = 60;
    refuses(bad, TimeContractError::PlanNotRoutine, "a mission is never routine");
    bad = plan;
    bad.Items[2] = Item("plan.week", M::Compressed, 1440, I::None, R::OnComplete, 1, 500);
    refuses(bad, TimeContractError::PlanNotRoutine, "compression is not an item of a plan");
    bad = plan;
    std::swap(bad.Items[0], bad.Items[1]);
    refuses(bad, TimeContractError::PlanOutOfOrder, "items must be in time order");
    bad = plan;
    bad.Items[1].StartMinute = 400;
    refuses(bad, TimeContractError::PlanOverlap, "items may not overlap");
    bad = plan;
    bad.SpanMinutes = 1500;
    refuses(bad, TimeContractError::PlanOutOfSpan, "items must end inside the span");
    bad = plan;
    bad.Items[0].Decl.CostMinutes = 0;
    refuses(bad, TimeContractError::CostOutOfRange, "an unsound declaration spoils the plan");
    Expect(ResolvePlan(TimePlan{100, 1, {}}, TimeGranularity::Week).Ok() && ResolvePlan(TimePlan{100, 1, {}}, TimeGranularity::Week).Ledger.EndMinute == 100,
           "an empty plan resolves to its span");

    // Determinism: the same plan gives the same ledger every time, down to a fixed hash.
    Expect(ResolvePlan(plan, TimeGranularity::Week).Ledger == ResolvePlan(plan, TimeGranularity::Week).Ledger, "resolution is deterministic");
}

void TestRandomEquivalence()
{
    TimeTestRng rng(0x5eed2042ULL);
    int compressible = 0;
    int needingPlay = 0;
    std::int64_t items = 0;
    for (int trial = 0; trial < 10000; ++trial)
    {
        const TimePlan plan = RandomPlan(rng);
        Expect(ValidatePlan(plan) == TimeContractError::None, "the generated plan is sound");
        const bool hasFirst = std::any_of(plan.Items.begin(), plan.Items.end(), [](const PlannedInteraction& i) { return i.FirstOfKind; });
        const TimePlanResult scenes = ResolvePlan(plan, TimeGranularity::Scenes);
        Expect(scenes.Ok(), "scenes always resolve a sound plan");
        const TimeLedger& l = scenes.Ledger;
        Expect(l.EndMinute == plan.SpanMinutes && l.Completed == static_cast<std::int32_t>(plan.Items.size()), "every interaction completes and time ends at the span");
        Expect(l.TotalCharged == TotalCost(plan), "a routine plan is charged exactly its costs");
        std::int64_t byMode = 0;
        for (const std::int64_t charged : l.ChargedByMode)
            byMode += charged;
        Expect(byMode == l.TotalCharged && l.ChargedByMode[static_cast<std::size_t>(M::UiPaused)] == 0 &&
                   l.ChargedByMode[static_cast<std::size_t>(M::AccessibilityPause)] == 0,
               "the per-mode charges add up and the pauses are free");
        const std::int32_t ordered = static_cast<std::int32_t>(
            std::count_if(plan.Items.begin(), plan.Items.end(), [](const PlannedInteraction& i) { return i.DecidedByOrder; }));
        Expect(l.DecidedByOrder == ordered, "every choice made by standing order is counted");
        Expect(ResolvePlan(plan, TimeGranularity::Scenes).Ledger == l, "a plan resolves the same way twice");
        items += static_cast<std::int64_t>(plan.Items.size());

        const EquivalenceReport report = CompareGranularities(plan);
        if (hasFirst)
        {
            ++needingPlay;
            Expect(report.Error == TimeContractError::PlanNeedsPlay, "a plan holding a first is never compressed");
            continue;
        }
        ++compressible;
        Expect(report.Equivalent(), "scenes, days, weeks and the span give the same persistent outcome");
        Expect(CompareLedgers(l, ResolvePlan(plan, TimeGranularity::Week).Ledger).empty(), "the weekly ledger matches the scene ledger");
    }
    Expect(compressible > 5000 && needingPlay > 500 && items > 50000, "the random plans cover both kinds and are not trivial");
}
