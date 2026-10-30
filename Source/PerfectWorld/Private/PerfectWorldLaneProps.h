#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldLaneBlocks.h"

namespace PWLane
{
/**
 * Street life along the lane: utility poles with sagging overhead wires and service drops, wires strung between
 * facades, two lines of festival bunting, street trees, a parked yellow taxi, scooters and a handcart.
 * Needs the building fronts recorded in the scene, so run it after the rows are built.
 */
void BuildLaneProps(FLaneScene& Scene);
} // namespace PWLane
