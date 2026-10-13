#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PerfectWorldInteractable.h"
#include "PerfectWorldNpcCharacter.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/** Which placeholder block outfit a person wears. Real art replaces the whole body at M9; gameplay never reads it. */
enum class EPWNpcOutfit : uint8
{
    /** The staff-caregiver prototype from LANE_LIFE_PLAN section B: 162 cm, silver-streaked hair, sage overshirt, key pouch. */
    HouseholdAdult,
    /** A plain, distinct outfit for the parent household: 168 cm, off-white shirt, charcoal trousers, dark hair. */
    ParentHousehold
};

/** Fixed numbers for a standing person, in centimetres and degrees per second. */
namespace PWNpc
{
constexpr float CapsuleRadius = 34.0f;        // the same as the player, so the two collide as equals
constexpr float LookAtRangeCm = 300.0f;       // within 3 m a person slowly turns to face the player
constexpr float TurnDegreesPerSecond = 70.0f; // slow: about two and a half seconds for a half turn
constexpr int32 InteractionPriority = 20;     // people outrank doors (10)
} // namespace PWNpc

/**
 * A standing, talkable person for the lane (LANE_LIFE_PLAN section B): a block body on a capsule like the player's, which
 * idles in place, slowly turns toward the player within 3 m (presentation only), and opens the conversation panel when
 * the player interacts. It decides nothing: who they are and what they say belong to the portable core, which the
 * integrator connects through UPWConversationPresenter::SetSessionFactory.
 *
 * It is not placed in any level. SpawnAt creates one; the integrator calls it from the lane or home setup.
 */
UCLASS()
class PERFECTWORLD_API APWNpcCharacter : public ACharacter, public IPWInteractable
{
    GENERATED_BODY()

public:
    APWNpcCharacter();

    /**
     * Spawns a person standing on the floor. Transform's location is where their feet are; its yaw is the way they face
     * when no one is near. PersonId is the stable id the core knows them by. Returns null if the spawn failed.
     */
    static APWNpcCharacter* SpawnAt(UWorld& World, const FTransform& Transform, FName PersonId, const FText& DisplayName,
                                    EPWNpcOutfit Outfit = EPWNpcOutfit::HouseholdAdult);

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    // IPWInteractable
    virtual FText GetInteractionVerb(const APawn* By) const override;
    virtual FVector GetInteractionPoint() const override;
    virtual int32 GetInteractionPriority() const override;
    virtual void Interact(APawn* By) override;

    FName GetPersonId() const { return PersonId; }
    const FText& GetDisplayName() const { return DisplayName; }

private:
    void BuildBody();
    void FacePlayerNow(const APawn& Player);

    FName PersonId;
    FText DisplayName;
    EPWNpcOutfit Outfit = EPWNpcOutfit::HouseholdAdult;
    double HomeYaw = 0.0;

    /** The engine's basic cube and shape material, painted per body part like the player's body. */
    UPROPERTY()
    TObjectPtr<UStaticMesh> CubeMesh;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> BodyBaseMaterial;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> BodyParts;

    UPROPERTY()
    TArray<TObjectPtr<UMaterialInstanceDynamic>> BodyPaints;
};
