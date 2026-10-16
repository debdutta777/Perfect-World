#include "PerfectWorldReadingPresenter.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldFrontEndHUD.h"
#include "UI/SPWReadingPanel.h"
#include "Widgets/SWeakWidget.h"

#define LOCTEXT_NAMESPACE "PerfectWorldReadingPresenter"

namespace
{
/** Above the conversation panel (110) and the front end (100), so the reading view takes the clicks and the keyboard. */
constexpr int32 ReadingZOrder = 112;
} // namespace

UPWReadingPresenter* UPWReadingPresenter::Get(const UObject* WorldContext)
{
    const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
    return World != nullptr ? World->GetSubsystem<UPWReadingPresenter>() : nullptr;
}

bool UPWReadingPresenter::FindBuiltInReading(const FName ContentId, FText& OutTitle, TArray<FText>& OutPages)
{
    // A fictional, neutral local paper of the alternate world, Monday 2 June 2042. No real people, no politics, no religion.
    if (ContentId != FName(TEXT("text.nabapara.home_newspaper")))
    {
        return false;
    }
    OutTitle = LOCTEXT("NewspaperTitle", "The Kolkata Morning Courier");
    OutPages.Reset();
    OutPages.Add(LOCTEXT("NewspaperPage1",
        "Monday, 2 June 2042\n\n"
        "WEATHER\n"
        "A warm, humid morning across the city. Expect a high of 36 degrees this afternoon, with a fair chance of a short "
        "evening shower in the south and east. Winds will be light from the south.\n\n"
        "The weather office says the pre-monsoon showers should become more frequent over the coming week. Residents are "
        "reminded to keep drains in front of their homes clear of leaves and litter, and to carry an umbrella in the "
        "evening."));
    OutPages.Add(LOCTEXT("NewspaperPage2",
        "SCHOOL NOTICE\n"
        "The new term begins today. Classes for all senior secondary students start at the usual morning time, and "
        "students are asked to bring their timetable and their identity card.\n\n"
        "Parents and guardians are invited to collect the term calendar from the school office during office hours. The "
        "calendar lists the half-term break, the dates of the internal tests and the holidays. Students in Class XI "
        "should note that the first unit tests are set for the third week of term."));
    OutPages.Add(LOCTEXT("NewspaperPage3",
        "AROUND THE CITY\n"
        "The municipal corporation has announced that the evening tram service on the central route will run every ten "
        "minutes from this week, instead of every fifteen. Timetables are posted at all stops.\n\n"
        "The corporation also reminds residents that household waste is collected before nine each morning. Please keep "
        "bins on the roadside only until collection, and keep wet and dry waste in separate bags.\n\n"
        "LOCAL MARKETS\n"
        "Fresh fish and seasonal fruit are in good supply this week. Prices at the neighbourhood markets are steady."));
    return true;
}

void UPWReadingPresenter::Deinitialize()
{
    // The world is going away: remove the panel, but do not touch input modes of a controller that is being destroyed.
    if (Holder.IsValid() && GEngine != nullptr && GEngine->GameViewport != nullptr)
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(Holder.ToSharedRef());
    }
    Holder.Reset();
    Panel.Reset();
    Controller.Reset();
    OpenContentId = NAME_None;
    Super::Deinitialize();
}

bool UPWReadingPresenter::OpenBuiltIn(const FName ContentId)
{
    FText Title;
    TArray<FText> Pages;
    return FindBuiltInReading(ContentId, Title, Pages) && Open(ContentId, Title, MoveTemp(Pages));
}

bool UPWReadingPresenter::Open(const FName ContentId, FText Title, TArray<FText> Pages)
{
    if (IsOpen() || Pages.IsEmpty() || GEngine == nullptr || GEngine->GameViewport == nullptr)
    {
        return false;
    }
    const UPWConversationPresenter* Conversation = UPWConversationPresenter::Get(this);
    if (Conversation != nullptr && Conversation->IsOpen())
    {
        return false; // a conversation already owns the screen and the input
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

    OpenContentId = ContentId;
    Controller = PlayerController;
    Panel = SNew(SPWReadingPanel)
        .OnClose(FSimpleDelegate::CreateUObject(this, &UPWReadingPresenter::Close));
    Holder = SNew(SWeakWidget).PossiblyNullContent(Panel);
    GEngine->GameViewport->AddViewportWidgetContent(Holder.ToSharedRef(), ReadingZOrder);
    Panel->SetContent(Title, Pages);
    ApplyReadingInput(*PlayerController);
    OnOpened.Broadcast(ContentId);
    return true;
}

void UPWReadingPresenter::Close()
{
    if (!IsOpen())
    {
        return;
    }
    // The key handler or button that called this is still on the Slate stack; it holds its own reference to the panel.
    const FName Closed = OpenContentId;
    if (Holder.IsValid() && GEngine != nullptr && GEngine->GameViewport != nullptr)
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(Holder.ToSharedRef());
    }
    Holder.Reset();
    Panel.Reset();
    OpenContentId = NAME_None;
    RestoreGameInput();
    Controller.Reset();
    // Last, so listeners (the clock) see a world that is already playable again.
    OnClosed.Broadcast(Closed);
}

void UPWReadingPresenter::ApplyReadingInput(APlayerController& PlayerController) const
{
    // The same switch the conversation panel and the front end make: Slate gets the keyboard and controller, the walking
    // player stops, and the cursor is shown. The view target and control rotation are not touched.
    PlayerController.SetShowMouseCursor(true);
    FInputModeUIOnly InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PlayerController.SetInputMode(InputMode);
    PlayerController.FlushPressedKeys();
}

void UPWReadingPresenter::RestoreGameInput() const
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

#undef LOCTEXT_NAMESPACE
