#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PerfectWorldReadingPresenter.generated.h"

class APlayerController;
class SPWReadingPanel;
class SWeakWidget;

/** Fired with the content id when the reading view appears or goes away. Native only. */
DECLARE_MULTICAST_DELEGATE_OneParam(FPWReadingStateChanged, FName /*ContentId*/);

/**
 * Opens, shows and closes the flat reading view (ROADMAP M1-19): a newspaper, a notice, later a letter or the phone's text.
 *
 * Like the conversation presenter this is a world subsystem: the reading belongs to the world's single local screen, any
 * actor opens it without knowing which pawn the player is, and its lifetime ends with the world. It owns no rules.
 *
 * Time: reading is free (LANE_LIFE_PLAN). Nothing here advances or pauses a clock; the integrator holds the game clock
 * while IsOpen() is true (poll it, or use OnOpened and OnClosed). Opening costs nothing and closing commits nothing.
 *
 * Input: opening switches the local controller to UI-only input with the cursor shown, so the keyboard and controller belong
 * to the panel and the walking player stops. Closing restores game-only input and the hidden cursor. The camera and the
 * engine's pause state are never touched.
 */
UCLASS()
class PERFECTWORLD_API UPWReadingPresenter : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /** The presenter of the world that WorldContext lives in, or null. */
    static UPWReadingPresenter* Get(const UObject* WorldContext);

    /**
     * The built-in text for a content id, or false when the id is unknown (the outputs are then untouched).
     * Known ids: text.nabapara.home_newspaper.
     */
    static bool FindBuiltInReading(FName ContentId, FText& OutTitle, TArray<FText>& OutPages);

    virtual void Deinitialize() override;

    bool IsOpen() const { return Panel.IsValid(); }

    /** The content id of the open reading, or NAME_None. */
    FName GetOpenContentId() const { return OpenContentId; }

    /**
     * Shows the pages. False (and nothing changes) if a reading or a conversation is already open, a menu is up, there is
     * no screen, or Pages is empty.
     */
    bool Open(FName ContentId, FText Title, TArray<FText> Pages);

    /** Looks the id up in the built-in table and opens it. False when the id is unknown or Open() refuses. */
    bool OpenBuiltIn(FName ContentId);

    /** Closes the view, restores play input, then broadcasts OnClosed. Does nothing when nothing is open. */
    void Close();

    /** After the view is shown and input is switched. */
    FPWReadingStateChanged OnOpened;
    /** After the view is gone and play input is back. Not fired when the world is torn down. */
    FPWReadingStateChanged OnClosed;

private:
    void ApplyReadingInput(APlayerController& PlayerController) const;
    void RestoreGameInput() const;

    FName OpenContentId;
    TWeakObjectPtr<APlayerController> Controller;
    TSharedPtr<SPWReadingPanel> Panel;
    TSharedPtr<SWeakWidget> Holder;
};
