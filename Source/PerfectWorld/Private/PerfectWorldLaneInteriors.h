#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldLaneBuildings.h"

namespace PWLane
{
/**
 * The opening home: the first enterable, furnished ground-floor interior of the lane (LANE_LIFE_PLAN.md C "Opening home").
 *
 * Which building: the first-row building on the -Y side with Index 2. The layout is fixed by LaneHash(-124, 2, 11) =
 * 1655793196, which gives X -300 to 700 cm (Frame origin (-300, -400), U = +X, V = -Y), Width 1000, Depth 900,
 * Setback 0, 5 storeys of 300 cm, WallCream, and "house door" mode (the front door is at U 700, X 400). The interior is
 * authored for exactly that footprint, so IsOpeningHome also checks the dimensions: if the generator ever changes them,
 * the building falls back to the old solid volume and FLaneScene::bOpeningHomeBuilt stays false (the lane logs a warning).
 *
 * Local coordinates used throughout the interior files: U along the frontage (X), V from the front face into the
 * building (-Y), W above GroundZ (the finished floor is W 15, the ceiling underside W 289).
 */
constexpr double HomeFrontDoorU = 700.0; // centre of the front door along the frontage (the old house-door position, 0.70 W)

/**
 * Door layout, shared by the shell, the furniture rules and PerfectWorldHomeLayoutTests.cpp (the rooms' furniture and the
 * adult's spot are placed against these numbers). Every door is 94 cm clear between its jamb strips (the leaf is 90) and
 * 212 cm clear above the floor.
 *
 * The living door (door.nabapara.home_living) is in the partition at U 620 between the entry and the living room. It sits
 * at the BACK of that partition (V 318-412), hinged on its back jamb, and swings into the ENTRY only (one-way): open
 * (its default state), the leaf stands against the back partition at V 410-422 instead of across the room. It used to be
 * mid-wall and swing into the living room, which put the open leaf across the household adult's spot and line of sight
 * (Docs/STATUS.md, lane life stage 3, defect 1).
 */
constexpr double HomeDoorHalfClear = 47.0;
constexpr double HomeDoorClearHeight = 212.0;
constexpr double HomeLivingPartitionU = 620.0; // the partition between the entry and the living room
constexpr double HomeLivingDoorV = 365.0;      // centre of the living doorway along that partition
constexpr double HomeAcrossPartitionV = 437.0; // the partition between the front rooms and the back rooms
constexpr double HomeBedroomDoorU = 200.0;
constexpr double HomeKitchenDoorU = 500.0;
constexpr double HomeWashroomDoorU = 860.0;

/** True for the one building that gets the walkable interior. */
bool IsOpeningHome(const FLaneBuilding& Building);

/**
 * Replaces the building's solid ground-floor volume: 25 cm perimeter walls with real openings (front door, windows), a
 * floor slab, a ceiling, partitions and doorways for five rooms, furniture, ceiling lights and the door specs. The upper
 * storeys stay a solid block above the ceiling so the street facade is unchanged. Call instead of AddWallVolume.
 */
void BuildOpeningHome(FLaneScene& Scene, const FLaneBuilding& Building);
} // namespace PWLane
