#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldLaneBuildings.h"

namespace PWLane
{
/**
 * The ground floor of a first-row building: roller-shutter shopfronts with signboards, open stalls with striped
 * awnings and goods, or a house door with a barred window, plus a few potted plants on the footpath.
 */
void BuildLaneGroundFloor(FLaneScene& Scene, const FLaneBuilding& Building);
} // namespace PWLane
