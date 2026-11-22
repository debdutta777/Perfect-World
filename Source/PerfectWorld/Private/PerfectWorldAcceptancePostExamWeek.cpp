#include "PerfectWorldAcceptanceExamination.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Misc/FileHelper.h"
#include "PerfectWorldAcceptanceSchoolR7Steps.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldSchoolLive.h"
#include "PerfectWorldSchoolPresenter.h"
#include "UI/SPWExaminationPanel.h"
#include "WorldExamFollowup.h"
#include "WorldHash.h"
#include "WorldSave.h"

namespace
{
using namespace PerfectWorld;
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

struct FP506Run
{
    bool Continue = false;
    bool TaskToday = false;
    bool Missed = false;
    std::string Outcome;
    std::string TaskId;
    GameInstant WeekStart = 0;
    WorldSaveData Saved;
    P407UFollowupCommand Resolved;
};

FStepDef P506Action(const FString& Name, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run)
{
    FStepDef Step;
    Step.Name = Name;
    Step.TimeoutSeconds = 40.0;
    Step.Run = MoveTemp(Run);
    return Step;
}

TSharedPtr<SPWExaminationPanel> P506Panel(UPWAcceptanceSubsystem& Sub)
{
    const auto* Host = UPWSchoolPresenter::Get(Sub.GetPlayWorld());
    return Host != nullptr && Host->GetKind() == EPWSchoolPanel::P406Examination
        ? StaticCastSharedPtr<SPWExaminationPanel>(Host->GetPanel()) : nullptr;
}

std::optional<P407UFollowupView> P506Task(const WorldSim& Sim, const std::string& Id)
{
    for (const auto& Row : Sim.P407UFollowups()) if (Row.Action.Id == Id) return Row;
    return {};
}

bool P506ReadSlot(UPWAcceptanceSubsystem& Sub, WorldSaveData& Out)
{
    const int32 Slot = UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot();
    FString Text;
    if (Slot < 0 || (Sub.GetArgs().Slot >= 0 && Slot != Sub.GetArgs().Slot) ||
        !FFileHelper::LoadFileToString(Text, *UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotPath(Slot))) return false;
    const auto Result = DeserializeWorldSave(TCHAR_TO_UTF8(*Text), Out);
    return Result.Succeeded() && Result.SourceVersion == WorldSaveSchemaVersion && Result.HashVerified;
}

FStepDef P506Phase(FStepDef Step, const TSharedRef<FP506Run>& Run, bool Continue, bool TaskOnly = false)
{
    auto Inner = MoveTemp(Step.Run);
    Step.Run = [Inner = MoveTemp(Inner), Run, Continue, TaskOnly](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        return Run->Continue == Continue && (!TaskOnly || Run->TaskToday) ? Inner(Sub, Rt) :
            FStepOutcome::Pass(TEXT("belongs to another process phase or appointment day"));
    };
    return Step;
}

// One real time command per frame; due events and partial advances retain the original target.
FStepOutcome P506Move(UPWLaneLifeSubsystem& Life, GameInstant Target, FStepRuntime& Rt)
{
    if (++Rt.LastProbe > 128) return FStepOutcome::Fail(TEXT("bounded ordinary-time command budget exhausted"));
    const bool Settled = IsWorldQuiescent(Life.GetWorldState());
    if (Settled && Life.GetNow() == Target) return FStepOutcome::Pass(TEXT("reached the disclosed ordinary-time target"));
    if (Life.GetNow() > Target) return FStepOutcome::Fail(TEXT("ordinary time passed the target"));
    Command Cmd{CommandSource::System, SettleDueEvents{}};
    if (Settled) Cmd = {CommandSource::Player, AdvanceTime{Target - Life.GetNow()}};
    const auto Reply = Life.SubmitSchoolCommand(Cmd);
    return Reply.Accepted ? FStepOutcome::Wait() : FStepOutcome::Fail(PWLaneLife::ToFString(Reply.Why.Code));
}

bool P506Press(UPWAcceptanceSubsystem& Sub, P407UFollowupAction Kind, const std::string& Id, bool Wait = false)
{
    const auto Panel = P506Panel(Sub);
    if (!Panel.IsValid()) return false;
    Panel->P406Refresh();
    const auto Actions = Panel->P406GetModel().Actions;
    for (int32 Index = 0; Index < Actions.Num(); ++Index)
        if (Actions[Index].bP407UFollowup && Actions[Index].bP407UWait == Wait &&
            Actions[Index].P407UCommand.Kind == Kind && Actions[Index].P407UCommand.ActionId == Id)
        {
            if (!Actions[Index].bEnabled) return false;
            Panel->P406SelectFocus(Index);
            const auto Key = Kind == P407UFollowupAction::P502Resume || Kind == P407UFollowupAction::P502Resolve
                ? EKeys::Gamepad_FaceButton_Bottom : EKeys::Enter;
            Panel->OnPreviewKeyDown(FGeometry(), FKeyEvent(Key, FModifierKeysState(), 0, false, 0, 0));
            return Panel->P406GetNotice().IsEmpty();
        }
    return false;
}

FStepDef P506Climb(const TSharedRef<FP506Run>& Run)
{
    return P506Action(TEXT("p506_live_result"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        const auto Panel = P506Panel(Sub);
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!Panel.IsValid() || Life == nullptr) return FStepOutcome::Fail(TEXT("live examination is absent"));
        if (Rt.Phase > 64) return FStepOutcome::Fail(TEXT("bounded live examination action budget exhausted"));
        const auto Finish = Life->GetSim().ExaminationResult();
        if (Finish.Ok())
        {
            if (Finish.Outcome != Run->Outcome) return FStepOutcome::Fail(TEXT("live outcome was ") + PWLaneLife::ToFString(Finish.Outcome));
            for (const auto& Attempt : Finish.Attempts)
                if (Attempt.Method == TowerMethod::Combat) return FStepOutcome::Fail(TEXT("the scripted week requires an all-non-combat result"));
            Run->WeekStart = GameClock::StartOfDay(Life->GetNow());
            for (const auto& Row : Life->GetSim().P407UFollowups())
                if (!Row.P502Choices.empty()) { Run->TaskId = Row.Action.Id; break; }
            return !Run->TaskId.empty() ? FStepOutcome::Pass(TEXT("core classified the result and supplied its consequential task")) :
                FStepOutcome::Fail(TEXT("the result has no consequential task"));
        }
        const auto Model = Panel->P406GetModel();
        if (Run->Outcome == "withdrawn" && Model.World.CanWithdraw && Model.Session.NextFloor == 2)
            return Panel->P406TriggerWithdraw() ? FStepOutcome::Wait() : FStepOutcome::Fail(Panel->P406GetNotice().ToString());
        int32 Pick = INDEX_NONE;
        if (Model.Screen == P406Exam::EScreen::Decision)
        {
            const auto Kind = Run->Outcome == "deferred" ? TowerSessionAction::Defer :
                Run->Outcome == "declined" ? TowerSessionAction::Decline : TowerSessionAction::Enter;
            for (int32 Index = 0; Index < Model.Actions.Num(); ++Index)
                if (Model.Actions[Index].Kind == Kind && Model.Actions[Index].bEnabled) { Pick = Index; break; }
        }
        else if (Model.Session.Choices.empty())
        {
            const char* Route = Model.Session.NextFloor == 1 ? "tower.route.pace" :
                Model.Session.NextFloor == 2 ? "tower.route.changed_rule" : "tower.route.supply";
            for (int32 Index = 0; Index < Model.Actions.Num(); ++Index)
                if (Model.Actions[Index].Kind == TowerSessionAction::OpenRoute && Model.Actions[Index].Method != TowerMethod::Combat &&
                    Model.Actions[Index].RouteId == PWLaneLife::ToFString(Route)) { Pick = Index; break; }
        }
        else
        {
            // Scripted authored choices, never injected scores. Recovery beats have a single disclosed action.
            // P5-15 cohort: strong takes opt1 everywhere (6000, rank 1); poor takes opt3 and is ejected. Plain opt2
            // everywhere (5469, decile 6) and a floor-3 ejection (3500) are both poor now, so middle takes, on every
            // floor, opt2 on beat 1, the opt3 mistake on beat 2 and opt2 on beat 3. The mistake's strain 4 never
            // exceeds the reserve (4 or more, reset per route), so each floor survives it: 5547, rank 453, decile 5.
            const auto Suffix = Run->Outcome == "poor" ? ".opt3.label" :
                Run->Outcome == "middle" ? (Model.Session.Beat == 1 ? ".opt3.label" : ".opt2.label") : ".opt1.label";
            for (const auto& Choice : Model.Session.Choices)
                if (Model.Session.Choices.size() == 1 || Choice.TextKey.ends_with(Suffix))
                    for (int32 Index = 0; Index < Model.Actions.Num(); ++Index)
                        if (Model.Actions[Index].Kind == TowerSessionAction::Choose && Model.Actions[Index].ChoiceId == PWLaneLife::ToFString(Choice.Id))
                        { Pick = Index; break; }
        }
        if (Pick == INDEX_NONE || !Panel->P406TriggerAction(Pick)) return FStepOutcome::Fail(TEXT("scripted live action unavailable: ") + Panel->P406GetNotice().ToString());
        Rt.NextPhase();
        return FStepOutcome::Wait();
    });
}

FStepDef P506SchoolMorning(const FString& Name)
{
    return P506Action(Name, [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (Life == nullptr || !P406Exam::P407UAtSchool(Sub.GetPlayWorld())) return FStepOutcome::Fail(TEXT("ordinary school requires the live campus pawn"));
        const auto Cats = Life->GetSim().SchoolCatalogues();
        if (Cats.Day.School == nullptr) return FStepOutcome::Fail(TEXT("ordinary bell schedule absent"));
        if (Rt.Phase == 0)
        {
            const auto Reply = Life->SubmitSchoolCommand({CommandSource::Player, SchoolPlanDay{}});
            if (!Reply.Accepted) return FStepOutcome::Fail(PWLaneLife::ToFString(Reply.Why.Code));
            Rt.NextPhase();
        }
        const auto Arrival = QuerySchoolArrival(Life->GetWorldState(), Cats.Day);
        if (!Arrival.HasSchool) return FStepOutcome::Fail(TEXT("ordinary school calendar unavailable"));
        if (Arrival.DayKind != SchoolDayKind::SchoolDay) return FStepOutcome::Pass(TEXT("core calendar closes school; ordinary day planning remains usable"));
        const auto Dawn = GameClock::StartOfDay(Life->GetNow());
        if (Rt.Phase == 1)
        {
            const auto Target = Dawn + Arrival.GatesOpenMinute;
            if (Life->GetNow() != Target || !IsWorldQuiescent(Life->GetWorldState())) return P506Move(*Life, Target, Rt);
            const auto Reply = Life->SubmitSchoolCommand({CommandSource::Player, SchoolArrive{}});
            if (!Reply.Accepted) return FStepOutcome::Fail(PWLaneLife::ToFString(Reply.Why.Code));
            Rt.NextPhase();
        }
        if (Rt.Phase == 2)
        {
            const BellBlock* First = nullptr;
            for (const auto& Block : Cats.Day.School->Blocks) if (Block.Kind == BellBlockKind::Period) { First = &Block; break; }
            if (First == nullptr) return FStepOutcome::Fail(TEXT("ordinary period absent"));
            const auto Target = Dawn + First->StartMinute;
            if (Life->GetNow() != Target || !IsWorldQuiescent(Life->GetWorldState())) return P506Move(*Life, Target, Rt);
            const auto Bridge = PWSchoolLive::MakeDayWorld(*Sub.GetPlayWorld());
            FPWSchoolDayModel Day;
            FString Why;
            if (!Bridge->ReadModel(Day, Why)) return FStepOutcome::Fail(Why);
            if (!Day.bSchoolDay) return FStepOutcome::Pass(TEXT("calendar replacement programme has no ordinary lesson"));
            bool Opened = false;
            for (const auto& Row : Day.Rows) if (Row.bAttendAvailable)
            {
                const auto Issue = Bridge->OpenLesson(Row.BindingId, SchoolLessonMode::Attend, {});
                if (!Issue.IsEmpty()) return FStepOutcome::Fail(Issue.ToString());
                Opened = true;
                break;
            }
            if (!Opened) return FStepOutcome::Fail(TEXT("no disclosed Attend lesson at its bell"));
            Rt.NextPhase();
        }
        const auto Lesson = QuerySchoolLesson(Life->GetWorldState().SchoolSessions);
        if (!Lesson.Run) return FStepOutcome::Fail(TEXT("ordinary live lesson absent"));
        const auto Reply = Life->SubmitSchoolCommand({CommandSource::Player, SchoolLessonClose{Lesson.Run->Session}});
        return Reply.Accepted ? FStepOutcome::Pass(TEXT("live ordinary school arrival and one Attend lesson completed; no practice prerequisite bypass")) :
            FStepOutcome::Fail(PWLaneLife::ToFString(Reply.Why.Code));
    });
}

void P506Append(TArray<FStepDef>& Out, const char* Outcome)
{
    const auto Run = MakeShared<FP506Run>();
    Run->Outcome = Outcome;
    Out.Add(PWAcceptance::MakeWorldReadyStep());
    Out.Add(P506Action(TEXT("p506_process_phase"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (Sub.GetArgs().Start != TEXT("new") && Sub.GetArgs().Start != TEXT("continue"))
            return FStepOutcome::Fail(TEXT("post_exam_week scenarios require --restart-pair"));
        Run->Continue = Sub.GetArgs().Start == TEXT("continue");
        if (Run->Continue && !P506ReadSlot(Sub, Run->Saved)) return FStepOutcome::Fail(TEXT("original current-schema save could not be read"));
        return FStepOutcome::Pass(TEXT("explicit new/Continue process phase selected"));
    }));
    TArray<FStepDef> Morning;
    PWAcceptance::P412AppendExaminationMorningSteps(Morning);
    for (auto& Step : Morning)
    {
        const auto Name = TEXT("p506_") + Step.Name;
        if (Step.Name.Contains(TEXT("shot_"))) Step = PWAcceptance::MakeShotStep(Name); else Step.Name = Name;
        Out.Add(P506Phase(MoveTemp(Step), Run, false));
    }
    Out.Add(P506Phase(P506Climb(Run), Run, false));
    Out.Add(P506Phase(PWAcceptance::MakeShotStep(TEXT("p506_shot_result")), Run, false));
    Out.Add(P506Phase(P506Action(TEXT("p506_book_task"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        return P506Press(Sub, P407UFollowupAction::Book, Run->TaskId) ? FStepOutcome::Pass(TEXT("booked the core-selected task through the live panel")) :
            FStepOutcome::Fail(TEXT("task booking unavailable"));
    }), Run, false));
    for (int32 Day = 1; Day <= 7; ++Day)
    {
        const auto Prefix = FString::Printf(TEXT("p506_day%d_"), Day);
        Out.Add(P506Phase(P506Action(Prefix + TEXT("dawn"), [Run, Day](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
        {
            auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
            return Life != nullptr ? P506Move(*Life, Run->WeekStart + Day * MinutesPerDay + 6 * MinutesPerHour, Rt) :
                FStepOutcome::Fail(TEXT("live day owner absent"));
        }), Run, false));
        Out.Add(P506Phase(PWAcceptance::SchoolR7::ScACamera(Prefix + TEXT("camera"), Day % 2 == 0), Run, false));
        Out.Add(P506Phase(P506SchoolMorning(Prefix + TEXT("ordinary_school")), Run, false));
        Out.Add(P506Phase(P506Action(Prefix + TEXT("appointment_day"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
        {
            auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
            if (Life == nullptr) return FStepOutcome::Fail(TEXT("live task owner absent"));
            const auto Row = P506Task(Life->GetSim(), Run->TaskId);
            if (!Row) return FStepOutcome::Fail(TEXT("task disappeared"));
            Run->TaskToday = !Row->P502Record && GameClock::DayNumber(Row->At) == GameClock::DayNumber(Life->GetNow());
            if ((Run->TaskToday || !Rt.Memo.IsEmpty()) && !Run->Missed && (Run->Outcome == "strong" || Run->Outcome == "poor"))
            {
                if (Rt.Memo.IsEmpty()) Rt.Memo = FString::Printf(TEXT("%lld"), static_cast<long long>(Row->At + 1));
                const auto Target = FCString::Atoi64(*Rt.Memo);
                if (Life->GetNow() != Target || !IsWorldQuiescent(Life->GetWorldState())) return P506Move(*Life, Target, Rt);
                if (!P506Press(Sub, P407UFollowupAction::Book, Run->TaskId)) return FStepOutcome::Fail(TEXT("missed task did not expose rebooking"));
                Run->Missed = true;
                const auto Rebooked = P506Task(Life->GetSim(), Run->TaskId);
                if (!Rebooked || Rebooked->At < Target) return FStepOutcome::Fail(TEXT("replacement task slot is in the past"));
                Run->TaskToday = GameClock::DayNumber(Rebooked->At) == GameClock::DayNumber(Target);
            }
            return FStepOutcome::Pass(TEXT("appointment day selected; strong/poor miss once and use the live rebook action"));
        }), Run, false));
        Out.Add(P506Phase(P506Action(Prefix + TEXT("wait_task"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
        {
            auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
            if (Life == nullptr || Rt.Phase > 128) return FStepOutcome::Fail(TEXT("bounded task wait exhausted"));
            const auto Row = P506Task(Life->GetSim(), Run->TaskId);
            if (!Row) return FStepOutcome::Fail(TEXT("booked task absent"));
            if (IsWorldQuiescent(Life->GetWorldState()) && Life->GetNow() == Row->At) return FStepOutcome::Pass(TEXT("wait reached the disclosed task slot"));
            if (!IsWorldQuiescent(Life->GetWorldState()))
            {
                const auto Reply = Life->SubmitSchoolCommand({CommandSource::System, SettleDueEvents{}});
                if (!Reply.Accepted) return FStepOutcome::Fail(PWLaneLife::ToFString(Reply.Why.Code));
            }
            else if (!P506Press(Sub, P407UFollowupAction::Attend, Run->TaskId, true)) return FStepOutcome::Fail(TEXT("live appointment wait unavailable"));
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }), Run, false, true));
        for (const auto Kind : {P407UFollowupAction::Attend, P407UFollowupAction::P502Start, P407UFollowupAction::P502Leave})
            Out.Add(P506Phase(P506Action(Prefix + FString::Printf(TEXT("task_action%d"), static_cast<int32>(Kind)), [Run, Kind](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
            {
                const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
                if (Life == nullptr) return FStepOutcome::Fail(TEXT("live owner absent"));
                const auto Before = Life->GetNow();
                if (!P506Press(Sub, Kind, Run->TaskId) || Life->GetNow() != Before) return FStepOutcome::Fail(TEXT("arrival/start/leave refused or charged time"));
                return FStepOutcome::Pass(TEXT("live zero-minute task transition accepted"));
            }), Run, false, true));
        Out.Add(P506Phase(P506Action(Prefix + TEXT("leave_input_and_resume"), [Run, Day](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
        {
            auto* Host = UPWSchoolPresenter::Get(Sub.GetPlayWorld());
            const auto* Player = Sub.GetPlayWorld()->GetFirstPlayerController();
            if (Host == nullptr || Host->IsOpen() || Player == nullptr || Player->ShouldShowMouseCursor() ||
                UPWInteractionComponent::IsWorldInputBlocked(Sub.GetPawn())) return FStepOutcome::Fail(TEXT("leaving the task did not restore world input"));
            if (!Host->P406OpenExamination() || !P506Press(Sub, P407UFollowupAction::P502Resume, Run->TaskId))
                return FStepOutcome::Fail(TEXT("paused task did not resume from the public live projection"));
            const auto Panel = P506Panel(Sub);
            if (!Panel.IsValid()) return FStepOutcome::Fail(TEXT("resumed task panel absent"));
            Panel->OnPreviewKeyDown(FGeometry(), FKeyEvent(Day % 2 == 0 ? EKeys::Escape : EKeys::Gamepad_FaceButton_Right,
                FModifierKeysState(), 0, false, 0, 0));
            if (Host->IsOpen() || Player->ShouldShowMouseCursor() ||
                UPWInteractionComponent::IsWorldInputBlocked(Sub.GetPawn()) || !Host->P406OpenExamination())
                return FStepOutcome::Fail(TEXT("Esc/B close did not restore world input and permit reopening"));
            return FStepOutcome::Pass(TEXT("world input restored after leave and Esc/B close; task resumed via gamepad action"));
        }), Run, false, true));
        Out.Add(P506Phase(P506Action(Prefix + TEXT("resolve_once"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
        {
            const auto Panel = P506Panel(Sub);
            const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
            if (!Panel.IsValid() || Life == nullptr) return FStepOutcome::Fail(TEXT("live task screen absent"));
            const auto Row = P506Task(Life->GetSim(), Run->TaskId);
            if (!Row || !Row->P502CanResolve || Row->P502Choices.empty()) return FStepOutcome::Fail(TEXT("task is not ready to resolve"));
            Run->Resolved = {P407UFollowupAction::P502Resolve, Row->Action.Id, Row->At, Row->P502Choices.front().Id, Row->P502Revision};
            const auto Before = Life->GetNow();
            if (!P506Press(Sub, P407UFollowupAction::P502Resolve, Run->TaskId)) return FStepOutcome::Fail(TEXT("live task choice refused"));
            const auto Done = P506Task(Life->GetSim(), Run->TaskId);
            const auto Recorded = HashWorldState(Life->GetWorldState());
            P406Exam::FAction Duplicate;
            Duplicate.bP407UFollowup = true;
            Duplicate.P407UCommand = Run->Resolved;
            const auto Repeated = P406Exam::P406MakeLiveWorld(*Sub.GetPlayWorld())->Submit(Duplicate);
            if (Repeated.Accepted || Repeated.Why.Code != "p502.completed" || Recorded != HashWorldState(Life->GetWorldState()))
                return FStepOutcome::Fail(TEXT("duplicate completion changed the live task or its receipt"));
            return Done && Done->P502Stage == P502TaskStage::Completed && Done->P502Record &&
                Done->P502Record->Id == Row->P502Choices.front().Id && Done->P502Record->Records == Row->P502Choices.front().Records &&
                Life->GetNow() - Before == Row->Action.DurationMinutes ? FStepOutcome::Pass(TEXT("one disclosed choice recorded its exact consequence and duration")) :
                FStepOutcome::Fail(TEXT("task record or charged duration differs from the public choice"));
        }), Run, false, true));
    }
    Out.Add(P506Phase(PWAcceptance::MakeShotStep(TEXT("p506_shot_week_record")), Run, false));
    Out.Add(P506Phase(P506Action(TEXT("p506_refuse_other_offer"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (Life == nullptr) return FStepOutcome::Fail(TEXT("live owner absent"));
        for (const auto& Row : Life->GetSim().P407UFollowups())
            if (Row.Action.Id != Run->TaskId && !Row.Action.OfferId.empty() && Row.CanBook)
            {
                const auto Before = Life->GetNow();
                return P506Press(Sub, P407UFollowupAction::Book, Row.Action.Id) && P506Press(Sub, P407UFollowupAction::Cancel, Row.Action.Id) && Before == Life->GetNow()
                    ? FStepOutcome::Pass(TEXT("another qualification offer booked then refused through live cancellation without spending time")) :
                    FStepOutcome::Fail(TEXT("optional offer refusal unavailable"));
            }
        return FStepOutcome::Fail(TEXT("no other actionable qualification offer to refuse"));
    }), Run, false));
    Out.Add(P506Phase(P506Action(TEXT("p506_ordinary_timetable_and_save"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto Panel = P506Panel(Sub);
        auto* Host = UPWSchoolPresenter::Get(Sub.GetPlayWorld());
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!Panel.IsValid() || Host == nullptr || Life == nullptr || Sub.GetHud() == nullptr) return FStepOutcome::Fail(TEXT("save/timetable owners absent"));
        const auto Task = P506Task(Life->GetSim(), Run->TaskId);
        if (!Task || !Task->P502Record || Life->GetSim().ExaminationWorldView().DecisionRequired) return FStepOutcome::Fail(TEXT("week ended without its task or repeated the exam decision"));
        const auto Actions = Panel->P406GetModel().Actions;
        bool Opened = false;
        for (int32 Index = 0; Index < Actions.Num(); ++Index) if (Actions[Index].bP506SchoolDay) { Opened = Panel->P406TriggerAction(Index); break; }
        if (!Opened || Host->GetKind() != EPWSchoolPanel::Day || !Host->GetDayPanel().IsValid()) return FStepOutcome::Fail(TEXT("ordinary timetable handoff failed"));
        Host->Close();
        if (UPWInteractionComponent::IsWorldInputBlocked(Sub.GetPawn())) return FStepOutcome::Fail(TEXT("closing timetable did not restore input"));
        bool Saved = false;
        const auto Notice = Sub.GetHud()->SaveActiveGame(Saved);
        return Saved && P506ReadSlot(Sub, Run->Saved) && HashWorldState(Run->Saved.World) == HashWorldState(Life->GetWorldState())
            ? FStepOutcome::Pass(TEXT("seven ordinary calendar days, task and refusal saved; next process must use title Continue")) : FStepOutcome::Fail(Notice.ToString());
    }), Run, false));
    Out.Add(P506Phase(PWAcceptance::MakeEnterWorldStep(), Run, true));
    Out.Add(P506Phase(P506Action(TEXT("p506_continued_task_once"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        auto* Host = UPWSchoolPresenter::Get(Sub.GetPlayWorld());
        if (Life == nullptr || Host == nullptr) return FStepOutcome::Fail(TEXT("continued owners absent"));
        const auto& Live = Life->GetSim();
        const auto* Player = Sub.GetPlayWorld()->GetFirstPlayerController();
        // Bind the original authenticated snapshot to production catalogues; never inspect follow-up storage keys.
        WorldSim Original(Run->Saved.World, MaxEventsPerAdvance, Live.SchoolCatalogues().Day, {}, Live.SchoolContent(), Live.AwakeningContent(), Live.ExaminationContent());
        std::size_t Count = 0;
        for (const auto& Expected : Original.P407UFollowups()) if (Expected.P502Record)
        {
            ++Count;
            Run->TaskId = Expected.Action.Id;
            const auto Actual = P506Task(Live, Expected.Action.Id);
            if (!Actual || Actual->P502Stage != P502TaskStage::Completed || !Actual->P502Record ||
                Actual->P502Record->Id != Expected.P502Record->Id || Actual->P502Record->Records != Expected.P502Record->Records ||
                Actual->P502Revision != Expected.P502Revision || Actual->At != Expected.At) return FStepOutcome::Fail(TEXT("fresh-process task record differs from the original snapshot"));
            P406Exam::FAction Duplicate;
            Duplicate.bP407UFollowup = true;
            Duplicate.P407UCommand = {P407UFollowupAction::P502Resolve, Actual->Action.Id, Actual->At, Actual->P502Record->Id, Actual->P502Revision};
            const auto Before = HashWorldState(Life->GetWorldState());
            const auto Reply = P406Exam::P406MakeLiveWorld(*Sub.GetPlayWorld())->Submit(Duplicate);
            if (Reply.Accepted || Reply.Why.Code != "p502.completed" || Before != HashWorldState(Life->GetWorldState()))
                return FStepOutcome::Fail(TEXT("duplicate completion changed the continued life"));
        }
        std::size_t LiveCount = 0;
        for (const auto& Row : Live.P407UFollowups()) if (Row.P502Record) ++LiveCount;
        if (Count != 1 || LiveCount != 1 || Live.ExaminationResult().Outcome != Run->Outcome || Live.ExaminationWorldView().DecisionRequired ||
            SerialiseReceipts(Live.State().DayLoop.Receipts) != SerialiseReceipts(Original.State().DayLoop.Receipts) ||
            Host->IsOpen() || Player == nullptr || Player->ShouldShowMouseCursor() ||
            UPWInteractionComponent::IsWorldInputBlocked(Sub.GetPawn())) return FStepOutcome::Fail(TEXT("Continue lost the single task, outcome or world input"));
        if (!Host->P406OpenExamination()) return FStepOutcome::Fail(TEXT("continued result cannot reopen"));
        return FStepOutcome::Pass(TEXT("fresh-process title Continue retained exactly one task record; duplicate live resolve was inert; no repeated exam decision"));
    }), Run, true));
    Out.Add(P506Phase(PWAcceptance::MakeShotStep(TEXT("p506_shot_continued_record")), Run, true));
}
}

namespace PWAcceptance
{
void P506AppendStrongWeekSteps(TArray<FStepDef>& Out) { P506Append(Out, "strong"); }
void P506AppendMiddleWeekSteps(TArray<FStepDef>& Out) { P506Append(Out, "middle"); }
void P506AppendPoorWeekSteps(TArray<FStepDef>& Out) { P506Append(Out, "poor"); }
void P506AppendWithdrawnWeekSteps(TArray<FStepDef>& Out) { P506Append(Out, "withdrawn"); }
void P506AppendDeferredWeekSteps(TArray<FStepDef>& Out) { P506Append(Out, "deferred"); }
void P506AppendDeclinedWeekSteps(TArray<FStepDef>& Out) { P506Append(Out, "declined"); }
}
