#include "UI/SPWPauseMenu.h"

#include "PerfectWorldCommerceBridge.h"
#include "UI/PWWidgets.h"
#include "UI/SPWInventoryPanel.h"
#include "Widgets/Layout/SBox.h"

// R88: Inventory shares the existing pause menu's focus, layout, time and input policy.
void SPWPauseMenu::OpenInventory(const bool bFromWorld)
{
    if (R88InventoryPanel.IsValid() || bQuestsOpen || bTodayOpen || bSettingsOpen || PendingExit != EPendingExit::None) return;
    PWUI::CancelPendingFocus(AsShared());
    bR88InventoryFromWorld = bFromWorld;
    const TWeakPtr<SWidget> Self = AsShared();
    R88InventoryPanel = SNew(SPWInventoryPanel)
        .Bridge(PWCommerce::CmBridge::Resolve(Hud.IsValid() ? Hud->GetWorld() : nullptr))
        .Space_Lambda([Self]() { return SPWPauseMenu::PauseSpaceOf(Self); })
        .OnClose(FSimpleDelegate::CreateSP(this, &SPWPauseMenu::CloseInventory));
    ContentHolder->SetContent(R88InventoryPanel.ToSharedRef());
}

void SPWPauseMenu::CloseInventory()
{
    PWUI::CancelPendingFocus(AsShared());
    R88InventoryPanel.Reset();
    if (bR88InventoryFromWorld)
    {
        bR88InventoryFromWorld = false;
        if (auto* Owner = Hud.Get()) Owner->ResumeGame();
        return;
    }
    ContentHolder->SetContent(MenuContent.ToSharedRef());
    PWUI::FocusWhenReady(AsShared(), R88InventoryButton);
}
