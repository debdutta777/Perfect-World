#pragma once

#include "RandomStreams.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace PerfectWorld
{
/** StructureCommon.cpp: StructureSelectCase draws from sorted stable IDs with
 * KeyedRandomBelow. Tower uses its own stream and (route, examination attempt).
 * No history or mutable RNG is consumed. Reuse seed, attempt and content on reload.
 * ScenarioT supplies unique Id and RouteId strings. Empty pools return nullptr.
 */
template <typename ScenarioT>
[[nodiscard]] const ScenarioT* P404BSelectRouteScenario(const std::vector<ScenarioT>& scenarios,
    std::string_view routeId, std::uint64_t worldSeed, std::uint32_t attemptNumber)
{
    std::vector<const ScenarioT*> pool;
    for (const auto& scenario : scenarios)
        if (scenario.RouteId == routeId) pool.push_back(&scenario);
    if (pool.empty()) return nullptr;
    std::sort(pool.begin(), pool.end(), [](const auto* a, const auto* b) { return a->Id < b->Id; });
    const std::array<std::int64_t, 1> key{static_cast<std::int64_t>(attemptNumber)};
    return pool[KeyedRandomBelow(worldSeed, RandomStreamId::Tower, pool.size(), key, routeId)];
}
} // namespace PerfectWorld
