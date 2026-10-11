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
 * BaseMaterial is /Engine/BasicShapes/BasicShapeMaterial (vector parameter "Color"); if it is null the blocks keep
 * the cube's own material and the scene is shown unpainted rather than not at all.
 */
void BuildKolkataLane(UWorld& World, UStaticMesh& Cube, UMaterialInterface* BaseMaterial);
} // namespace PWLane
