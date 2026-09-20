#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PerfectWorldMenuGameMode.generated.h"

class SWidget;
class UPerfectWorldCharacterCreatorSubsystem;
struct FPWLoadSlotCache;

/**
 * Front-end flow: Title -> New Game / Load Game / Settings. New Game goes
 * Title -> Appearance (PerfectWorldAppearancePanel.cpp) -> Creator
 * (PerfectWorldCreatorPanel.cpp); Back from Appearance returns to Title and
 * Back from the Creator returns to Appearance. Loading a slot goes straight
 * to the Creator, as before. This game mode owns only screen switching;
 * each screen's widget tree is built by its own function.
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
    void ShowAppearance();
    void ShowCreator();

    /** Sends the player to the screen a just-loaded record was left on. */
    void ResumeFromLoadedRecord();

    void SetActiveWidget(TSharedRef<SWidget> NewWidget);
    void RefreshLoadSlotCache();

    TSharedRef<SWidget> BuildTitleScreen();
    TSharedRef<SWidget> BuildLoadGameScreen();
    TSharedRef<SWidget> BuildSettingsScreen();

    TWeakObjectPtr<UPerfectWorldCharacterCreatorSubsystem> Creator;
    TSharedPtr<SWidget> ActiveWidget;
    TSharedPtr<FPWLoadSlotCache> LoadSlotCache;
};
