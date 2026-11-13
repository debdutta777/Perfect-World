#pragma once

#include "CoreMinimal.h"
#include "AmbientCrowd.h"
#include "GameFramework/Character.h"
#include "PerfectWorldInteractable.h"
#include "PerfectWorldNpcLooks.h"
#include "PerfectWorldCrowdFigures.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

namespace PWCrowd
{
struct FLook
{
    double Height = 162;
    TArray<FPWNpcBlock> Blocks;
};
/** Uses the NPC block kit, face, proportions and pose groups; never a named cast identity. */
FLook MakeLook(const PerfectWorld::CrowdPerson& Person);
FText ExchangeLine(const std::string& Key);
FVector Point(PerfectWorld::NeighbourhoodPoint P);
FVector RoutePoint(const PerfectWorld::CrowdRouteSlot& Route, double Distance);
/** Clips the authored polyline at Target. No extrapolation beyond a waiting spot. */
TArray<FVector> Approach(const PerfectWorld::CrowdRouteSlot& Route, double Target, bool bReverse);
/** Entrance events are credible boundaries, but never permission to visibly pop. */
bool MayChangePresence(bool bOldVisible, bool bNewVisible);
}

/** Anonymous, transient ACharacter. Shares the NPC block kit, mover, sweeps and poses.
 * Does not register with named presence, conversations, journals, saves or gameplay RNG.
 */
UCLASS(Transient, NotPlaceable)
class APWCrowdFigure : public ACharacter, public IPWInteractable
{
    GENERATED_BODY()
public:
    APWCrowdFigure();
    static APWCrowdFigure* Spawn(UWorld& World, const PerfectWorld::CrowdPerson& Person,
        const FVector& Feet, const FRotator& Facing);
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual FText GetInteractionVerb(const APawn* By) const override;
    virtual FVector GetInteractionPoint() const override;
    virtual int32 GetInteractionPriority() const override;
    virtual void Interact(APawn* By) override;
    void Present(const PerfectWorld::CrowdPerson& Person, const PerfectWorld::CrowdRouteSlot& Route);
    void SetClockRunning(bool bRunning);
    void ApplyPose(const FPWNpcPose& Pose);
    FVector Feet() const;
    const PerfectWorld::CrowdPerson& Person() const { return CrowdPerson; }
    const FPWNpcPose& Pose() const { return CurrentPose; }
    bool IsTalkHeld() const { return TalkRemaining > 0; }
private:
    PerfectWorld::CrowdPerson CrowdPerson;
    PWCrowd::FLook Look;
    FPWNpcPose CurrentPose;
    bool bClockRunning = false;
    float TalkRemaining = 0;
    UPROPERTY() TObjectPtr<UPWNpcMovementComponent> Mover;
    UPROPERTY() TObjectPtr<UStaticMesh> Cube;
    UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
};
