#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "PerfectWorldCommerceBridge.h"

#include "PerfectWorldBuyPresenter.generated.h"

class APlayerController;
class SPWBuyPanel;
class SWeakWidget;

/** Fired with the shop id when the buy panel appears or goes away. Native only. */
DECLARE_MULTICAST_DELEGATE_OneParam(FPWBuyStateChanged, FName /*ShopId*/);

/**
 * Opens, shows and closes the paused buy panel (R8-05), the way UPWReadingPresenter does for the reading view: a world subsystem, so the
 * counter that opens it needs no reference to the player or the screen, and its lifetime ends with the world. It owns no rules.
 *
 * Time. The panel is a paused overlay: nothing here advances a clock. The integrator holds the game clock while IsOpen() is true (the
 * same hold as the reading panel, see UPWLaneLifeSubsystem::UpdateClockHold). Opening costs no time and Back commits nothing; a
 * purchase's own command declares its zero minutes.
 *
 * Service. Open is given the offer the world already accepted (BeginService). Closing for any reason ends that service through the
 * bridge (EndService), exactly once, so a counter is never left held open.
 *
 * Input. Opening switches the local controller to UI-only input with the cursor shown and flushes pressed keys, so the interact key
 * that opened the panel is gone before the panel can see it. Closing restores game-only input unless a menu opened meanwhile.
 */
UCLASS()
class PERFECTWORLD_API UPWBuyPresenter : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /** The presenter of the world that WorldContext lives in, or null. */
    static UPWBuyPresenter* Get(const UObject* WorldContext);
    /** True when the world that WorldContext lives in has its buy panel open (false when there is no presenter): the one-line check for the hooks. */
    static bool IsOpenIn(const UObject* WorldContext);

    virtual void Deinitialize() override;

    bool IsOpen() const { return Panel.IsValid(); }
    FName GetOpenShopId() const { return OpenShopId; }
    TSharedPtr<SPWBuyPanel> GetPanel() const { return Panel; }

    /**
     * Shows the panel for an accepted offer. False (and nothing changes) when a panel, a conversation or a reading is already open, a menu is
     * up, there is no screen, or the offer is not Ok with goods. The caller then ends the service itself.
     */
    bool Open(const TSharedRef<PWCommerce::ICmCommerceWorld>& Bridge, const PWCommerce::FCmServiceView& Service);

    /** Closes the panel, ends the service, restores play input, then broadcasts OnClosed. Does nothing when nothing is open. */
    void Close();

    FPWBuyStateChanged OnOpened;
    FPWBuyStateChanged OnClosed;

private:
    void ApplyPanelInput(APlayerController& PlayerController) const;
    void RestoreGameInput() const;

    FName OpenShopId;
    TWeakObjectPtr<APlayerController> Controller;
    TSharedPtr<PWCommerce::ICmCommerceWorld> OpenBridge;
    TSharedPtr<SPWBuyPanel> Panel;
    TSharedPtr<SWeakWidget> Holder;
};
