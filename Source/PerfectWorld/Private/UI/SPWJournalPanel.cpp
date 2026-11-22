#include "UI/SPWJournalPanel.h"

#include "GameClock.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "LaneJournal.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "UI/SPWAwakeningSigil.h"
#include "WorldState.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldJournal"

namespace PWJournal
{
FText AgreementText(const FString& ShortTextKey)
{
    const FString Prefix(AgreementKeyPrefix);
    if (ShortTextKey.StartsWith(Prefix, ESearchCase::CaseSensitive))
    {
        // Short text for each ShortTextKey the portable journal maps (LaneJournal.cpp; the meanings are in Docs notes for R4-04).
        // These describe agreements, not completed work.
        const FString Suffix = ShortTextKey.Mid(Prefix.Len());
        // FString's == ignores case; keys are exact.
        const auto Is = [&Suffix](const TCHAR* Key) { return Suffix.Equals(Key, ESearchCase::CaseSensitive); };
        if (Is(TEXT("evening_rota_agreed"))) return NSLOCTEXT("PerfectWorldJournal", "AgrEveningRotaAgreed", "Share the evening jobs. The details are not settled yet.");
        if (Is(TEXT("evening_rota_dishes_weekdays"))) return NSLOCTEXT("PerfectWorldJournal", "AgrEveningRotaDishes", "You take the dishes on weekdays.");
        if (Is(TEXT("evening_rota_alternate_days"))) return NSLOCTEXT("PerfectWorldJournal", "AgrEveningRotaAlternate", "Alternate days for the evening jobs.");
        if (Is(TEXT("evening_rota_sweep_and_cook"))) return NSLOCTEXT("PerfectWorldJournal", "AgrEveningRotaSweepCook", "You sweep and they cook.");
        if (Is(TEXT("care_jobs_breakfast_weekdays"))) return NSLOCTEXT("PerfectWorldJournal", "AgrCareBreakfast", "You set out breakfast on weekdays.");
        if (Is(TEXT("care_jobs_turns"))) return NSLOCTEXT("PerfectWorldJournal", "AgrCareTurns", "Take turns with the others, day by day.");
        if (Is(TEXT("care_jobs_study_room"))) return NSLOCTEXT("PerfectWorldJournal", "AgrCareStudyRoom", "You tidy the study room after homework hour.");
        if (Is(TEXT("nirmal_bose.book_evening_agreed"))) return NSLOCTEXT("PerfectWorldJournal", "AgrNirmalBook", "The book evening you agreed on.");
        if (Is(TEXT("rupa_ghosh.visit_promised"))) return NSLOCTEXT("PerfectWorldJournal", "AgrRupaVisit", "The visit you promised.");
        if (Is(TEXT("sahana_mitra.sewing_evening_agreed"))) return NSLOCTEXT("PerfectWorldJournal", "AgrSahanaSewing", "The sewing evening you agreed on.");
        if (Is(TEXT("subrata_dey.step_promised"))) return NSLOCTEXT("PerfectWorldJournal", "AgrSubrataStep", "The help with the step you promised.");
        if (Is(TEXT("debashis_pal.bring_promised"))) return NSLOCTEXT("PerfectWorldJournal", "AgrDebashisBring", "Bring something in for him to look at.");
        if (Is(TEXT("mahua_saha.sort_promised"))) return NSLOCTEXT("PerfectWorldJournal", "AgrMahuaSort", "Help sort loose screws one quiet afternoon.");
        if (Is(TEXT("prabir_chatterjee.lane_stories_agreed"))) return NSLOCTEXT("PerfectWorldJournal", "AgrPrabirStories", "Talk about the lane with Prabir another afternoon.");
        if (Is(TEXT("ishita_banerjee.quiet_evening_agreed"))) return NSLOCTEXT("PerfectWorldJournal", "AgrIshitaEvening", "Another quiet evening talking with Ishita.");
        if (Is(TEXT("madhabi_das.ward_records_agreed"))) return NSLOCTEXT("PerfectWorldJournal", "AgrMadhabiRecords", "Talk about the old ward records with Madhabi another evening.");
        if (Is(TEXT("arup_basu.parcel_round_agreed"))) return NSLOCTEXT("PerfectWorldJournal", "AgrArupRound", "Hear about Arup's parcel round another evening.");
        if (Is(TEXT("satyajit_dutta.bring_garment_agreed"))) return NSLOCTEXT("PerfectWorldJournal", "AgrTailorGarment", "Bring a garment to discuss alterations. No order placed.");
    }
    return NSLOCTEXT("PerfectWorldJournal", "AgrFallback", "An agreement you made.");
}

FText MetAtText(const int64 FirstMetMinute)
{
    const PerfectWorld::CalendarTime Calendar = PerfectWorld::GameClock::ToCalendar(FirstMetMinute);
    return FText::Format(LOCTEXT("MetAtFmt", "Met at {0}"),
                         FText::FromString(FString::Printf(TEXT("%02d:%02d"), Calendar.Hour, Calendar.Minute)));
}

FPWJournalModel BuildModel(const PerfectWorld::LaneJournalView& View, const FText& Clock)
{
    const FText Someone = LOCTEXT("SomeoneYouKnow", "Someone you know");
    FPWJournalModel Model;
    Model.bAvailable = true;
    Model.Clock = Clock;
    for (const PerfectWorld::LaneJournalPerson& Person : View.PeopleMetToday)
    {
        FPWJournalPersonRow& Row = Model.People.AddDefaulted_GetRef();
        Row.Name = Person.DisplayName.empty() ? Someone : PWLaneLife::ToText(Person.DisplayName);
        Row.MetAt = MetAtText(Person.FirstMetMinute);
    }
    for (const PerfectWorld::LaneJournalAgreement& Agreement : View.AgreementsRemembered)
    {
        const std::string_view Label = PerfectWorld::LaneJournalDisplayName(Agreement.PersonId);
        FPWJournalAgreementRow& Row = Model.Agreements.AddDefaulted_GetRef();
        Row.Who = Label.empty() ? Someone : PWLaneLife::ToText(Label);
        Row.Text = AgreementText(PWLaneLife::ToFString(Agreement.ShortTextKey));
    }
    return Model;
}

FPWJournalModel ReadModel(const UObject* WorldContext)
{
    const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(WorldContext);
    if (Life == nullptr)
    {
        return FPWJournalModel();
    }
    const PerfectWorld::WorldState& World = Life->GetWorldState();
    const auto Day = PerfectWorld::GameClock::DayNumber(World.Clock.Now());
    return BuildModel(PerfectWorld::LaneJournalToday(World, Day), Life->GetClockText());
}

bool Compact(const float Width, const float Height)
{
    return Height < 760.0f || Width < 760.0f;
}

float PanelWidth(const float Width, const float)
{
    return FMath::Clamp(Width - 32.0f, 280.0f, 640.0f);
}

float PanelMaxHeight(const float, const float Height)
{
    return FMath::Max(120.0f, Height - 32.0f);
}
} // namespace PWJournal

namespace
{
TSharedRef<SWidget> JpHeading(const FText& Text)
{
    return PWUI::MakeText(Text, PWUI::ETextRole::SectionHeading, PWUI::FStyle::Get().Accent, true);
}

/** A quiet inset line that says why a list is empty. */
TSharedRef<SWidget> JpNote(const FText& Text, const TCHAR* Tag)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    return SNew(SBorder)
        .Tag(FName(Tag))
        .BorderImage(&Style.BrushInset)
        .Padding(FMargin(14.0f, 10.0f))
        [
            PWUI::MakeText(Text, 16, Style.TextSecondary, false, true)
        ];
}
} // namespace

void SPWJournalPanel::Construct(const FArguments& InArgs)
{
    Model = InArgs._Model;
    Space = InArgs._Space;
    OnClose = InArgs._OnClose;
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const auto IsCompact = [this]()
    {
        const FVector2D S = Space.Get();
        return PWJournal::Compact(static_cast<float>(S.X), static_cast<float>(S.Y));
    };

    const TSharedRef<SWidget> BackButton = PWUI::MakeButton(
        LOCTEXT("Back", "Back"), PWUI::EButtonKind::Primary,
        FOnClicked::CreateSPLambda(this, [this]()
        {
            RequestClose();
            return FReply::Handled();
        }), true, 20, 0.0f);

    const TSharedRef<SWidget> Header =
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("Title", "Today"))
            .Font_Lambda([IsCompact]() { return PWUI::Font(IsCompact() ? 22 : 32, true); })
            .ColorAndOpacity(FSlateColor(Style.TextPrimary))
        ]
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(16.0f, 0.0f, 0.0f, 0.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this]() { return Model.Clock; })
            .Font(PWUI::Font(15, PWUI::EFontWeight::Medium))
            .ColorAndOpacity(FSlateColor(Style.TextSecondary))
            .Justification(ETextJustify::Right)
            .AutoWrapText(true)
        ];

    ChildSlot
    [
        SNew(SBox)
        .WidthOverride_Lambda([this]()
        {
            const FVector2D S = Space.Get();
            return FOptionalSize(PWJournal::PanelWidth(static_cast<float>(S.X), static_cast<float>(S.Y)));
        })
        .MaxDesiredHeight_Lambda([this]()
        {
            const FVector2D S = Space.Get();
            return FOptionalSize(PWJournal::PanelMaxHeight(static_cast<float>(S.X), static_cast<float>(S.Y)));
        })
        [
            SNew(SBorder)
            .Tag(FName(PWJournal::PanelTag))
            .BorderImage(&Style.BrushPanel)
            .Padding(0.0f)
            [
                SNew(SOverlay)
                + SOverlay::Slot()
                [
                    SNew(SPWCornerBrackets)
                ]
                + SOverlay::Slot()
                [
                    SNew(SBox)
                    .Padding_Lambda([IsCompact]() { return IsCompact() ? FMargin(24.0f, 10.0f) : FMargin(40.0f, 28.0f); })
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            Header
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 8.0f)
                        [
                            PWUI::MakeRule()
                        ]
                        + SVerticalBox::Slot().FillHeight(1.0f)
                        [
                            BuildBody()
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
                        [
                            SNew(SBox)
                            .Visibility_Lambda([IsCompact]() { return IsCompact() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
                            [
                                PWUI::MakeText(LOCTEXT("Hint", "Up and Down scroll. Esc or B goes back."),
                                               PWUI::ETextRole::Meta, Style.TextSecondary, true)
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f, 0.0f, 0.0f)
                        [
                            SNew(SBox).WidthOverride(240.0f)
                            [
                                BackButton
                            ]
                        ]
                    ]
                ]
            ]
        ]
    ];

    PWUI::FocusWhenReady(AsShared(), BackButton);
}

TSharedRef<SWidget> SPWJournalPanel::BuildBody()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    if (!Model.bAvailable)
    {
        return JpNote(LOCTEXT("Unavailable", "Your journal cannot be read right now. Go back, resume and try again."),
                      PWJournal::UnavailableTag);
    }
    return SAssignNew(BodyScroll, SScrollBox)
        .ScrollBarStyle(&Style.ScrollBar)
        .AllowOverscroll(EAllowOverscroll::No)
        + SScrollBox::Slot().Padding(0.0f, 0.0f, 12.0f, 0.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                BuildPeopleSection()
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 0.0f)
            [
                BuildAgreementsSection()
            ]
        ];
}

TSharedRef<SWidget> SPWJournalPanel::BuildPeopleSection()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
    Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[JpHeading(LOCTEXT("PeopleHeading", "People met today"))];
    if (Model.People.IsEmpty())
    {
        Box->AddSlot().AutoHeight()
        [
            JpNote(LOCTEXT("NoPeople", "You have not spoken with anyone yet today. People you talk with will appear here."),
                   PWJournal::EmptyPeopleTag)
        ];
    }
    for (const FPWJournalPersonRow& Row : Model.People)
    {
        Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SNew(SBorder)
            .Tag(FName(PWJournal::PersonRowTag))
            .BorderImage(&Style.BrushInset)
            .Padding(FMargin(14.0f, 10.0f))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                [
                    PWUI::MakeText(Row.Name, 20, Style.TextPrimary, true, true)
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.0f, 0.0f, 0.0f, 0.0f)
                [
                    PWUI::MakeText(Row.MetAt, 16, Style.TextSecondary)
                ]
            ]
        ];
    }
    return Box;
}

TSharedRef<SWidget> SPWJournalPanel::BuildAgreementsSection()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
    Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[JpHeading(LOCTEXT("AgreementsHeading", "Agreements remembered"))];
    if (Model.Agreements.IsEmpty())
    {
        Box->AddSlot().AutoHeight()
        [
            JpNote(LOCTEXT("NoAgreements", "No agreements yet. Promises and arrangements you make will be kept here."),
                   PWJournal::EmptyAgreementsTag)
        ];
    }
    for (const FPWJournalAgreementRow& Row : Model.Agreements)
    {
        Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SNew(SBorder)
            .Tag(FName(PWJournal::AgreementRowTag))
            .BorderImage(&Style.BrushInset)
            .Padding(FMargin(14.0f, 10.0f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    PWUI::MakeText(Row.Who, PWUI::ETextRole::Meta, Style.Accent, true)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
                [
                    PWUI::MakeText(Row.Text, 18, Style.TextPrimary, false, true)
                ]
            ]
        ];
    }
    return Box;
}

float SPWJournalPanel::GetScrollOffset() const
{
    return BodyScroll.IsValid() ? BodyScroll->GetScrollOffset() : 0.0f;
}

float SPWJournalPanel::GetScrollOffsetOfEnd() const
{
    return BodyScroll.IsValid() ? BodyScroll->GetScrollOffsetOfEnd() : 0.0f;
}

void SPWJournalPanel::ScrollBody(const float Delta)
{
    if (BodyScroll.IsValid())
    {
        BodyScroll->SetScrollOffset(FMath::Clamp(BodyScroll->GetScrollOffset() + Delta, 0.0f, BodyScroll->GetScrollOffsetOfEnd()));
    }
}

void SPWJournalPanel::RequestClose()
{
    OnClose.ExecuteIfBound();
}

FReply SPWJournalPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    constexpr float JpKeyStep = 72.0f;
    const FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right)
    {
        // A held key auto-repeats; this screen goes back on the first press only.
        if (!InKeyEvent.IsRepeat())
        {
            RequestClose();
        }
        return FReply::Handled();
    }
    // A long list scrolls; repeats are welcome here.
    if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up || Key == EKeys::Gamepad_RightStick_Up)
    {
        ScrollBody(-JpKeyStep);
        return FReply::Handled();
    }
    if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down || Key == EKeys::Gamepad_RightStick_Down)
    {
        ScrollBody(JpKeyStep);
        return FReply::Handled();
    }
    if (Key == EKeys::PageUp || Key == EKeys::PageDown)
    {
        const float Page = FMath::Max(JpKeyStep, static_cast<float>(MyGeometry.GetLocalSize().Y) * 0.5f);
        ScrollBody(Key == EKeys::PageUp ? -Page : Page);
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
