#pragma once

#include "ContentSchemas.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

// Helpers shared by ContentStructureSchemas.cpp (the record types and the family and
// structure rules) and ContentStructureCases.cpp (the per-case rules) for the explain,
// investigate and coordinate structures (ROADMAP M2-09).
namespace PerfectWorld::ContentSchema::StructureChecks
{
inline void Report(std::vector<ContentIssue>& issues, const ContentRecord& record, std::string message)
{
    issues.push_back({record.File, record.Id, std::move(message)});
}

inline std::string StrOf(const JsonValue* object, std::string_view name)
{
    const JsonValue* value = object != nullptr ? object->Find(name) : nullptr;
    return value != nullptr && value->IsString() ? value->String : std::string();
}

inline std::int64_t IntOf(const JsonValue* object, std::string_view name)
{
    const JsonValue* value = object != nullptr ? object->Find(name) : nullptr;
    return value != nullptr && value->Integer ? *value->Integer : 0;
}

inline bool FlagOf(const JsonValue* object, std::string_view name)
{
    const JsonValue* value = object != nullptr ? object->Find(name) : nullptr;
    return value != nullptr && value->Type == JsonValue::Kind::Bool && value->Bool;
}

inline std::vector<std::string> StrsOf(const JsonValue* object, std::string_view name)
{
    std::vector<std::string> out;
    const JsonValue* value = object != nullptr ? object->Find(name) : nullptr;
    if (value != nullptr && value->IsArray())
    {
        for (const JsonValue& item : value->Array)
        {
            if (item.IsString())
                out.push_back(item.String);
        }
    }
    return out;
}

inline const std::vector<JsonValue>& ItemsOf(const JsonValue* object, std::string_view name)
{
    static const std::vector<JsonValue> none;
    const JsonValue* value = object != nullptr ? object->Find(name) : nullptr;
    return value != nullptr && value->IsArray() ? value->Array : none;
}

inline bool Contains(const std::vector<std::string>& list, const std::string& item)
{
    return std::find(list.begin(), list.end(), item) != list.end();
}

/** The family's counted template (the numbers a case must fit), as the family record states them. */
struct CaseContext
{
    const ContentRecord& Case;
    const ContentRecord& Family;
    const JsonValue* Template = nullptr;
    std::vector<ContentIssue>& Issues;

    void Say(std::string message) const { Report(Issues, Case, std::move(message)); }
};

/** "wants A to B <what>, found N" when `count` is outside the template's min..max fields. */
inline void CheckCount(const CaseContext& c, std::string_view minField, std::string_view maxField, std::int64_t count, std::string_view what)
{
    const std::int64_t low = IntOf(c.Template, minField);
    const std::int64_t high = IntOf(c.Template, maxField);
    if (count < low || count > high)
        c.Say("the family template wants " + std::to_string(low) + " to " + std::to_string(high) + " " + std::string(what) + ", found " +
              std::to_string(count));
}

/** IDs of the objects in `array` that are well-formed and unique; reports the rest. Returns them in order. */
std::vector<std::string> CollectIds(const CaseContext& c, std::string_view arrayField, std::string_view label);

void CheckExplainCase(const CaseContext& c);
void CheckInvestigateCase(const CaseContext& c);
void CheckCoordinateCase(const CaseContext& c);
} // namespace PerfectWorld::ContentSchema::StructureChecks
