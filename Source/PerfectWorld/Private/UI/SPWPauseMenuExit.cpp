#include "UI/SPWPauseMenu.h"

#include "PerfectWorldFrontEndHUD.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// The pause menu's exit confirmation (Return to Title, Quit), split from SPWPauseMenu.cpp to keep that file under 500 lines when
// the Today entry arrived (R4-05). The code is unchanged. The text keys keep the pause menu's namespace.
#define LOCTEXT_NAMESPACE "PerfectWorldPauseMenu"

TSharedRef<SWidget> SPWPauseMenu::BuildExitConfirmation()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const FText Title = PendingExit == EPendingExit::ReturnToTitle
        ? LOCTEXT("ReturnToTitleConfirmTitle", "Return to Title?")
        : LOCTEXT("QuitConfirmTitle", "Quit Perfect World?");
    const FText ConfirmLabel = PendingExit == EPendingExit::ReturnToTitle
        ? LOCTEXT("ConfirmReturnToTitle", "Return to Title")
        : LOCTEXT("ConfirmQuit", "Quit");
    const bool bJustSaved = !Status.IsEmpty() && !bStatusIsError;
    const FText Warning = bJustSaved
        ? LOCTEXT("SavedJustNow", "You saved just now, so nothing will be lost.")
        : LOCTEXT("UnsavedProgressWarning", "Unsaved progress since your last save will be lost.");

    return PWUI::MakeDialog(
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
        [
            PWUI::MakeText(Title, 30, Style.TextPrimary, true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 24.0f)
        [
            PWUI::MakeText(Warning, 18, bJustSaved ? Style.Good : Style.TextSecondary, false, true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
        [
            SNew(SBox).WidthOverride(360.0f)
            [
                PWUI::MakeButton(LOCTEXT("CancelExit", "Cancel"), PWUI::EButtonKind::Primary,
                    FOnClicked::CreateSPLambda(this, [this]()
                    {
                        CancelExitConfirmation();
                        return FReply::Handled();
                    }), true, 22, 0.0f)
            ]
        ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SBox).WidthOverride(360.0f)
            [
                PWUI::MakeButton(ConfirmLabel, PWUI::EButtonKind::Secondary,
                    FOnClicked::CreateSPLambda(this, [this]()
                    {
                        ConfirmExit();
                        return FReply::Handled();
                    }), true, 22, 0.0f)
            ]
        ],
        FMargin(48.0f, 36.0f));
}

void SPWPauseMenu::BeginExitConfirmation(EPendingExit Exit, const TSharedRef<SWidget>& InvokingWidget)
{
    PWUI::CancelPendingFocus(AsShared());
    PendingExit = Exit;
    InvokingControl = InvokingWidget;
    ContentHolder->SetContent(BuildExitConfirmation());
    PWUI::FocusWhenReady(AsShared(), ContentHolder);
}

void SPWPauseMenu::CancelExitConfirmation()
{
    PWUI::CancelPendingFocus(AsShared());
    const TWeakPtr<SWidget> RestoreFocus = InvokingControl;
    PendingExit = EPendingExit::None;
    ContentHolder->SetContent(MenuContent.ToSharedRef());
    InvokingControl.Reset();
    PWUI::FocusWhenReady(AsShared(), RestoreFocus);
}

void SPWPauseMenu::ConfirmExit()
{
    if (APerfectWorldFrontEndHUD* Owner = Hud.Get())
    {
        if (PendingExit == EPendingExit::ReturnToTitle)
        {
            Owner->ReturnToTitle();
        }
        else if (PendingExit == EPendingExit::QuitGame)
        {
            Owner->QuitGame();
        }
    }
}

#undef LOCTEXT_NAMESPACE
