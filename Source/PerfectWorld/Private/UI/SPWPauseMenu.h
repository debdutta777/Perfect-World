#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldFrontEndHUD.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

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
    TWeakObjectPtr<APerfectWorldFrontEndHUD> Hud;
    FText Status;
    bool bStatusIsError = false;
};
