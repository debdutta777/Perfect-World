#include "PerfectWorldLaneBarriers.h"

namespace PWLane
{
namespace
{
constexpr double MaxFilledGap = 450.0;     // wider than this and a gap is a side alley
constexpr double AlleyStopAbsY = 1100.0;   // first-row buildings are at least 700 deep behind a 400 front
constexpr double GapFillAbsY = 1500.0;     // fills past the deepest first-row building
constexpr double BarrierTop = 400.0;       // above head height; the camera is not blocked by these anyway
constexpr double Overlap = 10.0;           // barriers sink into the neighbouring walls so no crack is left
constexpr double AlleyStopThickness = 40.0;

void SealGap(FLaneBlocks& K, int32 Side, double XA, double XB)
{
    const double Sign = static_cast<double>(Side);
    const double Z0 = GroundZ - 20.0;
    const double Z1 = GroundZ + BarrierTop;
    if (XB - XA > MaxFilledGap)
    {
        // R7-02: the +Y north alley is the neighbourhood's cross street now (NeighbourhoodPlan.h), so its stopping barrier goes.
        // Only that gap: the -Y alley stop and every narrow-gap seal stay.
        if (Side > 0 && XA == AlleyPlusA && XB == AlleyPlusB)
        {
            return;
        }
        K.Barrier(FVector(XA - Overlap, Sign * (AlleyStopAbsY - AlleyStopThickness), Z0), FVector(XB + Overlap, Sign * AlleyStopAbsY, Z1));
    }
    else
    {
        // Abuts the neighbouring walls exactly (no sink-in along X), so it never stands on a setback neighbour's footpath.
        K.Barrier(FVector(XA, Sign * FootpathOuter, Z0), FVector(XB, Sign * GapFillAbsY, Z1));
        // The footpath carries on across the gap, so the walking route along it is unbroken (same level, same kerb).
        K.Box(ELaneColour::Footpath, FVector(XA, Sign * RoadHalfWidth, GroundZ - 20.0), FVector(XB, Sign * FootpathOuter, GroundZ + FootpathHeight));
        K.Box(ELaneColour::Concrete, FVector(XA, Sign * RoadHalfWidth, GroundZ - 20.0),
              FVector(XB, Sign * (RoadHalfWidth + 12.0), GroundZ + FootpathHeight + 2.0));
    }
}
} // namespace

void BuildLaneBarriers(FLaneScene& S)
{
    for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        const int32 Side = SideIndex == 0 ? -1 : 1;
        TArray<FLaneFront> Fronts = S.Fronts[SideIndex];
        Fronts.Sort([](const FLaneFront& A, const FLaneFront& B) { return A.X0 < B.X0; });

        double Cursor = LaneX0;
        for (const FLaneFront& Front : Fronts)
        {
            if (Front.X0 - Cursor > 1.0)
            {
                SealGap(S.Blocks, Side, Cursor, Front.X0);
            }
            Cursor = FMath::Max(Cursor, Front.X1);
        }
        if (LaneX1 - Cursor > 1.0)
        {
            SealGap(S.Blocks, Side, Cursor, LaneX1);
        }
    }
}
} // namespace PWLane
