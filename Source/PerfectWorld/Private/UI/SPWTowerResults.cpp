#include "UI/SPWTowerResults.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include <algorithm>

#define LOCTEXT_NAMESPACE "PerfectWorldPhase2A8"

FText SPWTowerResults::Text(const std::string& Key) const
{
    const FText Resolved = World.IsValid() ? World->ResolveText(Key) : FText::GetEmpty();
    return Resolved.IsEmpty() ? LOCTEXT("MissingText", "Details are unavailable. Return to your school plan.") : Resolved;
}

FText SPWTowerResults::Summary() const
{
    using PerfectWorld::TowerParticipation;
    if (PerfectWorld::SchoolFollowupOutcome(View).empty()) return Text("phase2.a8.pending");
    if (View.Participation == TowerParticipation::Deferred)
        return LOCTEXT("Deferred", "Examination deferred. No tower rank was earned this year.");
    if (View.Participation == TowerParticipation::Declined)
        return LOCTEXT("Declined", "Examination declined. No tower rank was earned this year.");
    const FText Status = View.Participation == TowerParticipation::Withdrawn ? LOCTEXT("Withdrawn", "Withdrawn") :
        View.Participation == TowerParticipation::Ejected ? LOCTEXT("Ejected", "Tower attempt ended after defeat") : LOCTEXT("Completed", "Tower completed");
    return FText::Format(LOCTEXT("Summary", "{0}. Floors cleared: {1}. Score: {2}. National rank: {3}; decile: {4}.\nRank uses the current synthetic 1,000-person cohort. Academic marks are a separate record.\nSupport application envelope: {5} paise; this is not awarded money."),
        Status, FText::AsNumber(View.Result.FloorsCleared), FText::AsNumber(View.Result.ScorePoints),
        FText::AsNumber(View.Result.NationalRank), FText::AsNumber(View.Result.Decile), FText::AsNumber(View.Result.SupportEnvelopePaise));
}

void SPWTowerResults::Construct(const FArguments& InArgs)
{
    World = InArgs._World;
    Space = InArgs._Space;
    OnClose = InArgs._OnClose;
    if (World.IsValid())
    {
        View = World->QueryResults();
        Actions = PerfectWorld::QuerySchoolFollowups(World->FollowupContent(), View);
    }
    const auto& Style = PWUI::FStyle::Get();
    const TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
    Rows->AddSlot().AutoHeight().Padding(0, 0, 0, 12)[PWUI::MakeText(Summary(), 18, Style.TextPrimary, false, true)];
    Rows->AddSlot().AutoHeight().Padding(0, 0, 0, 12)[PWUI::MakeText(Text("phase2.a8.continuity"), 18, Style.TextSecondary, false, true)];
    for (const auto& Credential : View.Result.Credentials)
    {
        // Credential IDs and evidence are supplied by the view, never reconstructed.
        Rows->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
        [PWUI::MakeText(FText::Format(LOCTEXT("Credential", "Credential: {0}. Evidence: {1}/1000."),
            Text("phase2.a8." + std::string(Credential.Id)), FText::AsNumber(Credential.ScoreMilli)),
            18, Style.TextSecondary, false, true)];
    }
    for (const auto& Action : Actions)
    {
        const std::string Id = Action.Id;
        const TSharedRef<SWidget> Button = PWUI::MakeButton(Text(Action.TitleTextKey), PWUI::EButtonKind::Secondary,
            FOnClicked::CreateSPLambda(this, [this, Id]() { OpenAction(Id); return FReply::Handled(); }), true, 20, 0.0f);
        Buttons.Add(Button);
        Rows->AddSlot().AutoHeight().Padding(0, 8, 0, 4)[Button];
        Rows->AddSlot().AutoHeight()[PWUI::MakeText(Text(Action.BriefTextKey), 18, Style.TextPrimary, false, true)];
        Rows->AddSlot().AutoHeight().Padding(0, 4, 0, 8)
        [PWUI::MakeText(FText::Format(LOCTEXT("SuggestedTime", "Suggested day after exam: {0}, {1}:{2}; activity duration: {3} minutes. Confirm availability in your plan."),
            FText::AsNumber(Action.DayOffset), FText::FromString(FString::Printf(TEXT("%02d"), Action.Minute / 60)),
            FText::FromString(FString::Printf(TEXT("%02d"), Action.Minute % 60)), FText::AsNumber(Action.DurationMinutes)),
            16, Style.TextSecondary, false, true)];
    }
    Back = PWUI::MakeButton(LOCTEXT("Back", "Back"), PWUI::EButtonKind::Primary,
        FOnClicked::CreateSPLambda(this, [this]() { Close(); return FReply::Handled(); }), true, 20, 0.0f);
    ChildSlot
    [
        SNew(SBox)
        .WidthOverride_Lambda([this]() { return FOptionalSize(FMath::Max(300.0f, FMath::Min(960.0f, static_cast<float>(Space.Get().X) - 32.0f))); })
        .HeightOverride_Lambda([this]() { return FOptionalSize(FMath::Max(240.0f, static_cast<float>(Space.Get().Y) - 32.0f)); })
        [PWUI::MakeBracketPanel(
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[PWUI::MakeText(Text("phase2.a8.title"), 24, Style.Accent, true, true)]
            + SVerticalBox::Slot().FillHeight(1.0f)
            [SAssignNew(Scroll, SScrollBox).ScrollBarStyle(&Style.ScrollBar).AllowOverscroll(EAllowOverscroll::No)
                + SScrollBox::Slot().Padding(0, 0, 12, 0)[Rows]]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 8)
            [PWUI::MakeText(TAttribute<FText>::CreateSPLambda(this, [this]() { return Feedback; }), 16, Style.TextSecondary, false, true)]
            + SVerticalBox::Slot().AutoHeight()[Back.ToSharedRef()], FMargin(16))]
    ];
    PWUI::FocusWhenReady(AsShared(), Buttons.IsEmpty() ? TWeakPtr<SWidget>(Back) : TWeakPtr<SWidget>(Buttons[0]));
}

bool SPWTowerResults::OpenAction(const std::string& ActionId)
{
    const TSharedRef<SWidget> KeepAlive = AsShared();
    if (!World.IsValid()) return false;
    const auto Current = PerfectWorld::QuerySchoolFollowups(World->FollowupContent(), World->QueryResults());
    const bool Available = std::any_of(Current.begin(), Current.end(), [&](const auto& Action) { return Action.Id == ActionId; });
    // Hold bridge alive across callbacks, which may replace the host panel.
    const TSharedPtr<IA8ResultsWorld> Bridge = World;
    const bool Accepted = Available && Bridge->OpenNextAction(ActionId);
    Feedback = Text(Accepted ? "phase2.a8.accepted" : "phase2.a8.rejected");
    return Accepted;
}

void SPWTowerResults::Close()
{
    PWUI::CancelPendingFocus(AsShared());
    OnClose.ExecuteIfBound();
}

void SPWTowerResults::FocusAction(int32 Index)
{
    if (!FSlateApplication::IsInitialized()) return;
    const TSharedPtr<SWidget> Target = Index == Buttons.Num() ? Back : Buttons.IsValidIndex(Index) ? Buttons[Index] : nullptr;
    if (!Target.IsValid()) return;
    const auto Focusable = PWUI::FindFirstFocusable(Target.ToSharedRef());
    if (Focusable.IsValid()) FSlateApplication::Get().SetKeyboardFocus(Focusable, EFocusCause::Navigation);
    if (Index < Buttons.Num() && Scroll.IsValid()) Scroll->ScrollDescendantIntoView(Target, false);
}

FReply SPWTowerResults::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right)
    {
        if (!InKeyEvent.IsRepeat()) Close();
        return FReply::Handled();
    }
    const bool Up = Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up;
    const bool Down = Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down;
    if (Up || Down)
    {
        int32 Current = Buttons.Num();
        for (int32 Index = 0; Index < Buttons.Num(); ++Index)
            if (Buttons[Index]->HasKeyboardFocus() || Buttons[Index]->HasFocusedDescendants()) { Current = Index; break; }
        FocusAction(FMath::Clamp(Current + (Up ? -1 : 1), 0, Buttons.Num()));
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
