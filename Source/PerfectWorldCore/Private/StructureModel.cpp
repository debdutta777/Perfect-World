#include "StructureModel.h"

#include "RandomStreams.h"

#include <algorithm>
#include <limits>

// The model-a-system catalogue, costs and case selection (ROADMAP M2-08). The
// rules are documented in Public/StructureModel.h; the exposure runner is in
// StructureModelRun.cpp and the save text in StructureModelText.cpp.
namespace PerfectWorld
{
namespace
{
bool ModelDiscountsCommit(const ModelCaseDef& def, ModelGoal goal)
{
    return def.Role == ModelRole::Solver || goal == ModelGoal::UnderstandModel;
}

bool ModelDiscountsInvestigation(const ModelCaseDef& def, ModelGoal goal)
{
    return def.Role == ModelRole::Experimenter || goal == ModelGoal::TestMeasurements;
}

bool ModelDiscountsExplain(const ModelCaseDef& def, ModelGoal goal)
{
    return def.Role == ModelRole::Recorder || def.Role == ModelRole::Presenter || goal == ModelGoal::ExplainToGroup;
}
} // namespace

std::int32_t ModelBudgetUnits(ModelScarce scarce) noexcept
{
    return scarce == ModelScarce::Time ? ModelScarceTimeUnits : ModelWorkUnits;
}

ModelInvestigation ModelMatchingInvestigation(ModelFault fault) noexcept
{
    switch (fault)
    {
    case ModelFault::Timing:
        return ModelInvestigation::CheckTimes;
    case ModelFault::HiddenFlow:
        return ModelInvestigation::LookForFlow;
    case ModelFault::Scale:
        return ModelInvestigation::CompareMarkings;
    case ModelFault::None:
        break;
    }
    return ModelInvestigation::AcceptReadings;
}

std::int32_t ModelExampleCost(const ModelCaseDef& def) noexcept
{
    return ModelExampleUnits + (def.Scarce == ModelScarce::Help ? ModelHelpSurcharge : 0);
}

std::int32_t ModelCommitCost(const ModelCaseDef& def, ModelGoal goal, bool methodKnown) noexcept
{
    const std::int32_t base = methodKnown ? ModelCommitKnownUnits : ModelCommitDeriveUnits;
    return ModelDiscountsCommit(def, goal) ? std::max<std::int32_t>(1, base - ModelDiscountUnits) : base;
}

std::int32_t ModelInvestigationCost(const ModelCaseDef& def, ModelGoal goal, ModelInvestigation investigation) noexcept
{
    std::int32_t base = 0;
    switch (investigation)
    {
    case ModelInvestigation::CheckTimes:
        base = ModelCheckTimesUnits + (def.Scarce == ModelScarce::Readings ? ModelReadingsSurcharge : 0);
        break;
    case ModelInvestigation::CompareMarkings:
        base = ModelCompareMarkingsUnits + (def.Scarce == ModelScarce::Equipment ? ModelEquipmentSurcharge : 0);
        break;
    case ModelInvestigation::LookForFlow:
        base = ModelLookForFlowUnits + (def.Scarce == ModelScarce::Equipment ? ModelEquipmentSurcharge : 0);
        break;
    case ModelInvestigation::None:
    case ModelInvestigation::AcceptReadings:
        return 0;
    }
    return ModelDiscountsInvestigation(def, goal) ? std::max<std::int32_t>(0, base - ModelDiscountUnits) : base;
}

std::int32_t ModelExplainCost(const ModelCaseDef& def, ModelGoal goal) noexcept
{
    return ModelDiscountsExplain(def, goal) ? 0 : ModelExplainUnits;
}

bool ModelExplanationOwed(const ModelCaseDef& def, ModelGoal goal) noexcept
{
    return def.Role == ModelRole::Recorder || def.Role == ModelRole::Presenter || goal == ModelGoal::ExplainToGroup;
}

std::string_view ModelErrorCode(ModelError error) noexcept
{
    switch (error)
    {
    case ModelError::None:
        return {};
    case ModelError::UnknownFamily:
        return "model.unknown_family";
    case ModelError::UnknownCase:
        return "model.unknown_case";
    case ModelError::Busy:
        return "model.busy";
    case ModelError::NotActive:
        return "model.not_active";
    case ModelError::WrongBeat:
        return "model.wrong_beat";
    case ModelError::ExampleNotNeeded:
        return "model.example_not_needed";
    case ModelError::BadModel:
        return "model.bad_model";
    case ModelError::UnknownLink:
        return "model.unknown_link";
    case ModelError::UnitsShort:
        return "model.units_short";
    case ModelError::NotSecure:
        return "model.not_secure";
    case ModelError::InvalidSkill:
        return "model.invalid_skill";
    case ModelError::InvalidState:
        return "model.invalid_state";
    }
    return "model.invalid_state";
}

const ModelFamilyDef* ModelCatalogue::FindFamily(std::string_view id) const noexcept
{
    const auto it = std::lower_bound(Families.begin(), Families.end(), id,
                                     [](const ModelFamilyDef& family, std::string_view key) { return family.Id < key; });
    return it != Families.end() && it->Id == id ? &*it : nullptr;
}

const ModelCaseDef* ModelCatalogue::FindCase(std::string_view id) const noexcept
{
    for (const ModelFamilyDef& family : Families)
    {
        const auto it = std::lower_bound(family.Cases.begin(), family.Cases.end(), id,
                                         [](const ModelCaseDef& def, std::string_view key) { return def.Id < key; });
        if (it != family.Cases.end() && it->Id == id)
            return &*it;
    }
    return nullptr;
}

// ---- Loading ----------------------------------------------------------------------

namespace
{
std::string ModelLoadString(const JsonValue* object, std::string_view name)
{
    const JsonValue* value = object != nullptr ? object->Find(name) : nullptr;
    return value != nullptr && value->IsString() ? value->String : std::string();
}

bool ModelLoadInt32(const JsonValue* object, std::string_view name, std::int32_t& out)
{
    const JsonValue* value = object != nullptr ? object->Find(name) : nullptr;
    if (value == nullptr || !value->Integer || *value->Integer < std::numeric_limits<std::int32_t>::min() ||
        *value->Integer > std::numeric_limits<std::int32_t>::max())
        return false;
    out = static_cast<std::int32_t>(*value->Integer);
    return true;
}

std::string ModelLoadFamily(const ContentRecord& record, ModelFamilyDef& def)
{
    def.Id = record.Id;
    def.Name = ModelLoadString(&record.Data, "name");
    def.SkillId = ModelLoadString(&record.Data, "skill");
    if (!ModelLoadInt32(&record.Data, "minCases", def.MinCases) || def.MinCases < 1)
        return "minCases must be a positive whole number";
    const JsonValue* methods = record.Data.Find("methods");
    if (methods == nullptr || !methods->IsArray() || methods->Array.empty())
        return "a family needs at least one method";
    for (const JsonValue& entry : methods->Array)
    {
        ModelMethod method;
        method.Id = ModelLoadString(&entry, "id");
        method.Name = ModelLoadString(&entry, "name");
        if (!ModelIdWellFormed(method.Id) || !ModelLoadInt32(&entry, "knownAtMilli", method.KnownAtMilli) || method.KnownAtMilli < 0)
            return "a method needs a well-formed id and a non-negative knownAtMilli";
        if (std::any_of(def.Methods.begin(), def.Methods.end(), [&](const ModelMethod& m) { return m.Id == method.Id; }))
            return "the family names the method " + method.Id + " twice";
        def.Methods.push_back(std::move(method));
    }
    return {};
}

/** The cheapest time a careful player spends, for a character with no method, under the dearest goal. */
bool ModelCompletable(const ModelCaseDef& def)
{
    for (const ModelGoal goal : {ModelGoal::UnderstandModel, ModelGoal::TestMeasurements, ModelGoal::ExplainToGroup})
    {
        const std::int32_t commit = std::min(ModelExampleCost(def) + ModelCommitCost(def, goal, true), ModelCommitCost(def, goal, false));
        const std::int32_t total = commit + ModelInvestigationCost(def, goal, ModelMatchingInvestigation(def.Fault)) +
                                   (ModelExplanationOwed(def, goal) ? ModelExplainCost(def, goal) : 0);
        if (total > ModelBudgetUnits(def.Scarce))
            return false;
    }
    return true;
}

std::string ModelLoadCase(const ContentRecord& record, const ModelFamilyDef& family, ModelCaseDef& def)
{
    def.Id = record.Id;
    def.FamilyId = family.Id;
    def.Name = ModelLoadString(&record.Data, "name");
    def.Subject = ModelLoadString(&record.Data, "subject");
    def.Brief = ModelLoadString(&record.Data, "brief");
    def.MethodId = ModelLoadString(&record.Data, "method");
    if (!ModelParseName(ModelUnknownNames, ModelLoadString(&record.Data, "unknown"), def.Unknown) ||
        !ModelParseName(ModelRoleNames, ModelLoadString(&record.Data, "role"), def.Role) ||
        !ModelParseName(ModelScarceNames, ModelLoadString(&record.Data, "scarce"), def.Scarce) ||
        !ModelParseName(ModelFaultNames, ModelLoadString(&record.Data, "fault"), def.Fault))
        return "unknown, role, scarce and fault must each be a known name";
    const auto method = std::find_if(family.Methods.begin(), family.Methods.end(), [&](const ModelMethod& m) { return m.Id == def.MethodId; });
    if (method == family.Methods.end())
        return "the method " + def.MethodId + " is not one of the family's methods";
    def.MethodKnownAtMilli = method->KnownAtMilli;

    const JsonValue* quantities = record.Data.Find("quantities");
    const JsonValue* links = record.Data.Find("links");
    if (quantities == nullptr || !quantities->IsArray() || links == nullptr || !links->IsArray())
        return "a case needs quantities and links";
    for (const JsonValue& entry : quantities->Array)
    {
        const JsonValue* given = entry.Find("given");
        def.Quantities.push_back({ModelLoadString(&entry, "id"), ModelLoadString(&entry, "name"), ModelLoadString(&entry, "kind"),
                                  given == nullptr || given->Bool});
    }
    std::size_t sound = 0;
    for (const JsonValue& entry : links->Array)
    {
        const JsonValue* flag = entry.Find("sound");
        ModelLink link{ModelLoadString(&entry, "id"), ModelLoadString(&entry, "from"), ModelLoadString(&entry, "to"),
                       flag != nullptr && flag->Bool};
        if (!ModelIdWellFormed(link.Id) || std::any_of(def.Links.begin(), def.Links.end(), [&](const ModelLink& l) { return l.Id == link.Id; }))
            return "link IDs must be well-formed and unique";
        sound += link.Sound ? 1 : 0;
        def.Links.push_back(std::move(link));
    }
    if (sound == 0 || sound == def.Links.size() || def.Links.size() > ModelMaxLinks)
        return "a case needs at least one sound and one unsound link, and at most " + std::to_string(ModelMaxLinks) + " links";
    if (!ModelCompletable(def))
        return "cannot be completed within its " + std::to_string(ModelBudgetUnits(def.Scarce)) + " units";
    return {};
}
} // namespace

ModelCatalogueResult LoadModelCatalogue(const ContentDb& content)
{
    ModelCatalogueResult result;
    for (const ContentRecord* record : content.OfType("problem_family"))
    {
        ModelFamilyDef def;
        if (const std::string error = ModelLoadFamily(*record, def); !error.empty())
        {
            result.Error = record->Id + ": " + error;
            result.Catalogue.Families.clear();
            return result;
        }
        result.Catalogue.Families.push_back(std::move(def));
    }
    for (const ContentRecord* record : content.OfType("problem_case"))
    {
        const std::string familyId = ModelLoadString(&record->Data, "family");
        const auto family = std::find_if(result.Catalogue.Families.begin(), result.Catalogue.Families.end(),
                                         [&](const ModelFamilyDef& f) { return f.Id == familyId; });
        ModelCaseDef def;
        std::string error = family == result.Catalogue.Families.end() ? "its family " + familyId + " does not exist"
                                                                      : ModelLoadCase(*record, *family, def);
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

const ModelFamilyRecord* StructureModelState::FindFamily(std::string_view familyId) const noexcept
{
    const auto it = std::lower_bound(Families.begin(), Families.end(), familyId,
                                     [](const ModelFamilyRecord& record, std::string_view key) { return record.FamilyId < key; });
    return it != Families.end() && it->FamilyId == familyId ? &*it : nullptr;
}

bool ModelFamilySecure(const StructureModelState& state, std::string_view familyId) noexcept
{
    const ModelFamilyRecord* record = state.FindFamily(familyId);
    return record != nullptr && record->Secure;
}

ModelSelection SelectModelCase(const ModelCatalogue& catalogue, const StructureModelState& state, std::string_view familyId,
                               std::uint64_t worldSeed, GameInstant at)
{
    ModelSelection selection;
    const ModelFamilyDef* family = catalogue.FindFamily(familyId);
    if (family == nullptr)
    {
        selection.Error = ModelError::UnknownFamily;
        return selection;
    }
    if (family->Cases.empty())
    {
        selection.Error = ModelError::UnknownCase;
        return selection;
    }
    const ModelFamilyRecord* record = state.FindFamily(familyId);
    const auto contains = [](const std::vector<std::string>& ids, const std::string& id) {
        return std::find(ids.begin(), ids.end(), id) != ids.end();
    };

    // The last cases shown are not offered again: up to ModelRecentKept, but always leaving one choice.
    std::vector<std::string> excluded;
    if (record != nullptr)
    {
        const std::size_t window = std::min(ModelRecentKept, family->Cases.size() - 1);
        const std::size_t keep = std::min(window, record->Recent.size());
        excluded.assign(record->Recent.end() - static_cast<std::ptrdiff_t>(keep), record->Recent.end());
    }
    std::vector<const ModelCaseDef*> eligible;
    for (const ModelCaseDef& def : family->Cases)
    {
        if (!contains(excluded, def.Id))
            eligible.push_back(&def);
    }
    // A case the player has not met comes before one they have.
    std::vector<const ModelCaseDef*> unseen;
    if (record != nullptr)
    {
        for (const ModelCaseDef* def : eligible)
        {
            if (!std::binary_search(record->Seen.begin(), record->Seen.end(), def->Id))
                unseen.push_back(def);
        }
    }
    else
        unseen = eligible;
    selection.FromUnseen = !unseen.empty();
    const std::vector<const ModelCaseDef*>& pool = selection.FromUnseen ? unseen : eligible;
    selection.Candidates = pool.size();
    const std::array<std::int64_t, 2> key{at, record != nullptr ? static_cast<std::int64_t>(record->Exposures) : 0};
    selection.CaseId = pool[KeyedRandomBelow(worldSeed, RandomStreamId::Opportunity, pool.size(), key, familyId)]->Id;
    return selection;
}
} // namespace PerfectWorld
