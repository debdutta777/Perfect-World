#include "UI/SPWReviewPage.h"

#include "UI/PWCreationModel.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "UI/SPWAwakeningSigil.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldReviewPage"

void SPWReviewPage::Construct(const FArguments& InArgs, const TSharedRef<FPWCreationModel>& InModel)
{
    Model = InModel;
    OnJumpToStep = InArgs._OnJumpToStep;
    const PWUI::FStyle& Style = PWUI::FStyle::Get();

    const TSharedRef<SWidget> Who =
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(STextBlock)
            .Text_Lambda([this]() { return Model->Summary.FullName; })
            .Font(PWUI::Font(30, true))
            .ColorAndOpacity(FSlateColor(Style.TextPrimary))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(TAttribute<FText>::CreateLambda([this]()
            {
                return FText::Format(LOCTEXT("BornFmt", "Born {0}. Age {1} on 2 June 2042."), Model->Summary.BirthDateText,
                                     FText::AsNumber(Model->Summary.AgeOnCampaignStart));
            }), 17, Style.TextSecondary, false, true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(TAttribute<FText>::CreateLambda([this]()
            {
                return FText::Format(LOCTEXT("DetailsFmt", "Pronouns: {0}    Language: {1}    Home city: {2}"),
                                     Model->Summary.PronounsText, Model->Summary.LanguageText, Model->Summary.HomeCityText);
            }), 17, Style.TextSecondary, false, true)
        ];

    const TSharedRef<SWidget> Household =
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(STextBlock)
            .Text_Lambda([this]() { return Model->Summary.HouseholdName; })
            .Font(PWUI::Font(24, true))
            .ColorAndOpacity(FSlateColor(Style.TextPrimary))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(TAttribute<FText>::CreateLambda([this]()
            {
                return FText::Format(LOCTEXT("GuardianFmt", "Guardian: {0}"), Model->Summary.GuardianText);
            }), 17, Style.TextSecondary, false, true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return Model->Summary.OpeningHomeText; }),
                           16, Style.TextPrimary, false, true)
        ];

    const TSharedRef<STextBlock> ReasonText = PWUI::MakeText(
        TAttribute<FText>::CreateLambda([this]()
        {
            return FText::Format(LOCTEXT("ReasonFmt", "Your reason: {0}"), Model->Summary.StreamReason);
        }), 16, Style.TextSecondary, false, true);
    ReasonText->SetVisibility(TAttribute<EVisibility>::CreateLambda(
        [this]() { return Model->Summary.StreamReason.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; }));

    const TSharedRef<SWidget> School =
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(STextBlock)
            .Text_Lambda([this]() { return Model->Summary.StreamName; })
            .Font(PWUI::Font(24, true))
            .ColorAndOpacity(FSlateColor(Style.TextPrimary))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
        [
            ReasonText
        ];

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            PWUI::MakeText(LOCTEXT("Title", "Ready to begin?"), 32, Style.TextPrimary, true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 14, 0, 0)
        [PWUI::MakeBracketPanel(
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 20, 0)
            [SNew(SPWAwakeningSigil).Diameter(160).Pictogram(EPWSigilPictogram::FirstMorning)
                .Motes_Lambda([this]() { return GetCachedGeometry().GetLocalSize().X >= 1440.0f; })]
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
            [PWUI::MakeText(LOCTEXT("FirstMorning", "Your first morning"), 24, Style.TextPrimary, true, true)])]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 14.0f)
        [
            BuildVerdict()
        ]
        + SVerticalBox::Slot().FillHeight(1.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 12.0f, 0.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 14.0f, 0.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        BuildSection(LOCTEXT("WhoHeading", "WHO YOU ARE"), static_cast<int32>(PWUI::ECreationStep::Identity), Who)
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 0.0f)
                    [
                        BuildSection(LOCTEXT("HouseholdHeading", "WHERE YOU LIVE"), static_cast<int32>(PWUI::ECreationStep::Household), Household)
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 0.0f)
                    [
                        BuildSection(LOCTEXT("SchoolHeading", "YOUR STREAM"), static_cast<int32>(PWUI::ECreationStep::Stream), School)
                    ]
                ]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SBox)
                    .WidthOverride(560.0f)
                    [
                        BuildTraitsPanel()
                    ]
                ]
            ]
        ]
    ];
}

TSharedRef<SWidget> SPWReviewPage::BuildVerdict()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();

    const TSharedRef<STextBlock> Ready = PWUI::MakeText(
        LOCTEXT("ReadyText", "Everything adds up. Choose Start Life when you are ready. Once your life begins these choices are locked."),
        18, Style.Good, true, true);
    Ready->SetVisibility(TAttribute<EVisibility>::CreateLambda(
        [this]() { return Model->Summary.bReadyToStart ? EVisibility::Visible : EVisibility::Collapsed; }));

    const TSharedRef<STextBlock> Blocked = PWUI::MakeText(
        TAttribute<FText>::CreateLambda([this]() { return Model->Summary.BlockingMessage; }), 18, Style.Danger, true, true);
    Blocked->SetVisibility(TAttribute<EVisibility>::CreateLambda(
        [this]() { return Model->Summary.bReadyToStart ? EVisibility::Collapsed : EVisibility::Visible; }));

    const TSharedRef<STextBlock> Warnings = PWUI::MakeText(
        TAttribute<FText>::CreateLambda([this]()
        {
            return FText::Join(FText::FromString(TEXT("\n\n")), Model->Summary.Warnings);
        }), 16, Style.TextSecondary, false, true);
    Warnings->SetVisibility(TAttribute<EVisibility>::CreateLambda(
        [this]() { return Model->Summary.Warnings.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed; }));

    return SNew(SBorder)
        .BorderImage(&Style.BrushInset)
        .Padding(FMargin(22.0f, 14.0f))
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[Ready]
            + SVerticalBox::Slot().AutoHeight()[Blocked]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)[Warnings]
        ];
}

TSharedRef<SWidget> SPWReviewPage::BuildSection(const FText& Heading, int32 StepToEdit, const TSharedRef<SWidget>& Body)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    return SNew(SBorder)
        .BorderImage(&Style.BrushCard)
        .Padding(FMargin(22.0f, 16.0f))
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                [
                    PWUI::MakeText(Heading, 13, Style.Accent, true)
                ]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    PWUI::MakeButton(LOCTEXT("Change", "Change"), PWUI::EButtonKind::Secondary,
                                     FOnClicked::CreateLambda([this, StepToEdit]()
                                     {
                                         OnJumpToStep.ExecuteIfBound(StepToEdit);
                                         return FReply::Handled();
                                     }),
                                     true, 14)
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
            [
                Body
            ]
        ];
}

TSharedRef<SWidget> SPWReviewPage::BuildTraitsPanel()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();

    const TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
    for (int32 Index = 0; Index < Model->Traits.Num(); ++Index)
    {
        Rows->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
        [
            SNew(SHorizontalBox)
            .Visibility_Lambda([this, Index]()
            {
                return Model->Traits.IsValidIndex(Index) && Model->Traits[Index].Rank > 0 ? EVisibility::Visible : EVisibility::Collapsed;
            })
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [
                PWUI::MakeText(TAttribute<FText>::CreateLambda([this, Index]()
                {
                    return Model->Traits.IsValidIndex(Index) ? Model->Traits[Index].DisplayName : FText::GetEmpty();
                }), 17, Style.TextPrimary)
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(12.0f, 0.0f)
            [
                PWUI::MakeText(TAttribute<FText>::CreateLambda([this, Index]()
                {
                    return Model->Traits.IsValidIndex(Index)
                        ? FText::Format(LOCTEXT("RankFmt", "Rank {0}"), FText::AsNumber(Model->Traits[Index].Rank))
                        : FText::GetEmpty();
                }), 17, Style.Accent, true)
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SBox)
                .WidthOverride(90.0f)
                [
                    PWUI::MakeText(TAttribute<FText>::CreateLambda([this, Index]()
                    {
                        return Model->Traits.IsValidIndex(Index)
                            ? FText::Format(LOCTEXT("CostFmt", "{0} pts"), FText::AsNumber(Model->Traits[Index].Cost))
                            : FText::GetEmpty();
                    }), 16, Style.TextSecondary)
                ]
            ]
        ];
    }

    const TSharedRef<SWidget> Body =
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            PWUI::MakeText(TAttribute<FText>::CreateLambda([this]()
            {
                return FText::Format(LOCTEXT("TotalFmt", "{0} / {1} points spent"), FText::AsNumber(Model->Summary.Spent),
                                     FText::AsNumber(Model->Summary.TotalBudget));
            }), 22, Style.TextPrimary, true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
        [
            Rows
        ];
    return BuildSection(LOCTEXT("TraitsHeading", "YOUR BACKGROUND"), static_cast<int32>(PWUI::ECreationStep::Traits), Body);
}

#undef LOCTEXT_NAMESPACE
