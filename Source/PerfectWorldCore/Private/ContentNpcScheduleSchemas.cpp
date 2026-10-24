#include "FaridaSchedule.h"

#include "ContentSchemaBuilders.h"
#include "NpcScheduleContent.h"

#include <set>

namespace PerfectWorld::ContentSchema
{
using namespace Build;
namespace
{
const TypeSchema& RoutineShape()
{
    static const TypeSchema s{"npc_routine", "", {
        Int("days", true, 1, 127), Int("start", true, 0, 1439), Int("end", true, 1, 1440),
        Str("zone", true), Str("role", true)}};
    return s;
}
const TypeSchema& RouteShape()
{
    static const TypeSchema s{"npc_route", "", {
        Str("id", true), Str("from", true), Str("to", true), Int("lengthCm", true, 1, 100000)}};
    return s;
}
const TypeSchema& PathShape()
{
    static const TypeSchema s{"npc_travel", "", {
        Str("from", true), Str("to", true), Int("minutes", true, 1, ScheduleMaxTravelMinutes), Strs("routes", true)}};
    return s;
}

std::string Shape(const JsonValue& object, const TypeSchema& schema)
{
    if (!object.IsObject())
        return schema.Type + " must be an object";
    std::set<std::string> names;
    for (const auto& [name, value] : object.Object)
    {
        if (!names.insert(name).second)
            return "duplicate field " + name;
        const FieldSpec* spec = nullptr;
        for (const auto& field : schema.Fields)
            if (field.Name == name)
                spec = &field;
        if (spec == nullptr)
            return "unknown field " + name + " in " + schema.Type;
        switch (spec->Kind)
        {
        case FieldKind::String:
            if (!value.IsString() || value.String.empty())
                return name + " must be a nonempty string";
            break;
        case FieldKind::Int:
            if (value.Type != JsonValue::Kind::Number || !value.Integer || *value.Integer < spec->Min || *value.Integer > spec->Max)
                return name + " must be an integer in range";
            break;
        case FieldKind::StringArray:
        case FieldKind::ObjectArray:
            if (!value.IsArray() || value.Array.empty() || value.Array.size() > 128)
                return name + " must contain 1..128 items";
            for (const auto& item : value.Array)
            {
                if (spec->Kind == FieldKind::StringArray)
                {
                    if (!item.IsString() || item.String.empty())
                        return name + " requires nonempty strings";
                }
                else if (const auto error = Shape(item, *spec->Nested); !error.empty())
                    return name + ": " + error;
            }
            break;
        default:
            return "unsupported NPC schema field";
        }
    }
    for (const auto& field : schema.Fields)
        if (field.Required && object.Find(field.Name) == nullptr)
            return "missing field " + field.Name;
    return {};
}
} // namespace

const TypeSchema& NpcScheduleSchema()
{
    static const TypeSchema s{"npc_schedule", "npc_schedule.", Base({
        Str("person", true), Str("home", true), Str("workplace", true), Str("door", true),
        Int("speedCmPerMinute", true, 140, 140), Obj("routine", true, &RoutineShape(), true),
        Obj("routes", true, &RouteShape(), true), Obj("travel", true, &PathShape(), true)})};
    return s;
}

std::string ValidateNpcScheduleShape(const JsonValue& data)
{
    return Shape(data, NpcScheduleSchema());
}

void ValidateNpcScheduleSemantics(const ContentDb& db, std::vector<ContentIssue>& issues)
{
    for (const auto* record : db.OfType("npc_schedule"))
    {
        if (record->Id != FaridaScheduleId)
        {
            // Other frozen schedules (the care worker) are decoded by the generic loader; any other identity is refused there.
            const auto other = LoadNpcSchedule(db, record->Id);
            if (!other.Ok())
                issues.push_back({record->File, record->Id, other.Error});
            continue;
        }
        const auto result = LoadFaridaSchedule(db);
        if (!result.Ok())
            issues.push_back({record->File, record->Id, result.Error});
    }
}
} // namespace PerfectWorld::ContentSchema
