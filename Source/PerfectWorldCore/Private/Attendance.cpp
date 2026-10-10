#include "Attendance.h"

#include "AttendanceInternal.h"

#include <algorithm>
#include <bit>

// The attendance record: lateness, the running totals and their thresholds
// (ROADMAP M2-07). The rules are documented in Attendance.h.
namespace PerfectWorld
{
namespace
{
constexpr std::int32_t AttendanceNoArrival = static_cast<std::int32_t>(MinutesPerDay);

/** Days since the campaign's first day: GameClock::DayNumber counts from 1970, so subtract the epoch's. Day 0 is the Monday. */
std::int64_t AttendanceCampaignDay(GameInstant instant)
{
    return GameClock::DayNumber(instant) - GameClock::DayNumber(0);
}

AttendanceDay AttendanceAbsentDay(std::int64_t day, std::int32_t periods)
{
    AttendanceDay entry;
    entry.Day = day;
    entry.Outcome = AttendanceOutcome::Absent;
    entry.ArrivalMinute = AttendanceNoArrival;
    entry.PeriodsHeld = periods;
    entry.PeriodMask = 0;
    return entry;
}

void AttendanceApply(AttendanceRecord& record, const AttendanceDay& day)
{
    ++record.SchoolDays;
    switch (day.Outcome)
    {
    case AttendanceOutcome::OnTime: ++record.OnTimeDays; break;
    case AttendanceOutcome::Late: ++record.LateDays; break;
    case AttendanceOutcome::VeryLate: ++record.VeryLateDays; break;
    case AttendanceOutcome::Absent: ++record.AbsentDays; break;
    }
    record.PeriodsHeld += day.PeriodsHeld;
    record.PeriodsAttended += std::popcount(day.PeriodMask);
    record.Days.push_back(day);
    if (record.Days.size() > AttendanceKeptDays)
        record.Days.erase(record.Days.begin());
    record.NextDay = day.Day + 1;
}

AttendanceResult AttendanceRefuse(AttendanceResult result, std::string why)
{
    result.Ok = false;
    result.Error = std::move(why);
    return result;
}

/** Closes the school days in [record.NextDay, upTo) as absent. Returns how many. */
std::int32_t AttendanceCloseDays(AttendanceRecord& record, std::int64_t upTo, std::int32_t periods)
{
    std::int32_t closed = 0;
    for (std::int64_t day = record.NextDay; day < upTo; ++day)
    {
        if (!AttendanceIsSchoolDay(day))
            continue;
        AttendanceApply(record, AttendanceAbsentDay(day, periods));
        ++closed;
    }
    record.NextDay = std::max(record.NextDay, upTo);
    return closed;
}

/** Checks everything a record call needs and returns the day, or an error in `result`. */
std::optional<std::int64_t> AttendanceCheckCall(const AttendanceRecord& record, const BellSchedule& schedule, GameInstant instant,
                                                AttendanceResult& result)
{
    if (!ValidateBellSchedule(schedule).empty())
    {
        result.Error = "the bell schedule is not valid";
        return std::nullopt;
    }
    if (instant < 0 || instant > ScheduleMaxInstant)
    {
        result.Error = "the instant is outside the clock";
        return std::nullopt;
    }
    const std::int64_t day = AttendanceCampaignDay(instant);
    if (day < record.NextDay)
    {
        result.Error = "that day is already recorded";
        return std::nullopt;
    }
    if (day - record.NextDay > AttendanceMaxCatchUpDays)
    {
        result.Error = "the record is more than a year behind; start a new record";
        return std::nullopt;
    }
    return day;
}

void AttendanceFinish(AttendanceResult& result, const AttendanceRecord& before, const AttendanceRecord& after)
{
    result.StandingBefore = AttendanceStandingOf(before);
    result.StandingAfter = AttendanceStandingOf(after);
    const std::int64_t marksBefore = AttendanceLateMarks(before);
    const std::int64_t marksAfter = AttendanceLateMarks(after);
    result.LateWarningIssued = marksAfter / AttendanceLateMarksPerWarning > marksBefore / AttendanceLateMarksPerWarning;
    result.GuardianNoticeIssued = marksBefore < AttendanceGuardianNoticeLateMarks && marksAfter >= AttendanceGuardianNoticeLateMarks;
    result.Ok = true;
}
} // namespace

namespace AttendanceInternal
{
std::string_view OutcomeName(AttendanceOutcome outcome)
{
    switch (outcome)
    {
    case AttendanceOutcome::OnTime: return "ontime";
    case AttendanceOutcome::Late: return "late";
    case AttendanceOutcome::VeryLate: return "verylate";
    case AttendanceOutcome::Absent: return "absent";
    }
    return "absent";
}

std::optional<AttendanceOutcome> OutcomeFromName(std::string_view name)
{
    for (const AttendanceOutcome outcome : {AttendanceOutcome::OnTime, AttendanceOutcome::Late, AttendanceOutcome::VeryLate, AttendanceOutcome::Absent})
    {
        if (OutcomeName(outcome) == name)
            return outcome;
    }
    return std::nullopt;
}
} // namespace AttendanceInternal

bool AttendanceIsSchoolDay(std::int64_t day)
{
    const unsigned weekday = AttendanceInternal::WeekdayOfDay(day);
    return (AttendanceSchoolWeekdays >> weekday & 1u) != 0;
}

AttendanceForecast ForecastAttendance(const BellSchedule& schedule, std::int32_t arrival, std::int32_t departure)
{
    AttendanceForecast forecast;
    forecast.PeriodsHeld = BellPeriodCount(schedule);
    const std::int32_t bell = BellFirstBell(schedule);
    forecast.MinutesAfterBell = arrival > bell ? arrival - bell : 0;
    std::int32_t index = 0;
    for (const BellBlock& block : schedule.Blocks)
    {
        if (block.Kind != BellBlockKind::Period)
            continue;
        if (arrival <= block.StartMinute + AttendancePeriodGraceMinutes && departure >= block.EndMinute)
            forecast.PeriodMask |= 1u << index;
        ++index;
    }
    forecast.PeriodsAttended = std::popcount(forecast.PeriodMask);
    if (forecast.PeriodsAttended == 0)
    {
        forecast.Outcome = AttendanceOutcome::Absent;
        forecast.MissesAssembly = true;
        return forecast;
    }
    if (arrival <= bell)
        forecast.Outcome = AttendanceOutcome::OnTime;
    else if (arrival <= bell + AttendanceBellGraceMinutes)
        forecast.Outcome = AttendanceOutcome::Late;
    else
        forecast.Outcome = AttendanceOutcome::VeryLate;
    forecast.AddsLateMark = forecast.Outcome == AttendanceOutcome::Late || forecast.Outcome == AttendanceOutcome::VeryLate;
    forecast.MissesAssembly = forecast.Outcome == AttendanceOutcome::VeryLate;
    return forecast;
}

AttendanceRecord StartAttendanceRecord(std::int64_t firstDay)
{
    AttendanceRecord record;
    record.FirstDay = firstDay;
    record.NextDay = firstDay;
    return record;
}

std::int64_t AttendanceLateMarks(const AttendanceRecord& record)
{
    return record.LateDays + record.VeryLateDays;
}

std::int64_t AttendanceLateWarnings(const AttendanceRecord& record)
{
    return AttendanceLateMarks(record) / AttendanceLateMarksPerWarning;
}

bool AttendanceGuardianNotified(const AttendanceRecord& record)
{
    return AttendanceLateMarks(record) >= AttendanceGuardianNoticeLateMarks;
}

std::int32_t AttendancePermille(const AttendanceRecord& record)
{
    if (record.PeriodsHeld <= 0)
        return 1000;
    return static_cast<std::int32_t>(record.PeriodsAttended * 1000 / record.PeriodsHeld);
}

AttendanceStanding AttendanceStandingOf(const AttendanceRecord& record)
{
    const std::int32_t permille = AttendancePermille(record);
    if (permille < AttendanceEligiblePermille)
        return AttendanceStanding::AtRisk;
    if (permille < AttendanceWarnPermille)
        return AttendanceStanding::Warned;
    return AttendanceStanding::Good;
}

AttendanceResult RecordAttendanceArrival(AttendanceRecord& record, const BellSchedule& schedule, GameInstant arrival,
                                         std::int32_t departure)
{
    AttendanceResult result;
    const std::optional<std::int64_t> day = AttendanceCheckCall(record, schedule, arrival, result);
    if (!day)
        return result;
    if (!AttendanceIsSchoolDay(*day))
        return AttendanceRefuse(result, "that day is not a school day");
    if (departure < 0 || departure > AttendanceStaysUntilDismissal)
        return AttendanceRefuse(result, "the departure minute is outside the day");

    const std::int32_t minute = static_cast<std::int32_t>(arrival - GameClock::StartOfDay(arrival));
    const AttendanceForecast forecast = ForecastAttendance(schedule, minute, departure);
    AttendanceRecord work = record;
    result.DaysClosedAbsent = AttendanceCloseDays(work, *day, forecast.PeriodsHeld);
    AttendanceDay entry;
    entry.Day = *day;
    entry.Outcome = forecast.Outcome;
    entry.ArrivalMinute = forecast.Outcome == AttendanceOutcome::Absent ? AttendanceNoArrival : minute;
    entry.PeriodsHeld = forecast.PeriodsHeld;
    entry.PeriodMask = forecast.PeriodMask;
    AttendanceApply(work, entry);
    result.Day = entry;
    AttendanceFinish(result, record, work);
    record = std::move(work);
    return result;
}

AttendanceResult RecordAttendanceAbsence(AttendanceRecord& record, const BellSchedule& schedule, GameInstant dayInstant)
{
    AttendanceResult result;
    const std::optional<std::int64_t> day = AttendanceCheckCall(record, schedule, dayInstant, result);
    if (!day)
        return result;
    if (!AttendanceIsSchoolDay(*day))
        return AttendanceRefuse(result, "that day is not a school day");
    const std::int32_t periods = BellPeriodCount(schedule);
    AttendanceRecord work = record;
    result.DaysClosedAbsent = AttendanceCloseDays(work, *day, periods);
    result.Day = AttendanceAbsentDay(*day, periods);
    AttendanceApply(work, result.Day);
    AttendanceFinish(result, record, work);
    record = std::move(work);
    return result;
}

AttendanceResult CloseAttendanceBefore(AttendanceRecord& record, const BellSchedule& schedule, GameInstant now)
{
    AttendanceResult result;
    if (!ValidateBellSchedule(schedule).empty())
        return AttendanceRefuse(result, "the bell schedule is not valid");
    if (now < 0 || now > ScheduleMaxInstant)
        return AttendanceRefuse(result, "the instant is outside the clock");
    const std::int64_t today = AttendanceCampaignDay(now);
    if (today - record.NextDay > AttendanceMaxCatchUpDays)
        return AttendanceRefuse(result, "the record is more than a year behind; start a new record");
    AttendanceRecord work = record;
    result.DaysClosedAbsent = AttendanceCloseDays(work, today, BellPeriodCount(schedule));
    AttendanceFinish(result, record, work);
    record = std::move(work);
    return result;
}
} // namespace PerfectWorld
