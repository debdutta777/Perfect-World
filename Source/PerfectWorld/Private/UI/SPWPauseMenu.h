#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldFrontEndHUD.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SBox;

/** The in-world pause menu: Resume, Today, Quests, Inventory (R88), Settings, Save, Return to Title, Quit. */
class SPWPauseMenu : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWPauseMenu) {}
        SLATE_ARGUMENT(TWeakObjectPtr<APerfectWorldFrontEndHUD>, Hud)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

    /** The space the menu lays out in (the viewport, or a test's override); shared with the swapped-in panels. */
    static FVector2D PauseSpaceOf(const TWeakPtr<SWidget>& Self);
    void OpenInventory(bool bFromWorld = false); // R88
    TSharedPtr<class SPWInventoryPanel> R88GetInventory() const { return R88InventoryPanel; }

private:
    enum class EPendingExit : uint8
    {
        None,
        ReturnToTitle,
        QuitGame
    };

    TSharedRef<SWidget> BuildMenuContent();
    TSharedRef<SWidget> BuildExitConfirmation();
    void BeginExitConfirmation(EPendingExit Exit, const TSharedRef<SWidget>& InvokingControl);
    void CancelExitConfirmation();
    void ConfirmExit();
    /** Swaps the Today journal in for the menu; closing it puts the menu back and returns focus to the Today entry. */
    void OpenToday();
    void CloseToday();
    /** F3: the quest window; opened from the menu, or straight away when the quests key paused the game (closing then resumes play). */
    void OpenQuests(bool bFromWorld);
    void CloseQuests();
    void CloseInventory(); // R88
    /** F4: settings (with the Controls tab) while playing; closing returns focus to the Settings entry. */
    void OpenSettings();
    void CloseSettings();

    TWeakObjectPtr<APerfectWorldFrontEndHUD> Hud;
    TSharedPtr<SBox> ContentHolder;
    TSharedPtr<SWidget> MenuContent;
    TSharedPtr<SWidget> TodayButton;
    TSharedPtr<SWidget> QuestsButton;
    TSharedPtr<SWidget> R88InventoryButton;
    TSharedPtr<class SPWInventoryPanel> R88InventoryPanel;
    TSharedPtr<SWidget> SettingsButton;
    TSharedPtr<SWidget> ReturnToTitleButton;
    TSharedPtr<SWidget> QuitButton;
    TWeakPtr<SWidget> InvokingControl;
    EPendingExit PendingExit = EPendingExit::None;
    bool bTodayOpen = false;
    bool bQuestsOpen = false;
    bool bQuestsFromWorld = false;
    bool bSettingsOpen = false;
    bool bR88InventoryFromWorld = false;
    FText Status;
    bool bStatusIsError = false;
};
