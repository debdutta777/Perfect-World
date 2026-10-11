#include "PerfectWorldFrontEndHUD.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "UI/PWDelegates.h"
#include "UI/SPWFrontEndRoot.h"
#include "Widgets/SWeakWidget.h"

#define LOCTEXT_NAMESPACE "PerfectWorldFrontEndHUD"

namespace
{
/** Slate draws viewport content in Z order; the front end sits above everything the game adds. */
constexpr int32 FrontEndZOrder = 100;
/** The title view is a slow turn of the camera, so the block scene reads as a place and not a backdrop. */
constexpr float TitleTurnDegreesPerSecond = 2.5f;
constexpr float TitleViewPitchDegrees = -6.0f;

/** The core usually explains a refusal; this stands in only if a message ever comes back empty. */
FText HudMessageOrFallback(const FText& Message)
{
    return Message.IsEmpty() ? LOCTEXT("Unexplained", "That did not work, and no reason was given.") : Message;
}
} // namespace

APerfectWorldFrontEndHUD::APerfectWorldFrontEndHUD()
{
    PrimaryActorTick.bCanEverTick = true;
}

void APerfectWorldFrontEndHUD::BeginPlay()
{
    Super::BeginPlay();

    if (IsRunningDedicatedServer() || GEngine == nullptr || GEngine->GameViewport == nullptr || PlayerOwner == nullptr)
    {
        return;
    }

    Root = SNew(SPWFrontEndRoot).Hud(this);
    ViewportHolder = SNew(SWeakWidget).PossiblyNullContent(Root);
    GEngine->GameViewport->AddViewportWidgetContent(ViewportHolder.ToSharedRef(), FrontEndZOrder);

    // In the world (game-only input) Escape or the controller's menu button opens the pause menu. While a menu is up
    // the keyboard belongs to Slate, and SPWFrontEndRoot routes Escape back here to resume.
    EnableInput(PlayerOwner);
    if (InputComponent != nullptr)
    {
        InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &APerfectWorldFrontEndHUD::ToggleInWorldMenu)
            .bExecuteWhenPaused = true;
        InputComponent->BindKey(EKeys::Gamepad_Special_Right, IE_Pressed, this, &APerfectWorldFrontEndHUD::ToggleInWorldMenu)
            .bExecuteWhenPaused = true;
    }

    // Play opens on the title screen, with the pawn held still behind it.
    Mode = EFrontEndMode::Title;
    Root->ShowTitle();
    ApplyMenuInput();
    FaceTitleView();
}

void APerfectWorldFrontEndHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GEngine != nullptr && GEngine->GameViewport != nullptr && ViewportHolder.IsValid())
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(ViewportHolder.ToSharedRef());
    }
    ViewportHolder.Reset();
    Root.Reset();
    Super::EndPlay(EndPlayReason);
}

void APerfectWorldFrontEndHUD::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Mode == EFrontEndMode::Title && PlayerOwner != nullptr)
    {
        TitleYaw = FRotator::NormalizeAxis(TitleYaw + DeltaSeconds * TitleTurnDegreesPerSecond);
        FaceTitleView();
    }
}

UPerfectWorldCharacterCreatorSubsystem* APerfectWorldFrontEndHUD::GetCreator() const
{
    const UWorld* World = GetWorld();
    UGameInstance* Instance = World != nullptr ? World->GetGameInstance() : nullptr;
    return Instance != nullptr ? Instance->GetSubsystem<UPerfectWorldCharacterCreatorSubsystem>() : nullptr;
}

void APerfectWorldFrontEndHUD::ToggleInWorldMenu()
{
    if (!Root.IsValid())
    {
        return;
    }
    if (Mode == EFrontEndMode::Playing)
    {
        Mode = EFrontEndMode::Paused;
        UGameplayStatics::SetGamePaused(this, true);
        Root->ShowPause();
        ApplyMenuInput();
    }
    else if (Mode == EFrontEndMode::Paused)
    {
        ResumeGame();
    }
}

void APerfectWorldFrontEndHUD::BeginNewCreation()
{
    UPerfectWorldCharacterCreatorSubsystem* Creator = GetCreator();
    if (Creator == nullptr || !Root.IsValid())
    {
        return;
    }
    Creator->BeginNewCreation();
    ActiveSlot = -1;
    Root->ShowCreation(static_cast<int32>(PWUI::ECreationStep::Household));
}

FText APerfectWorldFrontEndHUD::ContinueLatest()
{
    const int32 Slot = UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot();
    if (Slot < 0)
    {
        return LOCTEXT("NothingToContinue", "There is no saved game to continue.");
    }
    return LoadSlot(Slot);
}

FText APerfectWorldFrontEndHUD::LoadSlot(int32 SlotIndex)
{
    UPerfectWorldCharacterCreatorSubsystem* Creator = GetCreator();
    if (Creator == nullptr || !Root.IsValid())
    {
        return LOCTEXT("NoCreator", "The game is not ready to load yet.");
    }
    if (!Creator->LoadFromSlot(SlotIndex))
    {
        return HudMessageOrFallback(Creator->GetLastErrorMessage());
    }
    ActiveSlot = SlotIndex;

    // A successful load leaves a message only when the primary file was lost and the backup was used.
    const FText Notice = Creator->GetLastErrorMessage();
    if (Creator->HasLifeStarted())
    {
        EnterWorld(Notice);
        return FText::GetEmpty();
    }

    // A draft: send the player back to the page they had reached.
    int32 Step = static_cast<int32>(PWUI::ECreationStep::Household);
    switch (Creator->GetResumePoint())
    {
    case EPWResumePoint::Appearance: Step = static_cast<int32>(PWUI::ECreationStep::Household); break;
    case EPWResumePoint::Allocation: Step = static_cast<int32>(PWUI::ECreationStep::Traits); break;
    case EPWResumePoint::Identity: Step = static_cast<int32>(PWUI::ECreationStep::Identity); break;
    case EPWResumePoint::Stream: Step = static_cast<int32>(PWUI::ECreationStep::Stream); break;
    case EPWResumePoint::Review:
    case EPWResumePoint::InWorld: Step = static_cast<int32>(PWUI::ECreationStep::Review); break;
    }
    Root->ShowCreation(Step);
    if (!Notice.IsEmpty())
    {
        Root->ShowToast(Notice);
    }
    return FText::GetEmpty();
}

FText APerfectWorldFrontEndHUD::StartNewLife(int32 SlotIndex)
{
    UPerfectWorldCharacterCreatorSubsystem* Creator = GetCreator();
    if (Creator == nullptr)
    {
        return LOCTEXT("NoCreatorToStart", "The game is not ready to begin a life yet.");
    }
    if (!Creator->StartLife())
    {
        return HudMessageOrFallback(Creator->GetLastErrorMessage());
    }

    // Life has begun and cannot be undone, so enter the world either way and say if the save failed.
    FText Notice;
    if (Creator->SaveToSlot(SlotIndex))
    {
        ActiveSlot = SlotIndex;
        Notice = FText::Format(LOCTEXT("StartedAndSaved", "Your life has begun. Saved to slot {0}."),
                               FText::AsNumber(SlotIndex + 1));
    }
    else
    {
        ActiveSlot = -1;
        Notice = FText::Format(LOCTEXT("StartedNotSaved", "Your life has begun, but it could not be saved: {0} Use Pause, then Save, to try again."),
                               Creator->GetLastErrorMessage());
    }
    EnterWorld(Notice);
    return FText::GetEmpty();
}

void APerfectWorldFrontEndHUD::ResumeGame()
{
    if (Mode != EFrontEndMode::Paused || !Root.IsValid())
    {
        return;
    }
    Mode = EFrontEndMode::Playing;
    UGameplayStatics::SetGamePaused(this, false);
    Root->Hide();
    ApplyGameInput();
}

FText APerfectWorldFrontEndHUD::SaveActiveGame(bool& bOutSucceeded)
{
    bOutSucceeded = false;
    UPerfectWorldCharacterCreatorSubsystem* Creator = GetCreator();
    if (Creator == nullptr)
    {
        return LOCTEXT("NothingToSave", "There is no life to save.");
    }
    const int32 Slot = ActiveSlot >= 0 ? ActiveSlot : UPerfectWorldCharacterCreatorSubsystem::FindFirstEmptySaveSlot();
    if (Slot < 0)
    {
        return LOCTEXT("NoFreeSlot",
                       "All three save slots are in use and this life has none of its own. Quit to the title screen, "
                       "then start a new game to choose a slot to replace.");
    }
    bOutSucceeded = Creator->SaveToSlot(Slot);
    if (bOutSucceeded)
    {
        ActiveSlot = Slot;
    }
    return HudMessageOrFallback(Creator->GetLastErrorMessage());
}

void APerfectWorldFrontEndHUD::ReturnToTitle()
{
    if (!Root.IsValid())
    {
        return;
    }
    UGameplayStatics::SetGamePaused(this, false);
    Mode = EFrontEndMode::Title;
    ActiveSlot = -1;
    Root->ShowTitle();
    ApplyMenuInput();
    FaceTitleView();
}

void APerfectWorldFrontEndHUD::QuitGame()
{
    UKismetSystemLibrary::QuitGame(this, PlayerOwner, EQuitPreference::Quit, false);
}

void APerfectWorldFrontEndHUD::EnterWorld(const FText& Notice)
{
    if (!Root.IsValid())
    {
        return;
    }
    Mode = EFrontEndMode::Playing;
    UGameplayStatics::SetGamePaused(this, false);
    Root->Hide();
    ApplyGameInput();
    if (PlayerOwner != nullptr)
    {
        // Keep facing the way the title view was turned, but level the camera for play.
        PlayerOwner->SetControlRotation(FRotator(0.0f, TitleYaw, 0.0f));
    }
    if (!Notice.IsEmpty())
    {
        Root->ShowToast(Notice);
    }
}

void APerfectWorldFrontEndHUD::ApplyMenuInput()
{
    if (PlayerOwner == nullptr)
    {
        return;
    }
    PlayerOwner->SetShowMouseCursor(true);
    FInputModeUIOnly InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PlayerOwner->SetInputMode(InputMode);
    PlayerOwner->FlushPressedKeys();
}

void APerfectWorldFrontEndHUD::ApplyGameInput()
{
    if (PlayerOwner == nullptr)
    {
        return;
    }
    PlayerOwner->SetShowMouseCursor(false);
    PlayerOwner->SetInputMode(FInputModeGameOnly());
    PlayerOwner->FlushPressedKeys();
}

void APerfectWorldFrontEndHUD::FaceTitleView()
{
    if (PlayerOwner != nullptr)
    {
        PlayerOwner->SetControlRotation(FRotator(TitleViewPitchDegrees, TitleYaw, 0.0f));
    }
}

#undef LOCTEXT_NAMESPACE
