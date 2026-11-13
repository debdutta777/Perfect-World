#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldLaneBuildings.h"
#include "PerfectWorldLaneHomeFurniture.h"

namespace PWLane
{
/**
 * Nabapara Shishu Griha, the children's home: the lane's care interior (LANE_LIFE_PLAN.md C, "Institutional care needs a
 * genuinely resolved care opening"). A player who grew up in institutional care (Household Resources rank 0) starts here
 * and gets the same lighting, navigability and interaction care as the family home: personal sleeping and storage space,
 * shared study and eating space, and an available staff member. Ground floor only; the upper storeys stay a solid block so
 * the street facade is unchanged.
 *
 * Which building: the first-row building on the -Y side with Index 1, next door to the opening home. BuildFirstRows gives
 * side -1 the hash LaneHash(-124, 1, 11) = 408894755 (Side * 131 + 7 = -124), which yields X -1400 to -300 cm (the cursor
 * after the corner building X -2400 to -1400; Frame origin (-1400, -400), U = +X, V = -Y), Width 1100 (600 + 100 * (H % 6 = 5)),
 * Depth 900 (700 + 100 * ((H >> 16) % 3 = 2)), Setback 0 ((H >> 12) % 4 = 3, so no setback), 3 storeys of 300 cm
 * (3 + (H >> 8) % 3 = 3), wall colour WallButter ((H >> 20) % 8 = 5, differing from the previous building's 0), and no extra
 * gap after it ((H >> 24) % 7 = 3). The generic ground floor would give it open stalls (Mode = (1 + 1) % 3 = 2); the
 * integrator calls BuildCareHome INSTEAD of BuildLaneGroundFloor and AddWallVolume (it builds its own frontage, interior and
 * the solid storeys above, like BuildTeaShop). IsCareHomeBuilding checks the footprint too, so a generator change makes the
 * lane fall back to the old building. It is not the opening home (-Y, Index 2, X -300 to 700) nor the tea shop (+Y, Index 3).
 *
 * Local coordinates (as for the opening home): U along the frontage (world X), V from the front face into the building
 * (world -Y here), heights above the finished floor (W 15; the ceiling underside is W 289). Rooms, inside the 25 cm walls:
 *   Entry    U 25-500   V 25-501   staff desk, noticeboard, bag hooks, shoe rack, the reserved stair (U 320-500, V 25-315)
 *   Common   U 512-1075 V 25-501   two study tables under the front windows, the dining table, bookshelf, cubbies, water stand
 *   Dormitory U 25-814  V 513-875  five beds along the back wall, each with a locked trunk at its foot; bed 2 is the resident's
 *   Washroom U 826-1075 V 513-875  two basins, a toilet, a shower corner, towel hooks
 * Partitions are 12 cm: entry|common at U 506 (500-512), front rooms|back rooms at V 507 (501-513), dormitory|washroom at
 * U 820 (814-826). Doors are 94 cm clear between jambs (the leaf is 90) and 212 cm clear above the floor.
 */
inline constexpr const TCHAR* CareDoorFrontId = TEXT("door.nabapara.care_front");         // one-way inward, Private
inline constexpr const TCHAR* CareDoorDormitoryId = TEXT("door.nabapara.care_dormitory"); // two-way, Private
inline constexpr const TCHAR* CareDoorCommonId = TEXT("door.nabapara.care_common");       // starts open, Public
inline constexpr const TCHAR* CareDoorWashroomId = TEXT("door.nabapara.care_washroom");   // two-way, Public
inline constexpr uint32 CareGeneratorSeed = 408894755u; // FLaneBuilding::Seed of the building (LaneHash(-124, 1, 11))

/** A rectangle in building-local U, V (centimetres). */
struct FCareRect
{
    double U0;
    double U1;
    double V0;
    double V1;
};

enum class ECareRoom : uint8
{
    Entry,
    Common,
    Dormitory,
    Washroom
};

namespace CareLayout
{
inline constexpr double Width = 1100.0;
inline constexpr double Depth = 900.0;
inline constexpr double DoorHalfClear = 47.0;
inline constexpr double DoorClearHeight = 212.0;
inline constexpr double EntryEastU = 506.0;      // centre line of the partition between the entry and the common room
inline constexpr double AcrossV = 507.0;         // centre line of the partition between the front rooms and the back rooms
inline constexpr double WashWestU = 820.0;       // centre line of the partition between the dormitory and the washroom
inline constexpr double FrontDoorU = 235.0;      // centre of the front door along the frontage
inline constexpr double CommonDoorV = 435.0;     // centre of the entry|common doorway along its partition
inline constexpr double DormDoorU = 120.0;       // centre of the dormitory door in the across partition
inline constexpr double WashDoorU = 950.0;       // centre of the washroom door in the across partition
inline constexpr FCareRect Entry{25.0, 500.0, 25.0, 501.0};
inline constexpr FCareRect Common{512.0, 1075.0, 25.0, 501.0};
inline constexpr FCareRect Dormitory{25.0, 814.0, 513.0, 875.0};
inline constexpr FCareRect Washroom{826.0, 1075.0, 513.0, 875.0};
} // namespace CareLayout

/** One piece of furniture or fixture, in building-local U, V and heights above the finished floor. */
struct FCarePiece
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

/**
 * Where the care worker stands: in the common room, 120 cm from the entry|common doorway's threshold (U 506, V 435), turned
 * to face it. A player stepping through the doorway is well inside the 2 m reach and the 50 degree half-cone, and the spot is
 * clear of every piece of furniture by 88 cm. The common door's leaf lies in the entry (it swings into the entry only), so no
 * open leaf is ever between the two people. The staff desk in the entry is where the worker sits at other times.
 */
struct FCareStaffSpot
{
    double U = 0.0;
    double V = 0.0;
    double FaceU = 0.0; // the point they face: the centre of the doorway's threshold
    double FaceV = 0.0;
};

/** The resident's lockable trunk at the foot of their bed: where to stand to open it. */
struct FCareStorageSpec
{
    FName Id;
    const TCHAR* Verb = TEXT("");
    double U = 0.0; // centre of the trunk's footprint
    double V = 0.0;
    double HeightCm = 0.0; // top of the lid
    double StandU = 0.0;
    double StandV = 0.0;
    double StandClearCm = 0.0;
};

/** Something personal to the resident, one per room (the same attention the family home gives its personal objects). */
struct FCarePersonalObject
{
    const TCHAR* Id;
    ECareRoom Room;
    double U;
    double V;
};

namespace CareData
{
// Furniture and fixtures: U0, U1, V0, V1, H0, H1 (H above the finished floor), solid. Solid pieces above 45 cm cannot be climbed;
// the toilet (40) and the bucket (30) are the low exceptions, as in the family home. Tables are a top slab on four legs.
inline constexpr FCarePiece PieceTable[] = {
    // Entry: staff desk with a pedestal either side, the staff chair, the shoe rack under the bag hooks
    {ELaneColour::DoorWood, 27.0, 87.0, 250.0, 300.0, 0.0, 70.0, true},
    {ELaneColour::DoorWood, 27.0, 87.0, 330.0, 380.0, 0.0, 70.0, true},
    {ELaneColour::WoodLight, 27.0, 87.0, 250.0, 380.0, 70.0, 76.0, true},
    {ELaneColour::DoorWood, 95.0, 133.0, 295.0, 333.0, 0.0, 45.0, true},
    {ELaneColour::DoorWood, 129.0, 133.0, 295.0, 333.0, 45.0, 90.0, true},
    {ELaneColour::WoodLight, 28.0, 62.0, 28.0, 95.0, 0.0, 50.0, true},
    // Entry: decoration. Desk top (ledger, lamp, pen cup, bell), the key box, shoes, the bag hooks and the resident's school bag, the noticeboard, the clock, the doormat
    {ELaneColour::TrimDark, 40.0, 70.0, 270.0, 300.0, 76.0, 80.0, false},
    {ELaneColour::Chrome, 60.0, 70.0, 345.0, 355.0, 76.0, 80.0, false},
    {ELaneColour::GoodsYellow, 55.0, 75.0, 340.0, 360.0, 80.0, 98.0, false},
    {ELaneColour::GoodsGreen, 72.0, 80.0, 320.0, 328.0, 76.0, 86.0, false},
    {ELaneColour::Chrome, 45.0, 55.0, 355.0, 365.0, 76.0, 82.0, false},
    {ELaneColour::TrimDark, 25.0, 28.0, 300.0, 330.0, 120.0, 150.0, false},
    {ELaneColour::Chrome, 28.0, 30.0, 308.0, 312.0, 128.0, 142.0, false},
    {ELaneColour::Chrome, 28.0, 30.0, 318.0, 322.0, 128.0, 142.0, false},
    {ELaneColour::GoodsRed, 34.0, 46.0, 36.0, 58.0, 50.0, 57.0, false},
    {ELaneColour::TrimDark, 34.0, 46.0, 64.0, 86.0, 50.0, 57.0, false},
    {ELaneColour::DoorWood, 25.0, 30.0, 30.0, 90.0, 148.0, 158.0, false},
    {ELaneColour::Chrome, 30.0, 34.0, 38.0, 42.0, 150.0, 154.0, false},
    {ELaneColour::Chrome, 30.0, 34.0, 76.0, 80.0, 150.0, 154.0, false},
    {ELaneColour::ClothTeal, 30.0, 42.0, 46.0, 76.0, 98.0, 152.0, false},
    {ELaneColour::DoorWood, 25.0, 28.0, 98.0, 212.0, 108.0, 172.0, false},
    {ELaneColour::WoodLight, 28.0, 29.0, 102.0, 208.0, 112.0, 168.0, false},
    {ELaneColour::TrimLight, 29.0, 30.0, 108.0, 140.0, 130.0, 160.0, false},
    {ELaneColour::TrimLight, 29.0, 30.0, 146.0, 176.0, 138.0, 162.0, false},
    {ELaneColour::GoodsYellow, 29.0, 30.0, 182.0, 204.0, 118.0, 146.0, false},
    {ELaneColour::GoodsRed, 29.0, 30.0, 124.0, 127.0, 156.0, 159.0, false},
    {ELaneColour::GoodsRed, 29.0, 30.0, 160.0, 163.0, 158.0, 161.0, false},
    {ELaneColour::TrimLight, 497.0, 500.0, 195.0, 225.0, 190.0, 220.0, false},
    {ELaneColour::TrimDark, 496.0, 500.0, 208.0, 212.0, 203.0, 206.0, false},
    {ELaneColour::ClothMustard, 205.0, 265.0, 35.0, 75.0, 0.0, 1.0, false},
    // Common room: study tables T1 and T2 under the front windows with a bench each, their books
    {ELaneColour::WoodLight, 535.0, 665.0, 28.0, 93.0, 70.0, 76.0, true},
    {ELaneColour::DoorWood, 537.0, 543.0, 30.0, 36.0, 0.0, 70.0, true},
    {ELaneColour::DoorWood, 537.0, 543.0, 85.0, 91.0, 0.0, 70.0, true},
    {ELaneColour::DoorWood, 657.0, 663.0, 30.0, 36.0, 0.0, 70.0, true},
    {ELaneColour::DoorWood, 657.0, 663.0, 85.0, 91.0, 0.0, 70.0, true},
    {ELaneColour::DoorWood, 540.0, 660.0, 98.0, 126.0, 0.0, 47.0, true},
    {ELaneColour::WoodLight, 690.0, 820.0, 28.0, 93.0, 70.0, 76.0, true},
    {ELaneColour::DoorWood, 692.0, 698.0, 30.0, 36.0, 0.0, 70.0, true},
    {ELaneColour::DoorWood, 692.0, 698.0, 85.0, 91.0, 0.0, 70.0, true},
    {ELaneColour::DoorWood, 812.0, 818.0, 30.0, 36.0, 0.0, 70.0, true},
    {ELaneColour::DoorWood, 812.0, 818.0, 85.0, 91.0, 0.0, 70.0, true},
    {ELaneColour::DoorWood, 695.0, 815.0, 98.0, 126.0, 0.0, 47.0, true},
    {ELaneColour::GoodsRed, 570.0, 596.0, 50.0, 76.0, 76.0, 78.0, false},
    {ELaneColour::ShutterBlue, 600.0, 622.0, 40.0, 52.0, 76.0, 82.0, false},
    {ELaneColour::GoodsGreen, 715.0, 741.0, 45.0, 70.0, 76.0, 79.0, false},
    {ELaneColour::GoodsYellow, 760.0, 786.0, 50.0, 75.0, 76.0, 79.0, false},
    // Common room: the dining table, its two benches, what is on it, the rug
    {ELaneColour::ClothTeal, 790.0, 1030.0, 205.0, 385.0, 0.0, 1.0, false},
    {ELaneColour::WoodLight, 800.0, 1020.0, 252.0, 332.0, 70.0, 76.0, true},
    {ELaneColour::DoorWood, 802.0, 808.0, 254.0, 260.0, 0.0, 70.0, true},
    {ELaneColour::DoorWood, 802.0, 808.0, 324.0, 330.0, 0.0, 70.0, true},
    {ELaneColour::DoorWood, 1012.0, 1018.0, 254.0, 260.0, 0.0, 70.0, true},
    {ELaneColour::DoorWood, 1012.0, 1018.0, 324.0, 330.0, 0.0, 70.0, true},
    {ELaneColour::DoorWood, 805.0, 1015.0, 222.0, 248.0, 0.0, 47.0, true},
    {ELaneColour::DoorWood, 805.0, 1015.0, 336.0, 362.0, 0.0, 47.0, true},
    {ELaneColour::Chrome, 830.0, 850.0, 275.0, 295.0, 76.0, 80.0, false},
    {ELaneColour::GoodsGreen, 880.0, 900.0, 280.0, 300.0, 76.0, 86.0, false},
    {ELaneColour::GlassDusk, 940.0, 956.0, 285.0, 301.0, 76.0, 96.0, false},
    // Common room: the bookshelf on the partition, the cubby rack with the resident's own cubby, the water stand, the blackboard, the timetable
    {ELaneColour::WoodLight, 514.0, 540.0, 130.0, 230.0, 0.0, 150.0, true},
    {ELaneColour::GoodsRed, 540.0, 542.0, 134.0, 150.0, 40.0, 140.0, false},
    {ELaneColour::GoodsGreen, 540.0, 542.0, 153.0, 176.0, 40.0, 120.0, false},
    {ELaneColour::GoodsYellow, 540.0, 542.0, 180.0, 226.0, 60.0, 140.0, false},
    {ELaneColour::WoodLight, 1037.0, 1073.0, 90.0, 220.0, 0.0, 160.0, true},
    {ELaneColour::TrimDark, 1035.0, 1037.0, 94.0, 122.0, 100.0, 150.0, false},
    {ELaneColour::TrimDark, 1035.0, 1037.0, 94.0, 122.0, 40.0, 90.0, false},
    {ELaneColour::TrimDark, 1035.0, 1037.0, 125.0, 153.0, 100.0, 150.0, false},
    {ELaneColour::TrimDark, 1035.0, 1037.0, 125.0, 153.0, 40.0, 90.0, false},
    {ELaneColour::TrimDark, 1035.0, 1037.0, 156.0, 184.0, 100.0, 150.0, false},
    {ELaneColour::TrimDark, 1035.0, 1037.0, 156.0, 184.0, 40.0, 90.0, false},
    {ELaneColour::TrimDark, 1035.0, 1037.0, 187.0, 215.0, 100.0, 150.0, false},
    {ELaneColour::TrimDark, 1035.0, 1037.0, 187.0, 215.0, 40.0, 90.0, false},
    {ELaneColour::ClothMustard, 1030.0, 1037.0, 128.0, 150.0, 42.0, 60.0, false},
    {ELaneColour::TrimLight, 1036.0, 1038.0, 130.0, 150.0, 91.0, 96.0, false},
    {ELaneColour::GoodsGreen, 1031.0, 1037.0, 97.0, 119.0, 102.0, 118.0, false},
    {ELaneColour::GoodsRed, 1031.0, 1037.0, 159.0, 181.0, 102.0, 120.0, false},
    {ELaneColour::ShutterBlue, 1031.0, 1037.0, 190.0, 212.0, 42.0, 58.0, false},
    {ELaneColour::Concrete, 1040.0, 1073.0, 385.0, 425.0, 0.0, 90.0, true},
    {ELaneColour::PotTerracotta, 1048.0, 1068.0, 391.0, 411.0, 90.0, 120.0, false},
    {ELaneColour::Chrome, 1044.0, 1048.0, 399.0, 403.0, 94.0, 100.0, false},
    {ELaneColour::GoodsYellow, 1052.0, 1058.0, 413.0, 419.0, 90.0, 96.0, false},
    {ELaneColour::WoodLight, 880.0, 1010.0, 25.0, 28.0, 100.0, 180.0, false},
    {ELaneColour::TankBlack, 884.0, 1006.0, 25.0, 27.0, 104.0, 176.0, false},
    {ELaneColour::TrimLight, 900.0, 980.0, 25.0, 26.0, 150.0, 153.0, false},
    {ELaneColour::TrimLight, 1073.0, 1075.0, 240.0, 330.0, 110.0, 170.0, false},
    {ELaneColour::GoodsRed, 1073.0, 1075.0, 255.0, 285.0, 125.0, 155.0, false},
    // Dormitory: five beds along the back wall, each with a locked trunk at its foot; bed 2 (U 205) is the resident's
    {ELaneColour::DoorWood, 45.0, 135.0, 683.0, 873.0, 0.0, 40.0, true},
    {ELaneColour::ClothMagenta, 47.0, 133.0, 685.0, 871.0, 40.0, 48.0, true},
    {ELaneColour::ShutterSteel, 65.0, 115.0, 640.0, 680.0, 0.0, 50.0, true},
    {ELaneColour::DoorWood, 205.0, 295.0, 683.0, 873.0, 0.0, 40.0, true},
    {ELaneColour::ClothMustard, 207.0, 293.0, 685.0, 871.0, 40.0, 48.0, true},
    {ELaneColour::ShutterGreen, 225.0, 275.0, 640.0, 680.0, 0.0, 50.0, true},
    {ELaneColour::DoorWood, 395.0, 485.0, 683.0, 873.0, 0.0, 40.0, true},
    {ELaneColour::ClothTeal, 397.0, 483.0, 685.0, 871.0, 40.0, 48.0, true},
    {ELaneColour::ShutterSteel, 415.0, 465.0, 640.0, 680.0, 0.0, 50.0, true},
    {ELaneColour::DoorWood, 555.0, 645.0, 683.0, 873.0, 0.0, 40.0, true},
    {ELaneColour::ClothMagenta, 557.0, 643.0, 685.0, 871.0, 40.0, 48.0, true},
    {ELaneColour::ShutterBlue, 575.0, 625.0, 640.0, 680.0, 0.0, 50.0, true},
    {ELaneColour::DoorWood, 715.0, 805.0, 683.0, 873.0, 0.0, 40.0, true},
    {ELaneColour::ClothTeal, 717.0, 803.0, 685.0, 871.0, 40.0, 48.0, true},
    {ELaneColour::ShutterSteel, 735.0, 785.0, 640.0, 680.0, 0.0, 50.0, true},
    // Dormitory: pillows and folded blankets, the clothes and things that make each bed someone's
    {ELaneColour::TrimLight, 53.0, 127.0, 836.0, 870.0, 48.0, 56.0, false},
    {ELaneColour::ClothMustard, 51.0, 129.0, 690.0, 720.0, 48.0, 53.0, false},
    {ELaneColour::Chrome, 85.0, 95.0, 637.0, 640.0, 30.0, 38.0, false},
    {ELaneColour::TrimLight, 213.0, 287.0, 836.0, 870.0, 48.0, 56.0, false},
    {ELaneColour::ClothMagenta, 211.0, 289.0, 690.0, 720.0, 48.0, 53.0, false},
    {ELaneColour::TrimLight, 403.0, 477.0, 836.0, 870.0, 48.0, 56.0, false},
    {ELaneColour::ClothMustard, 401.0, 479.0, 690.0, 720.0, 48.0, 53.0, false},
    {ELaneColour::Chrome, 435.0, 445.0, 637.0, 640.0, 30.0, 38.0, false},
    {ELaneColour::TrimLight, 563.0, 637.0, 836.0, 870.0, 48.0, 56.0, false},
    {ELaneColour::ClothMustard, 561.0, 639.0, 690.0, 720.0, 48.0, 53.0, false},
    {ELaneColour::Chrome, 595.0, 605.0, 637.0, 640.0, 30.0, 38.0, false},
    {ELaneColour::TrimLight, 723.0, 797.0, 836.0, 870.0, 48.0, 56.0, false},
    {ELaneColour::ClothMustard, 721.0, 799.0, 690.0, 720.0, 48.0, 53.0, false},
    {ELaneColour::Chrome, 755.0, 765.0, 637.0, 640.0, 30.0, 38.0, false},
    {ELaneColour::GoodsOrange, 59.0, 75.0, 846.0, 862.0, 56.0, 70.0, false},
    {ELaneColour::GoodsYellow, 420.0, 444.0, 700.0, 712.0, 53.0, 56.0, false},
    {ELaneColour::ClothTeal, 575.0, 610.0, 692.0, 716.0, 53.0, 80.0, false},
    {ELaneColour::TankBlack, 735.0, 765.0, 842.0, 862.0, 56.0, 72.0, false},
    // Dormitory: the resident's own things. Nightstand with a tin box and a photograph, the trunk straps and padlock
    {ELaneColour::WoodLight, 297.0, 325.0, 840.0, 872.0, 0.0, 50.0, true},
    {ELaneColour::GoodsRed, 300.0, 314.0, 850.0, 866.0, 50.0, 58.0, false},
    {ELaneColour::DoorWood, 316.0, 324.0, 852.0, 856.0, 50.0, 64.0, false},
    {ELaneColour::TrimLight, 317.0, 323.0, 851.0, 852.0, 52.0, 62.0, false},
    {ELaneColour::TrimDark, 232.0, 238.0, 640.0, 680.0, 50.0, 52.0, false},
    {ELaneColour::TrimDark, 262.0, 268.0, 640.0, 680.0, 50.0, 52.0, false},
    {ELaneColour::Chrome, 244.0, 256.0, 637.0, 640.0, 28.0, 40.0, false},
    {ELaneColour::WoodLight, 250.0, 450.0, 513.0, 525.0, 150.0, 153.0, false},
    {ELaneColour::GoodsGreen, 260.0, 300.0, 515.0, 525.0, 153.0, 170.0, false},
    {ELaneColour::GoodsYellow, 330.0, 360.0, 515.0, 525.0, 153.0, 168.0, false},
    {ELaneColour::SignGreen, 180.0, 200.0, 513.0, 515.0, 130.0, 150.0, false},
    {ELaneColour::TrimLight, 186.0, 194.0, 515.0, 516.0, 138.0, 142.0, false},
    // Washroom: two basins on the east wall with mirrors, the resident's tumbler and toothbrush, a toilet, a shower corner, the towel hooks
    {ELaneColour::AcUnit, 1050.0, 1070.0, 600.0, 620.0, 0.0, 78.0, true},
    {ELaneColour::AcUnit, 1035.0, 1073.0, 590.0, 630.0, 78.0, 90.0, true},
    {ELaneColour::AcUnit, 1050.0, 1070.0, 660.0, 680.0, 0.0, 78.0, true},
    {ELaneColour::AcUnit, 1035.0, 1073.0, 650.0, 690.0, 78.0, 90.0, true},
    {ELaneColour::Chrome, 1073.0, 1075.0, 595.0, 625.0, 120.0, 165.0, false},
    {ELaneColour::Chrome, 1073.0, 1075.0, 655.0, 685.0, 120.0, 165.0, false},
    {ELaneColour::GoodsGreen, 1040.0, 1048.0, 610.0, 618.0, 90.0, 100.0, false},
    {ELaneColour::TrimLight, 1042.0, 1043.0, 596.0, 597.0, 90.0, 106.0, false},
    {ELaneColour::GoodsYellow, 1042.0, 1052.0, 668.0, 676.0, 90.0, 94.0, false},
    {ELaneColour::AcUnit, 870.0, 910.0, 800.0, 840.0, 0.0, 40.0, true},
    {ELaneColour::AcUnit, 868.0, 912.0, 845.0, 873.0, 40.0, 95.0, true},
    {ELaneColour::Chrome, 1071.0, 1074.0, 832.0, 835.0, 100.0, 200.0, false},
    {ELaneColour::Chrome, 1058.0, 1074.0, 827.0, 839.0, 198.0, 203.0, false},
    {ELaneColour::ShutterBlue, 1035.0, 1065.0, 800.0, 830.0, 0.0, 30.0, true},
    {ELaneColour::GoodsRed, 1045.0, 1055.0, 810.0, 820.0, 30.0, 38.0, false},
    {ELaneColour::DoorWood, 826.0, 829.0, 580.0, 760.0, 150.0, 154.0, false},
    {ELaneColour::GoodsRed, 829.0, 835.0, 590.0, 620.0, 100.0, 150.0, false},
    {ELaneColour::ClothTeal, 829.0, 835.0, 630.0, 660.0, 100.0, 150.0, false},
    {ELaneColour::GoodsGreen, 829.0, 835.0, 670.0, 700.0, 100.0, 150.0, false},
    {ELaneColour::ClothMustard, 829.0, 835.0, 710.0, 740.0, 100.0, 150.0, false},
};

inline constexpr FCarePersonalObject PersonalTable[] = {
    {TEXT("personal.nabapara.care_school_bag"), ECareRoom::Entry, 36.0, 61.0},         // the bag on its hook above the shoe rack
    {TEXT("personal.nabapara.care_exercise_books"), ECareRoom::Common, 1033.0, 139.0}, // the resident's own cubby
    {TEXT("personal.nabapara.care_tin_box_photo"), ECareRoom::Dormitory, 307.0, 858.0}, // tin box and photograph on the nightstand
    {TEXT("personal.nabapara.care_tumbler"), ECareRoom::Washroom, 1044.0, 614.0},      // tumbler and toothbrush on the basin
};

// Anchors, as rows; the CareSeatSpecs, CareReadableSpecs and CareLightSpecs functions turn them into the home's spec types.
struct FCareSeatRow
{
    const TCHAR* Id;
    const TCHAR* Verb;
    double SeatU;
    double SeatV;
    double SeatHeightCm;
    double FaceU;
    double FaceV;
    double StandU;
    double StandV;
};

struct FCareReadableRow
{
    const TCHAR* Id;
    const TCHAR* ContentId;
    const TCHAR* Verb;
    double U;
    double V;
    double HeightCm;
    double StandU;
    double StandV;
};

struct FCareLightRow
{
    const TCHAR* Room;
    double U;
    double V;
    float AttenuationCm;
    float Lumens;
    float ColourTempK;
    const FCareRect* RoomRect;
};

// Stand points keep 45 cm or more from every solid piece, wall and door swing (the tests measure it).
inline constexpr FCareSeatRow SeatTable[] = {
    // The resident's bed (bed 2, U 205-295): sit on its east edge facing the gap between beds; stand up into the gap.
    {TEXT("seat.nabapara.care_bed"), TEXT("Sit on your bed"), 265.0, 740.0, 48.0, 1.0, 0.0, 345.0, 740.0},
    // The bench at study table 1 under the front window, facing the table (-V); stand up backwards into the aisle.
    {TEXT("seat.nabapara.care_study_bench"), TEXT("Sit at the study table"), 600.0, 112.0, 47.0, 0.0, -1.0, 600.0, 175.0},
    // The dining table's aisle-side bench, facing the table (-V); stand up into the aisle.
    {TEXT("seat.nabapara.care_dining_bench"), TEXT("Sit at the dining table"), 850.0, 349.0, 47.0, 0.0, -1.0, 850.0, 410.0},
};

inline constexpr FCareReadableRow ReadableTable[] = {
    // The cork noticeboard on the entry's west wall (a wall sheet), and the resident's exercise book on study table 1.
    {TEXT("readable.nabapara.care_noticeboard"), TEXT("text.nabapara.care_noticeboard"), TEXT("Read the noticeboard"), 28.0, 155.0, 140.0, 100.0, 155.0},
    {TEXT("readable.nabapara.care_exercise_book"), TEXT("text.nabapara.care_exercise_book"), TEXT("Read your exercise book"), 583.0, 63.0, 78.0, 590.0, 175.0},
};

inline constexpr FHomeFanSpec FanTable[] = {
    {200.0, 300.0, 70.0, 240.0}, // entry
    {640.0, 190.0, 70.0, 240.0}, // common room, over the study tables
    {920.0, 190.0, 70.0, 240.0}, // common room, over the dining table
    {300.0, 700.0, 70.0, 240.0}, // dormitory, west half
    {640.0, 700.0, 70.0, 240.0}, // dormitory, east half
};

// One shadow-casting light per fitting, each reaching no farther than its room's shorter side; the shadows and the 12 and 25 cm
// walls keep it in the room. Warm tungsten everywhere except the washroom's cooler work light.
inline constexpr FCareLightRow LightTable[] = {
    {TEXT("entry"), 360.0, 380.0, 330.0f, 900.0f, 2900.0f, &CareLayout::Entry},
    {TEXT("common"), 640.0, 330.0, 340.0f, 900.0f, 2900.0f, &CareLayout::Common},
    {TEXT("common"), 900.0, 420.0, 340.0f, 900.0f, 2900.0f, &CareLayout::Common},
    {TEXT("dormitory"), 130.0, 620.0, 330.0f, 800.0f, 2900.0f, &CareLayout::Dormitory},
    {TEXT("dormitory"), 470.0, 640.0, 330.0f, 800.0f, 2900.0f, &CareLayout::Dormitory},
    {TEXT("dormitory"), 730.0, 620.0, 330.0f, 800.0f, 2900.0f, &CareLayout::Dormitory},
    {TEXT("washroom"), 950.0, 690.0, 225.0f, 700.0f, 4000.0f, &CareLayout::Washroom},
};

inline constexpr FCareStaffSpot StaffSpot{600.0, 360.0, CareLayout::EntryEastU, CareLayout::CommonDoorV};
} // namespace CareData

/** True for the one building that gets the care interior (side -1, Index 1, X -1400 to -300, the footprint above). */
bool IsCareHomeBuilding(const FLaneBuilding& Building);

/**
 * Replaces the building's solid ground-floor volume: 25 cm walls with real openings (front door, three windows, two back
 * windows and a ventilator), floor, ceiling, partitions and doorways for four rooms, furniture, fans, ceiling lights, the
 * stair reservation's floor patch, the street dressing and the four door specs. Call INSTEAD of BuildLaneGroundFloor and
 * AddWallVolume. Adds to Scene.Doors and Scene.RoomLights. OutSolids, when given, receives every solid piece's footprint.
 */
void BuildCareHome(FLaneScene& Scene, const FLaneBuilding& Building, TArray<FHomeSolid>* OutSolids = nullptr);

/** True once BuildCareHome has added the front door spec to the scene. */
bool HasCareHome(const FLaneScene& Scene);

/** The furniture and fixtures (solid pieces and decoration), identical for the build and for tests. */
TConstArrayView<FCarePiece> GetCarePieces();
TConstArrayView<FCarePersonalObject> GetCarePersonalObjects();

FCareStaffSpot CareStaffSpotLocal();
/** The care worker's feet position (finished floor level) in the world, for the given building frame. */
FVector CareStaffSpot(const FLaneFrame& Frame);
/** Actor yaw in degrees so the actor's forward (+X) faces the doorway threshold. A mesh whose forward is not +X needs an offset. */
double CareStaffSpotYaw(const FLaneFrame& Frame);

/** The resident's bed edge, the study bench and the dining bench (same shape as the home's seat specs). */
TArray<FHomeSeatSpec> CareSeatSpecs();
/** The noticeboard in the entry (on the wall) and the resident's exercise book on a study table (on a surface). */
TArray<FHomeReadableSpec> CareReadableSpecs();
FCareStorageSpec CareStorageSpec();
/** The reserved upper-floor stair in the entry: the home's dog-leg shape (180 x 290), kept clear and marked by a floor patch. */
FHomeStairSpec CareStairSpec();
TArray<FHomeFanSpec> CareFanSpecs();
/** Shadow-casting, room-contained lights (see FHomeLightSpec); the integrator spawns them from these. */
TArray<FHomeLightSpec> CareLightSpecs();

/** A building-local point (U, V, height above the finished floor) in the world. */
FVector CareWorldPoint(const FLaneFrame& Frame, double U, double V, double HeightCm = 0.0);
} // namespace PWLane
