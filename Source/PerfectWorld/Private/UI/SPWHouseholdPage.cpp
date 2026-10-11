#include "UI/SPWHouseholdPage.h"

#include "UI/PWCreationModel.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldHouseholdPage"

void SPWHouseholdPage::Construct(const FArguments& InArgs, const TSharedRef<FPWCreationModel>& InModel)
{
    Model = InModel;
    const PWUI::FStyle& Style = PWUI::FStyle::Get();

    const TSharedRef<SHorizontalBox> Cards = SNew(SHorizontalBox);
    for (int32 Index = 0; Index < Model->Households.Num(); ++Index)
    {
        Cards->AddSlot().FillWidth(1.0f).Padding(8.0f, 0.0f)[BuildCard(Index)];
    }

    const TSharedRef<STextBlock> HomeText = PWUI::MakeText(
        TAttribute<FText>::CreateLambda([this]() { return Model->Summary.OpeningHomeText; }), 18, Style.TextPrimary, false, true);
    const TSharedRef<STextBlock> CareText = PWUI::MakeText(
        TAttribute<FText>::CreateLambda([this]() { return Model->Summary.CareNote; }), 16, Style.Warn, false, true);
    CareText->SetVisibility(TAttribute<EVisibility>::CreateLambda(
        [this]() { return Model->Summary.bGrewUpInCare ? EVisibility::Visible : EVisibility::Collapsed; }));

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            PWUI::MakeText(LOCTEXT("Title", "Where did you grow up?"), 32, Style.TextPrimary, true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 20.0f)
        [
            PWUI::MakeText(LOCTEXT("Subtitle",
                "Choose the household you live in at the start. It costs no points. Every household has a bed, meals, "
                "school and a guardian; they differ in what the story leans on."),
                17, Style.TextSecondary, false, true)
        ]
        + SVerticalBox::Slot().FillHeight(1.0f)
        [
            Cards
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 20.0f, 0.0f, 0.0f)
        [
            SNew(SBorder)
            .BorderImage(&Style.BrushInset)
            .Padding(FMargin(22.0f, 16.0f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    PWUI::MakeText(LOCTEXT("OpeningHome", "YOUR OPENING HOME"), 13, Style.Accent, true)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                [
                    HomeText
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
                [
                    CareText
                ]
            ]
        ]
    ];
}

TSharedRef<SWidget> SPWHouseholdPage::BuildCard(int32 Index)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const FPWHouseholdView& Household = Model->Households[Index];
    const FName HouseholdId = Household.Id;

    const TSharedRef<SWidget> Content =
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            PWUI::MakeText(Household.DisplayName, 24, Style.TextPrimary, true, true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(Household.GuardianExamples, 16, Style.TextSecondary, false, true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 18.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(LOCTEXT("StoryLabel", "STORY EMPHASIS"), 12, Style.Accent, true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(Household.StoryEmphasis, 16, Style.TextPrimary, false, true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 18.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(LOCTEXT("HomeLabel", "HOME"), 12, Style.Accent, true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(Household.HomeKitName, 16, Style.TextPrimary, false, true)
        ];

    return PWUI::MakeContentButton(
        Content, PWUI::EButtonKind::Card,
        FOnClicked::CreateLambda([this, HouseholdId]()
        {
            Model->Edit([HouseholdId](UPerfectWorldCharacterCreatorSubsystem& Creator)
            {
                return Creator.SetHouseholdTemplate(HouseholdId);
            });
            return FReply::Handled();
        }),
        TAttribute<bool>::CreateLambda([this, Index]()
        {
            return Model->Households.IsValidIndex(Index) && Model->Households[Index].bSelected;
        }),
        true, FMargin(20.0f, 18.0f));
}

#undef LOCTEXT_NAMESPACE
