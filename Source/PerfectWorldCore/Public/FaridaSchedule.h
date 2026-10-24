#pragma once

#include "ContentRegistry.h"
#include "Schedules.h"

#include <optional>

namespace PerfectWorld
{
constexpr std::string_view FaridaScheduleId = "npc_schedule.farida_qadri";
constexpr std::string_view FaridaPersonId = "person.farida_qadri";
constexpr std::int32_t FaridaTravelCmPerMinute = 140;

struct FaridaRoute
{
    std::string Id;
    std::string From;
    std::string To;
    std::int32_t LengthCm = 0;
};

struct FaridaTravelPath
{
    std::string From;
    std::string To;
    std::int32_t Minutes = 0;
    /** Ordered route IDs; routes may be traversed in either direction. */
    std::vector<std::string> Routes;
};

struct FaridaSchedule
{
    PersonSchedule Person;
    ScheduleTravelTable Travel;
    std::vector<FaridaRoute> Routes;
    std::vector<FaridaTravelPath> Paths;
};

struct FaridaScheduleResult
{
    std::optional<FaridaSchedule> Value;
    std::string Error;
    [[nodiscard]] bool Ok() const { return Value.has_value(); }
};

/** Strict typed decoding, including shape and portable routine/travel validation. */
[[nodiscard]] PERFECTWORLDCORE_API FaridaScheduleResult LoadFaridaSchedule(const ContentDb& db);
[[nodiscard]] PERFECTWORLDCORE_API std::string ValidateFaridaSchedule(const FaridaSchedule& schedule);

namespace ContentSchema
{
struct TypeSchema;
/** CAT: include this header and add &NpcScheduleSchema() to TopLevelSchemas(). */
[[nodiscard]] PERFECTWORLDCORE_API const TypeSchema& NpcScheduleSchema();
[[nodiscard]] PERFECTWORLDCORE_API std::string ValidateNpcScheduleShape(const JsonValue& data);
/** CAT: call from the shared semantics dispatcher after reference resolution.
 * Person, location, door and anchor tokens are lane bindings, not ContentDb refs.
 * REG: register both new core sources and the Farida test (CMake append supplied).
 */
PERFECTWORLDCORE_API void ValidateNpcScheduleSemantics(const ContentDb& db, std::vector<ContentIssue>& issues);
}
} // namespace PerfectWorld
