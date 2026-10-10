#include "StructureCoordinate.h"

#include "StructureInternal.h"

#include <algorithm>
#include <limits>

// The exposure runner for "coordinate a group" (ROADMAP M2-09): present a case, quote
// and apply decisions, grade the outcome, abbreviate by routine. The rules are
// documented in Public/StructureCoordinate.h. Every entry point works on a copy and
// replaces the state only on success, so a refusal changes nothing.
namespace PerfectWorld
{
using namespace StructureInternal;

namespace
{
bool CoordinateBeatMatches(CoordinateBeat beat, CoordinateActionKind kind)
{
    switch (kind)
    {
    case CoordinateActionKind::AssignWork:
        return beat == CoordinateBeat::Assign;
    case CoordinateActionKind::Exchange:
    case CoordinateActionKind::EndExchange:
        return beat == CoordinateBeat::Exchange;
    case CoordinateActionKind::Settle:
        return beat == CoordinateBeat::Settle;
    case CoordinateActionKind::Submit:
        return beat == CoordinateBeat::Submit;
    }
    return false;
}

const CoordinateMember* CoordinateFindMember(const CoordinateCaseDef& def, const std::string& id)
{
    const auto it = std::find_if(def.Members.begin(), def.Members.end(), [&](const CoordinateMember& m) { return m.Id == id; });
    return it == def.Members.end() ? nullptr : &*it;
}

CoordinateOutcome CoordinateEvaluate(const CoordinateCaseDef& def, const CoordinateExposure& exposure, bool checked)
{
    CoordinateOutcome outcome;
    outcome.FamilyId = def.FamilyId;
    outcome.CaseId = def.Id;

    std::vector<std::int32_t> load(def.Members.size(), 0);
    for (const CoordinateAssignment& pair : exposure.Assignment)
    {
        const auto task = std::find_if(def.Tasks.begin(), def.Tasks.end(), [&](const CoordinateTask& t) { return t.Id == pair.Task; });
        const auto member = std::find_if(def.Members.begin(), def.Members.end(), [&](const CoordinateMember& m) { return m.Id == pair.Member; });
        if (task == def.Tasks.end() || member == def.Members.end())
            continue;
        load[static_cast<std::size_t>(member - def.Members.begin())] += task->Load;
        if (std::find(task->Fits.begin(), task->Fits.end(), pair.Member) == task->Fits.end())
            ++outcome.MismatchedTasks;
    }
    for (std::size_t i = 0; i < def.Members.size(); ++i)
        outcome.OverloadedMembers += load[i] > def.Members[i].Capacity ? 1 : 0;

    for (const CoordinateFact& fact : def.Facts)
    {
        if (fact.Critical && std::find(exposure.Heard.begin(), exposure.Heard.end(), fact.Holder) == exposure.Heard.end())
            ++outcome.MissedFacts;
    }
    const std::vector<std::string> holders = CoordinateCriticalHolders(def);
    for (const std::string& heard : exposure.Heard)
        outcome.IdleConversations += std::find(holders.begin(), holders.end(), heard) == holders.end() ? 1 : 0;

    if (outcome.OverloadedMembers > 0)
        outcome.Failures |= CoordinateFailOverloaded;
    if (outcome.MismatchedTasks > 0)
        outcome.Failures |= CoordinateFailMismatched;
    if (outcome.MissedFacts > 0)
        outcome.Failures |= CoordinateFailUninformed;
    if (exposure.Resolution != CoordinateMatchingResolution(def.Constraint))
        outcome.Failures |= CoordinateFailUnsettled;
    if (def.Seam && !checked)
        outcome.Failures |= CoordinateFailSeam;
    if (outcome.IdleConversations > 0 || (checked && !def.Seam))
        outcome.Failures |= CoordinateFailWasted;
    outcome.Grade = GradeFor(outcome.Failures, CoordinateFailOverloaded | CoordinateFailUninformed | CoordinateFailUnsettled | CoordinateFailSeam);
    return outcome;
}
} // namespace

StructureBeginResult BeginCoordinateExposure(CoordinateState& state, const CoordinateCatalogue& catalogue, std::string_view caseId,
                                             CoordinateGoal goal, std::int32_t skillMilli)
{
    StructureBeginResult result;
    if (state.Exposure.Active())
    {
        result.Error = StructureError::Busy;
        return result;
    }
    const CoordinateCaseDef* def = catalogue.FindCase(caseId);
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
    if (static_cast<std::size_t>(goal) >= CoordinateGoalNames.size() || !IsCoordinateStateValid(state))
    {
        result.Error = StructureError::InvalidState;
        return result;
    }

    CoordinateState next = state;
    if (NoteCaseSeen(next.Families, def->FamilyId, def->Id) != StructureError::None)
    {
        result.Error = StructureError::InvalidState;
        return result;
    }
    CoordinateExposure& exposure = next.Exposure;
    exposure.FamilyId = def->FamilyId;
    exposure.CaseId = def->Id;
    exposure.Goal = goal;
    exposure.Beat = CoordinateBeat::Assign;
    exposure.UnitsTotal = CoordinateBudgetUnits(def->Scarce);
    exposure.UnitsLeft = exposure.UnitsTotal;
    exposure.MethodKnown = skillMilli >= def->MethodKnownAtMilli;
    result.Units = exposure.UnitsTotal;
    result.MethodKnown = exposure.MethodKnown;
    state = std::move(next);
    return result;
}

StructureError CancelCoordinateExposure(CoordinateState& state)
{
    if (!state.Exposure.Active())
        return StructureError::NotActive;
    state.Exposure = CoordinateExposure{};
    return StructureError::None;
}

StructureQuote QuoteCoordinateAction(const CoordinateState& state, const CoordinateCatalogue& catalogue, const CoordinateAction& action)
{
    StructureQuote quote;
    const CoordinateExposure& exposure = state.Exposure;
    if (!exposure.Active())
    {
        quote.Error = StructureError::NotActive;
        return quote;
    }
    const CoordinateCaseDef* def = catalogue.FindCase(exposure.CaseId);
    if (def == nullptr)
    {
        quote.Error = StructureError::UnknownCase;
        return quote;
    }
    if (!CoordinateBeatMatches(exposure.Beat, action.Kind))
    {
        quote.Error = StructureError::WrongBeat;
        return quote;
    }

    std::int32_t cost = 0;
    switch (action.Kind)
    {
    case CoordinateActionKind::AssignWork:
        cost = CoordinateAssignCost(*def, exposure.Goal, exposure.MethodKnown);
        if (!action.Assignment.empty() && action.Assignment.size() != def->Tasks.size())
            quote.Error = StructureError::BadChoice;
        for (std::size_t i = 0; i < action.Assignment.size() && quote.Error == StructureError::None; ++i)
        {
            const CoordinateAssignment& pair = action.Assignment[i];
            if (std::none_of(def->Tasks.begin(), def->Tasks.end(), [&](const CoordinateTask& t) { return t.Id == pair.Task; }) ||
                CoordinateFindMember(*def, pair.Member) == nullptr)
                quote.Error = StructureError::UnknownItem;
            else if (std::any_of(action.Assignment.begin(), action.Assignment.begin() + static_cast<std::ptrdiff_t>(i),
                                 [&](const CoordinateAssignment& earlier) { return earlier.Task == pair.Task; }))
                quote.Error = StructureError::BadChoice;
        }
        break;
    case CoordinateActionKind::Exchange:
    {
        cost = CoordinateExchangeCost(*def, exposure.Goal);
        const CoordinateMember* member = CoordinateFindMember(*def, action.Member);
        if (action.Member.empty() || (member != nullptr && member->Self))
            quote.Error = StructureError::BadChoice;
        else if (member == nullptr)
            quote.Error = StructureError::UnknownItem;
        else if (std::find(exposure.Heard.begin(), exposure.Heard.end(), action.Member) != exposure.Heard.end())
            quote.Error = StructureError::AlreadyDone;
        break;
    }
    case CoordinateActionKind::EndExchange:
        break;
    case CoordinateActionKind::Settle:
        if (action.Resolution == CoordinateResolution::None || static_cast<std::size_t>(action.Resolution) >= CoordinateResolutionNames.size())
            quote.Error = StructureError::BadChoice;
        cost = CoordinateSettleCost(*def, exposure.Goal, action.Resolution);
        break;
    case CoordinateActionKind::Submit:
        cost = CoordinateSubmitCost(*def, exposure.Goal, action.Check);
        break;
    }
    quote.Units = cost;
    quote.UnitsAfter = exposure.UnitsLeft - cost;
    if (quote.Error == StructureError::None && cost > exposure.UnitsLeft)
        quote.Error = StructureError::UnitsShort;
    return quote;
}

CoordinateStepResult ApplyCoordinateAction(CoordinateState& state, const CoordinateCatalogue& catalogue, const CoordinateAction& action)
{
    CoordinateStepResult result;
    const StructureQuote quote = QuoteCoordinateAction(state, catalogue, action);
    if (!quote.Ok())
    {
        result.Error = quote.Error;
        return result;
    }
    // Pricing with nothing chosen is allowed; committing nothing is not.
    if (action.Kind == CoordinateActionKind::AssignWork && action.Assignment.empty())
    {
        result.Error = StructureError::BadChoice;
        return result;
    }
    if (!IsCoordinateStateValid(state))
    {
        result.Error = StructureError::InvalidState;
        return result;
    }

    CoordinateState next = state;
    CoordinateExposure& exposure = next.Exposure;
    const CoordinateCaseDef& def = *catalogue.FindCase(exposure.CaseId);
    exposure.UnitsLeft = quote.UnitsAfter;
    result.Charged = quote.Units;
    switch (action.Kind)
    {
    case CoordinateActionKind::AssignWork:
        exposure.Assignment = action.Assignment;
        std::sort(exposure.Assignment.begin(), exposure.Assignment.end(),
                  [](const CoordinateAssignment& a, const CoordinateAssignment& b) { return a.Task < b.Task; });
        exposure.Beat = CoordinateBeat::Exchange;
        break;
    case CoordinateActionKind::Exchange:
        exposure.Heard.push_back(action.Member);
        std::sort(exposure.Heard.begin(), exposure.Heard.end());
        break;
    case CoordinateActionKind::EndExchange:
        exposure.Beat = CoordinateBeat::Settle;
        break;
    case CoordinateActionKind::Settle:
        exposure.Resolution = action.Resolution;
        exposure.Beat = CoordinateBeat::Submit;
        break;
    case CoordinateActionKind::Submit:
    {
        StructureFamilyRecord* record = FindFamilyMutable(next.Families, exposure.FamilyId);
        if (record == nullptr || record->Exposures == std::numeric_limits<std::uint32_t>::max())
        {
            result.Error = StructureError::InvalidState;
            return result;
        }
        result.Outcome = CoordinateEvaluate(def, exposure, action.Check);
        result.Outcome.UnitsSpent = exposure.UnitsTotal - exposure.UnitsLeft;
        NoteFinish(*record, true, result.Outcome.Grade, result.Outcome);
        result.Finished = true;
        next.Exposure = CoordinateExposure{};
        break;
    }
    }
    state = std::move(next);
    return result;
}

CoordinateAction CoordinateStandardAction(const CoordinateCaseDef& def, const CoordinateExposure& exposure)
{
    CoordinateAction action;
    switch (exposure.Beat)
    {
    case CoordinateBeat::Assign:
        action.Kind = CoordinateActionKind::AssignWork;
        action.Assignment = CoordinateSoundAssignment(def);
        break;
    case CoordinateBeat::Exchange:
    {
        action.Kind = CoordinateActionKind::EndExchange;
        if (CoordinateExchangeCost(def, exposure.Goal) > exposure.UnitsLeft)
            break;
        for (const std::string& holder : CoordinateCriticalHolders(def))
        {
            if (std::find(exposure.Heard.begin(), exposure.Heard.end(), holder) == exposure.Heard.end())
            {
                action.Kind = CoordinateActionKind::Exchange;
                action.Member = holder;
                break;
            }
        }
        break;
    }
    case CoordinateBeat::Settle:
        action.Kind = CoordinateActionKind::Settle;
        action.Resolution = CoordinateMatchingResolution(def.Constraint);
        if (CoordinateSettleCost(def, exposure.Goal, action.Resolution) > exposure.UnitsLeft)
            action.Resolution = CoordinateResolution::Leave;
        break;
    case CoordinateBeat::Submit:
        action.Kind = CoordinateActionKind::Submit;
        action.Check = def.Seam && CoordinateSubmitCost(def, exposure.Goal, true) <= exposure.UnitsLeft;
        break;
    }
    return action;
}

StructureRoutineResult ResolveCoordinateRoutine(CoordinateState& state, const CoordinateCatalogue& catalogue, std::string_view familyId)
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
    CoordinateState next = state;
    result.Error = ResolveRoutineOnRecords(next.Families, familyId, result.Outcome);
    if (result.Error == StructureError::None && !IsCoordinateStateValid(next))
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
