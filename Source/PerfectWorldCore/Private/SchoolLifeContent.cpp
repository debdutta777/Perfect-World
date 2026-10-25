#include "SchoolLifeContent.h"

#include "ContentFairness.h"
#include "ContentSchemaBuilders.h"

#include <algorithm>
#include <array>
#include <set>

namespace PerfectWorld
{
namespace
{
constexpr std::array<SchoolCastMember, 7> CanonicalCast{{
    {"person.amrita_basu", "Amrita Basu", "teacher", false},
    {"person.mira_sen", "Mira Sen", "classmate", false},
    {"person.ishan_sheikh", "Ishan Sheikh", "classmate", false},
    {"person.tara_das", "Tara Das", "classmate", false},
    {"person.deven_chakraborty", "Deven Chakraborty", "classmate", true},
    {"person.household_adult", "Your parent", "family_adult", false},
    {"person.care_worker", "Care worker", "care_adult", false},
}};

std::optional<RelationshipActKind> ParseActKind(std::string_view t) noexcept
{
    if (t == "help") return RelationshipActKind::Help;
    if (t == "gift") return RelationshipActKind::Gift;
    if (t == "confide") return RelationshipActKind::Confide;
    if (t == "defend") return RelationshipActKind::Defend;
    if (t == "apologise") return RelationshipActKind::Apologise;
    if (t == "share_time") return RelationshipActKind::ShareTime;
    if (t == "insult") return RelationshipActKind::Insult;
    return t == "betray" ? std::optional{RelationshipActKind::Betray} : (t == "bereave" ? std::optional{RelationshipActKind::Bereave} : std::nullopt);
}
std::string StrField(const JsonValue& o, std::string_view k) { const auto* v = o.Find(k); return v && v->IsString() ? v->String : std::string{}; }
std::int32_t IntField(const JsonValue& o, std::string_view k, std::int32_t fb = 0) { const auto* v = o.Find(k); return v && v->Integer ? static_cast<std::int32_t>(*v->Integer) : fb; }
bool BoolField(const JsonValue& o, std::string_view k, bool fb = false) { const auto* v = o.Find(k); return v && v->Type == JsonValue::Kind::Bool ? v->Bool : fb; }
bool CalendarHasSpan(const SchoolCalendar& cal, std::string_view id)
{
    for (const auto& a : cal.Assessments) if (a.Id == id) return true;
    for (const auto& e : cal.Events) if (e.Id == id) return true;
    for (const auto& h : cal.Holidays) if (h.Id == id) return true;
    for (const auto& b : cal.Breaks) if (b.Id == id) return true;
    return false;
}
} // namespace

const std::vector<SchoolCastMember>& KnownSchoolLifeCast() { static const std::vector<SchoolCastMember> c(CanonicalCast.begin(), CanonicalCast.end()); return c; }
const SchoolCastMember* FindSchoolLifeCast(std::string_view id) noexcept { for (const auto& m : KnownSchoolLifeCast()) if (m.PersonId == id) return &m; return nullptr; }
std::optional<SchoolArcKind> ParseSchoolArcKind(std::string_view t) noexcept { return t == "changing_friendship" ? std::optional{SchoolArcKind::ChangingFriendship} : (t == "group_project" ? std::optional{SchoolArcKind::GroupProject} : (t == "household_responsibility" ? std::optional{SchoolArcKind::HouseholdResponsibility} : std::nullopt)); }
std::string_view SchoolArcKindName(SchoolArcKind k) noexcept { return k == SchoolArcKind::GroupProject ? "group_project" : (k == SchoolArcKind::HouseholdResponsibility ? "household_responsibility" : "changing_friendship"); }
std::optional<SchoolOriginVariant> ParseSchoolOriginVariant(std::string_view t) noexcept { return t == "both" ? std::optional{SchoolOriginVariant::Both} : (t == "family" ? std::optional{SchoolOriginVariant::Family} : (t == "care_home" ? std::optional{SchoolOriginVariant::CareHome} : std::nullopt)); }
std::string_view SchoolOriginVariantName(SchoolOriginVariant v) noexcept { return v == SchoolOriginVariant::Family ? "family" : (v == SchoolOriginVariant::CareHome ? "care_home" : "both"); }
bool SchoolOriginMatches(SchoolOriginVariant o, bool care) noexcept { return o == SchoolOriginVariant::Both || (care ? o == SchoolOriginVariant::CareHome : o == SchoolOriginVariant::Family); }
std::optional<SchoolSceneSlotKind> ParseSchoolSceneSlotKind(std::string_view t) noexcept
{
    if (t == "assembly") return SchoolSceneSlotKind::Assembly;
    if (t == "timetable_period") return SchoolSceneSlotKind::TimetablePeriod;
    if (t == "school_break") return SchoolSceneSlotKind::SchoolBreak;
    if (t == "after_school") return SchoolSceneSlotKind::AfterSchool;
    return t == "care_handover" ? std::optional{SchoolSceneSlotKind::CareHandover} : (t == "home_evening" ? std::optional{SchoolSceneSlotKind::HomeEvening} : std::nullopt);
}
std::string_view SchoolSceneSlotKindName(SchoolSceneSlotKind k) noexcept
{
    if (k == SchoolSceneSlotKind::Assembly) return "assembly";
    if (k == SchoolSceneSlotKind::SchoolBreak) return "school_break";
    if (k == SchoolSceneSlotKind::AfterSchool) return "after_school";
    return k == SchoolSceneSlotKind::CareHandover ? "care_handover" : (k == SchoolSceneSlotKind::HomeEvening ? "home_evening" : "timetable_period");
}
const SchoolSceneNode* SchoolSceneArc::FindScene(std::string_view id) const noexcept { for (const auto& n : Scenes) if (n.Id == id) return &n; return nullptr; }
const SchoolSceneArc* SchoolSceneCatalogue::FindArc(std::string_view id) const noexcept { for (const auto& a : Arcs) if (a.Id == id) return &a; return nullptr; }
const std::string* SchoolTextTable::Find(std::string_view k) const noexcept { const auto it = Entries.find(k); return it != Entries.end() ? &it->second : nullptr; }

namespace ContentSchema
{
using namespace Build;
namespace
{
std::string ValidateObjectShape(const JsonValue& object, const TypeSchema& schema, const ContentDb* db)
{
    if (!object.IsObject()) return schema.Type + " must be an object";
    if (!schema.IdPrefix.empty() && StrField(object, "id").rfind(schema.IdPrefix, 0) != 0)
        return "ID must start with \"" + schema.IdPrefix + "\"";
    std::set<std::string> seen;
    for (const auto& [name, val] : object.Object)
    {
        if (!seen.insert(name).second) return "duplicate field " + name;
        const FieldSpec* spec = nullptr;
        for (const auto& f : schema.Fields) if (f.Name == name) spec = &f;
        if (spec == nullptr) return "unknown field \"" + name + "\" in " + schema.Type;
        if (spec->Kind == FieldKind::String || spec->Kind == FieldKind::Ref)
        {
            if (!val.IsString() || val.String.empty()) return "field \"" + name + "\" must be a non-empty string";
            if (spec->Kind == FieldKind::Ref && db != nullptr)
            {
                const ContentRecord* target = db->Find(val.String);
                if (target == nullptr) return "field \"" + name + "\" names " + val.String + ", which does not exist";
                if (target->Type != spec->RefTarget)
                    return "field \"" + name + "\" names " + val.String + " (" + target->Type + "), but needs a " + spec->RefTarget;
            }
        }
        else if (spec->Kind == FieldKind::Enum && (!val.IsString() || std::find(spec->Allowed.begin(), spec->Allowed.end(), val.String) == spec->Allowed.end()))
            return "field \"" + name + "\" has an unknown value";
        else if (spec->Kind == FieldKind::Int && (val.Type != JsonValue::Kind::Number || !val.Integer || *val.Integer < spec->Min || *val.Integer > spec->Max))
            return "field \"" + name + "\" is outside " + std::to_string(spec->Min) + ".." + std::to_string(spec->Max);
        else if (spec->Kind == FieldKind::Bool && val.Type != JsonValue::Kind::Bool)
            return "field \"" + name + "\" must be true or false";
        else if (spec->Kind == FieldKind::StringArray)
        {
            if (!val.IsArray() || val.Array.empty()) return "field \"" + name + "\" must be a non-empty array of strings";
            for (const JsonValue& item : val.Array) if (!item.IsString() || item.String.empty()) return "field \"" + name + "\" requires non-empty strings";
        }
        else if (spec->Kind == FieldKind::Object)
        {
            if (const std::string err = ValidateObjectShape(val, *spec->Nested, db); !err.empty()) return name + ": " + err;
        }
        else if (spec->Kind == FieldKind::ObjectArray)
        {
            if (!val.IsArray() || val.Array.empty()) return "field \"" + name + "\" must be a non-empty array of objects";
            for (const JsonValue& item : val.Array) if (const std::string err = ValidateObjectShape(item, *spec->Nested, db); !err.empty()) return name + ": " + err;
        }
    }
    for (const auto& f : schema.Fields) if (f.Required && object.Find(f.Name) == nullptr) return "missing required field \"" + f.Name + "\"";
    return {};
}
} // namespace

const TypeSchema& SchoolSceneSchema()
{
    static const TypeSchema castS{"school_scene_cast", "", {Str("person", true), OneOf("role", true, {"teacher", "classmate", "family_adult", "care_adult"}), OneOf("originVariant", true, {"both", "family", "care_home"}), Bool("proposedNewCast", true)}};
    static const TypeSchema schedS{"school_scene_schedule", "", {OneOf("slotKind", true, {"assembly", "timetable_period", "school_break", "after_school", "care_handover", "home_evening"}), Ref("bellSchedule", false, "bell_schedule"), Int("periodIndex", false, -1, 7), Str("subject", false), Str("zone", true), Int("startMinute", true, 0, 1439), Int("endMinute", true, 1, 1440)}};
    static const TypeSchema consS{"school_scene_consequence", "", {Str("targetPerson", true), OneOf("relationshipAct", true, {"help", "gift", "confide", "defend", "apologise", "share_time", "insult", "betray", "bereave"}), Str("relationshipEvent", true), Str("threadEvent", false), Str("factSet", true), Str("factCleared", false), Str("journalAgreementFact", false)}};
    static const TypeSchema choiceS{"school_scene_choice", "", {Str("id", true), Str("labelKey", true), Str("outcomeKey", true), Int("costMinutes", true, 0, 240), Bool("isRefusal", true), Bool("humiliating", true), Str("nextScene", false), Str("resolution", false), Obj("consequence", true, &consS, false)}};
    static const TypeSchema nodeS{"school_scene_node", "", {Str("id", true), Str("titleKey", true), Str("promptKey", true), Str("speaker", true), OneOf("originVariant", true, {"both", "family", "care_home"}), Obj("schedule", true, &schedS, false), Str("conversationTopic", false), Obj("choices", true, &choiceS, true)}};
    static const TypeSchema s{std::string(SchoolSceneTypeName), std::string(SchoolSceneIdPrefix), Base({OneOf("arcKind", true, {"changing_friendship", "group_project", "household_responsibility"}), Str("titleKey", true), Str("summaryKey", true), Ref("institution", true, "institution"), Ref("calendar", true, "calendar"), Str("term", true), Str("calendarSpan", false), Ref("threadTemplate", true, "thread_template"), Str("entryScene", true), Obj("cast", true, &castS, true), Obj("scenes", true, &nodeS, true), Strs("resolutions", true)})};
    return s;
}

std::string ValidateSchoolSceneShape(const JsonValue& data) { return ValidateObjectShape(data, SchoolSceneSchema(), nullptr); }

void ValidateSchoolSceneSemantics(const ContentDb& db, std::vector<ContentIssue>& issues)
{
    std::set<SchoolArcKind> seenKinds;
    for (const ContentRecord* record : db.OfType(SchoolSceneTypeName))
    {
        if (const std::string shapeErr = ValidateObjectShape(record->Data, SchoolSceneSchema(), &db); !shapeErr.empty())
        {
            issues.push_back({record->File, record->Id, shapeErr});
            continue;
        }
        std::vector<std::string> problems;
        const SchoolSceneArc arc = ParseSchoolSceneRecord(*record, problems);
        for (const std::string& p : problems) issues.push_back({record->File, record->Id, p});
        for (const std::string& p : ValidateSchoolSceneArc(arc, db)) issues.push_back({record->File, record->Id, p});
        if (!seenKinds.insert(arc.Kind).second)
            issues.push_back({record->File, record->Id, "duplicate school_scene arcKind " + std::string(SchoolArcKindName(arc.Kind))});
    }
}
} // namespace ContentSchema

SchoolSceneArc ParseSchoolSceneRecord(const ContentRecord& record, std::vector<std::string>& problems)
{
    const JsonValue& d = record.Data;
    SchoolSceneArc arc{record.Id, SchoolArcKind::ChangingFriendship, StrField(d, "titleKey"), StrField(d, "summaryKey"),
                       StrField(d, "institution"), StrField(d, "calendar"), StrField(d, "term"), StrField(d, "calendarSpan"),
                       StrField(d, "threadTemplate"), StrField(d, "entryScene"), {}, {}, {}};
    if (const auto kind = ParseSchoolArcKind(StrField(d, "arcKind"))) arc.Kind = *kind;
    else problems.push_back("unknown arcKind '" + StrField(d, "arcKind") + "'");

    if (const JsonValue* castArr = d.Find("cast"); castArr != nullptr && castArr->IsArray())
        for (const JsonValue& item : castArr->Array)
        {
            const auto orig = ParseSchoolOriginVariant(StrField(item, "originVariant"));
            if (!orig) problems.push_back("cast member '" + StrField(item, "person") + "' has unknown originVariant");
            arc.Cast.push_back({StrField(item, "person"), StrField(item, "role"),
                                orig.value_or(SchoolOriginVariant::Both), BoolField(item, "proposedNewCast", false)});
        }

    if (const JsonValue* scenesArr = d.Find("scenes"); scenesArr != nullptr && scenesArr->IsArray())
        for (const JsonValue& item : scenesArr->Array)
        {
            SchoolSceneNode node{StrField(item, "id"), StrField(item, "titleKey"), StrField(item, "promptKey"),
                                 StrField(item, "speaker"), SchoolOriginVariant::Both, {}, StrField(item, "conversationTopic"), {}};
            if (const auto orig = ParseSchoolOriginVariant(StrField(item, "originVariant"))) node.Origin = *orig;
            else problems.push_back("scene '" + node.Id + "' has unknown originVariant");
            if (const JsonValue* sc = item.Find("schedule"); sc != nullptr && sc->IsObject())
            {
                const auto slot = ParseSchoolSceneSlotKind(StrField(*sc, "slotKind"));
                if (!slot) problems.push_back("scene '" + node.Id + "' has unknown slotKind");
                node.Schedule = {slot.value_or(SchoolSceneSlotKind::TimetablePeriod), StrField(*sc, "bellSchedule"),
                                 IntField(*sc, "periodIndex", -1), StrField(*sc, "subject"), StrField(*sc, "zone"),
                                 IntField(*sc, "startMinute", 0), IntField(*sc, "endMinute", 0)};
            }
            if (const JsonValue* choicesArr = item.Find("choices"); choicesArr != nullptr && choicesArr->IsArray())
                for (const JsonValue& ch : choicesArr->Array)
                {
                    SchoolSceneChoice choice{StrField(ch, "id"), StrField(ch, "labelKey"), StrField(ch, "outcomeKey"),
                                             IntField(ch, "costMinutes", 0), BoolField(ch, "isRefusal", false),
                                             BoolField(ch, "humiliating", false), StrField(ch, "nextScene"),
                                             StrField(ch, "resolution"), {}};
                    if (const JsonValue* cons = ch.Find("consequence"); cons != nullptr && cons->IsObject())
                    {
                        const auto act = ParseActKind(StrField(*cons, "relationshipAct"));
                        if (!act) problems.push_back("choice '" + choice.Id + "' has unknown relationshipAct");
                        choice.Consequence = {StrField(*cons, "targetPerson"), act.value_or(RelationshipActKind::ShareTime),
                                              StrField(*cons, "relationshipEvent"), StrField(*cons, "threadEvent"),
                                              StrField(*cons, "factSet"), StrField(*cons, "factCleared"),
                                              StrField(*cons, "journalAgreementFact")};
                    }
                    node.Choices.push_back(std::move(choice));
                }
            arc.Scenes.push_back(std::move(node));
        }

    if (const JsonValue* resArr = d.Find("resolutions"); resArr != nullptr && resArr->IsArray())
        for (const JsonValue& r : resArr->Array) if (r.IsString()) arc.Resolutions.push_back(r.String);
    return arc;
}

std::vector<std::string> ValidateSchoolSceneArc(const SchoolSceneArc& arc, const ContentDb& db)
{
    std::vector<std::string> errs;
    if (arc.Id.rfind(SchoolSceneIdPrefix, 0) != 0)
        errs.push_back("arc ID '" + arc.Id + "' must start with '" + std::string(SchoolSceneIdPrefix) + "'");
    const BellScheduleResult bells = LoadBellSchedules(db);
    const SchoolCalendarResult cals = LoadSchoolCalendars(db);
    const ThreadTemplateResult threads = LoadThreadTemplates(db);

    const SchoolCalendar* cal = cals.Set.FindById(arc.CalendarId);
    if (cal == nullptr) errs.push_back("arc names calendar '" + arc.CalendarId + "', which does not exist");
    else
    {
        if (cal->InstitutionId != arc.InstitutionId) errs.push_back("calendar institution does not match arc institution");
        if (std::none_of(cal->Terms.begin(), cal->Terms.end(), [&](const auto& t) { return t.Id == arc.TermId; }))
            errs.push_back("calendar '" + arc.CalendarId + "' has no term '" + arc.TermId + "'");
        if (!arc.CalendarSpanId.empty() && !CalendarHasSpan(*cal, arc.CalendarSpanId))
            errs.push_back("calendar '" + arc.CalendarId + "' has no span '" + arc.CalendarSpanId + "'");
    }

    const ThreadTemplate* tmpl = threads.Set.FindById(arc.ThreadTemplateId);
    if (tmpl == nullptr) errs.push_back("arc names threadTemplate '" + arc.ThreadTemplateId + "', which does not exist");

    std::set<std::string> castIds;
    bool hasFamilyAdult = false, hasCareAdult = false;
    for (const SchoolSceneCastEntry& c : arc.Cast)
    {
        if (!castIds.insert(c.PersonId).second) errs.push_back("duplicate cast person '" + c.PersonId + "'");
        const SchoolCastMember* known = FindSchoolLifeCast(c.PersonId);
        if (known == nullptr) errs.push_back("unknown school cast person '" + c.PersonId + "'");
        else if (known->Role != c.Role || known->bProposedNewCast != c.bProposedNewCast)
            errs.push_back("cast metadata mismatch for '" + c.PersonId + "'");
        if (c.Role == "family_adult" && c.Origin == SchoolOriginVariant::Family) hasFamilyAdult = true;
        if (c.Role == "care_adult" && c.Origin == SchoolOriginVariant::CareHome) hasCareAdult = true;
    }
    if (!hasFamilyAdult || !hasCareAdult)
        errs.push_back("arc cast must declare both a family_adult (family) and a care_adult (care_home)");

    std::set<std::string> resSet(arc.Resolutions.begin(), arc.Resolutions.end());
    if (resSet.size() != arc.Resolutions.size() || resSet.empty()) errs.push_back("arc resolutions must be unique and non-empty");
    if (tmpl != nullptr)
        for (const std::string& r : arc.Resolutions)
            if (tmpl->FindResolution(r) == nullptr)
                errs.push_back("resolution '" + r + "' is not declared by threadTemplate '" + arc.ThreadTemplateId + "'");

    std::set<std::string> sceneIds;
    bool hasFamilyScene = false, hasCareScene = false;
    for (const SchoolSceneNode& s : arc.Scenes)
    {
        if (!sceneIds.insert(s.Id).second) errs.push_back("duplicate scene ID '" + s.Id + "'");
        if (s.Origin == SchoolOriginVariant::Family) hasFamilyScene = true;
        if (s.Origin == SchoolOriginVariant::CareHome) hasCareScene = true;
        if (castIds.count(s.SpeakerPersonId) == 0) errs.push_back("scene '" + s.Id + "' speaker '" + s.SpeakerPersonId + "' is not in cast");
        if (s.Schedule.StartMinute >= s.Schedule.EndMinute) errs.push_back("scene '" + s.Id + "' has invalid schedule window");

        const auto slot = s.Schedule.SlotKind;
        if (slot == SchoolSceneSlotKind::Assembly || slot == SchoolSceneSlotKind::TimetablePeriod ||
            slot == SchoolSceneSlotKind::SchoolBreak || slot == SchoolSceneSlotKind::AfterSchool)
        {
            const BellSchedule* bell = bells.Set.FindById(s.Schedule.BellScheduleId);
            if (bell == nullptr) errs.push_back("scene '" + s.Id + "' names unknown bellSchedule '" + s.Schedule.BellScheduleId + "'");
            else if (slot == SchoolSceneSlotKind::TimetablePeriod)
            {
                const BellBlock* blk = BellBlockAt(*bell, s.Schedule.StartMinute);
                if (blk == nullptr || blk->Kind != BellBlockKind::Period || BellPeriodIndex(*bell, *blk) != s.Schedule.PeriodIndex ||
                    blk->StartMinute != s.Schedule.StartMinute || blk->EndMinute != s.Schedule.EndMinute ||
                    blk->Room != s.Schedule.Zone || blk->Subject != s.Schedule.Subject)
                    errs.push_back("scene '" + s.Id + "' timetable_period does not match bellSchedule '" + bell->Id + "'");
            }
            else if (slot == SchoolSceneSlotKind::SchoolBreak)
            {
                const BellBlock* blk = BellBlockAt(*bell, s.Schedule.StartMinute);
                if (blk == nullptr || blk->Kind != BellBlockKind::Break || blk->Room != s.Schedule.Zone)
                    errs.push_back("scene '" + s.Id + "' school_break does not match bellSchedule break");
            }
            else if (slot == SchoolSceneSlotKind::AfterSchool &&
                     (bell->Bells.empty() || s.Schedule.StartMinute < bell->Bells.back() || s.Schedule.EndMinute > SchoolHomeEveningStartMinute))
                errs.push_back("scene '" + s.Id + "' after_school window must lie between dismissal and evening start");
        }
        else if (slot == SchoolSceneSlotKind::CareHandover &&
                 (s.Origin != SchoolOriginVariant::CareHome || s.Schedule.StartMinute < SchoolCareWorkerDutyStartMinute ||
                  s.Schedule.EndMinute > SchoolCareWorkerDutyEndMinute || s.Schedule.Zone != "location.nabapara.care_home"))
            errs.push_back("scene '" + s.Id + "' care_handover must be care_home at location.nabapara.care_home inside 07:00..15:00");
        else if (slot == SchoolSceneSlotKind::HomeEvening &&
                 (s.Schedule.StartMinute < SchoolHomeEveningStartMinute || s.Schedule.EndMinute > SchoolHomeEveningEndMinute))
            errs.push_back("scene '" + s.Id + "' home_evening must lie inside 18:00..22:00");

        if (s.Choices.size() < 2) errs.push_back("scene '" + s.Id + "' needs at least 2 choices");
        bool hasRefusal = false;
        std::set<std::string> choiceIds;
        for (const SchoolSceneChoice& ch : s.Choices)
        {
            if (!choiceIds.insert(ch.Id).second) errs.push_back("scene '" + s.Id + "' has duplicate choice '" + ch.Id + "'");
            if (ch.bRefusal) hasRefusal = true;
            if (ch.bHumiliating) errs.push_back("choice '" + ch.Id + "' in scene '" + s.Id + "' is marked humiliating");
            if (ch.bRefusal && (ch.Consequence.ActKind == RelationshipActKind::Insult || ch.Consequence.ActKind == RelationshipActKind::Betray))
                errs.push_back("refusal choice '" + ch.Id + "' must not use humiliating act insult/betray");
            if (ch.NextSceneId.empty() == ch.ResolutionId.empty())
                errs.push_back("choice '" + ch.Id + "' must set exactly one of nextScene or resolution");
            if (!ch.ResolutionId.empty() && resSet.count(ch.ResolutionId) == 0)
                errs.push_back("choice '" + ch.Id + "' names unknown resolution '" + ch.ResolutionId + "'");
            if (castIds.count(ch.Consequence.TargetPersonId) == 0)
                errs.push_back("choice '" + ch.Id + "' targetPerson '" + ch.Consequence.TargetPersonId + "' is not in cast");
            if (!IsRelationshipEventTypeFair(ch.Consequence.RelationshipEventType))
                errs.push_back("choice '" + ch.Id + "' relationshipEvent '" + ch.Consequence.RelationshipEventType + "' is not fair");
            if (!ch.Consequence.ThreadEventType.empty() && tmpl != nullptr &&
                std::none_of(tmpl->Triggers.begin(), tmpl->Triggers.end(),
                             [&](const ThreadEventTrigger& tr) { return tr.EventType == ch.Consequence.ThreadEventType; }))
                errs.push_back("choice '" + ch.Id + "' threadEvent '" + ch.Consequence.ThreadEventType + "' is not in threadTemplate");
        }
        if (!hasRefusal) errs.push_back("scene '" + s.Id + "' has no refusal choice");
    }
    if (!hasFamilyScene || !hasCareScene) errs.push_back("arc must include dedicated family and care_home variant scenes");

    const SchoolSceneNode* entry = arc.FindScene(arc.EntrySceneId);
    if (entry == nullptr || entry->Origin != SchoolOriginVariant::Both)
        errs.push_back("entryScene '" + arc.EntrySceneId + "' must exist and have originVariant 'both'");
    else
        for (const bool care : {false, true})
        {
            std::set<std::string> reachable{entry->Id}, reachedRes;
            for (bool changed = true; changed;)
            {
                changed = false;
                for (const SchoolSceneNode& s : arc.Scenes)
                {
                    if (reachable.count(s.Id) == 0) continue;
                    for (const SchoolSceneChoice& ch : s.Choices)
                    {
                        if (!ch.ResolutionId.empty()) reachedRes.insert(ch.ResolutionId);
                        if (!ch.NextSceneId.empty())
                        {
                            const SchoolSceneNode* next = arc.FindScene(ch.NextSceneId);
                            if (next == nullptr) errs.push_back("choice '" + ch.Id + "' names missing nextScene '" + ch.NextSceneId + "'");
                            else if (SchoolOriginMatches(next->Origin, care) && reachable.insert(next->Id).second) changed = true;
                        }
                    }
                }
            }
            const std::string tag = care ? "care_home" : "family";
            for (const SchoolSceneNode& s : arc.Scenes)
                if (SchoolOriginMatches(s.Origin, care) && reachable.count(s.Id) == 0)
                    errs.push_back("scene '" + s.Id + "' is unreachable for origin '" + tag + "'");
            for (const std::string& r : arc.Resolutions)
                if (reachedRes.count(r) == 0)
                    errs.push_back("resolution '" + r + "' is unreachable for origin '" + tag + "'");
        }
    return errs;
}

SchoolSceneLoadResult LoadSchoolSceneCatalogue(const ContentDb& db)
{
    SchoolSceneLoadResult res;
    std::vector<ContentIssue> issues;
    ContentSchema::ValidateSchoolSceneSemantics(db, issues);
    if (!issues.empty()) return res.Error = issues.front().Id + ": " + issues.front().Message, res;
    for (const ContentRecord* rec : db.OfType(SchoolSceneTypeName))
    {
        std::vector<std::string> problems;
        res.Catalogue.Arcs.push_back(ParseSchoolSceneRecord(*rec, problems));
    }
    std::sort(res.Catalogue.Arcs.begin(), res.Catalogue.Arcs.end(),
              [](const SchoolSceneArc& a, const SchoolSceneArc& b) { return a.Id < b.Id; });
    if (res.Catalogue.Arcs.empty()) res.Error = "no school_scene records found";
    return res;
}

ContentLoadResult LoadContentWithSchoolScenes(const std::vector<ContentFile>& files)
{
    const auto& known = KnownContentTypes();
    if (std::find(known.begin(), known.end(), SchoolSceneTypeName) != known.end()) return LoadContent(files);

    std::vector<ContentFile> baseFiles, sceneFiles;
    for (const ContentFile& f : files)
        (f.Path.find("/school_scene/") != std::string::npos ? sceneFiles : baseFiles).push_back(f);

    ContentLoadResult result = LoadContent(baseFiles);
    for (const ContentFile& f : sceneFiles)
    {
        const JsonParseResult parsed = ParseJson(f.Text);
        if (!parsed.Value || !parsed.Value->IsObject())
        {
            result.Issues.push_back({f.Path, "", "file is not a valid JSON object"});
            continue;
        }
        const std::string id = StrField(*parsed.Value, "id"), type = StrField(*parsed.Value, "type");
        if (type != SchoolSceneTypeName) result.Issues.push_back({f.Path, id, "expected type school_scene"});
        if (!f.Path.ends_with("/school_scene/" + id + ".json"))
            result.Issues.push_back({f.Path, id, "file path must be <type>/<id>.json"});
        if (!result.Db.Records.emplace(id, ContentRecord{type, id, f.Path, *parsed.Value, {}}).second)
            result.Issues.push_back({f.Path, id, "duplicate ID " + id});
    }
    ContentSchema::ValidateSchoolSceneSemantics(result.Db, result.Issues);
    return result;
}

SchoolTextLoadResult LoadSchoolTextTable(std::string_view jsonText)
{
    SchoolTextLoadResult res;
    const JsonParseResult parsed = ParseJson(jsonText);
    if (!parsed.Value || !parsed.Value->IsObject())
        return res.Error = parsed.Error.empty() ? "root must be a JSON object" : parsed.Error, res;
    const JsonValue* strings = parsed.Value->Find("strings");
    const JsonValue& tableObj = (strings != nullptr && strings->IsObject()) ? *strings : *parsed.Value;
    for (const auto& [k, v] : tableObj.Object)
    {
        if (k == "locale" || k == "bundle" || k == "note") continue;
        if (!v.IsString() || v.String.empty())
            return res.Table.Entries.clear(), res.Error = "text key '" + k + "' must be a non-empty string", res;
        res.Table.Entries.emplace(k, v.String);
    }
    if (res.Table.Entries.empty()) res.Error = "text table is empty";
    return res;
}

std::vector<std::string> ValidateSchoolSceneTextCoverage(const SchoolSceneCatalogue& catalogue, const SchoolTextTable& textTable)
{
    std::vector<std::string> missing;
    const auto req = [&](const std::string& key, const std::string& ctx) {
        if (key.empty() || textTable.Find(key) == nullptr) missing.push_back(ctx + " missing text key '" + key + "'");
    };
    for (const SchoolSceneArc& arc : catalogue.Arcs)
    {
        req(arc.TitleKey, arc.Id); req(arc.SummaryKey, arc.Id);
        for (const SchoolSceneNode& s : arc.Scenes)
        {
            req(s.TitleKey, s.Id); req(s.PromptKey, s.Id);
            for (const SchoolSceneChoice& ch : s.Choices)
            {
                req(ch.LabelKey, ch.Id); req(ch.OutcomeKey, ch.Id);
                if (!ch.Consequence.JournalAgreementFact.empty()) req("journal.agreement." + ch.Consequence.JournalAgreementFact, ch.Id);
            }
        }
    }
    return missing;
}

std::vector<const SchoolSceneNode*> QueryAvailableSchoolScenes(const SchoolSceneCatalogue& catalogue, const SchoolLifeTrigger& trigger)
{
    std::vector<const SchoolSceneNode*> out;
    for (const SchoolSceneArc& arc : catalogue.Arcs)
    {
        if (arc.TermId != trigger.CurrentTermId) continue;
        const auto it = trigger.ArcProgress.find(arc.Id);
        const std::string targetId = (it == trigger.ArcProgress.end() || it->second.empty()) ? arc.EntrySceneId : it->second;
        if (targetId.rfind("resolved:", 0) == 0) continue;
        const SchoolSceneNode* node = arc.FindScene(targetId);
        if (node != nullptr && SchoolOriginMatches(node->Origin, trigger.bGrewUpInCare) &&
            (trigger.CurrentZone.empty() || node->Schedule.Zone == trigger.CurrentZone) &&
            trigger.MinuteOfDay >= node->Schedule.StartMinute && trigger.MinuteOfDay < node->Schedule.EndMinute)
            out.push_back(node);
    }
    return out;
}

SchoolLifeOutcome ResolveSchoolSceneChoice(const SchoolSceneCatalogue& catalogue, std::string_view arcId,
                                           std::string_view sceneId, std::string_view choiceId, bool bGrewUpInCare)
{
    SchoolLifeOutcome out{false, "", std::string(arcId), std::string(sceneId), std::string(choiceId), "", "", "", 0, {}};
    const SchoolSceneArc* arc = catalogue.FindArc(arcId);
    if (arc == nullptr) return out.Error = "unknown arc", out;
    const SchoolSceneNode* node = arc->FindScene(sceneId);
    if (node == nullptr || !SchoolOriginMatches(node->Origin, bGrewUpInCare)) return out.Error = "unavailable scene", out;
    for (const SchoolSceneChoice& ch : node->Choices)
    {
        if (ch.Id != choiceId) continue;
        if (!ch.NextSceneId.empty())
        {
            const SchoolSceneNode* next = arc->FindScene(ch.NextSceneId);
            if (next == nullptr || !SchoolOriginMatches(next->Origin, bGrewUpInCare))
                return out.Error = "choice targets scene for different origin", out;
        }
        return out.bOk = true, out.NextSceneId = ch.NextSceneId, out.ResolutionId = ch.ResolutionId,
               out.OutcomeTextKey = ch.OutcomeKey, out.ElapsedMinutes = ch.CostMinutes, out.Consequence = ch.Consequence, out;
    }
    return out.Error = "unknown choice", out;
}
} // namespace PerfectWorld
