#include "PerfectWorldMenuGameMode.h"
#include "PerfectWorldCreatorPanel.h"
#include "PerfectWorldAppearancePanel.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Styling/CoreStyle.h"

/**
 * Describing a save slot reads and parses a file, so the Load Game rows
 * cannot ask for it every frame. The screen caches what the slots look
 * like and refreshes whenever the screen is (re)shown.
 */
struct FPWLoadSlotCache
{
    TArray<FText> Descriptions;
    TArray<bool> Occupied;
};

void APerfectWorldMenuGameMode::BeginPlay()
{
    Super::BeginPlay();
    if (!GetGameInstance() || !GetWorld() || !GetWorld()->GetGameViewport()) return;
    Creator = GetGameInstance()->GetSubsystem<UPerfectWorldCharacterCreatorSubsystem>();
    LoadSlotCache = MakeShared<FPWLoadSlotCache>();
    ShowTitle();
}

void APerfectWorldMenuGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
    if (ActiveWidget.IsValid() && GetWorld() && GetWorld()->GetGameViewport())
        GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(ActiveWidget.ToSharedRef());
    ActiveWidget.Reset();
    Super::EndPlay(Reason);
}

void APerfectWorldMenuGameMode::SetActiveWidget(TSharedRef<SWidget> NewWidget)
{
    if (!GetWorld() || !GetWorld()->GetGameViewport()) return;
    if (ActiveWidget.IsValid())
        GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(ActiveWidget.ToSharedRef());
    ActiveWidget = NewWidget;
    GetWorld()->GetGameViewport()->AddViewportWidgetContent(ActiveWidget.ToSharedRef());
    if (auto* PC = GetWorld()->GetFirstPlayerController())
    {
        PC->bShowMouseCursor = true;
        FInputModeUIOnly Mode;
        Mode.SetWidgetToFocus(ActiveWidget);
        PC->SetInputMode(Mode);
    }
}

void APerfectWorldMenuGameMode::RefreshLoadSlotCache()
{
    const int32 SlotCount = UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotCount();
    LoadSlotCache->Descriptions.SetNum(SlotCount);
    LoadSlotCache->Occupied.SetNum(SlotCount);
    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        LoadSlotCache->Occupied[Index] = UPerfectWorldCharacterCreatorSubsystem::IsSaveSlotOccupied(Index);
        LoadSlotCache->Descriptions[Index] =
            Creator.IsValid() ? Creator->DescribeSaveSlot(Index) : FText::GetEmpty();
    }
}

void APerfectWorldMenuGameMode::ShowTitle()
{
    SetActiveWidget(BuildTitleScreen());
}

void APerfectWorldMenuGameMode::ShowLoadGame()
{
    RefreshLoadSlotCache();
    SetActiveWidget(BuildLoadGameScreen());
}

void APerfectWorldMenuGameMode::ShowSettings()
{
    SetActiveWidget(BuildSettingsScreen());
}

void APerfectWorldMenuGameMode::ShowAppearance()
{
    // Record progress as the player moves, so a slot saved from here reopens
    // here rather than at the start of the flow.
    if (Creator.IsValid()) Creator->SetResumePoint(EPWResumePoint::Appearance);
    SetActiveWidget(BuildAppearancePanel(Creator,
        FSimpleDelegate::CreateUObject(this, &APerfectWorldMenuGameMode::ShowCreator),
        FSimpleDelegate::CreateUObject(this, &APerfectWorldMenuGameMode::ShowTitle)));
}

void APerfectWorldMenuGameMode::ShowCreator()
{
    if (Creator.IsValid()) Creator->SetResumePoint(EPWResumePoint::Allocation);
    SetActiveWidget(BuildCreatorPanel(Creator, FSimpleDelegate::CreateUObject(this, &APerfectWorldMenuGameMode::ShowAppearance)));
}

void APerfectWorldMenuGameMode::ResumeFromLoadedRecord()
{
    if (!Creator.IsValid())
    {
        ShowTitle();
        return;
    }
    switch (Creator->GetResumePoint())
    {
    case EPWResumePoint::Appearance:
        ShowAppearance();
        return;
    case EPWResumePoint::InWorld:
        // The origin was confirmed and play had begun, but no playable room
        // exists yet. Land on the confirmed creation, which already states
        // that world gameplay is not built, rather than inventing a screen.
        ShowCreator();
        return;
    case EPWResumePoint::Allocation:
    default:
        ShowCreator();
        return;
    }
}

TSharedRef<SWidget> APerfectWorldMenuGameMode::BuildTitleScreen()
{
    const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 32);
    const FSlateFontInfo SubtitleFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 17);
    const FSlateFontInfo ButtonFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16);

    auto MakeMenuButton = [ButtonFont](const TCHAR* Label, FOnClicked ButtonClicked)
    {
        return SNew(SBox).WidthOverride(260).HeightOverride(44)[
            SNew(SButton)
            .ContentPadding(FMargin(16, 10))
            .OnClicked(ButtonClicked)
            [SNew(STextBlock).Font(ButtonFont).Justification(ETextJustify::Center).Text(FText::FromString(Label))]
        ];
    };

    return SNew(SBorder).Padding(28).BorderBackgroundColor(FLinearColor(0.025f,0.045f,0.07f,1))[
        SNew(SVerticalBox)
        + SVerticalBox::Slot().FillHeight(1).VAlign(VAlign_Center)[
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,0,0,6)[SNew(STextBlock)
                .Font(TitleFont)
                .Text(FText::FromString(TEXT("PERFECT WORLD: AWAKENING")))]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,0,0,40)[SNew(STextBlock)
                .Font(SubtitleFont)
                .Text(FText::FromString(TEXT("New Life prototype")))]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,0,0,10)[
                MakeMenuButton(TEXT("New Game"), FOnClicked::CreateLambda([this]
                {
                    if (Creator.IsValid()) Creator->ResetCreation();
                    ShowAppearance();
                    return FReply::Handled();
                }))]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,0,0,10)[
                MakeMenuButton(TEXT("Load Game"), FOnClicked::CreateLambda([this]
                {
                    ShowLoadGame();
                    return FReply::Handled();
                }))]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,0,0,10)[
                MakeMenuButton(TEXT("Settings"), FOnClicked::CreateLambda([this]
                {
                    ShowSettings();
                    return FReply::Handled();
                }))]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,0,0,10)[
                MakeMenuButton(TEXT("Exit"), FOnClicked::CreateLambda([this]
                {
                    UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
                    return FReply::Handled();
                }))]
        ]
    ];
}

TSharedRef<SWidget> APerfectWorldMenuGameMode::BuildLoadGameScreen()
{
    const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 24);
    const FSlateFontInfo BodyFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 17);
    const FSlateFontInfo ButtonFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16);

    const int32 SlotCount = UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotCount();
    TSharedPtr<FPWLoadSlotCache> Cache = LoadSlotCache;
    auto Rows = SNew(SVerticalBox);
    for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
    {
        Rows->AddSlot().AutoHeight().Padding(6, 5)[
            SNew(SButton)
            .ContentPadding(FMargin(14, 10))
            .IsEnabled_Lambda([Cache, SlotIndex]
            {
                return Cache->Occupied.IsValidIndex(SlotIndex) && Cache->Occupied[SlotIndex];
            })
            .OnClicked_Lambda([this, SlotIndex]
            {
                // Return the player where they left off rather than restarting
                // the flow. A failed load leaves the creation in progress
                // untouched, so stay here and let the message explain why.
                if (!Creator.IsValid()) return FReply::Handled();
                if (!Creator->LoadFromSlot(SlotIndex))
                {
                    RefreshLoadSlotCache();
                    return FReply::Handled();
                }
                ResumeFromLoadedRecord();
                return FReply::Handled();
            })
            [SNew(STextBlock)
                .Font(BodyFont)
                .Text_Lambda([Cache, SlotIndex]
                {
                    if (!Cache->Descriptions.IsValidIndex(SlotIndex)) return FText::GetEmpty();
                    const bool bOccupied = Cache->Occupied.IsValidIndex(SlotIndex) && Cache->Occupied[SlotIndex];
                    return FText::FromString(FString::Printf(TEXT("Slot %d: %s"), SlotIndex + 1,
                        bOccupied ? *Cache->Descriptions[SlotIndex].ToString() : TEXT("Empty slot")));
                })]
        ];
    }

    return SNew(SBorder).Padding(28).BorderBackgroundColor(FLinearColor(0.025f,0.045f,0.07f,1))[
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,16)[SNew(STextBlock)
            .Font(TitleFont)
            .Text(FText::FromString(TEXT("Load Game")))]
        + SVerticalBox::Slot().FillHeight(1).Padding(0,0,0,16)[SNew(SScrollBox)+SScrollBox::Slot()[Rows]]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)[SNew(SBox).WidthOverride(160)[
            SNew(SButton)
            .ContentPadding(FMargin(14, 8))
            .OnClicked_Lambda([this]
            {
                ShowTitle();
                return FReply::Handled();
            })
            [SNew(STextBlock).Font(ButtonFont).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("Back")))]]]
    ];
}

TSharedRef<SWidget> APerfectWorldMenuGameMode::BuildSettingsScreen()
{
    const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 24);
    const FSlateFontInfo BodyFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 17);
    const FSlateFontInfo ButtonFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16);

    UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;

    return SNew(SBorder).Padding(28).BorderBackgroundColor(FLinearColor(0.025f,0.045f,0.07f,1))[
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,16)[SNew(STextBlock)
            .Font(TitleFont)
            .Text(FText::FromString(TEXT("Settings")))]
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)[
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock)
                .Font(BodyFont)
                .Text(FText::FromString(TEXT("Display mode")))]
            + SHorizontalBox::Slot().AutoWidth()[SNew(SButton)
                .ContentPadding(FMargin(12, 8))
                .IsEnabled(Settings != nullptr)
                .OnClicked_Lambda([Settings]
                {
                    if (!Settings) return FReply::Handled();
                    const EWindowMode::Type Current = Settings->GetFullscreenMode();
                    const EWindowMode::Type NextMode = (Current == EWindowMode::Windowed) ? EWindowMode::Fullscreen : EWindowMode::Windowed;
                    Settings->SetFullscreenMode(NextMode);
                    Settings->ApplySettings(false);
                    Settings->SaveSettings();
                    return FReply::Handled();
                })
                [SNew(STextBlock).Font(ButtonFont).Text_Lambda([Settings]
                {
                    if (!Settings) return FText::FromString(TEXT("Unavailable"));
                    return Settings->GetFullscreenMode() == EWindowMode::Windowed
                        ? FText::FromString(TEXT("Windowed (click for Fullscreen)"))
                        : FText::FromString(TEXT("Fullscreen (click for Windowed)"));
                })]]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)[
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock)
                .Font(BodyFont)
                .Text(FText::FromString(TEXT("Resolution scale")))]
            + SHorizontalBox::Slot().AutoWidth().Padding(8,0).VAlign(VAlign_Center)[SNew(STextBlock)
                .Font(BodyFont)
                .Text_Lambda([Settings]
                {
                    if (!Settings) return FText::GetEmpty();
                    return FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Settings->GetResolutionScaleNormalized() * 100.f)));
                })]
            + SHorizontalBox::Slot().FillWidth(2).VAlign(VAlign_Center)[SNew(SBox).MinDesiredWidth(220)[
                SNew(SSlider)
                .IsEnabled(Settings != nullptr)
                .Value_Lambda([Settings] { return Settings ? Settings->GetResolutionScaleNormalized() : 1.f; })
                .OnValueChanged_Lambda([Settings](float NewValue)
                {
                    if (!Settings) return;
                    Settings->SetResolutionScaleNormalized(NewValue);
                    Settings->ApplySettings(false);
                })
                .OnMouseCaptureEnd_Lambda([Settings] { if (Settings) Settings->SaveSettings(); })
            ]]
        ]
        + SVerticalBox::Slot().FillHeight(1)[SNew(SBox)]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)[SNew(SBox).WidthOverride(160)[
            SNew(SButton)
            .ContentPadding(FMargin(14, 8))
            .OnClicked_Lambda([this]
            {
                ShowTitle();
                return FReply::Handled();
            })
            [SNew(STextBlock).Font(ButtonFont).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("Back")))]]]
    ];
}
