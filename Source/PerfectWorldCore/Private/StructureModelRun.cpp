#include "StructureModel.h"

#include <algorithm>
#include <limits>

// The exposure runner for "model a system" (ROADMAP M2-08): present a case, quote
// and apply decisions, grade the outcome, abbreviate by routine. The rules are
// documented in Public/StructureModel.h. Every entry point works on a copy and
// replaces the state only on success, so a refusal changes nothing.
namespace PerfectWorld
{
namespace
{
ModelFamilyRecord& ModelEnsureFamily(StructureModelState& state, std::string_view familyId)
{
    const auto it = std::lower_bound(state.Families.begin(), state.Families.end(), familyId,
                                     [](const ModelFamilyRecord& record, std::string_view key) { return record.FamilyId < key; });
    if (it != state.Families.end() && it->FamilyId == familyId)
        return *it;
    ModelFamilyRecord record;
    record.FamilyId = std::string(familyId);
    return *state.Families.insert(it, std::move(record));
}

ModelFamilyRecord* ModelFindFamilyMutable(StructureModelState& state, std::string_view familyId)
{
    const auto it = std::find_if(state.Families.begin(), state.Families.end(),
                                 [&](const ModelFamilyRecord& record) { return record.FamilyId == familyId; });
    return it == state.Families.end() ? nullptr : &*it;
}

bool ModelBeatMatches(ModelBeat beat, ModelActionKind kind)
{
    switch (kind)
    {
    case ModelActionKind::WorkedExample:
    case ModelActionKind::CommitModel:
        return beat == ModelBeat::BuildModel;
    case ModelActionKind::Investigate:
        return beat == ModelBeat::ResolveDiscrepancy;
    case ModelActionKind::PresentClaim:
        return beat == ModelBeat::PresentClaim;
    }
    return false;
}

/** The sound links the player chose, left out, and wrongly added. */
void ModelCompareLinks(const ModelCaseDef& def, const std::vector<std::string>& chosen, std::int32_t& missing, std::int32_t& extra)
{
    missing = 0;
    extra = 0;
    for (const ModelLink& link : def.Links)
    {
        const bool picked = std::find(chosen.begin(), chosen.end(), link.Id) != chosen.end();
        if (link.Sound && !picked)
            ++missing;
        if (!link.Sound && picked)
            ++extra;
    }
}

ModelOutcome ModelEvaluate(const ModelCaseDef& def, const ModelExposure& exposure, const ModelAction& claim)
{
    ModelOutcome outcome;
    outcome.FamilyId = def.FamilyId;
    outcome.CaseId = def.Id;
    ModelCompareLinks(def, exposure.Links, outcome.MissingLinks, outcome.ExtraLinks);
    const bool modelSound = outcome.MissingLinks == 0 && outcome.ExtraLinks == 0;
    const ModelInvestigation matching = ModelMatchingInvestigation(def.Fault);
    const bool resolved = def.Fault == ModelFault::None || exposure.Investigation == matching;
    const bool hedged = claim.Claim != ModelClaim::Estimate;
    const bool foundNothing = exposure.Investigation != ModelInvestigation::AcceptReadings &&
                              !(def.Fault != ModelFault::None && exposure.Investigation == matching);

    if (!modelSound)
        outcome.Failures |= ModelFailWrongModel;
    if (!hedged && !resolved)
        outcome.Failures |= ModelFailUnsupported;
    if (foundNothing || (hedged && resolved && modelSound && def.Role != ModelRole::Critic))
        outcome.Failures |= ModelFailWastedTime;
    if (ModelExplanationOwed(def, exposure.Goal) && !claim.Explain)
        outcome.Failures |= ModelFailUnclear;

    if ((outcome.Failures & (ModelFailWrongModel | ModelFailUnsupported)) != 0)
        outcome.Grade = ModelGrade::Unsound;
    else
        outcome.Grade = outcome.Failures == 0 ? ModelGrade::Sound : ModelGrade::Partial;
    return outcome;
}

constexpr std::uint32_t ModelMaxCount = std::numeric_limits<std::uint32_t>::max();

/** Records a finished exposure against its family. */
void ModelRecordFinish(ModelFamilyRecord& record, bool foreground, ModelGrade grade, ModelOutcome& outcome)
{
    ++record.Exposures;
    if (foreground && grade == ModelGrade::Sound)
        ++record.SoundExposures;
    const bool secure = record.SoundExposures >= ModelSecureSoundExposures && record.Seen.size() >= ModelSecureDistinctCases;
    outcome.BecameSecure = secure && !record.Secure;
    record.Secure = record.Secure || secure;
}
} // namespace

ModelBeginResult BeginModelExposure(StructureModelState& state, const ModelCatalogue& catalogue, std::string_view caseId, ModelGoal goal,
                                    std::int32_t skillMilli)
{
    ModelBeginResult result;
    if (state.Exposure.Active())
    {
        result.Error = ModelError::Busy;
        return result;
    }
    const ModelCaseDef* def = catalogue.FindCase(caseId);
    if (def == nullptr)
    {
        result.Error = ModelError::UnknownCase;
        return result;
    }
    if (skillMilli < 0)
    {
        result.Error = ModelError::InvalidSkill;
        return result;
    }
    if (static_cast<std::size_t>(goal) >= ModelGoalNames.size() || !IsStructureModelStateValid(state))
    {
        result.Error = ModelError::InvalidState;
        return result;
    }

    StructureModelState next = state;
    if (next.FindFamily(def->FamilyId) == nullptr && next.Families.size() >= ModelMaxFamilies)
    {
        result.Error = ModelError::InvalidState;
        return result;
    }
    ModelFamilyRecord& record = ModelEnsureFamily(next, def->FamilyId);
    const auto seen = std::lower_bound(record.Seen.begin(), record.Seen.end(), def->Id);
    if (seen == record.Seen.end() || *seen != def->Id)
    {
        if (record.Seen.size() >= ModelMaxSeen)
        {
            result.Error = ModelError::InvalidState;
            return result;
        }
        record.Seen.insert(seen, def->Id);
    }
    record.Recent.push_back(def->Id);
    if (record.Recent.size() > ModelRecentKept)
        record.Recent.erase(record.Recent.begin());

    ModelExposure& exposure = next.Exposure;
    exposure.FamilyId = def->FamilyId;
    exposure.CaseId = def->Id;
    exposure.Goal = goal;
    exposure.Beat = ModelBeat::BuildModel;
    exposure.UnitsTotal = ModelBudgetUnits(def->Scarce);
    exposure.UnitsLeft = exposure.UnitsTotal;
    exposure.MethodKnown = skillMilli >= def->MethodKnownAtMilli;
    result.Units = exposure.UnitsTotal;
    result.MethodKnown = exposure.MethodKnown;
    state = std::move(next);
    return result;
}

ModelError CancelModelExposure(StructureModelState& state)
{
    if (!state.Exposure.Active())
        return ModelError::NotActive;
    state.Exposure = ModelExposure{};
    return ModelError::None;
}

ModelQuote QuoteModelAction(const StructureModelState& state, const ModelCatalogue& catalogue, const ModelAction& action)
{
    ModelQuote quote;
    const ModelExposure& exposure = state.Exposure;
    if (!exposure.Active())
    {
        quote.Error = ModelError::NotActive;
        return quote;
    }
    const ModelCaseDef* def = catalogue.FindCase(exposure.CaseId);
    if (def == nullptr)
    {
        quote.Error = ModelError::UnknownCase;
        return quote;
    }
    if (!ModelBeatMatches(exposure.Beat, action.Kind))
    {
        quote.Error = ModelError::WrongBeat;
        return quote;
    }

    std::int32_t cost = 0;
    switch (action.Kind)
    {
    case ModelActionKind::WorkedExample:
        if (exposure.MethodKnown || exposure.ExampleTaken)
            quote.Error = ModelError::ExampleNotNeeded;
        cost = ModelExampleCost(*def);
        break;
    case ModelActionKind::CommitModel:
        cost = ModelCommitCost(*def, exposure.Goal, exposure.MethodKnown || exposure.ExampleTaken);
        if (action.Links.size() > ModelMaxLinks)
            quote.Error = ModelError::BadModel;
        for (std::size_t i = 0; i < action.Links.size() && quote.Error == ModelError::None; ++i)
        {
            const std::string& id = action.Links[i];
            if (std::none_of(def->Links.begin(), def->Links.end(), [&](const ModelLink& link) { return link.Id == id; }))
                quote.Error = ModelError::UnknownLink;
            else if (std::find(action.Links.begin(), action.Links.begin() + static_cast<std::ptrdiff_t>(i), id) !=
                     action.Links.begin() + static_cast<std::ptrdiff_t>(i))
                quote.Error = ModelError::BadModel;
        }
        break;
    case ModelActionKind::Investigate:
        if (action.Investigation == ModelInvestigation::None || static_cast<std::size_t>(action.Investigation) >= ModelInvestigationNames.size())
            quote.Error = ModelError::BadModel;
        cost = ModelInvestigationCost(*def, exposure.Goal, action.Investigation);
        break;
    case ModelActionKind::PresentClaim:
        if (static_cast<std::size_t>(action.Claim) >= ModelClaimNames.size())
            quote.Error = ModelError::BadModel;
        cost = action.Explain ? ModelExplainCost(*def, exposure.Goal) : 0;
        break;
    }
    quote.Units = cost;
    quote.UnitsAfter = exposure.UnitsLeft - cost;
    if (quote.Error == ModelError::None && cost > exposure.UnitsLeft)
        quote.Error = ModelError::UnitsShort;
    return quote;
}

ModelStepResult ApplyModelAction(StructureModelState& state, const ModelCatalogue& catalogue, const ModelAction& action)
{
    ModelStepResult result;
    const ModelQuote quote = QuoteModelAction(state, catalogue, action);
    if (!quote.Ok())
    {
        result.Error = quote.Error;
        return result;
    }
    if (action.Kind == ModelActionKind::CommitModel && action.Links.empty())
    {
        result.Error = ModelError::BadModel;
        return result;
    }
    if (!IsStructureModelStateValid(state))
    {
        result.Error = ModelError::InvalidState;
        return result;
    }

    StructureModelState next = state;
    ModelExposure& exposure = next.Exposure;
    const ModelCaseDef& def = *catalogue.FindCase(exposure.CaseId);
    exposure.UnitsLeft = quote.UnitsAfter;
    result.Charged = quote.Units;
    switch (action.Kind)
    {
    case ModelActionKind::WorkedExample:
        exposure.ExampleTaken = true;
        break;
    case ModelActionKind::CommitModel:
        exposure.Links = action.Links;
        std::sort(exposure.Links.begin(), exposure.Links.end());
        exposure.Beat = ModelBeat::ResolveDiscrepancy;
        break;
    case ModelActionKind::Investigate:
        exposure.Investigation = action.Investigation;
        exposure.Beat = ModelBeat::PresentClaim;
        break;
    case ModelActionKind::PresentClaim:
    {
        ModelFamilyRecord* record = ModelFindFamilyMutable(next, exposure.FamilyId);
        if (record == nullptr || record->Exposures == ModelMaxCount)
        {
            result.Error = ModelError::InvalidState;
            return result;
        }
        result.Outcome = ModelEvaluate(def, exposure, action);
        result.Outcome.UnitsSpent = exposure.UnitsTotal - exposure.UnitsLeft;
        ModelRecordFinish(*record, true, result.Outcome.Grade, result.Outcome);
        result.Finished = true;
        next.Exposure = ModelExposure{};
        break;
    }
    }
    state = std::move(next);
    return result;
}

ModelAction ModelStandardAction(const ModelCaseDef& def, const ModelExposure& exposure)
{
    ModelAction action;
    switch (exposure.Beat)
    {
    case ModelBeat::BuildModel:
    {
        const bool shown = exposure.MethodKnown || exposure.ExampleTaken;
        if (!shown && ModelExampleCost(def) + ModelCommitCost(def, exposure.Goal, true) < ModelCommitCost(def, exposure.Goal, false))
        {
            action.Kind = ModelActionKind::WorkedExample;
            break;
        }
        action.Kind = ModelActionKind::CommitModel;
        for (const ModelLink& link : def.Links)
        {
            if (link.Sound)
                action.Links.push_back(link.Id);
        }
        break;
    }
    case ModelBeat::ResolveDiscrepancy:
        action.Kind = ModelActionKind::Investigate;
        action.Investigation = ModelMatchingInvestigation(def.Fault);
        break;
    case ModelBeat::PresentClaim:
        action.Kind = ModelActionKind::PresentClaim;
        action.Claim = ModelClaim::Estimate;
        action.Explain = ModelExplainCost(def, exposure.Goal) <= exposure.UnitsLeft;
        break;
    }
    return action;
}

ModelRoutineResult ResolveModelRoutine(StructureModelState& state, const ModelCatalogue& catalogue, std::string_view familyId)
{
    ModelRoutineResult result;
    if (state.Exposure.Active())
    {
        result.Error = ModelError::Busy;
        return result;
    }
    if (catalogue.FindFamily(familyId) == nullptr)
    {
        result.Error = ModelError::UnknownFamily;
        return result;
    }
    const ModelFamilyRecord* existing = state.FindFamily(familyId);
    if (existing == nullptr || !existing->Secure)
    {
        result.Error = ModelError::NotSecure;
        return result;
    }
    if (!IsStructureModelStateValid(state) || existing->Exposures == ModelMaxCount)
    {
        result.Error = ModelError::InvalidState;
        return result;
    }
    StructureModelState next = state;
    result.Outcome.FamilyId = std::string(familyId);
    result.Outcome.Foreground = false;
    ModelRecordFinish(*ModelFindFamilyMutable(next, familyId), false, ModelGrade::Sound, result.Outcome);
    state = std::move(next);
    return result;
}
} // namespace PerfectWorld
