#include "UI/SPWExaminationPanel.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "UI/PWWidgets.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SPWExaminationPanel::Construct(const FArguments& InArgs)
{
    P406Bridge = InArgs._BridgeOverride;
    if (!P406Bridge.IsValid() && InArgs._World != nullptr) P406Bridge = P406Exam::P406MakeLiveWorld(*InArgs._World);
    P406OnClose = InArgs._OnClose;
    P406Space = InArgs._ViewportSize;
    const auto& P406Style = PWUI::FStyle::Get();
    ChildSlot
    [
        SNew(SBox)
        .WidthOverride(FMath::Min(1040.0, FMath::Max(280.0, P406Space.X - 32.0)))
        .MaxDesiredHeight(FMath::Max(240.0, P406Space.Y - 32.0))
        [
            PWUI::MakeBracketPanel(
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return P406Model.Title; }), 24, P406Style.Accent, true, true)]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
                [PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return P406Notice; }), 16, P406Style.Warn, false, true)]
                + SVerticalBox::Slot().FillHeight(1.0f)
                [
                    SAssignNew(P406Scroll, SScrollBox)
                    .AllowOverscroll(EAllowOverscroll::No)
                    + SScrollBox::Slot()[SAssignNew(P406Body, SVerticalBox)]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)[PWUI::MakeRule()]
                + SVerticalBox::Slot().AutoHeight()[SAssignNew(P406Footer, SVerticalBox)]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
                [PWUI::MakeText(FText::FromString(TEXT("Up/Down or D-pad: focus | Enter/A: choose | PgUp/PgDn or shoulders: read | Esc/B: close")),
                    14, P406Style.TextSecondary, false, true)], FMargin(20.0f))
        ]
    ];
    SetTag(TEXT("P406ExaminationPanel"));
    P406Refresh();
}

void SPWExaminationPanel::P406Refresh()
{
    P406Model = P406Bridge.IsValid() ? P406Bridge->Read() : P406Exam::FModel{};
    if (P506Meeting.IsSet())
    {
        bool P506Available = false;
        const auto& P506Command = P506Meeting.GetValue().P407UCommand;
        for (const auto& P506Row : P406Model.P407UFollowups)
            if (P506Row.Action.Id == P506Command.ActionId && P506Row.Action.P502TaskId.empty() &&
                P506Row.CanAttend && P506Row.At == P506Command.ExpectedAt &&
                P506Row.P502Revision == P506Command.P502ExpectedRevision)
            {
                P506Available = true;
                P406Model.Screen = P406Exam::EScreen::P407USession;
                P406Model.Title = P406Exam::P407UFollowupText(P506Row.Action.TitleTextKey);
                P406Model.Lines.Reset();
                P406Model.Lines.Add(P406Exam::P407UFollowupText(P506Row.Action.BriefTextKey));
                P406Model.Lines.Add(FText::FromString(FString::Printf(
                    TEXT("Reading costs no time. Finish meeting spends %d minutes once and records attendance. Close to leave without attending."),
                    P506Row.Action.DurationMinutes)));
                P406Model.Offers.Reset();
                P406Model.Actions.Reset();
                auto P506Finish = P506Meeting.GetValue();
                P506Finish.Label = FText::FromString(TEXT("Finish meeting and record attendance"));
                P406Model.Actions.Add(MoveTemp(P506Finish));
                break;
            }
        if (!P506Available) P506Meeting.Reset();
    }
    P406Rebuild();
}

void SPWExaminationPanel::P406AddLine(const FText& Text)
{
    P406Body->AddSlot().AutoHeight().Padding(0.0f, 5.0f)
        [PWUI::MakeText(Text, 18, PWUI::FStyle::Get().TextPrimary, false, true)];
}

void SPWExaminationPanel::P406AddControl(const FText& Text, bool bEnabled, TFunction<bool()> Activate, bool bFooter)
{
    const int32 P406Index = P406Controls.Num();
    auto P406Button = PWUI::MakeButton(Text, PWUI::EButtonKind::Secondary,
        FOnClicked::CreateSPLambda(this, [this, P406Index]()
        {
            // Copy before activation: a new authoritative projection replaces these arrays.
            auto P406Run = P406Activations[P406Index];
            P406Run();
            return FReply::Handled();
        }), bEnabled, 18);
    P406Controls.Add(P406Button);
    P406Activations.Add(MoveTemp(Activate));
    (bFooter ? P406Footer : P406Body)->AddSlot().AutoHeight().Padding(0.0f, 4.0f)[P406Button];
}

void SPWExaminationPanel::P406Rebuild()
{
    PWUI::CancelPendingFocus(AsShared());
    P406Controls.Reset();
    P406Activations.Reset();
    P406Body->ClearChildren();
    P406Footer->ClearChildren();
    // P410: render the portable sitting projection verbatim; no age, lateness or allowance rules live here.
    const auto& P410Sittings = P406Model.World.Sittings;
    if (P406Model.Screen == P406Exam::EScreen::Decision)
    {
        const auto P410Opening = PerfectWorld::GameClock::ToCalendar(P410Sittings.OpensAt);
        P406AddLine(FText::FromString(FString::Printf(TEXT("Sitting opens: %04d-%02d-%02d %02d:%02d"),
            P410Opening.Year, P410Opening.Month, P410Opening.Day, P410Opening.Hour, P410Opening.Minute)));
    }
    if (P406Model.World.CanWithdraw && P410Sittings.LateMinutes > 0)
        P406AddLine(FText::FromString(FString::Printf(TEXT("Late entry: %lld minutes late | Reduced outside allowance: %lld minutes"),
            static_cast<long long>(P410Sittings.LateMinutes), static_cast<long long>(P410Sittings.AllowanceMinutes))));
    if (P410Sittings.HasNextSitting)
    {
        const auto P410Next = PerfectWorld::GameClock::ToCalendar(P410Sittings.NextSittingAt);
        P406AddLine(FText::FromString(FString::Printf(TEXT("Next annual sitting: %04d-%02d-%02d %02d:%02d"),
            P410Next.Year, P410Next.Month, P410Next.Day, P410Next.Hour, P410Next.Minute)));
    }
    for (const auto& P406Line : P406Model.Lines) P406AddLine(P406Line);
    for (int32 P406Index = 0; P406Index < P406Model.Actions.Num(); ++P406Index)
    {
        const auto& P406Row = P406Model.Actions[P406Index];
        P406AddControl(P406Row.Label, P406Row.bEnabled, [this, P406Index]() { return P406TriggerAction(P406Index); }, false);
    }
    if (P406Model.Offers.Num() > 0)
    {
        P406AddLine(FText::FromString(TEXT("Next steps - qualification offers, not automatic admission:")));
        for (const auto& P406Offer : P406Model.Offers) P406AddLine(P406Offer);
    }
    if (P406Model.World.CanWithdraw)
    {
        P406AddControl(FText::FromString(FString::Printf(TEXT("Withdraw - %lld outside minutes on leaving"),
            static_cast<long long>(P406Model.World.OutsideMinutes))), true, [this]() { return P406TriggerWithdraw(); }, true);
        P406Controls.Last()->SetTag(TEXT("P406Withdraw"));
    }
    P406AddControl(FText::FromString(P406Model.World.Finished ? TEXT("Continue into the day / close (task progress retained)") :
        P406Model.World.DecisionRequired ? TEXT("Pause menu") : TEXT("Close")),
        true, [this]() { P406Close(); return true; }, true);
    P406Selected = 0;
    P406SelectFocus(0);
}

bool SPWExaminationPanel::P406Send(const P406Exam::FAction& Action)
{
    if (!P406Bridge.IsValid()) return false;
    const auto P406Reply = P406Bridge->Submit(Action);
    const auto P506Code = FString(UTF8_TO_TCHAR(P406Reply.Why.Code.c_str()));
    P406Notice = P406Reply.Accepted ? FText::GetEmpty() :
        ((P506Code.StartsWith(TEXT("p502.")) || P506Code.StartsWith(TEXT("p506.")))
            ? FText::FromString(FString(UTF8_TO_TCHAR(P406Reply.Why.Message.c_str()))) : P406Exam::P406RefusalText(P506Code));
    // A successful handoff gives focus to the timetable or the world. The detached
    // examination panel must not queue another focus request while that happens.
    if (P406Reply.Accepted && (Action.bP506SchoolDay || Action.bP506ReturnToWorld))
        PWUI::CancelPendingFocus(AsShared());
    else P406Refresh();
    return P406Reply.Accepted;
}

bool SPWExaminationPanel::P406TriggerAction(int32 Index)
{
    if (!P406Model.Actions.IsValidIndex(Index) || !P406Model.Actions[Index].bEnabled) return false;
    const auto P406Action = P406Model.Actions[Index];
    if (P506Meeting.IsSet()) return P407UFinishSession();
    if (P406Action.bP407UFollowup && !P406Action.bP407UWait &&
        P406Action.P407UCommand.Kind == PerfectWorld::P407UFollowupAction::Attend)
        for (const auto& P506Row : P406Model.P407UFollowups)
            if (P506Row.Action.Id == P406Action.P407UCommand.ActionId && P506Row.Action.P502TaskId.empty())
            {
                P506Meeting = P406Action;
                P406Notice = FText::GetEmpty();
                P406Refresh();
                return true;
            }
    const bool P506Accepted = P406Send(P406Action);
    if (P506Accepted && P406Action.bP506ReturnToWorld) P406Close();
    return P506Accepted;
}

bool SPWExaminationPanel::P407UFinishSession()
{
    if (P506Meeting.IsSet())
    {
        const auto P506Finish = P506Meeting.GetValue();
        P506Meeting.Reset();
        return P406Send(P506Finish);
    }
    // Compatibility with the earlier attendance scenario: explicitly finish the first displayed task choice.
    // Actual UI buttons each submit one disclosed command; no widget-local session state survives reload.
    using PerfectWorld::P407UFollowupAction;
    for (const auto& P506Action : P406Model.Actions)
        if (P506Action.bP407UFollowup && P506Action.P407UCommand.Kind == P407UFollowupAction::P502Start && P506Action.bEnabled)
        {
            const auto P506Start = P506Action;
            if (!P406Send(P506Start)) return false;
            break;
        }
    for (const auto& P506Action : P406Model.Actions)
        if (P506Action.bP407UFollowup && P506Action.P407UCommand.Kind == P407UFollowupAction::P502Resolve && P506Action.bEnabled)
        {
            const auto P506Finish = P506Action;
            return P406Send(P506Finish);
        }
    return false;
}

bool SPWExaminationPanel::P406TriggerWithdraw()
{
    if (!P406Model.World.CanWithdraw) return false;
    P406Exam::FAction P406Action;
    P406Action.Kind = PerfectWorld::TowerSessionAction::Withdraw;
    return P406Send(P406Action);
}

void SPWExaminationPanel::P406Close()
{
    P506Meeting.Reset();
    PWUI::CancelPendingFocus(AsShared());
    P406OnClose.ExecuteIfBound();
}

TSharedPtr<SWidget> SPWExaminationPanel::P406InitialFocus() const
{
    return P406Controls.IsValidIndex(P406Selected) ? P406Controls[P406Selected] : nullptr;
}

void SPWExaminationPanel::P406SelectFocus(int32 Index)
{
    const int32 P406Count = P406Controls.Num();
    if (P406Count == 0) return;
    const int32 P406Direction = Index < P406Selected ? -1 : 1;
    for (int32 P406Attempt = 0; P406Attempt < P406Count; ++P406Attempt)
    {
        const int32 P406Candidate = ((Index + P406Attempt * P406Direction) % P406Count + P406Count) % P406Count;
        if (!P406Controls[P406Candidate]->IsEnabled()) continue;
        P406Selected = P406Candidate;
        PWUI::FocusWhenReady(AsShared(), P406Controls[P406Selected]);
        if (P406Selected < P406Model.Actions.Num())
            P406Scroll->ScrollDescendantIntoView(P406Controls[P406Selected], false);
        return;
    }
}

FReply SPWExaminationPanel::OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    const auto P406Key = Event.GetKey();
    if (P406Key == EKeys::Down || P406Key == EKeys::Gamepad_DPad_Down || (P406Key == EKeys::Tab && !Event.IsShiftDown()))
        P406SelectFocus(P406Selected + 1);
    else if (P406Key == EKeys::Up || P406Key == EKeys::Gamepad_DPad_Up || (P406Key == EKeys::Tab && Event.IsShiftDown()))
        P406SelectFocus(P406Selected - 1);
    else if (P406Key == EKeys::PageDown || P406Key == EKeys::Gamepad_RightShoulder)
        P406Scroll->SetScrollOffset(P406Scroll->GetScrollOffset() + 220.0f);
    else if (P406Key == EKeys::PageUp || P406Key == EKeys::Gamepad_LeftShoulder)
        P406Scroll->SetScrollOffset(FMath::Max(0.0f, P406Scroll->GetScrollOffset() - 220.0f));
    else if (P406Key == EKeys::Escape || P406Key == EKeys::Gamepad_FaceButton_Right || P406Key == EKeys::Gamepad_Special_Right)
        P406Close();
    else if (P406Key == EKeys::Enter || P406Key == EKeys::SpaceBar || P406Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        if (!Event.IsRepeat() && P406Activations.IsValidIndex(P406Selected))
        {
            auto P406Run = P406Activations[P406Selected];
            P406Run();
        }
    }
    else return SCompoundWidget::OnPreviewKeyDown(Geometry, Event);
    return FReply::Handled();
}
