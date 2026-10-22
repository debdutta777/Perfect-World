#pragma once

#include "CoreMinimal.h"
#include "NeighbourhoodPlan.h"
#include "PerfectWorldLaneBlocks.h"

class AActor;
class APWSchoolBoardActor;
class APWSchoolGateActor;
class APWShopHoursSignActor;
class UWorld;

/**
 * R7-03: Nabapara Vidyalaya, a proper school built of blocks (prefix Sc / PWSchool).
 *
 * Every extent comes from the portable neighbourhood plan (NeighbourhoodPlan.h, R7-01) and is frozen there:
 *   site.nabapara.school                  X 5000..12000, Y 4850..11500
 *   entrance.nabapara.school_south_gate   (6500, 4850), 400 cm opening
 *   school.nabapara.forecourt             X 5500..11500, Y 4850..6050
 *   building.nabapara.school_teaching     X 5500..8000, Y 6500..11000, three storeys
 *   school.nabapara.playground            X 8300..11700, Y 6500..11000
 *   entrance.nabapara.school_office       (6500, 6500)      entrance.nabapara.school_classroom (7500, 6500)
 * Z follows the lane: pedestrian and ground-floor datum -185 (PWLane::GroundZ + FootpathHeight).
 *
 * Blocks only. Collision is by the lane's own roles (ELaneCollision): walls, ground and paving block everything (the wall
 * colours, Footpath, Ground); solid props have invisible PropBlock boxes within a few centimetres of their shape; decoration
 * (windows, doors folded open, fans, posters, markings) has none. The ground floor is step free: the gate, forecourt,
 * verandah, both doorways and both rooms are at -185, so a wheelchair route exists from the gate to the classroom (the test
 * PerfectWorld.SchoolCampus.Reachability walks it with the 34 cm player capsule). The upper storeys are exterior massing.
 *
 * No door actor: the office and classroom doorways are 130 cm openings with their leaves folded back against the wall (a
 * public school door during school hours). A leaf that closed would need a new door id in the portable roster (and with it
 * a save-format change); that is described for the integrator in the order's report, not added here.
 */
namespace PWSchool
{
// ---- Numbers ----

/** Pedestrian and ground-floor datum, shared with the lane (the footpath top). */
constexpr double ScGroundZ = PWLane::GroundZ + PWLane::FootpathHeight;
constexpr double ScStoreyHeight = PWLane::OlderStoreyHeight; // 360: the school is an older, taller building
constexpr double ScSlab = 30.0;
constexpr double ScWall = 30.0;
constexpr double ScBoundaryWallHeight = 250.0;
constexpr double ScDoorWidth = 130.0;
constexpr double ScDoorHeight = 230.0;
constexpr double ScVerandahDepth = 350.0; // Y 6150..6500: the covered corridor in front of the teaching block

/** Finished floor of storey n (0 = ground), absolute Z. */
inline double ScFloorZ(int32 Storey)
{
    return ScGroundZ + static_cast<double>(Storey) * ScStoreyHeight;
}

/** An axis-aligned XY rectangle in world centimetres. */
struct FScRect
{
    double X0 = 0.0, Y0 = 0.0, X1 = 0.0, Y1 = 0.0;

    double CentreX() const { return 0.5 * (X0 + X1); }
    double CentreY() const { return 0.5 * (Y0 + Y1); }
    double Width() const { return X1 - X0; }
    double Depth() const { return Y1 - Y0; }
    bool ContainsXY(double X, double Y) const { return X >= X0 && X <= X1 && Y >= Y0 && Y <= Y1; }
};

/** The campus as the plan freezes it. bValid is false when any school feature is missing or malformed. */
struct FScCampus
{
    bool bValid = false;
    FScRect Site;
    FScRect Forecourt;
    FScRect Teaching;
    FScRect Playground;
    double GateX = 0.0;      // centre of the south gate opening
    double GateWidth = 0.0;  // 400
    double OfficeDoorX = 0.0;
    double ClassroomDoorX = 0.0;
};

/** Reads the campus from the plan's own features; nothing here is typed twice. */
FScCampus ScReadCampus(const PerfectWorld::NeighbourhoodPlan& Plan);

/** The plan every spawn uses (BuildNeighbourhoodPlan(0): the seed is metadata only). */
FScCampus ScReadCampus();

// ---- Built output ----

/** One named box of the built campus, kept so tests and the walking check can read the geometry back. */
struct FScPart
{
    FName Id;
    PWLane::ELaneColour Colour = PWLane::ELaneColour::WallCream;
    FBox Box;
};

/** True when the lane's collision table blocks the walking player for this colour (walls, ground, paving, prop and barrier). */
bool ScBlocksPlayer(PWLane::ELaneColour Colour);

/** A text sign drawn by APWShopHoursSignActor (text in the actor's front, facing along Yaw). */
struct FScTextSign
{
    FName Id;
    FString Text;
    FVector Location = FVector::ZeroVector;
    double Yaw = -90.0; // faces -Y: toward the para road and the forecourt's approach
    float SizeCm = 12.0f;
};

/** Where things are, for the spawn, the tests and the acceptance scenario. World centimetres, Z at the floor. */
struct FScPoints
{
    FVector GateCentre = FVector::ZeroVector;   // the middle of the opening on the boundary line
    FVector SignInStand = FVector::ZeroVector;  // where the player stands to sign in (110 cm from the register)
    FVector SignInPoint = FVector::ZeroVector;  // the register's interaction point (stand top)
    FVector BoardStand = FVector::ZeroVector;   // where the player stands to read the timetable
    FVector BoardPoint = FVector::ZeroVector;   // the timetable board's interaction point
    FVector BoardTextLocation = FVector::ZeroVector; // the middle of the timetable text on the board's face
    double BoardTextYaw = -90.0;
    float BoardTextSizeCm = 7.0f;
    FVector OfficeDoor = FVector::ZeroVector;
    FVector OfficeInside = FVector::ZeroVector;
    FVector ClassroomDoor = FVector::ZeroVector;
    FVector ClassroomAisle = FVector::ZeroVector; // centre aisle, third row
    FVector ClassroomFront = FVector::ZeroVector; // in front of the teacher's table, facing the blackboard
    FVector Playground = FVector::ZeroVector;     // the middle of the field
    FVector FlagPost = FVector::ZeroVector;
    FVector WaterTap = FVector::ZeroVector;
};

/** Where benches and desks stand; the acceptance and the tests count them. */
struct FScClassroomCounts
{
    int32 DeskUnits = 0; // double desks; each seats two
    int32 Seats = 0;
    int32 Fans = 0;
};

/** The blocks plus everything the spawn needs besides blocks. Built without a world, so tests can read it. */
struct FScBuilt
{
    PWLane::FLaneBlocks Blocks;
    TArray<FScPart> Parts;
    TArray<FScTextSign> Texts;
    TArray<PWLane::FLaneRoomLight> IndoorLights;  // shadowed, short reach
    TArray<PWLane::FLaneRoomLight> OutdoorLights; // unshadowed
    FScPoints Points;
    FScClassroomCounts Classroom;

    /** Boxes whose Id starts with Prefix, in build order. */
    TArray<const FScPart*> FindParts(const FString& Prefix) const;
    /** The one part with this exact id, or null. */
    const FScPart* FindPart(const TCHAR* Id) const;
    /** Union of every part whose Id starts with Prefix (zero-size when none). */
    FBox UnionOf(const FString& Prefix) const;
};

/** Builds the whole campus into Out. False (and Out empty) when the plan's school is not valid. */
bool ScBuildCampus(FScBuilt& Out, const FScCampus& Campus);

// ---- Spawn ----

/** What SpawnSchoolCampus made. */
struct FScSpawnResult
{
    bool bBuilt = false;
    int32 Instances = 0;
    int32 Components = 0;
    AActor* BlocksActor = nullptr;
    APWSchoolGateActor* Gate = nullptr;
    APWSchoolBoardActor* Board = nullptr;
    int32 TextSigns = 0;
    int32 Lights = 0;
};

/** The tag the blocks actor carries, so tests and the acceptance driver can find the campus. */
extern const FName ScBlocksActorTag;

/**
 * Builds the school from the neighbourhood plan and spawns its actors: the blocks, the text signs, the sign-in register, the
 * timetable board and the lights. Call once from BuildKolkataLane after the lane's blocks are committed:
 *
 *     PWSchool::SpawnSchoolCampus(World, Scene);
 *
 * Scene is the lane's scene, taken so a later order can mask against it; the campus needs nothing from it today. The cube mesh
 * and BasicShapeMaterial are loaded from the engine content like BuildKolkataLane's caller does.
 */
FScSpawnResult SpawnSchoolCampus(UWorld& World, const PWLane::FLaneScene& Scene);
} // namespace PWSchool

/** Short spelling for the order's contract: SpawnSchoolCampus(UWorld&, const FLaneScene&). */
using PWSchool::SpawnSchoolCampus;
