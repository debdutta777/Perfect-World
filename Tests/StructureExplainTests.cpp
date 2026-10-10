// ROADMAP M2-09: the "construct an explanation" runner (claims, evidence, limits, costs,
// failure kinds, refusals). The catalogue is the real Data/Content. The properties shared
// by every structure (completion, selection, mastery, routine, save text, random play)
// run through StructureOthersHarness.h.
#include "StructureOthersHarness.h"

using namespace PerfectWorld;

namespace
{
struct ExplainT
{
    using Catalogue = ExplainCatalogue;
    using State = ExplainState;
    using Action = ExplainAction;
    using Outcome = ExplainOutcome;
    using CaseDef = ExplainCaseDef;
    static constexpr int Goals = 3;
    static constexpr std::size_t UnitsField = 4;
    static constexpr const char* Header = "pw.structure_explain/1";
    static constexpr const char* IdleTail = "claim=-\nevidence=-\n";

    static const Catalogue& Cat() { return TestCatalogues->Explain; }
    static StructureBeginResult Begin(State& s, const Catalogue& c, std::string_view id, int goal, std::int32_t skill)
    {
        return BeginExplainExposure(s, c, id, static_cast<ExplainGoal>(goal), skill);
    }
    static StructureError Cancel(State& s) { return CancelExplainExposure(s); }
    static StructureQuote Quote(const State& s, const Catalogue& c, const Action& a) { return QuoteExplainAction(s, c, a); }
    static ExplainStepResult Apply(State& s, const Catalogue& c, const Action& a) { return ApplyExplainAction(s, c, a); }
    static Action Standard(const CaseDef& d, const ExplainExposure& e) { return ExplainStandardAction(d, e); }
    static StructureRoutineResult Routine(State& s, const Catalogue& c, std::string_view f) { return ResolveExplainRoutine(s, c, f); }
    static StructureSelection Select(const Catalogue& c, const State& s, std::string_view f, std::uint64_t seed, GameInstant at)
    {
        return SelectExplainCase(c, s, f, seed, at);
    }
    static std::string Serialise(const State& s) { return SerialiseStructureExplain(s); }
    static ExplainParse Parse(std::string_view t) { return DeserialiseStructureExplain(t); }
    static bool Valid(const State& s) { return IsExplainStateValid(s); }
    static std::int32_t Budget(const CaseDef& d) { return ExplainBudgetUnits(d.Scarce); }
    /** An action that quotes its price with nothing chosen but is refused when applied. */
    static bool PriceOnly(const Action& a)
    {
        return (a.Kind == ExplainActionKind::ChooseClaim && a.Claim.empty()) || (a.Kind == ExplainActionKind::PlaceEvidence && a.Evidence.empty());
    }

    static Action Random(OthersRng& rng, const CaseDef& def, const State& state)
    {
        Action action;
        const ExplainBeat beat = state.Exposure.Beat;
        action.Kind = rng.Below(100) < 75 ? (beat == ExplainBeat::Claim ? static_cast<ExplainActionKind>(rng.Below(2)) : beat == ExplainBeat::Evidence ? ExplainActionKind::PlaceEvidence : ExplainActionKind::PlaceLimitation)
                                          : static_cast<ExplainActionKind>(rng.Below(4));
        const std::size_t pick = rng.Below(10);
        action.Claim = pick == 0 ? "c_nope" : pick == 1 ? std::string() : def.Claims[rng.Below(def.Claims.size())].Id;
        for (std::size_t i = rng.Below(4); i > 0; --i)
            action.Evidence.push_back(rng.Below(20) == 0 ? "e_nope" : def.Evidence[rng.Below(def.Evidence.size())].Id);
        const std::size_t choose = rng.Below(10);
        action.Limitation = choose == 0 ? "l_nope" : choose < 3 ? std::string() : def.Limitations[rng.Below(def.Limitations.size())].Id;
        return action;
    }

    /** A foreground exposure that ends Unsound: the claim the sources contradict. */
    static Outcome FailOnce(State& state, const CaseDef& def)
    {
        Check(Begin(state, Cat(), def.Id, 0, FullSkill).Ok(), "begins");
        const auto bad = std::find_if(def.Claims.begin(), def.Claims.end(), [](const ExplainClaim& c) { return !c.Defensible; });
        Harness::Do<ExplainT>(state, ClaimAction(bad->Id));
        Harness::Do<ExplainT>(state, EvidenceAction({def.Evidence.front().Id}));
        return Harness::Do<ExplainT>(state, LimitationAction("")).Outcome;
    }

    static ExplainAction ClaimAction(std::string claim)
    {
        ExplainAction action;
        action.Kind = ExplainActionKind::ChooseClaim;
        action.Claim = std::move(claim);
        return action;
    }
    static ExplainAction EvidenceAction(std::vector<std::string> items)
    {
        ExplainAction action;
        action.Kind = ExplainActionKind::PlaceEvidence;
        action.Evidence = std::move(items);
        return action;
    }
    static ExplainAction LimitationAction(std::string limitation)
    {
        ExplainAction action;
        action.Kind = ExplainActionKind::PlaceLimitation;
        action.Limitation = std::move(limitation);
        return action;
    }
    static ExplainAction ParagraphAction()
    {
        ExplainAction action;
        action.Kind = ExplainActionKind::ModelParagraph;
        return action;
    }
};

const ExplainCaseDef& CaseOf(const std::string& shortName)
{
    const ExplainCaseDef* def = ExplainT::Cat().FindCase("explain_case.xi_" + shortName);
    Check(def != nullptr, "the named case exists");
    return *def;
}

ExplainOutcome PlayAll(const ExplainCaseDef& def, ExplainGoal goal, const std::string& claim, std::vector<std::string> items, const std::string& limitation)
{
    ExplainState state = Harness::Started<ExplainT>(def, static_cast<int>(goal), FullSkill);
    Harness::Do<ExplainT>(state, ExplainT::ClaimAction(claim));
    Harness::Do<ExplainT>(state, ExplainT::EvidenceAction(std::move(items)));
    return Harness::Do<ExplainT>(state, ExplainT::LimitationAction(limitation)).Outcome;
}

void TestCatalogueShape()
{
    std::set<std::string> triples, questions, roles, scarces, subjects;
    for (const ExplainCaseDef& def : Harness::Cases<ExplainT>())
    {
        const std::string question(ModelNameOf(ExplainQuestionNames, def.Question));
        const std::string role(ModelNameOf(ExplainRoleNames, def.Role));
        const std::string scarce(ModelNameOf(ExplainScarceNames, def.Scarce));
        triples.insert(question + "/" + role + "/" + scarce);
        questions.insert(question);
        roles.insert(role);
        scarces.insert(scarce);
        subjects.insert(def.Subject);
        const auto defensible = std::count_if(def.Claims.begin(), def.Claims.end(), [](const ExplainClaim& c) { return c.Defensible; });
        Check(defensible >= 2 && static_cast<std::size_t>(defensible) < def.Claims.size(), "at least two claims are defensible and at least one is not");
        Check(std::any_of(def.Evidence.begin(), def.Evidence.end(), [](const ExplainEvidence& e) { return e.Supports.empty() && e.Against.empty(); }),
              "some evidence is beside the point, so padding is possible");
    }
    Check(triples.size() == Harness::Cases<ExplainT>().size(), "no two cases share their question, role and scarce resource");
    Check(questions.size() == 3 && roles.size() == 4 && scarces.size() == 4 && subjects.size() == 4, "the cases cover every question, role, scarce resource and subject");
}

void TestNoHiddenRightAnswer()
{
    // Every defensible claim can be completed Sound, at every goal and every skill: interpretation has no single answer.
    int claims = 0;
    for (const ExplainCaseDef& def : Harness::Cases<ExplainT>())
    {
        for (const ExplainClaim& claim : def.Claims)
        {
            const ExplainPlan plan = ExplainPlanFor(def, claim.Id);
            Check(plan.ClaimId.empty() == !claim.Defensible, "only a defensible claim has a Sound plan");
            if (!claim.Defensible)
                continue;
            ++claims;
            for (int goal = 0; goal < ExplainT::Goals; ++goal)
            {
                for (const std::int32_t skill : {NoSkill, FullSkill})
                {
                    ExplainState state = Harness::Started<ExplainT>(def, goal, skill);
                    if (!state.Exposure.MethodKnown && ExplainParagraphCost(def) + ExplainClaimCost(def, static_cast<ExplainGoal>(goal), true) <
                                                           ExplainClaimCost(def, static_cast<ExplainGoal>(goal), false))
                        Harness::Do<ExplainT>(state, ExplainT::ParagraphAction());
                    Harness::Do<ExplainT>(state, ExplainT::ClaimAction(claim.Id));
                    Harness::Do<ExplainT>(state, ExplainT::EvidenceAction({plan.EvidenceId}));
                    const ExplainOutcome outcome = Harness::Do<ExplainT>(state, ExplainT::LimitationAction(plan.LimitationId)).Outcome;
                    Check(outcome.Grade == StructureGrade::Sound && outcome.Failures == 0 && outcome.Bounded && outcome.SupportingItems == 1,
                          "every defensible claim is completed soundly");
                }
            }
        }
    }
    Check(claims >= 8, "the cases offer at least eight defensible claims in all");
}

void TestCorrectAnswerIsNeverRefusedForAStat()
{
    for (const ExplainCaseDef& def : Harness::Cases<ExplainT>())
    {
        for (const ExplainClaim& claim : def.Claims)
        {
            if (!claim.Defensible)
                continue;
            const ExplainPlan plan = ExplainPlanFor(def, claim.Id);
            ExplainOutcome results[2];
            std::int32_t spent[2];
            for (int who = 0; who < 2; ++who)
            {
                ExplainState state = Harness::Started<ExplainT>(def, 0, who == 0 ? NoSkill : FullSkill);
                Check(state.Exposure.MethodKnown == (who == 1), "the least able character lacks the method, the master has it");
                Harness::Do<ExplainT>(state, ExplainT::ClaimAction(claim.Id));
                Check(state.Exposure.UnitsLeft >= 0, "the claim is accepted whatever the skill");
                Harness::Do<ExplainT>(state, ExplainT::EvidenceAction({plan.EvidenceId}));
                const ExplainStepResult last = Harness::Do<ExplainT>(state, ExplainT::LimitationAction(plan.LimitationId));
                results[who] = last.Outcome;
                spent[who] = last.Outcome.UnitsSpent;
            }
            Check(results[0].Grade == results[1].Grade && results[0].Failures == results[1].Failures &&
                      results[0].LearningPermille == results[1].LearningPermille && results[0].SupportingItems == results[1].SupportingItems,
                  "skill changes the price, never the result");
            Check(spent[0] >= spent[1] + 1, "the unfamiliar reader pays more for the claim");
        }
    }
}

void TestCostsShowBeforeCommit()
{
    // The poem: author (the claim is a unit cheaper), time scarce (five units).
    const ExplainCaseDef& tram = CaseOf("tram_poem");
    Check(ExplainBudgetUnits(tram.Scarce) == StructureScarceTimeUnits && ExplainBudgetUnits(tram.Scarce) == 5, "scarce time leaves five units");
    Check(ExplainClaimCost(tram, ExplainGoal::WeighEvidence, true) == 1 && ExplainClaimCost(tram, ExplainGoal::WeighEvidence, false) == 3,
          "the author's claim costs 2 less 1 practised, 4 less 1 unfamiliar");
    Check(ExplainParagraphCost(tram) == 1 && ExplainEvidenceCost(tram, ExplainGoal::ArgueClaim, 1) == 1 &&
              ExplainEvidenceCost(tram, ExplainGoal::ArgueClaim, 2) == 2 && ExplainEvidenceCost(tram, ExplainGoal::WeighEvidence, 2) == 1 &&
              ExplainEvidenceCost(tram, ExplainGoal::ArgueClaim, 0) == 0,
          "evidence is a unit an item, a unit off for the goal weigh_evidence");
    Check(ExplainLimitationCost(tram, ExplainGoal::ArgueClaim) == 1 && ExplainLimitationCost(tram, ExplainGoal::StateLimits) == 0,
          "a limit costs a unit, free for the goal state_limits");

    // The researcher takes a unit off the evidence beat; scarce counterpoint raises the limit.
    const ExplainCaseDef& canal = CaseOf("canal_log");
    Check(ExplainEvidenceCost(canal, ExplainGoal::ArgueClaim, 1) == 0 && ExplainEvidenceCost(canal, ExplainGoal::ArgueClaim, 2) == 1 &&
              ExplainEvidenceCost(canal, ExplainGoal::WeighEvidence, 2) == 1,
          "the researcher's discount and the goal weigh_evidence do not stack");
    Check(ExplainLimitationCost(canal, ExplainGoal::ArgueClaim) == 2 && ExplainLimitationCost(canal, ExplainGoal::StateLimits) == 1,
          "counterpoint scarce: a limit costs 1+1, a unit less for the goal state_limits");
    Check(ExplainClaimCost(canal, ExplainGoal::ArgueClaim, true) == 1 && ExplainClaimCost(canal, ExplainGoal::WeighEvidence, true) == 2,
          "the goal argue_claim makes the claim a unit cheaper where the role does not");

    // Scarce help raises the paragraph; the critic's limit is free.
    const ExplainCaseDef& tea = CaseOf("tea_price");
    Check(ExplainParagraphCost(tea) == 3 && ExplainLimitationCost(tea, ExplainGoal::ArgueClaim) == 0 && ExplainLimitationCost(tea, ExplainGoal::StateLimits) == 0,
          "help scarce: the paragraph costs 1+2; the critic's limit costs 1 less 1");

    // Scarce sources raise each item; the recorder's notes make the paragraph free.
    const ExplainCaseDef& annex = CaseOf("annex_notice");
    Check(ExplainParagraphCost(annex) == 0 && ExplainEvidenceCost(annex, ExplainGoal::ArgueClaim, 1) == 2 && ExplainEvidenceCost(annex, ExplainGoal::ArgueClaim, 2) == 4 &&
              ExplainEvidenceCost(annex, ExplainGoal::WeighEvidence, 1) == 1 && ExplainEvidenceCost(annex, ExplainGoal::WeighEvidence, 2) == 3,
          "sources scarce: an item costs 2, a unit off the beat for the goal; the recorder's paragraph is free");

    // Every price of the current beat can be read at the start, and quoting changes nothing.
    ExplainState state = Harness::Started<ExplainT>(annex, 0, NoSkill);
    const ExplainState before = state;
    const StructureQuote paragraph = QuoteExplainAction(state, ExplainT::Cat(), ExplainT::ParagraphAction());
    const StructureQuote claim = QuoteExplainAction(state, ExplainT::Cat(), ExplainT::ClaimAction(""));
    Check(paragraph.Ok() && paragraph.Units == 0 && paragraph.UnitsAfter == 7, "the paragraph is quoted: free of 7");
    Check(claim.Ok() && claim.Units == 3 && claim.UnitsAfter == 4, "choosing a claim with none named still quotes the unfamiliar price");
    Check(QuoteExplainAction(state, ExplainT::Cat(), ExplainT::EvidenceAction({})).Error == StructureError::WrongBeat, "a later beat is not yet legal");
    Check(state == before, "reading prices changes nothing");
    Harness::Do<ExplainT>(state, ExplainT::ParagraphAction());
    Check(QuoteExplainAction(state, ExplainT::Cat(), ExplainT::ClaimAction("")).Units == 1, "after the paragraph the claim is the practised price");
    Check(QuoteExplainAction(state, ExplainT::Cat(), ExplainT::ParagraphAction()).Error == StructureError::NotNeeded, "one paragraph is enough");
    const ExplainState shown = state;
    Check(ApplyExplainAction(state, ExplainT::Cat(), ExplainT::ParagraphAction()).Error == StructureError::NotNeeded && state == shown, "a second paragraph is refused");
    const ExplainState known = Harness::Started<ExplainT>(annex, 0, FullSkill);
    Check(QuoteExplainAction(known, ExplainT::Cat(), ExplainT::ParagraphAction()).Error == StructureError::NotNeeded, "a practised reader needs no paragraph");

    // The standard policy asks for the paragraph only when it is strictly cheaper.
    Check(ExplainStandardAction(tram, Harness::Started<ExplainT>(tram, 0, NoSkill).Exposure).Kind == ExplainActionKind::ModelParagraph, "the paragraph (1+1) beats reading unaided (3)");
    Check(ExplainStandardAction(annex, Harness::Started<ExplainT>(annex, 0, NoSkill).Exposure).Kind == ExplainActionKind::ModelParagraph, "the recorder's free paragraph beats reading unaided (3)");
    Check(ExplainStandardAction(tea, Harness::Started<ExplainT>(tea, 0, NoSkill).Exposure).Kind == ExplainActionKind::ChooseClaim,
          "the paragraph plus a practised reading (3+1) does not beat reading unaided (3)");
}

void TestUnitsRunOut()
{
    // The poem, unfamiliar: the paragraph (1) and a practised claim (1) leave three; two items leave one; the limit costs one.
    const ExplainCaseDef& tram = CaseOf("tram_poem");
    ExplainState state = Harness::Started<ExplainT>(tram, 0, NoSkill);
    Harness::Do<ExplainT>(state, ExplainT::ClaimAction("c_farewell"));
    Check(state.Exposure.UnitsLeft == 2, "deriving the claim leaves two units");
    Harness::Do<ExplainT>(state, ExplainT::EvidenceAction({"e_bell", "e_empty_seats"}));
    Check(state.Exposure.UnitsLeft == 0, "two items spend the rest");
    const ExplainState spent = state;
    const StructureQuote dear = QuoteExplainAction(state, ExplainT::Cat(), ExplainT::LimitationAction("l_memory"));
    Check(dear.Error == StructureError::UnitsShort && dear.Units == 1 && dear.UnitsAfter == -1, "the quote shows 1 against 0 left");
    Check(ApplyExplainAction(state, ExplainT::Cat(), ExplainT::LimitationAction("l_memory")).Error == StructureError::UnitsShort && state == spent,
          "a refusal for spent time changes nothing");
    // Every beat has a free way forward: stating no limit finishes the exposure, honestly overreaching.
    const ExplainOutcome outcome = Harness::Do<ExplainT>(state, ExplainT::LimitationAction("")).Outcome;
    Check(outcome.Failures == ExplainFailOverreach && outcome.Grade == StructureGrade::Partial, "a supported claim with no limit is right but overreaches");
    // Evidence is always affordable after any claim, so the exposure cannot strand.
    for (const ExplainCaseDef& def : Harness::Cases<ExplainT>())
    {
        for (int goal = 0; goal < ExplainT::Goals; ++goal)
        {
            ExplainState s = Harness::Started<ExplainT>(def, goal, NoSkill);
            Harness::Do<ExplainT>(s, ExplainT::ClaimAction(def.Claims.front().Id));
            Check(QuoteExplainAction(s, ExplainT::Cat(), ExplainT::EvidenceAction({def.Evidence.front().Id})).Ok(), "one piece of evidence is always affordable");
        }
    }
}

void TestFailureKinds()
{
    const ExplainCaseDef& tram = CaseOf("tram_poem");
    const ExplainGoal argue = ExplainGoal::ArgueClaim;

    ExplainOutcome o = PlayAll(tram, argue, "c_farewell", {"e_empty_seats", "e_bell"}, "l_memory");
    Check(o.Failures == 0 && o.Grade == StructureGrade::Sound && o.SupportingItems == 2 && o.PlacedItems == 2 && o.Bounded, "two supporting items and a fitting limit are Sound");
    o = PlayAll(tram, argue, "c_defiance", {"e_bell", "e_sparks"}, "l_sorrow");
    Check(o.Failures == 0 && o.Grade == StructureGrade::Sound, "the other defensible reading is as sound as the first");

    o = PlayAll(tram, argue, "c_complaint", {"e_year"}, "l_one_poem");
    Check(o.Failures == (ExplainFailIndefensible | ExplainFailPadding) && o.Grade == StructureGrade::Unsound && o.Bounded,
          "a reading the sources contradict is indefensible whatever is placed behind it");
    o = PlayAll(tram, argue, "c_defiance", {"e_empty_seats"}, "l_sorrow");
    Check(o.Failures == (ExplainFailUnsupported | ExplainFailPadding) && o.Grade == StructureGrade::Unsound && o.SupportingItems == 0,
          "a defensible claim backed only by evidence for another claim is unsupported");
    o = PlayAll(tram, argue, "c_farewell", {"e_empty_seats"}, "");
    Check(o.Failures == ExplainFailOverreach && o.Grade == StructureGrade::Partial && !o.Bounded, "no limit: right but overreaching");
    o = PlayAll(tram, argue, "c_farewell", {"e_empty_seats"}, "l_sorrow");
    Check(o.Failures == ExplainFailOverreach && !o.Bounded, "a limit that bounds another claim does not bound this one");
    o = PlayAll(tram, argue, "c_farewell", {"e_empty_seats", "e_year"}, "l_memory");
    Check(o.Failures == ExplainFailPadding && o.Grade == StructureGrade::Partial && o.SupportingItems == 1 && o.PlacedItems == 2,
          "evidence beside the point is padding, a wasted unit but not a wrong claim");
    o = PlayAll(tram, argue, "c_complaint", {"e_no_fares"}, "");
    Check(o.Failures == (ExplainFailIndefensible | ExplainFailOverreach | ExplainFailPadding), "failure kinds combine and stay separate");
    o = PlayAll(tram, argue, "c_defiance", {"e_bell"}, "l_one_poem");
    Check(o.Failures == 0, "evidence that supports two readings serves either");

    // The role and the goal do not change the grade, only the price.
    for (const ExplainGoal goal : {ExplainGoal::ArgueClaim, ExplainGoal::WeighEvidence, ExplainGoal::StateLimits})
        Check(PlayAll(CaseOf("canal_log"), goal, "c_flood", {"e_gauge"}, "l_one_log").Failures == 0, "the goal never changes what is Sound");
}

void TestRefusals()
{
    const ExplainCaseDef& tram = CaseOf("tram_poem");
    ExplainState state;
    Check(ApplyExplainAction(state, ExplainT::Cat(), ExplainT::ClaimAction("c_farewell")).Error == StructureError::NotActive, "no exposure, no action");
    Check(QuoteExplainAction(state, ExplainT::Cat(), ExplainT::ParagraphAction()).Error == StructureError::NotActive, "no exposure, no quote");
    Check(CancelExplainExposure(state) == StructureError::NotActive, "nothing to cancel");
    Check(BeginExplainExposure(state, ExplainT::Cat(), "explain_case.xi_nope", ExplainGoal::ArgueClaim, 0).Error == StructureError::UnknownCase, "unknown case");
    Check(BeginExplainExposure(state, ExplainT::Cat(), tram.Id, ExplainGoal::ArgueClaim, -1).Error == StructureError::InvalidSkill, "negative skill");
    Check(BeginExplainExposure(state, ExplainT::Cat(), tram.Id, static_cast<ExplainGoal>(9), 0).Error == StructureError::InvalidState, "goal out of range");
    Check(state == ExplainState{}, "refused begins change nothing");

    state = Harness::Started<ExplainT>(tram, 0, FullSkill);
    const ExplainState begun = state;
    Check(BeginExplainExposure(state, ExplainT::Cat(), tram.Id, ExplainGoal::ArgueClaim, 0).Error == StructureError::Busy && state == begun, "one exposure at a time");
    for (const ExplainAction& wrong : {ExplainT::EvidenceAction({"e_bell"}), ExplainT::LimitationAction("l_memory")})
        Check(ApplyExplainAction(state, ExplainT::Cat(), wrong).Error == StructureError::WrongBeat && state == begun, "an action of another beat is refused");
    Check(ApplyExplainAction(state, ExplainT::Cat(), ExplainT::ClaimAction("")).Error == StructureError::BadChoice && state == begun, "no claim named is refused");
    Check(ApplyExplainAction(state, ExplainT::Cat(), ExplainT::ClaimAction("c_nope")).Error == StructureError::UnknownItem && state == begun, "an unknown claim is refused");
    Harness::Do<ExplainT>(state, ExplainT::ClaimAction("c_farewell"));
    const ExplainState chosen = state;
    Check(ApplyExplainAction(state, ExplainT::Cat(), ExplainT::ClaimAction("c_defiance")).Error == StructureError::WrongBeat && state == chosen, "the claim is chosen once");
    Check(ApplyExplainAction(state, ExplainT::Cat(), ExplainT::EvidenceAction({})).Error == StructureError::BadChoice && state == chosen, "no evidence is refused");
    Check(ApplyExplainAction(state, ExplainT::Cat(), ExplainT::EvidenceAction({"e_nope"})).Error == StructureError::UnknownItem && state == chosen, "unknown evidence is refused");
    Check(ApplyExplainAction(state, ExplainT::Cat(), ExplainT::EvidenceAction({"e_bell", "e_bell"})).Error == StructureError::BadChoice && state == chosen, "a repeated item is refused");
    Check(ApplyExplainAction(state, ExplainT::Cat(), ExplainT::EvidenceAction({"e_bell", "e_sparks", "e_year"})).Error == StructureError::BadChoice && state == chosen,
          "more items than slots is refused");
    Harness::Do<ExplainT>(state, ExplainT::EvidenceAction({"e_sparks", "e_bell"}));
    Check(state.Exposure.EvidenceIds == std::vector<std::string>({"e_bell", "e_sparks"}), "the evidence is kept sorted");
    const ExplainState placed = state;
    Check(ApplyExplainAction(state, ExplainT::Cat(), ExplainT::LimitationAction("l_nope")).Error == StructureError::UnknownItem && state == placed, "an unknown limitation is refused");
    Check(ApplyExplainAction(state, ExplainT::Cat(), ExplainT::EvidenceAction({"e_bell"})).Error == StructureError::WrongBeat && state == placed, "the evidence is placed once");

    // The last decision ends the exposure once; a second finds nothing to end.
    Check(Harness::Do<ExplainT>(state, ExplainT::LimitationAction("l_memory")).Finished, "the limit finishes the exposure");
    const ExplainState finished = state;
    Check(ApplyExplainAction(state, ExplainT::Cat(), ExplainT::LimitationAction("l_memory")).Error == StructureError::NotActive && state == finished, "an outcome is applied exactly once");
    Check(state.FindFamily(tram.FamilyId)->Exposures == 1, "one exposure was recorded");

    // Cancel: no outcome, no mastery change, but the case stays seen.
    state = Harness::Started<ExplainT>(tram, 0, FullSkill);
    Check(CancelExplainExposure(state) == StructureError::None && !state.Exposure.Active(), "an exposure can be abandoned");
    const StructureFamilyRecord* record = state.FindFamily(tram.FamilyId);
    Check(record != nullptr && record->Exposures == 0 && record->SoundExposures == 0 && record->Seen.size() == 1, "abandoning records no outcome but the case was seen");
    Check(StructureErrorCode(StructureError::UnitsShort) == "structure.units_short" && StructureErrorCode(StructureError::None).empty(), "stable error codes");
}
} // namespace

void TestExplain()
{
    Harness::AllGeneric<ExplainT>();
    TestCatalogueShape();
    TestNoHiddenRightAnswer();
    TestCorrectAnswerIsNeverRefusedForAStat();
    TestCostsShowBeforeCommit();
    TestUnitsRunOut();
    TestFailureKinds();
    TestRefusals();
}
