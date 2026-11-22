#include "PerfectWorldAcceptanceExamination.h"

#include "Engine/World.h"
#include "Layout/Children.h"
#include "Misc/FileHelper.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldExaminationBridge.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldSchoolGate.h"
#include "PerfectWorldSchoolPresenter.h"
#include "UI/SPWExaminationPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "WorldHash.h"
#include "WorldSave.h"

namespace
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;
using PerfectWorld::TowerParticipation;
using PerfectWorld::TowerSessionAction;

struct FP412Run
{
    bool bContinue = false;
    PerfectWorld::GameInstant HallDeadline = 0;
    PerfectWorld::GameInstant NextSitting = 0;
    PerfectWorld::WorldSaveData Saved;
};

FStepDef P412Action(const TCHAR* Name, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run)
{
    FStepDef Step;
    Step.Name = Name;
    Step.TimeoutSeconds = 30.0;
    Step.Run = MoveTemp(Run);
    return Step;
}

TSharedPtr<SPWExaminationPanel> P412Panel(UPWAcceptanceSubsystem& Sub)
{
    const auto* Host = UPWSchoolPresenter::Get(Sub.GetPlayWorld());
    return Host != nullptr && Host->GetKind() == EPWSchoolPanel::P406Examination
        ? StaticCastSharedPtr<SPWExaminationPanel>(Host->GetPanel()) : nullptr;
}

FString P412Text(const TSharedRef<SWidget>& Root)
{
    if (!Root->GetVisibility().IsVisible()) return {};
    FString Words;
    if (Root->GetType() == FName(TEXT("STextBlock")))
        Words = static_cast<const STextBlock&>(Root.Get()).GetText().ToString() + TEXT("\n");
    FChildren* Children = Root->GetChildren();
    for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        Words += P412Text(Children->GetChildAt(Index));
    return Words;
}

bool P412Press(UPWAcceptanceSubsystem& Sub, TowerSessionAction Kind, FString& Why)
{
    const auto Panel = P412Panel(Sub);
    if (!Panel.IsValid()) { Why = TEXT("the live examination panel is absent"); return false; }
    const auto Rows = Panel->P406GetModel().Actions;
    for (int32 Index = 0; Index < Rows.Num(); ++Index)
    {
        if (Rows[Index].Kind != Kind || Rows[Index].bP407UFollowup) continue;
        if (!Rows[Index].bEnabled || !Panel->P406TriggerAction(Index))
        {
            Why = PWLaneLife::ToFString(Panel->P406GetModel().World.SitRefusal) + TEXT(": ") + Panel->P406GetNotice().ToString();
            return false;
        }
        return true;
    }
    Why = TEXT("the requested live participation action is absent");
    return false;
}

FStepDef P412PressStep(const TCHAR* Name, TowerSessionAction Kind)
{
    return P412Action(Name, [Kind](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        FString Why;
        return P412Press(Sub, Kind, Why) ? FStepOutcome::Pass(TEXT("live panel submitted the participation command")) : FStepOutcome::Fail(Why);
    });
}

FStepDef P412Phase(FStepDef Step, const TSharedRef<FP412Run>& Run, bool bContinue)
{
    // Keep screenshot names: their factories also capture the evidence filename.
    auto Inner = MoveTemp(Step.Run);
    Step.Run = [Inner = MoveTemp(Inner), Run, bContinue](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    { return Run->bContinue == bContinue ? Inner(Sub, Rt) : FStepOutcome::Pass(TEXT("belongs to the other process phase")); };
    return Step;
}

void P412Morning(TArray<FStepDef>& Out, const TCHAR* Prefix)
{
    TArray<FStepDef> Morning;
    PWAcceptance::P412AppendExaminationMorningSteps(Morning);
    for (auto& Step : Morning)
    {
        // Replace screenshot factories too, so both new lives have distinct evidence names.
        const FString Name = FString(Prefix) + Step.Name;
        if (Step.Name.StartsWith(TEXT("shot_")) || Step.Name.Contains(TEXT("_shot_")))
            Step = PWAcceptance::MakeShotStep(Name);
        else Step.Name = Name;
        Out.Add(MoveTemp(Step));
    }
}

FString P412Date(PerfectWorld::GameInstant At)
{
    const auto Date = PerfectWorld::GameClock::ToCalendar(At);
    return FString::Printf(TEXT("%04d-%02d-%02d %02d:%02d"), Date.Year, Date.Month, Date.Day, Date.Hour, Date.Minute);
}

bool P412ReadSlot(UPWAcceptanceSubsystem& Sub, PerfectWorld::WorldSaveData& Out)
{
    const int32 Slot = UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot();
    if (Slot < 0 || (Sub.GetArgs().Slot >= 0 && Sub.GetArgs().Slot != Slot)) return false;
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotPath(Slot))) return false;
    const auto Result = PerfectWorld::DeserializeWorldSave(TCHAR_TO_UTF8(*Text), Out);
    return Result.Succeeded() && Result.SourceVersion == PerfectWorld::WorldSaveSchemaVersion && Result.HashVerified;
}

bool P412BirthMatches(const UPWLaneLifeSubsystem& Life, const PerfectWorld::DateOfBirth& Birth)
{
    const auto* Bound = Life.GetSim().ExaminationContent().P409Birth;
    return Bound != nullptr && *Bound == Birth;
}

FStepDef P412Arrival(const TCHAR* Name, int64 Minutes)
{
    return P412Action(Name, [Minutes](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* World = Sub.GetPlayWorld();
        auto* Life = UPWLaneLifeSubsystem::Get(World);
        auto* Host = UPWSchoolPresenter::Get(World);
        if (World == nullptr || Life == nullptr || Host == nullptr) return FStepOutcome::Fail(TEXT("arrival owners are absent"));
        const auto Opening = Life->GetSim().ExaminationWorldView().Sittings.OpensAt;
        if (Life->GetNow() != Opening || Life->GetWorldState().Examination.Participation != TowerParticipation::Unset)
            return FStepOutcome::Fail(TEXT("arrival requires a fresh undecided examination morning"));
        const auto Before = PerfectWorld::HashWorldState(Life->GetWorldState());
        const auto Stopped = Life->SubmitAwakeningCommand({PerfectWorld::CommandSource::Player, PerfectWorld::AdvanceTime{Minutes}});
        if (Stopped.Accepted || Stopped.Why.Code != "exam.decision_required" || Before != PerfectWorld::HashWorldState(Life->GetWorldState()))
            return FStepOutcome::Fail(TEXT("ordinary time did not preserve the required 06:00 decision"));
        // Declared arrival seam: this existing helper submits time commands on a school-bound copy, then adopts/rebinds it.
        // Its copy has no countdown routing, so it can pass the required decision. This does NOT prove player waiting.
        Host->Close();
        FString Why;
        if (!PWSchool::ScAdvanceLaneClock(*World, Opening + Minutes, Why)) return FStepOutcome::Fail(Why);
        if (Life->GetNow() != Opening + Minutes || !Host->P406OpenExamination() || !P412Panel(Sub).IsValid())
            return FStepOutcome::Fail(TEXT("prepared arrival did not reach its instant and open the real panel"));
        return FStepOutcome::Pass(FString::Printf(TEXT("declared ScAdvanceLaneClock arrival seam: %lld minutes after 06:00; live wait was refused"), static_cast<long long>(Minutes)));
    });
}
} // namespace

namespace PWAcceptance
{
void P412AppendExaminationLateSteps(TArray<FStepDef>& Out)
{
    const auto Run = MakeShared<FP412Run>();
    Out.Add(P412Action(TEXT("p412_late_new_lives_only"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        return Sub.GetArgs().Start == TEXT("new") ? FStepOutcome::Pass(TEXT("two fresh lives; explicit new start")) :
            FStepOutcome::Fail(TEXT("exam_late_round9 requires -PWAcceptanceStart=new in private acceptance saves"));
    }));
    P412Morning(Out, TEXT("p412_late_"));
    Out.Add(P412Action(TEXT("p412_original_hall_deadline"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (Life == nullptr) return FStepOutcome::Fail(TEXT("live owner absent"));
        const auto View = Life->GetSim().ExaminationWorldView();
        Run->HallDeadline = View.Sittings.OpensAt + View.Sittings.AllowanceMinutes;
        return FStepOutcome::Pass(TEXT("captured opening plus the view's on-time allowance as the common hall deadline"));
    }));
    Out.Add(P412Arrival(TEXT("p412_prepared_arrival_0630"), 30));
    Out.Add(P412PressStep(TEXT("p412_late_live_sit"), TowerSessionAction::Enter));
    Out.Add(P412Action(TEXT("p412_late_words_and_allowance"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto Panel = P412Panel(Sub);
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!Panel.IsValid() || Life == nullptr) return FStepOutcome::Fail(TEXT("entered panel absent"));
        const auto View = Life->GetSim().ExaminationWorldView();
        const auto& Display = Panel->P406GetModel().World.Sittings;
        // No allowance subtraction here: both the displayed value and deadline evidence use the portable projection.
        const FString Words = FString::Printf(TEXT("Late entry: %lld minutes late | Reduced outside allowance: %lld minutes"),
            static_cast<long long>(View.Sittings.LateMinutes), static_cast<long long>(View.Sittings.AllowanceMinutes));
        if (!View.CanWithdraw || View.Sittings.LateMinutes != 30 || Display.LateMinutes != View.Sittings.LateMinutes ||
            Display.AllowanceMinutes != View.Sittings.AllowanceMinutes || !P412Text(Panel.ToSharedRef()).Contains(Words) ||
            Life->GetNow() + View.Sittings.AllowanceMinutes != Run->HallDeadline)
            return FStepOutcome::Fail(TEXT("late words, projected allowance or original hall deadline disagree"));
        return FStepOutcome::Pass(Words);
    }));
    Out.Add(MakeShotStep(TEXT("p412_shot_late_allowance")));
    Out.Add(P412Action(TEXT("p412_late_clear_one_floor"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        const auto Panel = P412Panel(Sub);
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!Panel.IsValid() || Life == nullptr) return FStepOutcome::Fail(TEXT("live floor absent"));
        const auto& State = Life->GetWorldState().Examination;
        const auto View = Life->GetSim().ExaminationWorldView();
        if (State.Participation != TowerParticipation::Entered || Life->GetNow() + View.Sittings.AllowanceMinutes != Run->HallDeadline ||
            View.Sittings.LateMinutes != 30) return FStepOutcome::Fail(TEXT("late session ended or its hall deadline moved"));
        if (State.Attempts.size() == 1 && !State.Attempts.front().Defeated && !State.Active && View.Session.NextFloor == 2)
            return FStepOutcome::Pass(TEXT("one live floor cleared; session Entered at 06:30; common hall deadline unchanged"));
        if (!State.Attempts.empty() || Rt.Phase > 16) return FStepOutcome::Fail(TEXT("one-floor action budget exceeded"));
        const auto Rows = Panel->P406GetModel().Actions;
        int32 Pick = INDEX_NONE;
        if (!State.Active)
        {
            for (int32 Index = 0; Index < Rows.Num(); ++Index)
                if (Rows[Index].bEnabled && Rows[Index].Kind == TowerSessionAction::OpenRoute && Rows[Index].Method != PerfectWorld::TowerMethod::Combat)
                { Pick = Index; break; }
        }
        else
        {
            // Choose from disclosed costs only; no scoring answers or fabricated performance result.
            int32 Strain = MAX_int32;
            for (const auto& Choice : View.Session.Choices)
                for (int32 Index = 0; Index < Rows.Num(); ++Index)
                    if (Rows[Index].bEnabled && Rows[Index].Kind == TowerSessionAction::Choose &&
                        Rows[Index].ChoiceId == PWLaneLife::ToFString(Choice.Id) && Choice.Strain < Strain)
                    { Pick = Index; Strain = Choice.Strain; }
        }
        if (Pick == INDEX_NONE || !Panel->P406TriggerAction(Pick)) return FStepOutcome::Fail(TEXT("no accepted live route/choice: ") + Panel->P406GetNotice().ToString());
        Rt.NextPhase();
        return FStepOutcome::Wait();
    }));
    Out.Add(MakeShotStep(TEXT("p412_shot_late_floor_cleared")));
    Out.Add(P412Action(TEXT("p412_finish_first_life"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto Panel = P412Panel(Sub);
        auto* Host = UPWSchoolPresenter::Get(Sub.GetPlayWorld());
        auto* Hud = Sub.GetHud();
        if (!Panel.IsValid() || Host == nullptr || Hud == nullptr || !Panel->P406TriggerWithdraw())
            return FStepOutcome::Fail(TEXT("first life could not leave its live examination"));
        Host->Close();
        Hud->ReturnToTitle();
        return Hud->IsMenuOpen() ? FStepOutcome::Pass(TEXT("withdrew through the panel; returned to title for a fresh missed-arrival life")) :
            FStepOutcome::Fail(TEXT("title did not open for the second new life"));
    }));
    P412Morning(Out, TEXT("p412_missed_"));
    Out.Add(P412Arrival(TEXT("p412_prepared_arrival_0701"), 61));
    Out.Add(P412Action(TEXT("p412_missed_words_and_next_sitting"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto Panel = P412Panel(Sub);
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!Panel.IsValid() || Life == nullptr) return FStepOutcome::Fail(TEXT("missed-arrival panel absent"));
        const auto Before = PerfectWorld::HashWorldState(Life->GetWorldState());
        // A disabled Sit button cannot submit: probe the same live bridge command path, then read the actual hosted panel.
        P406Exam::FAction Sit;
        Sit.Kind = TowerSessionAction::Enter;
        const auto Refused = P406Exam::P406MakeLiveWorld(*Sub.GetPlayWorld())->Submit(Sit);
        Panel->P406Refresh();
        const auto& View = Panel->P406GetModel().World;
        const auto Words = P412Text(Panel.ToSharedRef());
        // P413 (owner 2026-11-22): missing the window uses the life's one sitting; no later sitting is offered.
        const auto RefusalWords = P406Exam::P406RefusalText(TEXT("exam.sitting_used")).ToString();
        if (Refused.Accepted || Refused.Why.Code != "exam.sitting_used" || View.CanSit || View.SitRefusal != "exam.sitting_used" ||
            RefusalWords.IsEmpty() || Words.Contains(TEXT("exam.sitting_used")) || !Words.Contains(RefusalWords) ||
            View.Sittings.HasNextSitting || Words.Contains(TEXT("Next annual sitting")) ||
            Before != PerfectWorld::HashWorldState(Life->GetWorldState()) || Life->GetWorldState().Examination.Participation != TowerParticipation::Unset)
            return FStepOutcome::Fail(TEXT("07:01 refusal, readable panel text, absent next sitting or unchanged state disagree"));
        return FStepOutcome::Pass(TEXT("07:01 live Enter refused exam.sitting_used without mutation; panel explains the one sitting and shows no later sitting"));
    }));
    Out.Add(MakeShotStep(TEXT("p412_shot_missed_and_next_sitting")));
}

void P412AppendExaminationRetakeSteps(TArray<FStepDef>& Out)
{
    const auto Run = MakeShared<FP412Run>();
    Out.Add(MakeWorldReadyStep());
    Out.Add(P412Action(TEXT("p412_retake_process_phase"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (Sub.GetArgs().Start != TEXT("new") && Sub.GetArgs().Start != TEXT("continue"))
            return FStepOutcome::Fail(TEXT("exam_retake_round9 requires --restart-pair: explicit new then fresh-process continue"));
        Run->bContinue = Sub.GetArgs().Start == TEXT("continue");
        if (Run->bContinue && (!P412ReadSlot(Sub, Run->Saved) || Run->Saved.World.ExaminationEarlier.size() != 1 ||
            Run->Saved.World.ExaminationEarlier.front().Participation != TowerParticipation::Deferred ||
            Run->Saved.World.Examination.Participation != TowerParticipation::Entered || !Run->Saved.World.Awakening.Granted))
            return FStepOutcome::Fail(TEXT("Continue needs the original current-schema deferred-then-entered annual-retake slot"));
        return FStepOutcome::Pass(Run->bContinue ? TEXT("original retake save captured before title Continue") : TEXT("new-life retake phase"));
    }));
    TArray<FStepDef> New;
    P412Morning(New, TEXT("p412_retake_"));
    New.Add(P412PressStep(TEXT("p412_retake_live_defer"), TowerSessionAction::Defer));
    New.Add(P412Action(TEXT("p412_retake_live_year_to_stop"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        const auto Panel = P412Panel(Sub);
        if (Life == nullptr || !Panel.IsValid()) return FStepOutcome::Fail(TEXT("deferred live owners absent"));
        if (Rt.Phase == 0)
        {
            const auto View = Life->GetSim().ExaminationWorldView();
            if (Life->GetWorldState().Examination.Participation != TowerParticipation::Deferred || !View.Sittings.HasNextSitting ||
                P412Date(View.Sittings.NextSittingAt) != TEXT("2044-02-02 06:00")) return FStepOutcome::Fail(TEXT("deferral has no next-year sitting"));
            Run->NextSitting = View.Sittings.NextSittingAt;
        }
        if (Life->GetNow() > Run->NextSitting || Rt.Phase >= 128) return FStepOutcome::Fail(TEXT("annual live continuation exceeded its target or budget"));
        if (PerfectWorld::IsWorldQuiescent(Life->GetWorldState()) && Life->GetNow() == Run->NextSitting)
        {
            Panel->P406Refresh();
            const auto View = Life->GetSim().ExaminationWorldView();
            const auto Before = PerfectWorld::HashWorldState(Life->GetWorldState());
            const auto Stopped = Life->SubmitAwakeningCommand({PerfectWorld::CommandSource::Player, PerfectWorld::AdvanceTime{1}});
            if (Stopped.Accepted || Stopped.Why.Code != "exam.decision_required" || Before != PerfectWorld::HashWorldState(Life->GetWorldState()) ||
                !View.DecisionRequired || !View.CanSit || View.Sittings.SitTarget != 1 || View.Sittings.Sitting != 0 ||
                Panel->P406GetModel().Screen != P406Exam::EScreen::Decision ||
                !P412Text(Panel.ToSharedRef()).Contains(TEXT("Sitting opens: 2044-02-02 06:00")))
                return FStepOutcome::Fail(TEXT("next-year required decision, target sitting or live screen disagree"));
            return FStepOutcome::Pass(TEXT("bound live time.advance/time.settle traversed the year; 2044-02-02 06:00 refuses exam.decision_required; SitTarget=1 (stored Sitting remains 0 until Sit)"));
        }
        // P38/P39 continuation pattern, one command per frame. Ask past the opening to exercise the actual stop.
        const bool Settled = PerfectWorld::IsWorldQuiescent(Life->GetWorldState());
        const auto Before = Life->GetNow();
        PerfectWorld::Command Command{PerfectWorld::CommandSource::System, PerfectWorld::SettleDueEvents{}};
        if (Settled) Command = {PerfectWorld::CommandSource::Player, PerfectWorld::AdvanceTime{Run->NextSitting + 1 - Before}};
        const auto Reply = Life->SubmitAwakeningCommand(Command);
        if (!Reply.Accepted) return FStepOutcome::Fail(PWLaneLife::ToFString(Reply.Why.Code) + TEXT(": ") + PWLaneLife::ToFString(Reply.Why.Message));
        if (Settled && Life->GetNow() <= Before) return FStepOutcome::Fail(TEXT("annual advance made no clock progress"));
        Rt.Note = TEXT("bound live annual advance at ") + P412Date(Life->GetNow());
        Rt.NextPhase();
        return FStepOutcome::Wait();
    }));
    New.Add(MakeShotStep(TEXT("p412_shot_next_year_decision")));
    New.Add(P412PressStep(TEXT("p412_retake_live_sit"), TowerSessionAction::Enter));
    New.Add(P412Action(TEXT("p412_retake_save_sitting_and_birth"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        auto* Hud = Sub.GetHud();
        if (Life == nullptr || Hud == nullptr) return FStepOutcome::Fail(TEXT("save owners absent"));
        const auto View = Life->GetSim().ExaminationWorldView();
        if (View.Sittings.Sitting != 1 || Life->GetNow() != Run->NextSitting || !View.CanWithdraw ||
            Life->GetWorldState().Examination.Participation != TowerParticipation::Entered)
            return FStepOutcome::Fail(TEXT("live Sit did not roll over into entered sitting 1"));
        bool Saved = false;
        const auto Notice = Hud->SaveActiveGame(Saved);
        if (!Saved || !P412ReadSlot(Sub, Run->Saved)) return FStepOutcome::Fail(TEXT("retake save could not be read: ") + Notice.ToString());
        const auto& Birth = Run->Saved.Creation.Identity.Birth;
        if (PerfectWorld::AgeOnCampaignStart(Birth) != 16 || !P412BirthMatches(*Life, Birth) ||
            PerfectWorld::HashWorldState(Life->GetWorldState()) != PerfectWorld::HashWorldState(Run->Saved.World))
            return FStepOutcome::Fail(TEXT("retake snapshot or actual sixteen-year-old creation birth binding differs"));
        return FStepOutcome::Pass(TEXT("live save authenticated: entered sitting 1, deferred sitting 0, original grant and actual creation birth bound; exit for a fresh process"));
    }));
    for (auto& Step : New) Out.Add(P412Phase(MoveTemp(Step), Run, false));
    Out.Add(P412Phase(MakeEnterWorldStep(), Run, true));
    Out.Add(P412Phase(P412Action(TEXT("p412_retake_continued_sitting_and_birth"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        auto* Host = UPWSchoolPresenter::Get(Sub.GetPlayWorld());
        if (Life == nullptr || Host == nullptr) return FStepOutcome::Fail(TEXT("continued owners absent"));
        const auto& State = Life->GetWorldState();
        const auto& Saved = Run->Saved.World;
        const auto View = Life->GetSim().ExaminationWorldView();
        if (State.WorldSeed != Saved.WorldSeed || State.Examination != Saved.Examination || State.ExaminationEarlier != Saved.ExaminationEarlier ||
            State.Clock.Now() != Saved.Clock.Now() || !State.Awakening.Granted || !Saved.Awakening.Granted ||
            State.Awakening.Granted->At != Saved.Awakening.Granted->At || State.Awakening.Granted->Result != Saved.Awakening.Granted->Result ||
            View.Sittings.Sitting != 1 || !View.CanWithdraw || View.SitRefusal == "exam.age_limit" ||
            !P412BirthMatches(*Life, Run->Saved.Creation.Identity.Birth))
            return FStepOutcome::Fail(TEXT("fresh-process Continue lost annual history, standing clock, grant or actual creation birth binding"));
        if (!P412Panel(Sub).IsValid() && !Host->P406OpenExamination()) return FStepOutcome::Fail(TEXT("continued examination cannot reopen"));
        const auto Panel = P412Panel(Sub);
        if (!Panel.IsValid() || Panel->P406GetModel().World.Sittings.Sitting != 1 ||
            P412Text(Panel.ToSharedRef()).Contains(P406Exam::P406RefusalText(TEXT("exam.age_limit")).ToString()))
            return FStepOutcome::Fail(TEXT("continued live panel lost sitting 1 or shows an age-limit refusal"));
        return FStepOutcome::Pass(TEXT("fresh-process title Continue retained sitting 1, deferred history, grant and clock; non-null birth binding equals the original saved birth; no age-limit refusal"));
    }), Run, true));
    Out.Add(P412Phase(MakeShotStep(TEXT("p412_shot_continued_annual_retake")), Run, true));
}
} // namespace PWAcceptance
