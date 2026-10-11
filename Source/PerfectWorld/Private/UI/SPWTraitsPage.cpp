#include "UI/SPWTraitsPage.h"

#include "CharacterCreation.h"
#include "UI/PWCreationModel.h"
#include "UI/PWStyle.h"
#include "UI/PWTraitText.h"
#include "UI/PWWidgets.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldTraitsPage"

void SPWTraitsPage::Construct(const FArguments& InArgs, const TSharedRef<FPWCreationModel>& InModel)
{
    Model = InModel;
    const PWUI::FStyle& Style = PWUI::FStyle::Get();

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            PWUI::MakeText(LOCTEXT("Title", "What is your background?"), 32, Style.TextPrimary, true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 14.0f)
        [
            PWUI::MakeText(LOCTEXT("Subtitle",
                "Spend exactly 1,000 points across twenty traits. Each rank costs more than the one before, so a broad "
                "history holds up as well as a specialised one. At most four traits may reach rank 4."),
                16, Style.TextSecondary, false, true)
        ]
        + SVerticalBox::Slot().AutoHeight()
        [
            BuildPointsBar()
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 14.0f)
        [
            BuildPresetRow()
        ]
        + SVerticalBox::Slot().FillHeight(1.0f)
        [
            BuildTraitList()
        ]
    ];
}

TSharedRef<SWidget> SPWTraitsPage::BuildPointsBar()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();

    const auto StatusColor = [this]() -> FLinearColor
    {
        const PWUI::FStyle& S = PWUI::FStyle::Get();
        if (Model->Summary.Spent > Model->Summary.TotalBudget) return S.Danger;
        return Model->Summary.Remaining == 0 ? S.Good : S.Accent;
    };
    const auto Fraction = [this]() -> float
    {
        const int32 Total = FMath::Max(1, Model->Summary.TotalBudget);
        return FMath::Clamp(static_cast<float>(Model->Summary.Spent) / static_cast<float>(Total), 0.0f, 1.0f);
    };

    return SNew(SBorder)
        .BorderImage(&Style.BrushInset)
        .Padding(FMargin(22.0f, 14.0f))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text_Lambda([this]()
                {
                    return FText::Format(LOCTEXT("PointsFmt", "{0} / {1}"), FText::AsNumber(Model->Summary.Spent),
                                         FText::AsNumber(Model->Summary.TotalBudget));
                })
                .Font(PWUI::Font(34, true))
                .ColorAndOpacity_Lambda([StatusColor]() { return FSlateColor(StatusColor()); })
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 6.0f, 22.0f, 0.0f)
            [
                PWUI::MakeText(LOCTEXT("PointsLabel", "points spent"), 16, Style.TextSecondary)
            ]
            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
            [
                SNew(SBorder)
                .BorderImage(&Style.BrushTrack)
                .Padding(3.0f)
                [
                    SNew(SBox)
                    .HeightOverride(18.0f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([Fraction]() { return Fraction(); }))
                        [
                            SNew(SImage)
                            .Image_Lambda([this]() -> const FSlateBrush*
                            {
                                const PWUI::FStyle& S = PWUI::FStyle::Get();
                                if (Model->Summary.Spent > Model->Summary.TotalBudget) return &S.BrushFillDanger;
                                return Model->Summary.Remaining == 0 ? &S.BrushFillGood : &S.BrushFillAccent;
                            })
                        ]
                        + SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([Fraction]() { return 1.0f - Fraction(); }))
                        [
                            SNullWidget::NullWidget
                        ]
                    ]
                ]
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(22.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text_Lambda([this]()
                {
                    return FText::Format(LOCTEXT("RemainingFmt", "{0} left"), FText::AsNumber(Model->Summary.Remaining));
                })
                .Font(PWUI::Font(20, true))
                .ColorAndOpacity_Lambda([StatusColor]() { return FSlateColor(StatusColor()); })
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(26.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text_Lambda([this]()
                {
                    return FText::Format(LOCTEXT("RankFourFmt", "Rank 4 slots: {0} / {1}"),
                                         FText::AsNumber(Model->Summary.RankFourCount),
                                         FText::AsNumber(Model->Summary.MaxRankFour));
                })
                .Font(PWUI::Font(16))
                .ColorAndOpacity(FSlateColor(Style.TextSecondary))
            ]
        ];
}

TSharedRef<SWidget> SPWTraitsPage::BuildPresetRow()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SWrapBox> Row = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(10.0, 10.0));

    Row->AddSlot().VAlign(VAlign_Center)
    [
        PWUI::MakeText(LOCTEXT("PresetsLabel", "Start from a preset:"), 16, Style.TextSecondary)
    ];

    for (int32 Index = 0; Index < Model->Presets.Num(); ++Index)
    {
        const FName PresetId = Model->Presets[Index].Id;
        Row->AddSlot().VAlign(VAlign_Center)
        [
            PWUI::MakeButton(
                Model->Presets[Index].DisplayName, PWUI::EButtonKind::Chip,
                FOnClicked::CreateLambda([this, PresetId]()
                {
                    Model->Edit([PresetId](UPerfectWorldCharacterCreatorSubsystem& Creator)
                    {
                        return Creator.ApplyPreset(PresetId);
                    });
                    return FReply::Handled();
                }),
                true, 16, 0.0f,
                TAttribute<bool>::CreateLambda([this, Index]()
                {
                    return Model->Presets.IsValidIndex(Index) && Model->Presets[Index].bMatchesCurrent;
                }))
        ];
    }

    Row->AddSlot().VAlign(VAlign_Center)
    [
        PWUI::MakeButton(
            LOCTEXT("ClearAll", "Clear all"), PWUI::EButtonKind::Secondary,
            FOnClicked::CreateLambda([this]()
            {
                Model->Edit([](UPerfectWorldCharacterCreatorSubsystem& Creator)
                {
                    bool bAllCleared = true;
                    for (const FPWTraitView& Trait : Creator.GetTraits())
                    {
                        if (Trait.Rank > 0)
                        {
                            bAllCleared = Creator.SetTraitRank(Trait.Id, 0) && bAllCleared;
                        }
                    }
                    return bAllCleared;
                });
                return FReply::Handled();
            }),
            true, 16)
    ];

    Row->AddSlot().VAlign(VAlign_Center)
    [
        PWUI::MakeButton(
            LOCTEXT("ResetDefault", "Reset to default"), PWUI::EButtonKind::Secondary,
            FOnClicked::CreateLambda([this]()
            {
                if (Model->Presets.Num() > 0)
                {
                    const FName DefaultId = Model->Presets[0].Id;
                    Model->Edit([DefaultId](UPerfectWorldCharacterCreatorSubsystem& Creator)
                    {
                        return Creator.ApplyPreset(DefaultId);
                    });
                }
                return FReply::Handled();
            }),
            true, 16)
    ];
    return Row;
}

TSharedRef<SWidget> SPWTraitsPage::BuildTraitList()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SScrollBox> List = SNew(SScrollBox)
        .ScrollBarStyle(&Style.ScrollBar)
        .AllowOverscroll(EAllowOverscroll::No)
        .NavigationDestination(EDescendantScrollDestination::IntoView)
        .ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll);

    FName LastGroup = NAME_None;
    for (int32 Index = 0; Index < Model->Traits.Num(); ++Index)
    {
        const FPWTraitView& Trait = Model->Traits[Index];
        if (Trait.Group != LastGroup)
        {
            LastGroup = Trait.Group;
            List->AddSlot().Padding(2.0f, Index == 0 ? 0.0f : 14.0f, 0.0f, 6.0f)
            [
                PWUI::MakeText(PWUI::GetGroupTitle(Trait.Group), 14, Style.Accent, true)
            ];
        }
        List->AddSlot().Padding(0.0f, 0.0f, 12.0f, 8.0f)
        [
            BuildTraitRow(Index)
        ];
    }
    return List;
}

TSharedRef<SWidget> SPWTraitsPage::BuildTraitRow(int32 Index)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const FPWTraitView& Trait = Model->Traits[Index];
    const PWUI::FTraitText& Text = PWUI::GetTraitText(Trait.Id);
    const FName TraitId = Trait.Id;

    const FText Tip = FText::Format(
        LOCTEXT("TraitTip", "{0}\n{1}\n\n{2}\n\nCumulative cost: rank 1 = {3}, rank 2 = {4}, rank 3 = {5}, rank 4 = {6} points."),
        Trait.DisplayName, Text.Short, Text.Effect,
        FText::AsNumber(PerfectWorld::CharacterCreator::CostForRank(1)),
        FText::AsNumber(PerfectWorld::CharacterCreator::CostForRank(2)),
        FText::AsNumber(PerfectWorld::CharacterCreator::CostForRank(3)),
        FText::AsNumber(PerfectWorld::CharacterCreator::CostForRank(4)));

    const TSharedRef<SBorder> Row =
        SNew(SBorder)
        .BorderImage(&Style.BrushCard)
        .Padding(FMargin(18.0f, 12.0f))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(0.30f).VAlign(VAlign_Center)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    PWUI::MakeText(Trait.DisplayName, 20, Style.TextPrimary, true)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                [
                    PWUI::MakeText(Text.Short, 14, Style.TextSecondary, false, true)
                ]
            ]
            + SHorizontalBox::Slot().FillWidth(0.34f).VAlign(VAlign_Center).Padding(18.0f, 0.0f)
            [
                PWUI::MakeText(Text.Effect, 14, Style.TextMuted, false, true)
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    PWUI::MakeButton(
                        INVTEXT("-"), PWUI::EButtonKind::Secondary,
                        FOnClicked::CreateLambda([this, TraitId, Index]()
                        {
                            SetRank(TraitId, RankAt(Index) - 1);
                            return FReply::Handled();
                        }),
                        TAttribute<bool>::CreateLambda([this, Index]() { return RankAt(Index) > 0; }), 22, 40.0f)
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f)
                [
                    SNew(SBox)
                    .WidthOverride(210.0f)
                    .HeightOverride(30.0f)
                    [
                        SNew(SSlider)
                        .Style(&Style.Slider)
                        .MinValue(0.0f)
                        .MaxValue(static_cast<float>(PerfectWorld::MaxRank))
                        .StepSize(1.0f)
                        .MouseUsesStep(true)
                        .IsFocusable(false)
                        .Value_Lambda([this, Index]() { return static_cast<float>(RankAt(Index)); })
                        .OnValueChanged_Lambda([this, TraitId, Index](float Value)
                        {
                            const int32 Target = FMath::RoundToInt(Value);
                            if (Target != RankAt(Index))
                            {
                                SetRank(TraitId, Target);
                            }
                        })
                    ]
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    PWUI::MakeButton(
                        INVTEXT("+"), PWUI::EButtonKind::Secondary,
                        FOnClicked::CreateLambda([this, TraitId, Index]()
                        {
                            SetRank(TraitId, RankAt(Index) + 1);
                            return FReply::Handled();
                        }),
                        TAttribute<bool>::CreateLambda([this, Index]() { return RankAt(Index) < PerfectWorld::MaxRank; }), 22, 40.0f)
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.0f, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SBox)
                    .WidthOverride(150.0f)
                    [
                        SNew(STextBlock)
                        .Text_Lambda([this, Index]() { return RankSummary(Index); })
                        .Font(PWUI::Font(15, true))
                        .ColorAndOpacity(FSlateColor(Style.TextPrimary))
                    ]
                ]
            ]
        ];
    Row->SetToolTipText(Tip);
    return Row;
}

void SPWTraitsPage::SetRank(FName TraitId, int32 Rank)
{
    Model->Edit([TraitId, Rank](UPerfectWorldCharacterCreatorSubsystem& Creator)
    {
        return Creator.SetTraitRank(TraitId, Rank);
    });
}

int32 SPWTraitsPage::RankAt(int32 Index) const
{
    return Model->Traits.IsValidIndex(Index) ? Model->Traits[Index].Rank : 0;
}

FText SPWTraitsPage::RankSummary(int32 Index) const
{
    if (!Model->Traits.IsValidIndex(Index))
    {
        return FText::GetEmpty();
    }
    const int32 Rank = Model->Traits[Index].Rank;
    const int32 Cost = Model->Traits[Index].Cost;
    if (Rank >= PerfectWorld::MaxRank)
    {
        return FText::Format(LOCTEXT("RankMaxFmt", "Rank {0}   {1} pts (max)"), FText::AsNumber(Rank), FText::AsNumber(Cost));
    }
    const int32 NextCost = PerfectWorld::CharacterCreator::CostForRank(Rank + 1) - PerfectWorld::CharacterCreator::CostForRank(Rank);
    return FText::Format(LOCTEXT("RankFmt", "Rank {0}   {1} pts, next +{2}"), FText::AsNumber(Rank),
                         FText::AsNumber(Cost), FText::AsNumber(NextCost));
}

#undef LOCTEXT_NAMESPACE
