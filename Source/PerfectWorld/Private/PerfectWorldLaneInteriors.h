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

/** True for the one building that gets the walkable interior. */
bool IsOpeningHome(const FLaneBuilding& Building);

/**
 * Replaces the building's solid ground-floor volume: 25 cm perimeter walls with real openings (front door, windows), a
 * floor slab, a ceiling, partitions and doorways for five rooms, furniture, ceiling lights and the door specs. The upper
 * storeys stay a solid block above the ceiling so the street facade is unchanged. Call instead of AddWallVolume.
 */
void BuildOpeningHome(FLaneScene& Scene, const FLaneBuilding& Building);
} // namespace PWLane
