#include "TutorialContent.h"

#include "ContentSchemas.h"

#include <algorithm>
#include <array>
#include <set>
#include <utility>

namespace PerfectWorld
{
namespace
{
const std::string& R93String(const JsonValue& value, std::string_view field)
{
    return value.Find(field)->String; // only after recursive shape validation
}
bool R93Shape(const ContentDb& db, const ContentSchema::TypeSchema& schema, const JsonValue& data,
    std::string_view parent, std::set<std::string>& identities, std::string& error)
{
    using ContentSchema::FieldKind;
    const auto fail = [&](std::string message) { error = std::move(message); return false; };
    if (!data.IsObject()) return fail("expected an object in " + schema.Type);
    std::set<std::string> fields;
    for (const auto& [key, value] : data.Object)
    {
        (void)value;
        if (!fields.insert(key).second) return fail("duplicate field " + key);
        if (std::none_of(schema.Fields.begin(), schema.Fields.end(), [&](const auto& f) { return f.Name == key; }))
            return fail("unknown field " + key);
    }
    for (const auto& field : schema.Fields)
    {
        const auto* value = data.Find(field.Name);
        if (value == nullptr)
        {
            if (field.Required) return fail("missing required field " + field.Name);
            continue;
        }
        const auto reference = [&](const JsonValue& ref) {
            const auto* target = ref.IsString() ? db.Find(ref.String) : nullptr;
            return target != nullptr && target->Type == field.RefType;
        };
        bool valid = false;
        switch (field.Kind)
        {
        case FieldKind::String: valid = value->IsString() && !value->String.empty(); break;
        case FieldKind::Enum:
            valid = value->IsString() && std::find(field.Values.begin(), field.Values.end(), value->String) != field.Values.end();
            break;
        case FieldKind::Int: valid = value->Integer && *value->Integer >= field.Min && *value->Integer <= field.Max; break;
        case FieldKind::Ref: valid = reference(*value); break;
        case FieldKind::RefArray:
            valid = value->IsArray() && std::all_of(value->Array.begin(), value->Array.end(), reference); break;
        case FieldKind::ObjectArray:
            if (!value->IsArray()) break;
            for (const auto& child : value->Array)
                if (!R93Shape(db, *field.Nested, child, parent.empty() ? R93String(data, "id") : parent, identities, error))
                    return false;
            valid = true;
            break;
        default: break; // tutorial schemas use only the kinds above
        }
        if (!valid) return fail("invalid field or reference " + field.Name);
    }
    if (!schema.IdPrefix.empty())
    {
        const auto& id = R93String(data, "id");
        const auto* record = db.Find(id);
        const bool stable = id.size() > schema.IdPrefix.size() && id.size() <= 96 &&
            std::all_of(id.begin(), id.end(), [](char c) {
                return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_';
            });
        if (!id.starts_with(schema.IdPrefix) || !stable || !identities.insert(id).second ||
            record == nullptr || record->Id != id || record->Type != schema.Type || record->ParentId != parent)
            return fail("invalid or duplicate registered identity " + id);
        if (parent.empty() && R93String(data, "type") != schema.Type) return fail("record type differs from metadata");
    }
    return true;
}

TutorialContentResult R93Decode(const ContentDb& db)
{
    TutorialContentResult out;
    const auto records = db.OfType("tutorial_content");
    if (records.size() != 1)
    {
        out.Issues.push_back({"tutorial_content", "tutorial.awakening", "one Awakening tutorial is required"});
        return out;
    }
    const auto& record = *records.front();
    const auto issue = [&](std::string message) { out.Issues.push_back({record.File, record.Id, std::move(message)}); };
    std::string error;
    std::set<std::string> identities;
    if (!R93Shape(db, ContentSchema::R93TutorialSchema(), record.Data, "", identities, error))
    { issue(error); return out; }
    const auto& data = record.Data;
    auto& content = out.Content;
    content.Id = record.Id;
    content.SchoolId = R93String(data, "school");
    content.AnnouncementId = R93String(data, "announcement");
    content.FamilyEveningId = R93String(data, "familyEvening");
    content.CareEveningId = R93String(data, "careEvening");
    const auto* school = db.Find(content.SchoolId);
    const auto* schoolKind = school->Data.Find("kind");
    const auto* dungeon = school->Data.Find("tutorialDungeon");
    if (content.Id != "tutorial.awakening" || schoolKind == nullptr || schoolKind->String != "school" ||
        dungeon == nullptr || dungeon->Type != JsonValue::Kind::Bool || !dungeon->Bool)
        issue("tutorial needs its stable identity and a school with a tutorial dungeon");
    std::set<std::string> actorIds;
    std::map<std::string, std::size_t> roleCounts;
    for (const auto& value : data.Find("actors")->Array)
    {
        TutorialActor actor{R93String(value, "id"), R93String(value, "role"), R93String(value, "nameKey"), {}, {}, {}};
        if (const auto* schedule = value.Find("schedule")) actor.ScheduleId = schedule->String;
        if (const auto* classroom = value.Find("classroom")) actor.ClassroomId = classroom->String;
        if (const auto* member = value.Find("member")) actor.MemberId = member->String;
        const bool fromSchedule = !actor.ScheduleId.empty();
        const bool fromClassroom = !actor.ClassroomId.empty() && !actor.MemberId.empty();
        if (actor.Role == "classmate")
        {
            if (fromSchedule == fromClassroom || (fromSchedule && (!actor.ClassroomId.empty() || !actor.MemberId.empty())))
                issue("classmate needs exactly one established cast source");
            if (fromClassroom)
            {
                const auto* members = db.Find(actor.ClassroomId)->Data.Find("members");
                bool found = false;
                if (members != nullptr && members->IsArray())
                    for (const auto& member : members->Array)
                    {
                        const auto* id = member.Find("id");
                        const auto* self = member.Find("self");
                        found |= id != nullptr && id->IsString() && id->String == actor.MemberId && self != nullptr &&
                            self->Type == JsonValue::Kind::Bool && !self->Bool;
                    }
                if (!found) issue("classroom member is missing or is the player");
            }
        }
        else if (fromSchedule || !actor.ClassroomId.empty() || !actor.MemberId.empty())
            issue("contextual teacher, family and evening staff roles must be bound by the presenter");
        actorIds.insert(actor.Id);
        ++roleCounts[actor.Role];
        content.Actors.push_back(std::move(actor));
    }
    if (content.Actors.size() != 5 || roleCounts["class_teacher"] != 1 || roleCounts["classmate"] != 2 ||
        roleCounts["family_member"] != 1 || roleCounts["evening_care_staff"] != 1)
        issue("tutorial needs a familiar teacher, two established classmates and both household roles");
    const auto localActor = [&](const std::string& id) { return actorIds.contains(id); };
    for (const auto& value : data.Find("scenes")->Array)
    {
        TutorialScene scene{R93String(value, "id"), {}};
        for (const auto& line : value.Find("lines")->Array)
        {
            TutorialLine decoded{R93String(line, "actor"), R93String(line, "textKey")};
            if (!localActor(decoded.ActorId)) issue("scene actor belongs to another tutorial");
            scene.Lines.push_back(std::move(decoded));
        }
        if (scene.Lines.empty()) issue("scene needs lines");
        content.Scenes.push_back(std::move(scene));
    }
    if (content.Scenes.size() != 3 || content.AnnouncementId == content.FamilyEveningId ||
        content.AnnouncementId == content.CareEveningId || content.FamilyEveningId == content.CareEveningId)
        issue("announcement and both evening variants must be distinct scenes");
    for (const auto& id : {content.AnnouncementId, content.FamilyEveningId, content.CareEveningId})
        if (FindTutorialScene(content, id) == nullptr) issue("scene reference belongs to another tutorial");
    const auto sceneRoles = [&](const std::string& id) {
        std::set<std::string> roles;
        if (const auto* scene = FindTutorialScene(content, id))
            for (const auto& line : scene->Lines)
                for (const auto& actor : content.Actors)
                    if (actor.Id == line.ActorId) roles.insert(actor.Role);
        return roles;
    };
    if (sceneRoles(content.AnnouncementId) != std::set<std::string>{"class_teacher", "classmate"} ||
        sceneRoles(content.FamilyEveningId) != std::set<std::string>{"family_member"} ||
        sceneRoles(content.CareEveningId) != std::set<std::string>{"evening_care_staff"})
        issue("scene speakers do not match announcement or household context");
    if (const auto* scene = FindTutorialScene(content, content.AnnouncementId))
        for (const auto& actor : content.Actors)
            if ((actor.Role == "class_teacher" || actor.Role == "classmate") &&
                std::none_of(scene->Lines.begin(), scene->Lines.end(), [&](const auto& line) { return line.ActorId == actor.Id; }))
                issue("announcement must include the teacher and both classmates");

    constexpr std::array<std::string_view, 3> R93SituationIds{
        "situation.awakening.crossing", "situation.awakening.corridor", "situation.awakening.delivery"};
    constexpr std::array<std::array<ConductAction, 4>, 3> R93SituationActions{{
        {ConductAction::Assist, ConductAction::Observe, ConductAction::Hold, ConductAction::Withdraw},
        {ConductAction::Hold, ConductAction::Improvise, ConductAction::Evade, ConductAction::Withdraw},
        {ConductAction::Assist, ConductAction::Observe, ConductAction::Evade, ConductAction::Withdraw}}};
    std::set<std::string> participants;
    for (const auto& actor : content.Actors)
        if (actor.Role == "class_teacher" || actor.Role == "classmate") participants.insert(actor.Id);
    const auto& situations = data.Find("situations")->Array;
    if (situations.size() != R93SituationIds.size()) issue("tutorial needs three ordered situations");
    for (std::size_t index = 0; index < situations.size(); ++index)
    {
        const auto& value = situations[index];
        TutorialSituation situation{R93String(value, "id"), R93String(value, "promptKey"), R93String(value, "recoveryKey"),
            static_cast<std::int32_t>(*value.Find("stakes")->Integer), {}, {}};
        if (index >= R93SituationIds.size() || situation.Id != R93SituationIds[index] || !IsConductTokenFair(situation.Id))
            issue("situation order or stable ID differs");
        std::set<std::string> parties;
        for (const auto& actor : value.Find("actors")->Array)
        {
            if (!localActor(actor.String) || !parties.insert(actor.String).second) issue("invalid or duplicate situation actor");
            situation.ActorIds.push_back(actor.String);
        }
        if (parties != participants) issue("situation needs the teacher and both classmates");
        std::set<ConductAction> actions;
        for (const auto& choice : value.Find("choices")->Array)
        {
            TutorialChoice decoded{R93String(choice, "id"), R93String(choice, "labelKey"), R93String(choice, "actionKey"),
                R93String(choice, "consequenceKey"), *ParseConductAction(R93String(choice, "action"))};
            const auto prefix = "choice.awakening." + situation.Id.substr(std::string_view("situation.awakening.").size()) + ".";
            if (decoded.Id != prefix + std::string(ConductActionName(decoded.Action)) ||
                !IsConductTokenFair(decoded.Id) || !actions.insert(decoded.Action).second)
                issue("choice must name its situation and distinct conduct action exactly");
            if (index < R93SituationActions.size() &&
                (situation.Choices.size() >= R93SituationActions[index].size() ||
                    decoded.Action != R93SituationActions[index][situation.Choices.size()]))
                issue("choice order or conduct tag differs from the tutorial contract");
            situation.Choices.push_back(std::move(decoded));
        }
        if (situation.Choices.size() != 4 || !actions.contains(ConductAction::Withdraw))
            issue("each situation needs three substantive routes and withdrawal");
        content.Situations.push_back(std::move(situation));
    }
    if (!out.Ok()) out.Content = {};
    return out;
}
} // namespace

TutorialContentResult LoadTutorialContent(const ContentDb& db, const ContentFile& textFile)
{
    auto out = R93Decode(db);
    if (!out.Ok()) return out;
    const auto issue = [&](std::string key, std::string message) {
        out.Issues.push_back({textFile.Path, std::move(key), std::move(message)});
    };
    std::string_view bytes = textFile.Text;
    if (bytes.starts_with("\xEF\xBB\xBF")) bytes.remove_prefix(3);
    const auto parsed = ParseJson(bytes);
    if (!parsed.Value || !parsed.Value->IsObject())
    { issue("tutorial.awakening", "text table must be a JSON object: " + parsed.Error); out.Content = {}; return out; }
    for (const auto& [key, value] : parsed.Value->Object)
    {
        if (!key.starts_with("text.awakening.") || !value.IsString() ||
            value.String.find_first_not_of(" \t\r\n") == std::string::npos)
            issue(key, "invalid text key or empty translation");
        else out.Content.Text.emplace(key, value.String);
    }
    const auto required = [&](const std::string& key) {
        if (!out.Content.Text.contains(key)) issue(key, "missing text key " + key);
    };
    for (const auto& actor : out.Content.Actors) required(actor.NameKey);
    for (const auto& scene : out.Content.Scenes) for (const auto& line : scene.Lines) required(line.TextKey);
    for (const auto& situation : out.Content.Situations)
    {
        required(situation.PromptKey);
        required(situation.RecoveryKey);
        for (const auto& choice : situation.Choices)
        { required(choice.LabelKey); required(choice.ActionKey); required(choice.ConsequenceKey); }
    }
    if (!out.Ok()) out.Content = {};
    return out;
}
const TutorialSituation* FindTutorialSituation(const TutorialContent& content, std::string_view id)
{
    for (const auto& situation : content.Situations) if (situation.Id == id) return &situation;
    return nullptr;
}
const TutorialChoice* FindTutorialChoice(const TutorialSituation& situation, std::string_view id)
{
    for (const auto& choice : situation.Choices) if (choice.Id == id) return &choice;
    return nullptr;
}
const TutorialScene* FindTutorialScene(const TutorialContent& content, std::string_view id)
{
    for (const auto& scene : content.Scenes) if (scene.Id == id) return &scene;
    return nullptr;
}
const std::string* FindTutorialText(const TutorialContent& content, std::string_view key)
{
    const auto found = content.Text.find(key);
    return found == content.Text.end() ? nullptr : &found->second;
}
namespace ContentSchema
{
void ValidateTutorialContentSemantics(const ContentDb& db, std::vector<ContentIssue>& issues)
{
    if (db.OfType("tutorial_content").empty()) return; // unrelated small registries remain valid
    const auto out = R93Decode(db);
    issues.insert(issues.end(), out.Issues.begin(), out.Issues.end());
}
}
} // namespace PerfectWorld
