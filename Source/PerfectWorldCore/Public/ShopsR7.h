#pragma once

#include "NeighbourhoodPlan.h"
#include "NpcBehaviour.h"

#include <span>
#include <string_view>

namespace PerfectWorld
{
struct ShopWindowR7 { int Open = 0, Close = 0; };
/** Immutable staff/threshold contract. EntranceId is a spatial entrance, never a save door. */
struct ShopStaffR7
{
    std::string_view PersonId, ScheduleId, DisplayName;
    int Age = 0, HeightCm = 0;
    std::string_view HomeId, WorkplaceId, EntranceId, CounterId;
    /** Assistant uses a distinct spot 150 cm east of the sweet-shop destination. */
    std::string_view PlanCounterId;
    NeighbourhoodPoint CounterPosition;
    int EntranceRouteCm = 0;
    std::span<const ShopWindowR7> Opening;
};
[[nodiscard]] PERFECTWORLDCORE_API std::span<const ShopStaffR7> ShopStaffR7Definitions() noexcept;
[[nodiscard]] PERFECTWORLDCORE_API bool IsShopStaffR7(std::string_view person) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API bool IsShopOpenR7(const ShopStaffR7& staff, int minute) noexcept;
struct ShopWorkR7
{
    NpcWorkPhase Phase = NpcWorkPhase::None;
    std::string_view Action, Pause;
    NpcDecisionReason Reason = NpcDecisionReason::Available;
};
/** Exact person + anchor + role opt-in for the existing safe-pause decision algorithm. */
[[nodiscard]] PERFECTWORLDCORE_API ShopWorkR7 ShopWorkAtR7(
    const PersonSchedule& person, const ScheduleResolution& block) noexcept;
} // namespace PerfectWorld
