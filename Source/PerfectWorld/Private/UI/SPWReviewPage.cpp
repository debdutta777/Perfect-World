#include "UI/SPWReviewPage.h"

#include "GameClock.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "UI/PWCreationModel.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "UI/SPWAwakeningSigil.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldReviewPage"

namespace
{
float CpReviewWidthOverride = 0.0f;
}

// Rules the review page follows, linked by name so the PerfectWorld.CreationPause automation can exercise them without a
// header (UP-06). Widths are the page's own logical width.
namespace PWCreationPause
{
/** Below 1100 units the background panel drops under the other sections instead of squeezing them beside it. */
bool ReviewStackTraits(const float Width)
{
    return Width < 1100.0f;
}

/** The hero's aperture: 160 where there is room, smaller as the window narrows. */
float ReviewHeroDiameter(const float Width)
{
    return Width >= 1440.0f ? 160.0f : (Width >= 1100.0f ? 120.0f : 88.0f);
}

/** When play opens, from the core's own campaign constants: for example "Monday 2 June 2042, 07:30". */
FText ReviewFirstMorningStamp()
{
    const PerfectWorld::GameInstant Start = PerfectWorld::GameClock::FromCalendar(
        PerfectWorld::CampaignStartYear, PerfectWorld::CampaignStartMonth, PerfectWorld::CampaignStartDay,
        PWLaneLife::CampaignStartMinuteOfDay / 60, PWLaneLife::CampaignStartMinuteOfDay % 60);
    const PerfectWorld::CalendarTime Calendar = PerfectWorld::GameClock::ToCalendar(Start);
    static const TCHAR* const Months[12] = {TEXT("January"), TEXT("February"), TEXT("March"), TEXT("April"), TEXT("May"),
        TEXT("June"), TEXT("July"), TEXT("August"), TEXT("September"), TEXT("October"), TEXT("November"), TEXT("December")};
    const FString Weekday(UTF8_TO_TCHAR(std::string(PerfectWorld::WeekdayName(Calendar.DayOfWeek)).c_str()));
    return FText::FromString(FString::Printf(TEXT("%s %d %s %d, %02d:%02d"), *Weekday, Calendar.Day, Months[FMath::Clamp(Calendar.Month - 1, 0, 11)], Calendar.Year, Calendar.Hour, Calendar.Minute));
}

void SetReviewWidthForTest(const float Width)
{
    CpReviewWidthOverride = Width;
}
} // namespace PWCreationPause

namespace
{
/** The page's own width; before the first layout it reports a roomy window so nothing starts squeezed. */
float CpReviewWidth(const SWidget& Page)
{
    if (CpReviewWidthOverride > 0.0f)
    {
        return CpReviewWidthOverride;
    }
    const float Width = static_cast<float>(Page.GetCachedGeometry().GetLocalSize().X);
    return Width > 1.0f ? Width : 1920.0f;
}

/** One beat of the first morning: a seal, a line, and a quieter line beneath it that collapses when empty. */
TSharedRef<SWidget> CpMakeMorningBeat(const EPWSigilPictogram Pictogram, const TAttribute<FText>& Line, const TAttribute<FText>& Sub)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<STextBlock> SubText = PWUI::MakeText(Sub, 15, Style.TextSecondary, false, true);
    SubText->SetVisibility(TAttribute<EVisibility>::CreateLambda(
        [Sub]() { return Sub.Get(FText::GetEmpty()).IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; }));
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.0f, 2.0f, 14.0f, 0.0f)
        [
            SNew(SPWAwakeningSigil).SizeClass(EPWSigilSizeClass::Seal).Diameter(36).Pictogram(Pictogram)
        ]
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Top)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                PWUI::MakeText(Line, 18, Style.TextPrimary, true, true)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
            [
                SubText
            ]
        ];
}
} // namespace

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

    // The first morning: what the core's summary already says, told as the day it opens on. Nothing here is added to it.
    const TSharedRef<FPWCreationModel> Data = InModel;
    const TSharedRef<SWidget> MorningBeats =
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            PWUI::MakeText(LOCTEXT("FirstMorning", "Your first morning"), 24, Style.TextPrimary, true, true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 12.0f)
        [
            SNew(STextBlock)
            .Tag(TEXT("CpMorningStamp"))
            .Text(PWCreationPause::ReviewFirstMorningStamp())
            .Font(PWUI::Font(17, true))
            .ColorAndOpacity(FSlateColor(Style.Accent))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
        [
            CpMakeMorningBeat(EPWSigilPictogram::Person,
                TAttribute<FText>::CreateLambda([Data]()
                {
                    return FText::Format(LOCTEXT("BeatWhoFmt", "You are {0}, {1}."), Data->Summary.FullName,
                                         FText::AsNumber(Data->Summary.AgeOnCampaignStart));
                }),
                TAttribute<FText>::CreateLambda([Data]()
                {
                    return Data->Summary.HomeCityText.IsEmpty() ? FText::GetEmpty()
                        : FText::Format(LOCTEXT("BeatWhoSubFmt", "Home city: {0}"), Data->Summary.HomeCityText);
                }))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
        [
            CpMakeMorningBeat(EPWSigilPictogram::Doorway,
                TAttribute<FText>::CreateLambda([Data]()
                {
                    return FText::Format(LOCTEXT("BeatWakeFmt", "You wake at {0}."), Data->Summary.HouseholdName);
                }),
                TAttribute<FText>::CreateLambda([Data]()
                {
                    const FText Guardian = Data->Summary.GuardianText.IsEmpty() ? FText::GetEmpty()
                        : FText::Format(LOCTEXT("BeatGuardianFmt", "Guardian: {0}"), Data->Summary.GuardianText);
                    if (Data->Summary.bGrewUpInCare && !Data->Summary.CareNote.IsEmpty())
                    {
                        return Guardian.IsEmpty() ? Data->Summary.CareNote
                            : FText::Format(LOCTEXT("BeatCareFmt", "{0}  {1}"), Guardian, Data->Summary.CareNote);
                    }
                    return Guardian;
                }))
        ]
        + SVerticalBox::Slot().AutoHeight()
        [
            CpMakeMorningBeat(EPWSigilPictogram::Paths,
                TAttribute<FText>::CreateLambda([Data]()
                {
                    return FText::Format(LOCTEXT("BeatStreamFmt", "Your stream is {0}."), Data->Summary.StreamName);
                }),
                TAttribute<FText>::CreateLambda([Data]()
                {
                    return Data->Summary.StreamReason.IsEmpty() ? FText::GetEmpty()
                        : FText::Format(LOCTEXT("ReasonFmt2", "Your reason: {0}"), Data->Summary.StreamReason);
                }))
        ];

    const TSharedRef<SWidget> FirstMorning = PWUI::MakeBracketPanel(
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 24.0f, 0.0f)
        [
            SNew(SPWAwakeningSigil).Diameter_Lambda([this]() { return PWCreationPause::ReviewHeroDiameter(CpReviewWidth(*this)); })
                .Pictogram(EPWSigilPictogram::FirstMorning)
                .FitRing_Lambda([this]() { return CpReviewWidth(*this) < 1440.0f; })
                .Motes_Lambda([this]() { return CpReviewWidth(*this) >= 1440.0f; })
        ]
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
        [
            MorningBeats
        ],
        FMargin(24.0f, 18.0f));

    const auto StackedVisibility = [this](const bool bWhenStacked) -> EVisibility
    {
        return PWCreationPause::ReviewStackTraits(CpReviewWidth(*this)) == bWhenStacked ? EVisibility::Visible : EVisibility::Collapsed;
    };

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            PWUI::MakeText(LOCTEXT("Title", "Ready to begin?"), 32, Style.TextPrimary, true)
        ]
        // The core's verdict comes first: it is what the player needs before anything else on this page.
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 14.0f)
        [
            BuildVerdict()
        ]
        + SVerticalBox::Slot().AutoHeight()
        [
            FirstMorning
        ]
        + SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 14.0f, 0.0f, 0.0f)
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
                    .Tag(TEXT("CpTraitsBeside"))
                    .WidthOverride(560.0f)
                    .Visibility_Lambda([StackedVisibility]() { return StackedVisibility(false); })
                    [
                        BuildTraitsPanel()
                    ]
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 12.0f, 0.0f)
            [
                SNew(SBox)
                .Tag(TEXT("CpTraitsBelow"))
                .Visibility_Lambda([StackedVisibility]() { return StackedVisibility(true); })
                [
                    BuildTraitsPanel()
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
