#pragma once

#include "CoreMinimal.h"

/** World-space exclusion volume including the leaf sweep and approach, not just the closed leaf. */
struct FNpDoorwayZone
{
    FName Id;
    FBox Bounds = FBox(ForceInit);
    TArray<FVector> WaitingFeet;
};

/** Transient presentation state. Never enters WorldSim or a save. */
struct FNpYieldState
{
    FName DoorId;
    FVector WaitingFeet = FVector::ZeroVector;
    double ClearSeconds = 0.0;
    bool IsActive() const { return !DoorId.IsNone(); }
};

namespace NpYield
{
constexpr double ApproachCm = 250.0;
constexpr double ReleaseCm = 350.0;
constexpr double ReleaseSeconds = 1.0;
constexpr double MaxDetourCm = 350.0;
using FPointAllowed = TFunction<bool(const FVector&)>;

/** Latched until the player is outside the larger radius for a full second. Stable nearest-point ties. */
bool Update(FNpYieldState& State, TConstArrayView<FNpDoorwayZone> Doors, const FVector& NpcFeet,
            double Radius, double HalfHeight, const FVector* PlayerFeet, bool bEligible, double Dt,
            const FPointAllowed& PointAllowed);
}
