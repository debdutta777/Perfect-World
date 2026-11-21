#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldLaneBuildings.h"

namespace PWLane
{
/**
 * Shanti Chai Corner: the lane's first public interior and first workplace (LANE_LIFE_PLAN.md B "Farida Qadri", C "Shanti
 * Chai Corner"). Ground floor only; the upper storeys stay a solid block so the street facade is unchanged.
 *
 * Which building: the first-row building on the +Y side with Index 3. BuildFirstRows gives side +1 the hash
 * LaneHash(138, 3, 11) = 10160749 (Side * 131 + 7 = 138), which yields X 600 to 1300 cm (Frame origin (600, 520),
 * U = +X, V = +Y), Width 700 (600 + 100 * (10160749 % 6 = 1)), Depth 900 (700 + 100 * ((H >> 16) % 3 = 2)), Setback 120
 * ((H >> 12) % 4 = 0), 3 storeys of 300 cm, and wall colour WallTerracotta. The generic ground floor would give it open
 * stalls (Mode = (3 + 1) % 3 = 1); the integrator calls BuildTeaShop INSTEAD of BuildLaneGroundFloor and AddWallVolume.
 * IsTeaShopBuilding checks the footprint too, so a generator change makes the lane fall back to the old building.
 *
 * Local coordinates (as for the opening home): U along the frontage (world X), V from the front face into the building
 * (world +Y here), W above GroundZ (finished floor W 15, ceiling underside W 289). Heights in the piece table are above the
 * finished floor. The 120 cm setback is extra footpath: the footpath in front of this building is 270 cm deep (V -270 to 0).
 */
inline constexpr const TCHAR* TeaShopShutterDoorId = TEXT("door.nabapara.shanti_chai_shutter"); // BusinessHours; owned by the core later
inline constexpr const TCHAR* TeaShopAnchorCounter = TEXT("anchor.shanti_chai.counter");
inline constexpr const TCHAR* TeaShopAnchorStove = TEXT("anchor.shanti_chai.stove");
inline constexpr const TCHAR* TeaShopAnchorWash = TEXT("anchor.shanti_chai.wash");
inline constexpr const TCHAR* TeaShopAnchorStorage = TEXT("anchor.shanti_chai.storage");
inline constexpr const TCHAR* TeaShopAnchorBenchWipe = TEXT("anchor.shanti_chai.bench_wipe");
inline constexpr const TCHAR* TeaShopAnchorRest = TEXT("anchor.shanti_chai.rest");
inline constexpr const TCHAR* TeaShopAnchorDoor = TEXT("anchor.shanti_chai.door");
inline constexpr uint32 TeaShopGeneratorSeed = 10160749u; // FLaneBuilding::Seed of the building (LaneHash(138, 3, 11))

/** A rectangle in building-local U, V (centimetres). */
struct FTeaShopRect
{
    double U0;
    double U1;
    double V0;
    double V1;

    constexpr bool Contains(double U, double V) const
    {
        return U >= U0 && U <= U1 && V >= V0 && V <= V1;
    }
};

namespace TeaShopLayout
{
inline constexpr double FloorW = FootpathHeight;  // finished floor, level with the footpath
inline constexpr double CeilingW = 289.0;         // ceiling underside: 274 cm clear
inline constexpr FTeaShopRect Interior{25.0, 675.0, 25.0, 875.0};      // inside the 25 cm walls
inline constexpr FTeaShopRect CustomerRoom{25.0, 675.0, 25.0, 330.0};  // covered seating, queue, the door
inline constexpr FTeaShopRect ServiceCounter{25.0, 575.0, 330.0, 390.0}; // the staff boundary: 60 deep, top 90 above the floor
inline constexpr FTeaShopRect StaffGap{575.0, 675.0, 330.0, 390.0};    // the side staff access: 100 cm clear, tile floor, STAFF sign
inline constexpr FTeaShopRect PrepArea{25.0, 675.0, 390.0, 600.0};     // stove, washing, preparation table; staff only
inline constexpr FTeaShopRect BackRoom{25.0, 675.0, 612.0, 875.0};     // storage and the rest corner; staff only
inline constexpr FTeaShopRect OutdoorSeating{60.0, 640.0, -90.0, 0.0}; // benches and low tables under the awning, on the footpath
inline constexpr double CounterTopH = 90.0;          // above the floor
inline constexpr double DoorCentreU = 350.0;         // customer entrance, clear 94 (U 303 to 397)
inline constexpr double DoorHalfClear = 47.0;
inline constexpr double ShutterHingeU = 395.0;       // the leaf spans U 305 to 395 and swings in or out (BusinessHours)
inline constexpr double ShutterHingeV = 12.5;
inline constexpr double PartitionV0 = 600.0;         // wall between the prep area and the back room, 12 thick
inline constexpr double PartitionV1 = 612.0;
inline constexpr double PartitionDoorCentreU = 622.0; // open doorway (no leaf), clear 90 (U 577 to 667)
inline constexpr double PartitionDoorHalfClear = 45.0;
inline constexpr double FootpathRouteV = -180.0;     // the lane footpath walking line (world Y 340): clear of the seating and the kerb pole
} // namespace TeaShopLayout

/** One piece of furniture or fixture, in building-local U, V and heights above the finished floor. */
struct FTeaShopPiece
{
    ELaneColour Colour;
    double U0;
    double U1;
    double V0;
    double V1;
    double H0;
    double H1;
    bool bSolid; // true: blocks Pawn, Camera and Visibility with a box of the same size; false: decoration
};

/** A work position for Farida's schedule: where to stand and which way to face. Building-local and world. */
struct FTeaShopAnchor
{
    FName Id;
    FVector2D Local = FVector2D::ZeroVector;  // U, V
    FVector2D Facing = FVector2D::ZeroVector; // unit direction in U, V
    FVector World = FVector::ZeroVector;      // standing point on the finished floor (Z = GroundZ + 15)
    double WorldYawDegrees = 0.0;
};

/** A short walking line (staff or customer), building-local and world, for deterministic travel. */
struct FTeaShopRoute
{
    FName Id;
    TArray<FVector2D> Local;
    TArray<FVector> World;
};

namespace TeaShopData
{
// Furniture and fixtures: U0, U1, V0, V1, H0, H1 (H above the finished floor), solid. Rows are written out in full (east side
// mirrored by hand, U' = 700 - U) so that tests and the schedule planner read exactly what is built. Seats and counters that stand
// on the floor are above 45 cm (the player steps up 45), so nothing can be climbed.
inline constexpr FTeaShopPiece PieceTable[] = {
    // Customer room, west group: wall bench, low table, bench (V 70-250); east group is its mirror.
    {ELaneColour::DoorWood, 27.0, 67.0, 70.0, 250.0, 0.0, 47.0, true},
    {ELaneColour::WoodLight, 27.0, 67.0, 68.0, 252.0, 47.0, 50.0, false},
    {ELaneColour::DoorWood, 95.0, 125.0, 105.0, 215.0, 0.0, 50.0, true},
    {ELaneColour::WoodLight, 80.0, 140.0, 90.0, 230.0, 50.0, 58.0, true},
    {ELaneColour::DoorWood, 150.0, 190.0, 70.0, 250.0, 0.0, 47.0, true},
    {ELaneColour::WoodLight, 150.0, 190.0, 68.0, 252.0, 47.0, 50.0, false},
    {ELaneColour::PotTerracotta, 95.0, 103.0, 120.0, 128.0, 58.0, 66.0, false},
    {ELaneColour::PotTerracotta, 115.0, 123.0, 190.0, 198.0, 58.0, 66.0, false},
    {ELaneColour::DoorWood, 633.0, 673.0, 70.0, 250.0, 0.0, 47.0, true},
    {ELaneColour::WoodLight, 633.0, 673.0, 68.0, 252.0, 47.0, 50.0, false},
    {ELaneColour::DoorWood, 575.0, 605.0, 105.0, 215.0, 0.0, 50.0, true},
    {ELaneColour::WoodLight, 560.0, 620.0, 90.0, 230.0, 50.0, 58.0, true},
    {ELaneColour::DoorWood, 510.0, 550.0, 70.0, 250.0, 0.0, 47.0, true},
    {ELaneColour::WoodLight, 510.0, 550.0, 68.0, 252.0, 47.0, 50.0, false},
    {ELaneColour::PotTerracotta, 597.0, 605.0, 120.0, 128.0, 58.0, 66.0, false},
    {ELaneColour::PotTerracotta, 577.0, 585.0, 190.0, 198.0, 58.0, 66.0, false},
    // Walls of the customer room: the chai menu board (west) and a calendar (east).
    {ELaneColour::GoodsYellow, 25.0, 28.0, 105.0, 215.0, 115.0, 165.0, false},
    {ELaneColour::TrimDark, 28.0, 29.0, 115.0, 205.0, 128.0, 133.0, false},
    {ELaneColour::TrimDark, 28.0, 29.0, 115.0, 205.0, 142.0, 147.0, false},
    {ELaneColour::TrimDark, 28.0, 29.0, 115.0, 205.0, 153.0, 158.0, false},
    {ELaneColour::GoodsRed, 672.0, 675.0, 140.0, 180.0, 120.0, 165.0, false},
    // The service counter: the staff boundary. Customer face at V 330, top 90. The side staff access is U 575-675.
    {ELaneColour::WoodLight, 25.0, 575.0, 332.0, 388.0, 0.0, 85.0, true},
    {ELaneColour::Concrete, 25.0, 575.0, 330.0, 390.0, 85.0, 90.0, true},
    {ELaneColour::GlassDusk, 90.0, 210.0, 345.0, 378.0, 90.0, 128.0, false},
    {ELaneColour::GoodsOrange, 240.0, 256.0, 350.0, 366.0, 90.0, 112.0, false},
    {ELaneColour::GoodsYellow, 262.0, 278.0, 350.0, 366.0, 90.0, 112.0, false},
    {ELaneColour::GoodsRed, 284.0, 300.0, 350.0, 366.0, 90.0, 108.0, false},
    {ELaneColour::Chrome, 340.0, 356.0, 355.0, 371.0, 90.0, 110.0, false},
    {ELaneColour::Chrome, 362.0, 378.0, 355.0, 371.0, 90.0, 110.0, false},
    {ELaneColour::DoorWood, 480.0, 520.0, 352.0, 376.0, 90.0, 102.0, false},
    // The staff access: a STAFF ONLY board hung 192-218 above the floor (clear of the head), over the tile threshold.
    {ELaneColour::SignRed, 585.0, 665.0, 354.0, 358.0, 192.0, 218.0, false},
    {ELaneColour::TrimLight, 595.0, 655.0, 353.0, 354.0, 200.0, 210.0, false},
    {ELaneColour::TrimDark, 590.0, 592.0, 355.0, 357.0, 218.0, 274.0, false},
    {ELaneColour::TrimDark, 658.0, 660.0, 355.0, 357.0, 218.0, 274.0, false},
    // Washing: a counter with a basin on the west wall (stand at U 160), shelves above it, a bucket behind.
    {ELaneColour::Concrete, 27.0, 110.0, 420.0, 560.0, 0.0, 82.0, true},
    {ELaneColour::TrimLight, 25.0, 112.0, 418.0, 562.0, 82.0, 88.0, true},
    {ELaneColour::TankBlack, 40.0, 100.0, 450.0, 530.0, 88.0, 91.0, false},
    {ELaneColour::Chrome, 100.0, 106.0, 485.0, 491.0, 88.0, 112.0, false},
    {ELaneColour::Rail, 45.0, 95.0, 535.0, 555.0, 88.0, 100.0, false},
    {ELaneColour::Chrome, 50.0, 60.0, 430.0, 440.0, 88.0, 100.0, false},
    {ELaneColour::Chrome, 70.0, 80.0, 430.0, 440.0, 88.0, 100.0, false},
    {ELaneColour::WoodLight, 25.0, 45.0, 420.0, 560.0, 135.0, 139.0, false},
    {ELaneColour::WoodLight, 25.0, 45.0, 420.0, 560.0, 170.0, 174.0, false},
    {ELaneColour::GoodsYellow, 28.0, 40.0, 430.0, 460.0, 139.0, 153.0, false},
    {ELaneColour::ClothTeal, 28.0, 40.0, 480.0, 520.0, 139.0, 150.0, false},
    {ELaneColour::ShutterBlue, 30.0, 60.0, 568.0, 598.0, 0.0, 30.0, false},
    // Preparation: the tea stove on a brick platform with kettle and milk pot (stand at V 478), a gas cylinder, the worktable.
    {ELaneColour::Concrete, 250.0, 430.0, 520.0, 596.0, 0.0, 78.0, true},
    {ELaneColour::TrimDark, 248.0, 432.0, 518.0, 598.0, 78.0, 82.0, true},
    {ELaneColour::TankBlack, 268.0, 340.0, 540.0, 575.0, 82.0, 86.0, false},
    {ELaneColour::TankBlack, 352.0, 412.0, 540.0, 575.0, 82.0, 86.0, false},
    {ELaneColour::Chrome, 285.0, 325.0, 548.0, 568.0, 86.0, 124.0, false},
    {ELaneColour::Chrome, 325.0, 340.0, 555.0, 561.0, 100.0, 112.0, false},
    {ELaneColour::AcUnit, 365.0, 395.0, 548.0, 568.0, 86.0, 106.0, false},
    {ELaneColour::GoodsRed, 440.0, 474.0, 545.0, 579.0, 0.0, 72.0, true},
    {ELaneColour::Chrome, 450.0, 464.0, 555.0, 569.0, 72.0, 80.0, false},
    {ELaneColour::WoodLight, 480.0, 560.0, 520.0, 590.0, 0.0, 78.0, true},
    {ELaneColour::WoodLight, 478.0, 562.0, 518.0, 592.0, 78.0, 84.0, true},
    {ELaneColour::GoodsYellow, 490.0, 510.0, 535.0, 555.0, 84.0, 92.0, false},
    {ELaneColour::GoodsGreen, 520.0, 535.0, 540.0, 555.0, 84.0, 90.0, false},
    {ELaneColour::ClothMustard, 540.0, 552.0, 560.0, 575.0, 84.0, 96.0, false},
    // Back room, storage: water drum, steel rack with tea, sugar and glasses, sacks, spare gas cylinders.
    {ELaneColour::ShutterBlue, 30.0, 90.0, 650.0, 710.0, 0.0, 95.0, true},
    {ELaneColour::TankLid, 34.0, 86.0, 654.0, 706.0, 95.0, 100.0, true},
    {ELaneColour::ShutterSteel, 330.0, 560.0, 835.0, 873.0, 0.0, 180.0, true},
    {ELaneColour::TrimDark, 332.0, 558.0, 826.0, 835.0, 44.0, 47.0, false},
    {ELaneColour::TrimDark, 332.0, 558.0, 826.0, 835.0, 88.0, 91.0, false},
    {ELaneColour::TrimDark, 332.0, 558.0, 826.0, 835.0, 132.0, 135.0, false},
    {ELaneColour::GoodsYellow, 340.0, 380.0, 827.0, 834.0, 47.0, 70.0, false},
    {ELaneColour::GoodsRed, 400.0, 440.0, 827.0, 834.0, 47.0, 66.0, false},
    {ELaneColour::GoodsGreen, 460.0, 520.0, 827.0, 834.0, 91.0, 112.0, false},
    {ELaneColour::ClothMustard, 345.0, 420.0, 827.0, 834.0, 135.0, 160.0, false},
    {ELaneColour::ClothMustard, 575.0, 670.0, 770.0, 872.0, 0.0, 66.0, true},
    {ELaneColour::ClothTeal, 590.0, 662.0, 790.0, 860.0, 66.0, 96.0, true},
    {ELaneColour::GoodsRed, 640.0, 672.0, 690.0, 722.0, 0.0, 72.0, true},
    {ELaneColour::GoodsRed, 640.0, 672.0, 728.0, 760.0, 0.0, 72.0, true},
    // Back room, rest corner: a cot with a mattress, a stool, a radio on a shelf, Farida's terracotta apron on its peg.
    {ELaneColour::DoorWood, 27.0, 117.0, 740.0, 870.0, 0.0, 40.0, true},
    {ELaneColour::ClothMagenta, 29.0, 115.0, 742.0, 868.0, 40.0, 50.0, true},
    {ELaneColour::TrimLight, 31.0, 111.0, 840.0, 866.0, 50.0, 58.0, false},
    {ELaneColour::ClothMustard, 31.0, 111.0, 745.0, 800.0, 50.0, 52.0, false},
    {ELaneColour::WoodLight, 200.0, 236.0, 770.0, 806.0, 0.0, 48.0, true},
    {ELaneColour::WoodLight, 25.0, 45.0, 650.0, 700.0, 150.0, 154.0, false},
    {ELaneColour::TankBlack, 28.0, 42.0, 660.0, 685.0, 154.0, 170.0, false},
    {ELaneColour::PotTerracotta, 25.0, 28.0, 712.0, 732.0, 125.0, 165.0, false},
    // Outdoor seating under the awning (footpath side, V -88 to 0): benches against the front wall, low tables in front.
    {ELaneColour::DoorWood, 70.0, 220.0, -38.0, -2.0, 0.0, 47.0, true},
    {ELaneColour::WoodLight, 70.0, 220.0, -40.0, 0.0, 47.0, 50.0, false},
    {ELaneColour::DoorWood, 110.0, 180.0, -78.0, -62.0, 0.0, 50.0, true},
    {ELaneColour::WoodLight, 80.0, 210.0, -88.0, -52.0, 50.0, 58.0, true},
    {ELaneColour::PotTerracotta, 120.0, 128.0, -75.0, -67.0, 58.0, 66.0, false},
    {ELaneColour::DoorWood, 480.0, 630.0, -38.0, -2.0, 0.0, 47.0, true},
    {ELaneColour::WoodLight, 480.0, 630.0, -40.0, 0.0, 47.0, 50.0, false},
    {ELaneColour::DoorWood, 520.0, 590.0, -78.0, -62.0, 0.0, 50.0, true},
    {ELaneColour::WoodLight, 490.0, 620.0, -88.0, -52.0, 50.0, 58.0, true},
    {ELaneColour::PotTerracotta, 540.0, 548.0, -75.0, -67.0, 58.0, 66.0, false},
};

struct FTeaShopAnchorSpec
{
    const TCHAR* Id;
    double U;
    double V;
    double FaceU;
    double FaceV;
};

// Stand points. Each keeps 40 cm or more from every solid piece (capsule radius 34). Facing is the direction of the work.
inline constexpr FTeaShopAnchorSpec AnchorTable[] = {
    {TeaShopAnchorCounter, 300.0, 437.0, 0.0, -1.0},    // behind the counter, facing the customers
    {TeaShopAnchorStove, 340.0, 478.0, 0.0, 1.0},       // in front of the stove platform
    {TeaShopAnchorWash, 160.0, 490.0, -1.0, 0.0},       // facing the basin counter
    {TeaShopAnchorStorage, 440.0, 790.0, 0.0, 1.0},     // in front of the steel rack
    {TeaShopAnchorBenchWipe, 110.0, 275.0, 0.0, -1.0},  // by the west table and benches (the same places repeat on the east side)
    {TeaShopAnchorRest, 160.0, 760.0, -1.0, 0.0},       // beside the cot, near the stool
    {TeaShopAnchorDoor, 350.0, -45.0, 0.0, 1.0},        // outside the entrance, facing in: unlocks and lifts the shutter
};

struct FTeaShopRouteSpec
{
    const TCHAR* Id;
    int32 Count;
    double Pts[8][2];
};

inline constexpr FTeaShopRouteSpec RouteTable[] = {
    {TEXT("route.shanti_chai.door_to_footpath"), 5, {{350.0, 90.0}, {350.0, 12.0}, {350.0, -45.0}, {350.0, -110.0}, {350.0, -180.0}}},
    {TEXT("route.shanti_chai.counter_to_door"), 6, {{300.0, 437.0}, {625.0, 437.0}, {625.0, 360.0}, {625.0, 290.0}, {350.0, 290.0}, {350.0, 90.0}}},
    {TEXT("route.shanti_chai.counter_to_stove"), 2, {{300.0, 437.0}, {340.0, 478.0}}},
    {TEXT("route.shanti_chai.counter_to_wash"), 2, {{300.0, 437.0}, {160.0, 490.0}}},
    {TEXT("route.shanti_chai.counter_to_storage"), 7, {{300.0, 437.0}, {625.0, 437.0}, {622.0, 520.0}, {622.0, 606.0}, {622.0, 650.0}, {440.0, 650.0}, {440.0, 790.0}}},
    {TEXT("route.shanti_chai.counter_to_rest"), 8, {{300.0, 437.0}, {625.0, 437.0}, {622.0, 606.0}, {622.0, 650.0}, {300.0, 650.0}, {160.0, 700.0}, {160.0, 730.0}, {160.0, 760.0}}},
    {TEXT("route.shanti_chai.counter_to_bench_wipe"), 6, {{300.0, 437.0}, {625.0, 437.0}, {625.0, 360.0}, {625.0, 290.0}, {110.0, 290.0}, {110.0, 275.0}}},
};
} // namespace TeaShopData

/** True for the one building that gets the tea shop interior (side +1, Index 3, X 600 to 1300, the footprint above). */
bool IsTeaShopBuilding(const FLaneBuilding& Building);

/**
 * Replaces the building's solid ground-floor volume: 25 cm walls with real openings (entrance, two barred hatch windows, a
 * back window), floor, ceiling, partition, furniture, awning, sign, ceiling lights, and the shutter door spec
 * (TeaShopShutterDoorId). Call INSTEAD of BuildLaneGroundFloor and AddWallVolume. Adds to Scene.Doors and Scene.RoomLights.
 */
void BuildTeaShop(FLaneScene& Scene, const FLaneBuilding& Building);

/** True once BuildTeaShop has added the shutter door spec to the scene. */
bool HasTeaShop(const FLaneScene& Scene);

/** The furniture and fixtures (solid pieces and decoration), identical for the build and for tests and planners. */
TConstArrayView<FTeaShopPiece> GetTeaShopPieces();

/** The seven work anchors, transformed by the building's frame. */
void GetTeaShopAnchors(const FLaneBuilding& Building, TArray<FTeaShopAnchor>& OutAnchors);

/**
 * Walking lines: route.shanti_chai.door_to_footpath (inside the door out to the footpath walking line) and
 * from the counter to the staff work places: counter_to_door, counter_to_stove, counter_to_wash, counter_to_storage,
 * counter_to_rest, counter_to_bench_wipe. Every point keeps at least 34 cm (the capsule radius) from solid pieces and walls.
 */
void GetTeaShopRoutes(const FLaneBuilding& Building, TArray<FTeaShopRoute>& OutRoutes);
} // namespace PWLane
