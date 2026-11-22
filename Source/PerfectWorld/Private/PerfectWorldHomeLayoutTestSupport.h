// Support for PerfectWorldHomeLayoutTests.cpp: builds the opening home like the lane does and measures its layout.
// Test-only (development builds); every function is inline so the unity build can include it anywhere.
#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "PerfectWorldLaneHomeFurniture.h"
#include "PerfectWorldLaneInteriors.h"

namespace PWHomeLayoutTest
{
using namespace PWLane;

constexpr double CellCm = 5.0;
constexpr int32 CellsU = 201; // U 0-1000
constexpr int32 CellsV = 181; // V 0-900
constexpr double Tolerance = 1.0e-6;
constexpr const TCHAR* LivingDoorId = TEXT("door.nabapara.home_living");

struct FRect
{
    double U0 = 0.0;
    double U1 = 0.0;
    double V0 = 0.0;
    double V1 = 0.0;
};

struct FBlock
{
    FRect R;
    double H0 = 0.0; // above the finished floor
    double H1 = 0.0;
};

struct FHome
{
    FLaneBuilding Building;
    FLaneScene Scene;
    TArray<FHomeSolid> Solids;
    TArray<FBlock> Blocks; // every collision box of the shell and the furniture, in building coordinates
    bool bBuilt = false;
};

inline double DistToRect(double U, double V, const FRect& R)
{
    const double DU = FMath::Max3(R.U0 - U, 0.0, U - R.U1);
    const double DV = FMath::Max3(R.V0 - V, 0.0, V - R.V1);
    return FMath::Sqrt(DU * DU + DV * DV);
}

inline bool Overlaps(const FRect& A, const FRect& B)
{
    return A.U0 < B.U1 && B.U0 < A.U1 && A.V0 < B.V1 && B.V0 < A.V1;
}

inline FRect RectOf(const FHomeSolid& S)
{
    return FRect{S.U0, S.U1, S.V0, S.V1};
}

inline void ToLocal(const FLaneFrame& F, const FVector& P, double& U, double& V)
{
    const FVector Rel = P - F.Origin;
    U = FVector::DotProduct(Rel, F.U);
    V = FVector::DotProduct(Rel, F.V);
}

/** Builds the home like the lane does. */
inline void BuildHome(FHome& Home)
{
    FLaneBuilding& B = Home.Building;
    B.Frame = MakeLaneFrame(-300.0, -400.0, FVector(1.0, 0.0, 0.0), FVector(0.0, -1.0, 0.0));
    B.Width = 1000.0;
    B.Depth = 900.0;
    B.Setback = 0.0;
    B.Storeys = 5;
    B.Side = -1;
    B.Index = 2;
    B.Wall = ELaneColour::WallCream;
    B.Detail = ELaneDetail::Full;
    if (!IsOpeningHome(B))
    {
        return;
    }
    BuildOpeningHome(Home.Scene, B);
    FLaneScene Spare;
    FurnishOpeningHome(Spare, B, &Home.Solids);

    const double FloorZ = GroundZ + HomeFloorW;
    for (const FBox& Box : Home.Scene.Blocks.BlockingBoxes())
    {
        double UA = 0.0;
        double VA = 0.0;
        double UB = 0.0;
        double VB = 0.0;
        ToLocal(B.Frame, Box.Min, UA, VA);
        ToLocal(B.Frame, Box.Max, UB, VB);
        FBlock Block;
        Block.R = FRect{FMath::Min(UA, UB), FMath::Max(UA, UB), FMath::Min(VA, VB), FMath::Max(VA, VB)};
        Block.H0 = Box.Min.Z - FloorZ;
        Block.H1 = Box.Max.Z - FloorZ;
        Home.Blocks.Add(Block);
    }
    Home.bBuilt = Home.Blocks.Num() > 0 && Home.Solids.Num() > 0;
}

/** True for something that stops a standing person (not the floor slab, not the lintel). */
inline bool IsBodyBlock(const FBlock& Block)
{
    return Block.H1 > 5.0 && Block.H0 < 180.0;
}

inline const FLaneDoorSpec* FindDoor(const FHome& Home, const TCHAR* Id)
{
    for (const FLaneDoorSpec& Door : Home.Scene.Doors)
    {
        if (Door.Id == FName(Id))
        {
            return &Door;
        }
    }
    return nullptr;
}

/** The floor the door's leaf can reach while it swings (leaf 90 + thickness, opening 95 degrees), on one side. */
inline FRect SwingZone(const FLaneFrame& F, const FLaneDoorSpec& Door, double SideSign)
{
    const double Yaw = FMath::DegreesToRadians(Door.ClosedYaw);
    const FVector Dir(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.0);
    const FVector Side = FVector(-Dir.Y, Dir.X, 0.0) * SideSign;
    FRect Zone{1.0e9, -1.0e9, 1.0e9, -1.0e9};
    for (const double Along : {-9.0, 92.0})
    {
        for (const double Across : {-2.0, 92.0})
        {
            double U = 0.0;
            double V = 0.0;
            ToLocal(F, Door.Hinge + Dir * Along + Side * Across, U, V);
            Zone.U0 = FMath::Min(Zone.U0, U);
            Zone.U1 = FMath::Max(Zone.U1, U);
            Zone.V0 = FMath::Min(Zone.V0, V);
            Zone.V1 = FMath::Max(Zone.V1, V);
        }
    }
    return Zone;
}

struct FNamedZone
{
    FName Door;
    FRect Zone;
};

/** Every swing zone of every door: both sides for a two-way door, the preferred side for a one-way door. */
inline TArray<FNamedZone> AllSwingZones(const FHome& Home)
{
    TArray<FNamedZone> Zones;
    for (const FLaneDoorSpec& Door : Home.Scene.Doors)
    {
        Zones.Add(FNamedZone{Door.Id, SwingZone(Home.Building.Frame, Door, Door.PreferredSwing)});
        if (Door.bTwoWay)
        {
            Zones.Add(FNamedZone{Door.Id, SwingZone(Home.Building.Frame, Door, -Door.PreferredSwing)});
        }
    }
    return Zones;
}

/** The living door's leaf at its default (open) state, as a thin rectangle in building coordinates. */
inline bool OpenLeafRect(const FHome& Home, FRect& Out)
{
    const FLaneDoorSpec* Door = FindDoor(Home, LivingDoorId);
    if (Door == nullptr)
    {
        return false;
    }
    const double Yaw = FMath::DegreesToRadians(Door->ClosedYaw);
    const FVector Dir(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.0);
    const FVector Side = FVector(-Dir.Y, Dir.X, 0.0) * Door->PreferredSwing;
    const double Open = FMath::DegreesToRadians(95.0);
    const FVector Tip = Door->Hinge + (Dir * FMath::Cos(Open) + Side * FMath::Sin(Open)) * 90.0;
    double UA = 0.0;
    double VA = 0.0;
    double UB = 0.0;
    double VB = 0.0;
    ToLocal(Home.Building.Frame, Door->Hinge, UA, VA);
    ToLocal(Home.Building.Frame, Tip, UB, VB);
    Out = FRect{FMath::Min(UA, UB) - 2.0, FMath::Max(UA, UB) + 2.0, FMath::Min(VA, VB) - 2.0, FMath::Max(VA, VB) + 2.0};
    return true;
}

/** Everything a person walking around the home bumps into: shell, furniture, the open living leaf and the reserved stair. */
inline TArray<FRect> WalkObstacles(const FHome& Home)
{
    TArray<FRect> Obstacles;
    for (const FBlock& Block : Home.Blocks)
    {
        if (IsBodyBlock(Block))
        {
            Obstacles.Add(Block.R);
        }
    }
    FRect Leaf;
    if (OpenLeafRect(Home, Leaf))
    {
        Obstacles.Add(Leaf);
    }
    const FHomeStairSpec Stair = HomeStairSpec();
    Obstacles.Add(FRect{Stair.U0, Stair.U1, Stair.V0, Stair.V1});
    return Obstacles;
}

/**
 * Flood fill over a 5 cm grid of the points where a disc of the given radius clears every obstacle and stays inside the
 * region: a route of width W exists where the radius is W / 2.
 */
inline TArray<uint8> FloodRoutes(const TArray<FRect>& Obstacles, double Radius, const FRect& Region, double SeedU, double SeedV)
{
    TArray<uint8> Free;
    Free.Init(0, CellsU * CellsV);
    for (int32 J = 0; J < CellsV; ++J)
    {
        for (int32 I = 0; I < CellsU; ++I)
        {
            const double U = CellCm * I;
            const double V = CellCm * J;
            bool bFree = U - Radius >= Region.U0 - Tolerance && U + Radius <= Region.U1 + Tolerance &&
                         V - Radius >= Region.V0 - Tolerance && V + Radius <= Region.V1 + Tolerance;
            for (int32 K = 0; bFree && K < Obstacles.Num(); ++K)
            {
                bFree = DistToRect(U, V, Obstacles[K]) >= Radius - Tolerance;
            }
            Free[J * CellsU + I] = bFree ? 1 : 0;
        }
    }
    TArray<uint8> Reached;
    Reached.Init(0, CellsU * CellsV);
    TArray<int32> Stack;
    const int32 Start = FMath::RoundToInt(SeedV / CellCm) * CellsU + FMath::RoundToInt(SeedU / CellCm);
    if (Free.IsValidIndex(Start) && Free[Start] != 0)
    {
        Reached[Start] = 1;
        Stack.Add(Start);
    }
    while (!Stack.IsEmpty())
    {
        const int32 Cell = Stack.Pop();
        const int32 I = Cell % CellsU;
        const int32 J = Cell / CellsU;
        const int32 Next[4][2] = {{I + 1, J}, {I - 1, J}, {I, J + 1}, {I, J - 1}};
        for (const auto& N : Next)
        {
            if (N[0] < 0 || N[0] >= CellsU || N[1] < 0 || N[1] >= CellsV)
            {
                continue;
            }
            const int32 Index = N[1] * CellsU + N[0];
            if (Free[Index] != 0 && Reached[Index] == 0)
            {
                Reached[Index] = 1;
                Stack.Add(Index);
            }
        }
    }
    return Reached;
}

inline bool IsReached(const TArray<uint8>& Reached, double U, double V)
{
    const int32 I = FMath::Clamp(FMath::RoundToInt(U / CellCm), 0, CellsU - 1);
    const int32 J = FMath::Clamp(FMath::RoundToInt(V / CellCm), 0, CellsV - 1);
    return Reached[J * CellsU + I] != 0;
}

inline double MinDistToSolids(const FHome& Home, double U, double V)
{
    double Best = 1.0e9;
    for (const FHomeSolid& Solid : Home.Solids)
    {
        Best = FMath::Min(Best, DistToRect(U, V, RectOf(Solid)));
    }
    return Best;
}

/** Distance to the nearest body-height collision box (shell, jambs, furniture). */
inline double MinDistToBodyBlocks(const FHome& Home, double U, double V)
{
    double Best = 1.0e9;
    for (const FBlock& Block : Home.Blocks)
    {
        if (IsBodyBlock(Block))
        {
            Best = FMath::Min(Best, DistToRect(U, V, Block.R));
        }
    }
    return Best;
}

inline double MinDistToZones(const FHome& Home, double U, double V)
{
    double Best = 1.0e9;
    for (const FNamedZone& Zone : AllSwingZones(Home))
    {
        Best = FMath::Min(Best, DistToRect(U, V, Zone.Zone));
    }
    return Best;
}

/** The free gap along a wall line at a doorway: from the nearest body-height box on one side of the centre to the other. */
inline bool MeasureDoorGap(const FHome& Home, bool bAlongU, double Line, double Centre, double& OutClear, double& OutHeadroom)
{
    double Lo = -1.0e9;
    double Hi = 1.0e9;
    OutHeadroom = 1.0e9;
    for (const FBlock& Block : Home.Blocks)
    {
        const double P0 = bAlongU ? Block.R.V0 : Block.R.U0;
        const double P1 = bAlongU ? Block.R.V1 : Block.R.U1;
        const double A0 = bAlongU ? Block.R.U0 : Block.R.V0;
        const double A1 = bAlongU ? Block.R.U1 : Block.R.V1;
        if (Line < P0 || Line > P1)
        {
            continue;
        }
        if (A0 <= Centre && Centre <= A1)
        {
            if (Block.H0 >= 100.0)
            {
                OutHeadroom = FMath::Min(OutHeadroom, Block.H0); // the lintel
                continue;
            }
            return false; // something solid stands in the doorway itself
        }
        if (!IsBodyBlock(Block))
        {
            continue;
        }
        if (A1 <= Centre)
        {
            Lo = FMath::Max(Lo, A1);
        }
        else if (A0 >= Centre)
        {
            Hi = FMath::Min(Hi, A0);
        }
    }
    OutClear = Hi - Lo;
    return Hi < 1.0e8 && Lo > -1.0e8;
}
} // namespace PWHomeLayoutTest

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
