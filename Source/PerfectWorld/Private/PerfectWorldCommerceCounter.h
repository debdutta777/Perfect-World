#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PerfectWorldInteractable.h"

#include "PerfectWorldCommerceBridge.h"

#include "PerfectWorldCommerceCounter.generated.h"

class USceneComponent;
class UWorld;

namespace PWCommerce
{
/** Where one staffed shop's customer counter is, and which shop it sells for. All values come from the shop's content binding and the building's frame. */
struct FCmCounterSpec
{
    /** The commerce shop id (R8-02 content), for example shop.shanti_chai. */
    FName ShopId;
    /** The actor tag, "interaction.nabapara.<shop>.buy". */
    FName Tag;
    /** The interaction point in world space: where the customer stands and the keeper serves across the counter. */
    FVector PointWorld = FVector::ZeroVector;
};

/**
 * Spawns one Buy counter per spec, once the lane is committed (the same time as SpawnKitShopDisplays). A spec without a shop id or tag is
 * skipped and logged; no fallback actor is made. Returns the number of counters spawned.
 */
int32 SpawnCommerceCounters(UWorld& World, const TArray<FCmCounterSpec>& Specs);
} // namespace PWCommerce

/**
 * The customer counter of a staffed shop (R8-05): "Buy", alongside the keeper's Talk and the browse shelf's Browse. The actor stands at the
 * interaction point; it has no mesh and no collision. Interact asks the world's commerce seam for service (ICmCommerceWorld::BeginService);
 * when the core says no, the player reads the reason as a notice (closed, keeper away, finishing a step, not enough money, bag full);
 * when it says yes, the paused buy panel opens (UPWBuyPresenter). The actor decides nothing and holds no price.
 *
 * Priority 15 like the browse shelf, so the keeper (Talk, 20) keeps the prompt when both are in reach; the player cycles between them with
 * the interaction component's cycle key, which also shows the hint.
 */
UCLASS()
class PERFECTWORLD_API APWCounterBuyActor : public AActor, public IPWInteractable
{
    GENERATED_BODY()

public:
    APWCounterBuyActor();

    /** InTag becomes an actor tag; PointWorld is the interaction point. */
    void Configure(FName InShopId, FName InTag, const FText& InVerb, const FVector& PointWorld);

    FName GetShopId() const { return ShopId; }
    /** The core's answer to the last Interact (None when the panel opened, or before the first Interact). */
    PerfectWorld::CommerceError GetLastError() const { return LastError; }
    /** True when the last Interact opened the panel. */
    bool DidOpenPanel() const { return bOpenedPanel; }
    FText GetLastNotice() const { return LastNotice; }

    // IPWInteractable
    virtual FText GetInteractionVerb(const APawn* By) const override;
    virtual FVector GetInteractionPoint() const override;
    virtual int32 GetInteractionPriority() const override;
    virtual void Interact(APawn* By) override;

private:
    void Say(APawn* By, const FText& Words);

    UPROPERTY()
    TObjectPtr<USceneComponent> Root;

    FName ShopId;
    FText Verb;
    PerfectWorld::CommerceError LastError = PerfectWorld::CommerceError::None;
    bool bOpenedPanel = false;
    FText LastNotice;
};
