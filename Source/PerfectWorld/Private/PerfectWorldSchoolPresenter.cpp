#include "PerfectWorldSchoolPresenter.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "PerfectWorldBuyPresenter.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldReadingPresenter.h"
#include "PerfectWorldSchoolLive.h"
#include "UI/SPWAcademicAssessment.h"
#include "UI/SPWClassCoordinate.h"
#include "UI/SPWClassExplain.h"
#include "UI/SPWClassInvestigate.h"
#include "UI/SPWClassModel.h"
#include "UI/SPWSchoolDay.h"
#include "UI/SPWStudyDesk.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWeakWidget.h"

namespace
{
/** With the buy and reading views (112): they never coexist, and the panel must take the clicks. */
constexpr int32 S03SchoolZOrder = 112;
} // namespace

UPWSchoolPresenter* UPWSchoolPresenter::Get(const UObject* WorldContext)
{
    const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
    return World != nullptr ? World->GetSubsystem<UPWSchoolPresenter>() : nullptr;
}

bool UPWSchoolPresenter::IsOpenIn(const UObject* WorldContext)
{
    const UPWSchoolPresenter* Presenter = Get(WorldContext);
    return Presenter != nullptr && Presenter->IsOpen();
}

void UPWSchoolPresenter::Deinitialize()
{
    if (Holder.IsValid() && GEngine != nullptr && GEngine->GameViewport != nullptr)
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(Holder.ToSharedRef());
    }
    Holder.Reset();
    Slot.Reset();
    Panel.Reset();
    Controller.Reset();
    Kind = EPWSchoolPanel::None;
    Super::Deinitialize();
}

FVector2D UPWSchoolPresenter::LogicalSpace() const
{
    FVector2D Size(1920.0, 1080.0);
    if (GEngine != nullptr && GEngine->GameViewport != nullptr)
    {
        FVector2D Pixels;
        GEngine->GameViewport->GetViewportSize(Pixels);
        if (Pixels.X > 1.0 && Pixels.Y > 1.0)
        {
            const float Scale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(FMath::RoundToInt(Pixels.X), FMath::RoundToInt(Pixels.Y)));
            Size = Pixels / FMath::Max(Scale, 0.1f);
        }
    }
    return Size;
}

bool UPWSchoolPresenter::BeginHosting()
{
    if (IsOpen() || GEngine == nullptr || GEngine->GameViewport == nullptr)
    {
        return false;
    }
    const UPWConversationPresenter* Conversation = UPWConversationPresenter::Get(this);
    const UPWReadingPresenter* Reading = UPWReadingPresenter::Get(this);
    if ((Conversation != nullptr && Conversation->IsOpen()) || (Reading != nullptr && Reading->IsOpen()) || UPWBuyPresenter::IsOpenIn(this))
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
    Controller = PlayerController;
    Slot = SNew(SBox);
    Holder = SNew(SWeakWidget).PossiblyNullContent(Slot);
    GEngine->GameViewport->AddViewportWidgetContent(Holder.ToSharedRef(), S03SchoolZOrder);
    PlayerController->SetShowMouseCursor(true);
    FInputModeUIOnly InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PlayerController->SetInputMode(InputMode);
    PlayerController->FlushPressedKeys();
    return true;
}

void UPWSchoolPresenter::ShowPanel(const TSharedRef<SWidget>& Content, const EPWSchoolPanel NewKind)
{
    const bool bWasOpen = IsOpen();
    Panel = Content;
    Kind = NewKind;
    Slot->SetContent(Content);
    if (!bWasOpen)
    {
        OnOpened.Broadcast();
    }
}

bool UPWSchoolPresenter::OpenDay()
{
    UWorld* World = GetWorld();
    if (World == nullptr || !BeginHosting())
    {
        return false;
    }
    TWeakObjectPtr<UPWSchoolPresenter> Weak(this);
    const TSharedRef<SPWSchoolDay> Day = SNew(SPWSchoolDay)
        .World(PWSchoolLive::MakeDayWorld(*World))
        .Space_Lambda([Weak]() { return Weak.IsValid() ? Weak->LogicalSpace() : FVector2D(1920.0, 1080.0); })
        .OnClose(FSimpleDelegate::CreateUObject(this, &UPWSchoolPresenter::Close))
        .OnOpened(FSimpleDelegate::CreateUObject(this, &UPWSchoolPresenter::HandOverToLesson));
    DayPanel = Day;
    ShowPanel(Day, EPWSchoolPanel::Day);
    return true;
}

void UPWSchoolPresenter::HandOverToLesson()
{
    const TSharedPtr<SPWSchoolDay> Day = DayPanel.Pin();
    if (Day.IsValid())
    {
        const FString Id = Day->GetSelectedId();
        // The day panel's button is still on the Slate stack; it holds its own reference.
        OpenLesson(Id);
    }
}

bool UPWSchoolPresenter::OpenLesson(const FString& BindingId)
{
    UWorld* World = GetWorld();
    FString Why;
    const PWSchoolLive::FS03Content* Content = World != nullptr ? PWSchoolLive::SharedContent(*World, Why) : nullptr;
    const PerfectWorld::SchoolLessonBinding* Binding =
        Content != nullptr ? PerfectWorld::FindSchoolLessonBinding(Content->Lessons, std::string(TCHAR_TO_UTF8(*BindingId))) : nullptr;
    if (Binding == nullptr)
    {
        return false;
    }
    if (!IsOpen() && !BeginHosting())
    {
        return false;
    }
    LessonBindingId = BindingId;
    const TSharedPtr<PWClassLesson::IBridge> Bridge = PWSchoolLive::MakeLessonBridge(*World, *Binding);
    TWeakObjectPtr<UPWSchoolPresenter> Weak(this);
    const TAttribute<FVector2D> Space = TAttribute<FVector2D>::CreateLambda([Weak]() { return Weak.IsValid() ? Weak->LogicalSpace() : FVector2D(1920.0, 1080.0); });
    const FSimpleDelegate Back = FSimpleDelegate::CreateUObject(this, &UPWSchoolPresenter::BackToDay);
    TSharedPtr<SWidget> Screen;
    switch (Binding->Structure)
    {
    case PerfectWorld::SchoolStructure::Model:
        Screen = SNew(SPWClassModel).Bridge(Bridge).Space(Space).OnClose(Back);
        break;
    case PerfectWorld::SchoolStructure::Explain:
        Screen = SNew(SPWClassExplain).Bridge(Bridge).Space(Space).OnClose(Back);
        break;
    case PerfectWorld::SchoolStructure::Investigate:
        Screen = SNew(SPWClassInvestigate).Bridge(Bridge).Space(Space).OnClose(Back);
        break;
    case PerfectWorld::SchoolStructure::Coordinate:
        Screen = SNew(SPWClassCoordinate).Bridge(Bridge).Space(Space).OnClose(Back);
        break;
    }
    if (!Screen.IsValid())
    {
        return false;
    }
    // The Back handler does not drop the panel on OnLessonClosed: the screen shows its summary and Back is the exit (Lessons handoff).
    ShowPanel(Screen.ToSharedRef(), EPWSchoolPanel::Lesson);
    return true;
}

void UPWSchoolPresenter::BackToDay()
{
    if (!IsOpen())
    {
        return;
    }
    Close();
    OpenDay(); // a lesson that is still open (core state) is reopened from the seat; leaving the screen never ends it
}

bool UPWSchoolPresenter::OpenDesk()
{
    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return false;
    }
    const TSharedRef<PWA5::IStudyWorld> Bridge = PWSchoolLive::MakeStudyWorld(*World);
    FText Reason;
    if (!Bridge->CanUseDesk(Reason) || !BeginHosting())
    {
        return false;
    }
    TWeakObjectPtr<UPWSchoolPresenter> Weak(this);
    const TSharedRef<SPWStudyDesk> Desk = SNew(SPWStudyDesk)
        .World(Bridge)
        .Space_Lambda([Weak]() { return Weak.IsValid() ? Weak->LogicalSpace() : FVector2D(1920.0, 1080.0); })
        .OnOpenPaper(FA5OpenPaper::CreateUObject(this, &UPWSchoolPresenter::OpenPaper))
        .OnClose(FSimpleDelegate::CreateUObject(this, &UPWSchoolPresenter::Close));
    ShowPanel(Desk, EPWSchoolPanel::Desk);
    return true;
}

void UPWSchoolPresenter::OpenPaper(const std::string& WorkId)
{
    UWorld* World = GetWorld();
    if (World == nullptr || !IsOpen())
    {
        return;
    }
    TWeakObjectPtr<UPWSchoolPresenter> Weak(this);
    const TSharedRef<SPWAcademicAssessment> Paper = SNew(SPWAcademicAssessment)
        .World(PWSchoolLive::MakeStudyWorld(*World))
        .WorkId(WorkId)
        .Space_Lambda([Weak]() { return Weak.IsValid() ? Weak->LogicalSpace() : FVector2D(1920.0, 1080.0); })
        .OnClose(FSimpleDelegate::CreateUObject(this, &UPWSchoolPresenter::BackToDesk));
    ShowPanel(Paper, EPWSchoolPanel::Paper);
}

void UPWSchoolPresenter::BackToDesk()
{
    if (!IsOpen())
    {
        return;
    }
    Close();
    OpenDesk();
}

void UPWSchoolPresenter::Close()
{
    if (!IsOpen())
    {
        return;
    }
    // The key handler or button that called this is still on the Slate stack; it holds its own reference to the panel.
    if (Holder.IsValid() && GEngine != nullptr && GEngine->GameViewport != nullptr)
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(Holder.ToSharedRef());
    }
    Holder.Reset();
    Slot.Reset();
    Panel.Reset();
    DayPanel.Reset();
    Kind = EPWSchoolPanel::None;
    APlayerController* PlayerController = Controller.Get();
    Controller.Reset();
    if (PlayerController != nullptr)
    {
        const APerfectWorldFrontEndHUD* Hud = Cast<APerfectWorldFrontEndHUD>(PlayerController->GetHUD());
        if (Hud == nullptr || !Hud->IsMenuOpen())
        {
            PlayerController->SetShowMouseCursor(false);
            PlayerController->SetInputMode(FInputModeGameOnly());
            PlayerController->FlushPressedKeys();
        }
    }
    OnClosed.Broadcast();
}
