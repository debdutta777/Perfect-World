#include "StructureInvestigate.h"

#include "StructureInternal.h"

#include <algorithm>
#include <limits>

// The exposure runner for "investigate a fault" (ROADMAP M2-09): present a case,
// quote and apply decisions, grade the outcome, abbreviate by routine. The rules are
// documented in Public/StructureInvestigate.h. Every entry point works on a copy and
// replaces the state only on success, so a refusal changes nothing.
namespace PerfectWorld
{
using namespace StructureInternal;

namespace
{
InvestigateOutcome InvestigateEvaluate(const InvestigateCaseDef& def, const InvestigateExposure& exposure, const InvestigateAction& action)
{
    InvestigateOutcome outcome;
    outcome.FamilyId = def.FamilyId;
    outcome.CaseId = def.Id;
    outcome.TestsRun = static_cast<std::int32_t>(exposure.TestsRun.size());

    // A test is redundant when the causes standing are the same just before and just after it.
    std::vector<std::string> run;
    std::vector<std::string> before = InvestigateRemaining(def, run);
    for (const std::string& id : exposure.TestsRun)
    {
        run.push_back(id);
        std::vector<std::string> after = InvestigateRemaining(def, run);
        if (after == before)
            ++outcome.RedundantTests;
        before = std::move(after);
    }
    outcome.RemainingCauses = static_cast<std::int32_t>(before.size());

    const InvestigateCause* fault = InvestigateFaultOf(def);
    if (action.Kind == InvestigateActionKind::Diagnose)
    {
        outcome.Diagnosed = action.Cause;
        if (fault == nullptr || action.Cause != fault->Id)
            outcome.Failures |= InvestigateFailWrongCause;
        if (before.size() > 1)
            outcome.Failures |= InvestigateFailUnsupported;
    }
    else if (before.size() == 1 && def.Role != InvestigateRole::Critic)
        outcome.Failures |= InvestigateFailAbandoned;
    if (outcome.RedundantTests > 0)
        outcome.Failures |= InvestigateFailRedundant;
    outcome.Grade = GradeFor(outcome.Failures, InvestigateFailWrongCause | InvestigateFailUnsupported);
    return outcome;
}
} // namespace

StructureBeginResult BeginInvestigateExposure(InvestigateState& state, const InvestigateCatalogue& catalogue, std::string_view caseId,
                                              InvestigateGoal goal, std::int32_t skillMilli)
{
    StructureBeginResult result;
    if (state.Exposure.Active())
    {
        result.Error = StructureError::Busy;
        return result;
    }
    const InvestigateCaseDef* def = catalogue.FindCase(caseId);
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
    if (static_cast<std::size_t>(goal) >= InvestigateGoalNames.size() || !IsInvestigateStateValid(state))
    {
        result.Error = StructureError::InvalidState;
        return result;
    }

    InvestigateState next = state;
    if (NoteCaseSeen(next.Families, def->FamilyId, def->Id) != StructureError::None)
    {
        result.Error = StructureError::InvalidState;
        return result;
    }
    InvestigateExposure& exposure = next.Exposure;
    exposure.FamilyId = def->FamilyId;
    exposure.CaseId = def->Id;
    exposure.Goal = goal;
    exposure.UnitsTotal = InvestigateBudgetUnits(def->Scarce);
    exposure.UnitsLeft = exposure.UnitsTotal;
    exposure.MethodKnown = skillMilli >= def->MethodKnownAtMilli;
    result.Units = exposure.UnitsTotal;
    result.MethodKnown = exposure.MethodKnown;
    state = std::move(next);
    return result;
}

StructureError CancelInvestigateExposure(InvestigateState& state)
{
    if (!state.Exposure.Active())
        return StructureError::NotActive;
    state.Exposure = InvestigateExposure{};
    return StructureError::None;
}

StructureQuote QuoteInvestigateAction(const InvestigateState& state, const InvestigateCatalogue& catalogue, const InvestigateAction& action)
{
    StructureQuote quote;
    const InvestigateExposure& exposure = state.Exposure;
    if (!exposure.Active())
    {
        quote.Error = StructureError::NotActive;
        return quote;
    }
    const InvestigateCaseDef* def = catalogue.FindCase(exposure.CaseId);
    if (def == nullptr)
    {
        quote.Error = StructureError::UnknownCase;
        return quote;
    }

    std::int32_t cost = 0;
    switch (action.Kind)
    {
    case InvestigateActionKind::RunTest:
    {
        const auto test = std::find_if(def->Tests.begin(), def->Tests.end(), [&](const InvestigateTest& t) { return t.Id == action.Test; });
        if (action.Test.empty())
            quote.Error = StructureError::BadChoice;
        else if (test == def->Tests.end())
            quote.Error = StructureError::UnknownItem;
        else if (std::find(exposure.TestsRun.begin(), exposure.TestsRun.end(), action.Test) != exposure.TestsRun.end())
            quote.Error = StructureError::AlreadyDone;
        if (test != def->Tests.end())
            cost = InvestigateTestCost(*def, exposure.Goal, *test, exposure.MethodKnown);
        break;
    }
    case InvestigateActionKind::Diagnose:
        if (action.Cause.empty())
            quote.Error = StructureError::BadChoice;
        else if (std::none_of(def->Causes.begin(), def->Causes.end(), [&](const InvestigateCause& c) { return c.Id == action.Cause; }))
            quote.Error = StructureError::UnknownItem;
        break;
    case InvestigateActionKind::ReportInconclusive:
        break;
    default:
        quote.Error = StructureError::WrongBeat;
        return quote;
    }
    quote.Units = cost;
    quote.UnitsAfter = exposure.UnitsLeft - cost;
    if (quote.Error == StructureError::None && cost > exposure.UnitsLeft)
        quote.Error = StructureError::UnitsShort;
    return quote;
}

InvestigateStepResult ApplyInvestigateAction(InvestigateState& state, const InvestigateCatalogue& catalogue, const InvestigateAction& action)
{
    InvestigateStepResult result;
    const StructureQuote quote = QuoteInvestigateAction(state, catalogue, action);
    if (!quote.Ok())
    {
        result.Error = quote.Error;
        return result;
    }
    if (!IsInvestigateStateValid(state))
    {
        result.Error = StructureError::InvalidState;
        return result;
    }

    InvestigateState next = state;
    InvestigateExposure& exposure = next.Exposure;
    const InvestigateCaseDef& def = *catalogue.FindCase(exposure.CaseId);
    exposure.UnitsLeft = quote.UnitsAfter;
    result.Charged = quote.Units;
    if (action.Kind == InvestigateActionKind::RunTest)
    {
        if (exposure.TestsRun.size() >= InvestigateMaxTests)
        {
            result.Error = StructureError::InvalidState;
            return result;
        }
        exposure.TestsRun.push_back(action.Test);
    }
    else
    {
        StructureFamilyRecord* record = FindFamilyMutable(next.Families, exposure.FamilyId);
        if (record == nullptr || record->Exposures == std::numeric_limits<std::uint32_t>::max())
        {
            result.Error = StructureError::InvalidState;
            return result;
        }
        result.Outcome = InvestigateEvaluate(def, exposure, action);
        result.Outcome.UnitsSpent = exposure.UnitsTotal - exposure.UnitsLeft;
        NoteFinish(*record, true, result.Outcome.Grade, result.Outcome);
        result.Finished = true;
        next.Exposure = InvestigateExposure{};
    }
    state = std::move(next);
    return result;
}

InvestigateAction InvestigateStandardAction(const InvestigateCaseDef& def, const InvestigateExposure& exposure)
{
    InvestigateAction action;
    const InvestigatePlan plan = InvestigatePlanFor(def, exposure.Goal, exposure.MethodKnown, exposure.TestsRun);
    if (plan.Found && plan.Units <= exposure.UnitsLeft)
    {
        if (!plan.Tests.empty())
        {
            action.Kind = InvestigateActionKind::RunTest;
            action.Test = plan.Tests.front();
            return action;
        }
        action.Kind = InvestigateActionKind::Diagnose;
        const InvestigateCause* fault = InvestigateFaultOf(def);
        action.Cause = fault != nullptr ? fault->Id : std::string();
        return action;
    }
    action.Kind = InvestigateActionKind::ReportInconclusive;
    return action;
}

StructureRoutineResult ResolveInvestigateRoutine(InvestigateState& state, const InvestigateCatalogue& catalogue, std::string_view familyId)
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
    InvestigateState next = state;
    result.Error = ResolveRoutineOnRecords(next.Families, familyId, result.Outcome);
    if (result.Error == StructureError::None && !IsInvestigateStateValid(next))
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
