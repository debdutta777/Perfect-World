#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldLaneKitFrontages.h"

namespace PerfectWorld
{
struct DoorDef;
}

namespace PWLane
{
/**
 * R4-01/R53/R61 production face of the seven unstaffed browsing shops: the kit's furnished
 * GeneralGoods/Workshop interior (PerfectWorldLaneKitFrontages.h) plus the two things the catalogue row alone does not give them.
 *
 *  1. The open stalls of the generic ground floor (BuildLaneGroundFloor, PerfectWorldLaneShops.cpp). The kit replaces that ground floor
 *     with a 25 cm wall, windows and a doorway, so a niche cannot be cut into the wall (the sales room is behind it). Each stall the
 *     generic builder would have drawn open is therefore brought FORWARD by StallRecessDepth onto the footpath: the same shelves, goods,
 *     cloth-topped counter, crates, bulb and striped awning, with the same numbers and the same hashes, but the back of the stall is
 *     the building's front face instead of a niche back. The awning is held by stay rods, never poles (poles would leave less than
 *     120 cm between them and the kerb). R61 scales the corner's 92 cm stall projection to 90 cm for its 60 cm setback,
 *     in visuals and collision. A stall that would cover the entrance approach (the doorway's centre +- ApproachHalfCm) is
 *     shifted to a side where it fits, or trimmed to the side remnant of at least StallMinWidthCm, or dropped: PlanKitStalls.
 *     Counters and crates carry collision of exactly their size; nothing else of the stall does (the shelves are out of reach behind
 *     the counter). Nothing is added inside the wall line, so no stall piece can meet the interior furniture or seal the doorway.
 *
 *  2. A posted board on the door canopy: "Browsing hours", the opening windows of the front door's core DoorDef (PostedHoursText) and
 *     "Unstaffed. Purchasing unavailable." The text is drawn by APWShopHoursSignActor (PerfectWorldShopDisplayActor.h) from the same
 *     string; the board behind it is voxel blocks added here. The browse interactable inside is APWShopBrowseActor, spawned by
 *     SpawnKitShopDisplays (PerfectWorldLaneKitDisplays.h).
 *
 * Integration: BuildFull calls BuildKitPublicFace right after BuildKitFrontage. BuildKitFrontage itself is untouched, so its collision
 * contract (the street face adds none) still holds; the public face adds its own boxes.
 */

/** One open stall on a frontage: its span along the frontage and the seed its goods are drawn from (the generic bay's hash). */
struct FKitStallBay
{
    double U0 = 0.0;
    double U1 = 0.0;
    uint32 Seed = 0u;
    bool bMoved = false; // true when it was shifted or trimmed to keep the entrance approach clear
};

namespace KitPublic
{
constexpr double ApproachHalfCm = 90.0; // a stall never stands within this of the doorway's centre (the generic door approach)
constexpr double StallMinWidthCm = 120.0;
constexpr double StallEdgeCm = 40.0;    // the generic pillar: the first and last stall start this far in, and stalls keep this apart

// The hours board stands on the door canopy of KfBuildFront: the canopy spans the doorway's centre +- 74 (its top is 242, its front edge
// 38 out), so a board 2 x 72 wide plus its 2 cm end rails sits wholly on it and clear of the stall awnings either side of the approach.
constexpr double SignHalfWidthCm = 72.0;
constexpr double SignFaceV = -38.0;
constexpr double SignBottomZ = 242.0;
constexpr double SignTopZ = 282.0;
constexpr float SignTextSizeCm = 7.0f; // 34 characters at this size are about 125 cm wide, inside the 144 cm board

constexpr double BrowseReachCm = 35.0;  // the browse interaction point is this far from the stand anchor, toward what it faces
constexpr double BrowseHeightCm = 95.0; // ... and this high above the finished floor

inline constexpr const TCHAR* UnstaffedPhrase = TEXT("Unstaffed. Purchasing unavailable.");
inline constexpr const TCHAR* BrowseVerb = TEXT("Browse goods");
} // namespace KitPublic

/** The seven catalogue buildings this applies to, in stable order. The staffed hardware store and repair workshop are excluded. */
TArray<FName> UnstaffedBrowseShopIds();
bool IsUnstaffedBrowseShop(FName BuildingId);

/** The open stall bays the generic ground floor (BuildLaneGroundFloor) draws for this building, in increasing U. Empty for shutters or a house door. */
void GetGenericStallBays(const FLaneBuilding& Building, TArray<FKitStallBay>& OutBays);

/** The bays the frontage keeps, shifted, trimmed or dropped around the entrance approach at DoorU, in increasing U. */
void PlanKitStalls(const FLaneBuilding& Building, double DoorU, TArray<FKitStallBay>& OutBays);

/** Collision boxes one stall adds: its counter and its crates. */
int32 KitStallCollisionBoxCount(const FKitStallBay& Bay);

/** "09:00-13:00 and 15:00-19:30" from the opening windows of a BusinessHours door (the legacy single window when there are none); empty for any other door. */
FString PostedHoursText(const PerfectWorld::DoorDef& Door);

/** The posted board: "Browsing hours", the hours line and the unstaffed phrase, one per line. Empty when the door has no business hours. */
FString PostedSignText(const PerfectWorld::DoorDef& Door);

/** The browse notice: what is on display, the hours line and the unstaffed phrase, one per line. Empty when the door has no business hours. */
FString BrowseNoticeText(const PerfectWorld::DoorDef& Door);

/** "interaction.nabapara.<name>.browse" for "building.nabapara.<name>"; NAME_None for any other id. The acceptance binding contract. */
FName BrowseActorTag(FName BuildingId);

/** World location and yaw (the text faces the footpath) of the posted text for a frontage whose doorway is centred at DoorU. */
FVector KitSignLocation(const FLaneBuilding& Building, double DoorU);
double KitSignYaw(const FLaneBuilding& Building);

/** The stalls and the hours board of a recognised kit frontage; does nothing unless it is one of the unstaffed browse shops with a built layout. */
void BuildKitPublicFace(FLaneScene& Scene, const FLaneBuilding& Building, const FLaneKitFrontage& Frontage);
} // namespace PWLane
