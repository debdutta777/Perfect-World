#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PerfectWorldBlockTestGameMode.generated.h"

class UMaterialInterface;
class UStaticMesh;

/**
 * A temporary block scene for the first packaged build (ROADMAP M0-15) and the front end's backdrop.
 *
 * When play starts it builds an early-morning block-built Kolkata lane around the player (see
 * Private/PerfectWorldKolkataLane.h): a road along +X with footpaths, two rows of block buildings with
 * shopfronts, balconies and rooftop tanks, overhead wires and a skyline, lit by a low warm sun, a sky and a
 * touch of fog. The player is the engine's default flying pawn at the origin, two metres above the road:
 * WASD and the mouse to look around. It holds no game state and is not game content; world entry (M1-15)
 * replaces it with a per-map game mode.
 */
UCLASS()
class PERFECTWORLD_API APerfectWorldBlockTestGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    APerfectWorldBlockTestGameMode();

    virtual void StartPlay() override;

private:
    void BuildLane();
    void BuildSkyAndSun();

    /** The engine's one-metre cube, referenced here so the cook includes it. */
    UPROPERTY()
    TObjectPtr<UStaticMesh> BlockMesh;

    /** The engine's basic shape material, painted per colour through dynamic instances ("Color" parameter). */
    UPROPERTY()
    TObjectPtr<UMaterialInterface> BlockMaterial;
};
