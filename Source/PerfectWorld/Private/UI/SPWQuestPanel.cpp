#include "UI/SPWQuestPanel.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "UI/SPWAwakeningSigil.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldQuestPanel"

namespace
{
/** A quiet inset line that says why a list or the window is empty. */
TSharedRef<SWidget> QwNote(const FText& Text, const TCHAR* Tag)
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

FLinearColor QwStatusColour(const PerfectWorld::QuestStatus Status)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    switch (Status)
    {
    case PerfectWorld::QuestStatus::Due: return Style.Accent;
    case PerfectWorld::QuestStatus::Done: return Style.Good;
    case PerfectWorld::QuestStatus::Missed: return Style.Danger;
    case PerfectWorld::QuestStatus::Open: break;
    }
    return Style.TextSecondary;
}

} // namespace

void SPWQuestPanel::Construct(const FArguments& InArgs)
{
    Model = InArgs._Model;
    Space = InArgs._Space;
    OnClose = InArgs._OnClose;
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const auto IsCompact = [this]()
    {
        const FVector2D S = GetSpace();
        return PWQuest::Compact(static_cast<float>(S.X), static_cast<float>(S.Y));
    };

    // The choice starts on the tracked quest when it is still listed, else on the first quest there is.
    const FString Tracked = PWQuest::GetTrackedId();
    if (Model.FindRow(Tracked) != nullptr) SelectedId = Tracked;
    else if (const FPWQuestRow* First = Model.FirstRow()) SelectedId = First->Id;

    const TSharedRef<SWidget> Back = PWUI::MakeButton(
        LOCTEXT("Back", "Back"), PWUI::EButtonKind::Primary,
        FOnClicked::CreateSPLambda(this, [this]()
        {
            RequestClose();
            return FReply::Handled();
        }), true, 20, 0.0f);
    BackButton = Back;

    const TSharedRef<SWidget> TrackButton = PWUI::MakeButton(
        TAttribute<FText>::CreateSPLambda(this, [this]()
        {
            const FPWQuestRow* Row = GetSelectedRow();
            return Row != nullptr && PWQuest::GetTrackedId() == Row->Id ? LOCTEXT("StopTracking", "Stop tracking") : LOCTEXT("Track", "Track this");
        }),
        PWUI::EButtonKind::Secondary,
        FOnClicked::CreateSPLambda(this, [this]()
        {
            ToggleTrackSelected();
            return FReply::Handled();
        }),
        TAttribute<bool>::CreateSPLambda(this, [this]() { return GetSelectedRow() != nullptr; }), 20, 0.0f);

    const TSharedRef<SWidget> Header =
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("Title", "Quests"))
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
            const FVector2D S = GetSpace();
            return FOptionalSize(PWQuest::PanelWidth(static_cast<float>(S.X), static_cast<float>(S.Y)));
        })
        .MaxDesiredHeight_Lambda([this]()
        {
            const FVector2D S = GetSpace();
            return FOptionalSize(PWQuest::PanelMaxHeight(static_cast<float>(S.X), static_cast<float>(S.Y)));
        })
        [
            SNew(SBorder)
            .Tag(FName(PWQuest::PanelTag))
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
                                PWUI::MakeText(LOCTEXT("Hint", "Up and Down choose. T or X tracks. Esc, J or B goes back."),
                                               PWUI::ETextRole::Meta, Style.TextSecondary, true)
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f, 0.0f, 0.0f)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)
                            [
                                SNew(SBox).WidthOverride(220.0f)[TrackButton]
                            ]
                            + SHorizontalBox::Slot().AutoWidth()
                            [
                                SNew(SBox).WidthOverride(220.0f)[Back]
                            ]
                        ]
                    ]
                ]
            ]
        ]
    ];

    const TSharedPtr<SWidget>* Chosen = RowButtons.Find(SelectedId);
    PWUI::FocusWhenReady(AsShared(), Chosen != nullptr ? TWeakPtr<SWidget>(*Chosen) : BackButton);
}

FVector2D SPWQuestPanel::GetSpace() const
{
    return Space.Get(FVector2D(1920.0, 1080.0));
}

TSharedRef<SWidget> SPWQuestPanel::BuildBody()
{
    RowButtons.Reset();
    RowOrder.Reset();
    ListScroll.Reset();
    DetailScroll.Reset();
    const FVector2D S = GetSpace();
    bTwoPane = PWQuest::TwoPane(static_cast<float>(S.X), static_cast<float>(S.Y));
    if (!Model.bAvailable)
    {
        return QwNote(LOCTEXT("Unavailable", "Your quests cannot be read right now. Go back, resume and try again."), PWQuest::UnavailableTag);
    }
    const TSharedRef<SWidget> List = BuildList();
    const TSharedRef<SWidget> Detail = BuildDetail();
    if (bTwoPane)
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SBox)
                .WidthOverride_Lambda([this]()
                {
                    const FVector2D Size = GetSpace();
                    return FOptionalSize(PWQuest::ListWidth(static_cast<float>(Size.X), static_cast<float>(Size.Y)));
                })
                [
                    List
                ]
            ]
            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(16.0f, 0.0f, 0.0f, 0.0f)
            [
                Detail
            ];
    }
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().FillHeight(0.45f)
        [
            List
        ]
        + SVerticalBox::Slot().FillHeight(0.55f).Padding(0.0f, 8.0f, 0.0f, 0.0f)
        [
            Detail
        ];
}

TSharedRef<SWidget> SPWQuestPanel::BuildList()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
    bool bFirstSection = true;
    for (const FPWQuestSectionModel& Section : Model.Sections)
    {
        Column->AddSlot().AutoHeight().Padding(0.0f, bFirstSection ? 0.0f : 14.0f, 0.0f, 6.0f)
        [
            PWUI::MakeText(Section.Heading, PWUI::ETextRole::SectionHeading, Style.Accent, true)
        ];
        bFirstSection = false;
        if (Section.Rows.IsEmpty())
        {
            Column->AddSlot().AutoHeight()[QwNote(Section.EmptyNote, PWQuest::EmptySectionTag)];
        }
        for (const FPWQuestRow& Row : Section.Rows)
        {
            Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[BuildRow(Row)];
        }
    }
    return SAssignNew(ListScroll, SScrollBox)
        .ScrollBarStyle(&Style.ScrollBar)
        .AllowOverscroll(EAllowOverscroll::No)
        + SScrollBox::Slot().Padding(0.0f, 0.0f, 12.0f, 0.0f)
        [
            Column
        ];
}

TSharedRef<SWidget> SPWQuestPanel::BuildRow(const FPWQuestRow& Row)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const FString Id = Row.Id;
    const TSharedRef<SWidget> Content =
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            PWUI::MakeText(Row.Title, 17, Style.TextPrimary, true, true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [
                PWUI::MakeText(Row.StatusLabel, PWUI::ETextRole::Meta, QwStatusColour(Row.Status))
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(10.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SBox)
                .Visibility_Lambda([Id]() { return PWQuest::GetTrackedId() == Id ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
                [
                    PWUI::MakeText(LOCTEXT("TrackedMark", "Tracked"), PWUI::ETextRole::Meta, Style.Accent, true)
                ]
            ]
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
    return SNew(SBox).Tag(FName(PWQuest::RowTag))[Button];
}

TSharedRef<SWidget> SPWQuestPanel::BuildDetail()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const auto Show = [this](const bool bWhenChosen)
    {
        return TAttribute<EVisibility>::CreateSPLambda(this, [this, bWhenChosen]()
        {
            return (GetSelectedRow() != nullptr) == bWhenChosen ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
        });
    };
    const auto Field = [this, &Style](const FText& Label, const TFunction<FText(const FPWQuestRow&)>& Pick) -> TSharedRef<SWidget>
    {
        return SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                PWUI::MakeText(Label, PWUI::ETextRole::Meta, Style.Accent, true)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 12.0f)
            [
                PWUI::MakeText(TAttribute<FText>::CreateSPLambda(this, [this, Pick]()
                {
                    const FPWQuestRow* Row = GetSelectedRow();
                    return Row != nullptr ? Pick(*Row) : FText::GetEmpty();
                }), 18, Style.TextPrimary, false, true)
            ];
    };

    const TSharedRef<SWidget> Chosen =
        SNew(SVerticalBox)
        .Visibility(Show(true))
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(STextBlock)
            .Text_Lambda([this]() { const FPWQuestRow* Row = GetSelectedRow(); return Row != nullptr ? Row->Title : FText::GetEmpty(); })
            .Font_Lambda([this]()
            {
                const FVector2D S = GetSpace();
                return PWUI::Font(PWQuest::Compact(static_cast<float>(S.X), static_cast<float>(S.Y)) ? 20 : 26, true);
            })
            .ColorAndOpacity(FSlateColor(Style.TextPrimary))
            .AutoWrapText(true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 8.0f)
        [
            SNew(SBox)
            .Visibility_Lambda([this]()
            {
                const FPWQuestRow* Row = GetSelectedRow();
                return Row != nullptr && PWQuest::GetTrackedId() == Row->Id ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
            })
            [
                PWUI::MakeText(LOCTEXT("TrackedOnHud", "Tracked: shown on your screen"), PWUI::ETextRole::Meta, Style.Accent, true)
            ]
        ]
        + SVerticalBox::Slot().AutoHeight()[Field(LOCTEXT("FieldWhat", "WHAT"), [](const FPWQuestRow& Row) { return Row.What; })]
        + SVerticalBox::Slot().AutoHeight()[Field(LOCTEXT("FieldWho", "WHO"), [](const FPWQuestRow& Row) { return Row.Who; })]
        + SVerticalBox::Slot().AutoHeight()[Field(LOCTEXT("FieldWhere", "WHERE"), [](const FPWQuestRow& Row) { return Row.Where; })]
        + SVerticalBox::Slot().AutoHeight()[Field(LOCTEXT("FieldWhen", "WHEN"), [](const FPWQuestRow& Row) { return Row.When; })]
        + SVerticalBox::Slot().AutoHeight()[Field(LOCTEXT("FieldStatus", "STATUS"), [](const FPWQuestRow& Row) { return Row.StatusLabel; })];

    const TSharedRef<SWidget> Prompt =
        SNew(SBox)
        .Visibility(Show(false))
        [
            PWUI::MakeText(TAttribute<FText>::CreateSPLambda(this, [this]()
            {
                return Model.RowCount() == 0
                    ? LOCTEXT("NothingYet", "Nothing here yet. The day's plans, promises you make and places you have not visited will appear in this window.")
                    : LOCTEXT("ChooseOne", "Choose something from the list to read about it.");
            }), 16, Style.TextSecondary, false, true)
        ];

    return SNew(SBorder)
        .Tag(FName(PWQuest::DetailTag))
        .BorderImage(&Style.BrushInset)
        .Padding(FMargin(16.0f, 12.0f))
        [
            SAssignNew(DetailScroll, SScrollBox)
            .ScrollBarStyle(&Style.ScrollBar)
            .AllowOverscroll(EAllowOverscroll::No)
            + SScrollBox::Slot().Padding(0.0f, 0.0f, 12.0f, 0.0f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[Prompt]
                + SVerticalBox::Slot().AutoHeight()[Chosen]
            ]
        ];
}

#undef LOCTEXT_NAMESPACE
