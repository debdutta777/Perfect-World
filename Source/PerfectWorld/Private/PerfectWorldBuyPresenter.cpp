#include "PerfectWorldBuyPresenter.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldReadingPresenter.h"
#include "UI/SPWBuyPanel.h"
#include "Widgets/SWeakWidget.h"

namespace
{
/** With the reading view (112) and above the conversation panel (110) and the front end (100): they never coexist, but the panel must take the clicks. */
constexpr int32 CmBuyZOrder = 112;
} // namespace

UPWBuyPresenter* UPWBuyPresenter::Get(const UObject* WorldContext)
{
    const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
    return World != nullptr ? World->GetSubsystem<UPWBuyPresenter>() : nullptr;
}

bool UPWBuyPresenter::IsOpenIn(const UObject* WorldContext)
{
    const UPWBuyPresenter* Presenter = Get(WorldContext);
    return Presenter != nullptr && Presenter->IsOpen();
}

void UPWBuyPresenter::Deinitialize()
{
    // The world is going away: remove the panel and let go of the bridge, but do not touch input modes of a controller being destroyed.
    if (Holder.IsValid() && GEngine != nullptr && GEngine->GameViewport != nullptr)
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(Holder.ToSharedRef());
    }
    if (OpenBridge.IsValid())
    {
        OpenBridge->EndService(OpenShopId);
    }
    Holder.Reset();
    Panel.Reset();
    OpenBridge.Reset();
    Controller.Reset();
    OpenShopId = NAME_None;
    Super::Deinitialize();
}

bool UPWBuyPresenter::Open(const TSharedRef<PWCommerce::ICmCommerceWorld>& Bridge, const PWCommerce::FCmServiceView& Service)
{
    if (IsOpen() || !Service.Ok() || Service.Goods.IsEmpty() || GEngine == nullptr || GEngine->GameViewport == nullptr)
    {
        return false;
    }
    const UPWConversationPresenter* Conversation = UPWConversationPresenter::Get(this);
    const UPWReadingPresenter* Reading = UPWReadingPresenter::Get(this);
    if ((Conversation != nullptr && Conversation->IsOpen()) || (Reading != nullptr && Reading->IsOpen()))
    {
        return false; // another panel already owns the screen and the input
    }
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    if (PlayerController == nullptr)
    {
        return false;
    }
    const APerfectWorldFrontEndHUD* Hud = Cast<APerfectWorldFrontEndHUD>(PlayerController->GetHUD());
    if (Hud != nullptr && Hud->IsMenuOpen())
    {
        return false; // a menu already owns the screen and the input
    }

    OpenShopId = Service.ShopId;
    OpenBridge = Bridge;
    Controller = PlayerController;
    Panel = SNew(SPWBuyPanel)
        .Bridge(OpenBridge)
        .Service(Service)
        .OnClose(FSimpleDelegate::CreateUObject(this, &UPWBuyPresenter::Close));
    Holder = SNew(SWeakWidget).PossiblyNullContent(Panel);
    GEngine->GameViewport->AddViewportWidgetContent(Holder.ToSharedRef(), CmBuyZOrder);
    ApplyPanelInput(*PlayerController);
    OnOpened.Broadcast(OpenShopId);
    return true;
}

void UPWBuyPresenter::Close()
{
    if (!IsOpen())
    {
        return;
    }
    // The key handler or button that called this is still on the Slate stack; it holds its own reference to the panel.
    const FName Closed = OpenShopId;
    if (Holder.IsValid() && GEngine != nullptr && GEngine->GameViewport != nullptr)
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(Holder.ToSharedRef());
    }
    const TSharedPtr<PWCommerce::ICmCommerceWorld> Bridge = OpenBridge;
    Holder.Reset();
    Panel.Reset();
    OpenBridge.Reset();
    OpenShopId = NAME_None;
    if (Bridge.IsValid())
    {
        Bridge->EndService(Closed);
    }
    RestoreGameInput();
    Controller.Reset();
    // Last, so listeners (the clock) see a world that is already playable again.
    OnClosed.Broadcast(Closed);
}

void UPWBuyPresenter::ApplyPanelInput(APlayerController& PlayerController) const
{
    // The same switch the reading view and the conversation panel make: Slate gets the keyboard and controller, the walking player
    // stops, and the cursor is shown. Flushing the pressed keys drops the interact key that opened the panel.
    PlayerController.SetShowMouseCursor(true);
    FInputModeUIOnly InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PlayerController.SetInputMode(InputMode);
    PlayerController.FlushPressedKeys();
}

void UPWBuyPresenter::RestoreGameInput() const
{
    APlayerController* PlayerController = Controller.Get();
    if (PlayerController == nullptr)
    {
        return;
    }
    const APerfectWorldFrontEndHUD* Hud = Cast<APerfectWorldFrontEndHUD>(PlayerController->GetHUD());
    if (Hud != nullptr && Hud->IsMenuOpen())
    {
        return; // a menu opened meanwhile and owns the input mode
    }
    PlayerController->SetShowMouseCursor(false);
    PlayerController->SetInputMode(FInputModeGameOnly());
    PlayerController->FlushPressedKeys();
}
