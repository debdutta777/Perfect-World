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
/** Shared by the shell and the furniture. Not in an anonymous namespace: the module builds in unity blobs, where two
 * .cpp files defining the same file-local constant collide (Rung 2, 2026-10-14). */
constexpr double HomeFloorW = FootpathHeight; // finished floor, W 15 (level with the footpath)
constexpr double HomeCeilingW = 289.0;        // ceiling underside: 274 cm clear above the floor

void FurnishOpeningHome(FLaneScene& Scene, const FLaneBuilding& Building);
} // namespace PWLane
