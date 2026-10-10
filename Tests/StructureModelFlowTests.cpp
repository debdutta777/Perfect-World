// ROADMAP M2-08: case selection, mastery and routine abbreviation, the save text and
// random sequences for the "model a system" runner. The catalogue is the real Data/Content.
#include "StructureModelTestSupport.h"

#include <map>
#include <set>

using namespace PerfectWorld;

namespace
{
/** Presents the next selected case, plays it with the standard policy and returns its ID. */
std::string PlayNext(StructureModelState& state, const ModelCatalogue& catalogue, std::uint64_t seed, int index)
{
    const ModelSelection selection = SelectModelCase(catalogue, state, ModelTestFamily, seed, static_cast<GameInstant>(index) * 60);
    Check(selection.Ok() && selection.Candidates >= 1, "a case is selected");
    if (const ModelFamilyRecord* record = state.FindFamily(ModelTestFamily))
    {
        const std::size_t window = std::min(ModelRecentKept, catalogue.Families.front().Cases.size() - 1);
        for (std::size_t i = 0; i < std::min(window, record->Recent.size()); ++i)
            Check(selection.CaseId != record->Recent[record->Recent.size() - 1 - i], "a recent case is never offered again");
    }
    const ModelGoal goal = AllGoals[static_cast<std::size_t>(index) % AllGoals.size()];
    Check(BeginModelExposure(state, catalogue, selection.CaseId, goal, index % 2 == 0 ? NoSkill : FullSkill).Ok(), "the case begins");
    (void)PlayStandard(state);
    return selection.CaseId;
}

std::vector<std::string> PlaySequence(std::uint64_t seed, int count)
{
    StructureModelState state;
    std::vector<std::string> order;
    for (int i = 0; i < count; ++i)
        order.push_back(PlayNext(state, Catalogue(), seed, i));
    return order;
}
} // namespace

void TestSelection()
{
    const ModelFamilyDef& family = Catalogue().Families.front();
    const StructureModelState fresh;
    const ModelSelection a = SelectModelCase(Catalogue(), fresh, ModelTestFamily, 42, 100);
    const ModelSelection b = SelectModelCase(Catalogue(), fresh, ModelTestFamily, 42, 100);
    Check(a.Ok() && a.CaseId == b.CaseId && a.FromUnseen && a.Candidates == family.Cases.size(), "selection is a pure function of its inputs");
    Check(fresh == StructureModelState{}, "selecting changes nothing");
    Check(SelectModelCase(Catalogue(), fresh, "problem_family.nope", 42, 100).Error == ModelError::UnknownFamily, "unknown family");

    // Deterministic across runs, different across seeds, and unseen cases come first.
    const std::vector<std::string> first = PlaySequence(42, 40);
    Check(first == PlaySequence(42, 40), "the same seed gives the same sequence");
    Check(first != PlaySequence(43, 40), "another seed gives another sequence");
    const std::set<std::string> opening(first.begin(), first.begin() + static_cast<std::ptrdiff_t>(family.Cases.size()));
    Check(opening.size() == family.Cases.size(), "the first exposures meet every case once before any repeats");

    // Over a long run every case keeps coming back, and none starves.
    StructureModelState state;
    std::map<std::string, int> counts;
    for (int i = 0; i < 400; ++i)
        ++counts[PlayNext(state, Catalogue(), 7, i)];
    Check(counts.size() == family.Cases.size(), "every case is offered");
    for (const auto& [id, count] : counts)
        Check(count >= 25 && count <= 80, "no case is starved or dominates (400 exposures over 8 cases)");
    Check(!SelectModelCase(Catalogue(), state, ModelTestFamily, 7, 99999).FromUnseen, "once every case is seen the pool is the eligible ones");
    Check(SelectModelCase(Catalogue(), state, ModelTestFamily, 7, 99999).Candidates == family.Cases.size() - ModelRecentKept,
          "the last three shown are held back");

    // A small family leaves at least one choice: two cases alternate, one case repeats.
    ModelCatalogue small = Catalogue();
    small.Families.front().Cases.resize(2);
    StructureModelState pair;
    std::string previous;
    std::set<std::string> seen;
    for (int i = 0; i < 12; ++i)
    {
        const std::string id = PlayNext(pair, small, 5, i);
        Check(id != previous, "two cases alternate");
        seen.insert(id);
        previous = id;
    }
    Check(seen.size() == 2, "both cases of the pair appear");
    small.Families.front().Cases.resize(1);
    StructureModelState single;
    for (int i = 0; i < 3; ++i)
        Check(PlayNext(single, small, 5, i) == small.Families.front().Cases.front().Id, "a family of one case repeats it");
}

void TestMasteryAndRoutine()
{
    const ModelFamilyDef& family = Catalogue().Families.front();
    const ModelCaseDef& a = family.Cases[0];
    const ModelCaseDef& b = family.Cases[1];
    const ModelCaseDef& c = family.Cases[2];
    StructureModelState state;
    const auto play = [&](const ModelCaseDef& def) {
        Check(BeginModelExposure(state, Catalogue(), def.Id, ModelGoal::UnderstandModel, FullSkill).Ok(), "begins");
        return PlayStandard(state);
    };

    // Four sound exposures of one case do not master a family: the player has met one case.
    for (int i = 0; i < 4; ++i)
        Check(!play(a).BecameSecure, "not secure yet");
    const ModelFamilyRecord* record = state.FindFamily(ModelTestFamily);
    Check(record->SoundExposures == 4 && record->Seen.size() == 1 && !record->Secure, "four sound exposures on one case are not mastery");
    const StructureModelState unsecure = state;
    Check(ResolveModelRoutine(state, Catalogue(), ModelTestFamily).Error == ModelError::NotSecure && state == unsecure,
          "routine is refused until the family is secure");
    Check(!ModelFamilySecure(state, ModelTestFamily), "the family is not secure");
    Check(!play(b).BecameSecure && !state.FindFamily(ModelTestFamily)->Secure, "two cases are not enough");
    Check(play(c).BecameSecure && ModelFamilySecure(state, ModelTestFamily), "the third case met with four sound exposures makes it secure");

    // A failed foreground exposure counts as an exposure but not a sound one, and mastery stays.
    Check(BeginModelExposure(state, Catalogue(), a.Id, ModelGoal::UnderstandModel, FullSkill).Ok(), "begins");
    Do(state, CommitAction({SoundLinks(a).front()}));
    Do(state, InvestigateAction(ModelInvestigation::AcceptReadings));
    const ModelOutcome failed = Do(state, ClaimAction(ModelClaim::Estimate, true)).Outcome;
    Check(failed.Grade == ModelGrade::Unsound && !failed.BecameSecure, "a wrong model is unsound");
    record = state.FindFamily(ModelTestFamily);
    Check(record->Exposures == 7 && record->SoundExposures == 6 && record->Secure, "a failure adds an exposure, no sound one, and mastery stays");

    // Routine: normal learning, no manual play, no new case, equal to the standard policy's result.
    const StructureModelState before = state;
    const ModelRoutineResult routine = ResolveModelRoutine(state, Catalogue(), ModelTestFamily);
    Check(routine.Ok() && routine.Outcome.Grade == ModelGrade::Sound && routine.Outcome.Failures == 0, "routine resolves soundly");
    Check(!routine.Outcome.Foreground && routine.Outcome.CaseId.empty() && routine.Outcome.UnitsSpent == 0 && !routine.Outcome.BecameSecure,
          "routine plays nothing and presents no case");
    Check(routine.Outcome.LearningPermille == ModelNormalLearningPermille, "routine earns the full normal value");
    const ModelFamilyRecord& after = *state.FindFamily(ModelTestFamily);
    const ModelFamilyRecord& was = *before.FindFamily(ModelTestFamily);
    Check(after.Seen == was.Seen && after.Recent == was.Recent, "routine leaves the cases seen and recent alone");
    Check(after.Exposures == was.Exposures + 1 && after.SoundExposures == was.SoundExposures && after.Secure, "it is an exposure but not a sound one");
    for (const ModelCaseDef& def : family.Cases)
    {
        for (const ModelGoal goal : AllGoals)
        {
            StructureModelState fg = Started(def, goal, FullSkill);
            const ModelOutcome played = PlayStandard(fg);
            Check(played.Grade == routine.Outcome.Grade && played.Failures == routine.Outcome.Failures &&
                      played.LearningPermille == routine.Outcome.LearningPermille && played.Foreground && !routine.Outcome.Foreground,
                  "routine equals foreground play under the standard policy in everything but having been played");
        }
    }
    for (int i = 0; i < 100; ++i)
        Check(ResolveModelRoutine(state, Catalogue(), ModelTestFamily).Ok(), "routine can be used every period");
    Check(state.FindFamily(ModelTestFamily)->SoundExposures == was.SoundExposures && state.FindFamily(ModelTestFamily)->Exposures == was.Exposures + 101,
          "routine never counts as sound practice and never pays more than normal");
    Check(IsStructureModelStateValid(state), "the state stays valid");

    // Not while an exposure runs.
    Check(BeginModelExposure(state, Catalogue(), b.Id, ModelGoal::UnderstandModel, FullSkill).Ok(), "begins");
    const StructureModelState running = state;
    Check(ResolveModelRoutine(state, Catalogue(), ModelTestFamily).Error == ModelError::Busy && state == running, "no routine during an exposure");
    Check(CancelModelExposure(state) == ModelError::None, "abandoned");

    // Mastery arrives with the fourth sound exposure when four different cases were met by selection.
    StructureModelState chosen;
    for (int i = 0; i < 4; ++i)
    {
        (void)PlayNext(chosen, Catalogue(), 11, i);
        Check(ModelFamilySecure(chosen, ModelTestFamily) == (i == 3), "secure exactly at the fourth sound exposure");
    }
}

namespace
{
std::string Mutated(const std::string& text, const std::string& from, const std::string& to)
{
    const std::size_t at = text.find(from);
    Check(at != std::string::npos, "the mutation target exists in the text");
    std::string out = text;
    if (at != std::string::npos)
        out.replace(at, from.size(), to);
    return out;
}

void CheckSaveContinues(const StructureModelState& state, const std::vector<ModelAction>& rest)
{
    const std::string text = SerialiseStructureModel(state);
    Check(!text.empty(), "a valid state serialises");
    const StructureModelParse parsed = DeserialiseStructureModel(text);
    Check(parsed.Ok() && parsed.State == state && SerialiseStructureModel(parsed.State) == text, "the text restores the same state");
    StructureModelState original = state;
    StructureModelState restored = parsed.State;
    for (const ModelAction& action : rest)
    {
        const ModelStepResult x = ApplyModelAction(original, Catalogue(), action);
        const ModelStepResult y = ApplyModelAction(restored, Catalogue(), action);
        Check(x.Ok() && y.Ok() && x.Charged == y.Charged && x.Finished == y.Finished && x.Outcome == y.Outcome, "a restored exposure plays on identically");
        Check(original == restored, "and ends in the same state");
    }
    Check(!original.Exposure.Active() && SerialiseStructureModel(original) == SerialiseStructureModel(restored), "the exposure finished once");
    const ModelStepResult again = ApplyModelAction(restored, Catalogue(), rest.back());
    Check(again.Error == ModelError::NotActive, "it cannot finish a second time");
}

void CheckEveryTruncationRefused(const std::string& text)
{
    for (std::size_t length = 0; length < text.size(); ++length)
        Check(!DeserialiseStructureModel(std::string_view(text).substr(0, length)).Ok(), "every truncation is refused");
    Check(!DeserialiseStructureModel(text + "x").Ok() && !DeserialiseStructureModel(text + "\n").Ok() && !DeserialiseStructureModel(text + text).Ok(),
          "trailing text is refused");
}
} // namespace

void TestText()
{
    const StructureModelState idle;
    const std::string idleText = SerialiseStructureModel(idle);
    Check(idleText == "pw.structure_model/1\nfamilies=0\nexposure=-\nlinks=-\nend\n", "the idle text is stable");
    Check(DeserialiseStructureModel(idleText).Ok() && DeserialiseStructureModel(idleText).State == idle, "the idle text restores");

    // One finished exposure of the tank, then the roof begun unfamiliar: a save at every beat.
    const ModelCaseDef& tank = CaseOf("water_tank_level");
    const ModelCaseDef& roof = CaseOf("roof_rainwater_tank");
    StructureModelState state = Started(tank, ModelGoal::UnderstandModel, FullSkill);
    (void)PlayStandard(state);
    Check(BeginModelExposure(state, Catalogue(), roof.Id, ModelGoal::UnderstandModel, NoSkill).Ok(), "the roof begins");
    std::vector<std::string> texts;
    texts.push_back(SerialiseStructureModel(state));
    const std::vector<ModelAction> script{ExampleAction(), CommitAction(SoundLinks(roof)), InvestigateAction(ModelInvestigation::AcceptReadings),
                                          ClaimAction(ModelClaim::Estimate, true)};
    for (std::size_t i = 0; i < script.size(); ++i)
    {
        CheckSaveContinues(state, std::vector<ModelAction>(script.begin() + static_cast<std::ptrdiff_t>(i), script.end()));
        if (i + 1 < script.size())
        {
            Do(state, script[i]);
            texts.push_back(SerialiseStructureModel(state));
        }
    }
    Check(texts[0] ==
              "pw.structure_model/1\nfamilies=1\nfamily=problem_family.xi_stock_and_flow,1,1,0\n"
              "seen=problem_case.xi_roof_rainwater_tank,problem_case.xi_water_tank_level\n"
              "recent=problem_case.xi_water_tank_level,problem_case.xi_roof_rainwater_tank\n"
              "exposure=problem_family.xi_stock_and_flow,problem_case.xi_roof_rainwater_tank,understand_model,build_model,7,7,0,0,none\n"
              "links=-\nend\n",
          "the mid-exposure text is stable");
    Check(texts[1].find("build_model,7,4,0,1,none") != std::string::npos, "the example (1+2, help is scarce) is in the save");
    Check(texts[3].find("present_claim,7,") != std::string::npos && texts[3].find("accept_readings") != std::string::npos, "the investigation is in the save");

    // A state with two families, and every truncation of every text.
    StructureModelState two = state;
    ModelFamilyRecord other;
    other.FamilyId = "problem_family.a_other";
    other.Seen = {"problem_case.a_one"};
    other.Recent = {"problem_case.a_one"};
    other.Exposures = 1;
    two.Families.insert(two.Families.begin(), other);
    Check(IsStructureModelStateValid(two) && DeserialiseStructureModel(SerialiseStructureModel(two)).State == two, "two families round-trip");
    texts.push_back(SerialiseStructureModel(two));
    texts.push_back(idleText);
    for (const std::string& text : texts)
        CheckEveryTruncationRefused(text);

    // Malformed and inconsistent texts are refused, never half-read.
    const std::string base = texts[0];
    const std::string family = "family=problem_family.xi_stock_and_flow,1,1,0";
    const std::string recent = "recent=problem_case.xi_water_tank_level,problem_case.xi_roof_rainwater_tank";
    const std::string exposure = "understand_model,build_model,7,7,0,0,none";
    const std::vector<std::pair<std::string, std::string>> bad{
        {"pw.structure_model/1", "pw.structure_model/2"}, {"families=1", "families=2"}, {"families=1", "families=01"}, {"families=1", "families=65"},
        {family, "family=problem_family.xi_stock_and_flow,01,1,0"}, {family, "family=problem_family.xi_stock_and_flow,1,2,0"},
        {family, "family=problem_family.xi_stock_and_flow,1,1,1"}, {family, "family=problem_family.xi_stock_and_flow,1,1,2"},
        {family, "family=Problem_Family,1,1,0"}, {family, "family=problem_family.xi_stock_and_flow,1,1"}, {family, "family=problem_family.xi_stock_and_flow,-1,1,0"},
        {"seen=problem_case.xi_roof_rainwater_tank,problem_case.xi_water_tank_level", "seen=problem_case.xi_water_tank_level,problem_case.xi_roof_rainwater_tank"},
        {"seen=problem_case.xi_roof_rainwater_tank,problem_case.xi_water_tank_level", "seen=problem_case.xi_roof_rainwater_tank,problem_case.xi_roof_rainwater_tank"},
        {recent, "recent=problem_case.xi_water_tank_level,problem_case.xi_other"}, {recent, "recent=problem_case.xi_roof_rainwater_tank,problem_case.xi_water_tank_level"},
        {recent, "recent=-"}, {recent, "recent=problem_case.xi_water_tank_level,,problem_case.xi_roof_rainwater_tank"},
        {exposure, "bogus,build_model,7,7,0,0,none"}, {exposure, "understand_model,bogus,7,7,0,0,none"}, {exposure, "understand_model,resolve_discrepancy,7,7,0,0,none"},
        {exposure, "understand_model,present_claim,7,7,0,0,check_times"}, {exposure, "understand_model,build_model,7,8,0,0,none"},
        {exposure, "understand_model,build_model,8,8,0,0,none"}, {exposure, "understand_model,build_model,0,0,0,0,none"},
        {exposure, "understand_model,build_model,7,7,1,1,none"}, {exposure, "understand_model,build_model,7,7,2,0,none"},
        {exposure, "understand_model,build_model,7,7,0,0,check_times"}, {exposure, "understand_model,build_model,7,7,0,0"},
        {exposure, "understand_model,build_model,07,7,0,0,none"}, {"links=-", "links=l_flow_adds"}, {"links=-", "links="},
        {"exposure=problem_family.xi_stock_and_flow,problem_case.xi_roof_rainwater_tank", "exposure=problem_family.xi_stock_and_flow,problem_case.xi_water_tank_level"},
        {"exposure=problem_family.xi_stock_and_flow,", "exposure=problem_family.other,"}, {"end\n", "fin\n"}, {"end\n", "end\nend\n"}, {"\n", "\r\n"}};
    for (const auto& [from, to] : bad)
        Check(!DeserialiseStructureModel(Mutated(base, from, to)).Ok(), "a malformed or inconsistent text is refused");
    Check(DeserialiseStructureModel(base).Ok(), "the unmutated text parses");
    // An inconsistent state does not serialise at all.
    StructureModelState broken = state;
    broken.Families.front().Secure = true;
    Check(!IsStructureModelStateValid(broken) && SerialiseStructureModel(broken).empty(), "an inconsistent state serialises to nothing");
}

namespace
{
std::string RandomRun(std::uint64_t seed, int steps, int& finished, int& refused, int& becameSecure)
{
    ModelRng rng{seed};
    const ModelFamilyDef& family = Catalogue().Families.front();
    StructureModelState state;
    std::string transcript;
    std::int32_t charged = 0;
    bool wasSecure = false;
    std::uint32_t lastExposures = 0;
    for (int step = 0; step < steps; ++step)
    {
        const StructureModelState before = state;
        ModelError error = ModelError::None;
        if (!state.Exposure.Active())
        {
            charged = 0;
            if (rng.Below(100) < 15)
                error = ResolveModelRoutine(state, Catalogue(), rng.Below(10) == 0 ? "problem_family.nope" : ModelTestFamily).Error;
            else
            {
                std::string id = SelectModelCase(Catalogue(), state, ModelTestFamily, 99, step).CaseId;
                if (rng.Below(10) < 3)
                    id = family.Cases[rng.Below(family.Cases.size())].Id;
                const std::int32_t skills[] = {-5, 0, 112000, 126000, 141000, FullSkill};
                error = BeginModelExposure(state, Catalogue(), id, static_cast<ModelGoal>(rng.Below(4)), skills[rng.Below(6)]).Error;
            }
        }
        else if (rng.Below(100) < 8)
            error = rng.Below(2) == 0 ? CancelModelExposure(state) : BeginModelExposure(state, Catalogue(), family.Cases[0].Id, ModelGoal::UnderstandModel, 0).Error;
        else
        {
            const ModelCaseDef& def = *Catalogue().FindCase(state.Exposure.CaseId);
            ModelAction action;
            action.Kind = rng.Below(100) < 75 ? static_cast<ModelActionKind>(state.Exposure.Beat == ModelBeat::BuildModel ? rng.Below(2) : state.Exposure.Beat == ModelBeat::ResolveDiscrepancy ? 2 : 3)
                                              : static_cast<ModelActionKind>(rng.Below(4));
            for (const ModelLink& link : def.Links)
                if (rng.Below(100) < 55)
                    action.Links.push_back(link.Id);
            if (rng.Below(20) == 0)
                action.Links.push_back(rng.Below(2) == 0 ? "l_nope" : (action.Links.empty() ? "l_none" : action.Links.front()));
            if (rng.Below(100) < 40)
                action.Links = SoundLinks(def); // a skilled player is common enough to master the family
            action.Investigation = rng.Below(100) < 40 ? ModelMatchingInvestigation(def.Fault) : static_cast<ModelInvestigation>(rng.Below(6));
            action.Claim = rng.Below(100) < 50 ? ModelClaim::Estimate : static_cast<ModelClaim>(rng.Below(4));
            action.Explain = rng.Below(2) == 0;
            const ModelQuote quote = QuoteModelAction(state, Catalogue(), action);
            const ModelStepResult result = ApplyModelAction(state, Catalogue(), action);
            const bool priceOnly = action.Kind == ModelActionKind::CommitModel && action.Links.empty() && quote.Ok();
            Check(result.Error == (priceOnly ? ModelError::BadModel : quote.Error), "the quote and the application agree on legality");
            error = result.Error;
            if (result.Ok())
            {
                Check(result.Charged == quote.Units, "the charge is the quote");
                charged += result.Charged;
                if (result.Finished)
                {
                    ++finished;
                    const ModelOutcome& o = result.Outcome;
                    const bool unsound = (o.Failures & (ModelFailWrongModel | ModelFailUnsupported)) != 0;
                    Check(o.Grade == (unsound ? ModelGrade::Unsound : o.Failures == 0 ? ModelGrade::Sound : ModelGrade::Partial), "the grade follows the failures");
                    Check(o.LearningPermille == ModelNormalLearningPermille && o.Foreground && o.UnitsSpent == charged &&
                              o.UnitsSpent <= ModelWorkUnits,
                          "learning is normal and the units add up");
                    becameSecure += o.BecameSecure ? 1 : 0;
                }
                else
                    Check(state.Exposure.UnitsTotal - state.Exposure.UnitsLeft == charged, "units spent equal the charges");
            }
        }
        transcript += std::to_string(static_cast<int>(error)) + ",";
        if (error != ModelError::None)
        {
            ++refused;
            Check(state == before, "a refusal changes nothing");
        }
        Check(IsStructureModelStateValid(state), "the state stays valid through random play");
        const std::string text = SerialiseStructureModel(state);
        const StructureModelParse parsed = DeserialiseStructureModel(text);
        Check(!text.empty() && parsed.Ok() && parsed.State == state, "every state round-trips through its text");
        if (const ModelFamilyRecord* record = state.FindFamily(ModelTestFamily))
        {
            Check(!wasSecure || record->Secure, "mastery is never lost");
            Check(record->Exposures >= lastExposures, "exposures never fall");
            wasSecure = record->Secure;
            lastExposures = record->Exposures;
        }
    }
    return transcript + SerialiseStructureModel(state);
}
} // namespace

void TestRandomSequences()
{
    int finished = 0, refused = 0, secure = 0;
    const std::string first = RandomRun(1234, 6000, finished, refused, secure);
    Check(finished > 150 && refused > 500 && secure == 1, "the random run finishes, refuses and masters the family once");
    int f2 = 0, r2 = 0, s2 = 0;
    Check(RandomRun(1234, 6000, f2, r2, s2) == first && f2 == finished, "the same seed gives the same run");
    Check(RandomRun(4321, 6000, f2, r2, s2) != first, "another seed gives another run");
}
