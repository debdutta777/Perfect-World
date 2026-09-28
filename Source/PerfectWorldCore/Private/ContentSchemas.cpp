#include "ContentSchemas.h"

#include <set>

namespace PerfectWorld::ContentSchema
{
namespace
{
FieldSpec Str(std::string name, bool required)
{
    FieldSpec f;
    f.Name = std::move(name);
    f.Kind = FieldKind::String;
    f.Required = required;
    return f;
}

FieldSpec Flag(std::string name, bool required)
{
    FieldSpec f = Str(std::move(name), required);
    f.Kind = FieldKind::Bool;
    return f;
}

FieldSpec Int(std::string name, bool required, std::int64_t min, std::int64_t max, bool nullable = false)
{
    FieldSpec f = Str(std::move(name), required);
    f.Kind = FieldKind::Int;
    f.Min = min;
    f.Max = max;
    f.Nullable = nullable;
    return f;
}

FieldSpec OneOf(std::string name, bool required, std::vector<std::string> values)
{
    FieldSpec f = Str(std::move(name), required);
    f.Kind = FieldKind::Enum;
    f.Values = std::move(values);
    return f;
}

FieldSpec Ref(std::string name, bool required, std::string type)
{
    FieldSpec f = Str(std::move(name), required);
    f.Kind = FieldKind::Ref;
    f.RefType = std::move(type);
    return f;
}

FieldSpec Refs(std::string name, bool required, std::string type)
{
    FieldSpec f = Ref(std::move(name), required, std::move(type));
    f.Kind = FieldKind::RefArray;
    return f;
}

FieldSpec Strs(std::string name, bool required)
{
    FieldSpec f = Str(std::move(name), required);
    f.Kind = FieldKind::StringArray;
    return f;
}

FieldSpec Obj(std::string name, bool required, const TypeSchema* nested, bool many)
{
    FieldSpec f = Str(std::move(name), required);
    f.Kind = many ? FieldKind::ObjectArray : FieldKind::Object;
    f.Nested = nested;
    return f;
}

const std::vector<std::string>& Ranks()
{
    static const std::vector<std::string> ranks = {"F", "E", "D", "C", "B", "A", "S"};
    return ranks;
}

// Every top-level record has these two; they are checked by the loader.
std::vector<FieldSpec> Base(std::vector<FieldSpec> fields)
{
    fields.insert(fields.begin(), {Str("type", true), Str("id", true), Str("note", false)});
    return fields;
}

const TypeSchema& District()
{
    static const TypeSchema s{"district", "district.", {
        Str("id", true), Str("name", true), OneOf("kind", true, {"authored", "generated"}), Flag("fictional", false),
        OneOf("archetype", false, {"old_dense_core", "riverfront", "market", "apartment_belt", "rail_yard", "government_quarter",
                                    "campus", "port", "informal_settlement", "industrial", "suburb", "park"}),
        Int("seed", false, 0, 4'294'967'295)}};
    return s;
}

const TypeSchema& GateSite()
{
    static const TypeSchema s{"gate_site", "gate.", {
        Str("id", true), Str("name", true), Ref("district", true, "district"), Str("location", false),
        OneOf("rank", true, Ranks()), OneOf("status", true, {"contained", "sealed", "open"}), Flag("realmAnchor", true)}};
    return s;
}

const TypeSchema& TowerExamination()
{
    static const TypeSchema s{"tower_examination", "", {
        Ref("centralHall", true, "institution"), Flag("certifiedHallPerCity", true), Str("note", false)}};
    return s;
}

const TypeSchema& Nation()
{
    static const TypeSchema s{"nation", "nation.", Base({
        Str("name", true), Ref("capitalCity", true, "city"), Ref("gateAuthority", true, "institution"),
        Ref("realmHomeRegion", true, "realm_region"), Refs("stageOneCities", true, "city"),
        Obj("towerExamination", true, &TowerExamination(), false)})};
    return s;
}

const TypeSchema& RealmRegion()
{
    static const TypeSchema s{"realm_region", "realm.region.", Base({
        Str("name", true), Int("floor", true, 1, 12), Refs("anchors", true, "gate_site")})};
    return s;
}

const TypeSchema& City()
{
    static const TypeSchema s{"city", "city.", Base({
        Str("name", true), Ref("nation", true, "nation"), Str("state", true), Str("role", true),
        Obj("districts", true, &District(), true), Obj("gateSites", true, &GateSite(), true)})};
    return s;
}

const TypeSchema& Institution()
{
    static const TypeSchema s{"institution", "institution.", Base({
        Str("name", true),
        OneOf("kind", true, {"gate_authority", "gate_authority_office", "examination_hall", "school", "gate_academy", "university",
                             "research_institute", "enterprise", "hospital", "polytechnic", "guild"}),
        Ref("schoolType", false, "school_type"), Ref("universityTier", false, "university_tier"),
        Refs("stages", false, "education_stage"), Ref("city", true, "city"), Ref("district", false, "district"),
        Str("location", false), Flag("tutorialDungeon", false), OneOf("certifiesUpToGrade", false, Ranks()),
        Flag("national", false)})};
    return s;
}

const TypeSchema& EducationStage()
{
    static const TypeSchema s{"education_stage", "stage.", Base({
        Str("name", true), Int("ageFrom", true, 0, 120), Int("ageTo", true, 0, 120, true), Flag("playable", true),
        Strs("streams", false), Str("endsWith", false), Flag("requiresAwakening", false)})};
    return s;
}

const TypeSchema& SchoolType()
{
    static const TypeSchema s{"school_type", "school_type.", Base({
        Str("name", true), OneOf("fees", true, {"none_or_nominal", "low", "low_to_moderate", "high", "scholarship_or_high"})})};
    return s;
}

const TypeSchema& UniversityTier()
{
    static const TypeSchema s{"university_tier", "university_tier.", Base({
        Str("name", true), OneOf("selectivity", true, {"very_high", "high", "broad", "varies"})})};
    return s;
}

const TypeSchema& Stratum()
{
    static const TypeSchema s{"stratum", "stratum.", Base({
        Int("householdResourcesRank", true, 0, 4), Str("name", true), Str("housing", true),
        OneOf("buffer", true, {"none", "small", "one_emergency", "large"}), Int("leavingCareAge", false, 16, 21)})};
    return s;
}

const TypeSchema& Resource()
{
    static const TypeSchema s{"resource", "resource.", Base({
        OneOf("scope", true, {"personal", "household", "social", "economy", "city"}), OneOf("currency", false, {"INR"}),
        Flag("lootable", false), Strs("parts", false), OneOf("gradedBy", false, {"gate_rank"}),
        OneOf("collection", false, {"licensed_only"})})};
    return s;
}

void Issue(std::vector<ContentIssue>& issues, const ContentRecord& record, std::string message)
{
    issues.push_back({record.File, record.Id, std::move(message)});
}

std::string StringField(const ContentRecord& record, std::string_view name)
{
    const JsonValue* value = record.Data.Find(name);
    return value != nullptr && value->IsString() ? value->String : std::string();
}
} // namespace

const std::vector<const TypeSchema*>& TopLevelSchemas()
{
    static const std::vector<const TypeSchema*> all = {&Nation(), &RealmRegion(), &City(), &Institution(), &EducationStage(),
                                                       &SchoolType(), &UniversityTier(), &Stratum(), &Resource()};
    return all;
}

const TypeSchema* FindTopLevel(std::string_view type)
{
    for (const TypeSchema* schema : TopLevelSchemas())
    {
        if (schema->Type == type)
        {
            return schema;
        }
    }
    return nullptr;
}

void CheckSemantics(const ContentDb& db, std::vector<ContentIssue>& issues)
{
    for (const ContentRecord* d : db.OfType("district"))
    {
        const bool generated = StringField(*d, "kind") == "generated";
        const bool hasRecipe = d->Data.Find("archetype") != nullptr && d->Data.Find("seed") != nullptr;
        if (generated && !hasRecipe)
            Issue(issues, *d, "a generated district needs an archetype and a seed");
        if (!generated && (d->Data.Find("archetype") != nullptr || d->Data.Find("seed") != nullptr))
            Issue(issues, *d, "an authored district has no archetype or seed");
    }

    std::set<std::string> listedAnchors;
    for (const ContentRecord* region : db.OfType("realm_region"))
    {
        for (const JsonValue& anchor : region->Data.Find("anchors")->Array)
        {
            listedAnchors.insert(anchor.String);
            const ContentRecord* gate = db.Find(anchor.String);
            const JsonValue* flag = gate != nullptr ? gate->Data.Find("realmAnchor") : nullptr;
            if (flag != nullptr && !flag->Bool)
                Issue(issues, *region, "anchor " + anchor.String + " is not marked realmAnchor");
        }
    }
    for (const ContentRecord* gate : db.OfType("gate_site"))
    {
        const ContentRecord* district = db.Find(StringField(*gate, "district"));
        if (district != nullptr && district->ParentId != gate->ParentId)
            Issue(issues, *gate, "its district belongs to another city");
        if (gate->Data.Find("realmAnchor")->Bool && listedAnchors.count(gate->Id) == 0)
            Issue(issues, *gate, "a realm anchor must be listed by a realm region");
    }
    for (const ContentRecord* city : db.OfType("city"))
    {
        int anchors = 0;
        for (const JsonValue& gate : city->Data.Find("gateSites")->Array)
            anchors += gate.Find("realmAnchor")->Bool ? 1 : 0;
        if (anchors != 1)
            Issue(issues, *city, "a city needs exactly one realm anchor Gate, found " + std::to_string(anchors));
    }
    for (const ContentRecord* nation : db.OfType("nation"))
    {
        for (const JsonValue& cityId : nation->Data.Find("stageOneCities")->Array)
        {
            const ContentRecord* city = db.Find(cityId.String);
            if (city != nullptr && StringField(*city, "nation") != nation->Id)
                Issue(issues, *nation, "stage-one city " + cityId.String + " belongs to another nation");
        }
    }
    for (const ContentRecord* inst : db.OfType("institution"))
    {
        const std::string kind = StringField(*inst, "kind");
        const ContentRecord* district = db.Find(StringField(*inst, "district"));
        if (district != nullptr && district->ParentId != StringField(*inst, "city"))
            Issue(issues, *inst, "its district is not in its city");
        if (kind == "school" && (inst->Data.Find("schoolType") == nullptr || inst->Data.Find("stages") == nullptr))
            Issue(issues, *inst, "a school needs a schoolType and stages");
        if (kind == "university" && inst->Data.Find("universityTier") == nullptr)
            Issue(issues, *inst, "a university needs a universityTier");
        if (kind != "school" && inst->Data.Find("schoolType") != nullptr)
            Issue(issues, *inst, "only a school has a schoolType");
    }
    for (const ContentRecord* stage : db.OfType("education_stage"))
    {
        const JsonValue* to = stage->Data.Find("ageTo");
        if (to != nullptr && to->Integer && *to->Integer < *stage->Data.Find("ageFrom")->Integer)
            Issue(issues, *stage, "ageTo is before ageFrom");
    }
    std::set<std::int64_t> ranks;
    for (const ContentRecord* stratum : db.OfType("stratum"))
    {
        if (!ranks.insert(*stratum->Data.Find("householdResourcesRank")->Integer).second)
            Issue(issues, *stratum, "two strata share a householdResourcesRank");
    }
}
} // namespace PerfectWorld::ContentSchema
