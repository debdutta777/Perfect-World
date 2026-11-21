#include "PerfectWorldAcceptanceExamination.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "PerfectWorldAcceptanceAwakening.h"
#include "PerfectWorldAwakeningResultBridge.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldSchoolPresenter.h"
#include "UI/SPWExaminationPanel.h"

namespace
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

FStepDef P406ScenarioAction(const TCHAR* P406Name,
    TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> P406Run)
{
    FStepDef P406Step;
    P406Step.Name = P406Name;
    P406Step.TimeoutSeconds = 30.0;
    P406Step.Run = MoveTemp(P406Run);
    return P406Step;
}

TSharedPtr<SPWExaminationPanel> P406ScenarioPanel(UPWAcceptanceSubsystem& P406Sub)
{
    const auto* P406Host = UPWSchoolPresenter::Get(P406Sub.GetPlayWorld());
    return P406Host != nullptr && P406Host->GetKind() == EPWSchoolPanel::P406Examination
        ? StaticCastSharedPtr<SPWExaminationPanel>(P406Host->GetPanel()) : nullptr;
}

FStepDef P406ScenarioPress(const TCHAR* P406Name, PerfectWorld::TowerSessionAction P406Kind)
{
    return P406ScenarioAction(P406Name, [P406Kind](UPWAcceptanceSubsystem& P406Sub, FStepRuntime&)
    {
        const auto P406Panel = P406ScenarioPanel(P406Sub);
        if (!P406Panel.IsValid()) return FStepOutcome::Fail(TEXT("the live examination panel is absent"));
        if (P406Kind == PerfectWorld::TowerSessionAction::Withdraw)
            return P406Panel->P406TriggerWithdraw() ? FStepOutcome::Pass(TEXT("panel submitted Withdraw"))
                : FStepOutcome::Fail(P406Panel->P406GetNotice().ToString());
        const auto P406Rows = P406Panel->P406GetModel().Actions;
        for (int32 P406Index = 0; P406Index < P406Rows.Num(); ++P406Index)
        {
            if (P406Rows[P406Index].Kind != P406Kind) continue;
            if (!P406Rows[P406Index].bEnabled)
                return FStepOutcome::Fail(TEXT("action unavailable: ") + PWLaneLife::ToFString(P406Panel->P406GetModel().World.SitRefusal));
            if (!P406Panel->P406TriggerAction(P406Index))
                return FStepOutcome::Fail(P406Panel->P406GetNotice().ToString());
            return FStepOutcome::Pass(TEXT("the live panel submitted one accepted examination command"));
        }
        return FStepOutcome::Fail(TEXT("the requested examination action is absent"));
    });
}

void P406ScenarioMorning(TArray<FStepDef>& P406Out)
{
    // Uses real movement, tutorial commands and grant; no fabricated WorldState or unbound temporary sim.
    PWAcceptance::P38AppendAwakeningSteps(P406Out);
    P406Out.Add(P406ScenarioAction(TEXT("p406_resolve_real_days_to_exam"), [](UPWAcceptanceSubsystem& P406Sub, FStepRuntime& P406Rt)
    {
        auto* P406Life = UPWLaneLifeSubsystem::Get(P406Sub.GetPlayWorld());
        const auto P406Bridge = PWAwakening::ApBridge::Resolve(P406Sub.GetPlayWorld());
        if (P406Life == nullptr || !P406Life->GetWorldState().Awakening.Granted || !P406Bridge.IsValid())
            return FStepOutcome::Fail(TEXT("requires the real grant and countdown bridge"));
        if (P406Rt.Phase == 0)
            P406Rt.Memo = FString::Printf(TEXT("%lld"), static_cast<long long>(P406Life->GetWorldState().Awakening.Granted->At));
        const auto P406Countdown = P406Life->GetSim().Countdown();
        if (!P406Countdown) return FStepOutcome::Fail(TEXT("the live countdown is absent"));
        if (!P406Countdown->Closed)
        {
            if (P406Rt.Phase >= 12) return FStepOutcome::Fail(TEXT("ordinary days exceeded the bounded continuation budget"));
            const auto P406Model = P406Bridge->ReadCountdownModel();
            if (!P406Model.bP312CanResolve || P406Model.P312Days.empty())
                return FStepOutcome::Fail(TEXT("the live countdown has no disclosed ordinary-day plan"));
            const auto P406Before = P406Life->GetNow();
            // Exact payload already disclosed by the production countdown bridge; honours bound contacts.
            const auto P406Reply = P406Life->SubmitAwakeningCommand({PerfectWorld::CommandSource::Player,
                PerfectWorld::CountdownResolveDays{P406Model.P312Days}});
            if (!P406Reply.Accepted) return FStepOutcome::Fail(PWLaneLife::ToFString(P406Reply.Why.Code) +
                TEXT(": ") + PWLaneLife::ToFString(P406Reply.Why.Message));
            if (P406Life->GetNow() <= P406Before) return FStepOutcome::Fail(TEXT("the accepted day made no clock progress"));
            P406Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        const auto P406Expected = PerfectWorld::CountdownEndInstant();
        const auto P406View = P406Life->GetSim().ExaminationWorldView();
        if (P406Life->GetNow() != P406Expected || !P406View.DecisionRequired ||
            P406Rt.Memo != FString::Printf(TEXT("%lld"), static_cast<long long>(P406Life->GetWorldState().Awakening.Granted->At)))
            return FStepOutcome::Fail(TEXT("the clock, required decision or unchanged grant disagree at examination morning"));
        return FStepOutcome::Pass(TEXT("real contact-bound day commands stopped at 2 February 2043 06:00 with the original grant"));
    }));
    P406Out.Add(P406ScenarioAction(TEXT("p406_automatic_decision_screen"), [](UPWAcceptanceSubsystem& P406Sub, FStepRuntime&)
    {
        const auto P406Panel = P406ScenarioPanel(P406Sub);
        const auto* P406Life = UPWLaneLifeSubsystem::Get(P406Sub.GetPlayWorld());
        if (!P406Panel.IsValid() || P406Life == nullptr) return FStepOutcome::Fail(TEXT("the decision screen did not open automatically"));
        const auto& P406Model = P406Panel->P406GetModel();
        if (P406Model.Screen != P406Exam::EScreen::Decision || P406Model.Actions.Num() != 3 ||
            P406Model.World.CanSit != P406Life->GetSim().ExaminationWorldView().CanSit)
            return FStepOutcome::Fail(TEXT("the automatic screen does not display the three live participation choices"));
        const auto P406Before = P406Life->GetNow();
        P406Panel->P406Refresh();
        if (P406Before != P406Life->GetNow()) return FStepOutcome::Fail(TEXT("reading advanced the clock"));
        return FStepOutcome::Pass(TEXT("automatic decision shows Sit, Defer, Decline and live Sit availability; reading costs no time"));
    }));
    P406Out.Add(PWAcceptance::MakeShotStep(TEXT("p406_shot_exam_decision")));
}

void P406ScenarioResult(TArray<FStepDef>& P406Out, PerfectWorld::TowerParticipation P406Expected)
{
    P406Out.Add(P406ScenarioAction(TEXT("p406_result_and_next_step"), [P406Expected](UPWAcceptanceSubsystem& P406Sub, FStepRuntime&)
    {
        const auto P406Panel = P406ScenarioPanel(P406Sub);
        const auto* P406Life = UPWLaneLifeSubsystem::Get(P406Sub.GetPlayWorld());
        if (!P406Panel.IsValid() || P406Life == nullptr) return FStepOutcome::Fail(TEXT("the result panel is absent"));
        const auto P406Finish = P406Life->GetSim().ExaminationResult();
        const auto& P406Model = P406Panel->P406GetModel();
        if (!P406Finish.Ok() || P406Finish.Participation != P406Expected || P406Model.Screen != P406Exam::EScreen::Result ||
            P406Model.Lines.Num() == 0 || P406Model.Offers.Num() == 0 || P406Model.World.CanWithdraw)
            return FStepOutcome::Fail(TEXT("stored participation, visible result and next-step offers disagree"));
        return FStepOutcome::Pass(TEXT("the derived result shows the outcome, reasons and at least one qualification next step"));
    }));
    P406Out.Add(PWAcceptance::MakeShotStep(TEXT("p406_shot_exam_result")));
    P406Out.Add(P406ScenarioAction(TEXT("p406_resume_ordinary_time"), [](UPWAcceptanceSubsystem& P406Sub, FStepRuntime& P406Rt)
    {
        auto* P406Host = UPWSchoolPresenter::Get(P406Sub.GetPlayWorld());
        auto* P406Life = UPWLaneLifeSubsystem::Get(P406Sub.GetPlayWorld());
        if (P406Host == nullptr || P406Life == nullptr) return FStepOutcome::Fail(TEXT("the live host is absent"));
        if (P406Rt.Phase == 0) { P406Host->Close(); P406Rt.NextPhase(); return FStepOutcome::Wait(); }
        if (P406Exam::P406PauseEntryVisible(P406Sub.GetPlayWorld()))
            return FStepOutcome::Fail(TEXT("the Examination pause entry remained after the session ended"));
        const auto P406Reply = P406Life->SubmitAwakeningCommand({PerfectWorld::CommandSource::Player, PerfectWorld::AdvanceTime{1}});
        return P406Reply.Accepted ? FStepOutcome::Pass(TEXT("result closed; ordinary time advances after the examination"))
            : FStepOutcome::Fail(PWLaneLife::ToFString(P406Reply.Why.Code));
    }));
}
}

namespace PWAcceptance
{
void P406AppendExaminationSteps(TArray<FStepDef>& Out)
{
    P406ScenarioMorning(Out);
    Out.Add(P406ScenarioPress(TEXT("p406_decline_once"), PerfectWorld::TowerSessionAction::Decline));
    P406ScenarioResult(Out, PerfectWorld::TowerParticipation::Declined);
}

void P406AppendExaminationWithdrawSteps(TArray<FStepDef>& Out)
{
    P406ScenarioMorning(Out);
    Out.Add(P406ScenarioAction(TEXT("p406b_pause_pending_decision"), [](UPWAcceptanceSubsystem& P406BSub, FStepRuntime&)
    {
        const auto P406BPanel = P406ScenarioPanel(P406BSub);
        auto* P406BWorld = P406BSub.GetPlayWorld();
        const auto* P406BPlayer = P406BWorld != nullptr ? P406BWorld->GetFirstPlayerController() : nullptr;
        auto* P406BHud = P406BPlayer != nullptr ? Cast<APerfectWorldFrontEndHUD>(P406BPlayer->GetHUD()) : nullptr;
        auto* P406BLife = UPWLaneLifeSubsystem::Get(P406BWorld);
        auto* P406BHost = UPWSchoolPresenter::Get(P406BWorld);
        if (!P406BPanel.IsValid() || P406BHud == nullptr || P406BLife == nullptr || P406BHost == nullptr)
            return FStepOutcome::Fail(TEXT("the examination or pause host is absent"));
        if (!P406BPanel->P406GetModel().World.CanSit)
            return FStepOutcome::Fail(TEXT("real runner content did not enable Sit"));
        const auto P406BBefore = P406BLife->GetNow();
        P406BPanel->OnPreviewKeyDown(FGeometry(), FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
        if (!P406BHud->IsPaused() || P406BHost->IsOpen())
            return FStepOutcome::Fail(TEXT("Escape did not hand the required decision to pause"));
        if (!P406Exam::P406PresentRequired(P406BWorld) || P406BHost->IsOpen() ||
            !P406BLife->GetSim().ExaminationWorldView().DecisionRequired || P406BLife->GetNow() != P406BBefore)
            return FStepOutcome::Fail(TEXT("automatic presentation displaced pause or changed the pending decision"));
        P406BHud->ResumeGame();
        P406Exam::P406PresentRequired(P406BWorld);
        if (!P406ScenarioPanel(P406BSub).IsValid() || P406BLife->GetNow() != P406BBefore)
            return FStepOutcome::Fail(TEXT("Resume did not restore the same required decision"));
        return FStepOutcome::Pass(TEXT("Escape exposes pause; presentation leaves it alone; Resume returns to the required decision at the same instant"));
    }));
    Out.Add(P406ScenarioPress(TEXT("p406_sit"), PerfectWorld::TowerSessionAction::Enter));
    Out.Add(P406ScenarioPress(TEXT("p406_open_route"), PerfectWorld::TowerSessionAction::OpenRoute));
    Out.Add(P406ScenarioPress(TEXT("p406_one_choice"), PerfectWorld::TowerSessionAction::Choose));
    Out.Add(MakeShotStep(TEXT("p406_shot_after_choice")));
    Out.Add(P406ScenarioPress(TEXT("p406_withdraw"), PerfectWorld::TowerSessionAction::Withdraw));
    P406ScenarioResult(Out, PerfectWorld::TowerParticipation::Withdrawn);
}
}
