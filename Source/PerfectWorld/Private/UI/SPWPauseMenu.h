#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldFrontEndHUD.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SBox;

/** The in-world pause menu: Resume, Save, Quit to Title, Quit. */
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

    TWeakObjectPtr<APerfectWorldFrontEndHUD> Hud;
    TSharedPtr<SBox> ContentHolder;
    TSharedPtr<SWidget> MenuContent;
    TSharedPtr<SWidget> ReturnToTitleButton;
    TSharedPtr<SWidget> QuitButton;
    TWeakPtr<SWidget> InvokingControl;
    EPendingExit PendingExit = EPendingExit::None;
    FText Status;
    bool bStatusIsError = false;
};
