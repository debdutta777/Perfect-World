#include "StructureInvestigate.h"

#include "StructureInternal.h"

#include <algorithm>

// The investigate catalogue, costs, plans and case selection (ROADMAP M2-09). The
// rules are documented in Public/StructureInvestigate.h; the exposure runner is in
// StructureInvestigateRun.cpp and the save text in StructureInvestigateText.cpp.
namespace PerfectWorld
{
using namespace StructureInternal;

namespace
{
bool InvestigateHas(const std::vector<std::string>& ids, const std::string& id)
{
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

/** The kind of test a role favours; the critic favours none. */
bool InvestigateRoleMatches(InvestigateRole role, InvestigateTestKind kind)
{
    return (role == InvestigateRole::Inspector && kind == InvestigateTestKind::Observe) ||
           (role == InvestigateRole::Experimenter && kind == InvestigateTestKind::Measure) ||
           (role == InvestigateRole::Repairer && kind == InvestigateTestKind::Swap);
}

bool InvestigateGoalMatches(InvestigateGoal goal, InvestigateTestKind kind)
{
    return (goal == InvestigateGoal::Inspect && kind == InvestigateTestKind::Observe) ||
           (goal == InvestigateGoal::Measure && kind == InvestigateTestKind::Measure) ||
           (goal == InvestigateGoal::SwapParts && kind == InvestigateTestKind::Swap);
}

bool InvestigateScarceMatches(InvestigateScarce scarce, InvestigateTestKind kind)
{
    return (scarce == InvestigateScarce::Access && kind == InvestigateTestKind::Observe) ||
           (scarce == InvestigateScarce::Equipment && kind == InvestigateTestKind::Measure) ||
           (scarce == InvestigateScarce::Parts && kind == InvestigateTestKind::Swap);
}
} // namespace

const InvestigateCause* InvestigateFaultOf(const InvestigateCaseDef& def) noexcept
{
    const auto it = std::find_if(def.Causes.begin(), def.Causes.end(), [](const InvestigateCause& c) { return c.Fault; });
    return it == def.Causes.end() ? nullptr : &*it;
}

bool InvestigateTestPositive(const InvestigateCaseDef& def, const InvestigateTest& test) noexcept
{
    const InvestigateCause* fault = InvestigateFaultOf(def);
    return fault != nullptr && InvestigateHas(test.Implicates, fault->Id);
}

std::vector<std::string> InvestigateRemaining(const InvestigateCaseDef& def, const std::vector<std::string>& testsRun)
{
    std::vector<std::string> remaining;
    for (const InvestigateCause& cause : def.Causes)
        remaining.push_back(cause.Id);
    for (const std::string& id : testsRun)
    {
        const auto test = std::find_if(def.Tests.begin(), def.Tests.end(), [&](const InvestigateTest& t) { return t.Id == id; });
        if (test == def.Tests.end())
            continue;
        const bool positive = InvestigateTestPositive(def, *test);
        remaining.erase(std::remove_if(remaining.begin(), remaining.end(),
                                       [&](const std::string& cause) { return InvestigateHas(test->Implicates, cause) != positive; }),
                        remaining.end());
    }
    return remaining;
}

std::int32_t InvestigateBudgetUnits(InvestigateScarce scarce) noexcept
{
    return StructureBudgetUnits(scarce == InvestigateScarce::Time);
}

std::int32_t InvestigateTestCost(const InvestigateCaseDef& def, InvestigateGoal goal, const InvestigateTest& test, bool methodKnown) noexcept
{
    std::int32_t cost = test.Units;
    if (!methodKnown && test.Kind != InvestigateTestKind::Observe)
        cost += InvestigateUnfamiliarUnits;
    if (InvestigateScarceMatches(def.Scarce, test.Kind))
        cost += InvestigateScarceSurcharge;
    if (InvestigateRoleMatches(def.Role, test.Kind) || InvestigateGoalMatches(goal, test.Kind))
        cost = std::max<std::int32_t>(InvestigateMinTestUnits, cost - StructureDiscountUnits);
    return cost;
}

InvestigatePlan InvestigatePlanFor(const InvestigateCaseDef& def, InvestigateGoal goal, bool methodKnown, const std::vector<std::string>& testsRun)
{
    InvestigatePlan best;
    if (InvestigateFaultOf(def) == nullptr)
        return best;
    std::vector<const InvestigateTest*> open;
    for (const InvestigateTest& test : def.Tests)
    {
        if (!InvestigateHas(testsRun, test.Id))
            open.push_back(&test);
    }
    if (open.size() > InvestigateMaxTests)
        return best;
    const std::uint32_t subsets = 1u << open.size();
    for (std::uint32_t mask = 0; mask < subsets; ++mask)
    {
        std::vector<std::string> run = testsRun;
        std::int32_t units = 0;
        for (std::size_t i = 0; i < open.size(); ++i)
        {
            if ((mask >> i & 1u) != 0)
            {
                run.push_back(open[i]->Id);
                units += InvestigateTestCost(def, goal, *open[i], methodKnown);
            }
        }
        if (InvestigateRemaining(def, run).size() != 1 || (best.Found && units >= best.Units))
            continue;
        best.Found = true;
        best.Units = units;
        best.Tests.clear();
        for (std::size_t i = 0; i < open.size(); ++i)
        {
            if ((mask >> i & 1u) != 0)
                best.Tests.push_back(open[i]->Id);
        }
    }
    return best;
}

// ---- Loading ----------------------------------------------------------------------

namespace
{
bool InvestigateCompletable(const InvestigateCaseDef& def)
{
    for (const InvestigateGoal goal : {InvestigateGoal::Inspect, InvestigateGoal::Measure, InvestigateGoal::SwapParts})
    {
        const InvestigatePlan plan = InvestigatePlanFor(def, goal, false, {});
        if (!plan.Found || plan.Units > InvestigateBudgetUnits(def.Scarce))
            return false;
    }
    return true;
}

template <typename Item>
bool InvestigateIdsUnique(const std::vector<Item>& items)
{
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        if (!ModelIdWellFormed(items[i].Id))
            return false;
        for (std::size_t j = 0; j < i; ++j)
        {
            if (items[j].Id == items[i].Id)
                return false;
        }
    }
    return true;
}

std::string InvestigateLoadCase(const ContentRecord& record, const StructureFamilyHeader& family, InvestigateCaseDef& def)
{
    if (const std::string error = LoadCaseBase(record, family, def); !error.empty())
        return error;
    if (!ModelParseName(InvestigateSymptomNames, JsonString(&record.Data, "symptom"), def.Symptom) ||
        !ModelParseName(InvestigateRoleNames, JsonString(&record.Data, "role"), def.Role) ||
        !ModelParseName(InvestigateScarceNames, JsonString(&record.Data, "scarce"), def.Scarce))
        return "symptom, role and scarce must each be a known name";
    for (const JsonValue& entry : JsonItems(&record.Data, "causes"))
        def.Causes.push_back({JsonString(&entry, "id"), JsonString(&entry, "name"), JsonFlag(&entry, "fault", false)});
    for (const JsonValue& entry : JsonItems(&record.Data, "tests"))
    {
        InvestigateTest test;
        test.Id = JsonString(&entry, "id");
        test.Name = JsonString(&entry, "name");
        test.Implicates = JsonStrings(&entry, "implicates");
        if (!ModelParseName(InvestigateTestKindNames, JsonString(&entry, "kind"), test.Kind) || !JsonInt32(&entry, "units", test.Units) ||
            test.Units < 1 || test.Units > 3)
            return "test " + test.Id + " needs a known kind and units from 1 to 3";
        def.Tests.push_back(std::move(test));
    }
    if (!InvestigateIdsUnique(def.Causes) || !InvestigateIdsUnique(def.Tests))
        return "cause and test IDs must be well-formed and unique";
    if (def.Tests.empty() || def.Tests.size() > InvestigateMaxTests || def.Causes.size() < 2 || def.Causes.size() > StructureMaxListed)
        return "a case needs at least two causes and from one to " + std::to_string(InvestigateMaxTests) + " tests";
    if (std::count_if(def.Causes.begin(), def.Causes.end(), [](const InvestigateCause& c) { return c.Fault; }) != 1)
        return "exactly one cause must be the fault";
    for (const InvestigateTest& test : def.Tests)
    {
        const bool known = std::all_of(test.Implicates.begin(), test.Implicates.end(), [&](const std::string& id) {
            return std::any_of(def.Causes.begin(), def.Causes.end(), [&](const InvestigateCause& c) { return c.Id == id; });
        });
        if (!known || test.Implicates.empty() || test.Implicates.size() >= def.Causes.size())
            return "test " + test.Id + " must implicate some, but not all, of the case's causes";
    }
    if (!InvestigateCompletable(def))
        return "cannot be completed within its " + std::to_string(InvestigateBudgetUnits(def.Scarce)) + " units";
    return {};
}
} // namespace

InvestigateCatalogueResult LoadInvestigateCatalogue(const ContentDb& content)
{
    InvestigateCatalogueResult result;
    for (const ContentRecord* record : content.OfType("investigate_family"))
    {
        InvestigateFamilyDef def;
        if (const std::string error = LoadFamilyHeader(*record, def); !error.empty())
        {
            result.Error = record->Id + ": " + error;
            result.Catalogue.Families.clear();
            return result;
        }
        result.Catalogue.Families.push_back(std::move(def));
    }
    for (const ContentRecord* record : content.OfType("investigate_case"))
    {
        const std::string familyId = JsonString(&record->Data, "family");
        const auto family = std::find_if(result.Catalogue.Families.begin(), result.Catalogue.Families.end(),
                                         [&](const InvestigateFamilyDef& f) { return f.Id == familyId; });
        InvestigateCaseDef def;
        const std::string error = family == result.Catalogue.Families.end() ? "its family " + familyId + " does not exist"
                                                                            : InvestigateLoadCase(*record, *family, def);
        if (!error.empty())
        {
            result.Error = record->Id + ": " + error;
            result.Catalogue.Families.clear();
            return result;
        }
        family->Cases.push_back(std::move(def));
    }
    return result;
}

// ---- Selection --------------------------------------------------------------------

bool InvestigateFamilySecure(const InvestigateState& state, std::string_view familyId) noexcept
{
    const StructureFamilyRecord* record = state.FindFamily(familyId);
    return record != nullptr && record->Secure;
}

StructureSelection SelectInvestigateCase(const InvestigateCatalogue& catalogue, const InvestigateState& state, std::string_view familyId,
                                         std::uint64_t worldSeed, GameInstant at)
{
    const InvestigateFamilyDef* family = catalogue.FindFamily(familyId);
    if (family == nullptr)
    {
        StructureSelection refused;
        refused.Error = StructureError::UnknownFamily;
        return refused;
    }
    std::vector<std::string> ids;
    for (const InvestigateCaseDef& def : family->Cases)
        ids.push_back(def.Id);
    return StructureSelectCase(ids, state.FindFamily(familyId), familyId, worldSeed, at);
}
} // namespace PerfectWorld
