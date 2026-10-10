#pragma once

// Private helpers shared by AttendanceBell.cpp, Attendance.cpp, AttendanceText.cpp and
// ContentScheduleSchemas.cpp (ROADMAP M2-07). Not part of the public API.

#include "Attendance.h"

#include <string>

namespace PerfectWorld::AttendanceInternal
{
/** Reads one bell_schedule record that already passed the schema (shape) checks. Missing parts stay empty or zero. */
BellSchedule ParseBellScheduleRecord(const ContentRecord& record);

/** Minutes of the day as "HH:MM", for messages. */
std::string ClockText(std::int32_t minuteOfDay);

/** 0 = Sunday .. 6 = Saturday for a GameClock day number, matching Weekday. */
unsigned WeekdayOfDay(std::int64_t day);

/** The outcome as a lowercase word ("ontime", "late", "verylate", "absent") and back. */
std::string_view OutcomeName(AttendanceOutcome outcome);
std::optional<AttendanceOutcome> OutcomeFromName(std::string_view name);
} // namespace PerfectWorld::AttendanceInternal
