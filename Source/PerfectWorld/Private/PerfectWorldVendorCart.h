#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PerfectWorldVendorCart.generated.h"

class APWNpcCharacter;
class UBoxComponent;
struct FCollisionQueryParams;
namespace PWLane
{
/** Integrator calls this for the NPC presenter's avoidance and snap query params. */
void AmbientMoveIgnoreParams(const AActor& Owner, FCollisionQueryParams& Params);
/** Coupled carts walk to catch up; they never use the generic offscreen snap. */
bool AmbientAllowsSnap(const AActor& Owner);
}
/** One inert block prop, coupled to Purnima's actual swept position, never to schedule progress. */
UCLASS()
class APWVendorCart : public AActor
{
    GENERATED_BODY()
public:
    APWVendorCart();
    static APWVendorCart* GetOrSpawn(UWorld& World);
    virtual void Tick(float DeltaSeconds) override;
    bool IsRouteBlocked() const { return bRouteBlocked; }
    int32 RejectedMoves() const { return RefusedMoves; }
private:
    UPROPERTY() TObjectPtr<UBoxComponent> Bounds;
    TWeakObjectPtr<APWNpcCharacter> Vendor;
    FVector LastVendorCentre = FVector::ZeroVector;
    bool bPlaced = false;
    bool bRouteBlocked = false;
    int32 RefusedMoves = 0;
};
