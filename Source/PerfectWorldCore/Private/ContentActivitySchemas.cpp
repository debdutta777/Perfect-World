#include "ContentSchemaBuilders.h"

#include <set>

// The activity record (ROADMAP M2-03; SCHOOL_AND_PROGRESSION "Every activity
// produces three separate records"; SYSTEMS.md §2.5 "Every interaction declares
// its time contract"). One file per activity under Data/Content/activity.
namespace PerfectWorld::ContentSchema
{
using namespace Build;

namespace
{
// Chosen bounds. A day is the longest single activity (sleep is eight hours).
constexpr std::int64_t ActivityMaxMinutes = 1440;
constexpr std::int64_t ActivityMaxAllowance = 20'000; // milli-XP per completion
constexpr std::int64_t ActivityMaxEvidence = 1000;    // milli per attribute per completion
constexpr std::int64_t ActivityMaxStressDelta = 20'000;
constexpr std::int64_t ActivitySplitWhole = 1000; // the split is in parts per thousand
constexpr std::size_t ActivityMaxSkills = 4;
// "Attribute growth at most one-twentieth of skill growth" (SYSTEMS_CATALOGUE).
constexpr std::int64_t ActivityEvidenceDivisor = 20;

const TypeSchema& ActivityClock()
{
    static const TypeSchema s{"activity_clock", "", {
        OneOf("mode", true, {"free_roam", "scene", "mission", "compressed"}),
        OneOf("interrupt", true, {"none", "at_beat", "immediate"}),
        OneOf("resolve", true, {"on_complete"})}};
    return s;
}

const TypeSchema& ActivityCondition()
{
    static const TypeSchema s{"activity_condition", "", {
        OneOf("activity", true, {"awake", "resting", "sleeping", "light_exertion", "heavy_exertion"}),
        OneOf("meal", false, {"snack", "light", "full", "feast"}),
        Int("stressDelta", false, -ActivityMaxStressDelta, ActivityMaxStressDelta)}};
    return s;
}

const TypeSchema& ActivityShare()
{
    static const TypeSchema s{"activity_share", "", {
        Ref("skill", true, "skill"), Int("permille", true, 1, ActivitySplitWhole)}};
    return s;
}

const TypeSchema& ActivityLearning()
{
    static const TypeSchema s{"activity_learning", "", {
        Int("allowanceMilliXp", true, 1, ActivityMaxAllowance), Obj("split", true, &ActivityShare(), true)}};
    return s;
}

const TypeSchema& ActivityEvidence()
{
    static const TypeSchema s{"activity_evidence", "", {
        Ref("attribute", true, "attribute"), Int("milli", true, 1, ActivityMaxEvidence)}};
    return s;
}

void ActivityIssue(std::vector<ContentIssue>& issues, const ContentRecord& record, std::string message)
{
    issues.push_back({record.File, record.Id, std::move(message)});
}

std::string ActivityString(const JsonValue* object, std::string_view name)
{
    const JsonValue* value = object != nullptr ? object->Find(name) : nullptr;
    return value != nullptr && value->IsString() ? value->String : std::string();
}

std::int64_t ActivityInt(const JsonValue* object, std::string_view name)
{
    const JsonValue* value = object != nullptr ? object->Find(name) : nullptr;
    return value != nullptr && value->Integer ? *value->Integer : 0;
}

void ActivityCheckSplit(std::vector<ContentIssue>& issues, const ContentRecord& record, const JsonValue& learning)
{
    const JsonValue* split = learning.Find("split");
    if (split == nullptr || !split->IsArray())
        return;
    if (split->Array.empty() || split->Array.size() > ActivityMaxSkills)
        ActivityIssue(issues, record, "the learning split needs between one and four skills");
    std::int64_t total = 0;
    std::set<std::string> seen;
    for (const JsonValue& share : split->Array)
    {
        total += ActivityInt(&share, "permille");
        const std::string skill = ActivityString(&share, "skill");
        if (!seen.insert(skill).second)
            ActivityIssue(issues, record, "the learning split names " + skill + " twice");
    }
    if (total != ActivitySplitWhole)
        ActivityIssue(issues, record, "the learning split sums to " + std::to_string(total) + " parts per thousand, not exactly " +
                                          std::to_string(ActivitySplitWhole));
}

void ActivityCheckEvidence(std::vector<ContentIssue>& issues, const ContentRecord& record, const JsonValue* learning)
{
    const JsonValue* evidence = record.Data.Find("evidence");
    if (evidence == nullptr || !evidence->IsArray() || evidence->Array.empty())
        return;
    if (learning == nullptr)
    {
        ActivityIssue(issues, record, "attribute evidence needs a learning allowance to bound it");
        return;
    }
    std::int64_t total = 0;
    std::set<std::string> seen;
    for (const JsonValue& entry : evidence->Array)
    {
        total += ActivityInt(&entry, "milli");
        const std::string attribute = ActivityString(&entry, "attribute");
        if (!seen.insert(attribute).second)
            ActivityIssue(issues, record, "attribute evidence names " + attribute + " twice");
    }
    const std::int64_t cap = ActivityInt(learning, "allowanceMilliXp") / ActivityEvidenceDivisor;
    if (total > cap)
        ActivityIssue(issues, record, "attribute evidence totals " + std::to_string(total) + " milli, above one twentieth (" +
                                          std::to_string(cap) + ") of the learning allowance");
}
} // namespace

const TypeSchema& ActivitySchema()
{
    static const TypeSchema s{"activity", "activity.", Base({
        Str("name", true), Int("durationMinutes", true, 1, ActivityMaxMinutes), Obj("clock", true, &ActivityClock(), false),
        OneOf("place", true, {"home", "classroom", "dining", "outdoors", "any"}),
        Obj("condition", true, &ActivityCondition(), false),
        OneOf("outcome", true, {"studied", "attended_class", "slept", "ate", "chore_done", "exercised", "rested"}),
        Obj("learning", false, &ActivityLearning(), false), Obj("evidence", false, &ActivityEvidence(), true)})};
    return s;
}

void CheckActivitySemantics(const ContentDb& db, std::vector<ContentIssue>& issues)
{
    for (const ContentRecord* record : db.OfType("activity"))
    {
        const JsonValue* learning = record->Data.Find("learning");
        if (learning != nullptr && learning->IsObject())
            ActivityCheckSplit(issues, *record, *learning);
        ActivityCheckEvidence(issues, *record, learning != nullptr && learning->IsObject() ? learning : nullptr);

        const JsonValue* condition = record->Data.Find("condition");
        const bool hasMeal = condition != nullptr && condition->Find("meal") != nullptr;
        const std::string outcome = ActivityString(&record->Data, "outcome");
        if (hasMeal != (outcome == "ate"))
            ActivityIssue(issues, *record, "a meal is eaten exactly when the outcome is ate");
        if (outcome == "slept" && ActivityString(condition, "activity") != "sleeping")
            ActivityIssue(issues, *record, "the outcome slept needs the condition activity sleeping");
    }
}
} // namespace PerfectWorld::ContentSchema
