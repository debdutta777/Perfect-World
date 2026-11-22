#include "PerfectWorldAcceptancePhase3.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "PerfectWorldAcceptanceAwakening.h"
#include "PerfectWorldAcceptanceAwakeningFloorSteps.h"
#include "PerfectWorldAcceptanceSchoolR7Steps.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldSchoolActors.h"
#include "PerfectWorldSchoolLive.h"
#include "PerfectWorldSchoolPresenter.h"
#include "UI/SPWAwakeningResultPanel.h"
#include "UI/SPWClassLesson.h"
#include "UI/SPWSchoolDay.h"
#include "WorldHash.h"
#include "WorldSave.h"

namespace
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;
using PWAcceptance::AwakeningFloor::AwFlFindFloor;

struct P39FLesson
{
    uint64 Session = 0;
    std::string Binding;
    PerfectWorld::GameInstant OpenedAt = 0;
    int32 Duration = 0;
};

struct P39FRestart
{
    bool bContinue = false;
    PerfectWorld::WorldSaveData Saved;
};

FStepDef P39Action(const TCHAR* Name, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run)
{
    FStepDef Step;
    Step.Name = Name;
    Step.TimeoutSeconds = 30.0;
    Step.Run = MoveTemp(Run);
    return Step;
}

bool P39AdvanceTo(UPWLaneLifeSubsystem& Life, PerfectWorld::GameInstant Target, FString& Why)
{
    // P38AdvanceTo's bound-sim path; never retry a compulsory-supervision interruption as an event cap.
    for (int32 Round = 0; Round < 128; ++Round)
    {
        if (Life.GetNow() > Target) { Why = TEXT("clock already passed the requested instant"); return false; }
        const bool Settled = PerfectWorld::IsWorldQuiescent(Life.GetWorldState());
        if (Settled && Life.GetNow() == Target) return true;
        const auto BeforeMakeUp = Life.GetWorldState().Awakening.MakeUp.Phase;
        PerfectWorld::Command Command{PerfectWorld::CommandSource::System, PerfectWorld::SettleDueEvents{}};
        if (Settled) Command = {PerfectWorld::CommandSource::Player, PerfectWorld::AdvanceTime{Target - Life.GetNow()}};
        const auto Result = Life.SubmitAwakeningCommand(Command);
        if (!Result.Accepted)
        { Why = PWLaneLife::ToFString(Result.Why.Code) + TEXT(": ") + PWLaneLife::ToFString(Result.Why.Message); return false; }
        if (BeforeMakeUp != Life.GetWorldState().Awakening.MakeUp.Phase)
        { Why = TEXT("time advance interrupted for compulsory supervision"); return false; }
    }
    Why = TEXT("live time continuation budget exhausted");
    return false;
}

TSharedPtr<SPWClassLesson> P39LessonPanel(UWorld* World)
{
    const auto* Presenter = UPWSchoolPresenter::Get(World);
    return Presenter != nullptr && Presenter->GetKind() == EPWSchoolPanel::Lesson && Presenter->GetPanel().IsValid()
        ? StaticCastSharedPtr<SPWClassLesson>(Presenter->GetPanel()) : nullptr;
}

APWSchoolSeatActor* P39Seat(UPWAcceptanceSubsystem& Sub)
{
    for (TActorIterator<APWSchoolSeatActor> It(Sub.GetPlayWorld()); It; ++It) return *It;
    return nullptr;
}

void P39AppendLesson(TArray<FStepDef>& Out, const TSharedRef<P39FLesson>& Run, bool Interrupted)
{
    using namespace PWAcceptance;
    using namespace SchoolR7;
    ScAWalkToGate(Out); // Includes normal boot/creation, home door and real plan-route walking.
    const auto P = ScAPoints();
    TArray<FVector> Route;
    Route.Add(FVector(P.GateCentre.X, P.GateCentre.Y - 100.0, ScAFloor));
    Route.Add(FVector(P.GateCentre.X, P.GateCentre.Y + 150.0, ScAFloor));
    Route.Add(P.SignInStand);
    Out.Add(ScAPointsWalk(TEXT("p39_walk_to_school_register"), Route, 60.0, &P.SignInPoint));
    Out.Add(P39Action(TEXT("p39_on_time_window"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (Life == nullptr || Life->GetMinuteOfDay() > 9 * 60 + 58) return FStepOutcome::Fail(TEXT("missed on-time sign-in window"));
        FString Why;
        if (Life->GetMinuteOfDay() < 9 * 60 + 40 &&
            !P39AdvanceTo(*Life, PerfectWorld::GameClock::StartOfDay(Life->GetNow()) + 9 * 60 + 40, Why)) return FStepOutcome::Fail(Why);
        return FStepOutcome::Pass(TEXT("live clock in the on-time window"));
    }));
    const auto Calls = MakeShared<int32>(1);
    Out.Add(ScABaselineStep(TEXT("p39_register_baseline"), Calls));
    Out.Add(ScASignInStep(TEXT("p39_real_school_sign_in"), Calls, FString(), nullptr));
    Out.Add(ScARecordStep(TEXT("p39_core_on_time_attendance"), PerfectWorld::AttendanceOutcome::OnTime, TEXT("on time")));
    const FVector SeatPoint = P.ClassroomAisle + FVector(0, 0, 90);
    Out.Add(ScAPointsWalk(TEXT("p39_walk_to_classroom_seat"), PWSchool::ScClassroomSeatRoute(PWSchool::ScReadCampus(), P), 90.0, &SeatPoint));
    Out.Add(P39Action(TEXT("p39_live_lesson_bell"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        FString Why;
        const auto* Content = Sub.GetPlayWorld() ? PWSchoolLive::SharedContent(*Sub.GetPlayWorld(), Why) : nullptr;
        if (Life == nullptr || Content == nullptr) return FStepOutcome::Fail(TEXT("school content unavailable: ") + Why);
        const auto View = PerfectWorld::BuildSchoolDayView(Content->Lessons, Life->GetWorldState().SchoolSessions, Life->GetWorldState().DayLoop, Content->Cats);
        if (!View.Ok() || !View.SchoolDay) return FStepOutcome::Fail(TEXT("no authoritative school timetable: ") + PWLaneLife::ToFString(View.Issue));
        for (const auto& Row : View.Rows)
        {
            if (Row.Status == PerfectWorld::SchoolDayRowStatus::Open) return FStepOutcome::Pass(TEXT("lesson period already open"));
            if (Row.Status == PerfectWorld::SchoolDayRowStatus::Upcoming)
                return P39AdvanceTo(*Life, PerfectWorld::GameClock::StartOfDay(Life->GetNow()) + Row.StartMinute, Why)
                    ? FStepOutcome::Pass(TEXT("live time.advance reached the lesson bell")) : FStepOutcome::Fail(Why);
        }
        return FStepOutcome::Fail(TEXT("no remaining lesson today"));
    }));
    Out.Add(MakeInteractStep(TEXT("p39_real_seat_opens_day_panel"),
        [](UPWAcceptanceSubsystem& Sub) -> AActor* { return P39Seat(Sub); },
        [](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const auto* Presenter = UPWSchoolPresenter::Get(Sub.GetPlayWorld());
            Detail = TEXT("real seat opened today's timetable");
            return P39Seat(Sub) != nullptr && P39Seat(Sub)->DidOpenPanel() && Presenter != nullptr &&
                Presenter->GetKind() == EPWSchoolPanel::Day && Presenter->GetDayPanel().IsValid();
        }, nullptr, 20.0));
    Out.Add(P39Action(TEXT("p39_open_live_lesson"), [Run, Interrupted](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto* Presenter = UPWSchoolPresenter::Get(Sub.GetPlayWorld());
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        const auto Day = Presenter ? Presenter->GetDayPanel() : nullptr;
        if (!Day.IsValid() || Life == nullptr || !PWSchoolLive::CommandsRouted()) return FStepOutcome::Fail(TEXT("no routed school-day panel"));
        Run->OpenedAt = Life->GetNow();
        const auto Mode = Interrupted ? PerfectWorld::SchoolLessonMode::Foreground : PerfectWorld::SchoolLessonMode::Attend;
        if (!Day->RequestOpen(Mode)) return FStepOutcome::Fail(TEXT("lesson open refused: ") + Day->GetNotice().ToString());
        const auto View = PerfectWorld::QuerySchoolLesson(Life->GetWorldState().SchoolSessions);
        const auto Lesson = P39LessonPanel(Sub.GetPlayWorld());
        if (!View.Run || !Lesson.IsValid() || View.Run->Mode != Mode) return FStepOutcome::Fail(TEXT("no matching core session and lesson screen"));
        Run->Session = View.Run->Session;
        Run->Binding = View.Run->Binding.Id;
        Run->Duration = Lesson->GetFrame().PeriodMinutes;
        return FStepOutcome::Pass(TEXT("core opened the requested lesson mode through the day panel"));
    }));
}

FStepDef P39ClosePanels()
{
    return P39Action(TEXT("p39_close_school_panels"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* Presenter = UPWSchoolPresenter::Get(Sub.GetPlayWorld());
        if (Presenter == nullptr) return FStepOutcome::Fail(TEXT("school presenter missing"));
        if (Rt.Phase == 0)
        {
            if (const auto Lesson = P39LessonPanel(Sub.GetPlayWorld())) Lesson->RequestClose();
            Rt.NextPhase(); return FStepOutcome::Wait();
        }
        Presenter->Close();
        return !Presenter->IsOpen() && !UPWInteractionComponent::IsWorldInputBlocked(Sub.GetPawn())
            ? FStepOutcome::Pass(TEXT("panels closed; world input restored")) : FStepOutcome::Fail(TEXT("panel still owns input"));
    });
}

bool P39ReadSlot(UPWAcceptanceSubsystem& Sub, PerfectWorld::WorldSaveData& Out)
{
    const int32 Slot = Sub.GetArgs().Slot >= 0 ? Sub.GetArgs().Slot : UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot();
    FString Text;
    if (Slot < 0 || !FFileHelper::LoadFileToString(Text, *UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotPath(Slot))) return false;
    const auto Result = PerfectWorld::DeserializeWorldSave(TCHAR_TO_UTF8(*Text), Out);
    return Result.Succeeded() && Result.SourceVersion == PerfectWorld::WorldSaveSchemaVersion && Result.HashVerified;
}

FStepDef P39Phase(FStepDef Step, const TSharedRef<P39FRestart>& Run, bool Continue)
{
    // Screenshot factories capture their names for the contracted filename; preserve their already-unique names.
    if (!Step.Name.StartsWith(TEXT("p39_shot_"))) Step.Name = FString(Continue ? TEXT("p39_continue_") : TEXT("p39_new_")) + Step.Name;
    auto Inner = MoveTemp(Step.Run);
    Step.Run = [Inner = MoveTemp(Inner), Run, Continue](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    { return Run->bContinue == Continue ? Inner(Sub, Rt) : FStepOutcome::Pass(TEXT("belongs to the other fresh-process phase")); };
    return Step;
}
} // namespace

namespace PWAcceptance
{
void P39AppendContinueSteps(TArray<FStepDef>& Out)
{
    const auto Run = MakeShared<P39FRestart>();
    const auto LessonRun = MakeShared<P39FLesson>();
    Out.Add(MakeWorldReadyStep());
    Out.Add(P39Action(TEXT("p39_restart_phase_and_current_schema_slot"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (Sub.GetArgs().Start != TEXT("new") && Sub.GetArgs().Start != TEXT("continue"))
            return FStepOutcome::Fail(TEXT("continue_round9 requires --restart-pair: explicit new then fresh-process continue"));
        Run->bContinue = Sub.GetArgs().Start == TEXT("continue");
        if (Run->bContinue && Sub.GetArgs().Slot >= 0 && Sub.GetArgs().Slot != UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot())
            return FStepOutcome::Fail(TEXT("requested slot is not the title Continue slot"));
        if (Run->bContinue && (!P39ReadSlot(Sub, Run->Saved) || Run->Saved.World.SchoolSessions.Active ||
            Run->Saved.World.SchoolSessions.Periods.size() != 1 || !Run->Saved.World.SchoolSessions.Periods.back().Completed ||
            Run->Saved.World.SchoolSessions.Periods.back().Cancelled || Run->Saved.World.DayLoop.Attendance.OnTimeDays != 1))
            return FStepOutcome::Fail(TEXT("phase 2 needs an authenticated current schema slot with one completed lesson and on-time sign-in"));
        return FStepOutcome::Pass(Run->bContinue ? TEXT("original current schema slot captured before Continue") : TEXT("new-life phase"));
    }));
    TArray<FStepDef> School;
    P39AppendLesson(School, LessonRun, false);
    for (auto& Step : School) Out.Add(P39Phase(MoveTemp(Step), Run, false));
    Out.Add(P39Phase(P39Action(TEXT("p39_complete_one_attended_lesson"), [LessonRun](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto Lesson = P39LessonPanel(Sub.GetPlayWorld());
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!Lesson.IsValid() || Life == nullptr) return FStepOutcome::Fail(TEXT("lesson screen missing"));
        const auto Result = Lesson->ExecuteClose(false);
        const auto& Sessions = Life->GetWorldState().SchoolSessions;
        if (!Result.Ok()) return FStepOutcome::Fail(TEXT("lesson close refused: ") + Lesson->GetLastIssueCode());
        if (Sessions.Active || Sessions.Periods.size() != 1 || Sessions.Periods.back().BindingId != LessonRun->Binding ||
            !Sessions.Periods.back().Completed || Sessions.Periods.back().Cancelled || Life->GetNow() != LessonRun->OpenedAt + LessonRun->Duration)
            return FStepOutcome::Fail(TEXT("core did not record one completed lesson and exactly its period charge"));
        return FStepOutcome::Pass(TEXT("school.lesson.close completed one lesson and charged the period once"));
    }), Run, false));
    Out.Add(P39Phase(MakeShotStep(TEXT("p39_shot_new_completed_lesson")), Run, false));
    Out.Add(P39Phase(P39ClosePanels(), Run, false));
    Out.Add(P39Phase(P39Action(TEXT("p39_save_school_and_wallet"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (Life == nullptr || Sub.GetHud() == nullptr) return FStepOutcome::Fail(TEXT("save owner missing"));
        bool Saved = false;
        const auto Notice = Sub.GetHud()->SaveActiveGame(Saved);
        if (!Saved || !P39ReadSlot(Sub, Run->Saved)) return FStepOutcome::Fail(TEXT("current schema save could not be authenticated: ") + Notice.ToString());
        const auto& State = Life->GetWorldState();
        return State.SchoolSessions == Run->Saved.World.SchoolSessions && State.DayLoop.Attendance == Run->Saved.World.DayLoop.Attendance &&
            State.Commerce.WalletPaise == Run->Saved.World.Commerce.WalletPaise && State.Awakening.Seeded
            ? FStepOutcome::Pass(TEXT("authenticated current schema slot contains actual school history and wallet; driver exits normally"))
            : FStepOutcome::Fail(TEXT("saved school, attendance or wallet differs from the live core"));
    }), Run, false));
    Out.Add(P39Phase(MakeEnterWorldStep(), Run, true));
    Out.Add(P39Phase(P39Action(TEXT("p39_continued_core_school_and_wallet"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (Life == nullptr) return FStepOutcome::Fail(TEXT("continued world missing"));
        const auto& State = Life->GetWorldState();
        const auto& Saved = Run->Saved.World;
        if (State.WorldSeed != Saved.WorldSeed || !State.Awakening.Seeded || State.SchoolSessions != Saved.SchoolSessions ||
            State.DayLoop.Attendance != Saved.DayLoop.Attendance || State.Commerce.WalletPaise != Saved.Commerce.WalletPaise ||
            State.Commerce.TransactionRevision != Saved.Commerce.TransactionRevision)
            return FStepOutcome::Fail(TEXT("fresh-process Continue lost the life, school record, attendance or wallet"));
        return FStepOutcome::Pass(FString::Printf(TEXT("fresh-process Continue retained one lesson, sign-in and wallet %lld paise"), static_cast<long long>(State.Commerce.WalletPaise)));
    }), Run, true));
    Out.Add(P39Phase(MakeShotStep(TEXT("p39_shot_continued_school_life")), Run, true));
}

void P39AppendInterruptedSteps(TArray<FStepDef>& Out)
{
    const auto Run = MakeShared<P39FLesson>();
    P39AppendLesson(Out, Run, true);
    Out.Add(P39Action(TEXT("p39_spend_some_foreground_lesson_units"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto Lesson = P39LessonPanel(Sub.GetPlayWorld());
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!Lesson.IsValid() || Life == nullptr) return FStepOutcome::Fail(TEXT("foreground lesson missing"));
        for (const auto& Key : Lesson->GetActionKeys())
        {
            if (Key == TEXT("open") || Key == TEXT("record") || !Lesson->IsActionEnabled(Key)) continue;
            const auto Seq = Life->GetWorldState().Seq;
            if (!Lesson->PressAction(Key)) return FStepOutcome::Fail(TEXT("enabled lesson action could not be pressed"));
            if (!Lesson->GetLastIssueCode().IsEmpty()) return FStepOutcome::Fail(TEXT("lesson choice refused: ") + Lesson->GetLastIssueCode());
            const auto& Frame = Lesson->GetFrame();
            if (Life->GetWorldState().Seq > Seq && Frame.bExposureActive && Frame.UnitsLeft < Frame.UnitsTotal && !Frame.bReadyToClose)
                return FStepOutcome::Pass(TEXT("real school.lesson.choose spent units; exposure remains unfinished"));
        }
        return FStepOutcome::Fail(TEXT("no unfinished, paid lesson action available"));
    }));
    Out.Add(P39Action(TEXT("p39_leave_early_two_presses_and_elapsed_charge"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto Lesson = P39LessonPanel(Sub.GetPlayWorld());
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!Lesson.IsValid() || Life == nullptr || !Lesson->CanLeaveEarly()) return FStepOutcome::Fail(TEXT("Leave early unavailable"));
        const auto Before = PerfectWorld::HashWorldState(Life->GetWorldState());
        if (Lesson->IsLeaveArmed() || Lesson->PressLeaveEarly() || !Lesson->IsLeaveArmed() ||
            PerfectWorld::HashWorldState(Life->GetWorldState()) != Before) return FStepOutcome::Fail(TEXT("first press did not only arm Leave early"));
        if (!Lesson->PressLeaveEarly()) return FStepOutcome::Fail(TEXT("second press refused: ") + Lesson->GetLastIssueCode());
        const auto& Result = Lesson->GetLastResult();
        int64 Elapsed = -1;
        for (const auto& Event : Result.Events)
        {
            if (Event.Kind == PerfectWorld::DayLoopEventKind::Completed || !Event.Credited.empty()) return FStepOutcome::Fail(TEXT("cancelled lesson emitted completion learning"));
            if (Event.Kind == PerfectWorld::DayLoopEventKind::Cancelled) Elapsed = Event.Minutes;
        }
        const auto& Sessions = Life->GetWorldState().SchoolSessions;
        if (!Result.Ok() || Result.Session != Run->Session || Elapsed <= 0 || Elapsed >= Run->Duration ||
            Life->GetNow() != Run->OpenedAt + Elapsed || Sessions.Active || Sessions.Periods.size() != 1 ||
            Sessions.Periods.back().BindingId != Run->Binding || !Sessions.Periods.back().Cancelled || Sessions.Periods.back().Completed)
            return FStepOutcome::Fail(TEXT("cancel record or partial charge disagrees with the core cancellation event"));
        return FStepOutcome::Pass(FString::Printf(TEXT("first press free; second recorded cancellation and charged only %lld elapsed minutes"), static_cast<long long>(Elapsed)));
    }));
    Out.Add(P39Action(TEXT("p39_duplicate_lesson_close_refused_atomically"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (Life == nullptr) return FStepOutcome::Fail(TEXT("live core missing"));
        const auto Before = PerfectWorld::HashWorldState(Life->GetWorldState());
        const auto Result = Life->SubmitSchoolCommand({PerfectWorld::CommandSource::Player, PerfectWorld::SchoolLessonClose{Run->Session, true, 0}});
        return !Result.Accepted && Result.Why.Code == "school.session.stale_session" && Result.Events.empty() &&
            PerfectWorld::HashWorldState(Life->GetWorldState()) == Before
            ? FStepOutcome::Pass(TEXT("core refused duplicate close with school.session.stale_session; full world hash unchanged"))
            : FStepOutcome::Fail(TEXT("duplicate close was accepted, mutated state or returned the wrong refusal: ") + PWLaneLife::ToFString(Result.Why.Code));
    }));
    Out.Add(MakeShotStep(TEXT("p39_shot_cancelled_lesson")));
    Out.Add(P39ClosePanels());
}

void P39AppendWithdrawalSteps(TArray<FStepDef>& Out)
{
    using namespace AwakeningFloor;
    Out.Add(MakeWorldReadyStep());
    Out.Add(MakeEnterWorldStep());
    Out.Add(AwFlStartStep()); // Sole declared start teleport. No scripted bridge installer.
    Out.Add(P39Action(TEXT("p39_awakening_day_real_entry"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        const auto* Floor = AwFlFindFloor(Sub);
        auto* Interaction = Sub.GetInteraction();
        if (Life == nullptr || Floor == nullptr || Interaction == nullptr) return FStepOutcome::Fail(TEXT("entry context missing"));
        if (Rt.Phase == 0)
        {
            if (!Life->GetWorldState().Awakening.Seeded || Life->GetWorldState().Awakening.Granted ||
                !Life->GetWorldState().Awakening.Conduct.Entries.empty() || Life->GetSim().AwakeningContent().Recipes == nullptr)
                return FStepOutcome::Fail(TEXT("withdrawal requires a fresh seeded life and bound recipes"));
            const auto* Pad = Floor->FindPad(Floor->GetLayout().EntryPad.Id);
            if (Pad == nullptr) return FStepOutcome::Fail(TEXT("campus terminal missing"));
            Sub.FaceWorldPoint(Pad->GetInteractionPoint());
            if (Interaction->GetTarget() != Pad || !Interaction->IsPromptVisible()) return FStepOutcome::Wait();
            FString Why;
            if (UPWInteractionComponent::IsWorldInputBlocked(Sub.GetPawn()) ||
                !P39AdvanceTo(*Life, Life->GetWorldState().Awakening.AppointmentAt, Why)) return FStepOutcome::Fail(TEXT("entry time/input: ") + Why);
            if (!Interaction->TryInteract() || !Floor->GetLastCode().IsEmpty()) return FStepOutcome::Fail(TEXT("tutorial entry refused: ") + Floor->GetLastCode());
            if (Life->GetWorldState().Tutorial.Phase != PerfectWorld::TutorialDungeonPhase::Active || Life->GetWorldState().Tutorial.NextSituation != 0)
                return FStepOutcome::Fail(TEXT("core did not start an untouched tutorial"));
            Rt.NextPhase();
        }
        if (Floor->IsTransitioning()) return FStepOutcome::Wait();
        return Floor->IsOccupied() ? FStepOutcome::Pass(TEXT("tutorial.enter accepted at the exact appointment; real floor transfer completed"))
            : FStepOutcome::Fail(TEXT("entry transfer failed"));
    }));
    // The nearby return pad is the floor's real immediate exit: Leave submits tutorial.withdraw before transferring to campus.
    Out.Add(AwFlWalkToReturnStep(TEXT("p39_walk_directly_to_early_exit")));
    Out.Add(P39Action(TEXT("p39_real_early_exit_grants_and_shows_result"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        const auto* Floor = AwFlFindFloor(Sub);
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        auto* Interaction = Sub.GetInteraction();
        const auto* Presenter = UPWSchoolPresenter::Get(Sub.GetPlayWorld());
        if (Floor == nullptr || Life == nullptr || Interaction == nullptr || Presenter == nullptr) return FStepOutcome::Fail(TEXT("withdrawal context missing"));
        if (Rt.Phase == 0)
        {
            const auto* Pad = Floor->FindPad(Floor->GetLayout().LeavePad.Id);
            if (Pad == nullptr) return FStepOutcome::Fail(TEXT("return pad missing"));
            Sub.FaceWorldPoint(Pad->GetInteractionPoint());
            if (Interaction->GetTarget() != Pad || !Interaction->IsPromptVisible()) return FStepOutcome::Wait();
            if (Life->GetWorldState().Tutorial.NextSituation != 0 || !Life->GetWorldState().Awakening.Conduct.Entries.empty())
                return FStepOutcome::Fail(TEXT("a situation was played before immediate withdrawal"));
            if (!Interaction->TryInteract()) return FStepOutcome::Fail(TEXT("early exit interaction refused"));
            if (!Life->GetWorldState().Awakening.Granted) return FStepOutcome::Fail(TEXT("core refused withdrawal; no grant: ") + Interaction->GetNotice().ToString());
            Rt.NextPhase();
        }
        if (Floor->IsTransitioning()) return FStepOutcome::Wait();
        const auto& State = Life->GetWorldState();
        if (Floor->IsOccupied() || !State.Tutorial.Withdrawn || State.Tutorial.NextSituation != 1 ||
            State.Tutorial.Phase != PerfectWorld::TutorialDungeonPhase::Completed || !State.Tutorial.StartedAt ||
            !State.Awakening.Granted || !State.LiveAwakenedProfile || State.Awakening.Granted->At != *State.Tutorial.StartedAt + PerfectWorld::TutorialSessionMinutes ||
            State.Tutorial.CompletedAt != State.Awakening.Granted->At || State.Awakening.Conduct.Entries.size() != 1 ||
            State.Awakening.Conduct.Entries.front().Record.Action != PerfectWorld::ConductAction::Withdraw ||
            Presenter->GetKind() != EPWSchoolPanel::AwakeningResult || !Presenter->GetPanel().IsValid())
            return FStepOutcome::Fail(TEXT("withdrawal, one-hour grant, conduct or real result host disagrees"));
        const auto Panel = StaticCastSharedPtr<SPWAwakeningResultPanel>(Presenter->GetPanel());
        if (!Panel->GetModel().bGranted || Panel->GetModel().Tattvas.Num() != PerfectWorld::TattvaCount ||
            Panel->GetModel().Capabilities.Num() != PerfectWorld::PowerDimensionCount)
            return FStepOutcome::Fail(TEXT("result panel does not display the grant"));
        int32 Entries = 0, Withdrawals = 0;
        for (const auto& Command : Life->GetSim().Log())
        {
            if (std::holds_alternative<PerfectWorld::TutorialAct>(Command.Payload) ||
                std::holds_alternative<PerfectWorld::CountdownPracticeConfirm>(Command.Payload))
                return FStepOutcome::Fail(TEXT("immediate withdrawal log contains a situation action or practice"));
            if (std::holds_alternative<PerfectWorld::TutorialEnter>(Command.Payload)) ++Entries;
            if (std::holds_alternative<PerfectWorld::TutorialWithdraw>(Command.Payload)) ++Withdrawals;
        }
        if (Entries != 1 || Withdrawals != 1) return FStepOutcome::Fail(TEXT("withdrawal requires exactly one real entry and one real withdrawal command"));
        return FStepOutcome::Pass(TEXT("immediate tutorial.withdraw granted; one withdrawal conduct entry, no situation actions/combat; real result visible on campus"));
    }));
    Out.Add(MakeShotStep(TEXT("p39_shot_withdrawal_grant")));
    Out.Add(P39ClosePanels());
}

void P39AppendOrdinarySteps(TArray<FStepDef>& Out)
{
    // Existing live, non-combat floor play, real return, 17:59 -> 18:00, visible countdown and ordinary-day button.
    P38AppendAwakeningSteps(Out);
    Out.Add(P39Action(TEXT("p317_decline_neighbour_shift_with_disclosed_cost"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        const auto Countdown = Life ? Life->GetSim().Countdown() : std::nullopt;
        if (!Countdown) return FStepOutcome::Fail(TEXT("no live countdown"));
        const auto* Neighbour = PerfectWorld::CountdownFindRecord(*Countdown, "oblig.neighbour_shop_shift");
        const auto* Teacher = PerfectWorld::CountdownFindRecord(*Countdown, "oblig.teacher_hall_setup");
        if (!Neighbour || !Teacher || Neighbour->Person.empty() || Teacher->Person.empty())
            return FStepOutcome::Fail(TEXT("the ordinary route requires real neighbour and teacher contacts"));
        for (const auto& Proposal : PerfectWorld::CountdownProposals(Life->GetWorldState(), *Countdown,
            Life->GetSim().AwakeningContent().Countdown, Neighbour->Id))
        {
            if (Proposal.Proposal != PerfectWorld::CountdownProposal::Decline || !Proposal.Available) continue;
            const auto Done = Life->SubmitAwakeningCommand({PerfectWorld::CommandSource::Player,
                PerfectWorld::CountdownNegotiateObligation{Neighbour->Id, Proposal.Proposal}});
            if (!Done.Accepted) return FStepOutcome::Fail(PWLaneLife::ToFString(Done.Why.Code));
            const auto After = Life->GetSim().Countdown();
            const auto* Record = After ? PerfectWorld::CountdownFindRecord(*After, Neighbour->Id) : nullptr;
            if (!Record || Record->Disposition != PerfectWorld::CountdownDisposition::Declined ||
                After->Goodwill != Countdown->Goodwill + Proposal.GoodwillIfAccepted || Done.To != Done.From)
                return FStepOutcome::Fail(TEXT("declining did not record the disclosed consequence at no time cost"));
            return FStepOutcome::Pass(FString::Printf(TEXT("real obligation command declined the shop shift; disclosed goodwill %+d; other promises remain standing"),
                Proposal.GoodwillIfAccepted));
        }
        return FStepOutcome::Fail(TEXT("the disclosed decline proposal is unavailable"));
    }));
    Out.Add(P39Action(TEXT("p39_ordinary_days_reach_fixed_exam_deadline_without_practice"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (Life == nullptr || !Life->GetWorldState().Awakening.Granted) return FStepOutcome::Fail(TEXT("live grant missing"));
        const auto GrantAt = Life->GetWorldState().Awakening.Granted->At;
        const auto Deadline = PerfectWorld::GameClock::FromCalendar(2043, 2, 2, 6, 0);
        for (const auto& Command : Life->GetSim().Log())
            if (std::holds_alternative<PerfectWorld::CountdownPracticeConfirm>(Command.Payload)) return FStepOutcome::Fail(TEXT("ordinary route already practised"));
        // P317: bare time cannot perform promises. Resolve ordinary days through the same command as the live planner.
        for (int32 DayIndex = 0; DayIndex < PerfectWorld::CountdownDays; ++DayIndex)
        {
            PerfectWorld::WeekEntry Rest;
            Rest.Kind = PerfectWorld::WeekEntryKind::Spend;
            Rest.StartMinute = 1439;
            Rest.Minutes = 1;
            Rest.Pass = PerfectWorld::DayPassKind::Rest;
            const auto Done = Life->SubmitAwakeningCommand({PerfectWorld::CommandSource::Player,
                PerfectWorld::CountdownResolveDays{{PerfectWorld::WeekDayPlan{{Rest}}}}});
            if (!Done.Accepted) return FStepOutcome::Fail(PWLaneLife::ToFString(Done.Why.Code) + TEXT(": ") + PWLaneLife::ToFString(Done.Why.Message));
            if (Done.To <= Done.From) return FStepOutcome::Fail(TEXT("ordinary day made no progress"));
            const auto Countdown = Life->GetSim().Countdown();
            const auto View = Life->GetSim().CountdownView();
            const auto& State = Life->GetWorldState();
            if (!Countdown || !View.Issue.Ok() || Countdown->End != Deadline || State.Awakening.ExaminationAt != Deadline ||
                View.ExaminationAt != Deadline || !State.Awakening.Granted || State.Awakening.Granted->At != GrantAt ||
                !State.LiveAwakenedProfile || State.LiveAwakenedProfile->SupervisedPracticeMinutes != 0 || Life->GetSim().LastCountdownPracticeResult())
                return FStepOutcome::Fail(TEXT("ordinary time lost countdown/grant, changed fixed deadline or introduced practice"));
            for (const auto& Window : State.LiveAwakenedProfile->Window)
                for (const auto& Day : Window)
                    if (Day.Minutes != 0 || Day.GainCentipoints != 0) return FStepOutcome::Fail(TEXT("ordinary route contains supernatural practice"));
            if (Countdown->Closed)
            {
                for (const auto& Obligation : Countdown->Obligations)
                    if (!Obligation.Done && Obligation.Disposition != PerfectWorld::CountdownDisposition::Declined)
                        return FStepOutcome::Fail(TEXT("an ordinary promise was bypassed without honouring or declining it"));
                return Countdown->Closed && View.ExaminationDue && Life->GetNow() == Deadline && PerfectWorld::IsWorldQuiescent(State)
                    ? FStepOutcome::Pass(TEXT("ordinary days honoured standing promises through real commands; zero practice; original 2 Feb 2043 06:00 reached without a dead end"))
                    : FStepOutcome::Fail(TEXT("fixed examination-morning boundary not reached cleanly"));
            }
        }
        return FStepOutcome::Fail(TEXT("ordinary resolution exceeded the ten-day budget"));
    }));
    Out.Add(MakeShotStep(TEXT("p39_shot_ordinary_exam_morning")));
}
} // namespace PWAcceptance
