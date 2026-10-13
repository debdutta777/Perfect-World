#include "PerfectWorldConversationPresenter.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldTemporaryConversation.h"
#include "UI/SPWConversationPanel.h"
#include "Widgets/SWeakWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogPWConversation, Log, All);

namespace
{
/** Above the front end (100), so the panel takes mouse clicks even though the front end's root covers the screen. */
constexpr int32 ConversationZOrder = 110;
/** The design is up to three choices; more are shown (nothing is hidden) but reported. */
constexpr int32 DesignedMaxChoices = 3;
} // namespace

UPWConversationPresenter* UPWConversationPresenter::Get(const UObject* WorldContext)
{
    const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
    return World != nullptr ? World->GetSubsystem<UPWConversationPresenter>() : nullptr;
}

void UPWConversationPresenter::Deinitialize()
{
    // The world is going away: remove the panel, but do not touch input modes of a controller that is being destroyed.
    if (Holder.IsValid() && GEngine != nullptr && GEngine->GameViewport != nullptr)
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(Holder.ToSharedRef());
    }
    Holder.Reset();
    Panel.Reset();
    Session.Reset();
    Controller.Reset();
    Super::Deinitialize();
}

void UPWConversationPresenter::SetSessionFactory(FPWConversationSessionFactory NewFactory)
{
    Factory = MoveTemp(NewFactory);
}

bool UPWConversationPresenter::BeginConversationWith(const FName PersonId, const FText& DisplayName)
{
    if (IsOpen())
    {
        return false;
    }
    TSharedPtr<IPWConversationSession> NewSession;
    if (Factory)
    {
        NewSession = Factory(PersonId, DisplayName);
    }
    else
    {
        // TEMPORARY: the core Conversation module is not wired in yet.
        NewSession = MakeShared<FPWTemporaryHouseholdConversation>(DisplayName);
    }
    return NewSession.IsValid() && Open(NewSession.ToSharedRef());
}

bool UPWConversationPresenter::Open(const TSharedRef<IPWConversationSession>& NewSession)
{
    if (IsOpen() || GEngine == nullptr || GEngine->GameViewport == nullptr)
    {
        return false;
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

    Session = NewSession;
    Controller = PlayerController;
    CurrentView = Session->GetView();
    if (CurrentView.Choices.Num() > DesignedMaxChoices)
    {
        UE_LOG(LogPWConversation, Warning, TEXT("A conversation line offered %d choices; the panel is designed for %d."),
               CurrentView.Choices.Num(), DesignedMaxChoices);
    }

    Panel = SNew(SPWConversationPanel)
        .OnChoose(FPWConversationChooseDelegate::CreateUObject(this, &UPWConversationPresenter::ChooseOption))
        .OnLeave(FSimpleDelegate::CreateUObject(this, &UPWConversationPresenter::LeaveConversation));
    Holder = SNew(SWeakWidget).PossiblyNullContent(Panel);
    GEngine->GameViewport->AddViewportWidgetContent(Holder.ToSharedRef(), ConversationZOrder);
    Panel->SetView(CurrentView);
    ApplyConversationInput(*PlayerController);
    return true;
}

void UPWConversationPresenter::ChooseOption(const FName ChoiceId)
{
    if (!Session.IsValid() || !Panel.IsValid())
    {
        return;
    }
    // Ignore a choice the current line did not offer (a stale click on a row that was just replaced).
    const bool bOffered = CurrentView.Choices.ContainsByPredicate([ChoiceId](const FPWConversationChoice& Choice)
    {
        return Choice.Id == ChoiceId;
    });
    if (!bOffered)
    {
        return;
    }

    // Keep the session alive across a call that may close this conversation.
    const TSharedPtr<IPWConversationSession> Active = Session;
    Active->Choose(ChoiceId);
    if (Active->IsFinished())
    {
        ClosePanelAndRestoreInput();
        return;
    }
    CurrentView = Active->GetView();
    Panel->SetView(CurrentView);
}

void UPWConversationPresenter::LeaveConversation()
{
    if (!Session.IsValid() || !CurrentView.bCanLeave)
    {
        return;
    }
    // Close first, so the session hears about the leave with play input already restored.
    const TSharedPtr<IPWConversationSession> Ended = Session;
    ClosePanelAndRestoreInput();
    Ended->Leave();
}

void UPWConversationPresenter::ClosePanelAndRestoreInput()
{
    if (Holder.IsValid() && GEngine != nullptr && GEngine->GameViewport != nullptr)
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(Holder.ToSharedRef());
    }
    Holder.Reset();
    Panel.Reset();
    Session.Reset();
    CurrentView = FPWConversationView();
    RestoreGameInput();
    Controller.Reset();
}

void UPWConversationPresenter::ApplyConversationInput(APlayerController& PlayerController) const
{
    // The same switch the front end makes for its menus: Slate gets the keyboard and controller, the walking player stops,
    // and the cursor is shown. The view target and control rotation are not touched, so the camera stays where it is.
    PlayerController.SetShowMouseCursor(true);
    FInputModeUIOnly InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PlayerController.SetInputMode(InputMode);
    PlayerController.FlushPressedKeys();
}

void UPWConversationPresenter::RestoreGameInput() const
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
