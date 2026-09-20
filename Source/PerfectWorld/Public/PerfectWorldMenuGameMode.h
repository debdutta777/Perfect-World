#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PerfectWorldMenuGameMode.generated.h"

class SWidget;
class UPerfectWorldCharacterCreatorSubsystem;
struct FPWLoadSlotCache;

/**
 * Front-end flow: Title -> New Game / Load Game / Settings, with the
 * character creator (PerfectWorldCreatorPanel.cpp) reached from either
 * New Game or a successful load. This game mode owns only screen
 * switching; each screen's widget tree is built by its own function.
 */
UCLASS()
class PERFECTWORLD_API APerfectWorldMenuGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void ShowTitle();
    void ShowLoadGame();
    void ShowSettings();
    void ShowCreator();

    void SetActiveWidget(TSharedRef<SWidget> NewWidget);
    void RefreshLoadSlotCache();

    TSharedRef<SWidget> BuildTitleScreen();
    TSharedRef<SWidget> BuildLoadGameScreen();
    TSharedRef<SWidget> BuildSettingsScreen();

    TWeakObjectPtr<UPerfectWorldCharacterCreatorSubsystem> Creator;
    TSharedPtr<SWidget> ActiveWidget;
    TSharedPtr<FPWLoadSlotCache> LoadSlotCache;
};
