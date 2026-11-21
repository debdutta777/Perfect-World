#include "UI/SPWPauseMenu.h"

#include "Engine/World.h"
#include "PerfectWorldExaminationBridge.h"
#include "UI/PWWidgets.h"
#include "UI/SPWExaminationPanel.h"
#include "Widgets/Layout/SBox.h"

bool SPWPauseMenu::P406OpenExamination()
{
    if (P406ExaminationPanel.IsValid() || P312CountdownPanel.IsValid() || bS03PlannerOpen || R88InventoryPanel.IsValid() ||
        bQuestsOpen || bTodayOpen || bSettingsOpen || PendingExit != EPendingExit::None) return false;
    UWorld* P406World = Hud.IsValid() ? Hud->GetWorld() : nullptr;
    if (!P406Exam::P406PauseEntryVisible(P406World)) return false;
    PWUI::CancelPendingFocus(AsShared());
    P406ExaminationPanel = SNew(SPWExaminationPanel).World(P406World).ViewportSize(PauseSpaceOf(AsShared()))
        .OnClose(FSimpleDelegate::CreateSP(this, &SPWPauseMenu::P406CloseExamination));
    ContentHolder->SetContent(P406ExaminationPanel.ToSharedRef());
    PWUI::FocusWhenReady(AsShared(), P406ExaminationPanel->P406InitialFocus());
    return true;
}

void SPWPauseMenu::P406CloseExamination()
{
    PWUI::CancelPendingFocus(AsShared());
    if (P406ExaminationPanel.IsValid()) PWUI::CancelPendingFocus(P406ExaminationPanel.ToSharedRef());
    P406ExaminationPanel.Reset();
    ContentHolder->SetContent(MenuContent.ToSharedRef());
    const TSharedPtr<SWidget> P406Target = P406Exam::P406PauseEntryVisible(Hud.IsValid() ? Hud->GetWorld() : nullptr)
        ? P406ExaminationButton : S03PlannerButton;
    PWUI::FocusWhenReady(AsShared(), P406Target);
}
