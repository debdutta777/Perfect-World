#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PerfectWorldInteractable.h"
#include "PerfectWorldShopDisplayActor.generated.h"

class USceneComponent;
class UTextRenderComponent;

/** Numbers shared by the shop display actors. */
namespace PWShopDisplay
{
constexpr int32 InteractionPriority = 15; // as PWFurniture::InteractionPriority: above doors (10), below people (20)
constexpr float NoticeSeconds = 6.0f;     // a three-line notice is read, not glanced at
} // namespace PWShopDisplay

/**
 * The goods on display in an unstaffed browsing shop (R4-01). The actor stands at the interaction point in front of the shelf or bin
 * the building's browse anchor serves; it has no mesh and no collision (the shelf is lane geometry). Interact shows the notice through
 * UPWInteractionComponent::ShowNotice on the interacting pawn: what is on display, the posted hours and that nobody is serving and
 * nothing can be bought. It opens no menu and starts no transaction. The notice and the tag come from the building and its core door
 * definition (PerfectWorldLaneKitStalls.h), never typed here. Priority 15.
 */
UCLASS()
class PERFECTWORLD_API APWShopBrowseActor : public AActor, public IPWInteractable
{
    GENERATED_BODY()

public:
    APWShopBrowseActor();

    /** All world values. InTag becomes an actor tag ("interaction.nabapara.<name>.browse"); InNotice is shown on Interact. */
    void Configure(FName InBuildingId, FName InTag, const FText& InVerb, const FText& InNotice, const FVector& PointWorld);

    FName GetBuildingId() const { return BuildingId; }
    FText GetNotice() const { return Notice; }
    FText GetVerb() const { return Verb; }

    // IPWInteractable
    virtual FText GetInteractionVerb(const APawn* By) const override;
    virtual FVector GetInteractionPoint() const override;
    virtual int32 GetInteractionPriority() const override { return PWShopDisplay::InteractionPriority; }
    virtual void Interact(APawn* By) override;

private:
    UPROPERTY()
    TObjectPtr<USceneComponent> Root;

    FName BuildingId;
    FText Verb;
    FText Notice;
};

/**
 * The posted hours of an unstaffed browsing shop: text drawn on the board over its door (the board itself is lane geometry). Its text
 * is derived from the front door's core definition (PostedSignText), so the sign cannot drift from the door's policy. It faces the
 * footpath: the component's +X (its front) points along the actor's forward vector. No collision, no interaction.
 */
UCLASS()
class PERFECTWORLD_API APWShopHoursSignActor : public AActor
{
    GENERATED_BODY()

public:
    APWShopHoursSignActor();

    void Configure(FName InBuildingId, const FString& InText, const FVector& Location, double YawDegrees, float TextSizeCm);

    FName GetBuildingId() const { return BuildingId; }
    /** The text exactly as posted, lines separated by newlines. */
    FString GetPostedText() const { return PostedText; }

private:
    UPROPERTY()
    TObjectPtr<UTextRenderComponent> Text;

    FName BuildingId;
    FString PostedText;
};
