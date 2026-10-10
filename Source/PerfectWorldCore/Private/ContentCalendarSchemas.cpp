#include "ContentSchemaBuilders.h"

#include "SchoolCalendar.h"

#include <algorithm>
#include <map>

// The calendar record (ROADMAP M3-09; SCHOOL_AND_PROGRESSION.md, GAME_PLAN.md Phase 2). One
// file per school and stage under Data/Content/calendar. The per-calendar rules live in
// ValidateSchoolCalendar (SchoolCalendar.h) so the validator and the loader apply exactly
// the same rules; this file adds the shape and the rules that tie a calendar to its school.
namespace PerfectWorld::ContentSchema
{
using namespace Build;

namespace
{
const std::vector<std::string>& CalendarSchemaStatuses()
{
    static const std::vector<std::string> statuses = {"unverified", "reviewed"};
    return statuses;
}

// Every dated entry has an ID, a name, a status and an optional note; the date fields differ.
const TypeSchema& CalendarSchemaTerm()
{
    static const TypeSchema s{"calendar_term", "", {
        Str("id", true), Str("name", true), Str("start", true), Str("end", true),
        OneOf("status", true, CalendarSchemaStatuses()), Str("note", false)}};
    return s;
}

const TypeSchema& CalendarSchemaHoliday()
{
    static const TypeSchema s{"calendar_holiday", "", {
        Str("id", true), Str("name", true), OneOf("kind", true, {"national", "state", "festival"}), Str("start", true),
        Str("end", true), OneOf("dateBasis", true, {"fixed", "lunisolar_approximate"}),
        OneOf("status", true, CalendarSchemaStatuses()), Str("note", false)}};
    return s;
}

const TypeSchema& CalendarSchemaAssessment()
{
    static const TypeSchema s{"calendar_assessment", "", {
        Str("id", true), Str("name", true), Str("term", true), Str("start", true), Str("end", true),
        OneOf("status", true, CalendarSchemaStatuses()), Str("note", false)}};
    return s;
}

const TypeSchema& CalendarSchemaEvent()
{
    static const TypeSchema s{"calendar_event", "", {
        Str("id", true), Str("name", true), OneOf("kind", true, {"sports_day"}), Str("date", true),
        OneOf("status", true, CalendarSchemaStatuses()), Str("note", false)}};
    return s;
}

void CalendarSchemaIssue(std::vector<ContentIssue>& issues, const ContentRecord& record, std::string message)
{
    issues.push_back({record.File, record.Id, std::move(message)});
}

void CalendarSchemaCheckSchool(const ContentDb& db, const ContentRecord& record, const SchoolCalendar& calendar,
                               std::vector<ContentIssue>& issues)
{
    const ContentRecord* school = db.Find(calendar.InstitutionId);
    if (school == nullptr)
        return;
    const JsonValue* kind = school->Data.Find("kind");
    if (kind == nullptr || !kind->IsString() || kind->String != "school")
        CalendarSchemaIssue(issues, record, "the institution " + calendar.InstitutionId + " is not a school");
    const JsonValue* stages = school->Data.Find("stages");
    bool offered = false;
    if (stages != nullptr && stages->IsArray())
    {
        for (const JsonValue& stage : stages->Array)
            offered = offered || (stage.IsString() && stage.String == calendar.StageId);
    }
    if (!offered)
        CalendarSchemaIssue(issues, record, "the school " + calendar.InstitutionId + " does not offer " + calendar.StageId);
}
} // namespace

const TypeSchema& CalendarSchema()
{
    static const TypeSchema s{"calendar", "calendar.", Base({
        Str("name", true), Ref("institution", true, "institution"), Ref("stage", true, "education_stage"), Str("state", true),
        Str("yearStart", true), Str("yearEnd", true), Str("awakeningDate", true), Str("examinationDate", true),
        Ints("schoolWeekdays", true, 0, 6), Obj("terms", true, &CalendarSchemaTerm(), true),
        Obj("breaks", true, &CalendarSchemaTerm(), true), Obj("holidays", true, &CalendarSchemaHoliday(), true),
        Obj("assessments", true, &CalendarSchemaAssessment(), true), Obj("events", true, &CalendarSchemaEvent(), true)})};
    return s;
}

void CheckCalendarSemantics(const ContentDb& db, std::vector<ContentIssue>& issues)
{
    std::map<std::pair<std::string, std::string>, std::vector<const ContentRecord*>> groups;
    for (const ContentRecord* record : db.OfType("calendar"))
    {
        std::vector<std::string> problems;
        const SchoolCalendar calendar = ParseSchoolCalendarRecord(*record, problems);
        if (problems.empty())
            problems = ValidateSchoolCalendar(calendar);
        for (std::string& problem : problems)
            CalendarSchemaIssue(issues, *record, std::move(problem));
        CalendarSchemaCheckSchool(db, *record, calendar, issues);
        groups[{calendar.InstitutionId, calendar.StageId}].push_back(record);
    }
    for (const auto& [key, records] : groups)
    {
        for (std::size_t i = 1; i < records.size(); ++i)
            CalendarSchemaIssue(issues, *records[i], "a second calendar for " + key.first + " at " + key.second);
    }
}
} // namespace PerfectWorld::ContentSchema
