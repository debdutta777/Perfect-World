#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldLaneKitFrontages.h"

class UWorld;

namespace PWLane
{
/**
 * Spawns the production actors of the seven unstaffed browsing shops (R4-01/R53/R61), once the lane is committed: for each shop whose kit front
 * door was built into the scene, an APWShopBrowseActor at the interaction point of its browse anchor (tag
 * "interaction.nabapara.<name>.browse", verb "Browse goods", notice from the front door's core hours) and an APWShopHoursSignActor with
 * the same hours on the board over the door. Every position comes from the portable layout through the building's frame. A shop whose
 * door, layout, anchor or hours did not resolve is logged and skipped; no fallback actor is made. Returns the number of shops served.
 *
 * Integration (the integrator's, PerfectWorldKolkataLane.cpp BuildKolkataLane, after SpawnMahua(World, Scene)):
 *     SpawnKitShopDisplays(World, Scene);
 */
int32 SpawnKitShopDisplays(UWorld& World, const FLaneScene& Scene);

/** Customer browse anchor: browse_shelf, browse_bins, then the Workshop customer_counter (R53). Null when none resolves. */
const LI::Anchor* FindKitBrowseAnchor(const LI::InteriorLayout& Layout);

/** World point of the browse interaction: KitPublic::BrowseReachCm from the stand anchor toward what it faces, KitPublic::BrowseHeightCm above the floor. */
FVector KitBrowsePoint(const FLaneBuilding& Building, const LI::InteriorLayout& Layout, const LI::Anchor& Anchor);
} // namespace PWLane
