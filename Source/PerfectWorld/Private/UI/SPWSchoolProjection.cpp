#include "UI/SPWSchoolProjection.h"

#include <algorithm>

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "UI/PWQuestText.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "UI/SPWAwakeningSigil.h"
#include "UI/SPWSchoolPlanner.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

// Words come from the shared A6 table in SPWSchoolPlanner.cpp (PWSchoolPlanner::Text), keyed phase2.a6.*.

namespace
{
const TCHAR* const GA6PathKeys[] = {TEXT("hunter"), TEXT("scholar"), TEXT("engineer"), TEXT("enterprise"), TEXT("athlete"),
                                   TEXT("clinician"), TEXT("public_leader"), TEXT("logistician"), TEXT("artist"), TEXT("ecologist")};

TFunction<void()>& A6ProjectionOpenHandler()
{
    static TFunction<void()> Handler;
    return Handler;
}

TSharedRef<SWidget> A6ProjectionNote(const FText& Words, const TCHAR* Tag)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    return SNew(SBorder)
        .Tag(FName(Tag))
        .BorderImage(&Style.BrushInset)
        .Padding(FMargin(14.0f, 10.0f))
        [
            PWUI::MakeText(Words, 16, Style.TextSecondary, false, true)
        ];
}

/** Unknown is never drawn in a warning colour: only demonstrated evidence is coloured at all. */
FLinearColor A6StateColour(const PerfectWorld::AptitudeFacetState State)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    switch (State)
    {
    case PerfectWorld::AptitudeFacetState::Established: return Style.Good;
    case PerfectWorld::AptitudeFacetState::Shown: return Style.Accent;
    case PerfectWorld::AptitudeFacetState::Observed:
    case PerfectWorld::AptitudeFacetState::Unobserved: break;
    }
    return Style.TextSecondary;
}

bool A6IsBack(const FKey& Key)
{
    return Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right || Key == EKeys::Gamepad_LeftShoulder;
}

bool A6IsUp(const FKey& Key)
{
    return Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up;
}

bool A6IsDown(const FKey& Key)
{
    return Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down;
}
} // namespace

const FPWProjectionPathRow* FPWProjectionModel::FindRow(const FString& Id) const
{
    return Paths.FindByPredicate([&Id](const FPWProjectionPathRow& Row) { return Row.Id == Id; });
}

namespace PWSchoolProjection
{
FText PathName(const PerfectWorld::AptitudePath Path)
{
    const int32 Index = static_cast<int32>(Path);
    return Index >= 0 && Index < static_cast<int32>(UE_ARRAY_COUNT(GA6PathKeys)) ? PWSchoolPlanner::Text(*(FString(TEXT("phase2.a6.path.")) + GA6PathKeys[Index])) : FText::GetEmpty();
}

FText FacetName(const PerfectWorld::AptitudeFacet Facet)
{
    return PWSchoolPlanner::Text(*(FString(TEXT("phase2.a6.facet.")) + PWLaneLife::ToFString(PerfectWorld::AptitudeFacetId(Facet))));
}

FText BandText(const PerfectWorld::AptitudeBand Band)
{
    using PerfectWorld::AptitudeBand;
    switch (Band)
    {
    case AptitudeBand::Developing: return PWSchoolPlanner::Text(TEXT("phase2.a6.band.developing"));
    case AptitudeBand::Plausible: return PWSchoolPlanner::Text(TEXT("phase2.a6.band.plausible"));
    case AptitudeBand::Strong: return PWSchoolPlanner::Text(TEXT("phase2.a6.band.strong"));
    case AptitudeBand::InsufficientObservation: break;
    }
    return PWSchoolPlanner::Text(TEXT("phase2.a6.band.insufficient"));
}

FText CoverageText(const PerfectWorld::AptitudeCoverage Coverage)
{
    using PerfectWorld::AptitudeCoverage;
    switch (Coverage)
    {
    case AptitudeCoverage::Sparse: return PWSchoolPlanner::Text(TEXT("phase2.a6.coverage.sparse"));
    case AptitudeCoverage::Partial: return PWSchoolPlanner::Text(TEXT("phase2.a6.coverage.partial"));
    case AptitudeCoverage::Broad: return PWSchoolPlanner::Text(TEXT("phase2.a6.coverage.broad"));
    case AptitudeCoverage::Unknown: break;
    }
    return PWSchoolPlanner::Text(TEXT("phase2.a6.coverage.unknown"));
}

FText FacetStateText(const PerfectWorld::AptitudeFacetState State)
{
    using PerfectWorld::AptitudeFacetState;
    switch (State)
    {
    case AptitudeFacetState::Observed: return PWSchoolPlanner::Text(TEXT("phase2.a6.facet_state.observed"));
    case AptitudeFacetState::Shown: return PWSchoolPlanner::Text(TEXT("phase2.a6.facet_state.shown"));
    case AptitudeFacetState::Established: return PWSchoolPlanner::Text(TEXT("phase2.a6.facet_state.established"));
    case AptitudeFacetState::Unobserved: break;
    }
    return PWSchoolPlanner::Text(TEXT("phase2.a6.facet_state.unobserved"));
}

FPWProjectionModel BuildModel(const PerfectWorld::AptitudeLedger& Ledger)
{
    using namespace PerfectWorld;
    FPWProjectionModel Model;
    const AptitudeProjection Projection = AptitudeProject(Ledger);
    // An inconsistent ledger projects to nothing: say so rather than show ten invented "unknown" paths.
    if (Projection.Paths.size() != AptitudePathCount) return Model;
    Model.bAvailable = true;
    Model.bNothingSeen = Ledger.Items.empty();
    for (const AptitudePathProjection& Path : Projection.Paths)
    {
        FPWProjectionPathRow& Row = Model.Paths.AddDefaulted_GetRef();
        Row.Id = PWLaneLife::ToFString(AptitudePathId(Path.Path));
        Row.Path = Path.Path;
        Row.Name = PathName(Path.Path);
        Row.BandValue = Path.Band;
        Row.bUnknown = Path.Band == AptitudeBand::InsufficientObservation;
        Row.Band = BandText(Path.Band);
        Row.Coverage = CoverageText(Path.Coverage);
        Row.Printout = PWLaneLife::ToText(AptitudePrintoutLine(Path));
        for (const AptitudeFacetStatement& Facet : Path.Facets)
        {
            FPWProjectionFacetRow& FacetRow = Row.Facets.AddDefaulted_GetRef();
            FacetRow.Name = FacetName(Facet.Facet);
            FacetRow.State = FacetStateText(Facet.State);
            FacetRow.StateValue = Facet.State;
        }
        for (const AptitudeFacet Facet : Path.Unobserved) Row.Unknown.Add(FacetName(Facet));
        const auto& Documented = AptitudeFacetsForPath(Path.Path);
        for (const AptitudeEvidence& Item : Ledger.Items)
        {
            if (std::find(Documented.begin(), Documented.end(), Item.Facet) != Documented.end()) ++Row.ObservationCount;
        }
    }
    return Model;
}

FPWProjectionModel ReadModel(IPWSchoolProjectionWorld& World)
{
    PerfectWorld::AptitudeLedger Ledger;
    FString Why;
    if (!World.ReadLedger(Ledger, Why)) return FPWProjectionModel();
    return BuildModel(Ledger);
}

void SetOpenHandler(TFunction<void()> Handler) { A6ProjectionOpenHandler() = MoveTemp(Handler); }

bool RequestOpen()
{
    if (!A6ProjectionOpenHandler()) return false;
    A6ProjectionOpenHandler()();
    return true;
}

bool TwoPane(const float Width, const float Height) { return PWQuest::TwoPane(Width, Height); }
} // namespace PWSchoolProjection

void SPWSchoolProjection::Construct(const FArguments& InArgs)
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
    if (!Model.Paths.IsEmpty()) SelectedId = Model.Paths[0].Id;

    const TSharedRef<SWidget> Back = PWUI::MakeButton(
        PWSchoolPlanner::Text(TEXT("phase2.a6.common.back")), PWUI::EButtonKind::Primary,
        FOnClicked::CreateSPLambda(this, [this]()
        {
            RequestClose();
            return FReply::Handled();
        }), true, 20, 0.0f);
    BackButton = Back;

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
            .Tag(FName(PWSchoolProjection::PanelTag))
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
                            SNew(STextBlock)
                            .Text(PWSchoolPlanner::Text(TEXT("phase2.a6.projection.title")))
                            .Font_Lambda([IsCompact]() { return PWUI::Font(IsCompact() ? 22 : 32, true); })
                            .ColorAndOpacity(FSlateColor(Style.TextPrimary))
                            .AutoWrapText(true)
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
                        [
                            PWUI::MakeText(PWSchoolPlanner::Text(TEXT("phase2.a6.projection.note")), 16, Style.TextSecondary, false, true)
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
                                PWUI::MakeText(PWSchoolPlanner::Text(TEXT("phase2.a6.projection.hint")), PWUI::ETextRole::Meta, Style.TextSecondary, true)
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f, 0.0f, 0.0f)
                        [
                            SNew(SBox).WidthOverride(220.0f)[Back]
                        ]
                    ]
                ]
            ]
        ]
    ];
    const TSharedPtr<SWidget>* Chosen = RowButtons.Find(SelectedId);
    PWUI::FocusWhenReady(AsShared(), Chosen != nullptr ? TWeakPtr<SWidget>(*Chosen) : BackButton);
}

FVector2D SPWSchoolProjection::GetSpace() const
{
    return Space.Get(FVector2D(1920.0, 1080.0));
}

TSharedRef<SWidget> SPWSchoolProjection::BuildBody()
{
    RowButtons.Reset();
    RowOrder.Reset();
    ListScroll.Reset();
    DetailScroll.Reset();
    DetailHolder.Reset();
    const FVector2D S = GetSpace();
    bTwoPane = PWSchoolProjection::TwoPane(static_cast<float>(S.X), static_cast<float>(S.Y));
    if (!Model.bAvailable)
    {
        return A6ProjectionNote(PWSchoolPlanner::Text(TEXT("phase2.a6.projection.unavailable")), PWSchoolProjection::UnavailableTag);
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
        + SVerticalBox::Slot().FillHeight(0.45f)[List]
        + SVerticalBox::Slot().FillHeight(0.55f).Padding(0.0f, 8.0f, 0.0f, 0.0f)[Detail];
}

TSharedRef<SWidget> SPWSchoolProjection::BuildList()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
    if (Model.bNothingSeen)
    {
        Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
        [
            A6ProjectionNote(PWSchoolPlanner::Text(TEXT("phase2.a6.projection.nothing_seen")), PWSchoolProjection::NothingSeenTag)
        ];
    }
    for (const FPWProjectionPathRow& Row : Model.Paths)
    {
        const FString Id = Row.Id;
        const TSharedRef<SWidget> Content =
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[PWUI::MakeText(Row.Name, 17, Style.TextPrimary, true, true)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
            [
                PWUI::MakeText(Row.Band, PWUI::ETextRole::Meta, Row.bUnknown ? Style.TextSecondary : Style.Accent, true)
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
        Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[SNew(SBox).Tag(FName(PWSchoolProjection::RowTag))[Button]];
    }
    return SAssignNew(ListScroll, SScrollBox)
        .ScrollBarStyle(&Style.ScrollBar)
        .AllowOverscroll(EAllowOverscroll::No)
        + SScrollBox::Slot().Padding(0.0f, 0.0f, 12.0f, 0.0f)
        [
            Column
        ];
}

TSharedRef<SWidget> SPWSchoolProjection::BuildDetailContent()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
    const FPWProjectionPathRow* Row = GetSelectedRow();
    if (Row != nullptr)
    {
        Column->AddSlot().AutoHeight()[PWUI::MakeText(Row->Name, 26, Style.TextPrimary, true, true)];
        Column->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(FText::Format(PWSchoolPlanner::Text(TEXT("phase2.a6.projection.row_fmt")), Row->Band, Row->Coverage), 18,
                           Row->bUnknown ? Style.TextSecondary : Style.Accent, true, true)
        ];
        Column->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 10.0f)[PWUI::MakeText(Row->Printout, 16, Style.TextPrimary, false, true)];
        Column->AddSlot().AutoHeight()[PWUI::MakeText(PWSchoolPlanner::Text(TEXT("phase2.a6.projection.facets")), PWUI::ETextRole::Meta, Style.Accent, true)];
        for (const FPWProjectionFacetRow& Facet : Row->Facets)
        {
            Column->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
            [
                SNew(SBox).Tag(FName(PWSchoolProjection::FacetTag))
                [
                    PWUI::MakeText(FText::Format(PWSchoolPlanner::Text(TEXT("phase2.a6.projection.row_fmt")), Facet.Name, Facet.State), 16,
                                   A6StateColour(Facet.StateValue), false, true)
                ]
            ];
        }
        Column->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(PWSchoolPlanner::Text(TEXT("phase2.a6.projection.unknown_heading")), PWUI::ETextRole::Meta, Style.Accent, true)
        ];
        if (Row->Unknown.IsEmpty())
        {
            Column->AddSlot().AutoHeight()[SNew(SBox).Tag(FName(PWSchoolProjection::UnknownTag))[PWUI::MakeText(PWSchoolPlanner::Text(TEXT("phase2.a6.projection.unknown_none")), 16, Style.TextSecondary, false, true)]];
        }
        else
        {
            Column->AddSlot().AutoHeight()
            [
                SNew(SBox).Tag(FName(PWSchoolProjection::UnknownTag))
                [
                    PWUI::MakeText(FText::Join(FText::FromString(TEXT(", ")), Row->Unknown), 16, Style.TextPrimary, false, true)
                ]
            ];
            Column->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
            [
                PWUI::MakeText(PWSchoolPlanner::Text(TEXT("phase2.a6.projection.unknown_invite")), PWUI::ETextRole::Meta, Style.TextSecondary, true)
            ];
        }
        Column->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(FText::Format(PWSchoolPlanner::Text(TEXT("phase2.a6.projection.evidence_fmt")), FText::AsNumber(Row->ObservationCount)),
                           PWUI::ETextRole::Meta, Style.TextSecondary, true)
        ];
    }
    return Column;
}

TSharedRef<SWidget> SPWSchoolProjection::BuildDetail()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    return SNew(SBorder)
        .Tag(FName(PWSchoolProjection::DetailTag))
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
                    BuildDetailContent()
                ]
            ]
        ];
}

bool SPWSchoolProjection::SelectRow(const FString& Id)
{
    if (Id == SelectedId || Model.FindRow(Id) == nullptr)
    {
        return false;
    }
    SelectedId = Id;
    if (DetailHolder.IsValid())
    {
        // Only the detail pane changes, so the list (and the focused row) is left alone. A pure presentation change.
        DetailHolder->SetContent(BuildDetailContent());
        if (DetailScroll.IsValid()) DetailScroll->SetScrollOffset(0.0f);
    }
    if (ListScroll.IsValid())
    {
        if (const TSharedPtr<SWidget>* Button = RowButtons.Find(Id)) ListScroll->ScrollDescendantIntoView(*Button, false);
    }
    return true;
}

bool SPWSchoolProjection::RefreshLayout()
{
    const FVector2D S = GetSpace();
    if (!BodyHolder.IsValid() || PWSchoolProjection::TwoPane(static_cast<float>(S.X), static_cast<float>(S.Y)) == bTwoPane)
    {
        return false;
    }
    const bool bHadFocus = HasFocusedDescendants();
    BodyHolder->SetContent(BuildBody());
    if (bHadFocus)
    {
        const TSharedPtr<SWidget>* Chosen = RowButtons.Find(SelectedId);
        PWUI::FocusWhenReady(AsShared(), Chosen != nullptr ? TWeakPtr<SWidget>(*Chosen) : BackButton);
    }
    return true;
}

void SPWSchoolProjection::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    RefreshLayout();
}

void SPWSchoolProjection::OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent)
{
    SCompoundWidget::OnFocusChanging(PreviousFocusPath, NewWidgetPath, InFocusEvent);
    // Moving focus to a path chooses it, so the detail never disagrees with the focused row.
    for (int32 Index = NewWidgetPath.Widgets.Num() - 1; Index >= 0; --Index)
    {
        const TSharedRef<SWidget>& Widget = NewWidgetPath.Widgets[Index].Widget;
        for (const TPair<FString, TSharedPtr<SWidget>>& Pair : RowButtons)
        {
            if (Pair.Value.Get() == &Widget.Get())
            {
                SelectRow(Pair.Key);
                return;
            }
        }
    }
}

void SPWSchoolProjection::FocusRow(const FString& Id)
{
    const TSharedPtr<SWidget>* Button = RowButtons.Find(Id);
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

bool SPWSchoolProjection::MoveSelection(const int32 Step)
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
    const FString Id = RowOrder[Target];
    SelectRow(Id);
    FocusRow(Id);
    return true;
}

void SPWSchoolProjection::RequestClose()
{
    OnClose.ExecuteIfBound();
}

FReply SPWSchoolProjection::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    constexpr float A6ScrollStep = 72.0f;
    const FKey Key = InKeyEvent.GetKey();
    if (A6IsBack(Key))
    {
        if (!InKeyEvent.IsRepeat())
        {
            RequestClose();
        }
        return FReply::Handled();
    }
    if (A6IsUp(Key) || A6IsDown(Key))
    {
        return MoveSelection(A6IsUp(Key) ? -1 : 1) ? FReply::Handled() : FReply::Unhandled();
    }
    if (Key == EKeys::Gamepad_RightStick_Up || Key == EKeys::Gamepad_RightStick_Down)
    {
        if (DetailScroll.IsValid())
        {
            const float Delta = Key == EKeys::Gamepad_RightStick_Up ? -A6ScrollStep : A6ScrollStep;
            DetailScroll->SetScrollOffset(FMath::Clamp(DetailScroll->GetScrollOffset() + Delta, 0.0f, DetailScroll->GetScrollOffsetOfEnd()));
        }
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}
