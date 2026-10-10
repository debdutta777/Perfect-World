#pragma once

#include "ContentRegistry.h"
#include "GameClock.h"
#include "Schedules.h"
#include "SchoolCalendar.h"

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
 * The Nabapara Vidyalaya bell schedule, attendance and lateness (ROADMAP M2-07;
 * SCHOOL_AND_PROGRESSION.md "Why attend rather than skip": consequences must be
 * visible before the choice; INDIA_WORLD_BIBLE.md "School bible": assembly, a
 * stream-specific period, an after-school choice; SYSTEMS_CATALOGUE.md "School
 * and education": Timetable, Attendance map<Day, PeriodMask>).
 *
 * Content. One `bell_schedule` record per stream (Data/Content/bell_schedule)
 * describes the one weekday that repeats Monday to Friday: the bells, then an
 * ordered list of blocks (the assembly, 45-minute periods, a tiffin break),
 * each with a room, and each period with a subject. The content validator and
 * ValidateBellSchedule enforce the same rules, listed with ValidateBellSchedule.
 * The docs fix the shape (assembly, class periods, streams) but no numbers, so
 * every number below is a chosen production assumption, not a claim about a real
 * West Bengal school (INDIA_WORLD_BIBLE.md says so too). The subject lists per
 * stream are WBCHSE-style and need the consultant check that document asks for.
 *
 * One calendar. BuildSchoolRoutine turns a bell schedule into M2-06 routine
 * entries (zone = room, activity = activity.attend_class for a period, a role for
 * the assembly and the break), so NPC classmates and the player share one
 * Schedules calendar. NPCs are never late: M2-06 resolves a gap that is too short
 * by shortening the lower-priority stay (the NPC leaves home early; the school
 * entry has the higher priority), so an NPC's arrival is exactly on the bell.
 * Only the player's recorded arrival can be late, because only the player's
 * choices (an oversleep exception, a detour) move it.
 *
 * Lateness (chosen; the docs say only that arriving after the bell marks the
 * student late). The bell is the first bell of the day, the start of assembly.
 * `arrival` is the minute of day the student is first at any school zone (the
 * gate counts). A period counts as attended when the student arrived no later
 * than AttendancePeriodGraceMinutes after its start and stays to its end.
 *   - at or before the bell ............ OnTime: nothing.
 *   - 1..AttendanceBellGraceMinutes after  Late: one late mark; every period is
 *                                           still attended.
 *   - more than the grace after ........ VeryLate: one late mark, the assembly is
 *                                           missed, and each period that began
 *                                           more than the period grace before the
 *                                           arrival is missed.
 *   - no period attended at all ........ Absent: no late mark; the periods count
 *                                           as held and not attended.
 * Exactly at the bell is on time; one minute after is late; one minute after the
 * grace is very late. ForecastAttendance gives the same answer without recording
 * it, so the consequence can be shown before the choice.
 *
 * The running record. AttendanceRecord keeps exact totals (school days by
 * outcome, periods held and attended) plus the last AttendanceKeptDays daily
 * entries (the catalogue's `Attendance map<Day, PeriodMask>`). Every Monday to
 * Friday is a school day unless a SchoolCalendar is passed (see "The calendar"
 * below). Recording
 * a day first closes any earlier unrecorded school days as absent, so a long
 * sleep or a skipped week has a deterministic effect. Thresholds, all chosen:
 *   - every AttendanceLateMarksPerWarning (3) late marks the teacher issues a
 *     warning; at AttendanceGuardianNoticeLateMarks (6) the guardian is told;
 *   - period attendance below AttendanceWarnPermille (85.0%) is Warned and below
 *     AttendanceEligiblePermille (75.0%) is AtRisk, the point at which the
 *     student would not be allowed to sit the board examinations. A record is
 *     one term; the integration starts a fresh one when a term begins.
 *
 * The calendar (ROADMAP M3-09 integration; SchoolCalendar.h). RecordAttendanceArrival,
 * RecordAttendanceAbsence and CloseAttendanceBefore take a trailing optional
 * `const SchoolCalendar*`, null by default. Null is exactly the Monday to Friday
 * rule above. When it is set, a day is a school day only when CalendarIsSchoolDay
 * says so, so a holiday, a break, a weekend, a gap between terms and any day outside
 * the calendar's year are never closed as absences, and recording an arrival or an
 * absence on one is refused with the code "attendance.not_school_day" (the
 * reason, such as the holiday's name, follows in the message). The record's
 * text form is unchanged: every day it holds is still a Monday to Friday school
 * day, and a calendar only ever removes days from that set. Starting a fresh
 * record at the first day of each term is the caller's job.
 *
 * Standalone, integer-only plain data plus free functions. Not part of
 * WorldState, the reducer, the hash or the save; to integrate, store the record
 * and call RecordAttendanceArrival from the day loop (M2-12).
 */

/** Serialised-format version of the attendance record text. */
constexpr std::uint32_t AttendanceFormatVersion = 1;

/** The school day sits inside 09:00 to 17:00 and lasts five to eight hours from first bell to last. */
constexpr std::int32_t AttendanceDayEarliestMinute = 9 * 60;
constexpr std::int32_t AttendanceDayLatestMinute = 17 * 60;
constexpr std::int32_t AttendanceMinDaySpanMinutes = 5 * 60;
constexpr std::int32_t AttendanceMaxDaySpanMinutes = 8 * 60;
/** A full day: six to eight periods, an assembly first, a break of at least twenty minutes, a period last. */
constexpr std::int32_t AttendanceMinPeriods = 6;
constexpr std::int32_t AttendanceMaxPeriods = 8;
constexpr std::int32_t AttendanceMinBreakMinutes = 20;
/** Lengths: a period is 30..60 minutes (the content uses 45, as activity.attend_class does); the assembly 10..30. */
constexpr std::int32_t AttendanceMinPeriodMinutes = 30;
constexpr std::int32_t AttendanceMaxPeriodMinutes = 60;
constexpr std::int32_t AttendanceMinAssemblyMinutes = 10;
constexpr std::int32_t AttendanceMaxAssemblyMinutes = 30;
/** Between blocks there is a passing gap of at most this long; moving to another room needs AttendanceRoomWalkMinutes. */
constexpr std::int32_t AttendanceMaxPassingMinutes = 15;
constexpr std::int32_t AttendanceRoomWalkMinutes = 3;

/** Lateness (see the comment above). */
constexpr std::int32_t AttendanceBellGraceMinutes = 5;
constexpr std::int32_t AttendancePeriodGraceMinutes = 5;
constexpr std::int32_t AttendanceLateMarksPerWarning = 3;
constexpr std::int32_t AttendanceGuardianNoticeLateMarks = 6;
constexpr std::int32_t AttendanceWarnPermille = 850;
constexpr std::int32_t AttendanceEligiblePermille = 750;
/** Passing `departure` this large means the student stays to dismissal. */
constexpr std::int32_t AttendanceStaysUntilDismissal = static_cast<std::int32_t>(MinutesPerDay);

/** Daily entries the record keeps; older days stay counted in the totals. */
constexpr std::size_t AttendanceKeptDays = 400;
/** A record call closes at most this many earlier unrecorded days; further behind is refused. */
constexpr std::int64_t AttendanceMaxCatchUpDays = 366;

/** Routine entries are Monday to Friday at this priority (above home and sleep, below an appointment). */
constexpr std::int32_t AttendanceSchoolPriority = 600;
constexpr std::uint8_t AttendanceSchoolWeekdays = 0x3E;

inline constexpr std::string_view AttendanceClassActivityId = "activity.attend_class";
inline constexpr std::string_view AttendanceAssemblyRole = "assembly";
inline constexpr std::string_view AttendanceBreakRole = "tiffin";

enum class BellBlockKind : std::uint8_t
{
    Assembly,
    Period,
    Break,
};

/** One stretch of the school day. Times are minutes of the day; End is exclusive. */
struct BellBlock
{
    BellBlockKind Kind = BellBlockKind::Period;
    std::int32_t StartMinute = 0;
    std::int32_t EndMinute = 0;
    /** A lowercase-token zone, the M2-06 zone of everyone in this block. */
    std::string Room;
    /** A period's subject token ("physics"); empty for the assembly and the break. */
    std::string Subject;

    friend bool operator==(const BellBlock&, const BellBlock&) = default;
};

/** One stream's weekday at one school and stage. */
struct BellSchedule
{
    std::string Id;
    std::string InstitutionId;
    std::string StageId;
    /** "science", "commerce" or "humanities". */
    std::string Stream;
    /** The gate opens; arriving earlier changes nothing. */
    std::int32_t GatesOpenMinute = 0;
    /** Every minute a bell rings, ascending: exactly the start and end of each block. */
    std::vector<std::int32_t> Bells;
    std::vector<BellBlock> Blocks;

    friend bool operator==(const BellSchedule&, const BellSchedule&) = default;
};

struct BellScheduleSet
{
    /** Sorted by Id. */
    std::vector<BellSchedule> Schedules;

    [[nodiscard]] PERFECTWORLDCORE_API const BellSchedule* FindById(std::string_view id) const noexcept;
    [[nodiscard]] PERFECTWORLDCORE_API const BellSchedule* Find(std::string_view institutionId, std::string_view stream) const noexcept;
};

struct BellScheduleResult
{
    /** Empty on success; otherwise the record and what is wrong with it. */
    std::string Error;
    BellScheduleSet Set;

    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

[[nodiscard]] PERFECTWORLDCORE_API bool AttendanceIsStream(std::string_view stream);
/** Whether the subject token is on the stream's list (the shared subjects, then the stream's own). */
[[nodiscard]] PERFECTWORLDCORE_API bool AttendanceSubjectTaught(std::string_view stream, std::string_view subject);

/**
 * Every problem with one schedule, empty when valid. The rules: a known stream;
 * blocks that are well-formed, in order and do not overlap; passing gaps of at
 * most 15 minutes and at least 3 when the room changes; an assembly first (and
 * only once), a period last, 6..8 periods, a break of at least 20 minutes;
 * period and assembly lengths in range; each period's subject known and taught
 * in the stream, english and bengali each at least once, no subject elsewhere;
 * the day inside 09:00..17:00 and 300..480 minutes from first bell to last; the
 * gate opening no earlier than 09:00 and before the first bell; bells strictly
 * ascending and exactly the block boundaries.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<std::string> ValidateBellSchedule(const BellSchedule& schedule);

/** Reads every bell_schedule record and refuses (never coerces) one that ValidateBellSchedule rejects. */
[[nodiscard]] PERFECTWORLDCORE_API BellScheduleResult LoadBellSchedules(const ContentDb& content);

/** The number of period blocks (the length of a day's attendance mask). */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t BellPeriodCount(const BellSchedule& schedule);
/** The minute of the first bell, the start of assembly: arriving after it is late. 0 for an empty schedule. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t BellFirstBell(const BellSchedule& schedule);
/** The block containing the minute of day, or nullptr in a gap or outside the day. */
[[nodiscard]] PERFECTWORLDCORE_API const BellBlock* BellBlockAt(const BellSchedule& schedule, std::int32_t minuteOfDay);
/** The zero-based period number of a period block of this schedule, or -1. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t BellPeriodIndex(const BellSchedule& schedule, const BellBlock& block);

struct AttendanceRoutineOptions
{
    std::int32_t Priority = AttendanceSchoolPriority;
    std::uint8_t Weekdays = AttendanceSchoolWeekdays;
};

/** One M2-06 routine entry per block, in order. */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<ScheduleEntry> BuildSchoolRoutine(const BellSchedule& schedule,
                                                                                  const AttendanceRoutineOptions& options = {});
/** A person whose routine is the school day alone; the caller adds home, sleep and other entries. */
[[nodiscard]] PERFECTWORLDCORE_API PersonSchedule BuildStudentSchedule(std::string_view personId, std::string_view homeZone,
                                                                        const BellSchedule& schedule,
                                                                        const AttendanceRoutineOptions& options = {});
/** Walking legs of AttendanceRoomWalkMinutes between every two distinct rooms of the whole set, in canonical order. */
[[nodiscard]] PERFECTWORLDCORE_API ScheduleTravelTable AttendanceSchoolTravel(const BellScheduleSet& set);
/** Adds or replaces one leg, keeping the table canonical. False (table untouched) for a leg ValidateScheduleTravel would refuse. */
PERFECTWORLDCORE_API bool AttendanceAddTravelLeg(ScheduleTravelTable& table, std::string_view from, std::string_view to,
                                                 std::int32_t minutes);

enum class AttendanceOutcome : std::uint8_t
{
    OnTime,
    Late,
    VeryLate,
    Absent,
};

/** What a given arrival and departure would do, without recording anything. */
struct AttendanceForecast
{
    AttendanceOutcome Outcome = AttendanceOutcome::Absent;
    /** Bit n is set when period n (zero-based) is attended. */
    std::uint32_t PeriodMask = 0;
    std::int32_t PeriodsHeld = 0;
    std::int32_t PeriodsAttended = 0;
    bool AddsLateMark = false;
    bool MissesAssembly = false;
    /** Minutes after the first bell, 0 when not after it. */
    std::int32_t MinutesAfterBell = 0;
};

/** Pure. `arrival` is a minute of day; `departure` is the minute the student leaves (AttendanceStaysUntilDismissal to stay). */
[[nodiscard]] PERFECTWORLDCORE_API AttendanceForecast ForecastAttendance(const BellSchedule& schedule, std::int32_t arrival,
                                                                          std::int32_t departure = AttendanceStaysUntilDismissal);

/** One school day in the record. Absent days have ArrivalMinute = MinutesPerDay. */
struct AttendanceDay
{
    /** Days since the campaign start (GameClock::DayNumber(instant) - GameClock::DayNumber(0)); day 0 is the first Monday. */
    std::int64_t Day = 0;
    AttendanceOutcome Outcome = AttendanceOutcome::Absent;
    std::int32_t ArrivalMinute = static_cast<std::int32_t>(MinutesPerDay);
    std::int32_t PeriodsHeld = 0;
    std::uint32_t PeriodMask = 0;

    friend bool operator==(const AttendanceDay&, const AttendanceDay&) = default;
};

/** The student's running record for one term. */
struct AttendanceRecord
{
    /** The first day the record covers. */
    std::int64_t FirstDay = 0;
    /** Every school day before this is accounted for; the next recorded day must be at least this. */
    std::int64_t NextDay = 0;
    std::int64_t SchoolDays = 0;
    std::int64_t OnTimeDays = 0;
    std::int64_t LateDays = 0;
    std::int64_t VeryLateDays = 0;
    std::int64_t AbsentDays = 0;
    std::int64_t PeriodsHeld = 0;
    std::int64_t PeriodsAttended = 0;
    /** The last AttendanceKeptDays school days, ascending. */
    std::vector<AttendanceDay> Days;

    friend bool operator==(const AttendanceRecord&, const AttendanceRecord&) = default;
};

enum class AttendanceStanding : std::uint8_t
{
    Good,
    Warned,
    AtRisk,
};

/** A fresh record that starts at the given day. */
[[nodiscard]] PERFECTWORLDCORE_API AttendanceRecord StartAttendanceRecord(std::int64_t firstDay);

/** Late marks are the late and very late days. */
[[nodiscard]] PERFECTWORLDCORE_API std::int64_t AttendanceLateMarks(const AttendanceRecord& record);
/** Late warnings issued so far: one per AttendanceLateMarksPerWarning marks. */
[[nodiscard]] PERFECTWORLDCORE_API std::int64_t AttendanceLateWarnings(const AttendanceRecord& record);
[[nodiscard]] PERFECTWORLDCORE_API bool AttendanceGuardianNotified(const AttendanceRecord& record);
/** Periods attended per thousand held, rounded down; 1000 when nothing has been held. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t AttendancePermille(const AttendanceRecord& record);
[[nodiscard]] PERFECTWORLDCORE_API AttendanceStanding AttendanceStandingOf(const AttendanceRecord& record);
/** Whether the day number is a Monday to Friday. */
[[nodiscard]] PERFECTWORLDCORE_API bool AttendanceIsSchoolDay(std::int64_t day);
/** The calendar's answer (CalendarIsSchoolDay) when `calendar` is set, else the Monday to Friday rule above. */
[[nodiscard]] PERFECTWORLDCORE_API bool AttendanceIsSchoolDay(const SchoolCalendar* calendar, std::int64_t day);

/** The stable code of a refusal because the day is not a school day. */
inline constexpr std::string_view AttendanceNotSchoolDayCode = "attendance.not_school_day";

struct AttendanceResult
{
    bool Ok = false;
    /** Why a call was refused (the record is untouched); empty when Ok. */
    std::string Error;
    /** A stable code for the refusals that have one (AttendanceNotSchoolDayCode); empty otherwise and when Ok. */
    std::string Code;
    /** The day just recorded. */
    AttendanceDay Day;
    /** Earlier unrecorded school days closed as absent by this call. */
    std::int32_t DaysClosedAbsent = 0;
    bool LateWarningIssued = false;
    bool GuardianNoticeIssued = false;
    AttendanceStanding StandingBefore = AttendanceStanding::Good;
    AttendanceStanding StandingAfter = AttendanceStanding::Good;
};

/**
 * Records the day containing `arrival`. `arrival` is the instant the student is
 * first at a school zone; `departure` the minute of that day they leave. Refused
 * (record untouched) for an invalid schedule, a weekend, a day already recorded,
 * an instant outside the clock, or a record more than AttendanceMaxCatchUpDays behind.
 * With a `calendar`, "a weekend" widens to any day that is not one of its school days.
 */
PERFECTWORLDCORE_API AttendanceResult RecordAttendanceArrival(AttendanceRecord& record, const BellSchedule& schedule,
                                                              GameInstant arrival,
                                                              std::int32_t departure = AttendanceStaysUntilDismissal,
                                                              const SchoolCalendar* calendar = nullptr);
/** Records the day containing the instant as absent (the student did not come). Same refusals. */
PERFECTWORLDCORE_API AttendanceResult RecordAttendanceAbsence(AttendanceRecord& record, const BellSchedule& schedule,
                                                              GameInstant dayInstant, const SchoolCalendar* calendar = nullptr);
/**
 * Closes every unrecorded school day before the day of `now` as absent (the record is then current to yesterday).
 * With a `calendar` only its school days are closed.
 */
PERFECTWORLDCORE_API AttendanceResult CloseAttendanceBefore(AttendanceRecord& record, const BellSchedule& schedule, GameInstant now,
                                                            const SchoolCalendar* calendar = nullptr);

/** "" when the record is internally consistent (totals equal the entries, counts and orders valid), else the first problem. */
[[nodiscard]] PERFECTWORLDCORE_API std::string ValidateAttendanceRecord(const AttendanceRecord& record);
[[nodiscard]] PERFECTWORLDCORE_API std::uint64_t HashAttendanceRecord(const AttendanceRecord& record);

/**
 * Text form: one line per record, plain decimal integers, lowercase tokens and
 * a closing "end <hash>" line. The same value always gives the same bytes. The
 * parser is a strict inverse: nothing for truncation at any byte, unknown or
 * reordered fields, a bad count, an inconsistent record, or a wrong hash.
 *
 *   pw.attendance 1
 *   range <firstDay> <nextDay>
 *   totals <schoolDays> <onTime> <late> <veryLate> <absent> <periodsHeld> <periodsAttended>
 *   days <n>
 *   day <day> <ontime|late|verylate|absent> <arrivalMinute> <periodsHeld> <periodMask>
 *   ...
 *   end <hash>
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseAttendanceRecord(const AttendanceRecord& record);
[[nodiscard]] PERFECTWORLDCORE_API std::optional<AttendanceRecord> ParseAttendanceRecord(std::string_view text);
} // namespace PerfectWorld
