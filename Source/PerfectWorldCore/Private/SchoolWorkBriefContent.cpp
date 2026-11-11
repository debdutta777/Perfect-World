#include "SchoolWorkBriefContent.h"

#include "ContentSchemaBuilders.h"

#include <algorithm>
#include <set>
#include <utility>

namespace PerfectWorld
{
namespace ContentSchema
{
const TypeSchema& SchoolWorkBriefSchema()
{
    using namespace Build;
    static const TypeSchema s{"school_work_brief", "school.work.", Base({
        Ref("calendar", true, "calendar"), Ref("bellSchedule", true, "bell_schedule"),
        OneOf("stream", true, {"science", "commerce", "humanities"}), Str("subject", true), Str("term", true),
        Str("issueDate", true), Int("period", true, 0, 7), Str("dueDate", true), Int("dueMinute", true, 0, 1439),
        Ref("paper", true, "assessment_paper"), Str("titleKey", true), Str("briefKey", true),
        Str("criteriaKey", true), Str("recoveryKey", true)})};
    return s;
}
void ValidateSchoolWorkBriefSemantics(const ContentDb& db, std::vector<ContentIssue>& issues)
{
    const auto result = DecodeSchoolWorkBriefContent(db);
    issues.insert(issues.end(), result.Issues.begin(), result.Issues.end());
}
} // namespace ContentSchema

namespace
{
// ContentDb is publicly constructible: repeat shape admission at the typed loader.
std::string P34Shape(const JsonValue& data)
{
    using ContentSchema::FieldKind;
    if (!data.IsObject()) return "record must be an object";
    const auto& schema = ContentSchema::SchoolWorkBriefSchema();
    std::set<std::string> keys;
    for (const auto& [key, value] : data.Object)
    {
        (void)value;
        if (!keys.insert(key).second) return "duplicate field " + key;
        if (std::none_of(schema.Fields.begin(), schema.Fields.end(), [&](const auto& f) { return f.Name == key; }))
            return "unknown field " + key;
    }
    for (const auto& field : schema.Fields)
    {
        const auto* value = data.Find(field.Name);
        if (!value)
        {
            if (field.Required) return "missing required field " + field.Name;
            continue;
        }
        bool valid = false;
        switch (field.Kind)
        {
        case FieldKind::String:
        case FieldKind::Ref: valid = value->IsString() && !value->String.empty(); break;
        case FieldKind::Int: valid = value->Integer && *value->Integer >= field.Min && *value->Integer <= field.Max; break;
        case FieldKind::Enum:
            valid = value->IsString() && std::find(field.Values.begin(), field.Values.end(), value->String) != field.Values.end(); break;
        default: break;
        }
        if (!valid) return "invalid field " + field.Name;
    }
    return {};
}
const std::string& P34Text(const JsonValue& data, std::string_view name) { return data.Find(name)->String; }
std::int32_t P34Int(const JsonValue& data, std::string_view name) { return static_cast<std::int32_t>(*data.Find(name)->Integer); }
std::set<std::string> P34TextKeys(const SchoolWorkBriefCatalogue& catalogue)
{
    std::set<std::string> keys;
    for (const auto& d : catalogue.Briefs)
        for (const auto* key : {&d.Brief.TitleTextKey, &d.Brief.BriefTextKey, &d.Brief.CriteriaTextKey, &d.Brief.RecoveryTextKey}) keys.insert(*key);
    return keys;
}
} // namespace

SchoolWorkBriefContentResult DecodeSchoolWorkBriefContent(const ContentDb& db)
{
    SchoolWorkBriefContentResult out;
    if (db.CountOfType("school_work_brief") == 0) return out;
    const auto calendars = LoadSchoolCalendars(db);
    const auto bells = LoadBellSchedules(db);
    const auto model = LoadModelCatalogue(db);
    const auto explain = LoadExplainCatalogue(db);
    const auto investigate = LoadInvestigateCatalogue(db);
    const auto coordinate = LoadCoordinateCatalogue(db);
    if (!calendars.Ok() || !bells.Ok() || !model.Ok() || !explain.Ok() || !investigate.Ok() || !coordinate.Ok())
    {
        out.Issues.push_back({"", "school_work_brief", "invalid calendar, bell schedule or structure catalogue"});
        return out;
    }
    const auto papers = LoadAssessmentCatalogue(db, {&model.Catalogue, &explain.Catalogue, &investigate.Catalogue, &coordinate.Catalogue});
    if (!papers.Ok()) { out.Issues.push_back({"", "school_work_brief", papers.Error}); return out; }
    std::set<std::string> ids;
    for (const auto* record : db.OfType("school_work_brief"))
    {
        const auto issue = [&](std::string message) { out.Issues.push_back({record->File, record->Id, std::move(message)}); };
        const auto& data = record->Data;
        if (const auto error = P34Shape(data); !error.empty()) { issue(error); continue; }
        if (db.Find(record->Id) != record || P34Text(data, "id") != record->Id ||
            P34Text(data, "type") != record->Type || !record->ParentId.empty() || !ids.insert(record->Id).second)
        { issue("brief metadata differs from its registered identity"); continue; }
        SchoolWorkBriefDef def;
        def.CalendarId = P34Text(data, "calendar");
        def.BellScheduleId = P34Text(data, "bellSchedule");
        def.Stream = P34Text(data, "stream");
        def.Subject = P34Text(data, "subject");
        def.TermId = P34Text(data, "term");
        def.Period = P34Int(data, "period");
        const auto* calendar = calendars.Set.FindById(def.CalendarId);
        const auto* schedule = bells.Set.FindById(def.BellScheduleId);
        const auto day = ParseCalendarDate(P34Text(data, "issueDate"));
        const auto dueDay = ParseCalendarDate(P34Text(data, "dueDate"));
        if (!calendar || !schedule || !day || !dueDay || *day < 0 || *dueDay < 0 ||
            *day > DayLoopMaxInstant / MinutesPerDay || *dueDay > DayLoopMaxInstant / MinutesPerDay)
        { issue("missing calendar/bell schedule or invalid date"); continue; }
        def.IssueDay = *day;
        std::int32_t index = 0;
        const BellBlock* period = nullptr;
        for (const auto& block : schedule->Blocks)
            if (block.Kind == BellBlockKind::Period && index++ == def.Period) period = &block;
        if (!period) { issue("period does not exist"); continue; }
        def.Brief = {record->Id, P34Text(data, "paper"), SchoolWorkKind::Homework,
            CalendarDayStartInstant(*day) + period->StartMinute, CalendarDayStartInstant(*dueDay) + P34Int(data, "dueMinute"),
            P34Text(data, "titleKey"), P34Text(data, "briefKey"), P34Text(data, "criteriaKey"), P34Text(data, "recoveryKey")};
        const auto valid = ValidateSchoolWorkBriefDef(def, *calendar, *schedule, papers.Catalogue);
        if (!valid.Ok()) { issue(valid.Code + ": " + valid.Detail); continue; }
        out.Catalogue.Briefs.push_back(std::move(def));
    }
    std::sort(out.Catalogue.Briefs.begin(), out.Catalogue.Briefs.end(), [](const auto& a, const auto& b) { return a.Brief.Id < b.Brief.Id; });
    if (!out.Ok()) out.Catalogue = {};
    return out;
}

SchoolWorkBriefContentResult LoadSchoolWorkBriefContent(const ContentDb& db, const ContentFile& textFile)
{
    auto out = DecodeSchoolWorkBriefContent(db);
    if (!out.Ok()) return out;
    const auto issue = [&](std::string key, std::string message) { out.Issues.push_back({textFile.Path, std::move(key), std::move(message)}); };
    if (out.Catalogue.Briefs.empty()) issue("school_work_brief", "no homework briefs");
    std::string_view bytes = textFile.Text;
    if (bytes.starts_with("\xEF\xBB\xBF")) bytes.remove_prefix(3);
    const auto parsed = ParseJson(bytes);
    if (!parsed.Value || !parsed.Value->IsObject())
    { issue("school_work_brief", "text table must be a JSON object: " + parsed.Error); out.Catalogue = {}; return out; }
    const auto required = P34TextKeys(out.Catalogue);
    for (const auto& [key, value] : parsed.Value->Object)
    {
        if (!required.contains(key)) issue(key, "unreferenced translation");
        else if (!value.IsString() || value.String.find_first_not_of(" \t\r\n") == std::string::npos) issue(key, "empty or invalid translation");
        else out.Catalogue.Text.emplace(key, value.String);
    }
    for (const auto& key : required) if (!out.Catalogue.Text.contains(key)) issue(key, "missing translation");
    if (!out.Ok()) out.Catalogue = {};
    return out;
}
} // namespace PerfectWorld
