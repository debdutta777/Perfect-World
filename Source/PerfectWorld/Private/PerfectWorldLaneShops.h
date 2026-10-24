#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldLaneBuildings.h"

namespace PWLane
{
/** Footpath circulation contract (see also PerfectWorldLaneRoutes.h): a continuous clear route at least this wide. */
constexpr double MinFootpathRoute = 120.0;

/** Depth (V, cm) of the niche cut into the building for an open stall's counter, shelves and goods. */
constexpr double StallRecessDepth = 100.0;

/** Height above GroundZ of the niche's ceiling; the wall above it is solid. */
constexpr double StallRecessTop = 258.0;

/**
 * The ground floor of a first-row building: roller-shutter shopfronts with signboards, open stalls with striped
 * awnings and goods, or a house door with a barred window, plus a few potted plants on the footpath.
 *
 * Circulation: an open stall's counter stands inside a niche cut into the building (one span per stall is
 * returned in OutRecesses, in increasing U, for the caller to carve out of the solid wall), so no counter,
 * shelf or goods stands on the footpath. Awning poles are used only where the awning is deep enough to leave
 * a wall-side route of at least MinFootpathRoute behind them. Pots stand only on footpaths that have a
 * setback, never in front of a door or a stall.
 */
void BuildLaneGroundFloor(FLaneScene& Scene, const FLaneBuilding& Building, TArray<FLaneSpan>& OutRecesses);
} // namespace PWLane
