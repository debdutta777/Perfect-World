#include "ContentSchemas.h"

#include "CharacterCreation.h"
#include "CommerceContent.h"
#include "ContentSchemaBuilders.h"
#include "FaridaSchedule.h"
#include "SchoolLifeContent.h"
#include "TowerScenarioContent.h"
#include "TutorialContent.h"

#include <algorithm>
#include <array>
#include <set>

namespace PerfectWorld::ContentSchema
{
using namespace Build;

namespace
{
const std::vector<std::string>& Ranks()
{
    static const std::vector<std::string> ranks = {"F", "E", "D", "C", "B", "A", "S"};
    return ranks;
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

// Latent stats (ROADMAP M2-02). Attribute values are Milli on 0..AttributeScaleMax;
// skill experience is whole XP (CHARACTER_CREATION.md "Canonical trait catalogue").
constexpr std::int64_t AttributeScaleMax = 100'000;
constexpr std::int64_t SkillXpMax = 10'000;

// One trait's contribution to a skill or attribute, indexed by rank 1..4 (rank 0
// contributes nothing). The trait must be one of the twenty creation traits.
const TypeSchema& SkillSeed()
{
    static const TypeSchema s{"skill_seed", "", {Str("trait", true), Ints("xpByRank", true, 0, SkillXpMax)}};
    return s;
}

const TypeSchema& AttributeSeed()
{
    static const TypeSchema s{"attribute_seed", "", {Str("trait", true), Ints("milliByRank", true, 0, AttributeScaleMax)}};
    return s;
}

const TypeSchema& Attribute()
{
    static const TypeSchema s{"attribute", "attribute.", Base({
        Str("name", true), Int("startMilli", true, 0, AttributeScaleMax), Int("maxMilli", true, 1, AttributeScaleMax),
        Obj("seeds", false, &AttributeSeed(), true)})};
    return s;
}

const TypeSchema& Skill()
{
    static const TypeSchema s{"skill", "skill.", Base({
        Str("name", true), OneOf("category", true, {"academic", "practical", "social", "physical", "creative", "personal"}),
        Int("baselineXp", true, 0, SkillXpMax), Obj("seeds", false, &SkillSeed(), true)})};
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

namespace
{
const TypeSchema& R82OpeningSchema()
{
    static const TypeSchema s{"commerce_opening", "", {
        Int("days", true, 1, 127), Int("start", true, 0, 1439), Int("end", true, 1, 1440)}};
    return s;
}
const TypeSchema& R82KeeperSchema()
{
    // People and spatial anchors belong to the lane registries, not ContentDb.
    static const TypeSchema s{"commerce_keeper", "", {Str("person", true), Str("counterAnchor", true)}};
    return s;
}
const TypeSchema& R82EntrySchema()
{
    static const TypeSchema s{"commerce_entry", "", {
        Ref("item", true, "commerce_item"), Int("unitPricePaise", true, 1, INT64_MAX)}};
    return s;
}
}
const TypeSchema& R82CommerceItemSchema()
{
    static const TypeSchema s{"commerce_item", "item.", Base({
        Str("name", true), Str("unit", true), Int("stackLimit", true, 10, 10),
        OneOf("effect", true, {"none", "tea", "food", "unsupported_supply"}), Int("hungerRelief", true, 0, 5000)})};
    return s;
}
const TypeSchema& R82CommerceShopSchema()
{
    static const TypeSchema s{"commerce_shop", "shop.", Base({
        Str("name", true), Str("counterAnchor", true), Obj("keepers", true, &R82KeeperSchema(), true),
        Obj("opening", true, &R82OpeningSchema(), true), Obj("entries", true, &R82EntrySchema(), true)})};
    return s;
}
const TypeSchema& R82CommerceFundingSchema()
{
    static const TypeSchema s{"commerce_funding", "commerce_funding.", Base({
        Ref("stratum", true, "stratum"), Int("householdResourcesRank", true, 0, 4),
        Int("startingWalletPaise", true, 1, INT64_MAX), Int("weeklyAllowancePaise", true, 1, INT64_MAX)})};
    return s;
}

const std::vector<const TypeSchema*>& TopLevelSchemas()
{
    static const std::vector<const TypeSchema*> all = {&Nation(), &RealmRegion(), &City(), &Institution(), &EducationStage(),
                                                       &SchoolType(), &UniversityTier(), &Stratum(), &Resource(), &Attribute(),
                                                       &Skill(), &ActivitySchema(), &StructureSchema(), &ProblemFamilySchema(),
                                                       &ProblemCaseSchema(), &BellScheduleSchema(), &ExplainFamilySchema(),
                                                       &ExplainCaseSchema(), &InvestigateFamilySchema(), &InvestigateCaseSchema(),
                                                       &CoordinateFamilySchema(), &CoordinateCaseSchema(), &CalendarSchema(),
                                                       &AssessmentPaperSchema(), &AbilityRecipeSchema(),
                                                       &ThreadTemplateSchema(), &NpcScheduleSchema(),
                                                       &R82CommerceItemSchema(), &R82CommerceShopSchema(), &R82CommerceFundingSchema(),
                                                       &R93TutorialSchema(), &SchoolSceneSchema(), &TowerScenarioSchema()};
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

namespace
{
// ---- Latent stat seeds ------------------------------------------------------------

constexpr std::size_t SeedRanks = 4; // ranks 1..4; rank 0 contributes nothing

void SeedCheck(std::vector<ContentIssue>& issues, const ContentRecord& record, std::string_view arrayName)
{
    const JsonValue* seeds = record.Data.Find("seeds");
    if (seeds == nullptr || !seeds->IsArray())
        return;
    std::set<std::string> seen;
    for (const JsonValue& seed : seeds->Array)
    {
        const JsonValue* trait = seed.Find("trait");
        const JsonValue* ranks = seed.Find(arrayName);
        if (trait == nullptr || !trait->IsString() || ranks == nullptr || !ranks->IsArray())
            continue;
        const auto& traits = CharacterCreator::TraitDefinitions();
        if (std::none_of(traits.begin(), traits.end(), [&](const TraitDefinition& t) { return t.Id == trait->String; }))
            Issue(issues, record, "seed names " + trait->String + ", which is not a creation trait");
        if (!seen.insert(trait->String).second)
            Issue(issues, record, "two seeds name the trait " + trait->String);
        if (ranks->Array.size() != SeedRanks)
        {
            Issue(issues, record, "seed " + trait->String + " needs exactly four values, ranks 1 to 4");
            continue;
        }
        for (std::size_t i = 1; i < SeedRanks; ++i)
        {
            if (ranks->Array[i].Integer && ranks->Array[i - 1].Integer && *ranks->Array[i].Integer < *ranks->Array[i - 1].Integer)
                Issue(issues, record, "seed " + trait->String + " must not fall as the rank rises");
        }
    }
}

constexpr std::array<std::string_view, 6> AttributeIds{"attribute.body", "attribute.agility", "attribute.intelligence",
                                                       "attribute.perception", "attribute.willpower", "attribute.charisma"};
} // namespace

void CheckSemantics(const ContentDb& db, std::vector<ContentIssue>& issues)
{
    CheckActivitySemantics(db, issues);
    CheckProblemSemantics(db, issues);
    CheckStructureSemantics(db, issues);
    CheckBellScheduleSemantics(db, issues);
    CheckCalendarSemantics(db, issues);
    CheckAssessmentSemantics(db, issues);
    CheckAbilitySemantics(db, issues);
    CheckThreadSemantics(db, issues);
    ValidateNpcScheduleSemantics(db, issues);
    ValidateCommerceContentSemantics(db, issues);
    ValidateSchoolSceneSemantics(db, issues);
    ValidateTowerScenarioSemantics(db, issues);
    ValidateTutorialContentSemantics(db, issues);
    for (const ContentRecord* skill : db.OfType("skill"))
        SeedCheck(issues, *skill, "xpByRank");
    const std::vector<const ContentRecord*> attributes = db.OfType("attribute");
    for (const ContentRecord* attribute : attributes)
    {
        SeedCheck(issues, *attribute, "milliByRank");
        const JsonValue* start = attribute->Data.Find("startMilli");
        const JsonValue* max = attribute->Data.Find("maxMilli");
        if (start != nullptr && max != nullptr && start->Integer && max->Integer && *start->Integer > *max->Integer)
            Issue(issues, *attribute, "startMilli is above maxMilli");
    }
    if (!attributes.empty())
    {
        // The six attributes are a closed set (SYSTEMS_CATALOGUE.md "Attributes, skills and latent growth").
        for (const std::string_view id : AttributeIds)
        {
            if (db.Find(id) == nullptr)
                issues.push_back({attributes.front()->File, std::string(id), "the attribute set is missing " + std::string(id)});
        }
        for (const ContentRecord* attribute : attributes)
        {
            if (std::find(AttributeIds.begin(), AttributeIds.end(), attribute->Id) == AttributeIds.end())
                Issue(issues, *attribute, "not one of the six attributes: body, agility, intelligence, perception, willpower, charisma");
        }
    }

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
