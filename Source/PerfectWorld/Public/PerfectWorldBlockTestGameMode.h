#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PerfectWorldBlockTestGameMode.generated.h"

class UStaticMesh;

/**
 * A temporary block test scene for the first packaged build (ROADMAP M0-15).
 *
 * It lays out a field of one-metre blocks, a few block towers, a sky and a
 * sun around the player when play starts, so the cooked game has something
 * to show before the asset pipeline (M1-07 to M1-10) and real maps exist.
 * The player is the engine's default flying pawn: WASD and the mouse to look
 * around. It holds no game state and is not game content; world entry
 * (M1-15) replaces it with a per-map game mode.
 */
UCLASS()
class PERFECTWORLD_API APerfectWorldBlockTestGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    APerfectWorldBlockTestGameMode();

    virtual void StartPlay() override;

private:
    void BuildBlockField();
    void BuildSkyAndSun();

    /** The engine's one-metre cube, referenced here so the cook includes it. */
    UPROPERTY()
    TObjectPtr<UStaticMesh> BlockMesh;
};
