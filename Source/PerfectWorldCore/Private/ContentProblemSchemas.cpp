#include "ContentSchemaBuilders.h"
#include "StructureModel.h"

#include <algorithm>
#include <map>
#include <set>

// The structure, problem family and problem case records (ROADMAP M2-08;
// SCHOOL_AND_PROGRESSION "Four interaction structures" and "What varies on the
// fortieth class": "Do not randomise these independently. Author compatible cases
// and track which the player has seen."). One file per record under
// Data/Content/structure, problem_family and problem_case.
//
// A family declares a template and the variation it allows; every case must fit the
// template and the family's lists, and across its cases the family must really vary
// (no two cases share their unknown, role and scarce resource, and every listed value
// is used). The runner's constants (beats, goals, claims, units) are checked against
// the structure record so the data and the code cannot drift apart.
namespace PerfectWorld::ContentSchema
{
using namespace Build;

namespace
{
// Chosen bounds.
constexpr std::int64_t ProblemMaxCases = 64;
constexpr std::int64_t ProblemMaxMilli = 1'000'000;
// "ROADMAP M2-08: at least 8 compatible cases" for the classroom family of a model structure.
constexpr std::int64_t ProblemClassroomMinCases = 8;

template <std::size_t N>
std::vector<std::string> ProblemNames(const std::array<std::string_view, N>& names, std::size_t skip = 0)
{
    return std::vector<std::string>(names.begin() + static_cast<std::ptrdiff_t>(skip), names.end());
}

const TypeSchema& ProblemMethod()
{
    static const TypeSchema s{"problem_method", "", {
        Str("id", true), Str("name", true), Strs("unknowns", true), Int("knownAtMilli", true, 0, ProblemMaxMilli)}};
    return s;
}

const TypeSchema& ProblemTemplate()
{
    static const TypeSchema s{"problem_template", "", {
        Int("stocks", true, 1, 4), Int("times", true, 1, 4), Int("minFlows", true, 1, 8), Int("maxFlows", true, 1, 8),
        Int("maxContexts", true, 0, 8), Int("minDistractors", true, 1, 8), Int("maxDistractors", true, 1, 8)}};
    return s;
}

const TypeSchema& ProblemQuantity()
{
    static const TypeSchema s{"problem_quantity", "", {
        Str("id", true), Str("name", true), OneOf("kind", true, ProblemNames(ModelQuantityKindNames)), Flag("given", true)}};
    return s;
}

const TypeSchema& ProblemLink()
{
    static const TypeSchema s{"problem_link", "", {
        Str("id", true), Str("from", true), Str("to", true), OneOf("effect", true, {"adds", "subtracts"}), Flag("sound", true)}};
    return s;
}

void ProblemIssue(std::vector<ContentIssue>& issues, const ContentRecord& record, std::string message)
{
    issues.push_back({record.File, record.Id, std::move(message)});
}

std::string ProblemString(const JsonValue* object, std::string_view name)
{
    const JsonValue* value = object != nullptr ? object->Find(name) : nullptr;
    return value != nullptr && value->IsString() ? value->String : std::string();
}

std::int64_t ProblemInt(const JsonValue* object, std::string_view name)
{
    const JsonValue* value = object != nullptr ? object->Find(name) : nullptr;
    return value != nullptr && value->Integer ? *value->Integer : 0;
}

std::vector<std::string> ProblemStrings(const JsonValue* object, std::string_view name)
{
    std::vector<std::string> out;
    const JsonValue* value = object != nullptr ? object->Find(name) : nullptr;
    if (value != nullptr && value->IsArray())
    {
        for (const JsonValue& item : value->Array)
            if (item.IsString())
                out.push_back(item.String);
    }
    return out;
}

bool ProblemHas(const std::vector<std::string>& list, const std::string& item)
{
    return std::find(list.begin(), list.end(), item) != list.end();
}

// ---- structure --------------------------------------------------------------------

template <std::size_t N>
void ProblemExpectList(std::vector<ContentIssue>& issues, const ContentRecord& record, std::string_view field,
                       const std::array<std::string_view, N>& names, std::size_t skip)
{
    const std::vector<std::string> expected = ProblemNames(names, skip);
    if (ProblemStrings(&record.Data, field) != expected)
    {
        std::string list;
        for (const std::string& name : expected)
            list += (list.empty() ? "" : ", ") + name;
        ProblemIssue(issues, record, "the model structure's " + std::string(field) + " must be " + list + ", in that order");
    }
}

void ProblemCheckStructure(std::vector<ContentIssue>& issues, const ContentRecord& record)
{
    if (ProblemString(&record.Data, "engine") != "model")
        return;
    ProblemExpectList(issues, record, "beats", ModelBeatNames, 0);
    ProblemExpectList(issues, record, "goals", ModelGoalNames, 0);
    ProblemExpectList(issues, record, "investigations", ModelInvestigationNames, 1);
    ProblemExpectList(issues, record, "claims", ModelClaimNames, 0);
    if (ProblemInt(&record.Data, "periodUnits") != ModelPeriodUnits || ProblemInt(&record.Data, "unitMinutes") != ModelUnitMinutes)
        ProblemIssue(issues, record, "the model structure's period is " + std::to_string(ModelPeriodUnits) + " units of " +
                                         std::to_string(ModelUnitMinutes) + " minutes");
}

// ---- family -----------------------------------------------------------------------

template <std::size_t N>
void ProblemCheckNames(std::vector<ContentIssue>& issues, const ContentRecord& record, std::string_view field,
                       const std::array<std::string_view, N>& allowed)
{
    const std::vector<std::string> values = ProblemStrings(&record.Data, field);
    if (values.empty())
        ProblemIssue(issues, record, "the family's " + std::string(field) + " list is empty");
    std::set<std::string> seen;
    for (const std::string& value : values)
    {
        if (std::none_of(allowed.begin(), allowed.end(), [&](std::string_view name) { return name == value; }))
            ProblemIssue(issues, record, "the family's " + std::string(field) + " names " + value + ", which is not a known value");
        if (!seen.insert(value).second)
            ProblemIssue(issues, record, "the family's " + std::string(field) + " names " + value + " twice");
    }
}

void ProblemCheckMethods(std::vector<ContentIssue>& issues, const ContentRecord& family)
{
    std::set<std::string> ids;
    const JsonValue* methods = family.Data.Find("methods");
    if (methods == nullptr || !methods->IsArray() || methods->Array.empty())
    {
        ProblemIssue(issues, family, "the family needs at least one method");
        return;
    }
    for (const JsonValue& method : methods->Array)
    {
        const std::string id = ProblemString(&method, "id");
        if (!ModelIdWellFormed(id) || !ids.insert(id).second)
            ProblemIssue(issues, family, "method IDs must be well-formed and unique (" + id + ")");
        const std::vector<std::string> unknowns = ProblemStrings(&method, "unknowns");
        if (unknowns.empty())
            ProblemIssue(issues, family, "method " + id + " solves for no unknown");
        for (const std::string& unknown : unknowns)
        {
            if (std::none_of(ModelUnknownNames.begin(), ModelUnknownNames.end(), [&](std::string_view n) { return n == unknown; }))
                ProblemIssue(issues, family, "method " + id + " names the unknown " + unknown + ", which is not a known value");
        }
    }
}

/** Every value the family lists must be used by at least one of its cases: the variation is real. */
void ProblemCheckCoverage(std::vector<ContentIssue>& issues, const ContentRecord& family, std::string_view listField,
                          std::string_view caseField, std::string_view label, const std::vector<const ContentRecord*>& cases)
{
    for (const std::string& value : ProblemStrings(&family.Data, listField))
    {
        const bool used = std::any_of(cases.begin(), cases.end(), [&](const ContentRecord* c) { return ProblemString(&c->Data, caseField) == value; });
        if (!used)
            ProblemIssue(issues, family, "no case uses the " + std::string(label) + " " + value + " the family lists");
    }
}

void ProblemCheckFamily(const ContentDb& db, std::vector<ContentIssue>& issues, const ContentRecord& family)
{
    const JsonValue& data = family.Data;
    ProblemCheckNames(issues, family, "subjects", ModelSubjectNames);
    ProblemCheckNames(issues, family, "unknowns", ModelUnknownNames);
    ProblemCheckNames(issues, family, "roles", ModelRoleNames);
    ProblemCheckNames(issues, family, "scarce", ModelScarceNames);
    ProblemCheckNames(issues, family, "faults", ModelFaultNames);
    ProblemCheckMethods(issues, family);
    const JsonValue* tmpl = data.Find("template");
    if (ProblemInt(tmpl, "minFlows") > ProblemInt(tmpl, "maxFlows"))
        ProblemIssue(issues, family, "the template's minFlows is above its maxFlows");
    if (ProblemInt(tmpl, "minDistractors") > ProblemInt(tmpl, "maxDistractors"))
        ProblemIssue(issues, family, "the template's minDistractors is above its maxDistractors");

    const ContentRecord* structure = db.Find(ProblemString(&data, "structure"));
    const bool classroomModel = structure != nullptr && ProblemString(&structure->Data, "engine") == "model" &&
                                ProblemString(&data, "setting") == "classroom";
    const std::int64_t minCases = ProblemInt(&data, "minCases");
    if (classroomModel && minCases < ProblemClassroomMinCases)
        ProblemIssue(issues, family, "a classroom model family declares at least " + std::to_string(ProblemClassroomMinCases) + " cases");

    std::vector<const ContentRecord*> cases;
    for (const ContentRecord* c : db.OfType("problem_case"))
    {
        if (ProblemString(&c->Data, "family") == family.Id)
            cases.push_back(c);
    }
    if (static_cast<std::int64_t>(cases.size()) < minCases)
        ProblemIssue(issues, family, "needs at least " + std::to_string(minCases) + " cases, found " + std::to_string(cases.size()));
    if (static_cast<std::int64_t>(cases.size()) > ProblemMaxCases)
        ProblemIssue(issues, family, "has more than " + std::to_string(ProblemMaxCases) + " cases");

    std::map<std::string, std::string> variation;
    for (const ContentRecord* c : cases)
    {
        const std::string key = ProblemString(&c->Data, "unknown") + "/" + ProblemString(&c->Data, "role") + "/" + ProblemString(&c->Data, "scarce");
        const auto [it, fresh] = variation.emplace(key, c->Id);
        if (!fresh)
            ProblemIssue(issues, *c, "repeats the unknown, role and scarce resource of " + it->second + "; the variation must be real");
    }
    ProblemCheckCoverage(issues, family, "subjects", "subject", "subject", cases);
    ProblemCheckCoverage(issues, family, "unknowns", "unknown", "unknown", cases);
    ProblemCheckCoverage(issues, family, "roles", "role", "role", cases);
    ProblemCheckCoverage(issues, family, "scarce", "scarce", "scarce resource", cases);
    ProblemCheckCoverage(issues, family, "faults", "fault", "fault", cases);
    const JsonValue* methods = data.Find("methods");
    if (methods != nullptr && methods->IsArray())
    {
        for (const JsonValue& method : methods->Array)
        {
            const std::string id = ProblemString(&method, "id");
            const bool used = std::any_of(cases.begin(), cases.end(), [&](const ContentRecord* c) { return ProblemString(&c->Data, "method") == id; });
            if (!used)
                ProblemIssue(issues, family, "no case uses the method " + id + " the family lists");
        }
    }
}

// ---- case -------------------------------------------------------------------------

struct ProblemQuantityInfo
{
    std::string Kind;
    bool Given = true;
};

void ProblemCheckAllowed(std::vector<ContentIssue>& issues, const ContentRecord& c, const ContentRecord& family, std::string_view field,
                         std::string_view list, std::string_view label)
{
    const std::string value = ProblemString(&c.Data, field);
    if (!ProblemHas(ProblemStrings(&family.Data, list), value))
        ProblemIssue(issues, c, "its " + std::string(label) + " " + value + " is not a " + std::string(label) + " the family allows");
}

void ProblemCheckQuantities(std::vector<ContentIssue>& issues, const ContentRecord& c, const JsonValue* tmpl,
                            std::map<std::string, ProblemQuantityInfo>& byId)
{
    std::int64_t stocks = 0, times = 0, flows = 0, contexts = 0, unknowns = 0;
    std::string unknownKind;
    const JsonValue* quantities = c.Data.Find("quantities");
    if (quantities == nullptr || !quantities->IsArray())
        return;
    for (const JsonValue& q : quantities->Array)
    {
        const std::string id = ProblemString(&q, "id");
        const std::string kind = ProblemString(&q, "kind");
        const JsonValue* given = q.Find("given");
        const bool isGiven = given == nullptr || given->Bool;
        if (!ModelIdWellFormed(id) || !byId.emplace(id, ProblemQuantityInfo{kind, isGiven}).second)
            ProblemIssue(issues, c, "quantity IDs must be well-formed and unique (" + id + ")");
        stocks += kind == "stock" ? 1 : 0;
        times += kind == "time" ? 1 : 0;
        flows += kind == "inflow" || kind == "outflow" ? 1 : 0;
        contexts += kind == "context" ? 1 : 0;
        if (!isGiven)
        {
            ++unknowns;
            unknownKind = kind;
        }
    }
    if (stocks != ProblemInt(tmpl, "stocks"))
        ProblemIssue(issues, c, "the family template wants exactly " + std::to_string(ProblemInt(tmpl, "stocks")) +
                                    " stock quantity, found " + std::to_string(stocks));
    if (times != ProblemInt(tmpl, "times"))
        ProblemIssue(issues, c, "the family template wants exactly " + std::to_string(ProblemInt(tmpl, "times")) +
                                    " time quantity, found " + std::to_string(times));
    if (flows < ProblemInt(tmpl, "minFlows") || flows > ProblemInt(tmpl, "maxFlows"))
        ProblemIssue(issues, c, "the family template wants " + std::to_string(ProblemInt(tmpl, "minFlows")) + " to " +
                                    std::to_string(ProblemInt(tmpl, "maxFlows")) + " flow quantities, found " + std::to_string(flows));
    if (contexts > ProblemInt(tmpl, "maxContexts"))
        ProblemIssue(issues, c, "the family template allows at most " + std::to_string(ProblemInt(tmpl, "maxContexts")) +
                                    " context quantities, found " + std::to_string(contexts));
    if (unknowns != 1)
        ProblemIssue(issues, c, "exactly one quantity must be unknown (given false), found " + std::to_string(unknowns));
    else if (unknownKind != ProblemString(&c.Data, "unknown"))
        ProblemIssue(issues, c, "the unknown quantity is a " + unknownKind + ", but the case says its unknown is a " +
                                    ProblemString(&c.Data, "unknown"));
}

void ProblemCheckLinks(std::vector<ContentIssue>& issues, const ContentRecord& c, const JsonValue* tmpl,
                       const std::map<std::string, ProblemQuantityInfo>& byId)
{
    const JsonValue* links = c.Data.Find("links");
    if (links == nullptr || !links->IsArray())
        return;
    std::set<std::string> linkIds;
    std::set<std::string> shapes;
    std::map<std::string, std::int64_t> soundFrom;
    std::int64_t distractors = 0;
    for (const JsonValue& link : links->Array)
    {
        const std::string id = ProblemString(&link, "id");
        const std::string from = ProblemString(&link, "from");
        const std::string to = ProblemString(&link, "to");
        const std::string effect = ProblemString(&link, "effect");
        const JsonValue* flag = link.Find("sound");
        const bool sound = flag != nullptr && flag->Bool;
        if (!ModelIdWellFormed(id) || !linkIds.insert(id).second)
            ProblemIssue(issues, c, "link IDs must be well-formed and unique (" + id + ")");
        const auto source = byId.find(from);
        const auto target = byId.find(to);
        if (source == byId.end() || target == byId.end() || from == to)
        {
            ProblemIssue(issues, c, "link " + id + " must join two different quantities of the case");
            continue;
        }
        if (!shapes.insert(from + ">" + to + ">" + effect).second)
            ProblemIssue(issues, c, "link " + id + " duplicates another link");
        if (!sound)
        {
            ++distractors;
            continue;
        }
        const std::string& kind = source->second.Kind;
        const bool fits = target->second.Kind == "stock" && ((kind == "inflow" && effect == "adds") || (kind == "outflow" && effect == "subtracts") ||
                                                             (kind == "time" && effect == "adds"));
        if (!fits)
            ProblemIssue(issues, c, "sound link " + id + " must join an inflow (adds), an outflow (subtracts) or the time (adds) to the stock");
        ++soundFrom[from];
    }
    for (const auto& [id, info] : byId)
    {
        const bool needs = info.Kind == "inflow" || info.Kind == "outflow" || info.Kind == "time";
        const auto it = soundFrom.find(id);
        const std::int64_t found = it == soundFrom.end() ? 0 : it->second;
        if (needs && found != 1)
            ProblemIssue(issues, c, "needs exactly one sound link from " + id + ", found " + std::to_string(found));
        if (!needs && found != 0)
            ProblemIssue(issues, c, "a " + info.Kind + " quantity (" + id + ") has no sound link");
    }
    if (distractors < ProblemInt(tmpl, "minDistractors") || distractors > ProblemInt(tmpl, "maxDistractors"))
        ProblemIssue(issues, c, "the family template wants " + std::to_string(ProblemInt(tmpl, "minDistractors")) + " to " +
                                    std::to_string(ProblemInt(tmpl, "maxDistractors")) + " unsound links, found " + std::to_string(distractors));
}

void ProblemCheckCase(const ContentDb& db, std::vector<ContentIssue>& issues, const ContentRecord& c)
{
    const ContentRecord* family = db.Find(ProblemString(&c.Data, "family"));
    if (family == nullptr)
        return;
    ProblemCheckAllowed(issues, c, *family, "subject", "subjects", "subject");
    ProblemCheckAllowed(issues, c, *family, "unknown", "unknowns", "unknown");
    ProblemCheckAllowed(issues, c, *family, "role", "roles", "role");
    ProblemCheckAllowed(issues, c, *family, "scarce", "scarce", "scarce resource");
    ProblemCheckAllowed(issues, c, *family, "fault", "faults", "fault");

    const std::string method = ProblemString(&c.Data, "method");
    const JsonValue* methods = family->Data.Find("methods");
    const JsonValue* found = nullptr;
    if (methods != nullptr && methods->IsArray())
    {
        for (const JsonValue& m : methods->Array)
            if (ProblemString(&m, "id") == method)
                found = &m;
    }
    if (found == nullptr)
        ProblemIssue(issues, c, "the method " + method + " is not one of the family's methods");
    else if (!ProblemHas(ProblemStrings(found, "unknowns"), ProblemString(&c.Data, "unknown")))
        ProblemIssue(issues, c, "the method " + method + " does not solve for an unknown " + ProblemString(&c.Data, "unknown"));

    const JsonValue* tmpl = family->Data.Find("template");
    std::map<std::string, ProblemQuantityInfo> byId;
    ProblemCheckQuantities(issues, c, tmpl, byId);
    ProblemCheckLinks(issues, c, tmpl, byId);
}
} // namespace

const TypeSchema& StructureSchema()
{
    static const TypeSchema s{"structure", "structure.", Base({
        Str("name", true), OneOf("engine", true, {"model", "explain", "investigate", "coordinate"}), Strs("beats", true),
        Strs("goals", false), Strs("investigations", false), Strs("claims", false), Strs("decisions", false), Strs("failures", false),
        Int("periodUnits", true, 1, 48),
        Int("unitMinutes", true, 1, 60)})};
    return s;
}

const TypeSchema& ProblemFamilySchema()
{
    static const TypeSchema s{"problem_family", "problem_family.", Base({
        Str("name", true), Ref("structure", true, "structure"), Ref("skill", true, "skill"),
        OneOf("setting", true, {"classroom", "assessment", "workplace"}), Int("minCases", true, 1, ProblemMaxCases),
        Strs("subjects", true), Strs("unknowns", true), Strs("roles", true), Strs("scarce", true), Strs("faults", true),
        Obj("methods", true, &ProblemMethod(), true), Obj("template", true, &ProblemTemplate(), false)})};
    return s;
}

const TypeSchema& ProblemCaseSchema()
{
    static const TypeSchema s{"problem_case", "problem_case.", Base({
        Ref("family", true, "problem_family"), Str("name", true), OneOf("subject", true, ProblemNames(ModelSubjectNames)),
        OneOf("unknown", true, ProblemNames(ModelUnknownNames)), OneOf("role", true, ProblemNames(ModelRoleNames)),
        OneOf("scarce", true, ProblemNames(ModelScarceNames)), OneOf("fault", true, ProblemNames(ModelFaultNames)),
        Str("method", true), Str("brief", true), Obj("quantities", true, &ProblemQuantity(), true),
        Obj("links", true, &ProblemLink(), true)})};
    return s;
}

void CheckProblemSemantics(const ContentDb& db, std::vector<ContentIssue>& issues)
{
    for (const ContentRecord* structure : db.OfType("structure"))
        ProblemCheckStructure(issues, *structure);
    for (const ContentRecord* family : db.OfType("problem_family"))
        ProblemCheckFamily(db, issues, *family);
    for (const ContentRecord* c : db.OfType("problem_case"))
        ProblemCheckCase(db, issues, *c);
}
} // namespace PerfectWorld::ContentSchema
