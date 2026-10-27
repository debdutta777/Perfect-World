#include "SchoolFollowupContent.h"

#include "ContentSchemaBuilders.h"

#include <algorithm>
#include <set>
#include <utility>

namespace PerfectWorld
{
namespace
{
bool A8Token(std::string_view value, std::string_view prefix)
{
    return value.starts_with(prefix) && value.size() > prefix.size() && value.size() <= 128 &&
        std::all_of(value.begin(), value.end(), [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_';
        });
}
std::string A8Shape(const JsonValue& data)
{
    using ContentSchema::FieldKind;
    if (!data.IsObject()) return "record must be an object";
    const auto& schema = ContentSchema::SchoolFollowupSchema();
    std::set<std::string> seen;
    for (const auto& [key, value] : data.Object)
    {
        (void)value;
        if (!seen.insert(key).second) return "duplicate field " + key;
        if (std::none_of(schema.Fields.begin(), schema.Fields.end(), [&](const auto& f) { return f.Name == key; }))
            return "unknown field " + key;
    }
    for (const auto& field : schema.Fields)
    {
        const auto* value = data.Find(field.Name);
        if (!value) { if (field.Required) return "missing field " + field.Name; else continue; }
        bool valid = false;
        if (field.Kind == FieldKind::String) valid = value->IsString();
        else if (field.Kind == FieldKind::Int)
            valid = value->Type == JsonValue::Kind::Number && value->Integer && *value->Integer >= field.Min && *value->Integer <= field.Max;
        else if (field.Kind == FieldKind::Enum)
            valid = value->IsString() && std::find(field.Values.begin(), field.Values.end(), value->String) != field.Values.end();
        if (!valid) return "invalid field " + field.Name;
    }
    return {};
}
const std::string& A8Text(const JsonValue& data, std::string_view field) { return data.Find(field)->String; }
std::int32_t A8Int(const JsonValue& data, std::string_view field)
{ return static_cast<std::int32_t>(*data.Find(field)->Integer); }
}

SchoolFollowupContentResult LoadSchoolFollowupContent(const ContentDb& db)
{
    SchoolFollowupContentResult out;
    std::set<std::string> appointments, offers, outcomes, ordinary;
    for (const auto* record : db.OfType("school_followup"))
    {
        const auto issue = [&](std::string message) { out.Issues.push_back({record->File, record->Id, std::move(message)}); };
        const auto& data = record->Data;
        if (const auto error = A8Shape(data); !error.empty()) { issue(error); continue; }
        if (db.Find(record->Id) != record || !record->ParentId.empty() || !A8Token(record->Id, "school_followup.p2.") ||
            A8Text(data, "id") != record->Id || A8Text(data, "type") != record->Type)
        { issue("follow-up metadata differs from registered identity"); continue; }
        SchoolFollowupAction action{record->Id, A8Text(data, "outcome"), A8Text(data, "offerId"),
            A8Text(data, "appointmentId"), A8Text(data, "kind"), A8Text(data, "titleTextKey"),
            A8Text(data, "briefTextKey"), A8Text(data, "destination"), A8Int(data, "dayOffset"),
            A8Int(data, "minute"), A8Int(data, "durationMinutes")};
        if (!A8Token(action.AppointmentId, "appointment.p2.") || !appointments.insert(action.AppointmentId).second)
            issue("invalid or duplicate appointment identity");
        if (!A8Token(action.TitleTextKey, "phase2.a8.") || !A8Token(action.BriefTextKey, "phase2.a8."))
            issue("invalid follow-up text key");
        if (action.Minute + action.DurationMinutes > 1440) issue("appointment crosses day boundary");
        if (!action.OfferId.empty())
        {
            const auto known = std::find_if(TowerOffers().begin(), TowerOffers().end(),
                [&](const auto& offer) { return offer.Id == action.OfferId; });
            if (known == TowerOffers().end() || !offers.insert(action.OfferId).second ||
                action.Outcome != "all" || action.Kind != "appointment") issue("invalid or duplicate tower offer mapping");
        }
        else if (action.Outcome == "all") ordinary.insert(action.Kind);
        else outcomes.insert(action.Outcome);
        out.Catalogue.Actions.push_back(std::move(action));
    }
    if (!db.OfType("school_followup").empty())
    {
        for (const auto& offer : TowerOffers())
            if (!offers.contains(std::string(offer.Id))) out.Issues.push_back({"school_followup", "", "missing offer mapping " + std::string(offer.Id)});
        for (const auto* outcome : {"strong", "middle", "poor", "withdrawn", "deferred", "declined"})
            if (!outcomes.contains(outcome)) out.Issues.push_back({"school_followup", "", "missing outcome action " + std::string(outcome)});
        for (const auto* kind : {"school", "study", "rest"})
            if (!ordinary.contains(kind)) out.Issues.push_back({"school_followup", "", "missing ordinary continuation " + std::string(kind)});
    }
    if (!out.Ok()) out.Catalogue = {};
    return out;
}

std::string_view SchoolFollowupOutcome(const TowerSessionView& view)
{
    if (!view.Result.Ok()) return {};
    switch (view.Participation)
    {
    case TowerParticipation::Withdrawn: return "withdrawn";
    case TowerParticipation::Deferred: return "deferred";
    case TowerParticipation::Declined: return "declined";
    case TowerParticipation::Completed:
    case TowerParticipation::Ejected:
        if (view.Result.Decile < 1 || view.Result.Decile > 10) return {};
        return view.Result.Decile == 1 ? "strong" : view.Result.Decile <= 5 ? "middle" : "poor";
    default: return {};
    }
}

std::vector<SchoolFollowupAction> QuerySchoolFollowups(const SchoolFollowupContent& content, const TowerSessionView& view)
{
    std::vector<SchoolFollowupAction> out;
    const auto outcome = SchoolFollowupOutcome(view);
    if (outcome.empty()) return out;
    for (const auto& action : content.Actions)
    {
        if (action.Outcome != "all" && action.Outcome != outcome) continue;
        if (!action.OfferId.empty() && std::find(view.Result.OfferIds.begin(), view.Result.OfferIds.end(), action.OfferId) == view.Result.OfferIds.end()) continue;
        out.push_back(action);
    }
    return out;
}

namespace ContentSchema
{
const TypeSchema& SchoolFollowupSchema()
{
    using namespace Build;
    static const TypeSchema schema{"school_followup", "school_followup.", Base({
        OneOf("outcome", true, {"all", "strong", "middle", "poor", "withdrawn", "deferred", "declined"}),
        Str("offerId", true), Str("appointmentId", true),
        OneOf("kind", true, {"appointment", "school", "study", "rest"}),
        Str("titleTextKey", true), Str("briefTextKey", true),
        OneOf("destination", true, {"place.school", "place.home", "location.nabapara.repair_workshop"}),
        Int("dayOffset", true, 1, 7), Int("minute", true, 0, 1439), Int("durationMinutes", true, 1, 120)})};
    return schema;
}
void ValidateSchoolFollowupSemantics(const ContentDb& db, std::vector<ContentIssue>& issues)
{
    const auto result = LoadSchoolFollowupContent(db);
    issues.insert(issues.end(), result.Issues.begin(), result.Issues.end());
}
}
} // namespace PerfectWorld
