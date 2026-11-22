#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UStaticMesh;
class UWorld;

namespace PWLane
{
/**
 * Builds the block-built Kolkata lane that sits behind the front end: a 5 m road running along +X from the
 * origin with raised footpaths, two rows of three to five storey block buildings (shopfronts, balconies,
 * rooftop tanks, warm windows), utility poles with sagging wires, trees, a taxi, and a hazy skyline.
 *
 * The layout is fixed by a small integer hash, so every run is identical. Everything is the engine cube, placed
 * as instances grouped by colour (one instanced mesh component and one dynamic material per colour). A few small
 * warm point lights are added near the starting view. Nothing ticks.
 *
 * Collision (for the walking player, M1-13), by explicit role (ELaneCollision in PerfectWorldLaneBlocks.h): walls,
 * ground, road and footpaths block everything (World); small decoration (trim, wires, signs, awnings, goods, leaves)
 * has none; solid props (poles, trunks, vehicles, counters, pots) have invisible boxes within about 5 cm of their
 * visible shape that block the Pawn, the Camera and Visibility (Prop, ELaneColour::PropBlock); the lane boundary
 * (gaps between buildings, alley ends) is closed by invisible Pawn-only barriers (Boundary, ELaneColour::Barrier).
 *
 * Footpath circulation: counters stand in niches cut into the building, awning poles, trees and utility poles stand
 * on the kerb edge or well clear of the wall, and pots are placed last, so a continuous route at least 120 cm wide
 * runs along every footpath (PerfectWorldLaneRoutes.h reports it in development builds).
 *
 * BaseMaterial is /Engine/BasicShapes/BasicShapeMaterial (vector parameter "Color"); if it is null the blocks keep
 * the cube's own material and the scene is shown unpainted rather than not at all.
 */
void BuildKolkataLane(UWorld& World, UStaticMesh& Cube, UMaterialInterface* BaseMaterial);
} // namespace PWLane
