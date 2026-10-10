// ROADMAP M2-09: the "investigate a fault" runner (tests, elimination, costs, failure
// kinds, refusals). The catalogue is the real Data/Content. The properties shared by
// every structure (completion, selection, mastery, routine, save text, random play) run
// through StructureOthersHarness.h.
#include "StructureOthersHarness.h"

using namespace PerfectWorld;

namespace
{
struct InvestigateT
{
    using Catalogue = InvestigateCatalogue;
    using State = InvestigateState;
    using Action = InvestigateAction;
    using Outcome = InvestigateOutcome;
    using CaseDef = InvestigateCaseDef;
    static constexpr int Goals = 3;
    static constexpr std::size_t UnitsField = 3;
    static constexpr const char* Header = "pw.structure_investigate/1";
    static constexpr const char* IdleTail = "tests=-\n";

    static const Catalogue& Cat() { return TestCatalogues->Investigate; }
    static StructureBeginResult Begin(State& s, const Catalogue& c, std::string_view id, int goal, std::int32_t skill)
    {
        return BeginInvestigateExposure(s, c, id, static_cast<InvestigateGoal>(goal), skill);
    }
    static StructureError Cancel(State& s) { return CancelInvestigateExposure(s); }
    static StructureQuote Quote(const State& s, const Catalogue& c, const Action& a) { return QuoteInvestigateAction(s, c, a); }
    static InvestigateStepResult Apply(State& s, const Catalogue& c, const Action& a) { return ApplyInvestigateAction(s, c, a); }
    static Action Standard(const CaseDef& d, const InvestigateExposure& e) { return InvestigateStandardAction(d, e); }
    static StructureRoutineResult Routine(State& s, const Catalogue& c, std::string_view f) { return ResolveInvestigateRoutine(s, c, f); }
    static StructureSelection Select(const Catalogue& c, const State& s, std::string_view f, std::uint64_t seed, GameInstant at)
    {
        return SelectInvestigateCase(c, s, f, seed, at);
    }
    static std::string Serialise(const State& s) { return SerialiseStructureInvestigate(s); }
    static InvestigateParse Parse(std::string_view t) { return DeserialiseStructureInvestigate(t); }
    static bool Valid(const State& s) { return IsInvestigateStateValid(s); }
    static std::int32_t Budget(const CaseDef& d) { return InvestigateBudgetUnits(d.Scarce); }
    static bool PriceOnly(const Action&) { return false; }

    static Action Random(OthersRng& rng, const CaseDef& def, const State&)
    {
        Action action;
        const std::size_t kind = rng.Below(100);
        action.Kind = kind < 70 ? InvestigateActionKind::RunTest : kind < 85 ? InvestigateActionKind::Diagnose : InvestigateActionKind::ReportInconclusive;
        const std::size_t pick = rng.Below(30);
        action.Test = pick == 0 ? "t_nope" : pick == 1 ? std::string() : def.Tests[rng.Below(def.Tests.size())].Id;
        const std::size_t cause = rng.Below(30);
        action.Cause = cause == 0 ? "c_nope" : cause == 1 ? std::string() : def.Causes[rng.Below(def.Causes.size())].Id;
        return action;
    }

    /** A foreground exposure that ends Unsound: a diagnosis made at once, of a cause that is not the fault. */
    static Outcome FailOnce(State& state, const CaseDef& def)
    {
        Check(Begin(state, Cat(), def.Id, 0, FullSkill).Ok(), "begins");
        const auto wrong = std::find_if(def.Causes.begin(), def.Causes.end(), [](const InvestigateCause& c) { return !c.Fault; });
        return Harness::Do<InvestigateT>(state, DiagnoseAction(wrong->Id)).Outcome;
    }

    static InvestigateAction TestAction(std::string id)
    {
        InvestigateAction action;
        action.Kind = InvestigateActionKind::RunTest;
        action.Test = std::move(id);
        return action;
    }
    static InvestigateAction DiagnoseAction(std::string cause)
    {
        InvestigateAction action;
        action.Kind = InvestigateActionKind::Diagnose;
        action.Cause = std::move(cause);
        return action;
    }
    static InvestigateAction ReportAction()
    {
        InvestigateAction action;
        action.Kind = InvestigateActionKind::ReportInconclusive;
        return action;
    }
};

using IT = InvestigateT;

const InvestigateCaseDef& CaseOf(const std::string& shortName)
{
    const InvestigateCaseDef* def = IT::Cat().FindCase("investigate_case.xi_" + shortName);
    Check(def != nullptr, "the named case exists");
    return *def;
}

const InvestigateTest& TestOf(const InvestigateCaseDef& def, const std::string& id)
{
    const auto it = std::find_if(def.Tests.begin(), def.Tests.end(), [&](const InvestigateTest& t) { return t.Id == id; });
    Check(it != def.Tests.end(), "the named test exists");
    return *it;
}

InvestigateOutcome PlayTests(const InvestigateCaseDef& def, const std::vector<std::string>& tests, const InvestigateAction& end, int goal = 0)
{
    InvestigateState state = Harness::Started<IT>(def, goal, FullSkill);
    for (const std::string& id : tests)
        Harness::Do<IT>(state, IT::TestAction(id));
    return Harness::Do<IT>(state, end).Outcome;
}

void TestCatalogueShape()
{
    std::set<std::string> triples, symptoms, roles, scarces, subjects;
    for (const InvestigateCaseDef& def : Harness::Cases<IT>())
    {
        const std::string symptom(ModelNameOf(InvestigateSymptomNames, def.Symptom));
        const std::string role(ModelNameOf(InvestigateRoleNames, def.Role));
        const std::string scarce(ModelNameOf(InvestigateScarceNames, def.Scarce));
        triples.insert(symptom + "/" + role + "/" + scarce);
        symptoms.insert(symptom);
        roles.insert(role);
        scarces.insert(scarce);
        subjects.insert(def.Subject);
        Check(def.Causes.size() >= 4 && def.Tests.size() >= 4, "four or more causes and tests");
        Check(InvestigateFaultOf(def) != nullptr && std::count_if(def.Causes.begin(), def.Causes.end(), [](const InvestigateCause& c) { return c.Fault; }) == 1,
              "exactly one cause is the fault");
        std::set<std::int32_t> prices;
        for (const InvestigateTest& test : def.Tests)
            prices.insert(test.Units);
        Check(prices.size() >= 2, "tests have different costs");

        // Whichever cause were the fault, the tests could isolate it: no cause is a dead end.
        for (const InvestigateCause& cause : def.Causes)
        {
            InvestigateCaseDef moved = def;
            for (InvestigateCause& c : moved.Causes)
                c.Fault = c.Id == cause.Id;
            Check(InvestigatePlanFor(moved, InvestigateGoal::Inspect, false, {}).Found, "every cause can be isolated by some tests");
        }
        // The tests cannot all be run: scarcity is real.
        std::int32_t all = 0;
        for (const InvestigateTest& test : def.Tests)
            all += InvestigateTestCost(def, InvestigateGoal::Inspect, test, true);
        Check(all > InvestigateBudgetUnits(def.Scarce), "the period cannot hold every test");
    }
    Check(triples.size() == Harness::Cases<IT>().size(), "no two cases share their symptom, role and scarce resource");
    Check(symptoms.size() == 3 && roles.size() == 4 && scarces.size() == 4 && subjects.size() == 3, "the cases cover every symptom, role, scarce resource and subject");
}

void TestEliminationAndResults()
{
    const InvestigateCaseDef& fan = CaseOf("classroom_fan");
    Check(InvestigateFaultOf(fan)->Id == "c_capacitor", "the fan's fault is the capacitor");
    Check(InvestigateRemaining(fan, {}).size() == 4, "all four causes stand at the start");
    Check(InvestigateTestPositive(fan, TestOf(fan, "t_hum")) && !InvestigateTestPositive(fan, TestOf(fan, "t_spin")), "a test reads positive when the fault is among its causes");
    Check(InvestigateRemaining(fan, {"t_hum"}) == std::vector<std::string>({"c_capacitor", "c_regulator"}), "a positive result keeps only the implicated causes");
    Check(InvestigateRemaining(fan, {"t_spin"}) == std::vector<std::string>({"c_capacitor", "c_dust", "c_regulator"}), "a negative result removes the implicated causes");
    Check(InvestigateRemaining(fan, {"t_hum", "t_steps"}) == std::vector<std::string>({"c_capacitor"}), "two tests isolate the fault");
    Check(InvestigateRemaining(fan, {"t_steps", "t_hum"}) == InvestigateRemaining(fan, {"t_hum", "t_steps"}), "the result does not depend on the order");
    Check(InvestigateRemaining(fan, {"t_nope"}).size() == 4, "an unknown test is ignored");

    // Plans: the cheapest isolating tests, which change with the price.
    InvestigatePlan plan = InvestigatePlanFor(fan, InvestigateGoal::Inspect, true, {});
    Check(plan.Found && plan.Tests == std::vector<std::string>({"t_swap_capacitor"}) && plan.Units == 3, "practised, the direct swap (3) is the cheapest");
    plan = InvestigatePlanFor(fan, InvestigateGoal::Inspect, false, {});
    Check(plan.Found && plan.Units == 4 && plan.Tests == std::vector<std::string>({"t_swap_capacitor"}), "unfamiliar, the swap costs 4");
    plan = InvestigatePlanFor(fan, InvestigateGoal::Inspect, true, {"t_swap_capacitor"});
    Check(plan.Found && plan.Tests.empty() && plan.Units == 0, "once isolated, no further tests are planned");
    plan = InvestigatePlanFor(fan, InvestigateGoal::Inspect, true, {"t_hum"});
    Check(plan.Found && plan.Tests == std::vector<std::string>({"t_steps"}) && plan.Units == 3, "after the hum, the meter or the swap (3 each) isolates; a tie goes to the earlier-authored test");
    const InvestigateCaseDef& torch = CaseOf("torch_circuit");
    plan = InvestigatePlanFor(torch, InvestigateGoal::Inspect, true, {});
    Check(plan.Found && plan.Tests == std::vector<std::string>({"t_meter_loop", "t_bypass"}) && plan.Units == 3,
          "the clip: a reading (1) and a bridge (2) beat re-seating every clip (4)");
    plan = InvestigatePlanFor(torch, InvestigateGoal::Inspect, true, {"t_meter_loop"});
    Check(plan.Found && plan.Tests == std::vector<std::string>({"t_bypass"}), "after the reading, the bridge settles it");
}

void TestCostsShowBeforeCommit()
{
    // The fan: inspector (observing is a unit cheaper), equipment scarce (a measurement a unit dearer), seven units.
    const InvestigateCaseDef& fan = CaseOf("classroom_fan");
    Check(InvestigateBudgetUnits(fan.Scarce) == 7 && InvestigateBudgetUnits(CaseOf("cash_total").Scarce) == StructureScarceTimeUnits, "equipment leaves seven units, time five");
    const auto cost = [](const InvestigateCaseDef& def, InvestigateGoal goal, const char* test, bool known) {
        return InvestigateTestCost(def, goal, TestOf(def, test), known);
    };
    Check(cost(fan, InvestigateGoal::SwapParts, "t_spin", true) == 1 && cost(fan, InvestigateGoal::SwapParts, "t_hum", true) == 1,
          "observing: base less a unit for the inspector, to a floor of one");
    Check(cost(fan, InvestigateGoal::SwapParts, "t_steps", true) == 3 && cost(fan, InvestigateGoal::SwapParts, "t_steps", false) == 4,
          "a measurement: 2 plus a unit for scarce equipment, a unit more for an unfamiliar method");
    Check(cost(fan, InvestigateGoal::Measure, "t_steps", true) == 2 && cost(fan, InvestigateGoal::Measure, "t_steps", false) == 3,
          "the goal measure takes a unit off a measurement");
    Check(cost(fan, InvestigateGoal::SwapParts, "t_swap_capacitor", true) == 2 && cost(fan, InvestigateGoal::Inspect, "t_swap_capacitor", false) == 4 &&
              cost(fan, InvestigateGoal::Inspect, "t_swap_capacitor", true) == 3,
          "a swap: 3 practised, 4 unfamiliar, a unit less for the goal swap_parts");
    Check(cost(fan, InvestigateGoal::Inspect, "t_spin", false) == 1, "observing needs no method");

    // Scarce parts raise the swap; the experimenter takes a unit off the meter.
    const InvestigateCaseDef& torch = CaseOf("torch_circuit");
    Check(cost(torch, InvestigateGoal::Inspect, "t_meter_loop", true) == 1 && cost(torch, InvestigateGoal::Inspect, "t_meter_loop", false) == 2,
          "the experimenter's meter costs 2 less 1, a unit more unfamiliar");
    Check(cost(torch, InvestigateGoal::Inspect, "t_bypass", true) == 2 && cost(torch, InvestigateGoal::Inspect, "t_bypass", false) == 3 &&
              cost(torch, InvestigateGoal::SwapParts, "t_bypass", true) == 1,
          "parts scarce: a swap costs a unit more, a unit less for the goal swap_parts");
    // Scarce access raises a look; the repairer takes a unit off a swap.
    const InvestigateCaseDef& lamp = CaseOf("bicycle_lamp");
    Check(cost(lamp, InvestigateGoal::SwapParts, "t_roller", false) == 2 && cost(lamp, InvestigateGoal::Inspect, "t_roller", false) == 1 &&
              cost(lamp, InvestigateGoal::Inspect, "t_shake", true) == 2,
          "access scarce: a look costs a unit more, a unit less for the goal inspect");
    Check(cost(lamp, InvestigateGoal::Inspect, "t_strip_earth", true) == 2 && cost(lamp, InvestigateGoal::Inspect, "t_strip_earth", false) == 3 &&
              cost(lamp, InvestigateGoal::Inspect, "t_swap_bulb", true) == 1,
          "the repairer's swap costs its base less 1, a unit more unfamiliar");
    const InvestigateCaseDef& cash = CaseOf("cash_total");
    Check(cost(cash, InvestigateGoal::SwapParts, "t_formula", true) == 2 && cost(cash, InvestigateGoal::Inspect, "t_formula", true) == 1 &&
              cost(cash, InvestigateGoal::SwapParts, "t_recount", false) == 4,
          "the critic has no discount of its own");

    // Every price can be read before it is paid, and quoting changes nothing.
    InvestigateState state = Harness::Started<IT>(fan, 0, NoSkill);
    const InvestigateState before = state;
    const StructureQuote quote = QuoteInvestigateAction(state, IT::Cat(), IT::TestAction("t_steps"));
    Check(quote.Ok() && quote.Units == 4 && quote.UnitsAfter == 3, "the meter is quoted: 4 of 7");
    Check(QuoteInvestigateAction(state, IT::Cat(), IT::DiagnoseAction("c_dust")).Units == 0 && QuoteInvestigateAction(state, IT::Cat(), IT::ReportAction()).Units == 0,
          "concluding is free");
    Check(state == before, "reading prices changes nothing");
}

void TestUnitsRunOut()
{
    // The fan, unfamiliar, goal inspect: the swap (4), the hum (1) and a look (1) leave one unit; the meter (4) is shown as too dear.
    const InvestigateCaseDef& fan = CaseOf("classroom_fan");
    InvestigateState state = Harness::Started<IT>(fan, 0, NoSkill);
    Harness::Do<IT>(state, IT::TestAction("t_swap_capacitor"));
    Harness::Do<IT>(state, IT::TestAction("t_hum"));
    Check(state.Exposure.UnitsLeft == 2, "two tests leave two units");
    const InvestigateState spent = state;
    const StructureQuote dear = QuoteInvestigateAction(state, IT::Cat(), IT::TestAction("t_steps"));
    Check(dear.Error == StructureError::UnitsShort && dear.Units == 4 && dear.UnitsAfter == -2, "the quote shows 4 against 2 left");
    Check(ApplyInvestigateAction(state, IT::Cat(), IT::TestAction("t_steps")).Error == StructureError::UnitsShort && state == spent, "a refusal for spent time changes nothing");
    // Every state has a free way forward: the honest report ends the exposure, and the fault was isolated, so it abandoned the search.
    const InvestigateOutcome outcome = Harness::Do<IT>(state, IT::ReportAction()).Outcome;
    Check((outcome.Failures & InvestigateFailAbandoned) != 0 && (outcome.Failures & InvestigateFailRedundant) != 0 && outcome.Grade == StructureGrade::Partial,
          "the swap had already isolated the fault, so the hum was redundant and the report abandoned it");

    // The standard policy takes the cheapest isolating tests the units allow.
    InvestigateState careful = Harness::Started<IT>(fan, 0, NoSkill);
    Check(InvestigateStandardAction(fan, careful.Exposure).Test == "t_swap_capacitor", "the direct swap (4) beats a look and a reading (1+4)");
    Check(Harness::PlayStandard<IT>(careful).Grade == StructureGrade::Sound, "and the period ends soundly");
    for (const InvestigateCaseDef& def : Harness::Cases<IT>())
    {
        InvestigateState empty = Harness::Started<IT>(def, 0, NoSkill);
        empty.Exposure.UnitsLeft = 0;
        Check(QuoteInvestigateAction(empty, IT::Cat(), IT::ReportAction()).Ok() && QuoteInvestigateAction(empty, IT::Cat(), IT::DiagnoseAction(def.Causes[0].Id)).Ok(),
              "with no units left the exposure can still end");
        Check(InvestigateStandardAction(def, empty.Exposure).Kind == InvestigateActionKind::ReportInconclusive, "and the policy ends it honestly");
    }
}

void TestFailureKinds()
{
    const InvestigateCaseDef& fan = CaseOf("classroom_fan");
    // Sound: isolate by elimination and diagnose, by any route.
    InvestigateOutcome o = PlayTests(fan, {"t_hum", "t_steps"}, IT::DiagnoseAction("c_capacitor"));
    Check(o.Failures == 0 && o.Grade == StructureGrade::Sound && o.TestsRun == 2 && o.RemainingCauses == 1 && o.Diagnosed == "c_capacitor", "two tests and the right diagnosis are Sound");
    o = PlayTests(fan, {"t_spin", "t_blades", "t_steps"}, IT::DiagnoseAction("c_capacitor"));
    Check(o.Failures == 0 && o.RemainingCauses == 1, "eliminating three causes isolates the fourth: a second sound route");
    o = PlayTests(fan, {"t_swap_capacitor"}, IT::DiagnoseAction("c_capacitor"));
    Check(o.Failures == 0, "the direct test is a third");

    // A guess is unsupported, right or wrong; a wrong diagnosis is a wrong cause.
    o = PlayTests(fan, {"t_hum"}, IT::DiagnoseAction("c_capacitor"));
    Check(o.Failures == InvestigateFailUnsupported && o.Grade == StructureGrade::Unsound && o.RemainingCauses == 2, "a right guess with two causes standing is unsupported, not lucky");
    o = PlayTests(fan, {"t_hum"}, IT::DiagnoseAction("c_regulator"));
    Check(o.Failures == (InvestigateFailWrongCause | InvestigateFailUnsupported) && o.Grade == StructureGrade::Unsound, "a wrong guess is wrong and unsupported");
    o = PlayTests(fan, {"t_hum", "t_steps"}, IT::DiagnoseAction("c_dust"));
    Check(o.Failures == InvestigateFailWrongCause && o.Grade == StructureGrade::Unsound, "an isolated fault diagnosed wrongly is only a wrong cause");
    o = PlayTests(fan, {}, IT::DiagnoseAction("c_capacitor"));
    Check(o.Failures == InvestigateFailUnsupported && o.RemainingCauses == 4, "diagnosing at once is unsupported");

    // A test whose answer was already known is wasted time.
    o = PlayTests(fan, {"t_swap_capacitor", "t_hum"}, IT::DiagnoseAction("c_capacitor"));
    Check(o.Failures == InvestigateFailRedundant && o.RedundantTests == 1 && o.Grade == StructureGrade::Partial, "a test after the fault is isolated is redundant");
    o = PlayTests(fan, {"t_hum", "t_steps", "t_swap_capacitor"}, IT::DiagnoseAction("c_capacitor"));
    Check(o.RedundantTests == 1 && o.Failures == InvestigateFailRedundant, "only the test that settled nothing new is counted");
    o = PlayTests(fan, {"t_spin"}, IT::ReportAction());
    Check(o.Failures == 0 && o.Grade == StructureGrade::Sound && o.RemainingCauses == 3 && o.Diagnosed.empty(), "an honest report on an unfinished search is Sound");
    o = PlayTests(fan, {}, IT::ReportAction());
    Check(o.Failures == 0 && o.RemainingCauses == 4, "so is a report with nothing tried");

    // Giving up on a fault the tests had found is abandoning the search, but not for the critic.
    o = PlayTests(fan, {"t_hum", "t_steps"}, IT::ReportAction());
    Check(o.Failures == InvestigateFailAbandoned && o.Grade == StructureGrade::Partial, "reporting an isolated fault as unfound abandons the search");
    const InvestigateCaseDef& cash = CaseOf("cash_total");
    o = PlayTests(cash, {"t_formula"}, IT::ReportAction());
    Check(o.Failures == 0 && o.Grade == StructureGrade::Sound, "the critic's honest report is never abandonment");

    // The role and the goal do not change the grade, only the price.
    for (int goal = 0; goal < IT::Goals; ++goal)
        Check(PlayTests(cash, {"t_formula"}, IT::DiagnoseAction("c_range"), goal).Failures == 0, "the goal never changes what is Sound");
}

void TestCorrectAnswerIsNeverRefusedForAStat()
{
    for (const InvestigateCaseDef& def : Harness::Cases<IT>())
    {
        const InvestigatePlan plan = InvestigatePlanFor(def, InvestigateGoal::Inspect, false, {});
        InvestigateOutcome results[2];
        for (int who = 0; who < 2; ++who)
        {
            InvestigateState state = Harness::Started<IT>(def, 0, who == 0 ? NoSkill : FullSkill);
            for (const std::string& id : plan.Tests)
                Harness::Do<IT>(state, IT::TestAction(id));
            results[who] = Harness::Do<IT>(state, IT::DiagnoseAction(InvestigateFaultOf(def)->Id)).Outcome;
        }
        Check(results[0].Grade == StructureGrade::Sound && results[1].Grade == StructureGrade::Sound && results[0].Failures == results[1].Failures &&
                  results[0].RemainingCauses == results[1].RemainingCauses && results[0].LearningPermille == results[1].LearningPermille,
              "skill changes the price, never the result");
        Check(results[0].UnitsSpent >= results[1].UnitsSpent, "an unfamiliar method costs at least as much");
    }
}

void TestRefusals()
{
    const InvestigateCaseDef& fan = CaseOf("classroom_fan");
    InvestigateState state;
    Check(ApplyInvestigateAction(state, IT::Cat(), IT::TestAction("t_hum")).Error == StructureError::NotActive, "no exposure, no action");
    Check(QuoteInvestigateAction(state, IT::Cat(), IT::ReportAction()).Error == StructureError::NotActive, "no exposure, no quote");
    Check(CancelInvestigateExposure(state) == StructureError::NotActive, "nothing to cancel");
    Check(BeginInvestigateExposure(state, IT::Cat(), "investigate_case.xi_nope", InvestigateGoal::Inspect, 0).Error == StructureError::UnknownCase, "unknown case");
    Check(BeginInvestigateExposure(state, IT::Cat(), fan.Id, InvestigateGoal::Inspect, -1).Error == StructureError::InvalidSkill, "negative skill");
    Check(BeginInvestigateExposure(state, IT::Cat(), fan.Id, static_cast<InvestigateGoal>(9), 0).Error == StructureError::InvalidState, "goal out of range");
    Check(state == InvestigateState{}, "refused begins change nothing");

    state = Harness::Started<IT>(fan, 0, FullSkill);
    const InvestigateState begun = state;
    Check(BeginInvestigateExposure(state, IT::Cat(), fan.Id, InvestigateGoal::Inspect, 0).Error == StructureError::Busy && state == begun, "one exposure at a time");
    Check(ApplyInvestigateAction(state, IT::Cat(), IT::TestAction("")).Error == StructureError::BadChoice && state == begun, "no test named is refused");
    Check(ApplyInvestigateAction(state, IT::Cat(), IT::TestAction("t_nope")).Error == StructureError::UnknownItem && state == begun, "an unknown test is refused");
    Check(ApplyInvestigateAction(state, IT::Cat(), IT::DiagnoseAction("")).Error == StructureError::BadChoice && state == begun, "no cause named is refused");
    Check(ApplyInvestigateAction(state, IT::Cat(), IT::DiagnoseAction("c_nope")).Error == StructureError::UnknownItem && state == begun, "an unknown cause is refused");
    InvestigateAction bogus;
    bogus.Kind = static_cast<InvestigateActionKind>(9);
    Check(ApplyInvestigateAction(state, IT::Cat(), bogus).Error == StructureError::WrongBeat && state == begun, "a decision outside the structure is refused");
    Harness::Do<IT>(state, IT::TestAction("t_hum"));
    const InvestigateState ran = state;
    Check(ApplyInvestigateAction(state, IT::Cat(), IT::TestAction("t_hum")).Error == StructureError::AlreadyDone && state == ran, "a test is run once");
    Check(state.Exposure.TestsRun == std::vector<std::string>({"t_hum"}), "the tests are kept in the order run");
    Harness::Do<IT>(state, IT::TestAction("t_blades"));
    Check(state.Exposure.TestsRun == std::vector<std::string>({"t_hum", "t_blades"}), "later tests follow");

    // Concluding ends the exposure once; a second finds nothing to end.
    Check(Harness::Do<IT>(state, IT::DiagnoseAction("c_capacitor")).Finished, "the diagnosis finishes the exposure");
    const InvestigateState finished = state;
    Check(ApplyInvestigateAction(state, IT::Cat(), IT::ReportAction()).Error == StructureError::NotActive && state == finished, "an outcome is applied exactly once");
    Check(state.FindFamily(fan.FamilyId)->Exposures == 1, "one exposure was recorded");

    // Cancel: no outcome, no mastery change, but the case stays seen.
    state = Harness::Started<IT>(fan, 0, FullSkill);
    Harness::Do<IT>(state, IT::TestAction("t_hum"));
    Check(CancelInvestigateExposure(state) == StructureError::None && !state.Exposure.Active(), "an exposure can be abandoned");
    const StructureFamilyRecord* record = state.FindFamily(fan.FamilyId);
    Check(record != nullptr && record->Exposures == 0 && record->SoundExposures == 0 && record->Seen.size() == 1, "abandoning records no outcome but the case was seen");
}
} // namespace

void TestInvestigate()
{
    Harness::AllGeneric<IT>();
    TestCatalogueShape();
    TestEliminationAndResults();
    TestCostsShowBeforeCommit();
    TestUnitsRunOut();
    TestFailureKinds();
    TestCorrectAnswerIsNeverRefusedForAStat();
    TestRefusals();
}
