// ROADMAP M2-09, a first step toward M2-21 ("Four-structure distinctness check"): the four
// structures share controls, not meaning (SCHOOL_AND_PROGRESSION.md). Here the portable
// part of that claim is checked: no two structures share a beat, a goal, a decision or a
// failure meaning, they have different numbers of beats, and the standard careful play of
// one case of each decides different things in a different order. The same final posture,
// declining to commit to the answer the evidence invites, is graded differently by each.
#include "StructureOthersTestSupport.h"

#include <set>

using namespace PerfectWorld;

namespace
{
struct Profile
{
    std::string Name;
    std::vector<std::string> Beats;
    std::vector<std::string> Goals;
    std::vector<std::string> Decisions;
    std::vector<std::string> Failures;
    /** The decisions the standard careful policy makes on one case, in order. */
    std::string Transcript;
    /** What declining to commit is graded. */
    StructureGrade Decline = StructureGrade::Sound;
};

template <std::size_t N>
std::vector<std::string> Names(const std::array<std::string_view, N>& names)
{
    return std::vector<std::string>(names.begin(), names.end());
}

std::string Joined(const std::vector<std::string>& parts)
{
    std::string text;
    for (const std::string& part : parts)
        text += (text.empty() ? "" : ">") + part;
    return text;
}

Profile ModelProfile()
{
    // The model structure (M2-08) names its decisions and failure bits in code, not in arrays.
    static const std::array<std::string_view, 4> decisions{"worked_example", "commit_model", "investigate", "present_claim"};
    static const std::array<std::string_view, 4> failures{"wrong_model", "unsupported_conclusion", "wasted_time", "unclear_explanation"};
    static_assert(ModelFailWrongModel == 1 && ModelFailUnsupported == 2 && ModelFailWastedTime == 4 && ModelFailUnclear == 8);
    Profile profile{"model", Names(ModelBeatNames), Names(ModelGoalNames), Names(decisions), Names(failures), "", StructureGrade::Sound};

    const ModelCatalogue& cat = TestCatalogues->Model;
    const ModelCaseDef& def = *cat.FindCase("problem_case.xi_water_tank_level");
    StructureModelState state;
    Check(BeginModelExposure(state, cat, def.Id, ModelGoal::UnderstandModel, NoSkill).Ok(), "the model case begins");
    std::vector<std::string> played;
    for (int i = 0; i < 8 && state.Exposure.Active(); ++i)
    {
        const ModelAction action = ModelStandardAction(def, state.Exposure);
        played.emplace_back(decisions[static_cast<std::size_t>(action.Kind)]);
        Check(ApplyModelAction(state, cat, action).Ok(), "the model's standard action is legal");
    }
    profile.Transcript = Joined(played);

    // Declining to commit on readings that were fine: a hedge wastes the period.
    const ModelCaseDef& roof = *cat.FindCase("problem_case.xi_roof_rainwater_tank");
    StructureModelState hedge;
    Check(BeginModelExposure(hedge, cat, roof.Id, ModelGoal::UnderstandModel, FullSkill).Ok(), "the roof case begins");
    ModelAction commit;
    commit.Kind = ModelActionKind::CommitModel;
    for (const ModelLink& link : roof.Links)
        if (link.Sound)
            commit.Links.push_back(link.Id);
    ModelAction investigate;
    investigate.Kind = ModelActionKind::Investigate;
    investigate.Investigation = ModelInvestigation::AcceptReadings;
    ModelAction claim;
    claim.Kind = ModelActionKind::PresentClaim;
    claim.Claim = ModelClaim::NeedMoreMeasurement;
    claim.Explain = true;
    Check(ApplyModelAction(hedge, cat, commit).Ok() && ApplyModelAction(hedge, cat, investigate).Ok(), "the roof case is played");
    const ModelStepResult last = ApplyModelAction(hedge, cat, claim);
    Check(last.Ok() && last.Finished, "the hedge finishes the exposure");
    profile.Decline = last.Outcome.Grade == ModelGrade::Sound ? StructureGrade::Sound : last.Outcome.Grade == ModelGrade::Partial ? StructureGrade::Partial : StructureGrade::Unsound;
    return profile;
}

Profile ExplainProfile()
{
    Profile profile{"explain", Names(ExplainBeatNames), Names(ExplainGoalNames), Names(ExplainDecisionNames), Names(ExplainFailureNames), "", StructureGrade::Sound};
    const ExplainCatalogue& cat = TestCatalogues->Explain;
    const ExplainCaseDef& def = *cat.FindCase("explain_case.xi_tram_poem");
    ExplainState state;
    Check(BeginExplainExposure(state, cat, def.Id, ExplainGoal::ArgueClaim, NoSkill).Ok(), "the explain case begins");
    std::vector<std::string> played;
    for (int i = 0; i < 8 && state.Exposure.Active(); ++i)
    {
        const ExplainAction action = ExplainStandardAction(def, state.Exposure);
        played.emplace_back(ExplainDecisionNames[static_cast<std::size_t>(action.Kind)]);
        Check(ApplyExplainAction(state, cat, action).Ok(), "the explain standard action is legal");
    }
    profile.Transcript = Joined(played);

    // Declining to name a limit.
    ExplainState bare;
    Check(BeginExplainExposure(bare, cat, def.Id, ExplainGoal::ArgueClaim, FullSkill).Ok(), "the explain case begins");
    ExplainAction claim;
    claim.Kind = ExplainActionKind::ChooseClaim;
    claim.Claim = "c_farewell";
    ExplainAction evidence;
    evidence.Kind = ExplainActionKind::PlaceEvidence;
    evidence.Evidence = {"e_empty_seats"};
    ExplainAction limit;
    limit.Kind = ExplainActionKind::PlaceLimitation;
    Check(ApplyExplainAction(bare, cat, claim).Ok() && ApplyExplainAction(bare, cat, evidence).Ok(), "the explain case is played");
    const ExplainStepResult last = ApplyExplainAction(bare, cat, limit);
    Check(last.Ok() && last.Finished, "stating no limit finishes the exposure");
    profile.Decline = last.Outcome.Grade;
    return profile;
}

Profile InvestigateProfile()
{
    Profile profile{"investigate", Names(InvestigateBeatNames), Names(InvestigateGoalNames), Names(InvestigateDecisionNames), Names(InvestigateFailureNames), "",
                    StructureGrade::Sound};
    const InvestigateCatalogue& cat = TestCatalogues->Investigate;
    const InvestigateCaseDef& def = *cat.FindCase("investigate_case.xi_classroom_fan");
    InvestigateState state;
    Check(BeginInvestigateExposure(state, cat, def.Id, InvestigateGoal::Inspect, NoSkill).Ok(), "the investigate case begins");
    std::vector<std::string> played;
    for (int i = 0; i < 8 && state.Exposure.Active(); ++i)
    {
        const InvestigateAction action = InvestigateStandardAction(def, state.Exposure);
        played.emplace_back(InvestigateDecisionNames[static_cast<std::size_t>(action.Kind)]);
        Check(ApplyInvestigateAction(state, cat, action).Ok(), "the investigate standard action is legal");
    }
    profile.Transcript = Joined(played);

    // Declining to diagnose before the fault is isolated: "I cannot say yet".
    InvestigateState open;
    Check(BeginInvestigateExposure(open, cat, def.Id, InvestigateGoal::Inspect, FullSkill).Ok(), "the investigate case begins");
    InvestigateAction report;
    report.Kind = InvestigateActionKind::ReportInconclusive;
    const InvestigateStepResult last = ApplyInvestigateAction(open, cat, report);
    Check(last.Ok() && last.Finished, "the report finishes the exposure");
    profile.Decline = last.Outcome.Grade;
    return profile;
}

Profile CoordinateProfile()
{
    Profile profile{"coordinate", Names(CoordinateBeatNames), Names(CoordinateGoalNames), Names(CoordinateDecisionNames), Names(CoordinateFailureNames), "",
                    StructureGrade::Sound};
    const CoordinateCatalogue& cat = TestCatalogues->Coordinate;
    const CoordinateCaseDef& def = *cat.FindCase("coordinate_case.xi_science_stall");
    CoordinateState state;
    Check(BeginCoordinateExposure(state, cat, def.Id, CoordinateGoal::OrganiseWork, NoSkill).Ok(), "the coordinate case begins");
    std::vector<std::string> played;
    for (int i = 0; i < 12 && state.Exposure.Active(); ++i)
    {
        const CoordinateAction action = CoordinateStandardAction(def, state.Exposure);
        played.emplace_back(CoordinateDecisionNames[static_cast<std::size_t>(action.Kind)]);
        Check(ApplyCoordinateAction(state, cat, action).Ok(), "the coordinate standard action is legal");
    }
    profile.Transcript = Joined(played);

    // Declining to settle the constraint.
    CoordinateState left;
    Check(BeginCoordinateExposure(left, cat, def.Id, CoordinateGoal::OrganiseWork, FullSkill).Ok(), "the coordinate case begins");
    CoordinateAction assign;
    assign.Kind = CoordinateActionKind::AssignWork;
    assign.Assignment = CoordinateSoundAssignment(def);
    CoordinateAction end;
    end.Kind = CoordinateActionKind::EndExchange;
    CoordinateAction settle;
    settle.Kind = CoordinateActionKind::Settle;
    settle.Resolution = CoordinateResolution::Leave;
    CoordinateAction submit;
    submit.Kind = CoordinateActionKind::Submit;
    Check(ApplyCoordinateAction(left, cat, assign).Ok() && ApplyCoordinateAction(left, cat, end).Ok() && ApplyCoordinateAction(left, cat, settle).Ok(),
          "the coordinate case is played");
    const CoordinateStepResult last = ApplyCoordinateAction(left, cat, submit);
    Check(last.Ok() && last.Finished, "submitting finishes the exposure");
    profile.Decline = last.Outcome.Grade;
    return profile;
}

bool Disjoint(const std::vector<std::string>& a, const std::vector<std::string>& b)
{
    return std::none_of(a.begin(), a.end(), [&](const std::string& item) { return std::find(b.begin(), b.end(), item) != b.end(); });
}
} // namespace

void TestDistinct(const std::vector<ContentFile>& files)
{
    const std::vector<Profile> profiles{ModelProfile(), ExplainProfile(), InvestigateProfile(), CoordinateProfile()};
    std::set<std::size_t> beatCounts;
    std::set<std::string> transcripts, firstDecisions;
    std::set<int> declines;
    for (std::size_t i = 0; i < profiles.size(); ++i)
    {
        const Profile& p = profiles[i];
        Check(!p.Beats.empty() && !p.Goals.empty() && !p.Decisions.empty() && p.Failures.size() >= 4 && !p.Transcript.empty(), "every structure has beats, goals, decisions and failure meanings");
        beatCounts.insert(p.Beats.size());
        transcripts.insert(p.Transcript);
        firstDecisions.insert(p.Transcript.substr(0, p.Transcript.find('>')));
        declines.insert(static_cast<int>(p.Decline));
        for (std::size_t j = i + 1; j < profiles.size(); ++j)
        {
            const Profile& q = profiles[j];
            Check(p.Beats != q.Beats && Disjoint(p.Beats, q.Beats), "no two structures share a beat");
            Check(Disjoint(p.Goals, q.Goals), "no two structures share a goal");
            Check(Disjoint(p.Decisions, q.Decisions), "no two structures share a decision");
            Check(Disjoint(p.Failures, q.Failures), "no two structures share a failure meaning");
            Check(p.Transcript != q.Transcript, "the standard play of two structures is never the same sequence of decisions");
        }
    }
    Check(beatCounts == std::set<std::size_t>({2, 3, 4}), "the structures have two, three and four beats");
    Check(transcripts.size() == 4 && firstDecisions.size() >= 3, "four different plays, opening in at least three different ways");

    // The same posture means different things: declining to commit is a wasteful hedge, an overreach, an honest report and a failure to deliver.
    Check(profiles[0].Decline == StructureGrade::Partial && profiles[1].Decline == StructureGrade::Partial && profiles[2].Decline == StructureGrade::Sound &&
              profiles[3].Decline == StructureGrade::Unsound && declines.size() == 3,
          "declining to commit is graded differently by the structures");

    // The structure records in the data agree with the runners: four records, four engines.
    std::set<std::string> engines;
    const ContentLoadResult content = LoadContent(files);
    for (const ContentRecord* record : content.Db.OfType("structure"))
    {
        const JsonValue* engine = record->Data.Find("engine");
        Check(engine != nullptr && engine->IsString(), "a structure names its engine");
        engines.insert(engine->String);
    }
    Check(engines == std::set<std::string>({"model", "explain", "investigate", "coordinate"}), "the data has one structure record for each of the four engines");
}
