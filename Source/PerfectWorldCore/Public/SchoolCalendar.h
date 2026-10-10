#pragma once

#include "ContentRegistry.h"
#include "GameClock.h"

#include <cstdint>
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
 * The 2042-43 school calendar (ROADMAP M3-09; SCHOOL_AND_PROGRESSION.md, GAME_PLAN.md
 * Phase 2 "the calendar is real", AWAKENING_AND_EXAM.md "the ten days").
 *
 * Content. One `calendar` record per school and stage (Data/Content/calendar) lists the
 * year window, the terms, the breaks between and inside them, the holidays and festival
 * closures, the assessment windows and the sports day, plus the Awakening and the
 * national examination. Every date is written "YYYY-MM-DD". Every entry carries a
 * `status` of "unverified" until a cultural reviewer clears it (ROADMAP M3-20). Fixed
 * dates (Independence Day) are documented; festival dates follow the lunisolar calendar
 * and are chosen approximations that say so in their note.
 *
 * Days. A calendar day is a whole number counted from the campaign start: day 0 is
 * Monday 2 June 2042, the same numbering AttendanceDay::Day uses
 * (GameClock::DayNumber(instant) - GameClock::DayNumber(0)). Ranges are inclusive at
 * both ends. Everything here is integers and pure functions of a calendar and a day.
 *
 * A day is a school day when it is inside the year window, falls on one of the
 * calendar's school weekdays, is not inside a holiday or a break, and is inside a term.
 * A holiday that falls on a weekend changes nothing for school; ClassifyCalendarDay
 * reports the weekend first, CalendarHolidayOn still names the holiday.
 *
 * Attendance integration (not done in this step; Attendance* is unchanged). Attendance
 * today treats every Monday to Friday as a school day through AttendanceIsSchoolDay(day)
 * and the AttendanceSchoolWeekdays mask. To use the calendar:
 *   1. Hold a `const SchoolCalendar*` next to the BellSchedule (found with
 *      SchoolCalendarSet::Find(schedule.InstitutionId, schedule.StageId)).
 *   2. Add an overload AttendanceIsSchoolDay(const SchoolCalendar* calendar, std::int64_t
 *      day) that returns CalendarIsSchoolDay(*calendar, day) when the pointer is set and
 *      the existing weekday rule when it is null; the day argument is already the same
 *      numbering, so no conversion is needed.
 *   3. Call that overload from the three places that use the weekday rule today:
 *      AttendanceCloseDays (so a holiday is never closed as an absence),
 *      AttendanceCheckCall (so recording on a holiday is refused with "that day is not a
 *      school day") and the routine's weekday mask (BuildSchoolRoutine keeps its
 *      Monday to Friday entries; the day loop skips them when CalendarIsSchoolDay is false).
 *   4. Start a fresh AttendanceRecord at the first day of the current term
 *      (CalendarTermOn(calendar, day)->First), since a record is one term.
 */

using CalendarDay = std::int64_t;

/** The Awakening is ten days before the examination (AWAKENING_AND_EXAM.md). */
constexpr std::int32_t CalendarAwakeningToExamDays = 10;

/** The entries every calendar must declare, by ID (the validator enforces them). */
inline constexpr std::string_view CalendarIndependenceDayId = "independence_day";
inline constexpr std::string_view CalendarDurgaPujaId = "durga_puja";
inline constexpr std::string_view CalendarKaliPujaDiwaliId = "kali_puja_diwali";
inline constexpr std::string_view CalendarSaraswatiPujaId = "saraswati_puja";
inline constexpr std::string_view CalendarWinterBreakId = "winter_break";
inline constexpr std::string_view CalendarSportsDayId = "sports_day";

/** A named inclusive range of days. */
struct CalendarSpan
{
    std::string Id;
    std::string Name;
    CalendarDay First = 0;
    CalendarDay Last = 0;
    /** True until a cultural reviewer has cleared the entry (the record's status is "unverified"). */
    bool Unverified = true;

    friend bool operator==(const CalendarSpan&, const CalendarSpan&) = default;
};

using CalendarTerm = CalendarSpan;
using CalendarBreak = CalendarSpan;

struct CalendarHoliday : CalendarSpan
{
    /** "national", "state" or "festival". */
    std::string Kind;
    /** True for a lunisolar festival date that is a chosen approximation. */
    bool Approximate = false;
    /** The record's own note, kept so the validator can require the approximation to be stated. */
    std::string Note;

    friend bool operator==(const CalendarHoliday&, const CalendarHoliday&) = default;
};

struct CalendarAssessment : CalendarSpan
{
    /** The term whose days contain this window. */
    std::string TermId;

    friend bool operator==(const CalendarAssessment&, const CalendarAssessment&) = default;
};

/** A single-day school event (First == Last); the school stays open. */
struct CalendarEvent : CalendarSpan
{
    /** "sports_day". */
    std::string Kind;

    friend bool operator==(const CalendarEvent&, const CalendarEvent&) = default;
};

struct SchoolCalendar
{
    std::string Id;
    std::string InstitutionId;
    std::string StageId;
    std::string State;
    /** The first and last day of the calendar's window. */
    CalendarDay FirstDay = 0;
    CalendarDay LastDay = 0;
    /** The Awakening Day (the last day of the latent-life year) and the national examination. */
    CalendarDay AwakeningDay = 0;
    CalendarDay ExaminationDay = 0;
    /** Bit n is set when Weekday n (Sunday is 0) can be a school day. */
    std::uint8_t SchoolWeekdays = 0;
    std::vector<CalendarTerm> Terms;
    std::vector<CalendarBreak> Breaks;
    std::vector<CalendarHoliday> Holidays;
    std::vector<CalendarAssessment> Assessments;
    std::vector<CalendarEvent> Events;
};

struct SchoolCalendarSet
{
    /** Sorted by Id. */
    std::vector<SchoolCalendar> Calendars;

    [[nodiscard]] PERFECTWORLDCORE_API const SchoolCalendar* FindById(std::string_view id) const noexcept;
    [[nodiscard]] PERFECTWORLDCORE_API const SchoolCalendar* Find(std::string_view institutionId, std::string_view stageId) const noexcept;
};

struct SchoolCalendarResult
{
    /** Empty on success; otherwise the record and what is wrong with it. */
    std::string Error;
    SchoolCalendarSet Set;

    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

// ---- Dates ---------------------------------------------------------------------

/** The calendar day for a civil date, or nothing for a date that does not exist. */
[[nodiscard]] PERFECTWORLDCORE_API std::optional<CalendarDay> CalendarDayFromDate(std::int32_t year, std::int32_t month, std::int32_t day);
/** Reads exactly "YYYY-MM-DD" (four digits, two, two) for a date that exists; anything else gives nothing. */
[[nodiscard]] PERFECTWORLDCORE_API std::optional<CalendarDay> ParseCalendarDate(std::string_view text);
/** "YYYY-MM-DD" for the day. */
[[nodiscard]] PERFECTWORLDCORE_API std::string FormatCalendarDate(CalendarDay day);
[[nodiscard]] PERFECTWORLDCORE_API CalendarDay CalendarDayOfInstant(GameInstant instant);
/** The instant of 00:00 that day. */
[[nodiscard]] PERFECTWORLDCORE_API GameInstant CalendarDayStartInstant(CalendarDay day);
[[nodiscard]] PERFECTWORLDCORE_API Weekday CalendarWeekdayOf(CalendarDay day);

// ---- Content -------------------------------------------------------------------

/**
 * Reads one calendar record. Anything that cannot be read (a date that is not
 * "YYYY-MM-DD" or does not exist, an unknown kind) is appended to `problems`, one
 * message each, and the field is left at its default; the result is only meaningful
 * when `problems` stays empty.
 */
[[nodiscard]] PERFECTWORLDCORE_API SchoolCalendar ParseSchoolCalendarRecord(const ContentRecord& record,
                                                                            std::vector<std::string>& problems);

/**
 * Every rule a parsed calendar must meet, empty when valid:
 *  - the window starts on the campaign start and is ordered; the Awakening is inside it and
 *    a school day; the examination is exactly ten days after the Awakening, inside the
 *    window, and no day from the Awakening to the examination (the ten days) is a school day;
 *  - school weekdays are declared; terms exist, are ordered, inside the window and do not overlap;
 *  - breaks are ordered, inside the window, do not overlap and each lies wholly inside one term
 *    or wholly between terms; every day of the window is in a term or a break;
 *  - holidays are ordered, inside the window, do not overlap and each lies wholly inside one term
 *    or one break (so a closure is always explained);
 *  - assessments are ordered, do not overlap, name an existing term, lie wholly inside it and
 *    contain at least one school day; events are single days and school days;
 *  - IDs are unique within a list; a lunisolar festival is marked approximate, unverified and says
 *    "approximate, to be confirmed" in its note;
 *  - the calendar declares Independence Day on 15 August 2042, Durga Puja, Kali Puja and Diwali,
 *    Saraswati Puja, a winter break and a sports day.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<std::string> ValidateSchoolCalendar(const SchoolCalendar& calendar);

/** Reads every calendar record and refuses (never coerces) one that cannot be read or fails ValidateSchoolCalendar. */
[[nodiscard]] PERFECTWORLDCORE_API SchoolCalendarResult LoadSchoolCalendars(const ContentDb& content);

// ---- Queries -------------------------------------------------------------------

enum class SchoolDayKind : std::uint8_t
{
    /** Before the first day or after the last day of the calendar. */
    OutsideYear,
    /** A weekday the school does not hold classes on (the weekend). */
    NonSchoolWeekday,
    Holiday,
    Break,
    /** Inside the window but in no term (a gap; a valid calendar has none). */
    OutsideTerm,
    SchoolDay,
};

[[nodiscard]] PERFECTWORLDCORE_API std::string_view SchoolDayKindName(SchoolDayKind kind) noexcept;

/** Whether the school can hold classes on this weekday (the calendar's school weekdays). */
[[nodiscard]] PERFECTWORLDCORE_API bool CalendarWeekdayIsSchoolDay(const SchoolCalendar& calendar, Weekday weekday);

/** The entry that contains the day, or nullptr. The pointers stay valid while the calendar is not changed or destroyed. */
[[nodiscard]] PERFECTWORLDCORE_API const CalendarTerm* CalendarTermOn(const SchoolCalendar& calendar, CalendarDay day);
[[nodiscard]] PERFECTWORLDCORE_API const CalendarBreak* CalendarBreakOn(const SchoolCalendar& calendar, CalendarDay day);
[[nodiscard]] PERFECTWORLDCORE_API const CalendarHoliday* CalendarHolidayOn(const SchoolCalendar& calendar, CalendarDay day);
[[nodiscard]] PERFECTWORLDCORE_API const CalendarAssessment* CalendarAssessmentOn(const SchoolCalendar& calendar, CalendarDay day);
[[nodiscard]] PERFECTWORLDCORE_API const CalendarEvent* CalendarEventOn(const SchoolCalendar& calendar, CalendarDay day);

/** The day's status, checked in the order the enumerators are listed. */
[[nodiscard]] PERFECTWORLDCORE_API SchoolDayKind ClassifyCalendarDay(const SchoolCalendar& calendar, CalendarDay day);

/** True for SchoolDayKind::SchoolDay. This is the attendance hook (see the integration notes above). */
[[nodiscard]] PERFECTWORLDCORE_API bool CalendarIsSchoolDay(const SchoolCalendar& calendar, CalendarDay day);
/** The same for the day containing the instant. */
[[nodiscard]] PERFECTWORLDCORE_API bool CalendarIsSchoolDayAt(const SchoolCalendar& calendar, GameInstant instant);

/** The first school day after the given day, or nothing when the calendar has none left. */
[[nodiscard]] PERFECTWORLDCORE_API std::optional<CalendarDay> CalendarNextSchoolDay(const SchoolCalendar& calendar, CalendarDay day);
/** The given day if it is a school day, else the next one. */
[[nodiscard]] PERFECTWORLDCORE_API std::optional<CalendarDay> CalendarSchoolDayOnOrAfter(const SchoolCalendar& calendar, CalendarDay day);
/** The last school day before the given day, or nothing. */
[[nodiscard]] PERFECTWORLDCORE_API std::optional<CalendarDay> CalendarPreviousSchoolDay(const SchoolCalendar& calendar, CalendarDay day);
/** School days in first..last inclusive; 0 for an inverted range. */
[[nodiscard]] PERFECTWORLDCORE_API std::int64_t CalendarCountSchoolDays(const SchoolCalendar& calendar, CalendarDay first, CalendarDay last);
} // namespace PerfectWorld
