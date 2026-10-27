#include "UI/SPWSchoolDay.h"

#include "Framework/Application/SlateApplication.h"
#include "GameClock.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "SchoolCalendar.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "UI/SPWAwakeningSigil.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldSchoolDay"

using PerfectWorld::SchoolDayRowStatus;
using PerfectWorld::SchoolLessonMode;

// ---------------------------------------------------------------------------------------------------------------- words

namespace
{
FString SdFromUtf8(const std::string& Text)
{
    return FString(UTF8_TO_TCHAR(Text.c_str()));
}

/** "political_science" -> "Political science". */
FText SdHumanise(const std::string& Token)
{
    FString Text = SdFromUtf8(Token).Replace(TEXT("_"), TEXT(" "));
    if (!Text.IsEmpty()) Text[0] = FChar::ToUpper(Text[0]);
    return FText::FromString(Text);
}

FText SdWordsOrEmpty(const PerfectWorld::SchoolLessonContent& Content, const std::string& Key)
{
    return Key.empty() ? FText::GetEmpty() : PWSchoolDay::Words(Content, Key);
}

FLinearColor SdStatusColour(const SchoolDayRowStatus Status)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    switch (Status)
    {
    case SchoolDayRowStatus::Open:
    case SchoolDayRowStatus::Active: return Style.Accent;
    case SchoolDayRowStatus::Done: return Style.Good;
    case SchoolDayRowStatus::Cancelled: return Style.Warn;
    case SchoolDayRowStatus::Missed:
    case SchoolDayRowStatus::Unavailable: return Style.Danger;
    case SchoolDayRowStatus::Upcoming: break;
    }
    return Style.TextSecondary;
}

/** A quiet inset line that says why a list or the window is empty. */
TSharedRef<SWidget> SdNote(const FText& Text, const TCHAR* Tag)
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

bool SdIsBack(const FKey& Key)
{
    return Key == EKeys::Escape || Key == EKeys::J || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right
        || Key == EKeys::Gamepad_LeftShoulder;
}
bool SdIsUp(const FKey& Key) { return Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up; }
bool SdIsDown(const FKey& Key) { return Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down; }
} // namespace

namespace PWSchoolDay
{
FText Words(const PerfectWorld::SchoolLessonContent& Content, const std::string& Key)
{
    if (const std::string* Text = PerfectWorld::FindSchoolLessonText(Content, Key))
    {
        return FText::FromString(SdFromUtf8(*Text));
    }
    return LOCTEXT("NoWords", "No description is available for this yet.");
}

FText TimeText(const int32 MinuteOfDay)
{
    const int32 Minute = FMath::Clamp(MinuteOfDay, 0, 24 * 60 - 1);
    return FText::FromString(FString::Printf(TEXT("%02d:%02d"), Minute / 60, Minute % 60));
}

FText ClockText(const int64 CampaignDay, const int32 MinuteOfDay)
{
    const std::string_view Weekday = PerfectWorld::WeekdayName(PerfectWorld::CalendarWeekdayOf(CampaignDay));
    return FText::Format(LOCTEXT("ClockFmt", "{0} {1}, {2}"), FText::FromString(SdFromUtf8(std::string(Weekday))),
                         FText::FromString(SdFromUtf8(PerfectWorld::FormatCalendarDate(CampaignDay))), TimeText(MinuteOfDay));
}

FText StatusText(const SchoolDayRowStatus Status)
{
    switch (Status)
    {
    case SchoolDayRowStatus::Upcoming: return LOCTEXT("StatusUpcoming", "Opens at the bell");
    case SchoolDayRowStatus::Open: return LOCTEXT("StatusOpen", "Open now");
    case SchoolDayRowStatus::Active: return LOCTEXT("StatusActive", "In progress");
    case SchoolDayRowStatus::Done: return LOCTEXT("StatusDone", "Done");
    case SchoolDayRowStatus::Cancelled: return LOCTEXT("StatusCancelled", "Stopped early");
    case SchoolDayRowStatus::Missed: return LOCTEXT("StatusMissed", "Missed");
    case SchoolDayRowStatus::Unavailable: return LOCTEXT("StatusUnavailable", "Not available");
    }
    return FText::GetEmpty();
}

FText StructureText(const PerfectWorld::SchoolStructure Structure)
{
    switch (Structure)
    {
    case PerfectWorld::SchoolStructure::Model: return LOCTEXT("StructureModel", "Model a system");
    case PerfectWorld::SchoolStructure::Explain: return LOCTEXT("StructureExplain", "Construct an explanation");
    case PerfectWorld::SchoolStructure::Investigate: return LOCTEXT("StructureInvestigate", "Investigate a fault");
    case PerfectWorld::SchoolStructure::Coordinate: return LOCTEXT("StructureCoordinate", "Coordinate a group");
    }
    return FText::GetEmpty();
}

FPWSchoolDayModel BuildModel(const PerfectWorld::SchoolDayView& View, const PerfectWorld::SchoolLessonContent& Content)
{
    FPWSchoolDayModel Model;
    if (!View.Ok())
    {
        return Model;
    }
    Model.bAvailable = true;
    Model.bSchoolDay = View.SchoolDay;
    Model.bLessonOpen = View.LessonOpen;
    Model.Clock = ClockText(View.Day, View.MinuteOfDay);
    Model.ForegroundLine = FText::Format(LOCTEXT("ForegroundFmt", "Foreground lessons today: {0} of {1}"),
                                         FText::AsNumber(View.ForegroundUsed), FText::AsNumber(View.ForegroundLimit));
    Model.Closure = SdWordsOrEmpty(Content, View.ClosureTextKey);
    if (View.NextSchoolDay >= 0)
    {
        const std::string_view Weekday = PerfectWorld::WeekdayName(PerfectWorld::CalendarWeekdayOf(View.NextSchoolDay));
        Model.NextSchoolDay = FText::Format(LOCTEXT("NextDayFmt", "Lessons resume on {0} {1}."), FText::FromString(SdFromUtf8(std::string(Weekday))),
                                            FText::FromString(SdFromUtf8(PerfectWorld::FormatCalendarDate(View.NextSchoolDay))));
    }
    Model.Absence = SdWordsOrEmpty(Content, View.AbsenceTextKey);
    Model.AbsenceRecovery = SdWordsOrEmpty(Content, View.AbsenceRecoveryTextKey);
    const SchoolLessonMode Modes[3] = {SchoolLessonMode::Foreground, SchoolLessonMode::Routine, SchoolLessonMode::Attend};
    for (int32 Index = 0; Index < 3; ++Index)
    {
        if (const PerfectWorld::SchoolLessonModeText* Mode = PerfectWorld::FindSchoolLessonModeText(Content, Modes[Index]))
        {
            Model.ModeTitle[Index] = Words(Content, Mode->TitleTextKey);
            Model.ModeBrief[Index] = Words(Content, Mode->BriefTextKey);
        }
    }
    for (const PerfectWorld::SchoolDayRow& Source : View.Rows)
    {
        FPWSchoolDayRow& Row = Model.Rows.AddDefaulted_GetRef();
        Row.BindingId = SdFromUtf8(Source.BindingId);
        Row.Period = Source.Period;
        Row.Status = Source.Status;
        Row.Subject = SdHumanise(Source.Subject);
        Row.Title = Words(Content, Source.TitleTextKey);
        Row.Brief = Words(Content, Source.BriefTextKey);
        Row.Role = Words(Content, Source.RoleTextKey);
        Row.StructureLabel = StructureText(Source.Structure);
        Row.TimeLabel = FText::Format(LOCTEXT("TimeFmt", "{0} to {1}"), TimeText(Source.StartMinute), TimeText(Source.EndMinute));
        Row.StatusLabel = StatusText(Source.Status);
        Row.Reason = SdWordsOrEmpty(Content, Source.ReasonTextKey);
        Row.Recovery = SdWordsOrEmpty(Content, Source.RecoveryTextKey);
        Row.ForegroundReason = SdWordsOrEmpty(Content, Source.ForegroundReasonTextKey);
        Row.RoutineReason = SdWordsOrEmpty(Content, Source.RoutineReasonTextKey);
        Row.bForegroundAvailable = Source.ForegroundAvailable;
        Row.bRoutineAvailable = Source.RoutineAvailable;
        Row.bAttendAvailable = Source.AttendAvailable;
        if (Source.Status == SchoolDayRowStatus::Open)
        {
            for (const PerfectWorld::SchoolLessonGoalOption& Goal : PerfectWorld::SchoolLessonGoalOptions(Content, Source.Structure))
            {
                Row.Goals.Add({SdFromUtf8(Goal.Name), Words(Content, Goal.TextKey)});
            }
        }
    }
    return Model;
}

bool Compact(const float Width, const float Height)
{
    return Height < 760.0f || Width < 760.0f;
}
bool TwoPane(const float Width, const float)
{
    return Width >= 700.0f;
}
float PanelWidth(const float Width, const float)
{
    return FMath::Clamp(Width - 32.0f, 280.0f, 1100.0f);
}
float PanelMaxHeight(const float, const float Height)
{
    return FMath::Max(120.0f, Height - 32.0f);
}
float ListWidth(const float Width, const float Height)
{
    return FMath::Clamp(PanelWidth(Width, Height) * 0.36f, 220.0f, 380.0f);
}
} // namespace PWSchoolDay

const FPWSchoolDayRow* FPWSchoolDayModel::FindRow(const FString& BindingId) const
{
    return Rows.FindByPredicate([&BindingId](const FPWSchoolDayRow& Row) { return Row.BindingId == BindingId; });
}

const FPWSchoolDayRow* FPWSchoolDayModel::StartingRow() const
{
    for (const FPWSchoolDayRow& Row : Rows)
    {
        if (Row.Status == SchoolDayRowStatus::Open || Row.Status == SchoolDayRowStatus::Active) return &Row;
    }
    for (const FPWSchoolDayRow& Row : Rows)
    {
        if (Row.Status == SchoolDayRowStatus::Upcoming) return &Row;
    }
    return Rows.IsEmpty() ? nullptr : &Rows[0];
}

// --------------------------------------------------------------------------------------------------------------- widget

void SPWSchoolDay::Construct(const FArguments& InArgs)
{
    World = InArgs._World;
    Space = InArgs._Space;
    OnClose = InArgs._OnClose;
    OnOpened = InArgs._OnOpened;
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const auto IsCompact = [this]()
    {
        const FVector2D S = GetSpace();
        return PWSchoolDay::Compact(static_cast<float>(S.X), static_cast<float>(S.Y));
    };

    if (World.IsValid() && !World->ReadModel(Model, Why))
    {
        Model = FPWSchoolDayModel();
    }
    if (const FPWSchoolDayRow* Start = Model.StartingRow()) SelectedId = Start->BindingId;

    const TSharedRef<SWidget> Close = PWUI::MakeButton(
        LOCTEXT("Close", "Close"), PWUI::EButtonKind::Secondary,
        FOnClicked::CreateSPLambda(this, [this]()
        {
            RequestClose();
            return FReply::Handled();
        }), true, 20, 0.0f);
    CloseButton = Close;

    ChildSlot
    [
        SNew(SBox)
        .WidthOverride_Lambda([this]()
        {
            const FVector2D S = GetSpace();
            return FOptionalSize(PWSchoolDay::PanelWidth(static_cast<float>(S.X), static_cast<float>(S.Y)));
        })
        .MaxDesiredHeight_Lambda([this]()
        {
            const FVector2D S = GetSpace();
            return FOptionalSize(PWSchoolDay::PanelMaxHeight(static_cast<float>(S.X), static_cast<float>(S.Y)));
        })
        [
            SNew(SBorder)
            .Tag(FName(PWSchoolDay::PanelTag))
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
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("Title", "School day"))
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
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 8.0f)
                        [
                            PWUI::MakeRule()
                        ]
                        + SVerticalBox::Slot().FillHeight(1.0f)
                        [
                            SAssignNew(BodyHolder, SBox)
                            [
                                BuildBody()
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
                        [
                            SNew(SBox)
                            .Visibility_Lambda([IsCompact]() { return IsCompact() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
                            [
                                PWUI::MakeText(LOCTEXT("Hint", "Up and Down choose a period. Esc, J or B closes."),
                                               PWUI::ETextRole::Meta, Style.TextSecondary, true)
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f, 0.0f, 0.0f)
                        [
                            SNew(SBox).WidthOverride(220.0f)[Close]
                        ]
                    ]
                ]
            ]
        ]
    ];

    const TSharedPtr<SWidget>* Chosen = RowButtons.Find(SelectedId);
    PWUI::FocusWhenReady(AsShared(), Chosen != nullptr ? TWeakPtr<SWidget>(*Chosen) : CloseButton);
}

FVector2D SPWSchoolDay::GetSpace() const
{
    return Space.Get(FVector2D(1920.0, 1080.0));
}

FString SPWSchoolDay::GetSelectedGoal(const FPWSchoolDayRow& Row) const
{
    if (const FString* Chosen = GoalByRow.Find(Row.BindingId))
    {
        return *Chosen;
    }
    return Row.Goals.IsEmpty() ? FString() : Row.Goals[0].Name;
}

TSharedRef<SWidget> SPWSchoolDay::BuildBody()
{
    RowButtons.Reset();
    RowOrder.Reset();
    ListScroll.Reset();
    DetailScroll.Reset();
    DetailHolder.Reset();
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const FVector2D S = GetSpace();
    bTwoPane = PWSchoolDay::TwoPane(static_cast<float>(S.X), static_cast<float>(S.Y));
    if (!Model.bAvailable)
    {
        return SdNote(LOCTEXT("Unavailable", "The school day cannot be read right now. Close this window and try again."), PWSchoolDay::UnavailableTag);
    }
    const TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
    if (!Model.Absence.IsEmpty())
    {
        Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
        [
            SNew(SBorder)
            .Tag(FName(PWSchoolDay::AbsenceTag))
            .BorderImage(&Style.BrushInset)
            .Padding(FMargin(14.0f, 10.0f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[PWUI::MakeText(Model.Absence, 16, Style.TextPrimary, true, true)]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)[PWUI::MakeText(Model.AbsenceRecovery, 16, Style.TextSecondary, false, true)]
            ]
        ];
    }
    if (!Model.bSchoolDay)
    {
        Column->AddSlot().AutoHeight()
        [
            SNew(SBorder)
            .Tag(FName(PWSchoolDay::ClosureTag))
            .BorderImage(&Style.BrushInset)
            .Padding(FMargin(16.0f, 12.0f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[PWUI::MakeText(Model.Closure, 20, Style.TextPrimary, true, true)]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)[PWUI::MakeText(Model.NextSchoolDay, 16, Style.TextSecondary, false, true)]
            ]
        ];
        return Column;
    }
    Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
    [
        PWUI::MakeText(Model.ForegroundLine, PWUI::ETextRole::Meta, Style.Accent, true)
    ];
    const TSharedRef<SWidget> List = BuildList();
    const TSharedRef<SWidget> Detail = BuildDetail();
    RebuildDetail();
    if (bTwoPane)
    {
        Column->AddSlot().FillHeight(1.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SBox)
                .WidthOverride_Lambda([this]()
                {
                    const FVector2D Size = GetSpace();
                    return FOptionalSize(PWSchoolDay::ListWidth(static_cast<float>(Size.X), static_cast<float>(Size.Y)));
                })
                [
                    List
                ]
            ]
            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(16.0f, 0.0f, 0.0f, 0.0f)
            [
                Detail
            ]
        ];
    }
    else
    {
        Column->AddSlot().FillHeight(0.45f)[List];
        Column->AddSlot().FillHeight(0.55f).Padding(0.0f, 8.0f, 0.0f, 0.0f)[Detail];
    }
    return Column;
}

TSharedRef<SWidget> SPWSchoolDay::BuildList()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
    for (const FPWSchoolDayRow& Row : Model.Rows)
    {
        Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[BuildRow(Row)];
    }
    return SAssignNew(ListScroll, SScrollBox)
        .ScrollBarStyle(&Style.ScrollBar)
        .AllowOverscroll(EAllowOverscroll::No)
        + SScrollBox::Slot().Padding(0.0f, 0.0f, 12.0f, 0.0f)
        [
            Column
        ];
}

TSharedRef<SWidget> SPWSchoolDay::BuildRow(const FPWSchoolDayRow& Row)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const FString Id = Row.BindingId;
    const TSharedRef<SWidget> Content =
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            PWUI::MakeText(FText::Format(LOCTEXT("RowTitleFmt", "{0}. {1}"), FText::AsNumber(Row.Period + 1), Row.Subject), 17, Style.TextPrimary, true, true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(Row.TimeLabel, PWUI::ETextRole::Meta, Style.TextSecondary)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(Row.StatusLabel, PWUI::ETextRole::Meta, SdStatusColour(Row.Status))
        ];
    const TSharedRef<SWidget> Button = PWUI::MakeContentButton(
        Content, PWUI::EButtonKind::TextRow,
        FOnClicked::CreateSPLambda(this, [this, Id]()
        {
            SelectRow(Id);
            return FReply::Handled();
        }),
        TAttribute<bool>::CreateSPLambda(this, [this, Id]() { return SelectedId == Id; }),
        true, FMargin(12.0f, 8.0f));
    RowButtons.Add(Id, Button);
    RowOrder.Add(Id);
    return SNew(SBox).Tag(FName(PWSchoolDay::RowTag))[Button];
}

TSharedRef<SWidget> SPWSchoolDay::BuildDetail()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    return SNew(SBorder)
        .Tag(FName(PWSchoolDay::DetailTag))
        .BorderImage(&Style.BrushInset)
        .Padding(FMargin(16.0f, 12.0f))
        [
            SAssignNew(DetailScroll, SScrollBox)
            .ScrollBarStyle(&Style.ScrollBar)
            .AllowOverscroll(EAllowOverscroll::No)
            + SScrollBox::Slot().Padding(0.0f, 0.0f, 12.0f, 0.0f)
            [
                SAssignNew(DetailHolder, SBox)
                [
                    SNew(SBox)
                ]
            ]
        ];
}

TSharedRef<SWidget> SPWSchoolDay::BuildModes(const FPWSchoolDayRow& Row)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
    const SchoolLessonMode Modes[3] = {SchoolLessonMode::Foreground, SchoolLessonMode::Routine, SchoolLessonMode::Attend};
    const bool Available[3] = {Row.bForegroundAvailable, Row.bRoutineAvailable, Row.bAttendAvailable};
    for (int32 Index = 0; Index < 3; ++Index)
    {
        const SchoolLessonMode Mode = Modes[Index];
        Column->AddSlot().AutoHeight().Padding(0.0f, Index == 0 ? 0.0f : 12.0f, 0.0f, 0.0f)
        [
            SNew(SBox).Tag(FName(PWSchoolDay::ModeTag))
            [
                PWUI::MakeButton(Model.ModeTitle[Index],
                                 Index == 0 ? PWUI::EButtonKind::Primary : PWUI::EButtonKind::Secondary,
                                 FOnClicked::CreateSPLambda(this, [this, Mode]()
                                 {
                                     RequestOpen(Mode);
                                     return FReply::Handled();
                                 }), Available[Index], 18, 0.0f)
            ]
        ];
        const FText& Reason = Index == 0 ? Row.ForegroundReason : Index == 1 ? Row.RoutineReason : FText::GetEmpty();
        Column->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(Available[Index] || Reason.IsEmpty() ? Model.ModeBrief[Index] : Reason, 15,
                           Available[Index] ? Style.TextSecondary : Style.Danger, false, true)
        ];
        if (Index == 0 && Row.bForegroundAvailable)
        {
            for (const FPWSchoolDayGoal& Goal : Row.Goals)
            {
                const FString Name = Goal.Name;
                const FString RowId = Row.BindingId;
                Column->AddSlot().AutoHeight().Padding(16.0f, 4.0f, 0.0f, 0.0f)
                [
                    SNew(SBox).Tag(FName(PWSchoolDay::GoalTag))
                    [
                        PWUI::MakeButton(Goal.Label, PWUI::EButtonKind::Chip,
                                         FOnClicked::CreateSPLambda(this, [this, Name]()
                                         {
                                             SelectGoal(Name);
                                             return FReply::Handled();
                                         }), true, 15, 0.0f,
                                         TAttribute<bool>::CreateSPLambda(this, [this, RowId, Name]()
                                         {
                                             const FPWSchoolDayRow* Chosen = Model.FindRow(RowId);
                                             return Chosen != nullptr && GetSelectedGoal(*Chosen) == Name;
                                         }))
                    ]
                ];
            }
        }
    }
    return Column;
}

void SPWSchoolDay::RebuildDetail()
{
    if (!DetailHolder.IsValid())
    {
        return;
    }
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const FPWSchoolDayRow* Row = GetSelectedRow();
    if (Row == nullptr)
    {
        DetailHolder->SetContent(PWUI::MakeText(LOCTEXT("ChooseOne", "Choose a period from the list to read about it."), 16, Style.TextSecondary, false, true));
        return;
    }
    const TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
    Column->AddSlot().AutoHeight()[PWUI::MakeText(Row->Title, 24, Style.TextPrimary, true, true)];
    Column->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
    [
        PWUI::MakeText(FText::Format(LOCTEXT("DetailMetaFmt", "{0}  |  {1}"), Row->StructureLabel, Row->TimeLabel), PWUI::ETextRole::Meta, Style.TextSecondary, true)
    ];
    Column->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 10.0f)[PWUI::MakeText(Row->StatusLabel, PWUI::ETextRole::Meta, SdStatusColour(Row->Status), true)];
    Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)[PWUI::MakeText(Row->Brief, 18, Style.TextPrimary, false, true)];
    Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)[PWUI::MakeText(Row->Role, 16, Style.TextSecondary, false, true)];
    if (!Row->Reason.IsEmpty())
    {
        Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[PWUI::MakeText(LOCTEXT("WhyHeading", "WHY NOT NOW"), PWUI::ETextRole::Meta, Style.Accent, true)];
        Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)[PWUI::MakeText(Row->Reason, 17, Style.TextPrimary, false, true)];
    }
    if (!Row->Recovery.IsEmpty())
    {
        Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[PWUI::MakeText(LOCTEXT("RecoveryHeading", "WHAT YOU CAN DO"), PWUI::ETextRole::Meta, Style.Accent, true)];
        Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)[PWUI::MakeText(Row->Recovery, 17, Style.TextPrimary, false, true)];
    }
    if (Row->Status == SchoolDayRowStatus::Active)
    {
        Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
        [
            PWUI::MakeText(LOCTEXT("ActiveNote", "This lesson is open. Carry on in the lesson window."), 17, Style.TextPrimary, false, true)
        ];
    }
    if (Row->Status == SchoolDayRowStatus::Open)
    {
        Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[PWUI::MakeText(LOCTEXT("ModesHeading", "HOW TO SPEND THIS PERIOD"), PWUI::ETextRole::Meta, Style.Accent, true)];
        Column->AddSlot().AutoHeight()[BuildModes(*Row)];
    }
    Column->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
    [
        PWUI::MakeText(TAttribute<FText>::CreateSPLambda(this, [this]() { return Notice; }), 16, Style.Danger, true, true)
    ];
    DetailHolder->SetContent(Column);
}

bool SPWSchoolDay::SelectRow(const FString& BindingId)
{
    if (BindingId == SelectedId || Model.FindRow(BindingId) == nullptr)
    {
        return false;
    }
    SelectedId = BindingId;
    Notice = FText::GetEmpty();
    RebuildDetail();
    if (ListScroll.IsValid())
    {
        if (const TSharedPtr<SWidget>* Button = RowButtons.Find(BindingId)) ListScroll->ScrollDescendantIntoView(*Button, false);
    }
    if (DetailScroll.IsValid()) DetailScroll->SetScrollOffset(0.0f);
    return true;
}

bool SPWSchoolDay::SelectGoal(const FString& GoalName)
{
    const FPWSchoolDayRow* Row = GetSelectedRow();
    if (Row == nullptr || !Row->Goals.ContainsByPredicate([&GoalName](const FPWSchoolDayGoal& Goal) { return Goal.Name == GoalName; }))
    {
        return false;
    }
    GoalByRow.Add(Row->BindingId, GoalName);
    return true;
}

bool SPWSchoolDay::RequestOpen(const SchoolLessonMode Mode)
{
    const FPWSchoolDayRow* Row = GetSelectedRow();
    if (Row == nullptr || !World.IsValid())
    {
        return false;
    }
    const bool bAvailable = Mode == SchoolLessonMode::Foreground ? Row->bForegroundAvailable
        : Mode == SchoolLessonMode::Routine ? Row->bRoutineAvailable : Row->bAttendAvailable;
    if (!bAvailable)
    {
        return false;
    }
    const FString Goal = Mode == SchoolLessonMode::Foreground ? GetSelectedGoal(*Row) : FString();
    const FText Refusal = World->OpenLesson(Row->BindingId, Mode, Goal);
    if (!Refusal.IsEmpty())
    {
        // The core said no (the world moved on since the view was read): show its words and the timetable as it now is.
        const FString Keep = SelectedId;
        Refresh();
        SelectedId = Model.FindRow(Keep) != nullptr ? Keep : SelectedId;
        Notice = Refusal;
        RebuildDetail();
        return false;
    }
    Notice = FText::GetEmpty();
    OnOpened.ExecuteIfBound();
    return true;
}

void SPWSchoolDay::Refresh()
{
    if (!World.IsValid() || !BodyHolder.IsValid())
    {
        return;
    }
    FPWSchoolDayModel Fresh;
    if (!World->ReadModel(Fresh, Why))
    {
        Fresh = FPWSchoolDayModel();
    }
    Model = MoveTemp(Fresh);
    if (Model.FindRow(SelectedId) == nullptr)
    {
        const FPWSchoolDayRow* Start = Model.StartingRow();
        SelectedId = Start != nullptr ? Start->BindingId : FString();
    }
    BodyHolder->SetContent(BuildBody());
    const TSharedPtr<SWidget>* Chosen = RowButtons.Find(SelectedId);
    PWUI::FocusWhenReady(AsShared(), Chosen != nullptr ? TWeakPtr<SWidget>(*Chosen) : CloseButton);
}

void SPWSchoolDay::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    const FVector2D S = GetSpace();
    if (BodyHolder.IsValid() && PWSchoolDay::TwoPane(static_cast<float>(S.X), static_cast<float>(S.Y)) != bTwoPane)
    {
        BodyHolder->SetContent(BuildBody());
    }
}

void SPWSchoolDay::FocusRow(const FString& BindingId)
{
    const TSharedPtr<SWidget>* Button = RowButtons.Find(BindingId);
    if (Button == nullptr || !FSlateApplication::IsInitialized())
    {
        return;
    }
    const TSharedPtr<SWidget> Found = PWUI::FindFirstFocusable(Button->ToSharedRef());
    if (Found.IsValid())
    {
        FSlateApplication::Get().SetKeyboardFocus(Found, EFocusCause::Navigation);
    }
}

bool SPWSchoolDay::MoveSelection(const int32 Step)
{
    if (RowOrder.IsEmpty())
    {
        return false;
    }
    const int32 Index = RowOrder.IndexOfByKey(SelectedId);
    const int32 Target = Index == INDEX_NONE ? 0 : Index + Step;
    if (Target < 0 || Target >= RowOrder.Num())
    {
        return false;
    }
    SelectRow(RowOrder[Target]);
    FocusRow(RowOrder[Target]);
    return true;
}

void SPWSchoolDay::ScrollDetail(const float Delta)
{
    if (DetailScroll.IsValid())
    {
        DetailScroll->SetScrollOffset(FMath::Clamp(DetailScroll->GetScrollOffset() + Delta, 0.0f, DetailScroll->GetScrollOffsetOfEnd()));
    }
}

void SPWSchoolDay::RequestClose()
{
    OnClose.ExecuteIfBound();
}

FReply SPWSchoolDay::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    constexpr float SdScrollStep = 72.0f;
    const FKey Key = InKeyEvent.GetKey();
    if (SdIsBack(Key))
    {
        // A held key auto-repeats; this window closes on the first press only.
        if (!InKeyEvent.IsRepeat())
        {
            RequestClose();
        }
        return FReply::Handled();
    }
    // Choosing repeats; at either end the key is left for the focus navigation (Down from the last period reaches the buttons).
    if (SdIsUp(Key) || SdIsDown(Key))
    {
        return MoveSelection(SdIsUp(Key) ? -1 : 1) ? FReply::Handled() : FReply::Unhandled();
    }
    if (Key == EKeys::Gamepad_RightStick_Up || Key == EKeys::Gamepad_RightStick_Down)
    {
        ScrollDetail(Key == EKeys::Gamepad_RightStick_Up ? -SdScrollStep : SdScrollStep);
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
