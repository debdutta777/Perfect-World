#pragma once

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "PerfectWorldLaneBarriers.h"
#include "PerfectWorldLaneBuildings.h"
#include "PerfectWorldLaneProps.h"
#include "PerfectWorldNeighbourhoodRoads.h"
#include "PerfectWorldNeighbourhoodStreets.h"

namespace PWNsTest
{
/** The overlay built once, exactly as SpawnNeighbourhoodStreets builds it, with every box logged, plus the lane as BuildKolkataLane builds it. */
struct FFixture
{
    PWNs::FNsContext Ctx;
    PWLane::FLaneBlocks Blocks;
    TArray<PWNs::FNsBox> Log;
    PWLane::FLaneScene Lane;
};

inline const FFixture& Get()
{
    static const TUniquePtr<FFixture> Built = []
    {
        TUniquePtr<FFixture> F = MakeUnique<FFixture>();
        PWLane::BuildLaneRows(F->Lane);
        PWLane::BuildLaneProps(F->Lane);
        PWLane::BuildLaneBarriers(F->Lane);
        F->Ctx.Plan = PerfectWorld::BuildNeighbourhoodPlan(0);
        F->Ctx.Lane = &F->Lane;
        PWNs::FNsSink Sink(F->Blocks, &F->Log);
        PWNs::NsBuildAll(Sink, F->Ctx);
        return F;
    }();
    return *Built;
}

inline bool IsWall(PWLane::ELaneColour C)
{
    return static_cast<int32>(C) < PWLane::WallColourCount;
}

/** A solid a standing capsule of Radius cm at (X, Y) would hit between heights Zlo and Zhi (walls, solid props and Pawn barriers). */
inline bool SolidAt(const TArray<PWNs::FNsBox>& Log, double X, double Y, double Radius, double Zlo, double Zhi)
{
    for (const PWNs::FNsBox& B : Log)
    {
        const bool bBlocks = IsWall(B.Colour) || B.Colour == PWLane::ELaneColour::PropBlock || B.Colour == PWLane::ELaneColour::Barrier;
        if (!bBlocks || B.Max.Z <= Zlo || B.Min.Z >= Zhi)
        {
            continue;
        }
        const double Dx = FMath::Max3(B.Min.X - X, 0.0, X - B.Max.X);
        const double Dy = FMath::Max3(B.Min.Y - Y, 0.0, Y - B.Max.Y);
        if (Dx * Dx + Dy * Dy < Radius * Radius)
        {
            return true;
        }
    }
    return false;
}

/** True when a footpath or carriageway box of the overlay (or a crossing ramp, or the lane's own road) lies under (X, Y). */
inline bool FloorAt(const FFixture& F, double X, double Y)
{
    for (const PWNs::FNsBox& B : F.Log)
    {
        if ((B.Colour == PWLane::ELaneColour::Footpath || B.Colour == PWLane::ELaneColour::Asphalt) && X >= B.Min.X && X <= B.Max.X &&
            Y >= B.Min.Y && Y <= B.Max.Y)
        {
            return true;
        }
    }
    for (const PWNs::FNsRect& R : F.Ctx.Ramps)
    {
        if (R.Contains(X, Y))
        {
            return true;
        }
    }
    return X >= PWLane::LaneX0 && X <= PWLane::LaneX1 && Y >= -PWLane::RoadHalfWidth && Y <= PWLane::RoadHalfWidth;
}

constexpr double WalkLo = -135.0; // a body clears anything under 45 cm above the footpath (-185)
constexpr double WalkHi = -30.0;
constexpr double CapsuleRadius = 34.0;
} // namespace PWNsTest

#endif
