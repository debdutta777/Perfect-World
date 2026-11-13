#pragma once

#include "CoreMinimal.h"
#include "AmbientCrowd.h"
#include "GameFramework/Actor.h"
#include "PerfectWorldCrowdFigures.h"
#include "PerfectWorldCrowdPresenter.generated.h"

class APlayerController;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class APWCrowdPresenter;
struct FR75PCrowdRecord;

namespace PWCrowd
{
/** Call after lane/campus collision and the loaded lane world are installed. Missing calendar refuses spawning. */
APWCrowdPresenter* SpawnCrowdPresenter(UWorld& World);
}

/** One transient lane presenter. Queries at 2 Hz; only cached instances move each frame.
 * 36 ambient actors by default, within the model's 48-person total; 80 far figures.
 * Visible stale figures wait until occluded. LOD swaps preserve identity, look and feet.
 */
UCLASS(Transient, NotPlaceable)
class APWCrowdPresenter : public AActor
{
    GENERATED_BODY()
public:
    APWCrowdPresenter();
    /** Timing defaults to bell_schedule.nabapara_xi_science (gates 09:30, bell 10:00, dismissal 15:50);
     * SpawnCrowdPresenter derives it from the loaded bell_schedule content. */
    bool Initialise(uint64 Seed, const PerfectWorld::SchoolCalendar& Calendar, const PerfectWorld::CrowdSchoolTiming& Timing = {});
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void RefreshNow();
    /** Explicit clock/query seam for automation or a caller already holding the lane clock. */
    void SampleAt(PerfectWorld::GameInstant Now, PerfectWorld::NeighbourhoodPoint Centre,
        PerfectWorld::CrowdPresentationBudget Budget = {}, int32 Fraction = 0, bool bReconstruct = false);
    /** Optional authoritative ClockMode.CarryMilliseconds accessor; no local gameplay clock is invented. */
    void SetMinuteFractionSource(TFunction<int32()> Source) { MinuteFractionSource = MoveTemp(Source); }
    int32 ActorCount() const;
    int32 FarFigureCount() const;
    int32 CubeInstanceCount() const;
    const PerfectWorld::CrowdSnapshot& Snapshot() const { return Latest; }
    struct FObserved
    {
        FString Id;
        PerfectWorld::CrowdCohort Cohort;
        FVector Feet;
        bool bActor = false;
    };
    TArray<FObserved> ObservedPeople() const;
    APWCrowdFigure* NearestStudent(const FVector& From) const;
    bool IsPointVisible(const FVector& Feet) const;
#if WITH_DEV_AUTOMATION_TESTS
    void SetVisibilityForAutomation(TFunction<bool(const FVector&)> Source) { VisibilitySource = MoveTemp(Source); }
#endif
private:
    const PerfectWorld::CrowdRouteSlot* Route(uint32 Slot) const;
    void Reconcile(const PerfectWorld::CrowdSnapshot& Snapshot, bool bDiscontinuity);
    void MakeFar(FR75PCrowdRecord& Record);
    void ReleaseFar(FR75PCrowdRecord& Record);
    void DrawFar(FR75PCrowdRecord& Record);
    void Demote(FR75PCrowdRecord& Record);
    void AdvanceFar(float DeltaSeconds, bool bRunning);
    UInstancedStaticMeshComponent* Batch(FColor Tint);

    TUniquePtr<PerfectWorld::AmbientCrowd> Model;
    PerfectWorld::SchoolCalendar SchoolCalendar;
    PerfectWorld::CrowdSchoolTiming SchoolTiming;
    PerfectWorld::CrowdSnapshot Latest;
    TMap<FString, TSharedPtr<FR75PCrowdRecord>> People;
    TMap<uint32, TArray<int32>> FreeInstances;
    TFunction<int32()> MinuteFractionSource;
#if WITH_DEV_AUTOMATION_TESTS
    TFunction<bool(const FVector&)> VisibilitySource;
#endif
    PerfectWorld::GameInstant PreviousNow = 0;
    uint64 RestoreToken = 0;
    uint64 WorldSeed = 0;
    double UntilSample = 0;
    double UntilLog = 0;
    double LastUpdateMs = 0;
    double LastFrameMs = 0;
    bool bHaveNow = false;
    UPROPERTY() TObjectPtr<UStaticMesh> Cube;
    UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;
    UPROPERTY() TMap<uint32, TObjectPtr<UInstancedStaticMeshComponent>> Batches;
};
