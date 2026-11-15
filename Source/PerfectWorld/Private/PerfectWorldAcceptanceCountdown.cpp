#include "PerfectWorldAcceptanceCountdown.h"

#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/Children.h"
#include "PerfectWorldAcceptanceAwakening.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "UI/SPWCountdownPanel.h"
#include "UI/SPWPauseMenu.h"
#include "Widgets/SWindow.h"

namespace
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

struct FP312Run
{
    TWeakPtr<SPWPauseMenu> Pause;
    TWeakPtr<SPWCountdownPanel> Panel;
    FString Opportunity;
    PerfectWorld::GameInstant GrantAt = 0;
};

FStepDef P312Action(const TCHAR* Name, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run)
{
    FStepDef Step;
    Step.Name = Name;
    Step.TimeoutSeconds = 30.0;
    Step.Run = MoveTemp(Run);
    return Step;
}

TSharedPtr<SWidget> P312FindWidget(const TSharedRef<SWidget>& Root, const FString& Type, const FName Tag = NAME_None)
{
    if (!Root->GetVisibility().IsVisible()) return nullptr;
    if ((!Type.IsEmpty() && Root->GetTypeAsString() == Type) || (!Tag.IsNone() && Root->GetTag() == Tag)) return Root;
    FChildren* Children = Root->GetChildren();
    for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        if (auto Found = P312FindWidget(Children->GetChildAt(Index), Type, Tag)) return Found;
    return nullptr;
}

TSharedPtr<SPWPauseMenu> P312PauseOnScreen()
{
    if (FSlateApplication::IsInitialized())
        for (const auto& Window : FSlateApplication::Get().GetTopLevelWindows())
            if (auto Found = P312FindWidget(Window, TEXT("SPWPauseMenu"))) return StaticCastSharedPtr<SPWPauseMenu>(Found);
    return nullptr;
}
}

namespace PWAcceptance
{
void P312AppendCountdownSteps(TArray<FStepDef>& Out)
{
    // Reuse the real walked tutorial/grant path. Stop after its 17:59/18:00 boundary assertion, before its ordinary-day tail.
    P38AppendAwakeningSteps(Out);
    for (int32 Index = 0; Index < Out.Num(); ++Index)
        if (Out[Index].Name == TEXT("countdown_closed_1759_opens_1800")) { Out.SetNum(Index + 1); break; }
    const auto Run = MakeShared<FP312Run>();
    Out.Add(P312Action(TEXT("p312_pass_evening_and_open_pause"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        auto* Hud = Sub.GetHud();
        if (!Life || !Hud || !Life->GetWorldState().Awakening.Granted) return FStepOutcome::Fail(TEXT("no live grant or pause host"));
        Run->GrantAt = Life->GetWorldState().Awakening.Granted->At;
        const auto Done = Life->SubmitAwakeningCommand({PerfectWorld::CommandSource::Player, PerfectWorld::AdvanceTime{1}});
        if (!Done.Accepted || Life->GetNow() <= PerfectWorld::CountdownStartInstant())
            return FStepOutcome::Fail(TEXT("could not pass the evening opening: ") + PWLaneLife::ToFString(Done.Why.Message));
        Hud->ToggleInWorldMenu();
        return Hud->IsPaused() ? FStepOutcome::Pass(TEXT("passed 18:00 using the bound clock; opened the real pause menu"))
            : FStepOutcome::Fail(TEXT("pause did not open"));
    }));
    Out.Add(P312Action(TEXT("p312_pause_countdown_entry_opens_panel"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto Pause = P312PauseOnScreen();
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!Pause.IsValid()) return FStepOutcome::Wait();
        if (!Life || !P312FindWidget(Pause.ToSharedRef(), FString(), FName(TEXT("P312PauseCountdown"))))
            return FStepOutcome::Fail(TEXT("the visible Countdown entry is missing"));
        const auto Before = Life->GetNow();
        if (!Pause->P312OpenCountdown() || !Pause->P312GetCountdown().IsValid()) return FStepOutcome::Fail(TEXT("the pause entry could not host the panel"));
        Run->Pause = Pause;
        Run->Panel = Pause->P312GetCountdown();
        const auto Panel = Run->Panel.Pin();
        const auto Countdown = Life->GetSim().Countdown();
        if (!Countdown || Countdown->Obligations.empty() || !Panel->GetModel().bAvailable ||
            Panel->GetModel().Obligations.Num() != static_cast<int32>(Countdown->Obligations.size()) || Before != Life->GetNow())
            return FStepOutcome::Fail(TEXT("the panel does not show live obligations, or opening consumed time"));
        return FStepOutcome::Pass(TEXT("the pause-menu Countdown action opened the live panel with core obligations, without consuming time"));
    }));
    Out.Add(MakeShotStep(TEXT("p312_shot_countdown_open")));
    Out.Add(P312Action(TEXT("p312_take_opportunity_from_panel"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto Panel = Run->Panel.Pin();
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!Panel.IsValid() || !Life) return FStepOutcome::Fail(TEXT("no countdown panel"));
        const auto Rows = Panel->GetModel().Offers;
        for (int32 Index = 0; Index < Rows.Num(); ++Index)
        {
            if (!Rows[Index].bOpen || Rows[Index].bP312Practice) continue;
            Run->Opportunity = Rows[Index].Id;
            Panel->SelectOfferIndex(Index);
            if (!Panel->TriggerConfirmSelectedOffer()) return FStepOutcome::Fail(Panel->GetLastFeedbackText().ToString());
            const auto State = Life->GetSim().Countdown();
            if (!State || !PerfectWorld::CountdownFindCommitment(*State, TCHAR_TO_UTF8(*Run->Opportunity)))
                return FStepOutcome::Fail(TEXT("the panel's opportunity was not committed in the core"));
            return FStepOutcome::Pass(TEXT("panel submitted countdown.opportunity.take; the core stored the commitment"));
        }
        return FStepOutcome::Fail(TEXT("no authored opportunity is open"));
    }));
    Out.Add(P312Action(TEXT("p312_negotiate_from_panel"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto Panel = Run->Panel.Pin();
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!Panel.IsValid() || !Life) return FStepOutcome::Fail(TEXT("no live panel"));
        const auto Rows = Panel->GetModel().Obligations;
        for (const auto& Row : Rows)
            for (const auto& Proposal : Row.Proposals)
                if (Proposal.bAvailable && Proposal.Proposal == PerfectWorld::CountdownProposal::Defer)
                {
                    if (!Panel->P312TriggerProposal(Row.Id, Proposal.Proposal)) return FStepOutcome::Fail(Panel->GetLastFeedbackText().ToString());
                    const auto State = Life->GetSim().Countdown();
                    const auto* Record = State ? PerfectWorld::CountdownFindRecord(*State, TCHAR_TO_UTF8(*Row.Id)) : nullptr;
                    return Record && Record->Negotiated ? FStepOutcome::Pass(TEXT("panel submitted a disclosed deferral; the core stored the single attempt, accepted or refused"))
                        : FStepOutcome::Fail(TEXT("the negotiation attempt is absent from the core"));
                }
        return FStepOutcome::Fail(TEXT("no disclosed deferral is available"));
    }));
    Out.Add(P312Action(TEXT("p312_keep_ordinary_day_is_free"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto Panel = Run->Panel.Pin();
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!Panel.IsValid() || !Life) return FStepOutcome::Fail(TEXT("no live panel"));
        const auto Before = Life->GetNow();
        const auto Seq = Life->GetWorldState().Seq;
        if (!Panel->TriggerKeepOrdinaryDay() || Before != Life->GetNow() || Seq != Life->GetWorldState().Seq)
            return FStepOutcome::Fail(TEXT("keeping an ordinary day was refused or submitted a hidden command"));
        return FStepOutcome::Pass(TEXT("ordinary-day acknowledgement cost no time or command and retained taken commitments"));
    }));
    Out.Add(P312Action(TEXT("p312_resolve_days_through_panel_to_deadline"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        const auto Panel = Run->Panel.Pin();
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!Panel.IsValid() || !Life) return FStepOutcome::Fail(TEXT("no live panel"));
        auto State = Life->GetSim().Countdown();
        if (!State) return FStepOutcome::Fail(TEXT("countdown state disappeared"));
        if (!State->Closed)
        {
            if (Rt.Phase >= 12) return FStepOutcome::Fail(TEXT("day resolution exceeded the ten-day continuation budget"));
            const auto Before = Life->GetNow();
            if (!Panel->GetModel().bP312CanResolve || !Panel->P312TriggerResolveDays()) return FStepOutcome::Fail(Panel->GetLastFeedbackText().ToString());
            if (Life->GetNow() <= Before) return FStepOutcome::Fail(TEXT("the accepted day made no progress"));
            Rt.NextPhase();
            return FStepOutcome::Wait(); // each next day and the news disclosure are rendered before the next press
        }
        bool Honoured = false;
        int32 NewsLines = 0;
        for (const auto& Line : State->Ledger)
        {
            Honoured = Honoured || Line.Kind == PerfectWorld::CountdownLedgerKind::Honoured;
            if (Line.Kind == PerfectWorld::CountdownLedgerKind::News) ++NewsLines;
        }
        const auto* Commitment = PerfectWorld::CountdownFindCommitment(*State, TCHAR_TO_UTF8(*Run->Opportunity));
        const auto View = Life->GetSim().CountdownView();
        const auto Expected = PerfectWorld::GameClock::FromCalendar(2043, 2, 2, 6, 0);
        if (!Honoured || !Commitment || !Commitment->Resolved || NewsLines != 1 || Life->GetNow() != Expected || State->End != Expected ||
            !View.ExaminationDue || View.PreparationAvailable || !PerfectWorld::IsWorldDayInStep(Life->GetWorldState()) ||
            !PerfectWorld::IsWorldQuiescent(Life->GetWorldState()) || !Life->GetWorldState().Awakening.Granted ||
            Life->GetWorldState().Awakening.Granted->At != Run->GrantAt)
            return FStepOutcome::Fail(TEXT("deadline, honoured promise, resolved opportunity, one news delivery or unchanged grant disagrees with core truth"));
        return FStepOutcome::Pass(TEXT("the core closed the countdown at 2 February 2043 06:00; a standing obligation was honoured and the selected opportunity resolved; one news delivery"));
    }));
    Out.Add(MakeShotStep(TEXT("p312_shot_countdown_deadline")));
    Out.Add(P312Action(TEXT("p312_return_to_play_after_deadline"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* Hud = Sub.GetHud();
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        const auto Pause = Run->Pause.Pin();
        if (!Hud || !Life) return FStepOutcome::Fail(TEXT("no pause host"));
        if (Rt.Phase == 0)
        {
            if (!Pause.IsValid()) return FStepOutcome::Fail(TEXT("pause menu was lost before closing"));
            Pause->P312CloseCountdown();
            Hud->ResumeGame();
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        const auto Done = Life->SubmitAwakeningCommand({PerfectWorld::CommandSource::Player, PerfectWorld::AdvanceTime{1}});
        return !Hud->IsMenuOpen() && Done.Accepted ? FStepOutcome::Pass(TEXT("panel and pause closed; ordinary time still advances after examination morning, with no countdown dead end"))
            : FStepOutcome::Fail(TEXT("play did not resume: ") + PWLaneLife::ToFString(Done.Why.Message));
    }));
}
}
