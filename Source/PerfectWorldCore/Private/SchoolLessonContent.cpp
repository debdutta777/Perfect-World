#include "SchoolLessonContent.h"

#include "ContentSchemaBuilders.h"
#include "ContentSchemas.h"

#include <algorithm>
#include <set>
#include <utility>

namespace PerfectWorld
{
namespace ContentSchema
{
namespace
{
using namespace Build;
std::vector<std::string> A1Names(std::initializer_list<std::string_view> names) { return {names.begin(), names.end()}; }
const std::vector<std::string> A1Structures = A1Names({"model", "explain", "investigate", "coordinate"});
const TypeSchema& A1LessonSchema()
{
    static const TypeSchema s{"school_lesson_entry", "", {
        Str("id", true), Int("period", true, 0, 7), Str("subject", true), OneOf("structure", true, A1Structures),
        Str("family", true), Str("familySubject", true), Str("role", true),
        Str("titleKey", true), Str("briefKey", true), Str("roleKey", true)}};
    return s;
}
const TypeSchema& A1ClosureSchema()
{
    static const TypeSchema s{"school_lesson_closure", "", {
        OneOf("kind", true, A1Names({"weekend", "holiday", "break", "outside_year", "outside_term"})),
        Str("span", false), Str("textKey", true)}};
    return s;
}
const TypeSchema& A1ExplanationSchema()
{
    static const TypeSchema s{"school_lesson_explanation", "", {
        OneOf("code", true, std::vector<std::string>(SchoolLessonExplanationCodes.begin(), SchoolLessonExplanationCodes.end())),
        Str("textKey", true), Str("recoveryKey", true)}};
    return s;
}
const TypeSchema& A1GoalSchema()
{
    static const TypeSchema s{"school_lesson_goal", "", {
        OneOf("structure", true, A1Structures), Str("goal", true), Str("textKey", true)}};
    return s;
}
const TypeSchema& A1ModeSchema()
{
    static const TypeSchema s{"school_lesson_mode", "", {
        OneOf("mode", true, A1Names({"foreground", "routine", "attend"})), Str("titleKey", true), Str("briefKey", true)}};
    return s;
}
} // namespace
const TypeSchema& SchoolLessonSchema()
{
    static const TypeSchema s{"school_lesson", "p2.", Base({
        OneOf("scope", true, A1Names({"science", "commerce", "humanities", "school"})),
        Str("bellSchedule", false), Str("calendar", false),
        Obj("lessons", false, &A1LessonSchema(), true), Obj("closures", false, &A1ClosureSchema(), true),
        Obj("explanations", false, &A1ExplanationSchema(), true), Obj("goals", false, &A1GoalSchema(), true),
        Obj("modes", false, &A1ModeSchema(), true)})};
    return s;
}
void ValidateSchoolLessonSemantics(const ContentDb& db, std::vector<ContentIssue>& issues)
{
    const auto result = DecodeSchoolLessonContent(db);
    issues.insert(issues.end(), result.Issues.begin(), result.Issues.end());
}
} // namespace ContentSchema

namespace
{
using ContentSchema::FieldKind;
// Repeat the schema at this boundary, since ContentDb is publicly constructible.
std::string A1Shape(const ContentSchema::TypeSchema& schema, const JsonValue& data)
{
    if (!data.IsObject()) return "record must be an object";
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
        if (value == nullptr)
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
            valid = value->IsString() && std::find(field.Values.begin(), field.Values.end(), value->String) != field.Values.end();
            break;
        case FieldKind::ObjectArray:
            if (!value->IsArray()) break;
            for (const auto& child : value->Array)
                if (const auto error = A1Shape(*field.Nested, child); !error.empty()) return field.Name + ": " + error;
            valid = true;
            break;
        default: break;
        }
        if (!valid) return "invalid field " + field.Name;
    }
    return {};
}
bool A1Token(std::string_view text, std::string_view prefix)
{
    return IsReceiptIdWellFormed(text) && text.starts_with(prefix) && text.size() > prefix.size();
}
const std::string& A1Text(const JsonValue& data, std::string_view field) { return data.Find(field)->String; }
std::int64_t A1Int(const JsonValue& data, std::string_view field) { return *data.Find(field)->Integer; }
std::string A1OptText(const JsonValue& data, std::string_view field)
{
    const auto* value = data.Find(field);
    return value != nullptr ? value->String : std::string();
}
std::size_t A1StructureIndex(std::string_view name)
{
    return static_cast<std::size_t>(std::find(AssessmentStructureNames.begin(), AssessmentStructureNames.end(), name) - AssessmentStructureNames.begin());
}
constexpr std::array<std::string_view, 4> A1FamilyTypes{"problem_family", "explain_family", "investigate_family", "coordinate_family"};
bool A1ListHas(const JsonValue& record, std::string_view field, std::string_view value)
{
    const auto* list = record.Find(field);
    return list != nullptr && list->IsArray() && std::any_of(list->Array.begin(), list->Array.end(),
        [&](const JsonValue& item) { return item.IsString() && item.String == value; });
}
bool A1CaseExists(const ContentDb& db, std::size_t structure, std::string_view family, std::string_view subject, std::string_view role)
{
    for (const auto* record : db.OfType(AssessmentCaseTypes[structure]))
    {
        const auto* f = record->Data.Find("family");
        const auto* s = record->Data.Find("subject");
        const auto* r = record->Data.Find("role");
        if (f && s && r && f->String == family && s->String == subject && r->String == role) return true;
    }
    return false;
}
SchoolDayKind A1ClosureKind(std::string_view name)
{
    if (name == "weekend") return SchoolDayKind::NonSchoolWeekday;
    if (name == "holiday") return SchoolDayKind::Holiday;
    if (name == "break") return SchoolDayKind::Break;
    if (name == "outside_year") return SchoolDayKind::OutsideYear;
    return SchoolDayKind::OutsideTerm;
}
SchoolLessonMode A1Mode(std::string_view name)
{
    return name == "foreground" ? SchoolLessonMode::Foreground : name == "routine" ? SchoolLessonMode::Routine : SchoolLessonMode::Attend;
}
std::vector<std::string> A1GoalNames(std::size_t structure)
{
    const auto copy = [](const auto& names) { return std::vector<std::string>(names.begin(), names.end()); };
    switch (structure)
    {
    case 0: return copy(ModelGoalNames);
    case 1: return copy(ExplainGoalNames);
    case 2: return copy(InvestigateGoalNames);
    default: return copy(CoordinateGoalNames);
    }
}
std::vector<std::string> A1ReferencedKeys(const SchoolLessonContent& c)
{
    std::vector<std::string> keys;
    for (const auto& stream : c.Streams)
        for (const auto& lesson : stream.Lessons)
            for (const auto* key : {&lesson.Binding.TitleTextKey, &lesson.Binding.BriefTextKey, &lesson.Binding.RoleTextKey}) keys.push_back(*key);
    for (const auto& x : c.Closures) keys.push_back(x.TextKey);
    for (const auto& x : c.Explanations) { keys.push_back(x.TextKey); keys.push_back(x.RecoveryTextKey); }
    for (const auto& x : c.Goals) keys.push_back(x.TextKey);
    for (const auto& x : c.Modes) { keys.push_back(x.TitleTextKey); keys.push_back(x.BriefTextKey); }
    return keys;
}
} // namespace

SchoolLessonContentResult DecodeSchoolLessonContent(const ContentDb& db)
{
    SchoolLessonContentResult out;
    const auto bells = LoadBellSchedules(db);
    const auto calendars = LoadSchoolCalendars(db);
    std::set<std::string> keys, ids;
    std::set<std::string> streamsSeen, bellsSeen;
    for (const auto* record : db.OfType(SchoolLessonContentType))
    {
        const auto issue = [&](std::string message) { out.Issues.push_back({record->File, record->Id, std::move(message)}); };
        const auto& data = record->Data;
        if (const auto error = A1Shape(ContentSchema::SchoolLessonSchema(), data); !error.empty()) { issue(error); continue; }
        if (db.Find(record->Id) != record || !A1Token(record->Id, "p2.") || A1Text(data, "id") != record->Id ||
            A1Text(data, "type") != record->Type || !record->ParentId.empty())
        { issue("school lesson metadata differs from its registered identity"); continue; }
        const auto key = [&](const std::string& text) {
            if (!A1Token(text, SchoolLessonTextPrefix)) issue("text key is not a phase2.a1 dotted token: " + text);
            else if (!keys.insert(text).second) issue("text key is used twice: " + text);
        };
        const auto section = [&](const char* name) { const auto* v = data.Find(name); return v != nullptr && !v->Array.empty(); };
        const auto& scope = A1Text(data, "scope");
        if (scope != SchoolLessonSchoolScope)
        {
            if (!streamsSeen.insert(scope).second) issue("two records name the stream " + scope);
            if (section("closures") || section("explanations") || section("goals") || section("modes") || data.Find("calendar"))
                issue("a stream record carries only its bell schedule and lessons");
            if (data.Find("bellSchedule") == nullptr || !section("lessons")) { issue("a stream record needs bellSchedule and lessons"); continue; }
            SchoolLessonStream stream{record->Id, scope, A1Text(data, "bellSchedule"), {}};
            if (const auto* target = db.Find(stream.BellScheduleId); target == nullptr || target->Type != "bell_schedule")
                issue("bell schedule record is missing: " + stream.BellScheduleId);
            const auto* schedule = bells.Ok() ? bells.Set.FindById(stream.BellScheduleId) : nullptr;
            if (bells.Ok() && (schedule == nullptr || schedule->Stream != scope)) issue("bell schedule is missing or belongs to another stream");
            if (schedule != nullptr && !bellsSeen.insert(schedule->Id).second) issue("bell schedule is bound twice");
            std::set<std::int32_t> periods;
            for (const auto& value : data.Find("lessons")->Array)
            {
                SchoolLessonEntry entry;
                auto& b = entry.Binding;
                b.Id = A1Text(value, "id");
                b.Stream = scope;
                b.Subject = A1Text(value, "subject");
                b.Period = static_cast<std::int32_t>(A1Int(value, "period"));
                const auto structure = A1StructureIndex(A1Text(value, "structure"));
                b.Structure = static_cast<SchoolStructure>(structure);
                b.FamilyId = A1Text(value, "family");
                b.TitleTextKey = A1Text(value, "titleKey");
                b.BriefTextKey = A1Text(value, "briefKey");
                b.RoleTextKey = A1Text(value, "roleKey");
                entry.FamilySubject = A1Text(value, "familySubject");
                entry.Role = A1Text(value, "role");
                if (!A1Token(b.Id, "school.lesson.") || !ids.insert(b.Id).second) issue("lesson id is malformed or duplicated: " + b.Id);
                if (!periods.insert(b.Period).second) issue("two lessons bind period " + std::to_string(b.Period));
                key(b.TitleTextKey); key(b.BriefTextKey); key(b.RoleTextKey);
                if (schedule != nullptr)
                {
                    std::int32_t index = 0;
                    const BellBlock* block = nullptr;
                    for (const auto& candidate : schedule->Blocks)
                        if (candidate.Kind == BellBlockKind::Period && index++ == b.Period) block = &candidate;
                    if (block == nullptr || block->Subject != b.Subject) issue("lesson " + b.Id + " does not match the bell schedule's subject for its period");
                }
                const auto* family = db.Find(b.FamilyId);
                if (family == nullptr || family->Type != A1FamilyTypes[structure]) issue("lesson " + b.Id + " names a family of the wrong type or none");
                else if (!A1ListHas(family->Data, "subjects", entry.FamilySubject) || !A1ListHas(family->Data, "roles", entry.Role))
                    issue("lesson " + b.Id + " names a subject or role its family does not teach");
                else if (!A1CaseExists(db, structure, b.FamilyId, entry.FamilySubject, entry.Role))
                    issue("lesson " + b.Id + " names a subject and role that no authored case of its family has");
                stream.Lessons.push_back(std::move(entry));
            }
            std::sort(stream.Lessons.begin(), stream.Lessons.end(), [](const auto& a, const auto& b) { return a.Binding.Period < b.Binding.Period; });
            if (schedule != nullptr && static_cast<std::int32_t>(stream.Lessons.size()) != BellPeriodCount(*schedule))
                issue("every period of the bell schedule needs exactly one lesson");
            out.Catalogue.Streams.push_back(std::move(stream));
            continue;
        }
        if (!out.Catalogue.SchoolRecordId.empty()) { issue("there is one school-wide record"); continue; }
        if (data.Find("bellSchedule") || section("lessons") || data.Find("calendar") == nullptr)
        { issue("the school-wide record carries a calendar and no lessons"); continue; }
        out.Catalogue.SchoolRecordId = record->Id;
        out.Catalogue.CalendarId = A1Text(data, "calendar");
        const auto* calendar = calendars.Ok() ? calendars.Set.FindById(out.Catalogue.CalendarId) : nullptr;
        if (const auto* target = db.Find(out.Catalogue.CalendarId); target == nullptr || target->Type != "calendar")
            issue("calendar record is missing: " + out.Catalogue.CalendarId);
        std::set<std::pair<int, std::string>> closed;
        for (const auto& value : data.Find("closures") ? data.Find("closures")->Array : std::vector<JsonValue>{})
        {
            SchoolLessonClosure closure{A1ClosureKind(A1Text(value, "kind")), A1OptText(value, "span"), A1Text(value, "textKey")};
            key(closure.TextKey);
            if (!closed.insert({static_cast<int>(closure.Kind), closure.SpanId}).second) issue("duplicate closure " + closure.SpanId);
            if (!closure.SpanId.empty())
            {
                const bool found = calendar != nullptr && ((closure.Kind == SchoolDayKind::Holiday && std::any_of(calendar->Holidays.begin(), calendar->Holidays.end(), [&](const auto& h) { return h.Id == closure.SpanId; })) ||
                    (closure.Kind == SchoolDayKind::Break && std::any_of(calendar->Breaks.begin(), calendar->Breaks.end(), [&](const auto& b) { return b.Id == closure.SpanId; })));
                if (calendar != nullptr && !found) issue("closure names a span the calendar does not have: " + closure.SpanId);
            }
            out.Catalogue.Closures.push_back(std::move(closure));
        }
        for (const auto kind : {SchoolDayKind::NonSchoolWeekday, SchoolDayKind::Holiday, SchoolDayKind::Break, SchoolDayKind::OutsideYear, SchoolDayKind::OutsideTerm})
            if (!closed.contains({static_cast<int>(kind), std::string()})) issue("every closed-day kind needs a generic line");
        if (calendar != nullptr)
        {
            for (const auto& h : calendar->Holidays) if (!closed.contains({static_cast<int>(SchoolDayKind::Holiday), h.Id})) issue("holiday has no explanation: " + h.Id);
            for (const auto& b : calendar->Breaks) if (!closed.contains({static_cast<int>(SchoolDayKind::Break), b.Id})) issue("break has no explanation: " + b.Id);
        }
        std::set<std::string> codes;
        for (const auto& value : data.Find("explanations") ? data.Find("explanations")->Array : std::vector<JsonValue>{})
        {
            SchoolLessonExplanation e{A1Text(value, "code"), A1Text(value, "textKey"), A1Text(value, "recoveryKey")};
            key(e.TextKey); key(e.RecoveryTextKey);
            if (!codes.insert(e.Code).second) issue("duplicate explanation " + e.Code);
            out.Catalogue.Explanations.push_back(std::move(e));
        }
        if (codes.size() != SchoolLessonExplanationCodes.size()) issue("every unavailable-period reason needs one explanation");
        std::set<std::pair<std::size_t, std::string>> goals;
        for (const auto& value : data.Find("goals") ? data.Find("goals")->Array : std::vector<JsonValue>{})
        {
            SchoolLessonGoalText g{static_cast<SchoolStructure>(A1StructureIndex(A1Text(value, "structure"))), A1Text(value, "goal"), A1Text(value, "textKey")};
            key(g.TextKey);
            const auto names = A1GoalNames(static_cast<std::size_t>(g.Structure));
            if (std::find(names.begin(), names.end(), g.Goal) == names.end() || !goals.insert({static_cast<std::size_t>(g.Structure), g.Goal}).second)
                issue("goal is not a runner goal or is repeated: " + g.Goal);
            out.Catalogue.Goals.push_back(std::move(g));
        }
        for (std::size_t s = 0; s < 4; ++s)
            for (const auto& name : A1GoalNames(s)) if (!goals.contains({s, name})) issue("goal has no words: " + name);
        std::set<int> modes;
        for (const auto& value : data.Find("modes") ? data.Find("modes")->Array : std::vector<JsonValue>{})
        {
            SchoolLessonModeText m{A1Mode(A1Text(value, "mode")), A1Text(value, "titleKey"), A1Text(value, "briefKey")};
            key(m.TitleTextKey); key(m.BriefTextKey);
            if (!modes.insert(static_cast<int>(m.Mode)).second) issue("duplicate lesson mode");
            out.Catalogue.Modes.push_back(std::move(m));
        }
        if (modes.size() != 3) issue("foreground, routine and attend each need words");
    }
    const bool any = !out.Catalogue.Streams.empty() || !out.Catalogue.SchoolRecordId.empty();
    if (any && bells.Ok() && bellsSeen.size() != bells.Set.Schedules.size())
        out.Issues.push_back({"", "", "every bell schedule needs a school_lesson stream record"});
    if (any && out.Catalogue.SchoolRecordId.empty()) out.Issues.push_back({"", "", "the school-wide school_lesson record is missing"});
    std::sort(out.Catalogue.Streams.begin(), out.Catalogue.Streams.end(), [](const auto& a, const auto& b) { return a.Stream < b.Stream; });
    if (!out.Ok()) out.Catalogue = {};
    return out;
}

SchoolLessonContentResult LoadSchoolLessonContent(const ContentDb& db, const ContentFile& textFile)
{
    auto out = DecodeSchoolLessonContent(db);
    if (!out.Ok()) return out;
    const auto issue = [&](std::string key, std::string message) { out.Issues.push_back({textFile.Path, std::move(key), std::move(message)}); };
    if (out.Catalogue.Streams.empty()) { issue("school_lesson", "no school_lesson records"); return out; }
    std::string_view bytes = textFile.Text;
    if (bytes.starts_with("\xEF\xBB\xBF")) bytes.remove_prefix(3);
    const auto parsed = ParseJson(bytes);
    if (!parsed.Value || !parsed.Value->IsObject())
    { issue("school_lesson", "text table must be a JSON object: " + parsed.Error); out.Catalogue = {}; return out; }
    for (const auto& [key, value] : parsed.Value->Object)
    {
        if (!A1Token(key, SchoolLessonTextPrefix) || !value.IsString() || value.String.find_first_not_of(" \t\r\n") == std::string::npos)
            issue(key, "invalid text key or empty translation");
        else out.Catalogue.Text.emplace(key, value.String);
    }
    const auto required = A1ReferencedKeys(out.Catalogue);
    for (const auto& key : required) if (!out.Catalogue.Text.contains(key)) issue(key, "missing text key " + key);
    for (const auto& [key, text] : out.Catalogue.Text)
    {
        (void)text;
        if (std::find(required.begin(), required.end(), key) == required.end()) issue(key, "unreferenced text key " + key);
    }
    if (!out.Ok()) out.Catalogue = {};
    return out;
}

const SchoolLessonStream* FindSchoolLessonStream(const SchoolLessonContent& content, std::string_view stream)
{
    for (const auto& s : content.Streams) if (s.Stream == stream) return &s;
    return nullptr;
}
const SchoolLessonEntry* FindSchoolLesson(const SchoolLessonContent& content, std::string_view stream, std::int32_t period)
{
    const auto* s = FindSchoolLessonStream(content, stream);
    if (s == nullptr) return nullptr;
    for (const auto& lesson : s->Lessons) if (lesson.Binding.Period == period) return &lesson;
    return nullptr;
}
const SchoolLessonEntry* FindSchoolLessonById(const SchoolLessonContent& content, std::string_view bindingId)
{
    for (const auto& s : content.Streams)
        for (const auto& lesson : s.Lessons) if (lesson.Binding.Id == bindingId) return &lesson;
    return nullptr;
}
const SchoolLessonBinding* FindSchoolLessonBinding(const SchoolLessonContent& content, std::string_view bindingId)
{
    const auto* entry = FindSchoolLessonById(content, bindingId);
    return entry != nullptr ? &entry->Binding : nullptr;
}
std::vector<SchoolLessonBinding> SchoolLessonBindings(const SchoolLessonContent& content, std::string_view stream)
{
    std::vector<SchoolLessonBinding> out;
    if (const auto* s = FindSchoolLessonStream(content, stream)) for (const auto& lesson : s->Lessons) out.push_back(lesson.Binding);
    return out;
}
const SchoolLessonExplanation* FindSchoolLessonExplanation(const SchoolLessonContent& content, std::string_view code)
{
    for (const auto& e : content.Explanations) if (e.Code == code) return &e;
    return nullptr;
}
const SchoolLessonClosure* FindSchoolLessonClosure(const SchoolLessonContent& content, SchoolDayKind kind, std::string_view spanId)
{
    const SchoolLessonClosure* generic = nullptr;
    for (const auto& c : content.Closures)
    {
        if (c.Kind != kind) continue;
        if (!spanId.empty() && c.SpanId == spanId) return &c;
        if (c.SpanId.empty()) generic = &c;
    }
    return generic;
}
const SchoolLessonModeText* FindSchoolLessonModeText(const SchoolLessonContent& content, SchoolLessonMode mode)
{
    for (const auto& m : content.Modes) if (m.Mode == mode) return &m;
    return nullptr;
}
const std::string* FindSchoolLessonText(const SchoolLessonContent& content, std::string_view key)
{
    const auto found = content.Text.find(key);
    return found == content.Text.end() ? nullptr : &found->second;
}
std::vector<SchoolLessonGoalOption> SchoolLessonGoalOptions(const SchoolLessonContent& content, SchoolStructure structure)
{
    std::vector<SchoolLessonGoalOption> out;
    const auto add = [&](std::string_view name, SchoolLessonGoal goal) {
        for (const auto& g : content.Goals)
            if (g.Structure == structure && g.Goal == name) out.push_back({std::string(name), g.TextKey, goal});
    };
    switch (structure)
    {
    case SchoolStructure::Model: for (std::size_t i = 0; i < ModelGoalNames.size(); ++i) add(ModelGoalNames[i], static_cast<ModelGoal>(i)); break;
    case SchoolStructure::Explain: for (std::size_t i = 0; i < ExplainGoalNames.size(); ++i) add(ExplainGoalNames[i], static_cast<ExplainGoal>(i)); break;
    case SchoolStructure::Investigate: for (std::size_t i = 0; i < InvestigateGoalNames.size(); ++i) add(InvestigateGoalNames[i], static_cast<InvestigateGoal>(i)); break;
    case SchoolStructure::Coordinate: for (std::size_t i = 0; i < CoordinateGoalNames.size(); ++i) add(CoordinateGoalNames[i], static_cast<CoordinateGoal>(i)); break;
    }
    return out;
}
} // namespace PerfectWorld
