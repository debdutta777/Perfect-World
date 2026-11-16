#pragma once

#include "TowerScenarioContent.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <set>

template <typename RequireT>
void P404BSelectionCases(const PerfectWorld::TowerScenarioCatalogue& catalogue,
    const std::vector<PerfectWorld::ContentFile>& files, const PerfectWorld::ContentDb& cases,
    const RequireT& require)
{
    using namespace PerfectWorld;
    auto reversedFiles = files;
    std::reverse(reversedFiles.begin(), reversedFiles.end());
    const auto reloaded = LoadTowerScenarioFiles(reversedFiles, true, cases);
    require(reloaded.Ok(), "reloading reversed authored files accepts multiple scenarios per route");
    auto reordered = reloaded.Catalogue;
    std::reverse(reordered.Scenarios.begin(), reordered.Scenarios.end());
    const auto original = catalogue.Scenarios;
    require(TowerScenarioCatalogue{}.P404BSelectForRoute("tower.route.pace", 0, 0) == nullptr,
        "empty catalogue has no selectable scenario");
    require(catalogue.P404BSelectForRoute("tower.route.absent", 0, 0) == nullptr,
        "unknown route has no selectable scenario");
    for (const auto& route : TowerRoutes())
    {
        std::set<std::string> drawn, bySeed, byAttempt, expected;
        for (const auto& scenario : catalogue.Scenarios)
            if (scenario.RouteId == route.Id) expected.insert(scenario.Id);
        for (std::uint32_t attempt = 0; attempt < 64; ++attempt)
        {
            const auto* fixedSeed = catalogue.P404BSelectForRoute(route.Id, 404, attempt);
            const auto* fixedAttempt = catalogue.P404BSelectForRoute(route.Id, attempt, 0);
            require(fixedSeed && fixedAttempt, "every route stays covered for every tested draw");
            byAttempt.insert(fixedSeed->Id);
            bySeed.insert(fixedAttempt->Id);
            for (const std::uint64_t seed : {std::uint64_t{0}, std::uint64_t{404}, std::numeric_limits<std::uint64_t>::max()})
            {
                const auto* selected = catalogue.P404BSelectForRoute(route.Id, seed, attempt);
                require(selected && selected->RouteId == route.Id, "draw stays within its route pool");
                require(selected == catalogue.P404BSelectForRoute(route.Id, seed, attempt), "identical inputs select identically");
                require(*selected == *reloaded.Catalogue.P404BSelectForRoute(route.Id, seed, attempt), "reload retains the same scenario");
                require(selected->Id == reordered.P404BSelectForRoute(route.Id, seed, attempt)->Id, "vector order cannot reroll a draw");
                drawn.insert(selected->Id);
            }
        }
        require(drawn == expected && bySeed == expected && byAttempt == expected,
            "every original and new scenario remains selectable by both seed and attempt");
        const auto* last = catalogue.P404BSelectForRoute(route.Id, 0, std::numeric_limits<std::uint32_t>::max());
        require(last && last->Id == reordered.P404BSelectForRoute(route.Id, 0, std::numeric_limits<std::uint32_t>::max())->Id,
            "maximum attempt number is stable across ordering");
    }
    require(catalogue.Scenarios == original, "draws leave catalogue unchanged");
}
