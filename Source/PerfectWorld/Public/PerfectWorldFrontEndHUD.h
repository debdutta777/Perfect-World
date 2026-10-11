#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PerfectWorldFrontEndHUD.generated.h"

class SPWFrontEndRoot;
class SWeakWidget;
class UPerfectWorldCharacterCreatorSubsystem;

/**
 * Owns the first playable front end (ROADMAP M1-15, M1-23): the title screen,
 * the character-creation flow and the in-world pause menu, all drawn with Slate
 * over the 3D scene. It decides which input mode is active and when the world
 * becomes playable. It holds no rules; the character creator subsystem and the
 * portable core behind it do.
 *
 * Flow: play starts on the title screen with UI-only input and the cursor shown,
 * so the pawn cannot move. Starting a life or loading a started save hides the
 * menus and switches to game-only input. Escape in the world opens the pause menu.
 */
UCLASS()
class PERFECTWORLD_API APerfectWorldFrontEndHUD : public AHUD
{
    GENERATED_BODY()

public:
    APerfectWorldFrontEndHUD();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;

    /** Escape or the controller menu button while playing: opens the pause menu, or closes it. */
    void ToggleInWorldMenu();

    /** Resets the creator to a fresh draft and opens the first creation page. */
    void BeginNewCreation();

    /** Loads the most recently written occupied slot. Empty text on success, otherwise the message to show. */
    FText ContinueLatest();

    /** Loads a slot, then enters the world (started life) or reopens the creator (draft). Empty text on success. */
    FText LoadSlot(int32 SlotIndex);

    /** Begins life, saves it to the slot, and enters the world. Empty text on success, otherwise the message. */
    FText StartNewLife(int32 SlotIndex);

    void ResumeGame();

    /** Writes the active life to its slot. The returned text says what happened; bOutSucceeded says whether it worked. */
    FText SaveActiveGame(bool& bOutSucceeded);

    void ReturnToTitle();
    void QuitGame();

    UPerfectWorldCharacterCreatorSubsystem* GetCreator() const;

    /** True while any menu is up and the world is not accepting play input. */
    bool IsMenuOpen() const { return Mode != EFrontEndMode::Playing; }

    /** True while the in-world pause menu is up. */
    bool IsPaused() const { return Mode == EFrontEndMode::Paused; }

private:
    enum class EFrontEndMode : uint8
    {
        Title,
        Playing,
        Paused
    };

    void EnterWorld(const FText& Notice);
    void ApplyMenuInput();
    void ApplyGameInput();
    void FaceTitleView();

    TSharedPtr<SPWFrontEndRoot> Root;
    TSharedPtr<SWeakWidget> ViewportHolder;
    EFrontEndMode Mode = EFrontEndMode::Title;

    /** The slot the running life saves to, or -1 when it has none yet. */
    int32 ActiveSlot = -1;
    float TitleYaw = 0.0f;
};
