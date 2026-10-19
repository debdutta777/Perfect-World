#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldFrontEndHUD.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SBox;

/** The in-world pause menu: Resume, Today (the day's journal), Save, Return to Title, Quit. */
class SPWPauseMenu : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWPauseMenu) {}
        SLATE_ARGUMENT(TWeakObjectPtr<APerfectWorldFrontEndHUD>, Hud)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

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

    TWeakObjectPtr<APerfectWorldFrontEndHUD> Hud;
    TSharedPtr<SBox> ContentHolder;
    TSharedPtr<SWidget> MenuContent;
    TSharedPtr<SWidget> TodayButton;
    TSharedPtr<SWidget> ReturnToTitleButton;
    TSharedPtr<SWidget> QuitButton;
    TWeakPtr<SWidget> InvokingControl;
    EPendingExit PendingExit = EPendingExit::None;
    bool bTodayOpen = false;
    FText Status;
    bool bStatusIsError = false;
};
