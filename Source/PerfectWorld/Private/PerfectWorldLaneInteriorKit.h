#pragma once

#include "CoreMinimal.h"
#include "LaneInteriorLayout.h"
#include "PerfectWorldLaneBuildings.h"

namespace PWLane
{
namespace LI = PerfectWorld::LaneInterior;

/**
 * The parametric interior kit's presenter (LL-03). The portable core (LaneInteriorLayout.h) decides walls, openings, rooms,
 * furniture, anchors, clearances and lights; this file only turns that result into voxel block visuals, collision, room lights
 * and door specs, in the same style and with the same helpers as the tea shop, the opening home and the children's home.
 * Their file-local shells stay as they are; every later ground floor uses this one.
 *
 * Frame. The layout is in building-local centimetres (U along the frontage, V into the building) with heights above the
 * finished floor; the building's FLaneFrame carries any orientation, so a layout works on either side of the lane and on the
 * end-closing masses unchanged. The kit adds the finished-floor datum (LI::InteriorLayout::FinishedFloorCm) to every height.
 */

/** A layout anchor in world space: the standing point on the finished floor (a seat: the centre of the seat) and its facing. */
struct FKitAnchor
{
    FName Id;
    LI::AnchorKind Kind = LI::AnchorKind::Stand;
    FVector Location = FVector::ZeroVector;
    double YawDegrees = 0.0;
};

/** The colour bucket for a palette token the core uses ("wood_light", "plaster_warm", ...); the fallback for unknown tokens. */
ELaneColour KitColour(const std::string& Token, ELaneColour Fallback = ELaneColour::PlasterWarm);

/** Core input from a lane building: its width, depth and storey height. The caller adds the entrance, variant, furniture and door id. */
LI::InteriorInput MakeKitInput(const FLaneBuilding& Building, const char* BuildingId, LI::Variant Kind, LI::FurnitureSet Furniture,
                               int32 EntranceCentreU, const char* EntranceDoorId);

/** Building-local (U, V) and a height above the finished floor to a world point. */
FVector KitPoint(const FLaneBuilding& Building, const LI::InteriorLayout& Layout, double U, double V, double HeightCm = 0.0);

/** World yaw (degrees) of a unit facing in building-local U, V. */
double KitYaw(const FLaneBuilding& Building, int32 FaceU, int32 FaceV);

/** Every anchor of the layout, in world space. */
void GetKitAnchors(const FLaneBuilding& Building, const LI::InteriorLayout& Layout, TArray<FKitAnchor>& OutAnchors);

/** The door spec for one hung leaf (hinge, closed direction, default swing into the building), in the home's convention. */
FLaneDoorSpec KitDoorSpec(const FLaneBuilding& Building, const LI::InteriorLayout& Layout, const LI::Opening& Opening);

/**
 * Replaces the building's solid ground-floor volume: perimeter and partition walls cut around the layout's openings, trim and
 * glass, floors, the ceiling with the solid storeys above it, every furniture piece (with a collision box when solid), the
 * ceiling fittings with their RoomLights, and a door spec for each hung leaf (Scene.Doors). Call INSTEAD of
 * BuildLaneGroundFloor and AddWallVolume, with a layout whose Status is None.
 */
void BuildInteriorKit(FLaneScene& Scene, const FLaneBuilding& Building, const LI::InteriorLayout& Layout);

/** True once a door spec with this id is in the scene. */
bool HasKitDoor(const FLaneScene& Scene, const TCHAR* DoorId);
} // namespace PWLane
