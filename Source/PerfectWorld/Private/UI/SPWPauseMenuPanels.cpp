#include "UI/SPWPauseMenu.h"

#include "PerfectWorldFrontEndHUD.h"
#include "UI/PWQuestText.h"
#include "UI/PWWidgets.h"
#include "UI/SPWJournalPanel.h"
#include "UI/SPWQuestPanel.h"
#include "UI/SPWSettingsPanel.h"
#include "Widgets/Layout/SBox.h"

// The pause menu's swapped-in panels (Today, Quests, Settings), split from SPWPauseMenu.cpp to keep it under 500 lines.
#define LOCTEXT_NAMESPACE "PerfectWorldPauseMenu"

void SPWPauseMenu::OpenToday()
{
    if (bTodayOpen || PendingExit != EPendingExit::None)
    {
        return;
    }
    PWUI::CancelPendingFocus(AsShared());
    bTodayOpen = true;
    const TWeakPtr<SWidget> Self = AsShared();
    // The journal reads the one authoritative world through the lane-life subsystem (the same path as the clock above); time is already held.
    ContentHolder->SetContent(
        SNew(SPWJournalPanel)
        .Model(PWJournal::ReadModel(Hud.Get()))
        .Space_Lambda([Self]() { return SPWPauseMenu::PauseSpaceOf(Self); })
        .OnClose(FSimpleDelegate::CreateSPLambda(this, [this]() { CloseToday(); })));
}

void SPWPauseMenu::CloseToday()
{
    PWUI::CancelPendingFocus(AsShared());
    bTodayOpen = false;
    ContentHolder->SetContent(MenuContent.ToSharedRef());
    PWUI::FocusWhenReady(AsShared(), TodayButton);
}

void SPWPauseMenu::OpenQuests(const bool bFromWorld)
{
    if (bQuestsOpen || bTodayOpen || bSettingsOpen || PendingExit != EPendingExit::None)
    {
        return;
    }
    PWUI::CancelPendingFocus(AsShared());
    bQuestsOpen = true;
    bQuestsFromWorld = bFromWorld;
    const TWeakPtr<SWidget> Self = AsShared();
    ContentHolder->SetContent(
        SNew(SPWQuestPanel)
        .Model(PWQuest::ReadModel(Hud.Get()))
        .Space_Lambda([Self]() { return SPWPauseMenu::PauseSpaceOf(Self); })
        .OnClose(FSimpleDelegate::CreateSPLambda(this, [this]() { CloseQuests(); })));
}

void SPWPauseMenu::CloseQuests()
{
    PWUI::CancelPendingFocus(AsShared());
    bQuestsOpen = false;
    if (bQuestsFromWorld)
    {
        // Opened with the quests key: closing goes straight back to play.
        bQuestsFromWorld = false;
        if (APerfectWorldFrontEndHUD* Owner = Hud.Get())
        {
            Owner->ResumeGame();
        }
        return;
    }
    ContentHolder->SetContent(MenuContent.ToSharedRef());
    PWUI::FocusWhenReady(AsShared(), QuestsButton);
}

void SPWPauseMenu::OpenSettings()
{
    if (bSettingsOpen || bTodayOpen || bQuestsOpen || PendingExit != EPendingExit::None)
    {
        return;
    }
    PWUI::CancelPendingFocus(AsShared());
    bSettingsOpen = true;
    ContentHolder->SetContent(
        SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
        [
            SNew(SPWSettingsPanel).OnBack(FSimpleDelegate::CreateSPLambda(this, [this]() { CloseSettings(); }))
        ]);
}

void SPWPauseMenu::CloseSettings()
{
    PWUI::CancelPendingFocus(AsShared());
    bSettingsOpen = false;
    ContentHolder->SetContent(MenuContent.ToSharedRef());
    PWUI::FocusWhenReady(AsShared(), SettingsButton);
}

#undef LOCTEXT_NAMESPACE
