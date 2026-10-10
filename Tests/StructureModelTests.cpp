// ROADMAP M2-08: the "model a system" runner (play, costs, failure kinds, refusals).
// The catalogue is the real Data/Content. Selection, mastery, routine, the save text
// and random sequences are in StructureModelFlowTests.cpp.
#include "StructureModelTestSupport.h"

#include <set>

using namespace PerfectWorld;

namespace
{
std::string NameOfUnknown(const ModelCaseDef& def) { return std::string(ModelNameOf(ModelUnknownNames, def.Unknown)); }

void TestCatalogueShape()
{
    Check(Catalogue().Families.size() == 1, "one family is authored");
    const ModelFamilyDef& family = Catalogue().Families.front();
    Check(family.Id == ModelTestFamily && family.Cases.size() >= 8 && static_cast<std::int32_t>(family.Cases.size()) >= family.MinCases,
          "the classroom family has at least eight cases");
    std::set<std::string> triples, unknowns, roles, scarces, faults, subjects;
    for (const ModelCaseDef& def : family.Cases)
    {
        const std::string role(ModelNameOf(ModelRoleNames, def.Role));
        const std::string scarce(ModelNameOf(ModelScarceNames, def.Scarce));
        triples.insert(NameOfUnknown(def) + "/" + role + "/" + scarce);
        unknowns.insert(NameOfUnknown(def));
        roles.insert(role);
        scarces.insert(scarce);
        faults.insert(std::string(ModelNameOf(ModelFaultNames, def.Fault)));
        subjects.insert(def.Subject);
        Check(!SoundLinks(def).empty() && SoundLinks(def).size() < def.Links.size(), "a case has sound and unsound links");
    }
    Check(triples.size() == family.Cases.size(), "no two cases share their unknown, role and scarce resource");
    Check(unknowns.size() == 4 && roles.size() == 5 && scarces.size() == 4 && faults.size() == 4 && subjects.size() == 4,
          "the cases cover every unknown, role, scarce resource, fault and subject");
    for (std::size_t i = 1; i < family.Cases.size(); ++i)
        Check(family.Cases[i - 1].Id < family.Cases[i].Id, "cases are sorted by ID");
}

void TestEveryCaseCanBeCompleted()
{
    for (const ModelCaseDef& def : Catalogue().Families.front().Cases)
    {
        for (const ModelGoal goal : AllGoals)
        {
            for (const std::int32_t skill : {NoSkill, 115000, 130000, FullSkill})
            {
                StructureModelState state = Started(def, goal, skill);
                const ModelOutcome outcome = PlayStandard(state);
                Check(outcome.Grade == ModelGrade::Sound && outcome.Failures == 0, "every case can be completed soundly");
                Check(outcome.Foreground && outcome.LearningPermille == ModelNormalLearningPermille, "foreground learning is the normal value");
                Check(outcome.UnitsSpent >= 0 && outcome.UnitsSpent <= ModelBudgetUnits(def.Scarce), "within the budget");
                Check(outcome.MissingLinks == 0 && outcome.ExtraLinks == 0 && outcome.CaseId == def.Id && outcome.FamilyId == ModelTestFamily,
                      "the outcome names its case and a right model");
                const ModelFamilyRecord* record = state.FindFamily(ModelTestFamily);
                Check(record != nullptr && record->Exposures == 1 && record->SoundExposures == 1, "one sound exposure is recorded");
            }
        }
    }
}

void TestCorrectAnswerIsNeverRefusedForAStat()
{
    for (const ModelCaseDef& def : Catalogue().Families.front().Cases)
    {
        for (const ModelGoal goal : AllGoals)
        {
            // The least able character commits the right model at once, with no example.
            StructureModelState low = Started(def, goal, NoSkill);
            Check(!low.Exposure.MethodKnown, "the least able character lacks the method");
            const ModelStepResult commit = Do(low, CommitAction(SoundLinks(def)));
            Check(commit.Charged == ModelCommitCost(def, goal, false), "an unfamiliar method is charged the derive cost");
            Check(low.Exposure.UnitsLeft >= 0 && low.Exposure.Beat == ModelBeat::ResolveDiscrepancy, "the right model is accepted");

            // The same decisions by a master give the same result; only the price differs.
            StructureModelState high = Started(def, goal, FullSkill);
            Do(high, CommitAction(SoundLinks(def)));
            const ModelAction invest = InvestigateAction(ModelMatchingInvestigation(def.Fault));
            if (!QuoteModelAction(low, Catalogue(), invest).Ok())
                continue; // the derive left too little time for the test: refused for time, not for skill
            Do(low, invest);
            Do(high, invest);
            const bool explain = ModelExplainCost(def, goal) <= low.Exposure.UnitsLeft;
            const ModelOutcome lowOutcome = Do(low, ClaimAction(ModelClaim::Estimate, explain)).Outcome;
            const ModelOutcome highOutcome = Do(high, ClaimAction(ModelClaim::Estimate, explain)).Outcome;
            Check(lowOutcome.Grade == highOutcome.Grade && lowOutcome.Failures == highOutcome.Failures &&
                      lowOutcome.MissingLinks == highOutcome.MissingLinks && lowOutcome.ExtraLinks == highOutcome.ExtraLinks &&
                      lowOutcome.LearningPermille == highOutcome.LearningPermille,
                  "skill changes the price, never the result");
            Check(lowOutcome.UnitsSpent >= highOutcome.UnitsSpent, "an unfamiliar method costs at least as much");
            Check(lowOutcome.MissingLinks == 0 && lowOutcome.ExtraLinks == 0, "the committed right model is graded right");
        }
    }
}

void TestCostsShowBeforeCommit()
{
    // The water tank: solver (the model is a unit cheaper), time scarce (five units), a timing fault.
    const ModelCaseDef& tank = CaseOf("water_tank_level");
    Check(ModelBudgetUnits(tank.Scarce) == ModelScarceTimeUnits && ModelScarceTimeUnits == 5, "scarce time leaves five units");
    Check(ModelBudgetUnits(ModelScarce::Help) == ModelWorkUnits && ModelWorkUnits == 7 && ModelPeriodUnits == 9, "a period is nine units, seven of work");
    Check(ModelExampleCost(tank) == 1 && ModelCommitCost(tank, ModelGoal::ExplainToGroup, true) == 1 &&
              ModelCommitCost(tank, ModelGoal::ExplainToGroup, false) == 3,
          "the solver commits a unit cheaper: 1 practised, 3 derived");
    Check(ModelInvestigationCost(tank, ModelGoal::UnderstandModel, ModelInvestigation::CheckTimes) == 1 &&
              ModelInvestigationCost(tank, ModelGoal::TestMeasurements, ModelInvestigation::CheckTimes) == 0 &&
              ModelInvestigationCost(tank, ModelGoal::UnderstandModel, ModelInvestigation::AcceptReadings) == 0,
          "the goal test_measurements makes the investigation a unit cheaper");

    // Scarce equipment raises the price of the instruments, discounted for the experimenter.
    const ModelCaseDef& overhead = CaseOf("overhead_tank_draw");
    Check(ModelInvestigationCost(overhead, ModelGoal::UnderstandModel, ModelInvestigation::LookForFlow) == 2 &&
              ModelInvestigationCost(overhead, ModelGoal::UnderstandModel, ModelInvestigation::CompareMarkings) == 1 &&
              ModelInvestigationCost(overhead, ModelGoal::UnderstandModel, ModelInvestigation::CheckTimes) == 0,
          "scarce equipment: look for a flow 2+1-1, compare 1+1-1, check the times 1-1");
    Check(ModelInvestigationCost(CaseOf("cricket_required_rate"), ModelGoal::UnderstandModel, ModelInvestigation::LookForFlow) == 3,
          "without the experimenter's discount scarce equipment costs 2+1");

    // Scarce help raises the worked example; scarce readings the checking of times.
    Check(ModelExampleCost(CaseOf("daybook_cash_box")) == 3 && ModelExampleCost(tank) == 1, "scarce help: an example costs 1+2");
    Check(ModelInvestigationCost(CaseOf("stationery_notebooks"), ModelGoal::UnderstandModel, ModelInvestigation::CheckTimes) == 2,
          "scarce readings: checking the times costs 1+1");

    // The explanation is free for the recorder, presenter and the goal that owes it.
    Check(ModelExplainCost(tank, ModelGoal::UnderstandModel) == 1 && ModelExplainCost(tank, ModelGoal::ExplainToGroup) == 0 &&
              ModelExplainCost(CaseOf("stationery_notebooks"), ModelGoal::UnderstandModel) == 0 &&
              ModelExplainCost(CaseOf("monsoon_canal"), ModelGoal::UnderstandModel) == 0,
          "explaining is free for the recorder, the presenter and the explain goal");
    Check(ModelExplanationOwed(CaseOf("monsoon_canal"), ModelGoal::UnderstandModel) && ModelExplanationOwed(tank, ModelGoal::ExplainToGroup) &&
              !ModelExplanationOwed(tank, ModelGoal::UnderstandModel),
          "the presenter and the explain goal owe an explanation");

    // Every price of the current beat can be read at the start, and quoting changes nothing.
    StructureModelState state = Started(CaseOf("daybook_cash_box"), ModelGoal::UnderstandModel, NoSkill);
    const StructureModelState before = state;
    const ModelQuote example = QuoteModelAction(state, Catalogue(), ExampleAction());
    const ModelQuote commit = QuoteModelAction(state, Catalogue(), CommitAction({}));
    Check(example.Ok() && example.Units == 3 && example.UnitsAfter == 4, "the example is quoted: 3 of 7");
    Check(commit.Ok() && commit.Units == 3 && commit.UnitsAfter == 4, "committing with no links still quotes the derive cost (4 less 1 for the goal)");
    Check(QuoteModelAction(state, Catalogue(), InvestigateAction(ModelInvestigation::CheckTimes)).Error == ModelError::WrongBeat,
          "a later beat is not yet legal");
    Check(state == before, "reading prices changes nothing");

    // After the example the method counts as shown, and the commit is the practised price.
    Do(state, ExampleAction());
    Check(QuoteModelAction(state, Catalogue(), CommitAction({})).Units == 1, "after the example the commit is 2 less 1");
    Check(QuoteModelAction(state, Catalogue(), ExampleAction()).Error == ModelError::ExampleNotNeeded, "one example is enough");
    const StructureModelState shown = state;
    Check(ApplyModelAction(state, Catalogue(), ExampleAction()).Error == ModelError::ExampleNotNeeded && state == shown, "a second example is refused");
    const StructureModelState known = Started(CaseOf("daybook_cash_box"), ModelGoal::UnderstandModel, FullSkill);
    Check(QuoteModelAction(known, Catalogue(), ExampleAction()).Error == ModelError::ExampleNotNeeded, "a practised character needs no example");
}

void TestUnitsRunOut()
{
    // The canal: time scarce (five units), presenter, hidden flow. Deriving at the explain goal leaves one unit,
    // so the right investigation (two) is shown as too dear before the player commits to it.
    const ModelCaseDef& canal = CaseOf("monsoon_canal");
    StructureModelState state = Started(canal, ModelGoal::ExplainToGroup, NoSkill);
    Do(state, CommitAction(SoundLinks(canal)));
    Check(state.Exposure.UnitsLeft == 1, "deriving leaves one unit");
    const StructureModelState before = state;
    const ModelQuote dear = QuoteModelAction(state, Catalogue(), InvestigateAction(ModelInvestigation::LookForFlow));
    Check(dear.Error == ModelError::UnitsShort && dear.Units == 2 && dear.UnitsAfter == -1, "the quote shows 2 against 1 left");
    Check(ApplyModelAction(state, Catalogue(), InvestigateAction(ModelInvestigation::LookForFlow)).Error == ModelError::UnitsShort &&
              state == before,
          "a refusal for spent time changes nothing");
    // Every beat has a free way forward: accept the readings, then hedge honestly without explaining.
    Do(state, InvestigateAction(ModelInvestigation::AcceptReadings));
    Check(state.Exposure.UnitsLeft == 1, "accepting the readings is free");
    const ModelOutcome outcome = Do(state, ClaimAction(ModelClaim::BoundedRange, false)).Outcome;
    Check(outcome.Failures == ModelFailUnclear && outcome.Grade == ModelGrade::Partial, "the presenter owed an explanation: right but unclear");

    // The standard policy takes the example instead, which leaves enough.
    StructureModelState careful = Started(canal, ModelGoal::ExplainToGroup, NoSkill);
    Check(ModelStandardAction(canal, careful.Exposure).Kind == ModelActionKind::WorkedExample, "the example (1+2) beats deriving (4)");
    Check(PlayStandard(careful).Grade == ModelGrade::Sound, "and the period still ends soundly");
    // Where deriving is cheaper than the example plus the practised method, the policy derives.
    const StructureModelState help = Started(CaseOf("daybook_cash_box"), ModelGoal::TestMeasurements, NoSkill);
    Check(ModelStandardAction(CaseOf("daybook_cash_box"), help.Exposure).Kind == ModelActionKind::CommitModel, "scarce help: derive (4) beats 3+2");
}

ModelOutcome PlayAll(const ModelCaseDef& def, ModelGoal goal, std::vector<std::string> links, ModelInvestigation investigation,
                     ModelClaim claim, bool explain)
{
    StructureModelState state = Started(def, goal, FullSkill);
    Do(state, CommitAction(std::move(links)));
    Do(state, InvestigateAction(investigation));
    return Do(state, ClaimAction(claim, explain)).Outcome;
}

void TestFailureKinds()
{
    const ModelCaseDef& tank = CaseOf("water_tank_level");            // solver, timing fault
    const ModelCaseDef& roof = CaseOf("roof_rainwater_tank");         // experimenter, no fault
    const ModelCaseDef& stationery = CaseOf("stationery_notebooks");  // recorder, no fault
    const ModelCaseDef& cricket = CaseOf("cricket_required_rate");    // critic, scale fault
    const ModelCaseDef& overhead = CaseOf("overhead_tank_draw");      // experimenter, hidden flow
    const ModelGoal understand = ModelGoal::UnderstandModel;

    // A wrong model: a link missing, or one that does not belong.
    ModelOutcome o = PlayAll(tank, understand, {"l_flow_adds"}, ModelInvestigation::CheckTimes, ModelClaim::Estimate, false);
    Check(o.Failures == ModelFailWrongModel && o.Grade == ModelGrade::Unsound && o.MissingLinks == 1 && o.ExtraLinks == 0, "a missing link is a wrong model");
    o = PlayAll(tank, understand, {"l_flow_adds", "l_minutes_accumulate", "l_paint_adds"}, ModelInvestigation::CheckTimes, ModelClaim::Estimate, false);
    Check(o.Failures == ModelFailWrongModel && o.MissingLinks == 0 && o.ExtraLinks == 1, "a link that does not belong is a wrong model");
    o = PlayAll(tank, understand, {"l_paint_adds", "l_gain_drives_flow"}, ModelInvestigation::CheckTimes, ModelClaim::Estimate, false);
    Check(o.MissingLinks == 2 && o.ExtraLinks == 2, "both counts are reported");

    // An estimate over an unresolved discrepancy is unsupported; the honest hedge is sound.
    o = PlayAll(tank, understand, SoundLinks(tank), ModelInvestigation::AcceptReadings, ModelClaim::Estimate, false);
    Check(o.Failures == ModelFailUnsupported && o.Grade == ModelGrade::Unsound, "an estimate on untrusted readings is unsupported");
    o = PlayAll(tank, understand, SoundLinks(tank), ModelInvestigation::AcceptReadings, ModelClaim::BoundedRange, false);
    Check(o.Failures == 0 && o.Grade == ModelGrade::Sound, "a bounded range on untrusted readings is a sound posture");
    o = PlayAll(tank, understand, SoundLinks(tank), ModelInvestigation::AcceptReadings, ModelClaim::NeedMoreMeasurement, false);
    Check(o.Failures == 0, "asking for another measurement is sound while the discrepancy stands");
    o = PlayAll(tank, understand, SoundLinks(tank), ModelInvestigation::CheckTimes, ModelClaim::Estimate, false);
    Check(o.Failures == 0 && o.Grade == ModelGrade::Sound, "finding the clock fault and then estimating is sound");

    // The wrong investigation finds nothing: time wasted, and the discrepancy still stands.
    o = PlayAll(tank, understand, SoundLinks(tank), ModelInvestigation::LookForFlow, ModelClaim::Estimate, false);
    Check(o.Failures == (ModelFailWastedTime | ModelFailUnsupported) && o.Grade == ModelGrade::Unsound,
          "a leak hunt on a clock fault wastes time and leaves the estimate unsupported");
    o = PlayAll(tank, understand, SoundLinks(tank), ModelInvestigation::CompareMarkings, ModelClaim::BoundedRange, false);
    Check(o.Failures == ModelFailWastedTime && o.Grade == ModelGrade::Partial, "the wrong test, then an honest hedge: right but wasteful");

    // When the measurements are fine, hedging wastes the period and so does testing.
    o = PlayAll(roof, understand, SoundLinks(roof), ModelInvestigation::AcceptReadings, ModelClaim::Estimate, false);
    Check(o.Failures == 0 && o.Grade == ModelGrade::Sound, "trusting good readings and estimating is sound");
    o = PlayAll(roof, understand, SoundLinks(roof), ModelInvestigation::AcceptReadings, ModelClaim::NeedMoreMeasurement, false);
    Check(o.Failures == ModelFailWastedTime && o.Grade == ModelGrade::Partial, "hedging on good readings wastes the period");
    o = PlayAll(roof, understand, SoundLinks(roof), ModelInvestigation::LookForFlow, ModelClaim::Estimate, false);
    Check(o.Failures == ModelFailWastedTime, "testing good readings wastes the period");

    // A critic's hedge is the critic's job; an experimenter's is not.
    o = PlayAll(cricket, understand, SoundLinks(cricket), ModelInvestigation::CompareMarkings, ModelClaim::BoundedRange, false);
    Check(o.Failures == 0 && o.Grade == ModelGrade::Sound, "the critic who found the fault and still bounds the claim wastes nothing");
    o = PlayAll(overhead, understand, SoundLinks(overhead), ModelInvestigation::LookForFlow, ModelClaim::BoundedRange, false);
    Check(o.Failures == ModelFailWastedTime, "the experimenter who found the flow and still hedges wastes the period");

    // Wrong model and wasted time combine; the failure kinds stay separate.
    o = PlayAll(roof, understand, {"l_area_subtracts"}, ModelInvestigation::CheckTimes, ModelClaim::Estimate, false);
    Check(o.Failures == (ModelFailWrongModel | ModelFailWastedTime) && o.Grade == ModelGrade::Unsound, "a wrong model and a wasted test are both reported");

    // The recorder (and the explain goal) owe the group an explanation.
    o = PlayAll(stationery, understand, SoundLinks(stationery), ModelInvestigation::AcceptReadings, ModelClaim::Estimate, false);
    Check(o.Failures == ModelFailUnclear && o.Grade == ModelGrade::Partial, "a right estimate the group cannot follow is unclear, not wrong");
    o = PlayAll(stationery, understand, SoundLinks(stationery), ModelInvestigation::AcceptReadings, ModelClaim::Estimate, true);
    Check(o.Failures == 0, "explaining clears it");
    o = PlayAll(tank, ModelGoal::ExplainToGroup, SoundLinks(tank), ModelInvestigation::CheckTimes, ModelClaim::Estimate, false);
    Check(o.Failures == ModelFailUnclear, "the explain goal owes the explanation whatever the role");
    o = PlayAll(tank, understand, SoundLinks(tank), ModelInvestigation::CheckTimes, ModelClaim::Estimate, true);
    Check(o.Failures == 0 && o.Grade == ModelGrade::Sound, "an unrequired explanation costs a unit and breaks nothing");
}

void TestRefusals()
{
    const ModelCaseDef& tank = CaseOf("water_tank_level");
    StructureModelState state;
    Check(ApplyModelAction(state, Catalogue(), CommitAction({"l_flow_adds"})).Error == ModelError::NotActive, "no exposure, no action");
    Check(QuoteModelAction(state, Catalogue(), ExampleAction()).Error == ModelError::NotActive, "no exposure, no quote");
    Check(CancelModelExposure(state) == ModelError::NotActive, "nothing to cancel");
    Check(BeginModelExposure(state, Catalogue(), "problem_case.xi_nope", ModelGoal::UnderstandModel, 0).Error == ModelError::UnknownCase, "unknown case");
    Check(BeginModelExposure(state, Catalogue(), tank.Id, ModelGoal::UnderstandModel, -1).Error == ModelError::InvalidSkill, "negative skill");
    Check(BeginModelExposure(state, Catalogue(), tank.Id, static_cast<ModelGoal>(9), 0).Error == ModelError::InvalidState, "goal out of range");
    Check(state == StructureModelState{}, "refused begins change nothing");

    state = Started(tank, ModelGoal::UnderstandModel, FullSkill);
    const StructureModelState begun = state;
    Check(BeginModelExposure(state, Catalogue(), tank.Id, ModelGoal::UnderstandModel, 0).Error == ModelError::Busy && state == begun, "one exposure at a time");
    for (const ModelAction& wrong : {InvestigateAction(ModelInvestigation::CheckTimes), ClaimAction(ModelClaim::Estimate, false)})
        Check(ApplyModelAction(state, Catalogue(), wrong).Error == ModelError::WrongBeat && state == begun, "an action of another beat is refused");
    Check(ApplyModelAction(state, Catalogue(), CommitAction({})).Error == ModelError::BadModel && state == begun, "an empty model is refused");
    Check(ApplyModelAction(state, Catalogue(), CommitAction({"l_nope"})).Error == ModelError::UnknownLink && state == begun, "an unknown link is refused");
    Check(ApplyModelAction(state, Catalogue(), CommitAction({"l_flow_adds", "l_flow_adds"})).Error == ModelError::BadModel && state == begun,
          "a repeated link is refused");
    Do(state, CommitAction({"l_flow_adds", "l_minutes_accumulate"}));
    const StructureModelState committed = state;
    Check(ApplyModelAction(state, Catalogue(), CommitAction(SoundLinks(tank))).Error == ModelError::WrongBeat && state == committed,
          "the model is committed once");
    Check(ApplyModelAction(state, Catalogue(), InvestigateAction(ModelInvestigation::None)).Error == ModelError::BadModel && state == committed,
          "no investigation is not a choice");
    Do(state, InvestigateAction(ModelInvestigation::CheckTimes));
    const StructureModelState investigated = state;
    Check(ApplyModelAction(state, Catalogue(), InvestigateAction(ModelInvestigation::LookForFlow)).Error == ModelError::WrongBeat && state == investigated,
          "only one investigation fits before the group must report");
    Check(ApplyModelAction(state, Catalogue(), ClaimAction(static_cast<ModelClaim>(7), false)).Error == ModelError::BadModel && state == investigated,
          "an unknown claim is refused");

    // The claim ends the exposure once; a second claim finds nothing to end.
    Check(Do(state, ClaimAction(ModelClaim::Estimate, false)).Finished, "the claim finishes the exposure");
    const StructureModelState finished = state;
    Check(ApplyModelAction(state, Catalogue(), ClaimAction(ModelClaim::Estimate, false)).Error == ModelError::NotActive && state == finished,
          "an outcome is applied exactly once");
    Check(state.FindFamily(ModelTestFamily)->Exposures == 1, "one exposure was recorded");

    // Cancel: no outcome, no mastery change, but the case stays seen.
    state = Started(tank, ModelGoal::UnderstandModel, FullSkill);
    Check(CancelModelExposure(state) == ModelError::None && !state.Exposure.Active(), "an exposure can be abandoned");
    const ModelFamilyRecord* record = state.FindFamily(ModelTestFamily);
    Check(record != nullptr && record->Exposures == 0 && record->SoundExposures == 0 && record->Seen.size() == 1,
          "abandoning records no outcome but the case was seen");
    Check(ResolveModelRoutine(state, Catalogue(), "problem_family.nope").Error == ModelError::UnknownFamily, "routine: unknown family");
    Check(ModelErrorCode(ModelError::UnitsShort) == "model.units_short" && ModelErrorCode(ModelError::None).empty(), "stable error codes");
}
} // namespace

void TestSelection();
void TestMasteryAndRoutine();
void TestText();
void TestRandomSequences();

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: PerfectWorldStructureModelTests <Data/Content>\n";
        return 2;
    }
    const std::vector<ContentFile> files = ReadContentDirectory(argv[1]);
    const ContentLoadResult content = LoadContent(files);
    for (const ContentIssue& issue : content.Issues)
        std::cerr << issue.File << ": " << issue.Message << '\n';
    Check(content.Ok(), "the real content validates");
    const ModelCatalogueResult loaded = LoadModelCatalogue(content.Db);
    Check(loaded.Ok(), loaded.Error.c_str());
    ModelTestCatalogue = &loaded.Catalogue;

    TestCatalogueShape();
    TestEveryCaseCanBeCompleted();
    TestCorrectAnswerIsNeverRefusedForAStat();
    TestCostsShowBeforeCommit();
    TestUnitsRunOut();
    TestFailureKinds();
    TestRefusals();
    TestSelection();
    TestMasteryAndRoutine();
    TestText();
    TestRandomSequences();
    TestContentRules(files);
    TestCatalogueRules(files);
    std::cout << "StructureModelTests passed (" << ModelTestChecks << " checks)\n";
    return 0;
}
