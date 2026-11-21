#pragma once

#include "ContentRegistry.h"
#include "TowerSession.h"

namespace PerfectWorld
{
struct TowerRunnerCatalogueResult
{
    std::vector<ContentIssue> Issues;
    TowerRunnerCatalogue Catalogue;
    [[nodiscard]] bool Ok() const noexcept { return Issues.empty(); }
};

/** Production graph-to-linear adapter, version P405/1. Keeps every scenario
 * variant; P404B selects one per route when opened. Scenario/beat/choice IDs are
 * the authored IDs verbatim, never vector indices or translated text. Once
 * shipped, these IDs, their order and scoring/cost semantics are immutable.
 *
 * Each beat's metric is normalized against its best non-combat decision. The
 * other metric is a constant neutral 1000, independent of the chosen action.
 * TowerSession averages these local observations; this deliberately differs
 * from the graph evaluator's per-metric sums. Mistakes include automatic recovery
 * time/evidence and cost four reserve, with no reserve recovery. Other choices
 * cost no reserve. Combat options remain restricted to a combat route method.
 * All caller-owned content must remain compatible on Continue and replay.
 */
[[nodiscard]] PERFECTWORLDCORE_API TowerRunnerCatalogueResult LoadTowerRunners(const ContentDb& db);
} // namespace PerfectWorld
