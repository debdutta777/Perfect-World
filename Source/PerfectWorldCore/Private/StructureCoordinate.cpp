#include "StructureCoordinate.h"

#include "StructureInternal.h"

#include <algorithm>

// The coordinate catalogue, costs, sound assignments and case selection (ROADMAP
// M2-09). The rules are documented in Public/StructureCoordinate.h; the exposure
// runner is in StructureCoordinateRun.cpp and the save text in
// StructureCoordinateText.cpp.
namespace PerfectWorld
{
using namespace StructureInternal;

namespace
{
bool CoordinateHas(const std::vector<std::string>& ids, const std::string& id)
{
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

bool CoordinateDiscountsAssign(const CoordinateCaseDef& def, CoordinateGoal goal)
{
    return def.Role == CoordinateRole::Chair || goal == CoordinateGoal::OrganiseWork;
}

bool CoordinateDiscountsExchange(const CoordinateCaseDef& def, CoordinateGoal goal)
{
    return def.Role == CoordinateRole::Mediator || goal == CoordinateGoal::HearEveryone;
}

bool CoordinateDiscountsSettle(const CoordinateCaseDef& def, CoordinateGoal goal)
{
    return def.Role == CoordinateRole::Treasurer || goal == CoordinateGoal::SettleConstraint;
}

bool CoordinateDiscountsSubmit(const CoordinateCaseDef& def, CoordinateGoal goal)
{
    return def.Role == CoordinateRole::Scribe || goal == CoordinateGoal::SubmitCleanly;
}
} // namespace

CoordinateResolution CoordinateMatchingResolution(CoordinateConstraint constraint) noexcept
{
    switch (constraint)
    {
    case CoordinateConstraint::Capacity:
        return CoordinateResolution::Rebalance;
    case CoordinateConstraint::Clash:
        return CoordinateResolution::Reschedule;
    case CoordinateConstraint::Budget:
        return CoordinateResolution::TrimScope;
    case CoordinateConstraint::Deadline:
        return CoordinateResolution::AskExtension;
    }
    return CoordinateResolution::Leave;
}

std::vector<CoordinateAssignment> CoordinateSoundAssignment(const CoordinateCaseDef& def)
{
    const std::size_t tasks = def.Tasks.size();
    const std::size_t members = def.Members.size();
    if (tasks == 0 || members == 0 || tasks > StructureMaxListed || members > StructureMaxListed)
        return {};
    std::vector<std::size_t> pick(tasks, 0);
    while (true)
    {
        std::vector<std::int32_t> load(members, 0);
        bool fits = true;
        for (std::size_t t = 0; t < tasks && fits; ++t)
        {
            load[pick[t]] += def.Tasks[t].Load;
            fits = CoordinateHas(def.Tasks[t].Fits, def.Members[pick[t]].Id) && load[pick[t]] <= def.Members[pick[t]].Capacity;
        }
        if (fits)
        {
            std::vector<CoordinateAssignment> assignment;
            for (std::size_t t = 0; t < tasks; ++t)
                assignment.push_back({def.Tasks[t].Id, def.Members[pick[t]].Id});
            return assignment;
        }
        // Advance the odometer, the last task moving fastest.
        std::size_t at = tasks;
        while (at > 0 && ++pick[at - 1] == members)
            pick[--at] = 0;
        if (at == 0)
            return {};
    }
}

std::vector<std::string> CoordinateCriticalHolders(const CoordinateCaseDef& def)
{
    std::vector<std::string> holders;
    for (const CoordinateFact& fact : def.Facts)
    {
        if (fact.Critical && !CoordinateHas(holders, fact.Holder))
            holders.push_back(fact.Holder);
    }
    return holders;
}

std::vector<std::string> CoordinateKnownFacts(const CoordinateCaseDef& def, const std::vector<std::string>& heard)
{
    std::vector<std::string> known;
    for (const CoordinateFact& fact : def.Facts)
    {
        const auto holder = std::find_if(def.Members.begin(), def.Members.end(), [&](const CoordinateMember& m) { return m.Id == fact.Holder; });
        if (CoordinateHas(heard, fact.Holder) || (holder != def.Members.end() && holder->Self))
            known.push_back(fact.Id);
    }
    return known;
}

std::int32_t CoordinateBudgetUnits(CoordinateScarce scarce) noexcept
{
    return StructureBudgetUnits(scarce == CoordinateScarce::Time);
}

std::int32_t CoordinateAssignCost(const CoordinateCaseDef& def, CoordinateGoal goal, bool methodKnown) noexcept
{
    const std::int32_t base = methodKnown ? CoordinateAssignKnownUnits : CoordinateAssignDeriveUnits;
    return CoordinateDiscountsAssign(def, goal) ? std::max<std::int32_t>(1, base - StructureDiscountUnits) : base;
}

std::int32_t CoordinateExchangeCost(const CoordinateCaseDef& def, CoordinateGoal goal) noexcept
{
    const std::int32_t base = CoordinateExchangeUnits + (def.Scarce == CoordinateScarce::Attention ? CoordinateAttentionSurcharge : 0);
    return CoordinateDiscountsExchange(def, goal) ? std::max<std::int32_t>(0, base - StructureDiscountUnits) : base;
}

std::int32_t CoordinateSettleCost(const CoordinateCaseDef& def, CoordinateGoal goal, CoordinateResolution resolution) noexcept
{
    if (resolution == CoordinateResolution::Leave || resolution == CoordinateResolution::None)
        return 0;
    const std::int32_t base = CoordinateSettleUnits + (def.Scarce == CoordinateScarce::Space ? CoordinateSpaceSurcharge : 0);
    return CoordinateDiscountsSettle(def, goal) ? std::max<std::int32_t>(1, base - StructureDiscountUnits) : base;
}

std::int32_t CoordinateSubmitCost(const CoordinateCaseDef& def, CoordinateGoal goal, bool checked) noexcept
{
    if (!checked)
        return 0;
    const std::int32_t base = CoordinateCheckUnits + (def.Scarce == CoordinateScarce::Materials ? CoordinateMaterialsSurcharge : 0);
    return CoordinateDiscountsSubmit(def, goal) ? std::max<std::int32_t>(0, base - StructureDiscountUnits) : base;
}

// ---- Loading ----------------------------------------------------------------------

namespace
{
/** The cheapest honest Sound completion, for a character with no method, under every goal. */
bool CoordinateCompletable(const CoordinateCaseDef& def)
{
    const std::int32_t holders = static_cast<std::int32_t>(CoordinateCriticalHolders(def).size());
    for (const CoordinateGoal goal : {CoordinateGoal::OrganiseWork, CoordinateGoal::HearEveryone, CoordinateGoal::SettleConstraint, CoordinateGoal::SubmitCleanly})
    {
        const std::int32_t total = CoordinateAssignCost(def, goal, false) + holders * CoordinateExchangeCost(def, goal) +
                                   CoordinateSettleCost(def, goal, CoordinateMatchingResolution(def.Constraint)) +
                                   CoordinateSubmitCost(def, goal, def.Seam);
        if (total > CoordinateBudgetUnits(def.Scarce))
            return false;
    }
    return true;
}

template <typename Item>
bool CoordinateIdsUnique(const std::vector<Item>& items)
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

std::string CoordinateLoadCase(const ContentRecord& record, const StructureFamilyHeader& family, CoordinateCaseDef& def)
{
    if (const std::string error = LoadCaseBase(record, family, def); !error.empty())
        return error;
    if (!ModelParseName(CoordinateRoleNames, JsonString(&record.Data, "role"), def.Role) ||
        !ModelParseName(CoordinateScarceNames, JsonString(&record.Data, "scarce"), def.Scarce) ||
        !ModelParseName(CoordinateConstraintNames, JsonString(&record.Data, "constraint"), def.Constraint))
        return "role, scarce and constraint must each be a known name";
    def.Seam = JsonFlag(&record.Data, "seam", false);
    for (const JsonValue& entry : JsonItems(&record.Data, "members"))
    {
        CoordinateMember member;
        member.Id = JsonString(&entry, "id");
        member.Name = JsonString(&entry, "name");
        member.Self = JsonFlag(&entry, "self", false);
        if (!JsonInt32(&entry, "capacity", member.Capacity) || member.Capacity < 1 || member.Capacity > 3)
            return "member " + member.Id + " needs a capacity from 1 to 3";
        def.Members.push_back(std::move(member));
    }
    for (const JsonValue& entry : JsonItems(&record.Data, "tasks"))
    {
        CoordinateTask task;
        task.Id = JsonString(&entry, "id");
        task.Name = JsonString(&entry, "name");
        task.Fits = JsonStrings(&entry, "fits");
        if (!JsonInt32(&entry, "load", task.Load) || task.Load < 1 || task.Load > 2)
            return "task " + task.Id + " needs a load of 1 or 2";
        def.Tasks.push_back(std::move(task));
    }
    for (const JsonValue& entry : JsonItems(&record.Data, "facts"))
        def.Facts.push_back({JsonString(&entry, "id"), JsonString(&entry, "holder"), JsonString(&entry, "text"), JsonFlag(&entry, "critical", false)});
    if (!CoordinateIdsUnique(def.Members) || !CoordinateIdsUnique(def.Tasks) || !CoordinateIdsUnique(def.Facts))
        return "member, task and fact IDs must be well-formed and unique";
    if (def.Members.size() < 2 || def.Members.size() > StructureMaxListed || def.Tasks.empty() || def.Tasks.size() > StructureMaxListed)
        return "a case needs at least two members and at least one task";
    if (std::count_if(def.Members.begin(), def.Members.end(), [](const CoordinateMember& m) { return m.Self; }) != 1)
        return "exactly one member must be the player's own (self)";
    const auto isMember = [&](const std::string& id) {
        return std::any_of(def.Members.begin(), def.Members.end(), [&](const CoordinateMember& m) { return m.Id == id; });
    };
    for (const CoordinateTask& task : def.Tasks)
    {
        if (task.Fits.empty() || !std::all_of(task.Fits.begin(), task.Fits.end(), isMember))
            return "task " + task.Id + " must be suited to at least one member of the case";
    }
    for (const CoordinateFact& fact : def.Facts)
    {
        const auto holder = std::find_if(def.Members.begin(), def.Members.end(), [&](const CoordinateMember& m) { return m.Id == fact.Holder; });
        if (holder == def.Members.end() || (fact.Critical && holder->Self))
            return "fact " + fact.Id + " must be held by a member, and a critical fact by a classmate";
    }
    if (CoordinateCriticalHolders(def).empty())
        return "a case needs at least one critical fact";
    if (CoordinateSoundAssignment(def).empty())
        return "no assignment keeps every member within capacity and every task with a member who suits it";
    if (!CoordinateCompletable(def))
        return "cannot be completed within its " + std::to_string(CoordinateBudgetUnits(def.Scarce)) + " units";
    return {};
}
} // namespace

CoordinateCatalogueResult LoadCoordinateCatalogue(const ContentDb& content)
{
    CoordinateCatalogueResult result;
    for (const ContentRecord* record : content.OfType("coordinate_family"))
    {
        CoordinateFamilyDef def;
        if (const std::string error = LoadFamilyHeader(*record, def); !error.empty())
        {
            result.Error = record->Id + ": " + error;
            result.Catalogue.Families.clear();
            return result;
        }
        result.Catalogue.Families.push_back(std::move(def));
    }
    for (const ContentRecord* record : content.OfType("coordinate_case"))
    {
        const std::string familyId = JsonString(&record->Data, "family");
        const auto family = std::find_if(result.Catalogue.Families.begin(), result.Catalogue.Families.end(),
                                         [&](const CoordinateFamilyDef& f) { return f.Id == familyId; });
        CoordinateCaseDef def;
        const std::string error = family == result.Catalogue.Families.end() ? "its family " + familyId + " does not exist"
                                                                            : CoordinateLoadCase(*record, *family, def);
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

bool CoordinateFamilySecure(const CoordinateState& state, std::string_view familyId) noexcept
{
    const StructureFamilyRecord* record = state.FindFamily(familyId);
    return record != nullptr && record->Secure;
}

StructureSelection SelectCoordinateCase(const CoordinateCatalogue& catalogue, const CoordinateState& state, std::string_view familyId,
                                        std::uint64_t worldSeed, GameInstant at)
{
    const CoordinateFamilyDef* family = catalogue.FindFamily(familyId);
    if (family == nullptr)
    {
        StructureSelection refused;
        refused.Error = StructureError::UnknownFamily;
        return refused;
    }
    std::vector<std::string> ids;
    for (const CoordinateCaseDef& def : family->Cases)
        ids.push_back(def.Id);
    return StructureSelectCase(ids, state.FindFamily(familyId), familyId, worldSeed, at);
}
} // namespace PerfectWorld
