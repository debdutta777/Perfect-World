#pragma once

#include <array>
#include <span>
#include <string_view>
#include <cstdint>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
struct AmbientPoint { std::int32_t X, Y, Height; };
struct AmbientRoute
{
    std::string_view Person, Id, From, To;
    std::array<AmbientPoint, 6> Points;
    std::size_t Count;
    std::int32_t LengthCm;
};
/** Heights are above the lane road. Ends in the side alleys are visible thresholds, never spawn bays. */
[[nodiscard]] PERFECTWORLDCORE_API std::span<const AmbientRoute> LaneAmbientRoutes() noexcept;
inline constexpr std::array<std::string_view, 3> AmbientPeople{
    "person.purnima_das", "person.arun_paul", "person.rekha_bose"};
/** Fixed WORLD offset: turning to face a speaker cannot swing the cart through them. */
inline constexpr AmbientPoint AmbientCartOffset{0, -100, 45};
inline constexpr AmbientPoint AmbientCartHalfExtent{45, 62, 42};
} // namespace PerfectWorld
