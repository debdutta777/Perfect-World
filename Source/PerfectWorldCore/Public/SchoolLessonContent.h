#pragma once

#include "ContentRegistry.h"
#include "DayLoop.h"
#include "SchoolCalendar.h"
#include "SchoolSessions.h"

#include <array>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * Phase 2 order A1: the timetable and lesson catalogue.
 *
 * Content kind `school_lesson` (Data/Content/school_lesson/p2.*.json, ID prefix "p2."). One record per
 * stream binds the existing structure families (problem, explain, investigate, coordinate) to that stream's
 * real bell schedule: every PERIOD of the schedule gets exactly one SchoolLessonBinding (stream, subject,
 * zero-based period, structure, family, and the title / brief / role text keys). One further record (scope
 * "school") carries what is shared by the whole school: the explanation of every closed day (each holiday and
 * break of the calendar by name, then a generic line per kind), the explanation of every reason a period can
 * be unavailable and what to do next (including recovery after a missed period or an absence), the goal text
 * of every structure runner, and the words for the three lesson modes.
 *
 * The bell schedules stay the only time authority. A binding names the period by index and repeats the
 * schedule's subject so a re-ordered timetable is refused, not silently re-bound. Each lesson also names
 * the (subject, role) the family teaches it through; the pair must be reached by at least one authored case of
 * that family, so no lesson claims a case that does not exist. (Case selection stays the runner's: a lesson
 * binds a family, not a case.)
 *
 * Presentation is text keys only ("phase2.a1.*"). The keys resolve in Data/Text/en/phase2_a1.json, a flat
 * object of key -> nonempty string. LoadSchoolLessonContent refuses a missing key and an unreferenced one.
 *
 * BuildSchoolDayView projects today's timetable for the school-day panel from SchoolSessions' own views
 * (QuerySchoolPeriod, the period history) and the day loop's attendance. It decides nothing: opening, choosing
 * and closing remain the frozen school.lesson.* commands.
 */

inline constexpr std::string_view SchoolLessonContentType = "school_lesson";
inline constexpr std::string_view SchoolLessonTextPrefix = "phase2.a1.";
inline constexpr std::string_view SchoolLessonSchoolScope = "school";

/** Every reason the panel can explain. Codes are the suffixes of school.session.* plus three recovery cases. */
inline constexpr std::array<std::string_view, 12> SchoolLessonExplanationCodes{
    "closed_day", "period_used", "busy", "not_at_bell", "not_attending", "history_full", "invalid_state",
    "foreground_limit", "not_secure", "missed_absent", "missed_attended", "after_absence"};

struct SchoolLessonEntry
{
    SchoolLessonBinding Binding;
    /** The subject token the family recognises this lesson through (a family `subjects` entry). */
    std::string FamilySubject;
    /** The family role this lesson trains (a family `roles` entry). */
    std::string Role;
    friend bool operator==(const SchoolLessonEntry&, const SchoolLessonEntry&) = default;
};
struct SchoolLessonStream
{
    std::string RecordId;
    std::string Stream;
    std::string BellScheduleId;
    /** Period order, one per bell-schedule period. */
    std::vector<SchoolLessonEntry> Lessons;
    friend bool operator==(const SchoolLessonStream&, const SchoolLessonStream&) = default;
};
struct SchoolLessonClosure
{
    SchoolDayKind Kind = SchoolDayKind::Holiday;
    /** A calendar holiday or break ID for a named closure; empty for the generic line of the kind. */
    std::string SpanId;
    std::string TextKey;
    friend bool operator==(const SchoolLessonClosure&, const SchoolLessonClosure&) = default;
};
struct SchoolLessonExplanation
{
    std::string Code;
    std::string TextKey;
    std::string RecoveryTextKey;
    friend bool operator==(const SchoolLessonExplanation&, const SchoolLessonExplanation&) = default;
};
struct SchoolLessonGoalText
{
    SchoolStructure Structure = SchoolStructure::Model;
    /** One of the runner's goal names (ModelGoalNames and its siblings). */
    std::string Goal;
    std::string TextKey;
    friend bool operator==(const SchoolLessonGoalText&, const SchoolLessonGoalText&) = default;
};
struct SchoolLessonModeText
{
    SchoolLessonMode Mode = SchoolLessonMode::Attend;
    std::string TitleTextKey;
    std::string BriefTextKey;
    friend bool operator==(const SchoolLessonModeText&, const SchoolLessonModeText&) = default;
};
struct SchoolLessonContent
{
    /** Sorted by stream name. */
    std::vector<SchoolLessonStream> Streams;
    std::string SchoolRecordId;
    std::string CalendarId;
    std::vector<SchoolLessonClosure> Closures;
    std::vector<SchoolLessonExplanation> Explanations;
    std::vector<SchoolLessonGoalText> Goals;
    std::vector<SchoolLessonModeText> Modes;
    std::map<std::string, std::string, std::less<>> Text;
    friend bool operator==(const SchoolLessonContent&, const SchoolLessonContent&) = default;
};
struct SchoolLessonContentResult
{
    SchoolLessonContent Catalogue; // empty on any hard error
    std::vector<ContentIssue> Issues;
    [[nodiscard]] bool Ok() const { return Issues.empty(); }
};

/** Strict typed decoding and every cross-record rule (bell schedules, calendar, families, cases) with the text. */
[[nodiscard]] PERFECTWORLDCORE_API SchoolLessonContentResult LoadSchoolLessonContent(
    const ContentDb& db, const ContentFile& textFile);
/** The same rules without the text table (what the content validator runs). */
[[nodiscard]] PERFECTWORLDCORE_API SchoolLessonContentResult DecodeSchoolLessonContent(const ContentDb& db);

[[nodiscard]] PERFECTWORLDCORE_API const SchoolLessonStream* FindSchoolLessonStream(
    const SchoolLessonContent& content, std::string_view stream);
[[nodiscard]] PERFECTWORLDCORE_API const SchoolLessonEntry* FindSchoolLesson(
    const SchoolLessonContent& content, std::string_view stream, std::int32_t period);
[[nodiscard]] PERFECTWORLDCORE_API const SchoolLessonEntry* FindSchoolLessonById(
    const SchoolLessonContent& content, std::string_view bindingId);
/** The binding for the command boundary (resolve BindingId here, never from a widget). */
[[nodiscard]] PERFECTWORLDCORE_API const SchoolLessonBinding* FindSchoolLessonBinding(
    const SchoolLessonContent& content, std::string_view bindingId);
[[nodiscard]] PERFECTWORLDCORE_API std::vector<SchoolLessonBinding> SchoolLessonBindings(
    const SchoolLessonContent& content, std::string_view stream);
[[nodiscard]] PERFECTWORLDCORE_API const SchoolLessonExplanation* FindSchoolLessonExplanation(
    const SchoolLessonContent& content, std::string_view code);
/** The named closure for a holiday or break, else the generic line of the kind; null for a school day. */
[[nodiscard]] PERFECTWORLDCORE_API const SchoolLessonClosure* FindSchoolLessonClosure(
    const SchoolLessonContent& content, SchoolDayKind kind, std::string_view spanId);
[[nodiscard]] PERFECTWORLDCORE_API const SchoolLessonModeText* FindSchoolLessonModeText(
    const SchoolLessonContent& content, SchoolLessonMode mode);
[[nodiscard]] PERFECTWORLDCORE_API const std::string* FindSchoolLessonText(
    const SchoolLessonContent& content, std::string_view key);

/** The runner goal a student can pick for a structure's foreground lesson, with its authored words. */
struct SchoolLessonGoalOption
{
    std::string Name;
    std::string TextKey;
    SchoolLessonGoal Goal;
};
[[nodiscard]] PERFECTWORLDCORE_API std::vector<SchoolLessonGoalOption> SchoolLessonGoalOptions(
    const SchoolLessonContent& content, SchoolStructure structure);

enum class SchoolDayRowStatus : std::uint8_t
{
    /** Before the bell: the lesson opens at its start minute. */
    Upcoming,
    /** At the bell with everything satisfied: the three modes are offered. */
    Open,
    /** The lesson is open now. */
    Active,
    Done,
    /** Opened and cancelled: it still used its period (and its foreground slot). */
    Cancelled,
    /** The bell has passed without a lesson. */
    Missed,
    /** At the bell but refused (not attending, busy, history full ...). */
    Unavailable,
};
struct SchoolDayRow
{
    std::string BindingId;
    std::int32_t Period = 0;
    std::string Subject;
    std::string Room;
    std::int32_t StartMinute = 0;
    std::int32_t EndMinute = 0;
    SchoolStructure Structure = SchoolStructure::Model;
    std::string TitleTextKey;
    std::string BriefTextKey;
    std::string RoleTextKey;
    SchoolDayRowStatus Status = SchoolDayRowStatus::Upcoming;
    bool ForegroundAvailable = false;
    bool RoutineAvailable = false;
    bool AttendAvailable = false;
    /** The mode a Done or Cancelled period was spent in. */
    SchoolLessonMode DoneMode = SchoolLessonMode::Attend;
    /** Why the period cannot be opened now, or empty. */
    std::string ReasonTextKey;
    /** What to do instead, or empty. */
    std::string RecoveryTextKey;
    /** Set when an Open period cannot be played in a mode (the two-foreground limit; no secure routine). */
    std::string ForegroundReasonTextKey;
    std::string RoutineReasonTextKey;
    friend bool operator==(const SchoolDayRow&, const SchoolDayRow&) = default;
};
struct SchoolDayView
{
    /** Empty when the view could be built; otherwise a stable school.day.* code and the rest is empty. */
    std::string Issue;
    std::string Stream;
    std::int64_t Day = 0;
    std::int32_t MinuteOfDay = 0;
    SchoolDayKind Kind = SchoolDayKind::SchoolDay;
    bool SchoolDay = false;
    /** The closure's words on a day without school; the next day with school, or -1. */
    std::string ClosureTextKey;
    std::int64_t NextSchoolDay = -1;
    std::int32_t ForegroundUsed = 0;
    std::int32_t ForegroundLimit = SchoolForegroundLimit;
    bool LessonOpen = false;
    std::string ActiveBindingId;
    /** School days among the last fortnight recorded as wholly absent, with the words to recover from them. */
    std::int32_t RecentAbsentDays = 0;
    std::string AbsenceTextKey;
    std::string AbsenceRecoveryTextKey;
    /** Period order; empty on a day without school. */
    std::vector<SchoolDayRow> Rows;
    [[nodiscard]] bool Ok() const { return Issue.empty(); }
    [[nodiscard]] const SchoolDayRow* FindRow(std::string_view bindingId) const;
    friend bool operator==(const SchoolDayView&, const SchoolDayView&) = default;
};

/** Today's timetable. Pure: it reads, and never advances the clock or changes either state. */
[[nodiscard]] PERFECTWORLDCORE_API SchoolDayView BuildSchoolDayView(const SchoolLessonContent& content,
    const SchoolSessionsState& sessions, const DayLoopState& day, const SchoolSessionCatalogues& catalogues);

namespace ContentSchema
{
struct TypeSchema;
const TypeSchema& SchoolLessonSchema();
void ValidateSchoolLessonSemantics(const ContentDb& db, std::vector<ContentIssue>& issues);
}
} // namespace PerfectWorld
