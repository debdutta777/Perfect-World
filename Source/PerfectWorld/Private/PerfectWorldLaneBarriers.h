#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldLaneBlocks.h"

namespace PWLane
{
/**
 * Seals the walkable area of the lane with invisible Pawn-only blocks (ELaneColour::Barrier), so the player cannot
 * slip into the gaps between first-row buildings, walk down the side alleys past the building depth, or leave the
 * lane at either end. The two ends are already closed by the buildings built in BuildClosingRows.
 *
 * Needs the first-row building fronts recorded in the scene, so run it after BuildLaneRows.
 *  - A gap of 450 cm or less between two fronts is filled solid from the footpath line back to |Y| = 1500, and the
 *    footpath is carried across it so the route along the lane is unbroken.
 *  - A wider gap is a side alley. It stays open as far as the shortest possible building depth (|Y| = 1100) and is
 *    closed there, where the buildings on either side still have walls.
 */
void BuildLaneBarriers(FLaneScene& Scene);
} // namespace PWLane
