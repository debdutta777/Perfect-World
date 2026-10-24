#pragma once

#include "FaridaSchedule.h"

#include <string_view>

namespace PerfectWorld
{
/**
 * Generic loader for authored npc_schedule content (Farida, the care worker and Anindita).
 *
 * It reuses the Farida shape and portable routine/travel validation (ContentSchema::ValidateNpcScheduleShape,
 * ValidatePersonSchedule, ValidateNpcSchedule, ValidateScheduleTravel, ValidateScheduleTravelCoverage,
 * ValidateScheduleRoutineTravel, ResolveSchedule) and adds, per schedule, a frozen identity, workplace, door, the approved
 * route table and the hours the person may be at work. Farida's own schedule is delegated to her loader unchanged.
 * The decoded types are Farida's: a person, a travel table, the routes and the explicit route path of every leg.
 */
using NpcSchedule = FaridaSchedule;
using NpcScheduleResult = FaridaScheduleResult;

constexpr std::string_view CareWorkerScheduleId = "npc_schedule.care_worker";
constexpr std::string_view CareWorkerPersonId = "person.care_worker";
constexpr std::string_view CareWorkerHomeZone = "location.nabapara.care_worker_home";
/** On duty from 07:00 to 15:00 (minutes of the day, end exclusive). */
constexpr std::int32_t CareWorkerDutyStartMinute = 7 * 60;
constexpr std::int32_t CareWorkerDutyEndMinute = 15 * 60;

/** Strict typed decoding of a frozen Farida, care-worker or Anindita schedule. Any other ID is refused. */
[[nodiscard]] PERFECTWORLDCORE_API NpcScheduleResult LoadNpcSchedule(const ContentDb& db, std::string_view scheduleId);
/** Portable validation of a decoded schedule against the frozen contract of its person. "" when valid. */
[[nodiscard]] PERFECTWORLDCORE_API std::string ValidateNpcScheduleContent(const NpcSchedule& schedule);
} // namespace PerfectWorld
