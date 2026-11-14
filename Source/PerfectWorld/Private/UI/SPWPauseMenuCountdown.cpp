#include "UI/SPWPauseMenu.h"

#include "Engine/World.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "UI/PWWidgets.h"
#include "UI/SPWCountdownPanel.h"
#include "Widgets/Layout/SBox.h"

// P312: same pause host, focus return and input/time ownership as SPWPauseMenuPlanner.cpp.
bool SPWPauseMenu::P312OpenCountdown()
{
    if (P312CountdownPanel.IsValid() || bS03PlannerOpen || R88InventoryPanel.IsValid() || bQuestsOpen || bTodayOpen || bSettingsOpen || PendingExit != EPendingExit::None) return false;
    UWorld* World = Hud.IsValid() ? Hud->GetWorld() : nullptr;
    const auto* Life = UPWLaneLifeSubsystem::Get(World);
    if (Life == nullptr || !Life->GetSim().Countdown()) return false;
    PWUI::CancelPendingFocus(AsShared());
    const auto Space = PauseSpaceOf(AsShared());
    P312CountdownPanel = SNew(SPWCountdownPanel).World(World).ViewportSize(Space)
        .OnClose(FSimpleDelegate::CreateSP(this, &SPWPauseMenu::P312CloseCountdown));
    ContentHolder->SetContent(P312CountdownPanel.ToSharedRef());
    PWUI::FocusWhenReady(AsShared(), P312CountdownPanel->GetInitialFocusTarget());
    return true;
}

void SPWPauseMenu::P312CloseCountdown()
{
    PWUI::CancelPendingFocus(AsShared());
    if (P312CountdownPanel.IsValid()) PWUI::CancelPendingFocus(P312CountdownPanel.ToSharedRef());
    P312CountdownPanel.Reset();
    ContentHolder->SetContent(MenuContent.ToSharedRef());
    PWUI::FocusWhenReady(AsShared(), P312CountdownButton);
}
