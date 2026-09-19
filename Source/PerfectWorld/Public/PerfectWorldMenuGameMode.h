#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PerfectWorldMenuGameMode.generated.h"

class SWidget;

/** Prototype creation menu; starting life confirms origin, not a playable city. */
UCLASS()
class PERFECTWORLD_API APerfectWorldMenuGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    TSharedPtr<SWidget> Menu;
};
