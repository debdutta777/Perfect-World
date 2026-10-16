#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PerfectWorldInteractable.h"
#include "PerfectWorldNpcMovementComponent.h"
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
    ParentHousehold,
    /** Farida: 158 cm, sturdy frame, tied-back hair, cream shirt and terracotta apron. */
    ShopProprietor
};

/** Which part of the pose moves a block: the body (torso, head, face) leans and sways; limbs swing about a pivot. */
enum class EPWNpcPartGroup : uint8
{
    Body,
    LegLeft,
    LegRight,
    ArmLeft,
    ArmRight
};

/** Where a block rests (X forward, Y right, Z above the floor, in cm) and the joint its group rotates about. */
struct FPWNpcPartRest
{
    EPWNpcPartGroup Group = EPWNpcPartGroup::Body;
    FVector Rest = FVector::ZeroVector;
    FVector Pivot = FVector::ZeroVector;
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
 * A talkable person for the lane (LANE_LIFE_PLAN section B): a block body on a capsule like the player's. Left alone, it
 * stands, slowly turns toward the player within 3 m and opens the conversation panel when the player interacts. Fed an
 * intent from the portable core (GetNpcMovement()->SetIntent), it walks its route, works at its anchor, sleeps, waits and
 * stops to talk, all presentation: UPWNpcMovementComponent does the moving, this class turns its pose into block rotations.
 * It decides nothing: who they are, what they say and what they are doing belong to the core, which the integrator
 * connects through UPWConversationPresenter::SetSessionFactory and SetIntent.
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

    // IPWInteractable
    virtual FText GetInteractionVerb(const APawn* By) const override;
    virtual FVector GetInteractionPoint() const override;
    virtual int32 GetInteractionPriority() const override;
    virtual void Interact(APawn* By) override;

    UPWNpcMovementComponent* GetNpcMovement() const { return NpcMovement; }

    /**
     * Why they cannot be talked to right now, as a short sentence for the interaction notice ("Farida is asleep."), or
     * empty when nothing declines. The verb stays "Talk to <name>" when the intent is not talkable, so the person can
     * still be targeted; Interact then shows this through UPWInteractionComponent::ShowNotice on the interacting pawn and
     * opens no conversation.
     */
    FText GetDeclineText() const;

    /** Poses the blocks. Bound to the movement component's OnPose; also callable directly (tests, the integrator). */
    void ApplyPose(const FPWNpcPose& Pose);

    FName GetPersonId() const { return PersonId; }
    const FText& GetDisplayName() const { return DisplayName; }

private:
    void BuildBody();

    FName PersonId;
    FText DisplayName;
    EPWNpcOutfit Outfit = EPWNpcOutfit::HouseholdAdult;

    UPROPERTY()
    TObjectPtr<UPWNpcMovementComponent> NpcMovement;

    /** One entry per BodyParts element: where the block rests and which joint it swings about. */
    TArray<FPWNpcPartRest> PartRests;

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
