#pragma once

#include "SchoolSessions.h"

namespace PerfectWorld
{
/** Persisted planner types separated from SchoolWeek.h so WorldState can own them
 * without including WeekCompression's WorldState-valued results recursively. */
inline constexpr std::uint32_t SchoolWeekContractVersion = 1;
struct SchoolStandingOrder
{
    std::string Id;
    /** Bit n = Weekday n. Bells and calendar remain the school-time authority. */
    std::uint8_t Weekdays = AttendanceSchoolWeekdays;
    bool AttendSchool = true;
    SchoolLessonMode LessonMode = SchoolLessonMode::Attend;
};
struct SchoolWeekState
{
    std::vector<SchoolStandingOrder> Orders;
    /** Absolute campaign days, sorted unique. */
    std::vector<std::int64_t> SceneDays;
};
} // namespace PerfectWorld
