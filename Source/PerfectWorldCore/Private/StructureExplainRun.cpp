#include "StructureExplain.h"

#include "StructureInternal.h"

#include <algorithm>
#include <limits>

// The exposure runner for "construct an explanation" (ROADMAP M2-09): present a case,
// quote and apply decisions, grade the outcome, abbreviate by routine. The rules are
// documented in Public/StructureExplain.h. Every entry point works on a copy and
// replaces the state only on success, so a refusal changes nothing.
namespace PerfectWorld
{
using namespace StructureInternal;

namespace
{
bool ExplainBeatMatches(ExplainBeat beat, ExplainActionKind kind)
{
    switch (kind)
    {
    case ExplainActionKind::ModelParagraph:
    case ExplainActionKind::ChooseClaim:
        return beat == ExplainBeat::Claim;
    case ExplainActionKind::PlaceEvidence:
        return beat == ExplainBeat::Evidence;
    case ExplainActionKind::PlaceLimitation:
        return beat == ExplainBeat::Limitation;
    }
    return false;
}

bool ExplainListed(const std::vector<std::string>& ids, const std::string& id)
{
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

ExplainOutcome ExplainEvaluate(const ExplainCaseDef& def, const ExplainExposure& exposure, const std::string& limitationId)
{
    ExplainOutcome outcome;
    outcome.FamilyId = def.FamilyId;
    outcome.CaseId = def.Id;
    const auto claim = std::find_if(def.Claims.begin(), def.Claims.end(), [&](const ExplainClaim& c) { return c.Id == exposure.ClaimId; });
    const bool defensible = claim != def.Claims.end() && claim->Defensible;
    for (const std::string& id : exposure.EvidenceIds)
    {
        const auto item = std::find_if(def.Evidence.begin(), def.Evidence.end(), [&](const ExplainEvidence& e) { return e.Id == id; });
        ++outcome.PlacedItems;
        if (item != def.Evidence.end() && ExplainListed(item->Supports, exposure.ClaimId))
            ++outcome.SupportingItems;
    }
    const auto limitation = std::find_if(def.Limitations.begin(), def.Limitations.end(), [&](const ExplainLimitation& l) { return l.Id == limitationId; });
    outcome.Bounded = limitation != def.Limitations.end() && ExplainListed(limitation->Bounds, exposure.ClaimId);

    if (!defensible)
        outcome.Failures |= ExplainFailIndefensible;
    else if (outcome.SupportingItems == 0)
        outcome.Failures |= ExplainFailUnsupported;
    if (!outcome.Bounded)
        outcome.Failures |= ExplainFailOverreach;
    if (outcome.SupportingItems < outcome.PlacedItems)
        outcome.Failures |= ExplainFailPadding;
    outcome.Grade = GradeFor(outcome.Failures, ExplainFailIndefensible | ExplainFailUnsupported);
    return outcome;
}
} // namespace

StructureBeginResult BeginExplainExposure(ExplainState& state, const ExplainCatalogue& catalogue, std::string_view caseId, ExplainGoal goal,
                                          std::int32_t skillMilli)
{
    StructureBeginResult result;
    if (state.Exposure.Active())
    {
        result.Error = StructureError::Busy;
        return result;
    }
    const ExplainCaseDef* def = catalogue.FindCase(caseId);
    if (def == nullptr)
    {
        result.Error = StructureError::UnknownCase;
        return result;
    }
    if (skillMilli < 0)
    {
        result.Error = StructureError::InvalidSkill;
        return result;
    }
    if (static_cast<std::size_t>(goal) >= ExplainGoalNames.size() || !IsExplainStateValid(state))
    {
        result.Error = StructureError::InvalidState;
        return result;
    }

    ExplainState next = state;
    if (NoteCaseSeen(next.Families, def->FamilyId, def->Id) != StructureError::None)
    {
        result.Error = StructureError::InvalidState;
        return result;
    }
    ExplainExposure& exposure = next.Exposure;
    exposure.FamilyId = def->FamilyId;
    exposure.CaseId = def->Id;
    exposure.Goal = goal;
    exposure.Beat = ExplainBeat::Claim;
    exposure.UnitsTotal = ExplainBudgetUnits(def->Scarce);
    exposure.UnitsLeft = exposure.UnitsTotal;
    exposure.MethodKnown = skillMilli >= def->MethodKnownAtMilli;
    result.Units = exposure.UnitsTotal;
    result.MethodKnown = exposure.MethodKnown;
    state = std::move(next);
    return result;
}

StructureError CancelExplainExposure(ExplainState& state)
{
    if (!state.Exposure.Active())
        return StructureError::NotActive;
    state.Exposure = ExplainExposure{};
    return StructureError::None;
}

StructureQuote QuoteExplainAction(const ExplainState& state, const ExplainCatalogue& catalogue, const ExplainAction& action)
{
    StructureQuote quote;
    const ExplainExposure& exposure = state.Exposure;
    if (!exposure.Active())
    {
        quote.Error = StructureError::NotActive;
        return quote;
    }
    const ExplainCaseDef* def = catalogue.FindCase(exposure.CaseId);
    if (def == nullptr)
    {
        quote.Error = StructureError::UnknownCase;
        return quote;
    }
    if (!ExplainBeatMatches(exposure.Beat, action.Kind))
    {
        quote.Error = StructureError::WrongBeat;
        return quote;
    }

    std::int32_t cost = 0;
    switch (action.Kind)
    {
    case ExplainActionKind::ModelParagraph:
        if (exposure.MethodKnown || exposure.ParagraphTaken)
            quote.Error = StructureError::NotNeeded;
        cost = ExplainParagraphCost(*def);
        break;
    case ExplainActionKind::ChooseClaim:
        cost = ExplainClaimCost(*def, exposure.Goal, exposure.MethodKnown || exposure.ParagraphTaken);
        if (!action.Claim.empty() &&
            std::none_of(def->Claims.begin(), def->Claims.end(), [&](const ExplainClaim& c) { return c.Id == action.Claim; }))
            quote.Error = StructureError::UnknownItem;
        break;
    case ExplainActionKind::PlaceEvidence:
        cost = ExplainEvidenceCost(*def, exposure.Goal, std::max<std::size_t>(1, action.Evidence.size()));
        if (action.Evidence.size() > ExplainEvidenceSlots)
            quote.Error = StructureError::BadChoice;
        for (std::size_t i = 0; i < action.Evidence.size() && quote.Error == StructureError::None; ++i)
        {
            const std::string& id = action.Evidence[i];
            if (std::none_of(def->Evidence.begin(), def->Evidence.end(), [&](const ExplainEvidence& e) { return e.Id == id; }))
                quote.Error = StructureError::UnknownItem;
            else if (std::find(action.Evidence.begin(), action.Evidence.begin() + static_cast<std::ptrdiff_t>(i), id) !=
                     action.Evidence.begin() + static_cast<std::ptrdiff_t>(i))
                quote.Error = StructureError::BadChoice;
        }
        break;
    case ExplainActionKind::PlaceLimitation:
        cost = action.Limitation.empty() ? 0 : ExplainLimitationCost(*def, exposure.Goal);
        if (!action.Limitation.empty() &&
            std::none_of(def->Limitations.begin(), def->Limitations.end(), [&](const ExplainLimitation& l) { return l.Id == action.Limitation; }))
            quote.Error = StructureError::UnknownItem;
        break;
    }
    quote.Units = cost;
    quote.UnitsAfter = exposure.UnitsLeft - cost;
    if (quote.Error == StructureError::None && cost > exposure.UnitsLeft)
        quote.Error = StructureError::UnitsShort;
    return quote;
}

ExplainStepResult ApplyExplainAction(ExplainState& state, const ExplainCatalogue& catalogue, const ExplainAction& action)
{
    ExplainStepResult result;
    const StructureQuote quote = QuoteExplainAction(state, catalogue, action);
    if (!quote.Ok())
    {
        result.Error = quote.Error;
        return result;
    }
    // Pricing with nothing chosen is allowed; committing nothing is not.
    if ((action.Kind == ExplainActionKind::ChooseClaim && action.Claim.empty()) ||
        (action.Kind == ExplainActionKind::PlaceEvidence && action.Evidence.empty()))
    {
        result.Error = StructureError::BadChoice;
        return result;
    }
    if (!IsExplainStateValid(state))
    {
        result.Error = StructureError::InvalidState;
        return result;
    }

    ExplainState next = state;
    ExplainExposure& exposure = next.Exposure;
    const ExplainCaseDef& def = *catalogue.FindCase(exposure.CaseId);
    exposure.UnitsLeft = quote.UnitsAfter;
    result.Charged = quote.Units;
    switch (action.Kind)
    {
    case ExplainActionKind::ModelParagraph:
        exposure.ParagraphTaken = true;
        break;
    case ExplainActionKind::ChooseClaim:
        exposure.ClaimId = action.Claim;
        exposure.Beat = ExplainBeat::Evidence;
        break;
    case ExplainActionKind::PlaceEvidence:
        exposure.EvidenceIds = action.Evidence;
        std::sort(exposure.EvidenceIds.begin(), exposure.EvidenceIds.end());
        exposure.Beat = ExplainBeat::Limitation;
        break;
    case ExplainActionKind::PlaceLimitation:
    {
        StructureFamilyRecord* record = FindFamilyMutable(next.Families, exposure.FamilyId);
        if (record == nullptr || record->Exposures == std::numeric_limits<std::uint32_t>::max())
        {
            result.Error = StructureError::InvalidState;
            return result;
        }
        result.Outcome = ExplainEvaluate(def, exposure, action.Limitation);
        result.Outcome.UnitsSpent = exposure.UnitsTotal - exposure.UnitsLeft;
        NoteFinish(*record, true, result.Outcome.Grade, result.Outcome);
        result.Finished = true;
        next.Exposure = ExplainExposure{};
        break;
    }
    }
    state = std::move(next);
    return result;
}

ExplainAction ExplainStandardAction(const ExplainCaseDef& def, const ExplainExposure& exposure)
{
    ExplainAction action;
    switch (exposure.Beat)
    {
    case ExplainBeat::Claim:
    {
        const bool shown = exposure.MethodKnown || exposure.ParagraphTaken;
        if (!shown && ExplainParagraphCost(def) + ExplainClaimCost(def, exposure.Goal, true) < ExplainClaimCost(def, exposure.Goal, false))
        {
            action.Kind = ExplainActionKind::ModelParagraph;
            break;
        }
        action.Kind = ExplainActionKind::ChooseClaim;
        for (const ExplainClaim& claim : def.Claims)
        {
            if (!ExplainPlanFor(def, claim.Id).ClaimId.empty())
            {
                action.Claim = claim.Id;
                break;
            }
        }
        break;
    }
    case ExplainBeat::Evidence:
    {
        action.Kind = ExplainActionKind::PlaceEvidence;
        action.Evidence.push_back(ExplainPlanFor(def, exposure.ClaimId).EvidenceId);
        break;
    }
    case ExplainBeat::Limitation:
        action.Kind = ExplainActionKind::PlaceLimitation;
        if (ExplainLimitationCost(def, exposure.Goal) <= exposure.UnitsLeft)
            action.Limitation = ExplainPlanFor(def, exposure.ClaimId).LimitationId;
        break;
    }
    return action;
}

StructureRoutineResult ResolveExplainRoutine(ExplainState& state, const ExplainCatalogue& catalogue, std::string_view familyId)
{
    StructureRoutineResult result;
    if (state.Exposure.Active())
    {
        result.Error = StructureError::Busy;
        return result;
    }
    if (catalogue.FindFamily(familyId) == nullptr)
    {
        result.Error = StructureError::UnknownFamily;
        return result;
    }
    ExplainState next = state;
    result.Error = ResolveRoutineOnRecords(next.Families, familyId, result.Outcome);
    if (result.Error == StructureError::None && !IsExplainStateValid(next))
        result.Error = StructureError::InvalidState;
    if (!result.Ok())
    {
        result.Outcome = StructureOutcome{};
        return result;
    }
    state = std::move(next);
    return result;
}
} // namespace PerfectWorld
