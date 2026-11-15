#pragma once

#include "SchoolWork.h"

#include <map>

namespace PerfectWorld
{
/** One authored assignment, issued at the start of a zero-based teaching period
 * on an explicit school date. No repeating rule or generated assignment IDs. */
struct SchoolWorkBriefDef
{
    SchoolWorkBrief Brief;
    std::string CalendarId;
    std::string BellScheduleId;
    std::string Stream;
    std::string Subject;
    std::string TermId;
    CalendarDay IssueDay = 0;
    std::int32_t Period = 0;
    friend bool operator==(const SchoolWorkBriefDef&, const SchoolWorkBriefDef&) = default;
};
struct SchoolWorkBriefCatalogue
{
    /** Sorted by Brief.Id by the loader; issuance also accepts other orderings. */
    std::vector<SchoolWorkBriefDef> Briefs;
    std::map<std::string, std::string, std::less<>> Text;
};
/** Requires validated calendar/bell catalogues. Refuses closures, assessment and
 * sports/Awakening dates, wrong subject/period/stream, nonmatching papers and bad deadlines. */
[[nodiscard]] PERFECTWORLDCORE_API SchoolIssue ValidateSchoolWorkBriefDef(const SchoolWorkBriefDef& def,
    const SchoolCalendar& calendar, const BellSchedule& bells, const AssessmentCatalogue& papers);
struct SchoolWorkIssuanceResult
{
    SchoolIssue Issue;
    /** Original on refusal; otherwise the candidate for the reducer transaction. */
    SchoolWorkState State;
    /** Newly issued IDs, in (AvailableAt, Id) order. */
    std::vector<std::string> IssuedIds;
    [[nodiscard]] bool Ok() const noexcept { return Issue.Ok(); }
};
/** PURE system/content adapter. Process the inclusive [from, to] boundary range
 * reached by accepted time advancement, identically for manual time/compression.
 * Both endpoints are inclusive so due-now settlement is safe. Existing identical
 * records make replay inert; a changed brief under an issued ID refuses.
 *
 * Calls IssueSchoolWork on a copy at each actual boundary, never at `to` for an
 * earlier assignment. Attendance is deliberately irrelevant. Retrieval is the
 * read-only QuerySchoolAcademicRecord; overdue Begin/Submit remain SchoolWork's.
 * No clock, history cursor, panel, event queue, marks or new persistent state.
 * Historical saves do not replay before `from`; the integrator owns migration. */
[[nodiscard]] PERFECTWORLDCORE_API SchoolWorkIssuanceResult IssueScheduledSchoolWork(
    const SchoolWorkState& state, const SchoolWorkBriefCatalogue& catalogue,
    const SchoolCalendar& calendar, const BellSchedule& bells, const AssessmentCatalogue& papers,
    GameInstant from, GameInstant to);

/**
 * P3-14 recovery window: recovered work is due no earlier than seven days after it reached the life. Disclosed to the
 * player with the recovered record (IssuedAt is the recovery instant, Brief.DueAt the end of the window).
 */
inline constexpr GameInstant SchoolWorkRecoveryWindowMinutes = 7 * MinutesPerDay;

/** The authored brief as recovered at `at`: available from `at`, due at max(authored due, at + recovery window). */
[[nodiscard]] PERFECTWORLDCORE_API SchoolWorkBrief RecoveredSchoolWorkBrief(const SchoolWorkBrief& authored, GameInstant at) noexcept;

/**
 * True when `record` is `authored` issued by reconciliation: IssuedAt == Brief.AvailableAt, later than the authored
 * boundary, and exactly RecoveredSchoolWorkBrief(authored, IssuedAt). A normally issued record has IssuedAt equal to the
 * authored boundary. Scheduled issuance accepts both forms under the same id and refuses anything else.
 */
[[nodiscard]] PERFECTWORLDCORE_API bool IsRecoveredSchoolWork(const SchoolWorkRecord& record, const SchoolWorkBrief& authored) noexcept;

/**
 * PURE P3-14 reconciliation: every brief of this calendar and stream whose authored boundary lies strictly before `now`
 * and that has no record yet (its boundary passed while no catalogue was bound) is issued at `now` as
 * RecoveredSchoolWorkBrief, in (authored AvailableAt, Id) order. Existing records, submissions and deadlines are never
 * changed; an existing record that is neither form refuses identity_conflict. Boundaries at `now` stay with scheduled
 * issuance. Retrieval stays the read-only QuerySchoolAcademicRecord.
 */
[[nodiscard]] PERFECTWORLDCORE_API SchoolWorkIssuanceResult ReconcileMissedSchoolWork(
    const SchoolWorkState& state, const SchoolWorkBriefCatalogue& catalogue,
    const SchoolCalendar& calendar, const BellSchedule& bells, const AssessmentCatalogue& papers, GameInstant now);
} // namespace PerfectWorld
