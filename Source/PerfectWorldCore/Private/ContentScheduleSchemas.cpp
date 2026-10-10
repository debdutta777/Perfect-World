#include "ContentSchemaBuilders.h"

#include "Attendance.h"
#include "AttendanceInternal.h"

#include <algorithm>
#include <map>

// The bell_schedule record (ROADMAP M2-07; SCHOOL_AND_PROGRESSION.md, INDIA_WORLD_BIBLE.md
// "School bible"). One file per stream under Data/Content/bell_schedule. The per-record
// rules live in ValidateBellSchedule (Attendance.h) so the validator and the loader apply
// exactly the same rules; this file adds the shape and the rules across records.
namespace PerfectWorld::ContentSchema
{
using namespace Build;

namespace
{
// Chosen bounds on the record's integers; ValidateBellSchedule holds the real rules.
constexpr std::int64_t ScheduleSchemaMinutesPerDay = 1440;
constexpr std::int64_t ScheduleSchemaMaxBells = 64;

const TypeSchema& ScheduleSchemaBlock()
{
    static const TypeSchema s{"bell_block", "", {
        OneOf("kind", true, {"assembly", "period", "break"}), Int("start", true, 0, ScheduleSchemaMinutesPerDay - 1),
        Int("end", true, 1, ScheduleSchemaMinutesPerDay), Str("room", true), Str("subject", false)}};
    return s;
}

std::string ScheduleSchemaString(const JsonValue& object, std::string_view name)
{
    const JsonValue* value = object.Find(name);
    return value != nullptr && value->IsString() ? value->String : std::string();
}

void ScheduleSchemaIssue(std::vector<ContentIssue>& issues, const ContentRecord& record, std::string message)
{
    issues.push_back({record.File, record.Id, std::move(message)});
}

/** The stage's declared streams, or empty when it declares none. */
std::vector<std::string> ScheduleSchemaStreams(const ContentDb& db, const std::string& stageId)
{
    std::vector<std::string> streams;
    const ContentRecord* stage = db.Find(stageId);
    const JsonValue* list = stage != nullptr ? stage->Data.Find("streams") : nullptr;
    if (list != nullptr && list->IsArray())
    {
        for (const JsonValue& item : list->Array)
        {
            if (item.IsString())
                streams.push_back(item.String);
        }
    }
    return streams;
}

void ScheduleSchemaCheckSchool(const ContentDb& db, const ContentRecord& record, const BellSchedule& schedule,
                               std::vector<ContentIssue>& issues)
{
    const ContentRecord* school = db.Find(schedule.InstitutionId);
    if (school == nullptr)
        return;
    if (ScheduleSchemaString(school->Data, "kind") != "school")
        ScheduleSchemaIssue(issues, record, "the institution " + schedule.InstitutionId + " is not a school");
    const JsonValue* stages = school->Data.Find("stages");
    bool offered = false;
    if (stages != nullptr && stages->IsArray())
    {
        for (const JsonValue& stage : stages->Array)
            offered = offered || (stage.IsString() && stage.String == schedule.StageId);
    }
    if (!offered)
        ScheduleSchemaIssue(issues, record, "the school " + schedule.InstitutionId + " does not offer " + schedule.StageId);
    const std::vector<std::string> streams = ScheduleSchemaStreams(db, schedule.StageId);
    if (std::find(streams.begin(), streams.end(), schedule.Stream) == streams.end())
        ScheduleSchemaIssue(issues, record, "the stage " + schedule.StageId + " has no " + schedule.Stream + " stream");
}

/** One school and stage: every stream the stage declares has exactly one schedule, and they share one set of bells. */
void ScheduleSchemaCheckGroup(const ContentDb& db, const std::string& institution, const std::string& stage,
                              const std::vector<std::pair<const ContentRecord*, BellSchedule>>& group,
                              std::vector<ContentIssue>& issues)
{
    const ContentRecord& anchor = *group.front().first;
    std::map<std::string, int> perStream;
    for (const auto& [record, schedule] : group)
    {
        if (++perStream[schedule.Stream] == 2)
            ScheduleSchemaIssue(issues, *record, "a second " + schedule.Stream + " schedule for " + institution + " at " + stage);
        if (schedule.Bells != group.front().second.Bells)
            ScheduleSchemaIssue(issues, *record,
                                "its bells differ from " + group.front().second.Id + ": one school rings one set of bells");
    }
    for (const std::string& stream : ScheduleSchemaStreams(db, stage))
    {
        if (perStream.count(stream) == 0)
            ScheduleSchemaIssue(issues, anchor, institution + " at " + stage + " has no schedule for the " + stream +
                                                    " stream: every stream needs a full day");
    }
}
} // namespace

const TypeSchema& BellScheduleSchema()
{
    static const TypeSchema s{"bell_schedule", "bell_schedule.", Base({
        Str("name", true), Ref("institution", true, "institution"), Ref("stage", true, "education_stage"),
        OneOf("stream", true, {"science", "commerce", "humanities"}),
        Int("gatesOpen", true, 0, ScheduleSchemaMinutesPerDay - 1),
        Ints("bells", true, 0, ScheduleSchemaMinutesPerDay), Obj("blocks", true, &ScheduleSchemaBlock(), true)})};
    return s;
}

void CheckBellScheduleSemantics(const ContentDb& db, std::vector<ContentIssue>& issues)
{
    std::map<std::pair<std::string, std::string>, std::vector<std::pair<const ContentRecord*, BellSchedule>>> groups;
    for (const ContentRecord* record : db.OfType("bell_schedule"))
    {
        BellSchedule schedule = AttendanceInternal::ParseBellScheduleRecord(*record);
        if (schedule.Bells.size() > static_cast<std::size_t>(ScheduleSchemaMaxBells))
            ScheduleSchemaIssue(issues, *record, "a day has at most " + std::to_string(ScheduleSchemaMaxBells) + " bells");
        for (const std::string& problem : ValidateBellSchedule(schedule))
            ScheduleSchemaIssue(issues, *record, problem);
        ScheduleSchemaCheckSchool(db, *record, schedule, issues);
        groups[{schedule.InstitutionId, schedule.StageId}].emplace_back(record, std::move(schedule));
    }
    for (const auto& [key, group] : groups)
        ScheduleSchemaCheckGroup(db, key.first, key.second, group, issues);
}
} // namespace PerfectWorld::ContentSchema
