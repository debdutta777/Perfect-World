#include "LaneOrigin.h"

#include <algorithm>
#include <array>

namespace PerfectWorld
{
namespace
{
const std::array<LaneOriginBinding, 6> Origins{{
    {"parent_household", false, "compact_family_flat", "building.nabapara.opening_home",
     "location.nabapara.opening_home", {"location.nabapara.opening_home"},
     std::string(HouseholdAdultId), "Your parent", "npc_schedule.household_adult", "available", "sleeping"},
    {"extended_family", false, "row_house", "building.nabapara.extended_family_home",
     "location.nabapara.extended_family_home", {"location.nabapara.extended_family_home"},
     "person.extended_family_adult", "Your relative", "npc_schedule.extended_family_adult", "available", "sleeping"},
    {"grandparent_household", false, "cooperative_flat", "building.nabapara.grandparent_home",
     "location.nabapara.grandparent_home", {"location.nabapara.grandparent_home"},
     "person.grandparent", "Your grandparent", "npc_schedule.grandparent", "available", "sleeping"},
    {"adult_sibling_household", false, "rented_flat", "building.nabapara.adult_sibling_home",
     "location.nabapara.adult_sibling_home", {"location.nabapara.adult_sibling_home"},
     "person.adult_sibling", "Your older sibling", "npc_schedule.adult_sibling", "available", "sleeping"},
    {"appointed_guardian", false, "staff_quarters", "building.nabapara.guardian_home",
     "location.nabapara.guardian_home", {"location.nabapara.guardian_home"},
     "person.appointed_guardian", "Your guardian", "npc_schedule.appointed_guardian", "available", "sleeping"},
    {"", true, std::string(InstitutionalHomeKitId), "building.nabapara.care_home",
     "location.nabapara.care_home", {"location.nabapara.care_home"},
     "person.care_worker", "Care worker", "npc_schedule.care_worker", "on_duty", "sleeping"}
}};

const LaneOriginBinding* FrozenBinding(const LaneOriginBinding& binding)
{
    for (const auto& row : Origins)
    {
        if (row == binding) return &row;
    }
    return nullptr;
}
} // namespace

std::span<const LaneOriginBinding> AuthoredLaneOrigins() noexcept
{
    return Origins;
}

LaneOriginResult ResolveLaneOrigin(const OpeningHomePlan& plan)
{
    return ResolveLaneOrigin(plan, AuthoredLaneOrigins());
}

LaneOriginResult ResolveLaneOrigin(const OpeningHomePlan& plan, std::span<const LaneOriginBinding> bindings)
{
    const auto& households = CharacterCreator::HouseholdDefinitions();
    const auto household = std::find_if(households.begin(), households.end(), [&](const auto& row) {
        return row.Id == plan.HouseholdTemplateId;
    });
    // Structural validation of a resolved plan only: costs, allocation and dressing remain creation-owned.
    if (household == households.end() || plan.ResourceRank < 0 || plan.ResourceRank > MaxRank ||
        plan.GrewUpInCare != (plan.ResourceRank == 0) ||
        plan.HomeKitId != (plan.GrewUpInCare ? InstitutionalHomeKitId : household->HomeKitId))
        return {LaneOriginError::InvalidPlan, {}};

    const LaneOriginBinding* selected = nullptr;
    for (const auto& row : bindings)
    {
        if (row.GrewUpInCare != plan.GrewUpInCare ||
            (!plan.GrewUpInCare && row.HouseholdTemplateId != plan.HouseholdTemplateId)) continue;
        if (selected != nullptr) return {LaneOriginError::DuplicateBinding, {}};
        selected = &row;
    }
    if (selected == nullptr) return {LaneOriginError::MissingBinding, {}};
    if (FrozenBinding(*selected) == nullptr || selected->HomeKitId != plan.HomeKitId)
        return {LaneOriginError::InvalidBinding, {}};
    return {LaneOriginError::None, *selected};
}

bool HasLaneOriginResidency(const LaneOriginBinding& binding, std::string_view locationId)
{
    if (FrozenBinding(binding) == nullptr) return false;
    return std::find(binding.ResidentLocationIds.begin(), binding.ResidentLocationIds.end(), locationId) !=
           binding.ResidentLocationIds.end();
}

LaneOriginAvailability ResolveLaneOriginAvailability(const LaneOriginBinding& binding, std::string_view scheduleId,
    const PersonSchedule& person, const ScheduleTravelTable& travel, GameInstant at)
{
    if (FrozenBinding(binding) == nullptr) return {LaneOriginError::InvalidBinding};
    if (scheduleId != binding.ScheduleId || person.PersonId != binding.CounterpartPersonId)
        return {LaneOriginError::ScheduleMismatch};
    if (at < 0 || at > ScheduleMaxInstant) return {LaneOriginError::InvalidTime};
    if (!ValidatePersonSchedule(person).empty() || !ValidateScheduleTravel(travel).empty() ||
        !ValidateScheduleTravelCoverage(person, travel).empty() || !ScheduleCoversWholeWeek(person) ||
        !ValidateScheduleRoutineTravel(person, travel).empty())
        return {LaneOriginError::InvalidSchedule};
    const auto resolved = ResolveSchedule(person, travel, at);
    if (!resolved.Valid) return {LaneOriginError::InvalidSchedule};
    if (resolved.Status != ScheduleStatus::Stay || resolved.Kind != ScheduleDoingKind::Role)
        return {};
    if (resolved.Doing == binding.SleepingRole)
        return {LaneOriginError::None, PersonAvailability::Asleep};
    if (resolved.Zone == binding.LocationId && resolved.Doing == binding.AvailableRole)
        return {LaneOriginError::None, PersonAvailability::Awake};
    return {};
}
} // namespace PerfectWorld
