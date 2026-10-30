#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldLaneBlocks.h"

namespace PWLane
{
#if !UE_BUILD_SHIPPING
/**
 * Debug report of the footpath circulation (development and editor builds only; compiled out of shipping).
 *
 * Walks both footpaths of the first-row buildings in 5 cm steps. At each step it takes the strip between the kerb
 * (|Y| = 250) and the building front, removes every invisible blocking box (PropBlock and Barrier) that stands in
 * the walking height band, and measures the free intervals left. A route needs an interval of at least
 * MinFootpathRoute (120 cm), and each step's interval must overlap one reachable at the step before by at least a
 * capsule width (68 cm). Side alleys are skipped (they have no footpath). Run it after the whole lane is laid out;
 * it logs the narrowest clear width found, where, and any break, as a Warning if the contract is not met.
 */
void ReportFootpathRoutes(const FLaneScene& Scene);
#else
inline void ReportFootpathRoutes(const FLaneScene&)
{
}
#endif
} // namespace PWLane
