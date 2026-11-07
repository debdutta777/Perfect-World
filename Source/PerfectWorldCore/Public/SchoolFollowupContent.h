#pragma once

#include "ContentRegistry.h"
#include "TowerSession.h"

namespace PerfectWorld
{
namespace ContentSchema { struct TypeSchema; }

/** Authored next steps, never admissions, awarded money or persistent bookings. */
struct SchoolFollowupAction
{
    std::string Id;
    std::string Outcome; // all, strong, middle, poor, withdrawn, deferred, declined
    std::string OfferId; // empty for ordinary school life and outcome conversations
    std::string AppointmentId;
    std::string Kind; // appointment, school, study, rest
    std::string TitleTextKey;
    std::string BriefTextKey;
    std::string Destination; // existing QuestItem PlaceKey vocabulary
    std::int32_t DayOffset = 1; // preferred day after exam; bridge resolves calendar
    std::int32_t Minute = 960;
    std::int32_t DurationMinutes = 15;
};
struct SchoolFollowupContent { std::vector<SchoolFollowupAction> Actions; };
struct SchoolFollowupContentResult
{
    SchoolFollowupContent Catalogue;
    std::vector<ContentIssue> Issues;
    [[nodiscard]] bool Ok() const { return Issues.empty(); }
};

[[nodiscard]] PERFECTWORLDCORE_API SchoolFollowupContentResult LoadSchoolFollowupContent(const ContentDb& db);
/** Empty while no terminal result is available. Classification uses core result deciles. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view SchoolFollowupOutcome(const TowerSessionView& view);
/** Copies owned action strings. No world mutation and no inferred offer eligibility. */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<SchoolFollowupAction> QuerySchoolFollowups(
    const SchoolFollowupContent& content, const TowerSessionView& view);
namespace ContentSchema
{
[[nodiscard]] PERFECTWORLDCORE_API const TypeSchema& SchoolFollowupSchema();
PERFECTWORLDCORE_API void ValidateSchoolFollowupSemantics(const ContentDb& db, std::vector<ContentIssue>& issues);
}
} // namespace PerfectWorld
