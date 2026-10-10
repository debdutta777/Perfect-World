#include "ContentSchemaBuilders.h"
#include "ContentStructureChecks.h"
#include "StructureCoordinate.h"
#include "StructureExplain.h"
#include "StructureInvestigate.h"

#include <map>
#include <set>

// The explain, investigate and coordinate structure records, their problem families
// and cases (ROADMAP M2-09; SCHOOL_AND_PROGRESSION "Four interaction structures" and
// "What varies on the fortieth class": "Do not randomise these independently. Author
// compatible cases and track which the player has seen."). One file per record under
// Data/Content/structure, explain_family, explain_case, investigate_family,
// investigate_case, coordinate_family and coordinate_case.
//
// They are separate record types from the model structure's problem_family and
// problem_case because a case's content is the structure's own (claims and evidence,
// causes and tests, members and facts), and the model records keep their M2-08 shape.
// The same rules hold as for the model family: a family declares the variation it
// allows, every case fits its template and the family's lists, and across its cases
// the family must really vary (no two cases share the same unknown, role and scarce
// resource, and every listed value is used). The structure record's lists are
// checked against the runner's constants so the data and the code cannot drift apart.
namespace PerfectWorld::ContentSchema
{
using namespace Build;
using namespace StructureChecks;

namespace
{
// Chosen bounds.
constexpr std::int64_t StructureMaxCases = 64;
constexpr std::int64_t StructureMaxMilli = 1'000'000;
// "ROADMAP M2-09: one small family each (3+ cases)".
constexpr std::int64_t StructureSmallFamilyMinCases = 3;

template <std::size_t N>
std::vector<std::string> StructNamesOf(const std::array<std::string_view, N>& names, std::size_t skip = 0)
{
    return std::vector<std::string>(names.begin() + static_cast<std::ptrdiff_t>(skip), names.end());
}

const TypeSchema& StructMethodType()
{
    static const TypeSchema s{"structure_method", "", {Str("id", true), Str("name", true), Int("knownAtMilli", true, 0, StructureMaxMilli)}};
    return s;
}

// ---- explain ----------------------------------------------------------------------

const TypeSchema& ExplainTemplate()
{
    static const TypeSchema s{"explain_template", "", {
        Int("minClaims", true, 2, 8), Int("maxClaims", true, 2, 8), Int("minDefensible", true, 1, 8), Int("minSupport", true, 1, 4),
        Int("minEvidence", true, 1, 16), Int("maxEvidence", true, 1, 16), Int("minLimitations", true, 1, 16), Int("maxLimitations", true, 1, 16)}};
    return s;
}

const TypeSchema& ExplainClaimType()
{
    static const TypeSchema s{"explain_claim", "", {Str("id", true), Str("name", true), Flag("defensible", true)}};
    return s;
}

const TypeSchema& ExplainEvidenceType()
{
    static const TypeSchema s{"explain_evidence", "", {Str("id", true), Str("name", true), Strs("supports", true), Strs("against", true)}};
    return s;
}

const TypeSchema& ExplainLimitationType()
{
    static const TypeSchema s{"explain_limitation", "", {Str("id", true), Str("name", true), Strs("bounds", true)}};
    return s;
}

// ---- investigate ------------------------------------------------------------------

const TypeSchema& InvestigateTemplate()
{
    static const TypeSchema s{"investigate_template", "", {
        Int("minCauses", true, 2, 16), Int("maxCauses", true, 2, 16), Int("minTests", true, 1, 8), Int("maxTests", true, 1, 8)}};
    return s;
}

const TypeSchema& InvestigateCauseType()
{
    static const TypeSchema s{"investigate_cause", "", {Str("id", true), Str("name", true), Flag("fault", true)}};
    return s;
}

const TypeSchema& InvestigateTestType()
{
    static const TypeSchema s{"investigate_test", "", {
        Str("id", true), Str("name", true), OneOf("kind", true, StructNamesOf(InvestigateTestKindNames)), Int("units", true, 1, 3),
        Strs("implicates", true)}};
    return s;
}

// ---- coordinate -------------------------------------------------------------------

const TypeSchema& CoordinateTemplate()
{
    static const TypeSchema s{"coordinate_template", "", {
        Int("minMembers", true, 2, 8), Int("maxMembers", true, 2, 8), Int("minTasks", true, 1, 8), Int("maxTasks", true, 1, 8),
        Int("maxCriticalHolders", true, 1, 4)}};
    return s;
}

const TypeSchema& CoordinateMemberType()
{
    static const TypeSchema s{"coordinate_member", "", {Str("id", true), Str("name", true), Int("capacity", true, 1, 3), Flag("self", true)}};
    return s;
}

const TypeSchema& CoordinateTaskType()
{
    static const TypeSchema s{"coordinate_task", "", {Str("id", true), Str("name", true), Int("load", true, 1, 2), Strs("fits", true)}};
    return s;
}

const TypeSchema& CoordinateFactType()
{
    static const TypeSchema s{"coordinate_fact", "", {Str("id", true), Str("holder", true), Str("text", true), Flag("critical", true)}};
    return s;
}

std::vector<FieldSpec> StructFamilyFields(const TypeSchema& templateType, std::vector<FieldSpec> axes)
{
    std::vector<FieldSpec> fields = {Str("name", true), Ref("structure", true, "structure"), Ref("skill", true, "skill"),
                                     OneOf("setting", true, {"classroom", "assessment", "workplace"}),
                                     Int("minCases", true, 1, StructureMaxCases)};
    fields.insert(fields.end(), axes.begin(), axes.end());
    fields.push_back(Obj("methods", true, &StructMethodType(), true));
    fields.push_back(Obj("template", true, &templateType, false));
    return fields;
}

// ---- The rules, by structure ------------------------------------------------------

/** One varied dimension of a family: the family's list, the case's field, and the values the runner knows. */
struct StructAxis
{
    std::string ListField;
    std::string CaseField;
    std::string Label;
    std::vector<std::string> Allowed;
};

struct StructKind
{
    std::string Engine;
    std::string CaseType;
    /** Subject, then the varied unknown, then role, then scarce resource. */
    std::vector<StructAxis> Axes;
    /** Template pairs that must not be inverted, as (min field, max field). */
    std::vector<std::pair<std::string, std::string>> Ranges;
    void (*CheckCase)(const CaseContext&) = nullptr;
};

const StructKind& StructKindOf(const std::string& engine)
{
    static const StructKind explain{"explain", "explain_case",
        {{"subjects", "subject", "subject", StructNamesOf(ExplainSubjectNames)}, {"questions", "question", "question", StructNamesOf(ExplainQuestionNames)},
         {"roles", "role", "role", StructNamesOf(ExplainRoleNames)}, {"scarce", "scarce", "scarce resource", StructNamesOf(ExplainScarceNames)}},
        {{"minClaims", "maxClaims"}, {"minEvidence", "maxEvidence"}, {"minLimitations", "maxLimitations"}}, &CheckExplainCase};
    static const StructKind investigate{"investigate", "investigate_case",
        {{"subjects", "subject", "subject", StructNamesOf(InvestigateSubjectNames)}, {"symptoms", "symptom", "symptom", StructNamesOf(InvestigateSymptomNames)},
         {"roles", "role", "role", StructNamesOf(InvestigateRoleNames)}, {"scarce", "scarce", "scarce resource", StructNamesOf(InvestigateScarceNames)}},
        {{"minCauses", "maxCauses"}, {"minTests", "maxTests"}}, &CheckInvestigateCase};
    static const StructKind coordinate{"coordinate", "coordinate_case",
        {{"subjects", "subject", "subject", StructNamesOf(CoordinateSubjectNames)}, {"constraints", "constraint", "constraint", StructNamesOf(CoordinateConstraintNames)},
         {"roles", "role", "role", StructNamesOf(CoordinateRoleNames)}, {"scarce", "scarce", "scarce resource", StructNamesOf(CoordinateScarceNames)}},
        {{"minMembers", "maxMembers"}, {"minTasks", "maxTasks"}}, &CheckCoordinateCase};
    return engine == "explain" ? explain : engine == "investigate" ? investigate : coordinate;
}

void StructCheckNames(std::vector<ContentIssue>& issues, const ContentRecord& record, const StructAxis& axis)
{
    const std::vector<std::string> values = StrsOf(&record.Data, axis.ListField);
    if (values.empty())
        Report(issues, record, "the family's " + axis.ListField + " list is empty");
    std::set<std::string> seen;
    for (const std::string& value : values)
    {
        if (!Contains(axis.Allowed, value))
            Report(issues, record, "the family's " + axis.ListField + " names " + value + ", which is not a known value");
        if (!seen.insert(value).second)
            Report(issues, record, "the family's " + axis.ListField + " names " + value + " twice");
    }
}

void StructCheckFamily(const ContentDb& db, std::vector<ContentIssue>& issues, const ContentRecord& family, const StructKind& kind)
{
    for (const StructAxis& axis : kind.Axes)
        StructCheckNames(issues, family, axis);

    const JsonValue* tmpl = family.Data.Find("template");
    for (const auto& [low, high] : kind.Ranges)
    {
        if (IntOf(tmpl, low) > IntOf(tmpl, high))
            Report(issues, family, "the template's " + low + " is above its " + high);
    }

    std::set<std::string> methodIds;
    const std::vector<JsonValue>& methods = ItemsOf(&family.Data, "methods");
    if (methods.empty())
        Report(issues, family, "the family needs at least one method");
    for (const JsonValue& method : methods)
    {
        const std::string id = StrOf(&method, "id");
        if (!ModelIdWellFormed(id) || !methodIds.insert(id).second)
            Report(issues, family, "method IDs must be well-formed and unique (" + id + ")");
    }

    const ContentRecord* structure = db.Find(StrOf(&family.Data, "structure"));
    if (structure != nullptr && StrOf(&structure->Data, "engine") != kind.Engine)
        Report(issues, family, "needs a structure whose engine is " + kind.Engine + ", but its structure's engine is " + StrOf(&structure->Data, "engine"));

    const std::int64_t minCases = IntOf(&family.Data, "minCases");
    if (StrOf(&family.Data, "setting") == "classroom" && minCases < StructureSmallFamilyMinCases)
        Report(issues, family, "a classroom " + kind.Engine + " family declares at least " + std::to_string(StructureSmallFamilyMinCases) + " cases");

    std::vector<const ContentRecord*> cases;
    for (const ContentRecord* c : db.OfType(kind.CaseType))
    {
        if (StrOf(&c->Data, "family") == family.Id)
            cases.push_back(c);
    }
    if (static_cast<std::int64_t>(cases.size()) < minCases)
        Report(issues, family, "needs at least " + std::to_string(minCases) + " cases, found " + std::to_string(cases.size()));
    if (static_cast<std::int64_t>(cases.size()) > StructureMaxCases)
        Report(issues, family, "has more than " + std::to_string(StructureMaxCases) + " cases");

    // The varied unknown, the role and the scarce resource: no two cases share all three.
    std::map<std::string, std::string> variation;
    for (const ContentRecord* c : cases)
    {
        std::string key;
        for (std::size_t i = 1; i < kind.Axes.size(); ++i)
            key += StrOf(&c->Data, kind.Axes[i].CaseField) + "/";
        const auto [it, fresh] = variation.emplace(key, c->Id);
        if (!fresh)
            Report(issues, *c, "repeats the " + kind.Axes[1].Label + ", role and scarce resource of " + it->second + "; the variation must be real");
    }
    for (const StructAxis& axis : kind.Axes)
    {
        for (const std::string& value : StrsOf(&family.Data, axis.ListField))
        {
            const bool used = std::any_of(cases.begin(), cases.end(), [&](const ContentRecord* c) { return StrOf(&c->Data, axis.CaseField) == value; });
            if (!used)
                Report(issues, family, "no case uses the " + axis.Label + " " + value + " the family lists");
        }
    }
    for (const std::string& id : methodIds)
    {
        const bool used = std::any_of(cases.begin(), cases.end(), [&](const ContentRecord* c) { return StrOf(&c->Data, "method") == id; });
        if (!used)
            Report(issues, family, "no case uses the method " + id + " the family lists");
    }
}

void StructCheckCase(const ContentDb& db, std::vector<ContentIssue>& issues, const ContentRecord& c, const StructKind& kind)
{
    const ContentRecord* family = db.Find(StrOf(&c.Data, "family"));
    if (family == nullptr)
        return;
    for (const StructAxis& axis : kind.Axes)
    {
        const std::string value = StrOf(&c.Data, axis.CaseField);
        if (!Contains(StrsOf(&family->Data, axis.ListField), value))
            Report(issues, c, "its " + axis.Label + " " + value + " is not a " + axis.Label + " the family allows");
    }
    const std::string method = StrOf(&c.Data, "method");
    const bool found = std::any_of(ItemsOf(&family->Data, "methods").begin(), ItemsOf(&family->Data, "methods").end(),
                                   [&](const JsonValue& m) { return StrOf(&m, "id") == method; });
    if (!found)
        Report(issues, c, "the method " + method + " is not one of the family's methods");
    kind.CheckCase({c, *family, family->Data.Find("template"), issues});
}

template <std::size_t N>
void StructExpectList(std::vector<ContentIssue>& issues, const ContentRecord& record, std::string_view field, const std::array<std::string_view, N>& names)
{
    const std::vector<std::string> expected = StructNamesOf(names);
    if (StrsOf(&record.Data, field) != expected)
    {
        std::string list;
        for (const std::string& name : expected)
            list += (list.empty() ? "" : ", ") + name;
        Report(issues, record, "the " + StrOf(&record.Data, "engine") + " structure's " + std::string(field) + " must be " + list + ", in that order");
    }
}

void StructCheckStructure(std::vector<ContentIssue>& issues, const ContentRecord& record)
{
    const std::string engine = StrOf(&record.Data, "engine");
    if (engine == "explain")
    {
        StructExpectList(issues, record, "beats", ExplainBeatNames);
        StructExpectList(issues, record, "goals", ExplainGoalNames);
        StructExpectList(issues, record, "decisions", ExplainDecisionNames);
        StructExpectList(issues, record, "failures", ExplainFailureNames);
    }
    else if (engine == "investigate")
    {
        StructExpectList(issues, record, "beats", InvestigateBeatNames);
        StructExpectList(issues, record, "goals", InvestigateGoalNames);
        StructExpectList(issues, record, "decisions", InvestigateDecisionNames);
        StructExpectList(issues, record, "failures", InvestigateFailureNames);
    }
    else if (engine == "coordinate")
    {
        StructExpectList(issues, record, "beats", CoordinateBeatNames);
        StructExpectList(issues, record, "goals", CoordinateGoalNames);
        StructExpectList(issues, record, "decisions", CoordinateDecisionNames);
        StructExpectList(issues, record, "failures", CoordinateFailureNames);
    }
    else
        return;
    if (IntOf(&record.Data, "periodUnits") != ModelPeriodUnits || IntOf(&record.Data, "unitMinutes") != ModelUnitMinutes)
        Report(issues, record, "the " + engine + " structure's period is " + std::to_string(ModelPeriodUnits) + " units of " +
                                   std::to_string(ModelUnitMinutes) + " minutes");
}
} // namespace

const TypeSchema& ExplainFamilySchema()
{
    static const TypeSchema s{"explain_family", "explain_family.", Base(StructFamilyFields(ExplainTemplate(), {
        Strs("subjects", true), Strs("questions", true), Strs("roles", true), Strs("scarce", true)}))};
    return s;
}

const TypeSchema& ExplainCaseSchema()
{
    static const TypeSchema s{"explain_case", "explain_case.", Base({
        Ref("family", true, "explain_family"), Str("name", true), OneOf("subject", true, StructNamesOf(ExplainSubjectNames)),
        OneOf("question", true, StructNamesOf(ExplainQuestionNames)), OneOf("role", true, StructNamesOf(ExplainRoleNames)),
        OneOf("scarce", true, StructNamesOf(ExplainScarceNames)), Str("method", true), Str("brief", true),
        Obj("claims", true, &ExplainClaimType(), true), Obj("evidence", true, &ExplainEvidenceType(), true),
        Obj("limitations", true, &ExplainLimitationType(), true)})};
    return s;
}

const TypeSchema& InvestigateFamilySchema()
{
    static const TypeSchema s{"investigate_family", "investigate_family.", Base(StructFamilyFields(InvestigateTemplate(), {
        Strs("subjects", true), Strs("symptoms", true), Strs("roles", true), Strs("scarce", true)}))};
    return s;
}

const TypeSchema& InvestigateCaseSchema()
{
    static const TypeSchema s{"investigate_case", "investigate_case.", Base({
        Ref("family", true, "investigate_family"), Str("name", true), OneOf("subject", true, StructNamesOf(InvestigateSubjectNames)),
        OneOf("symptom", true, StructNamesOf(InvestigateSymptomNames)), OneOf("role", true, StructNamesOf(InvestigateRoleNames)),
        OneOf("scarce", true, StructNamesOf(InvestigateScarceNames)), Str("method", true), Str("brief", true),
        Obj("causes", true, &InvestigateCauseType(), true), Obj("tests", true, &InvestigateTestType(), true)})};
    return s;
}

const TypeSchema& CoordinateFamilySchema()
{
    static const TypeSchema s{"coordinate_family", "coordinate_family.", Base(StructFamilyFields(CoordinateTemplate(), {
        Strs("subjects", true), Strs("constraints", true), Strs("roles", true), Strs("scarce", true)}))};
    return s;
}

const TypeSchema& CoordinateCaseSchema()
{
    static const TypeSchema s{"coordinate_case", "coordinate_case.", Base({
        Ref("family", true, "coordinate_family"), Str("name", true), OneOf("subject", true, StructNamesOf(CoordinateSubjectNames)),
        OneOf("constraint", true, StructNamesOf(CoordinateConstraintNames)), OneOf("role", true, StructNamesOf(CoordinateRoleNames)),
        OneOf("scarce", true, StructNamesOf(CoordinateScarceNames)), Str("method", true), Str("brief", true), Flag("seam", true),
        Obj("members", true, &CoordinateMemberType(), true), Obj("tasks", true, &CoordinateTaskType(), true),
        Obj("facts", true, &CoordinateFactType(), true)})};
    return s;
}

void CheckStructureSemantics(const ContentDb& db, std::vector<ContentIssue>& issues)
{
    for (const ContentRecord* structure : db.OfType("structure"))
        StructCheckStructure(issues, *structure);
    for (const char* engine : {"explain", "investigate", "coordinate"})
    {
        const StructKind& kind = StructKindOf(engine);
        for (const ContentRecord* family : db.OfType(std::string(engine) + "_family"))
            StructCheckFamily(db, issues, *family, kind);
        for (const ContentRecord* c : db.OfType(kind.CaseType))
            StructCheckCase(db, issues, *c, kind);
    }
}
} // namespace PerfectWorld::ContentSchema
