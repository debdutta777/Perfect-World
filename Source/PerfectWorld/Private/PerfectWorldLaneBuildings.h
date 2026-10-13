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
    int32 Storeys = 4;      // ground floor included
    double StoreyHeightCm = StoreyHeight; // floor to floor: 300 for flats; OlderStoreyHeight (360) for authored older houses and the school
    int32 Side = 0;         // -1 or +1 for the two lane rows, 0 for blocks that close the lane
    int32 Index = 0;        // position in its row
    ELaneColour Wall = ELaneColour::WallCream;
    ELaneDetail Detail = ELaneDetail::Full;
    uint32 Seed = 0;

    /** Finished-floor height of storey n above GroundZ (storey 0 is level with the footpath). */
    double FloorZ(int32 StoreyIndex) const
    {
        return PWLane::FloorZ(StoreyIndex, StoreyHeightCm);
    }

    /** Height of the roof slab above GroundZ: the floor level one storey above the top storey. */
    double RoofZ() const
    {
        return PWLane::FloorZ(Storeys, StoreyHeightCm);
    }
};

/** A span along a frontage (U, centimetres). */
struct FLaneSpan
{
    double U0 = 0.0;
    double U1 = 0.0;
};

/** Builds one building into the scene. First-row buildings also record their front for the overhead wires. */
void BuildLaneBuilding(FLaneScene& Scene, const FLaneBuilding& Building);

/** Lays out both rows of the lane, the buildings that close each end, the second and third rows, and the skyline. */
void BuildLaneRows(FLaneScene& Scene);
} // namespace PWLane
