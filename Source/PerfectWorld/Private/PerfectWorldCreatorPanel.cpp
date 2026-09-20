#include "PerfectWorldCreatorPanel.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Styling/CoreStyle.h"

namespace
{
/**
 * Describing a save slot reads and parses a file, so the slot rows cannot ask
 * for it every frame the way they do for budget values held in memory. The
 * panel caches what the slots look like and refreshes only after a save.
 */
struct FPWCreatorSlotCache
{
    TArray<FText> Descriptions;
    TArray<bool> Occupied;
};
} // namespace

TSharedRef<SWidget> BuildCreatorPanel(TWeakObjectPtr<UPerfectWorldCharacterCreatorSubsystem> Creator,
                                       FSimpleDelegate OnBackToTitle)
{
    const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 24);
    const FSlateFontInfo BodyFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 17);
    const FSlateFontInfo ButtonFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16);

    auto NameField = SNew(SEditableTextBox)
        .Font(BodyFont)
        .Padding(FMargin(10, 8))
        .HintText(FText::FromString(TEXT("Your given name (optional in prototype)")));

    auto Rows = SNew(SVerticalBox);
    if (Creator.IsValid())
    {
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
    }

    auto HouseholdRows = SNew(SVerticalBox);
    if (Creator.IsValid())
    {
        for (const auto& Household : Creator->GetHouseholds())
        {
            const FName Id = Household.Id;
            HouseholdRows->AddSlot().AutoHeight().Padding(6, 4)[
                SNew(SButton)
                .ContentPadding(FMargin(12, 8))
                .IsEnabled_Lambda([Creator] { return Creator.IsValid() && !Creator->GetCreationReview().HasStarted; })
                .OnClicked_Lambda([Creator, Id] { if (Creator.IsValid()) Creator->SetHouseholdTemplate(Id); return FReply::Handled(); })
                [SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock)
                        .Font(BodyFont)
                        .Text_Lambda([Creator, Id]
                        {
                            if (Creator.IsValid()) for (const auto& H : Creator->GetHouseholds()) if (H.Id == Id)
                                return FText::FromString(FString::Printf(TEXT("%s %s"), H.bSelected ? TEXT("[x]") : TEXT("[ ]"), *H.DisplayName.ToString()));
                            return FText::GetEmpty();
                        })]
                    + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock)
                        .Font(BodyFont)
                        .AutoWrapText(true)
                        .Text(Household.GuardianExamples)]
                ]
            ];
        }
    }

    const int32 SlotCount = UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotCount();
    TSharedRef<FPWCreatorSlotCache> SlotCache = MakeShared<FPWCreatorSlotCache>();
    auto RefreshSlots = [Creator, SlotCache, SlotCount]
    {
        SlotCache->Descriptions.SetNum(SlotCount);
        SlotCache->Occupied.SetNum(SlotCount);
        for (int32 Index = 0; Index < SlotCount; ++Index)
        {
            SlotCache->Occupied[Index] = UPerfectWorldCharacterCreatorSubsystem::IsSaveSlotOccupied(Index);
            SlotCache->Descriptions[Index] =
                Creator.IsValid() ? Creator->DescribeSaveSlot(Index) : FText::GetEmpty();
        }
    };
    RefreshSlots();

    auto SaveSlotRows = SNew(SVerticalBox);
    for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
    {
        SaveSlotRows->AddSlot().AutoHeight().Padding(6, 4)[
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock)
                .Font(BodyFont)
                .AutoWrapText(true)
                .Text_Lambda([SlotCache, SlotIndex]
                {
                    if (!SlotCache->Descriptions.IsValidIndex(SlotIndex)) return FText::GetEmpty();
                    const bool bOccupied = SlotCache->Occupied.IsValidIndex(SlotIndex) && SlotCache->Occupied[SlotIndex];
                    return FText::FromString(FString::Printf(TEXT("Slot %d: %s"), SlotIndex + 1,
                        bOccupied ? *SlotCache->Descriptions[SlotIndex].ToString() : TEXT("Empty slot")));
                })]
            + SHorizontalBox::Slot().AutoWidth().Padding(4,0)[SNew(SButton)
                .ContentPadding(FMargin(10, 6))
                .OnClicked_Lambda([Creator, SlotIndex, RefreshSlots]
                {
                    if (Creator.IsValid()) Creator->SaveToSlot(SlotIndex);
                    RefreshSlots();
                    return FReply::Handled();
                })
                [SNew(STextBlock).Font(ButtonFont).Text(FText::FromString(TEXT("Save")))]]
        ];
    }

    return SNew(SBorder).Padding(28).BorderBackgroundColor(FLinearColor(0.025f,0.045f,0.07f,1))[
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[SNew(STextBlock)
            .Font(TitleFont)
            .Text(FText::FromString(TEXT("PERFECT WORLD: AWAKENING | New Life prototype")))]
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[SNew(STextBlock)
            .Font(BodyFont)
            .Text(FText::FromString(TEXT("Kolkata, West Bengal | Nabapara Vidyalaya | Class XI | Age 16")))]
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[NameField]
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,4)[SNew(STextBlock)
            .Font(TitleFont)
            .Text(FText::FromString(TEXT("Household")))]
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)[HouseholdRows]
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[SNew(STextBlock)
            .Font(BodyFont)
            .AutoWrapText(true)
            .Text_Lambda([Creator]
            {
                if (!Creator.IsValid()) return FText::GetEmpty();
                // Ask for the resolved plan rather than the household's own kit:
                // at Household Resources rank 0 the character was raised in care
                // and the chosen template does not describe where they live.
                const FPWOpeningHomePlan Plan = Creator->GetOpeningHome();
                if (Plan.bGrewUpInCare)
                {
                    return FText::FromString(FString::Printf(
                        TEXT("Opening home: %s. Household Resources is rank 0, so this character grew up in institutional care and the household template above does not apply. Raise Household Resources to start in that household instead. %s (the room itself is not yet built)"),
                        *Plan.HomeKitName.ToString(), *Plan.DressingDescription.ToString()));
                }
                return FText::FromString(FString::Printf(
                    TEXT("Opening home: %s, %s. %s (the room itself is not yet built)"),
                    *Plan.HomeKitName.ToString(), *Plan.DressingTierName.ToString(),
                    *Plan.DressingDescription.ToString()));
            })]
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
        + SVerticalBox::Slot().AutoHeight().Padding(0,10,0,4)[SNew(STextBlock)
            .Font(TitleFont)
            .Text(FText::FromString(TEXT("Save Slots")))]
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,0)[SaveSlotRows]
        + SVerticalBox::Slot().AutoHeight().Padding(0,10,0,0)[SNew(STextBlock)
            .Font(BodyFont)
            .AutoWrapText(true)
            .Text(FText::FromString(TEXT("Identity and appearance are free. This desktop prototype implements the point ledger and disk saves; avatar controls and school gameplay are future work.")))]
        + SVerticalBox::Slot().AutoHeight().Padding(0,14,0,0)[SNew(SBox).WidthOverride(160)[SNew(SButton)
            .ContentPadding(FMargin(14, 8))
            .OnClicked_Lambda([OnBackToTitle]
            {
                OnBackToTitle.ExecuteIfBound();
                return FReply::Handled();
            })
            [SNew(STextBlock).Font(ButtonFont).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("Back to Title")))]]]
    ];
}
