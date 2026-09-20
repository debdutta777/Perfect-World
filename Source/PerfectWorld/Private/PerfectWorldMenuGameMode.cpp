#include "PerfectWorldMenuGameMode.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Styling/CoreStyle.h"

void APerfectWorldMenuGameMode::BeginPlay()
{
    Super::BeginPlay();
    if (!GetGameInstance() || !GetWorld()->GetGameViewport()) return;
    TWeakObjectPtr<UPerfectWorldCharacterCreatorSubsystem> Creator =
        GetGameInstance()->GetSubsystem<UPerfectWorldCharacterCreatorSubsystem>();
    if (!Creator.IsValid()) return;
    const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 24);
    const FSlateFontInfo BodyFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 17);
    const FSlateFontInfo ButtonFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16);
    auto NameField = SNew(SEditableTextBox)
        .Font(BodyFont)
        .Padding(FMargin(10, 8))
        .HintText(FText::FromString(TEXT("Your given name (optional in prototype)")));
    auto Rows = SNew(SVerticalBox);
    for (const auto& Trait : Creator->GetTraits())
    {
        const FName Id = Trait.Id;
        auto Change = [Creator, Id](int32 Delta)
        {
            if (Creator.IsValid())
                for (const auto& Current : Creator->GetTraits())
                    if (Current.Id == Id) { Creator->SetTraitRank(Id, Current.Rank + Delta); break; }
            return FReply::Handled();
        };
        Rows->AddSlot().AutoHeight().Padding(6, 5)[
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock)
                .Font(BodyFont)
                .Text(Trait.DisplayName)]
            + SHorizontalBox::Slot().AutoWidth()[SNew(SButton)
                .ContentPadding(FMargin(12, 7))
                .OnClicked_Lambda([Change] { return Change(-1); })
                [SNew(STextBlock).Font(ButtonFont).Text(FText::FromString(TEXT("-")))]]
            + SHorizontalBox::Slot().AutoWidth().Padding(14,0).VAlign(VAlign_Center)[SNew(STextBlock)
                .Font(BodyFont)
                .Text_Lambda([Creator, Id]
                {
                    if (Creator.IsValid()) for (const auto& T : Creator->GetTraits()) if (T.Id == Id)
                        return FText::FromString(FString::Printf(TEXT("Rank %d | %d pts"), T.Rank, T.Cost));
                    return FText::GetEmpty();
                })]
            + SHorizontalBox::Slot().AutoWidth()[SNew(SButton)
                .ContentPadding(FMargin(12, 7))
                .OnClicked_Lambda([Change] { return Change(1); })
                [SNew(STextBlock).Font(ButtonFont).Text(FText::FromString(TEXT("+")))]]
        ];
    }
    Menu = SNew(SBorder).Padding(28).BorderBackgroundColor(FLinearColor(0.025f,0.045f,0.07f,1))[
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[SNew(STextBlock)
            .Font(TitleFont)
            .Text(FText::FromString(TEXT("PERFECT WORLD: AWAKENING | New Life prototype")))]
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[SNew(STextBlock)
            .Font(BodyFont)
            .Text(FText::FromString(TEXT("Kolkata, West Bengal | Nabapara Vidyalaya | Class XI | Age 16")))]
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[NameField]
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[SNew(STextBlock)
            .Font(BodyFont)
            .AutoWrapText(true)
            .Text_Lambda([Creator]
            {
                if (!Creator.IsValid()) return FText::GetEmpty();
                const auto R = Creator->GetCreationReview();
                return FText::FromString(FString::Printf(TEXT("%d / 1000 points remaining | Rank costs: 0, 25, 60, 110, 180 | At most four rank-4 traits"), R.Remaining));
            })]
        + SVerticalBox::Slot().FillHeight(1).Padding(0,2,0,8)[SNew(SScrollBox)+SScrollBox::Slot()[Rows]]
        + SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(STextBlock)
            .Font(BodyFont)
            .AutoWrapText(true)
            .Text_Lambda([Creator]
            {
                if (!Creator.IsValid()) return FText::GetEmpty();
                if (Creator->GetCreationReview().HasStarted)
                    return FText::FromString(TEXT("Origin confirmed. School/world gameplay is planned and is not included in this prototype. New Draft starts a separate creation."));
                return Creator->GetLastErrorMessage();
            })]
        + SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).Padding(4)[SNew(SButton)
                .ContentPadding(FMargin(12, 8))
                .OnClicked_Lambda([Creator] { if (Creator.IsValid()) Creator->ApplyBalancedScholarPreset(); return FReply::Handled(); })
                [SNew(STextBlock).Font(ButtonFont).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("Balanced Scholar preset")))]]
            + SHorizontalBox::Slot().FillWidth(1).Padding(4)[SNew(SButton)
                .ContentPadding(FMargin(12, 8))
                .OnClicked_Lambda([Creator] { if (Creator.IsValid()) Creator->ResetCreation(); return FReply::Handled(); })
                [SNew(STextBlock).Font(ButtonFont).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("New Draft / Reset")))]]
            + SHorizontalBox::Slot().FillWidth(1).Padding(4)[SNew(SButton)
                .ContentPadding(FMargin(12, 8))
                .IsEnabled_Lambda([Creator] { return Creator.IsValid() && Creator->GetCreationReview().ReadyToStart; })
                .OnClicked_Lambda([Creator, NameField]
                    {
                        if (Creator.IsValid())
                        {
                            Creator->SetIdentity(NameField->GetText().ToString(), TEXT(""), TEXT(""), TEXT("Kolkata"), TEXT("Bengali"));
                            Creator->StartLife();
                        }
                        return FReply::Handled();
                    })
                [SNew(STextBlock).Font(ButtonFont).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("Confirm Origin")))]]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0,10,0,0)[SNew(STextBlock)
            .Font(BodyFont)
            .AutoWrapText(true)
            .Text(FText::FromString(TEXT("Identity and appearance are free. This desktop prototype implements the point ledger; avatar controls, saved files and school gameplay are future work.")))]
    ];
    GetWorld()->GetGameViewport()->AddViewportWidgetContent(Menu.ToSharedRef());
    if (auto* PC = GetWorld()->GetFirstPlayerController())
    {
        PC->bShowMouseCursor = true;
        FInputModeUIOnly Mode;
        Mode.SetWidgetToFocus(Menu);
        PC->SetInputMode(Mode);
    }
}

void APerfectWorldMenuGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Menu.IsValid() && GetWorld() && GetWorld()->GetGameViewport())
        GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(Menu.ToSharedRef());
    Menu.Reset();
    Super::EndPlay(Reason);
}
