#include "UI/SPWIdentityPage.h"

#include "Misc/DateTime.h"
#include "UI/PWCreationModel.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldIdentityPage"

namespace
{
FText IdentityMonthName(int32 Month)
{
    switch (Month)
    {
    case 1: return LOCTEXT("MonthJan", "January");
    case 2: return LOCTEXT("MonthFeb", "February");
    case 3: return LOCTEXT("MonthMar", "March");
    case 4: return LOCTEXT("MonthApr", "April");
    case 5: return LOCTEXT("MonthMay", "May");
    case 6: return LOCTEXT("MonthJun", "June");
    case 7: return LOCTEXT("MonthJul", "July");
    case 8: return LOCTEXT("MonthAug", "August");
    case 9: return LOCTEXT("MonthSep", "September");
    case 10: return LOCTEXT("MonthOct", "October");
    case 11: return LOCTEXT("MonthNov", "November");
    default: return LOCTEXT("MonthDec", "December");
    }
}

FText IdentityPlain(int32 Number)
{
    return FText::AsCultureInvariant(FString::FromInt(Number));
}

TSharedRef<SWidget> IdentityLabelled(const FText& Label, const TSharedRef<SWidget>& Field)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(2.0f, 0.0f, 0.0f, 6.0f)
        [
            PWUI::MakeText(Label, 13, Style.Accent, true)
        ]
        + SVerticalBox::Slot().AutoHeight()
        [
            Field
        ];
}

TSharedRef<SEditableTextBox> IdentityMakeBox(const FString& Initial, const FText& Hint, int32 MaxLength,
                                     TFunction<void(const FString&)> OnChanged)
{
    return SNew(SEditableTextBox)
        .Style(&PWUI::FStyle::Get().TextBox)
        .Text(FText::FromString(Initial))
        .HintText(Hint)
        .MaximumLength(MaxLength)
        .OnTextChanged_Lambda([OnChanged](const FText& NewText) { OnChanged(NewText.ToString()); });
}
} // namespace

void SPWIdentityPage::Construct(const FArguments& InArgs, const TSharedRef<FPWCreationModel>& InModel)
{
    Model = InModel;
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const FPWIdentityView& Start = Model->Identity;

    const TSharedRef<SEditableTextBox> GivenBox = IdentityMakeBox(
        Start.GivenName, LOCTEXT("GivenHint", "Given name"), 40,
        [this](const FString& Value) { Model->Edit([&Value](UPerfectWorldCharacterCreatorSubsystem& Creator) { return Creator.SetGivenName(Value); }); });
    const TSharedRef<SEditableTextBox> FamilyBox = IdentityMakeBox(
        Start.FamilyName, LOCTEXT("FamilyHint", "Family name"), 40,
        [this](const FString& Value) { Model->Edit([&Value](UPerfectWorldCharacterCreatorSubsystem& Creator) { return Creator.SetFamilyName(Value); }); });
    PronounsBox = IdentityMakeBox(
        Start.Pronouns, LOCTEXT("PronounsHint", "For example she/her, he/him, they/them, or your own"), 16,
        [this](const FString& Value) { Model->Edit([&Value](UPerfectWorldCharacterCreatorSubsystem& Creator) { return Creator.SetPronouns(Value); }); });
    LanguageBox = IdentityMakeBox(
        Start.PrimaryLanguage, LOCTEXT("LanguageHint", "The language you speak most at home"), 16,
        [this](const FString& Value) { Model->Edit([&Value](UPerfectWorldCharacterCreatorSubsystem& Creator) { return Creator.SetPrimaryLanguage(Value); }); });

    const TArray<FString> PronounChoices = {TEXT("she/her"), TEXT("he/him"), TEXT("they/them")};
    const TArray<FString> LanguageChoices = {TEXT("Bengali"), TEXT("Hindi"), TEXT("English"), TEXT("Urdu"), TEXT("Nepali")};

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            PWUI::MakeText(LOCTEXT("Title", "Who are you?"), 32, Style.TextPrimary, true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 22.0f)
        [
            PWUI::MakeText(LOCTEXT("Subtitle",
                "None of this costs points or changes your stats. You are 16 and in Class XI when the story opens in Kolkata "
                "on 2 June 2042."),
                16, Style.TextSecondary, false, true)
        ]
        + SVerticalBox::Slot().FillHeight(1.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 28.0f, 0.0f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 10.0f, 0.0f)
                    [
                        IdentityLabelled(LOCTEXT("GivenLabel", "GIVEN NAME"), GivenBox)
                    ]
                    + SHorizontalBox::Slot().FillWidth(1.0f).Padding(10.0f, 0.0f, 0.0f, 0.0f)
                    [
                        IdentityLabelled(LOCTEXT("FamilyLabel", "FAMILY NAME"), FamilyBox)
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 20.0f, 0.0f, 0.0f)
                [
                    IdentityLabelled(LOCTEXT("PronounsLabel", "PRONOUNS"),
                             SNew(SVerticalBox)
                             + SVerticalBox::Slot().AutoHeight()[PronounsBox.ToSharedRef()]
                             + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)[BuildChipRow(PronounChoices, true)])
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 20.0f, 0.0f, 0.0f)
                [
                    IdentityLabelled(LOCTEXT("LanguageLabel", "PRIMARY LANGUAGE"),
                             SNew(SVerticalBox)
                             + SVerticalBox::Slot().AutoHeight()[LanguageBox.ToSharedRef()]
                             + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)[BuildChipRow(LanguageChoices, false)])
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 20.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]()
                    {
                        return FText::Format(LOCTEXT("HomeCityFmt", "Home city: {0}"), FText::FromString(Model->Identity.HomeCity));
                    })
                    .Font(PWUI::Font(16))
                    .ColorAndOpacity(FSlateColor(Style.TextSecondary))
                ]
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SBox)
                .WidthOverride(560.0f)
                [
                    BuildDatePanel()
                ]
            ]
        ]
    ];
}

TSharedRef<SWidget> SPWIdentityPage::BuildChipRow(const TArray<FString>& Choices, bool bPronouns)
{
    const TSharedRef<SWrapBox> Row = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(10.0, 10.0));
    for (const FString& Choice : Choices)
    {
        Row->AddSlot()
        [
            PWUI::MakeButton(
                FText::FromString(Choice), PWUI::EButtonKind::Chip,
                FOnClicked::CreateLambda([this, Choice, bPronouns]()
                {
                    const TSharedPtr<SEditableTextBox> Box = bPronouns ? PronounsBox : LanguageBox;
                    if (Box.IsValid())
                    {
                        Box->SetText(FText::FromString(Choice));
                    }
                    Model->Edit([&Choice, bPronouns](UPerfectWorldCharacterCreatorSubsystem& Creator)
                    {
                        return bPronouns ? Creator.SetPronouns(Choice) : Creator.SetPrimaryLanguage(Choice);
                    });
                    return FReply::Handled();
                }),
                true, 16, 0.0f,
                TAttribute<bool>::CreateLambda([this, Choice, bPronouns]()
                {
                    return (bPronouns ? Model->Identity.Pronouns : Model->Identity.PrimaryLanguage) == Choice;
                }))
        ];
    }
    return Row;
}

TSharedRef<SWidget> SPWIdentityPage::BuildDatePart(const FText& Label, TAttribute<FText> Value, int32 Part)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
        [
            PWUI::MakeText(Label, 13, Style.TextMuted, true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                PWUI::MakeButton(INVTEXT("-"), PWUI::EButtonKind::Secondary,
                                 FOnClicked::CreateLambda([this, Part]() { StepBirth(Part, -1); return FReply::Handled(); }),
                                 true, 22, 40.0f)
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(Part == 1 ? 130.0f : 70.0f)
                [
                    SNew(STextBlock)
                    .Text(Value)
                    .Font(PWUI::Font(22, true))
                    .ColorAndOpacity(FSlateColor(Style.TextPrimary))
                    .Justification(ETextJustify::Center)
                ]
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                PWUI::MakeButton(INVTEXT("+"), PWUI::EButtonKind::Secondary,
                                 FOnClicked::CreateLambda([this, Part]() { StepBirth(Part, +1); return FReply::Handled(); }),
                                 true, 22, 40.0f)
            ]
        ];
}

TSharedRef<SWidget> SPWIdentityPage::BuildDatePanel()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    return SNew(SBorder)
        .BorderImage(&Style.BrushInset)
        .Padding(FMargin(26.0f, 22.0f))
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                PWUI::MakeText(LOCTEXT("DobLabel", "DATE OF BIRTH"), 13, Style.Accent, true)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text_Lambda([this]() { return Model->Summary.BirthDateText; })
                .Font(PWUI::Font(34, true))
                .ColorAndOpacity(FSlateColor(Style.TextPrimary))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 18.0f)
            [
                SNew(STextBlock)
                .Text_Lambda([this]()
                {
                    return FText::Format(LOCTEXT("AgeFmt", "Age on 2 June 2042: {0}"), IdentityPlain(Model->Identity.AgeOnCampaignStart));
                })
                .Font(PWUI::Font(17))
                .ColorAndOpacity(FSlateColor(Style.Good))
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 16.0f, 0.0f)
                [
                    BuildDatePart(LOCTEXT("DayLabel", "DAY"),
                                  TAttribute<FText>::CreateLambda([this]() { return IdentityPlain(Model->Identity.BirthDay); }), 0)
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 16.0f, 0.0f)
                [
                    BuildDatePart(LOCTEXT("MonthLabel", "MONTH"),
                                  TAttribute<FText>::CreateLambda([this]() { return IdentityMonthName(Model->Identity.BirthMonth); }), 1)
                ]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    BuildDatePart(LOCTEXT("YearLabel", "YEAR"),
                                  TAttribute<FText>::CreateLambda([this]() { return IdentityPlain(Model->Identity.BirthYear); }), 2)
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 20.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text_Lambda([this]()
                {
                    const FPWIdentityView& Id = Model->Identity;
                    return FText::Format(
                        LOCTEXT("WindowFmt",
                                "You are exactly 16 when the story opens, so the date must fall between {0} {1} {2} and {3} {4} {5}. "
                                "The buttons stop at those two dates."),
                        IdentityPlain(Id.EarliestDay), IdentityMonthName(Id.EarliestMonth), IdentityPlain(Id.EarliestYear),
                        IdentityPlain(Id.LatestDay), IdentityMonthName(Id.LatestMonth), IdentityPlain(Id.LatestYear));
                })
                .Font(PWUI::Font(15))
                .ColorAndOpacity(FSlateColor(Style.TextSecondary))
                .AutoWrapText(true)
            ]
        ];
}

void SPWIdentityPage::StepBirth(int32 Part, int32 Direction)
{
    const FPWIdentityView& Id = Model->Identity;
    int32 Year = Id.BirthYear;
    int32 Month = Id.BirthMonth;
    int32 Day = Id.BirthDay;

    if (Part == 0)
    {
        const FDateTime Next = FDateTime(Year, Month, Day) + FTimespan::FromDays(Direction);
        Year = Next.GetYear();
        Month = Next.GetMonth();
        Day = Next.GetDay();
    }
    else if (Part == 1)
    {
        Month += Direction;
        if (Month < 1)
        {
            Month = 12;
            --Year;
        }
        else if (Month > 12)
        {
            Month = 1;
            ++Year;
        }
        Day = FMath::Min(Day, FDateTime::DaysInMonth(Year, Month));
    }
    else
    {
        Year += Direction;
        Day = FMath::Min(Day, FDateTime::DaysInMonth(Year, Month));
    }

    // Stop at the ends of the window. The core still validates whatever is sent.
    FDateTime Candidate(Year, Month, Day);
    const FDateTime Earliest(Id.EarliestYear, Id.EarliestMonth, Id.EarliestDay);
    const FDateTime Latest(Id.LatestYear, Id.LatestMonth, Id.LatestDay);
    if (Candidate < Earliest)
    {
        Candidate = Earliest;
    }
    else if (Candidate > Latest)
    {
        Candidate = Latest;
    }
    Model->Edit([Candidate](UPerfectWorldCharacterCreatorSubsystem& Creator)
    {
        return Creator.SetDateOfBirth(Candidate.GetYear(), Candidate.GetMonth(), Candidate.GetDay());
    });
}

#undef LOCTEXT_NAMESPACE
