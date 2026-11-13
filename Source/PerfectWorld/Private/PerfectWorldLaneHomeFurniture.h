#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldLaneBuildings.h"

namespace PWLane
{
/**
 * Furniture, fixtures, ceiling fans and ceiling lights for the opening home (see PerfectWorldLaneInteriors.h for the
 * layout), plus the DATA the integrator turns into actors: the household adult's spot, the seat and readable-object
 * anchors, the reserved stair footprint and the room light specs. Furniture that is solid gets its own PropCollision
 * boxes (Pawn, Camera, Visibility); decoration has none. Lights go into Scene.RoomLights. Rooms (local U, V in cm, clear
 * of the walls):
 *   Living/eating  U 25-614   V 25-431     Entry/utility U 626-975  V 25-431
 *   Sleeping/study U 25-374   V 443-875    Kitchen       U 386-734  V 443-875    Washroom U 746-975 V 443-875
 * Everything below is in building-local coordinates (U along the frontage, V into the building, heights in cm above the
 * finished floor) unless a function says "World". HomeWorldPoint() converts with the building's frame.
 */
/** Shared by the shell and the furniture. Not in an anonymous namespace: the module builds in unity blobs, where two
 * .cpp files defining the same file-local constant collide (Rung 2, 2026-10-14). */
constexpr double HomeFloorW = FootpathHeight; // finished floor, W 15 (level with the footpath)
constexpr double HomeCeilingW = 289.0;        // ceiling underside: 274 cm clear above the floor
constexpr double HomePersonRadiusCm = 34.0;   // the walking capsule

/** The collision footprint of one solid piece of furniture (the same box FurnishOpeningHome gives PropCollision). */
struct FHomeSolid
{
    double U0 = 0.0;
    double U1 = 0.0;
    double V0 = 0.0;
    double V1 = 0.0;
    double H0 = 0.0; // bottom, above the finished floor
    double H1 = 0.0; // top
};

/**
 * Where the household adult stands: in the living room, 125 cm from the living doorway's threshold (U 620, V 365), turned
 * to face it. A player stepping through the doorway is well inside the 2 m reach and the 50 degree half-cone
 * (PWInteraction), and the spot is clear of every piece of furniture by 130 cm and of every door swing by 55 cm. The
 * living door's leaf now lies in the entry, so no open leaf is ever between the two people.
 */
struct FHomeAdultSpot
{
    double U = 0.0;
    double V = 0.0;
    double FaceU = 0.0; // the point they face: the centre of the living doorway's threshold
    double FaceV = 0.0;
};

/** A place to sit (the integrator's furniture actor spawns it): the divan, and the east chair at the table. */
struct FHomeSeatSpec
{
    FName Id;
    const TCHAR* Verb = TEXT(""); // the prompt, e.g. "Sit on the divan"
    double SeatU = 0.0;           // centre of the seating surface
    double SeatV = 0.0;
    double SeatHeightCm = 0.0;    // top of the cushion above the finished floor
    double FaceU = 0.0;           // unit vector: the way a seated person faces
    double FaceV = 0.0;
    double StandU = 0.0;          // where the person is put on standing up (also where they approach from)
    double StandV = 0.0;
    double StandClearCm = 0.0;    // the floor within this radius of the stand point is free of solid furniture and walls
};

/** Something to read: a newspaper on the table. ContentId names the text the reading presenter (RD) will show. */
struct FHomeReadableSpec
{
    FName Id;
    FName ContentId;
    const TCHAR* Verb = TEXT("");
    double U = 0.0; // centre of the sheet
    double V = 0.0;
    double HeightCm = 0.0;   // the sheet's surface above the finished floor
    double StandU = 0.0;     // where to stand to read it
    double StandV = 0.0;
    double StandClearCm = 0.0;
};

/**
 * The reserved upper-floor stair: a dog-leg (U-turn) flight of 17 equal risers, 25 cm treads, two 90 cm lanes. The
 * footprint is KEPT CLEAR (nothing solid, no door swing) and marked on the floor by a darker patch; the stairwell void in
 * the ceiling slab is not cut yet. Lower flight: 9 risers (8 treads) up the east lane from its foot line, then a 90 cm
 * landing across both lanes; upper flight: 8 risers (7 treads) back down the west lane to the upper floor's edge.
 */
struct FHomeStairSpec
{
    FName Id;
    double U0 = 0.0; // the keep-clear footprint
    double U1 = 0.0;
    double V0 = 0.0;
    double V1 = 0.0;
    double FloorToFloorCm = 0.0;
    int32 Risers = 0;
    double RiserCm = 0.0; // FloorToFloorCm / Risers
    double TreadCm = 0.0;
    double LaneWidthCm = 0.0;
    double FootV = 0.0;     // the foot line of the lower flight, on its east lane
    double LandingV0 = 0.0; // the landing across both lanes
    double LandingV1 = 0.0;
    double UpperEdgeV = 0.0; // where the upper flight meets the upper floor
};

/** A ceiling fan: its axis, the blade radius and the height of the blades' underside above the finished floor. */
struct FHomeFanSpec
{
    double U = 0.0;
    double V = 0.0;
    double RadiusCm = 0.0;
    double UndersideCm = 0.0;
};

/**
 * One room light. Scene.RoomLights gets Position, Radius (= AttenuationCm) and Lumens. The extra fields tell the integrator
 * how to spawn it: bCastShadows MUST be true (the old SpawnRoomLights switched shadows off, which let a light shine
 * through the walls), SourceRadiusCm a small soft source, ColourTempK the colour (2900 warm tungsten, 4000 for the kitchen
 * and washroom work lights; FLinearColor::MakeFromColorTemperature). AttenuationCm never exceeds the room's shorter side.
 */
struct FHomeLightSpec
{
    FName Room;
    double U = 0.0;
    double V = 0.0;
    float AttenuationCm = 0.0f;
    float Lumens = 0.0f;
    float SourceRadiusCm = 6.0f;
    float ColourTempK = 2900.0f;
    bool bCastShadows = true;
    double RoomU0 = 0.0; // the room the light must stay in
    double RoomU1 = 0.0;
    double RoomV0 = 0.0;
    double RoomV1 = 0.0;
};

FHomeAdultSpot HomeAdultSpot();
/** The adult's feet position (finished floor level) in the world, for the given building frame. */
FVector HomeAdultSpotWorld(const FLaneFrame& Frame);
/** Actor yaw in degrees so the actor's forward (+X) faces the doorway threshold. A mesh whose forward is not +X needs an offset. */
double HomeAdultSpotYaw(const FLaneFrame& Frame);

TArray<FHomeSeatSpec> HomeSeatSpecs();
FHomeReadableSpec HomeReadableSpec();
FHomeStairSpec HomeStairSpec();
TArray<FHomeFanSpec> HomeFanSpecs();
TArray<FHomeLightSpec> HomeLightSpecs();

/** A building-local point (U, V, height above the finished floor) in the world. */
FVector HomeWorldPoint(const FLaneFrame& Frame, double U, double V, double HeightCm = 0.0);

/** Builds the furniture, fans and lights. OutSolids, when given, receives the footprint of every solid piece. */
void FurnishOpeningHome(FLaneScene& Scene, const FLaneBuilding& Building, TArray<FHomeSolid>* OutSolids = nullptr);
} // namespace PWLane
