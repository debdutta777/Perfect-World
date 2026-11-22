#pragma once

#include "CoreMinimal.h"
#include "LaneFrontageCatalogue.h"
#include "PerfectWorldLaneInteriorKit.h"

namespace PWLane
{
/**
 * Generic kit-built frontages (LANE_LIFE_UPGRADE_PLAN LL-03, docs/LANE_BUILDING_INVENTORY.md). The portable catalogue
 * (LaneFrontageCatalogue.h) pins each frontage to the exact footprint the lane generator produces and names its building id,
 * variant, entrance and furniture set; this file recognises a generated building against that table and builds its ground floor
 * with the interior kit, INSTEAD of BuildLaneGroundFloor and AddWallVolume, exactly as the stationery shop does. The storeys above
 * the ceiling stay solid, so the street facade above the ground floor is unchanged.
 *
 * Doors. The door codec only knows authored doors, so these frontages have open doorways (no leaf, no sweep, no lane door id).
 * Each row names the door that would be added by a later door-set migration (FLaneKitFrontage::PlannedDoorId); nothing here
 * creates it. TODO(doors): migrate the planned doors, then set EntranceDoorId in the catalogue row.
 *
 * Integration (the integrator's, PerfectWorldLaneBuildings.cpp BuildFull, before the final else):
 *     else if (FLaneKitFrontage KitFrontage; IsKitFrontage(B, KitFrontage)) { BuildKitFrontage(S, B, KitFrontage); }
 */
struct FLaneKitFrontage
{
    FName BuildingId;
    FName PlannedDoorId;  // TODO(doors): not authored yet; NAME_None when none is planned
    FName DoorId;         // the lane door hung on the entrance; NAME_None builds an open doorway
    LI::Variant Kind = LI::Variant::CompactResidence;
    LI::FurnitureSet Furniture = LI::FurnitureSet::None;
    int32 EntranceCentreU = 0;
    bool bMirrorU = false;
    const LI::FrontageEntry* Entry = nullptr; // the catalogue row (static storage)
};

/** True, with the row's details in Out, only for a building whose side, frame, footprint, setback, storeys and seed all equal a catalogue row. */
bool IsKitFrontage(const FLaneBuilding& Building, FLaneKitFrontage& Out);

/** The portable layout of a recognised frontage (built once per call; identical for the build and the tests). */
LI::InteriorLayout MakeKitFrontageLayout(const FLaneBuilding& Building, const FLaneKitFrontage& Frontage);

/**
 * Replaces the building's solid ground-floor volume: the kit's walls with real openings (front doorway, windows, internal
 * doorways), floors, ceiling, furniture, ceiling lights, plus a plinth, a threshold, a canopy and a door lamp on the street face. The
 * storeys above stay a solid block. If the core refuses the layout the whole building stays a solid block and an error is logged.
 * Call INSTEAD of BuildLaneGroundFloor and AddWallVolume.
 */
void BuildKitFrontage(FLaneScene& Scene, const FLaneBuilding& Building, const FLaneKitFrontage& Frontage);

/** The front doorway of a layout (the opening from "outside"), or nullptr. */
const LI::Opening* KitFrontEntrance(const LI::InteriorLayout& Layout);
} // namespace PWLane
