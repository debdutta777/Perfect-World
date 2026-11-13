#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NeighbourhoodTraffic.h"
#include "PerfectWorldTrafficPresenter.generated.h"

class APWTrafficVehicle;
class APawn;
namespace PWTraffic
{
inline constexpr int32 R72PRadiusCm = 16000;
inline constexpr double R72PStepSeconds = PerfectWorld::NeighbourhoodTrafficStepMs / 1000.0;
/** The core traffic service bands use midnight-based minutes; campaign zero is 06:00. */
PerfectWorld::NeighbourhoodTrafficTime FromCampaignTime(PerfectWorld::GameInstant Now);
FVector RoadPoint(const PerfectWorld::NeighbourhoodVehicle& Vehicle);
FVector LaneDirection(const PerfectWorld::NeighbourhoodPlan& Plan, std::string_view LaneId);
APWTrafficVehicle* FindVehicle(UWorld* World, std::string_view Id);
}

struct FR72PTrafficSlot
{
    TWeakObjectPtr<APWTrafficVehicle> Actor;
    PerfectWorld::NeighbourhoodVehicle Target;
    FVector From = FVector::ZeroVector;
    FVector To = FVector::ZeroVector;
    bool bRetiring = false;
};

/** Transient, one-per-world bridge. The model owns routes, stops, service, phases and capacity. */
UCLASS()
class APWTrafficPresenter : public AActor
{
    GENERATED_BODY()
public:
    APWTrafficPresenter();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    /** Load/hidden reconciliation only. Rebuilds the seeded baseline; no traffic is saved. */
    void ResetTraffic(uint64 Seed, PerfectWorld::NeighbourhoodTrafficTime Now);
    /** Production/test seam: occupancy must cover the entire frame, including unrendered people. */
    void PresentFrame(double DeltaSeconds, const FVector& PlayerPosition,
        std::span<const PerfectWorld::NeighbourhoodPedestrianOccupancy> Occupancy);
    /** Crowd provider appends swept XY footprints for non-pawn/instanced pedestrians every frame. */
    TFunction<void(std::vector<PerfectWorld::NeighbourhoodPedestrianOccupancy>&)> AppendCrowdOccupancy;
    PerfectWorld::NeighbourhoodCrossingPhase CrossingPhase(std::string_view Id) const;
    PerfectWorld::NeighbourhoodTrafficTime GetPresentedTime() const { return PresentedTime; }
    int32 GetVehicleCount() const { return static_cast<int32>(Slots.size()); }
    bool IsCollisionHeld() const { return bCollisionHeld; }
    const std::vector<FR72PTrafficSlot>& GetSlots() const { return Slots; }
    const PerfectWorld::NeighbourhoodPlan& GetPlan() const { return Plan; }
    bool IsCrossingClear(std::string_view Id) const;

private:
    void GatherPedestrians(std::vector<PerfectWorld::NeighbourhoodPedestrianOccupancy>& Out);
    void BeginSegment(const FVector& PlayerPosition, std::span<const PerfectWorld::NeighbourhoodPedestrianOccupancy> Occupancy);
    bool IsObserved(const FVector& Centre, const FVector& Extent, const AActor* Ignored) const;
    bool AreRoadsObserved() const;
    bool IsMapEdge(const PerfectWorld::NeighbourhoodVehicle& Vehicle) const;
    void RemoveVehicles();
    void RemoveUnobservedRetirees();
    PerfectWorld::NeighbourhoodPlan Plan;
    TUniquePtr<PerfectWorld::NeighbourhoodTraffic> Model;
    // Portable snapshots contain std::string; use native move-aware storage rather than
    // TArray's bitwise relocation of arbitrary elements.
    std::vector<FR72PTrafficSlot> Slots;
    TMap<TWeakObjectPtr<APawn>, FVector> LastPawnPositions;
    PerfectWorld::NeighbourhoodTrafficTime ModelTime, PresentedTime;
    uint64 WorldSeed = 0, RestoreCount = 0;
    double SegmentSeconds = PWTraffic::R72PStepSeconds;
    bool bCollisionHeld = false;
};

namespace PWLane
{
/** Call once after static streets are built; idempotent per UWorld. Seed/clock come from lane life. */
APWTrafficPresenter* SpawnTrafficPresenter(UWorld& World);
}
