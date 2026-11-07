#include "UI/SPWPauseMenu.h"

#include "Engine/World.h"
#include "PerfectWorldSchoolLive.h"
#include "UI/PWWidgets.h"
#include "UI/SPWSchoolPlanner.h"
#include "Widgets/Layout/SBox.h"

// S03: the week planner shares the pause menu's focus, layout, time and input policy (copying SPWPauseMenuInventory.cpp).
void SPWPauseMenu::OpenPlanner()
{
    if (bS03PlannerOpen || R88InventoryPanel.IsValid() || bQuestsOpen || bTodayOpen || bSettingsOpen || PendingExit != EPendingExit::None) return;
    UWorld* World = Hud.IsValid() ? Hud->GetWorld() : nullptr;
    if (World == nullptr) return;
    PWUI::CancelPendingFocus(AsShared());
    bS03PlannerOpen = true;
    const TWeakPtr<SWidget> Self = AsShared();
    ContentHolder->SetContent(
        SNew(SPWSchoolPlanner)
        .World(PWSchoolLive::MakePlannerWorld(*World))
        .Space_Lambda([Self]() { return SPWPauseMenu::PauseSpaceOf(Self); })
        .OnClose(FSimpleDelegate::CreateSP(this, &SPWPauseMenu::ClosePlanner)));
}

void SPWPauseMenu::ClosePlanner()
{
    PWUI::CancelPendingFocus(AsShared());
    bS03PlannerOpen = false;
    ContentHolder->SetContent(MenuContent.ToSharedRef());
    PWUI::FocusWhenReady(AsShared(), S03PlannerButton);
}
