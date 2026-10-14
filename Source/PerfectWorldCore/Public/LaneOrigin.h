#pragma once

#include "CharacterCreation.h"
#include "Conversation.h"
#include "Schedules.h"

#include <span>

namespace PerfectWorld
{
/** Stable contracts for EN/LA. These IDs describe authored targets, not constructed interiors. */
struct LaneOriginBinding
{
    std::string HouseholdTemplateId;
    bool GrewUpInCare = false;
    std::string HomeKitId;
    std::string BuildingId;
    std::string LocationId;
    /** Only these locations grant resident access; never treat residency as a global flag. */
    std::vector<std::string> ResidentLocationIds;
    std::string CounterpartPersonId;
    std::string RelationshipLabel;
    std::string ScheduleId;
    /** Role at LocationId that permits ordinary conversation. No duty hours live here. */
    std::string AvailableRole;
    std::string SleepingRole;
    friend bool operator==(const LaneOriginBinding&, const LaneOriginBinding&) = default;
};

enum class LaneOriginError : std::uint8_t
{
    None, InvalidPlan, MissingBinding, InvalidBinding, DuplicateBinding,
    ScheduleMismatch, InvalidSchedule, InvalidTime
};

struct LaneOriginResult
{
    LaneOriginError Error = LaneOriginError::None;
    /** Empty on failure. Owns its strings, including when using a caller-owned catalogue. */
    LaneOriginBinding Binding;
    [[nodiscard]] bool Ok() const noexcept { return Error == LaneOriginError::None; }
};

[[nodiscard]] PERFECTWORLDCORE_API std::span<const LaneOriginBinding> AuthoredLaneOrigins() noexcept;
/** Accepts ResolveOpeningHome() output. Inconsistent care flags/kits are refused, never repaired. */
[[nodiscard]] PERFECTWORLDCORE_API LaneOriginResult ResolveLaneOrigin(const OpeningHomePlan& plan);
/** Explicit catalogue for loading/integration: missing, duplicate or altered frozen rows fail closed. */
[[nodiscard]] PERFECTWORLDCORE_API LaneOriginResult ResolveLaneOrigin(
    const OpeningHomePlan& plan, std::span<const LaneOriginBinding> bindings);
[[nodiscard]] PERFECTWORLDCORE_API bool HasLaneOriginResidency(
    const LaneOriginBinding& binding, std::string_view locationId);

struct LaneOriginAvailability
{
    LaneOriginError Error = LaneOriginError::None;
    PersonAvailability Availability = PersonAvailability::Busy;
    [[nodiscard]] bool Ok() const noexcept { return Error == LaneOriginError::None; }
};

/** The integrator loads ScheduleId and supplies that schedule and the authoritative world instant.
 * Uses ResolveSchedule including exceptions/travel; off duty, absent and travelling are Busy.
 * The bound available/sleeping roles must be authored in the schedule as Role entries.
 */
[[nodiscard]] PERFECTWORLDCORE_API LaneOriginAvailability ResolveLaneOriginAvailability(
    const LaneOriginBinding& binding, std::string_view scheduleId,
    const PersonSchedule& person, const ScheduleTravelTable& travel, GameInstant at);
} // namespace PerfectWorld
