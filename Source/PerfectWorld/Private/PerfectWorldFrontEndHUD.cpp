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
#include "PerfectWorldBlockTestGameMode.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldLaneLifePersistence.h"
#include "PerfectWorldPresentationSettings.h"
#include "PerfectWorldTitleView.h"
#include "UI/PWDelegates.h"
#include "UI/PWStartAcknowledgement.h"
#include "UI/SPWFrontEndRoot.h"
#include "Widgets/SWeakWidget.h"

#define LOCTEXT_NAMESPACE "PerfectWorldFrontEndHUD"

namespace
{
/** Slate draws viewport content in Z order; the front end sits above everything the game adds. */
constexpr int32 EnFrontEndZOrder = 100;
/** The core usually explains a refusal; this stands in only if a message ever comes back empty. */
FText EnHudMessageOrFallback(const FText& Message)
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
    GEngine->GameViewport->AddViewportWidgetContent(ViewportHolder.ToSharedRef(), EnFrontEndZOrder);

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
        // The title camera animates itself (and not at all under reduced motion). The HUD only keeps the controller looking
        // through it: a possession or a restart can hand the view back to the pawn while the title is still up.
        const APWTitleView* View = APWTitleView::Find(GetWorld());
        if (View == nullptr || !View->IsViewedBy(*PlayerOwner))
        {
            FaceTitleView();
        }
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
    if (!Creator->CanReplaceWorld())
    {
        Root->ShowToast(Creator->GetLastErrorMessage());
        return;
    }
    Creator->ClearLoadedPlacement();
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
        return EnHudMessageOrFallback(Creator->GetLastErrorMessage());
    }
    ActiveSlot = SlotIndex;

    // A successful load leaves a message only when the primary file was lost and the backup was used.
    const FText Notice = Creator->GetLastErrorMessage();
    if (Creator->HasLifeStarted())
    {
        return EnterWorld(Notice);
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
    if (Creator == nullptr || !Root.IsValid())
    {
        return LOCTEXT("NoCreatorToStart", "The game is not ready to begin a life yet.");
    }
    if (!Creator->StartLifeAndSaveToSlot(SlotIndex))
    {
        return EnHudMessageOrFallback(Creator->GetLastErrorMessage());
    }
    ActiveSlot = SlotIndex;
    const FText Notice = FText::Format(LOCTEXT("StartedAndSaved", "Your life has begun. Saved to slot {0}."),
                                     FText::AsNumber(SlotIndex + 1));
    // The save above succeeded (a failure returned its message already), so a placement that also succeeds earns the cue.
    // Family and care starts both come through here and are treated alike.
    return EnterWorld(Notice, /*bFreshLifeSaved=*/true);
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
        if (Root.IsValid()) Root->OpenSaveChooser();
        return FText::GetEmpty();
    }
    return SaveGameToSlot(Slot, bOutSucceeded);
}

FText APerfectWorldFrontEndHUD::SaveGameToSlot(int32 SlotIndex, bool& bOutSucceeded)
{
    bOutSucceeded = false;
    UPerfectWorldCharacterCreatorSubsystem* Creator = GetCreator();
    if (Creator == nullptr) return LOCTEXT("NothingToSaveInSlot", "There is no life to save.");
    bOutSucceeded = Creator->SaveToSlot(SlotIndex);
    if (bOutSucceeded)
    {
        ActiveSlot = SlotIndex;
    }
    return EnHudMessageOrFallback(Creator->GetLastErrorMessage());
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

FText APerfectWorldFrontEndHUD::EnterWorld(const FText& Notice, const bool bFreshLifeSaved)
{
    if (!Root.IsValid())
    {
        return LOCTEXT("NoWorldEntryUI", "The game is not ready to enter the world yet.");
    }
    UWorld* EnWorld = GetWorld();
    APerfectWorldBlockTestGameMode* EnGameMode = EnWorld != nullptr
        ? Cast<APerfectWorldBlockTestGameMode>(EnWorld->GetAuthGameMode()) : nullptr;
    auto* Creator = GetCreator();
    const auto* Saved = Creator != nullptr && Creator->GetLoadedPlacement() ? &*Creator->GetLoadedPlacement() : nullptr;
    bool Pb2UsedOrigin = false;
    const bool bEnPlaced = PlayerOwner != nullptr && EnGameMode != nullptr &&
        (Saved != nullptr ? PWLaneLifePersistence::PlacePlayerAtSavedPlacement(*PlayerOwner, *Saved, Pb2UsedOrigin)
                          : EnGameMode->PlacePlayerAtOpeningHome(*PlayerOwner));
    if (!bEnPlaced)
    {
        return LOCTEXT("HomeEntryFailed", "There is no safe place to resume. Please load the slot to try again.");
    }
    if (Creator != nullptr) Creator->ClearLoadedPlacement();
    // Placement succeeded: only now does the view go back to the pawn, which has not been moved or turned by the title.
    Mode = EFrontEndMode::Playing;
    UGameplayStatics::SetGamePaused(this, false);
    APWTitleView::Dismiss(*PlayerOwner);
    Root->Hide();
    ApplyGameInput();
    if (!Notice.IsEmpty())
    {
        Root->ShowToast(Notice);
    }
    if (Pb2UsedOrigin && !bFreshLifeSaved)
        Root->ShowToast(LOCTEXT("ResumeUsedOrigin", "Resumed safely at your opening home."));
    // Last, and only now: input already belongs to the game, so the cue (an overlay that fades by itself) delays nothing.
    // A load or Continue is not a start and never plays it.
    if (PWUI::FStartAcknowledgement::ShouldPlay(bFreshLifeSaved, bEnPlaced))
    {
        Root->PlayStartAcknowledgement();
    }
    return FText::GetEmpty();
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
    // The title has its own viewpoint over the morning lane (PerfectWorldTitleView.h). Presenting it changes only what the
    // controller looks through and hides the pawn; the pawn's position, rotation, control rotation, the lane clock (held by
    // Title mode in UPWLaneLifeSubsystem::UpdateClockHold) and every save are untouched.
    if (PlayerOwner != nullptr)
    {
        APWTitleView::Present(*PlayerOwner);
    }
}

#undef LOCTEXT_NAMESPACE
