#include "UI/SPWStreamPage.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "Input/NavigationReply.h"
#include "InputCoreTypes.h"
#include "UI/PWCreationModel.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldStreamPage"


void SPWStreamPage::Construct(const FArguments& InArgs, const TSharedRef<FPWCreationModel>& InModel)
{
    Model = InModel;
    NarrowLayout = InArgs._NarrowLayout;
    OnFocusDetails = InArgs._OnFocusDetails;
    OnFocusFooter = InArgs._OnFocusFooter;
    DetailReveal = DetailTransition.AddCurve(0, .16f, ECurveEaseFunction::CubicOut);
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    SAssignNew(ReasonBox, SEditableTextBox).Style(&Style.TextBox).TextShapingMethod(ETextShapingMethod::FullShaping)
        .Text(FText::FromString(Model->SchoolReason)).HintText(LOCTEXT("ReasonHint", "What draws you to this stream?"))
        .OnTextCommitted(this, &SPWStreamPage::CommitReason);
    for (int32 Index = 0; Index < Model->Streams.Num(); ++Index) Cards.Add(BuildCard(Index));
    Details = BuildDetails();
    SAssignNew(Root, SVerticalBox);
    ChildSlot[Root.ToSharedRef()];
    RebuildLayout(false);
}

void SPWStreamPage::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    if (LastStream != SelectedStreamId())
    {
        LastStream = SelectedStreamId();
        DetailTransition.Play(AsShared());
    }
    Details->SetRenderOpacity(DetailReveal.GetLerp());
    const bool bShouldBeNarrow = NarrowLayout.Get(AllottedGeometry.GetLocalSize().X < 1280.0f);
    if (!bLayoutInitialized || bShouldBeNarrow != bNarrow) RebuildLayout(bShouldBeNarrow);
}

void SPWStreamPage::RebuildLayout(bool bUseNarrowLayout)
{
    TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetKeyboardFocusedWidget();
    bNarrow = bUseNarrowLayout;
    bLayoutInitialized = true;
    Root->ClearChildren();
    Details.Reset();
    Details = BuildDetails();
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    Root->AddSlot().AutoHeight()[PWUI::MakeText(LOCTEXT("Title", "Which stream will you study?"), PWUI::ETextRole::PageHeading, Style.TextPrimary)];
    Root->AddSlot().AutoHeight().Padding(0, 6, 0, 18)[PWUI::MakeText(LOCTEXT("Intro", "A stream sets a timetable, not a destiny."), PWUI::ETextRole::Body, Style.TextSecondary)];
    if (bNarrow)
    {
        const TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
        for (const TSharedPtr<SWidget>& Card : Cards) Body->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[Card.ToSharedRef()];
        Body->AddSlot().AutoHeight()[PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return StreamProblem; }), PWUI::ETextRole::Body, Style.Error, true)];
        Body->AddSlot().AutoHeight().Padding(0, 10, 0, 0)[Details.ToSharedRef()];
        Root->AddSlot().AutoHeight()[Body];
    }
    else
    {
        const TSharedRef<SHorizontalBox> CardRow = SNew(SHorizontalBox);
        for (const TSharedPtr<SWidget>& Card : Cards) CardRow->AddSlot().FillWidth(1).Padding(0, 0, 12, 0)[Card.ToSharedRef()];
        Root->AddSlot().AutoHeight()[CardRow];
        Root->AddSlot().AutoHeight()[PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return StreamProblem; }), PWUI::ETextRole::Body, Style.Error, true)];
        Root->AddSlot().AutoHeight().Padding(0, 18, 0, 0)[Details.ToSharedRef()];
    }
    if (Focused.IsValid()) FSlateApplication::Get().SetKeyboardFocus(Focused, EFocusCause::SetDirectly);
}

FReply SPWStreamPage::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    int32 FocusedIndex = INDEX_NONE;
    for (int32 Index = 0; Index < Cards.Num(); ++Index)
    {
        if (Cards[Index]->HasKeyboardFocus() || Cards[Index]->HasFocusedDescendants()) FocusedIndex = Index;
    }
    const bool bCardDirection = bNarrow
        ? (Key == EKeys::Up || Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_DPad_Down)
        : (Key == EKeys::Left || Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_DPad_Right);
    if (bCardDirection && FocusedIndex != INDEX_NONE)
    {
        const bool bForward = Key == EKeys::Down || Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_DPad_Right;
        FSlateApplication::Get().SetKeyboardFocus(Cards[(FocusedIndex + (bForward ? 1 : -1) + Cards.Num()) % Cards.Num()], EFocusCause::Navigation);
        return FReply::Handled();
    }
    if (Key == EKeys::F6)
    {
        TSharedPtr<SWidget> Target;
        if (FocusedIndex != INDEX_NONE) Target = ReasonBox;
        else if (Cards.Num()) Target = Cards[0];
        if (Target.IsValid()) FSlateApplication::Get().SetKeyboardFocus(Target, EFocusCause::Navigation);
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

TSharedRef<SWidget> SPWStreamPage::BuildCard(int32 Index)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const FPWStreamView& Stream = Model->Streams[Index];
    const FName Id = Stream.Id;
    const TSharedRef<SWidget> Content = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[PWUI::MakeText(Stream.DisplayName, PWUI::ETextRole::SectionHeading, Style.TextPrimary, true)]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)[PWUI::MakeText(Stream.TimetableSummary, PWUI::ETextRole::Body, Style.TextSecondary, true)];
    CardContents.Add(Content);
    return PWUI::MakeContentButton(SNew(SBox).MinDesiredHeight_Lambda([this]()
        {
            float Height = 108.f;
            for (const auto& Item : CardContents) Height = FMath::Max(Height, static_cast<float>(Item->GetDesiredSize().Y));
            return Height;
        })[Content], PWUI::EButtonKind::Card,
        FOnClicked::CreateLambda([this, Id]()
        {
            const FString Reason = ReasonBox.IsValid() ? ReasonBox->GetText().ToString() : Model->SchoolReason;
            const bool bApplied = Model->Edit([Id, Reason](UPerfectWorldCharacterCreatorSubsystem& Creator) { return Creator.SetSchoolStream(Id, Reason); });
            StreamProblem = bApplied ? FText::GetEmpty() : Model->Message;
            return FReply::Handled();
        }), TAttribute<bool>::CreateLambda([this, Index]() { return Model->Streams.IsValidIndex(Index) && Model->Streams[Index].bSelected; }), true, FMargin(20, 16));
}

TSharedRef<SWidget> SPWStreamPage::BuildDetails()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    SampleDayRegion = PWUI::MakePanel(SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[PWUI::MakeText(LOCTEXT("SampleHeading", "A sample school day"), PWUI::ETextRole::SectionHeading, Style.TextPrimary)]
            // TODO(core adapter): replace this timetable-summary fallback when SampleDay rows are published.
            + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)[PWUI::MakeText(TAttribute<FText>::CreateLambda([this]()
            {
                const FPWStreamView* Stream = SelectedStream();
                if (!Stream) return FText::GetEmpty();
                if (Stream->SampleDay.IsEmpty()) return Stream->TimetableSummary;
                TArray<FText> Lines;
                for (const auto& Row : Stream->SampleDay) Lines.Add(FText::Format(LOCTEXT("SampleRow", "{0} — {1}"), Row.Label, Row.Description));
                return FText::Join(LOCTEXT("SampleBreak", "\n\n"), Lines);
            }), PWUI::ETextRole::Body, Style.TextSecondary, true)]);
    const TSharedRef<SWidget> ReasonPanel = PWUI::MakePanel(SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility_Lambda([this]() { const FPWStreamView* Stream = SelectedStream(); return Stream && !Stream->RouteFlexibilityText.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
                [PWUI::MakeText(LOCTEXT("RoutesHeading", "Room to change direction"), PWUI::ETextRole::SectionHeading, Style.TextPrimary)]]
            // TODO(core adapter): this remains collapsed until route text is published.
            + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)[PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { const FPWStreamView* Stream = SelectedStream(); return Stream ? Stream->RouteFlexibilityText : FText::GetEmpty(); }), PWUI::ETextRole::Body, Style.TextSecondary, true)]
            + SVerticalBox::Slot().AutoHeight()[PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { const FPWStreamView* Stream = SelectedStream(); return Stream ? Stream->EqualStartingXPText : FText::GetEmpty(); }), PWUI::ETextRole::Body, Style.TextSecondary, true)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 6)[PWUI::MakeText(LOCTEXT("ReasonLabel", "Your reason (optional)"), PWUI::ETextRole::Control, Style.TextPrimary)]
            + SVerticalBox::Slot().AutoHeight()[ReasonBox.ToSharedRef()]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)[PWUI::MakeText(LOCTEXT("ReasonHelper", "This is your note. It does not change your stats."), PWUI::ETextRole::Meta, Style.TextMuted, true)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)[PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return ReasonProblem; }), PWUI::ETextRole::Body, Style.Error, true)]);
    if (bNarrow) return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[SampleDayRegion.ToSharedRef()]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 14, 0, 0)[ReasonPanel];
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(.6f).Padding(0, 0, 24, 0)[SampleDayRegion.ToSharedRef()]
        + SHorizontalBox::Slot().FillWidth(.4f)[ReasonPanel];
}

void SPWStreamPage::CommitReason(const FText& Text, ETextCommit::Type CommitType)
{
    const FName Id = SelectedStreamId();
    const FString Reason = Text.ToString();
    const bool bApplied = Model->Edit([Id, Reason](UPerfectWorldCharacterCreatorSubsystem& Creator) { return Creator.SetSchoolStream(Id, Reason); });
    ReasonProblem = bApplied ? FText::GetEmpty() : Model->Message;
}

FName SPWStreamPage::SelectedStreamId() const
{
    for (const FPWStreamView& Stream : Model->Streams) if (Stream.bSelected) return Stream.Id;
    return Model->Streams.Num() > 0 ? Model->Streams[0].Id : NAME_None;
}

const FPWStreamView* SPWStreamPage::SelectedStream() const
{
    for (const FPWStreamView& Stream : Model->Streams) if (Stream.bSelected) return &Stream;
    return Model->Streams.Num() > 0 ? &Model->Streams[0] : nullptr;
}

FNavigationReply SPWStreamPage::OnNavigation(const FGeometry& Geometry, const FNavigationEvent& Event)
{
    const auto Direction = Event.GetNavigationType();
    const bool bAlongCards = bNarrow ? Direction == EUINavigation::Up || Direction == EUINavigation::Down
        : Direction == EUINavigation::Left || Direction == EUINavigation::Right;
    if (bAlongCards) for (int32 Index = 0; Index < Cards.Num(); ++Index)
        if (Cards[Index]->HasKeyboardFocus() || Cards[Index]->HasFocusedDescendants())
        {
            const bool bForward = Direction == EUINavigation::Down || Direction == EUINavigation::Right;
            return FNavigationReply::Explicit(Cards[(Index + (bForward ? 1 : Cards.Num() - 1)) % Cards.Num()]);
        }
    return SCompoundWidget::OnNavigation(Geometry, Event);
}

void SPWStreamPage::FocusRegion(bool bReverse, bool bFromFooter)
{
    for (const auto& Card : Cards) if (Card->HasKeyboardFocus() || Card->HasFocusedDescendants()) Region = 0;
    if (ReasonBox->HasKeyboardFocus() || ReasonBox->HasFocusedDescendants()) Region = 2;
    if (bFromFooter) Region = 3;
    Region = (Region + (bReverse ? 3 : 1)) % 4;
    if (Region == 1) OnFocusDetails.ExecuteIfBound(SampleDayRegion);
    else if (Region == 2) PWUI::FocusWhenReady(AsShared(), ReasonBox);
    else if (Region == 3) OnFocusFooter.ExecuteIfBound();
    else if (!Cards.IsEmpty()) PWUI::FocusWhenReady(AsShared(), Cards[0]);
}

#undef LOCTEXT_NAMESPACE
