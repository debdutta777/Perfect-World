#pragma once

#include "CoreMinimal.h"
#include "LaneEndLayouts.h"
#include "PerfectWorldLaneInteriorKit.h"

class UWorld;

namespace PWLane
{
namespace LE = PerfectWorld::LaneEnds;

/**
 * R6-05: the two masses that close the lane as public halls. The west mass (building.nabapara.west_end_block, four storeys) is a
 * furnished community hall, the east mass (building.nabapara.east_end_block, five storeys) a covered market hall. The portable
 * LaneEndLayouts.h decides everything physical (walls, the archway, windows, furniture, anchors, aisles, lights); this file turns it
 * into blocks with the interior kit's presenter (BuildInteriorKit), adds the street face and posts the hours.
 *
 * Entrance. One DOORLESS archway in the lane-facing wall, centred on the lane's axis: no leaf, no door id, no roster entry and no
 * save change. A roller-shutter box is drawn above it, shutters raised, with the posted hours on its face (APWShopHoursSignActor).
 * The hours are shown, not enforced: nothing refuses a visitor outside them.
 *
 * Frames. Both masses use the closing rows' own frames (U = +Y from Y -1300; V runs away from the lane: -X in the west, +X in the east),
 * so a layout is built unchanged and the interior is the same block-and-collision kit as every kit frontage. Only these two buildings
 * leave the generic ground floor; the storeys above, the roof and the facade windows are the existing closing-row geometry.
 *
 * Integration (the integrator's):
 *   PerfectWorldLaneBuildings.cpp BuildFull, before the final else:
 *       else if (LE::EndHall EndHall; IsLaneEndHall(B, EndHall)) { BuildLaneEndHall(S, B, EndHall); }
 *   PerfectWorldKolkataLane.cpp BuildKolkataLane, after SpawnKitShopDisplays:
 *       SpawnLaneEndSigns(World);
 */

/** The building the closing rows generate for this end (the generator's width, depth, storeys, seed and frame). */
FLaneBuilding MakeLaneEndBuilding(LE::EndHall Hall);

/** True, with the hall in Out, only for a full-detail Side 0 building whose frame, footprint and storeys equal a closing mass. */
bool IsLaneEndHall(const FLaneBuilding& Building, LE::EndHall& Out);

/** Builds the hall: the kit interior, the street face with its shutter box and lamps. A refused layout leaves a solid block and logs an error. */
void BuildLaneEndHall(FLaneScene& Scene, const FLaneBuilding& Building, LE::EndHall Hall);

/** The posted board text: "<label> hours" then the hours line, one per line. */
FString LaneEndSignText(LE::EndHall Hall);
/** World location (the middle of the shutter box's face) and yaw (the text faces the lane) of a hall's posted board. */
FVector LaneEndSignLocation(LE::EndHall Hall);
double LaneEndSignYaw(LE::EndHall Hall);

/** Spawns one APWShopHoursSignActor per hall (building id as its id). Returns the number spawned. */
int32 SpawnLaneEndSigns(UWorld& World);
} // namespace PWLane
