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
        K.Barrier(FVector(XA - Overlap, Sign * (AlleyStopAbsY - AlleyStopThickness), Z0), FVector(XB + Overlap, Sign * AlleyStopAbsY, Z1));
    }
    else
    {
        K.Barrier(FVector(XA - Overlap, Sign * FootpathOuter, Z0), FVector(XB + Overlap, Sign * GapFillAbsY, Z1));
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
