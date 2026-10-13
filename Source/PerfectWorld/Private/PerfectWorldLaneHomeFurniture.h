#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldLaneBuildings.h"

namespace PWLane
{
/**
 * Furniture, fixtures, ceiling fans and ceiling lights for the opening home (see PerfectWorldLaneInteriors.h for the
 * layout). Furniture that is solid gets its own PropCollision boxes (Pawn, Camera, Visibility); decoration has none.
 * Lights go into Scene.RoomLights. Rooms (local U, V in cm, clear of the walls):
 *   Living/eating  U 25-614   V 25-431     Entry/utility U 626-975  V 25-431
 *   Sleeping/study U 25-374   V 443-875    Kitchen       U 386-734  V 443-875    Washroom U 746-975 V 443-875
 */
void FurnishOpeningHome(FLaneScene& Scene, const FLaneBuilding& Building);
} // namespace PWLane
