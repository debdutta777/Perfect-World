#include "StructureExplain.h"

#include "StructureInternal.h"

#include <algorithm>

// The explain catalogue, costs, plans and case selection (ROADMAP M2-09). The rules
// are documented in Public/StructureExplain.h; the exposure runner is in
// StructureExplainRun.cpp and the save text in StructureExplainText.cpp.
namespace PerfectWorld
{
using namespace StructureInternal;

namespace
{
bool ExplainDiscountsClaim(const ExplainCaseDef& def, ExplainGoal goal)
{
    return def.Role == ExplainRole::Author || goal == ExplainGoal::ArgueClaim;
}

bool ExplainDiscountsEvidence(const ExplainCaseDef& def, ExplainGoal goal)
{
    return def.Role == ExplainRole::Researcher || goal == ExplainGoal::WeighEvidence;
}

bool ExplainDiscountsLimitation(const ExplainCaseDef& def, ExplainGoal goal)
{
    return def.Role == ExplainRole::Critic || goal == ExplainGoal::StateLimits;
}

bool ExplainHas(const std::vector<std::string>& ids, const std::string& id)
{
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}
} // namespace

std::int32_t ExplainBudgetUnits(ExplainScarce scarce) noexcept
{
    return StructureBudgetUnits(scarce == ExplainScarce::Time);
}

std::int32_t ExplainParagraphCost(const ExplainCaseDef& def) noexcept
{
    const std::int32_t base = ExplainParagraphUnits + (def.Scarce == ExplainScarce::Help ? ExplainHelpSurcharge : 0);
    return def.Role == ExplainRole::Recorder ? std::max<std::int32_t>(0, base - StructureDiscountUnits) : base;
}

std::int32_t ExplainClaimCost(const ExplainCaseDef& def, ExplainGoal goal, bool methodShown) noexcept
{
    const std::int32_t base = methodShown ? ExplainReadKnownUnits : ExplainReadDeriveUnits;
    return ExplainDiscountsClaim(def, goal) ? std::max<std::int32_t>(1, base - StructureDiscountUnits) : base;
}

std::int32_t ExplainEvidenceCost(const ExplainCaseDef& def, ExplainGoal goal, std::size_t items) noexcept
{
    const std::int32_t each = ExplainEvidenceUnits + (def.Scarce == ExplainScarce::Sources ? ExplainSourcesSurcharge : 0);
    const std::int32_t total = each * static_cast<std::int32_t>(items);
    return items > 0 && ExplainDiscountsEvidence(def, goal) ? std::max<std::int32_t>(0, total - StructureDiscountUnits) : total;
}

std::int32_t ExplainLimitationCost(const ExplainCaseDef& def, ExplainGoal goal) noexcept
{
    const std::int32_t base = ExplainLimitationUnits + (def.Scarce == ExplainScarce::Counterpoint ? ExplainCounterpointSurcharge : 0);
    return ExplainDiscountsLimitation(def, goal) ? std::max<std::int32_t>(0, base - StructureDiscountUnits) : base;
}

ExplainPlan ExplainPlanFor(const ExplainCaseDef& def, std::string_view claimId)
{
    const auto claim = std::find_if(def.Claims.begin(), def.Claims.end(), [&](const ExplainClaim& c) { return c.Id == claimId; });
    if (claim == def.Claims.end() || !claim->Defensible)
        return {};
    const std::string id(claimId);
    const auto evidence = std::find_if(def.Evidence.begin(), def.Evidence.end(), [&](const ExplainEvidence& e) { return ExplainHas(e.Supports, id); });
    const auto limitation = std::find_if(def.Limitations.begin(), def.Limitations.end(), [&](const ExplainLimitation& l) { return ExplainHas(l.Bounds, id); });
    if (evidence == def.Evidence.end() || limitation == def.Limitations.end())
        return {};
    return {id, evidence->Id, limitation->Id};
}

// ---- Loading ----------------------------------------------------------------------

namespace
{
/** The cheapest honest Sound completion, for a character with no method, under the dearest goal. */
bool ExplainCompletable(const ExplainCaseDef& def)
{
    for (const ExplainGoal goal : {ExplainGoal::ArgueClaim, ExplainGoal::WeighEvidence, ExplainGoal::StateLimits})
    {
        const std::int32_t claim = std::min(ExplainParagraphCost(def) + ExplainClaimCost(def, goal, true), ExplainClaimCost(def, goal, false));
        const std::int32_t total = claim + ExplainEvidenceCost(def, goal, 1) + ExplainLimitationCost(def, goal);
        if (total > ExplainBudgetUnits(def.Scarce))
            return false;
    }
    return true;
}

template <typename Item>
bool ExplainIdsUnique(const std::vector<Item>& items)
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

bool ExplainKnownClaims(const ExplainCaseDef& def, const std::vector<std::string>& ids)
{
    return std::all_of(ids.begin(), ids.end(), [&](const std::string& id) {
        return std::any_of(def.Claims.begin(), def.Claims.end(), [&](const ExplainClaim& c) { return c.Id == id; });
    });
}

std::string ExplainLoadCase(const ContentRecord& record, const StructureFamilyHeader& family, ExplainCaseDef& def)
{
    if (const std::string error = LoadCaseBase(record, family, def); !error.empty())
        return error;
    if (!ModelParseName(ExplainQuestionNames, JsonString(&record.Data, "question"), def.Question) ||
        !ModelParseName(ExplainRoleNames, JsonString(&record.Data, "role"), def.Role) ||
        !ModelParseName(ExplainScarceNames, JsonString(&record.Data, "scarce"), def.Scarce))
        return "question, role and scarce must each be a known name";
    for (const JsonValue& entry : JsonItems(&record.Data, "claims"))
        def.Claims.push_back({JsonString(&entry, "id"), JsonString(&entry, "name"), JsonFlag(&entry, "defensible", false)});
    for (const JsonValue& entry : JsonItems(&record.Data, "evidence"))
        def.Evidence.push_back({JsonString(&entry, "id"), JsonString(&entry, "name"), JsonStrings(&entry, "supports"), JsonStrings(&entry, "against")});
    for (const JsonValue& entry : JsonItems(&record.Data, "limitations"))
        def.Limitations.push_back({JsonString(&entry, "id"), JsonString(&entry, "name"), JsonStrings(&entry, "bounds")});
    if (!ExplainIdsUnique(def.Claims) || !ExplainIdsUnique(def.Evidence) || !ExplainIdsUnique(def.Limitations))
        return "claim, evidence and limitation IDs must be well-formed and unique";
    if (def.Claims.size() > StructureMaxListed || def.Evidence.size() > StructureMaxListed || def.Limitations.size() > StructureMaxListed)
        return "a case lists at most " + std::to_string(StructureMaxListed) + " claims, evidence items or limitations";
    for (const ExplainEvidence& item : def.Evidence)
    {
        if (!ExplainKnownClaims(def, item.Supports) || !ExplainKnownClaims(def, item.Against))
            return "evidence " + item.Id + " names a claim the case does not have";
    }
    for (const ExplainLimitation& limitation : def.Limitations)
    {
        if (!ExplainKnownClaims(def, limitation.Bounds))
            return "limitation " + limitation.Id + " names a claim the case does not have";
    }
    std::size_t defensible = 0;
    for (const ExplainClaim& claim : def.Claims)
    {
        if (!claim.Defensible)
            continue;
        ++defensible;
        if (ExplainPlanFor(def, claim.Id).ClaimId.empty())
            return "the defensible claim " + claim.Id + " has no supporting evidence or no bounding limitation";
    }
    if (defensible == 0 || defensible == def.Claims.size())
        return "a case needs at least one defensible and one indefensible claim";
    if (!ExplainCompletable(def))
        return "cannot be completed within its " + std::to_string(ExplainBudgetUnits(def.Scarce)) + " units";
    return {};
}
} // namespace

ExplainCatalogueResult LoadExplainCatalogue(const ContentDb& content)
{
    ExplainCatalogueResult result;
    for (const ContentRecord* record : content.OfType("explain_family"))
    {
        ExplainFamilyDef def;
        if (const std::string error = LoadFamilyHeader(*record, def); !error.empty())
        {
            result.Error = record->Id + ": " + error;
            result.Catalogue.Families.clear();
            return result;
        }
        result.Catalogue.Families.push_back(std::move(def));
    }
    for (const ContentRecord* record : content.OfType("explain_case"))
    {
        const std::string familyId = JsonString(&record->Data, "family");
        const auto family = std::find_if(result.Catalogue.Families.begin(), result.Catalogue.Families.end(),
                                         [&](const ExplainFamilyDef& f) { return f.Id == familyId; });
        ExplainCaseDef def;
        const std::string error = family == result.Catalogue.Families.end() ? "its family " + familyId + " does not exist"
                                                                            : ExplainLoadCase(*record, *family, def);
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

bool ExplainFamilySecure(const ExplainState& state, std::string_view familyId) noexcept
{
    const StructureFamilyRecord* record = state.FindFamily(familyId);
    return record != nullptr && record->Secure;
}

StructureSelection SelectExplainCase(const ExplainCatalogue& catalogue, const ExplainState& state, std::string_view familyId,
                                     std::uint64_t worldSeed, GameInstant at)
{
    const ExplainFamilyDef* family = catalogue.FindFamily(familyId);
    if (family == nullptr)
    {
        StructureSelection refused;
        refused.Error = StructureError::UnknownFamily;
        return refused;
    }
    std::vector<std::string> ids;
    for (const ExplainCaseDef& def : family->Cases)
        ids.push_back(def.Id);
    return StructureSelectCase(ids, state.FindFamily(familyId), familyId, worldSeed, at);
}
} // namespace PerfectWorld
