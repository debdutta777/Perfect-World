#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldLaneBlocks.h"

namespace PWLane
{
/** How much detail a block gets; the near rows get all of it, the skyline almost none. */
enum class ELaneDetail : uint8
{
    Full,   // forecourt, shopfront, balconies, window frames and shutters, roof clutter
    Medium, // window frames and glass, floor bands, parapet
    Low     // box, floor bands, strips of glass, parapet
};

/** One block building, in its own frame (see FLaneFrame). */
struct FLaneBuilding
{
    FLaneFrame Frame;
    double Width = 800.0;   // along the frontage (U)
    double Depth = 800.0;   // into the block (V)
    double Setback = 0.0;   // how far the front is set back from the footpath line
    int32 Storeys = 4;      // ground floor included, 3 m each
    int32 Side = 0;         // -1 or +1 for the two lane rows, 0 for blocks that close the lane
    int32 Index = 0;        // position in its row
    ELaneColour Wall = ELaneColour::WallCream;
    ELaneDetail Detail = ELaneDetail::Full;
    uint32 Seed = 0;
};

/** Builds one building into the scene. First-row buildings also record their front for the overhead wires. */
void BuildLaneBuilding(FLaneScene& Scene, const FLaneBuilding& Building);

/** Lays out both rows of the lane, the buildings that close each end, the second and third rows, and the skyline. */
void BuildLaneRows(FLaneScene& Scene);
} // namespace PWLane
