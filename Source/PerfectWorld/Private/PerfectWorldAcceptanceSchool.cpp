#include "PerfectWorldAcceptanceSchool.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameClock.h"
#include "GameFramework/PlayerController.h"
#include "PerfectWorldAcceptanceSchoolR7Steps.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldSchoolActors.h"
#include "PerfectWorldSchoolLive.h"
#include "PerfectWorldSchoolPresenter.h"
#include "UI/SPWClassLesson.h"
#include "UI/SPWSchoolDay.h"

// school_lesson_round9 (see the header). It copies commerce_shanti_round8's structure: real walking, the real interaction target, the real
// panel, and a conclusion read from the portable core's world rather than from panel text alone.

namespace
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

struct FS03LessonRun
{
    PerfectWorld::GameInstant ClockAtPanel = 0;
    FString SelectedId;
    FString BindingId;
    uint64 SessionId = 0;
    PerfectWorld::GameInstant ClockAtOpen = 0;
    int32 Choices = 0;
    bool bReady = false;
    bool bLessonScreen = false;
};

FStepDef S03Action(const TCHAR* Name, const double Timeout, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run)
{
    FStepDef Step;
    Step.Name = Name;
    Step.TimeoutSeconds = Timeout;
    Step.Run = MoveTemp(Run);
    return Step;
}

APWSchoolSeatActor* S03FindSeat(UWorld* World)
{
    if (World != nullptr)
    {
        for (TActorIterator<APWSchoolSeatActor> It(World); It; ++It)
        {
            return *It;
        }
    }
    return nullptr;
}

bool S03DayView(UPWAcceptanceSubsystem& Sub, PerfectWorld::SchoolDayView& Out, FString& Why)
{
    UWorld* World = Sub.GetPlayWorld();
    const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(World);
    const PWSchoolLive::FS03Content* Content = World != nullptr ? PWSchoolLive::SharedContent(*World, Why) : nullptr;
    if (Life == nullptr || Content == nullptr)
    {
        return false;
    }
    const PerfectWorld::WorldState& State = Life->GetWorldState();
    Out = PerfectWorld::BuildSchoolDayView(Content->Lessons, PWSchoolLive::SessionsOf(State), State.DayLoop, Content->Cats);
    if (!Out.Ok())
    {
        Why = PWLaneLife::ToFString(Out.Issue);
        return false;
    }
    return true;
}

TSharedPtr<SPWClassLesson> S03LessonScreen(UWorld* World)
{
    UPWSchoolPresenter* Presenter = UPWSchoolPresenter::Get(World);
    if (Presenter == nullptr || Presenter->GetKind() != EPWSchoolPanel::Lesson || !Presenter->GetPanel().IsValid())
    {
        return nullptr;
    }
    return StaticCastSharedPtr<SPWClassLesson>(Presenter->GetPanel());
}
} // namespace

namespace PWAcceptance
{
void AppendSchoolLessonRound9Steps(TArray<FStepDef>& Out)
{
    using namespace SchoolR7;
    const TSharedRef<FS03LessonRun> Run = MakeShared<FS03LessonRun>();

    // A school day: home, the gate, the on-time window, sign in (the core plans an empty day when it asks, then records the arrival).
    ScAWalkToGate(Out);
    const PWSchool::FScPoints P = ScAPoints();
    TArray<FVector> ToRegister;
    ToRegister.Add(FVector(P.GateCentre.X, P.GateCentre.Y - 100.0, ScAFloor));
    ToRegister.Add(FVector(P.GateCentre.X, P.GateCentre.Y + 150.0, ScAFloor));
    ToRegister.Add(P.SignInStand);
    Out.Add(ScAPointsWalk(TEXT("walk_through_gate_to_register"), ToRegister, 60.0, &P.SignInPoint));
    Out.Add(ScAClockStep(TEXT("clock_to_the_on_time_window_0940"), 9 * 60 + 40, 9 * 60 + 58));
    const TSharedRef<int32> Calls = MakeShared<int32>(1);
    Out.Add(ScABaselineStep(TEXT("register_baseline"), Calls));
    Out.Add(ScASignInStep(TEXT("start_the_school_day_sign_in"), Calls, FString(), nullptr));
    Out.Add(ScARecordStep(TEXT("core_recorded_on_time"), PerfectWorld::AttendanceOutcome::OnTime, TEXT("on time")));

    // To the classroom seat.
    // S06: the verandah's pillars stand every 500 cm; the straight diagonal from the forecourt to the doorway ran into the one at (7250, 6175).
    // PWSchool::ScClassroomSeatRoute goes up the office bay and along the verandah walk (checked clear for the capsule by PerfectWorld.SchoolCampus.SeatRoute).
    const TArray<FVector> ToSeat = PWSchool::ScClassroomSeatRoute(PWSchool::ScReadCampus(), P);
    const FVector SeatPoint = P.ClassroomAisle + FVector(0.0, 0.0, 90.0);
    Out.Add(ScAPointsWalk(TEXT("walk_to_the_classroom_seat"), ToSeat, 90.0, &SeatPoint));

    // At a bell: the first period that has not passed; a period already open is left alone.
    Out.Add(S03Action(TEXT("clock_to_a_lesson_bell"), 20.0, [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        PerfectWorld::SchoolDayView View;
        FString Why;
        UWorld* World = Sub.GetPlayWorld();
        UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(World);
        if (Life == nullptr || !S03DayView(Sub, View, Why))
        {
            return FStepOutcome::Fail(TEXT("no school day view: ") + Why);
        }
        if (!View.SchoolDay)
        {
            return FStepOutcome::Fail(TEXT("today is not a school day"));
        }
        for (const PerfectWorld::SchoolDayRow& Row : View.Rows)
        {
            if (Row.Status == PerfectWorld::SchoolDayRowStatus::Open)
            {
                return FStepOutcome::Pass(FString::Printf(TEXT("period %d (%hs) is open at the bell"), Row.Period, Row.Subject.c_str()));
            }
            if (Row.Status == PerfectWorld::SchoolDayRowStatus::Upcoming)
            {
                const PerfectWorld::GameInstant Target = PerfectWorld::GameClock::StartOfDay(Life->GetNow()) + Row.StartMinute;
                FString Move;
                if (!PWSchool::ScAdvanceLaneClock(*World, Target, Move))
                {
                    return FStepOutcome::Fail(Move);
                }
                return FStepOutcome::Pass(FString::Printf(TEXT("clock moved to the bell of period %d (%hs)"), Row.Period, Row.Subject.c_str()));
            }
        }
        return FStepOutcome::Fail(TEXT("no period is open or still to come today"));
    }));

    // "Take your seat" through the real interaction target.
    Out.Add(MakeInteractStep(TEXT("take_your_seat_opens_the_day_panel"),
        [](UPWAcceptanceSubsystem& Sub) -> AActor* { return S03FindSeat(Sub.GetPlayWorld()); },
        [Run](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const UPWSchoolPresenter* Presenter = UPWSchoolPresenter::Get(Sub.GetPlayWorld());
            const APWSchoolSeatActor* Seat = S03FindSeat(Sub.GetPlayWorld());
            if (Presenter == nullptr || Seat == nullptr || !Seat->DidOpenPanel() || Presenter->GetKind() != EPWSchoolPanel::Day || !Presenter->GetDayPanel().IsValid())
            {
                return false;
            }
            Run->ClockAtPanel = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld())->GetNow();
            Detail = TEXT("the real seat interaction opened the school-day panel");
            return true;
        },
        nullptr, 20.0));

    Out.Add(S03Action(TEXT("day_panel_blocks_world_input_and_holds_the_clock"), 10.0, [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        UWorld* World = Sub.GetPlayWorld();
        const UPWSchoolPresenter* Presenter = UPWSchoolPresenter::Get(World);
        const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(World);
        const TSharedPtr<SPWSchoolDay> Day = Presenter != nullptr ? Presenter->GetDayPanel() : nullptr;
        if (!Day.IsValid() || Life == nullptr || Sub.GetPawn() == nullptr)
        {
            return FStepOutcome::Fail(TEXT("the day panel is not open"));
        }
        if (!Day->GetModel().bAvailable || !Day->GetModel().bSchoolDay || Day->GetModel().Rows.Num() < 6)
        {
            return FStepOutcome::Fail(FString::Printf(TEXT("the panel's model is not today's timetable (available %d, rows %d)"), Day->GetModel().bAvailable, Day->GetModel().Rows.Num()));
        }
        if (!UPWInteractionComponent::IsWorldInputBlocked(Sub.GetPawn()))
        {
            return FStepOutcome::Fail(TEXT("world input is not blocked while the panel is open"));
        }
        if (Life->GetClockHold() != EPWClockHold::School)
        {
            return FStepOutcome::Wait();
        }
        if (Life->GetNow() != Run->ClockAtPanel)
        {
            return FStepOutcome::Fail(TEXT("the world clock moved while the panel was open"));
        }
        Run->SelectedId = Day->GetSelectedId();
        return FStepOutcome::Pass(FString::Printf(TEXT("%d periods listed, input blocked, clock held; chosen %s"), Day->GetModel().Rows.Num(), *Run->SelectedId));
    }));
    Out.Add(MakeShotStep(TEXT("shot_school_day_panel")));

    // Open a foreground lesson from the panel: the core opens it (school.lesson.open through the reducer) and the lesson screen takes over.
    Out.Add(S03Action(TEXT("open_a_lesson_from_the_panel"), 15.0, [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        UWorld* World = Sub.GetPlayWorld();
        UPWSchoolPresenter* Presenter = UPWSchoolPresenter::Get(World);
        const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(World);
        const TSharedPtr<SPWSchoolDay> Day = Presenter != nullptr ? Presenter->GetDayPanel() : nullptr;
        if (!Day.IsValid() || Life == nullptr)
        {
            return FStepOutcome::Fail(TEXT("the day panel is not open"));
        }
        if (!PWSchoolLive::CommandsRouted())
        {
            return FStepOutcome::Fail(TEXT("the school commands are not routed through the reducer"));
        }
        Run->ClockAtOpen = Life->GetNow();
        if (!Day->RequestOpen(PerfectWorld::SchoolLessonMode::Foreground))
        {
            return FStepOutcome::Fail(TEXT("the core refused the foreground lesson: ") + Day->GetNotice().ToString());
        }
        const PerfectWorld::SchoolLessonView View = PerfectWorld::QuerySchoolLesson(PWSchoolLive::SessionsOf(Life->GetWorldState()));
        if (!View.Run.has_value() || !S03LessonScreen(World).IsValid())
        {
            return FStepOutcome::Fail(TEXT("the core holds no open lesson, or the lesson screen did not open"));
        }
        Run->SessionId = View.Run->Session;
        Run->BindingId = PWLaneLife::ToFString(View.Run->Binding.Id);
        Run->bLessonScreen = true;
        return FStepOutcome::Pass(FString::Printf(TEXT("the core opened lesson %s (session %llu); the lesson screen took over"), *Run->BindingId, Run->SessionId));
    }));
    Out.Add(MakeShotStep(TEXT("shot_lesson_screen")));

    // Real choices: press enabled rows of the lesson screen (each is one school.lesson.choose through the reducer) until the exercise is
    // ready to record, or the budget is spent. At least one choice must be accepted by the core.
    Out.Add(S03Action(TEXT("make_real_lesson_choices"), 30.0, [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        UWorld* World = Sub.GetPlayWorld();
        const TSharedPtr<SPWClassLesson> Lesson = S03LessonScreen(World);
        const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(World);
        if (!Lesson.IsValid() || Life == nullptr)
        {
            return FStepOutcome::Fail(TEXT("no lesson screen"));
        }
        for (int32 Guard = 0; Guard < 40 && !Lesson->GetFrame().bReadyToClose; ++Guard)
        {
            bool bPressed = false;
            for (const FString& Key : Lesson->GetActionKeys())
            {
                if (Key == TEXT("open") || Key == TEXT("record") || !Lesson->IsActionEnabled(Key))
                {
                    continue;
                }
                if (Lesson->PressAction(Key))
                {
                    bPressed = true;
                    if (Lesson->GetLastResult().Ok() && Lesson->GetLastResult().Session == Run->SessionId)
                    {
                        ++Run->Choices;
                    }
                    break;
                }
            }
            if (!bPressed)
            {
                break;
            }
        }
        if (Run->Choices < 1)
        {
            return FStepOutcome::Fail(TEXT("the core accepted no lesson choice; last code '") + Lesson->GetLastIssueCode() + TEXT("'"));
        }
        Run->bReady = Lesson->GetFrame().bReadyToClose;
        return FStepOutcome::Pass(FString::Printf(TEXT("%d choices accepted by the core; ready to record: %d"), Run->Choices, Run->bReady));
    }));

    // Record the period (normal close when the exercise finished, else leave early); the core closes it and charges the time.
    Out.Add(S03Action(TEXT("record_the_lesson"), 15.0, [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        UWorld* World = Sub.GetPlayWorld();
        const TSharedPtr<SPWClassLesson> Lesson = S03LessonScreen(World);
        const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(World);
        if (!Lesson.IsValid() || Life == nullptr)
        {
            return FStepOutcome::Fail(TEXT("no lesson screen"));
        }
        const PerfectWorld::SchoolSessionResult Result = Lesson->ExecuteClose(!Run->bReady);
        if (!Result.Ok())
        {
            return FStepOutcome::Fail(TEXT("the core refused the close: ") + Lesson->GetLastIssueCode());
        }
        if (Life->GetNow() < Run->ClockAtOpen)
        {
            return FStepOutcome::Fail(TEXT("the clock went backwards"));
        }
        return FStepOutcome::Pass(FString::Printf(TEXT("the core closed the lesson (%s); the clock advanced %lld minutes"),
            Run->bReady ? TEXT("recorded") : TEXT("left early"), static_cast<long long>(Life->GetNow() - Run->ClockAtOpen)));
    }));

    // Back leaves the summary (to the day panel); closing the panel restores game input and the clock.
    Out.Add(S03Action(TEXT("close_the_panel_and_restore_play"), 15.0, [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        UWorld* World = Sub.GetPlayWorld();
        UPWSchoolPresenter* Presenter = UPWSchoolPresenter::Get(World);
        if (Presenter == nullptr)
        {
            return FStepOutcome::Fail(TEXT("no school presenter"));
        }
        if (Rt.Phase == 0)
        {
            if (const TSharedPtr<SPWClassLesson> Lesson = S03LessonScreen(World))
            {
                Lesson->RequestClose();
            }
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        if (Rt.Phase == 1)
        {
            if (Presenter->GetKind() != EPWSchoolPanel::Day)
            {
                return FStepOutcome::Fail(TEXT("Back did not return to the day panel"));
            }
            Presenter->Close();
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(World);
        const APlayerController* Controller = Sub.GetController();
        if (Presenter->IsOpen() || Sub.GetPawn() == nullptr || UPWInteractionComponent::IsWorldInputBlocked(Sub.GetPawn()) ||
            (Controller != nullptr && Controller->bShowMouseCursor))
        {
            return FStepOutcome::Fail(TEXT("world input was not restored after closing"));
        }
        return Life != nullptr && Life->GetClockHold() == EPWClockHold::None ? FStepOutcome::Pass(TEXT("panel closed, game input and the clock restored")) : FStepOutcome::Wait();
    }));

    // What the core recorded: no open lesson, and a period record for the opened binding.
    Out.Add(S03Action(TEXT("core_recorded_the_lesson"), 10.0, [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (Life == nullptr)
        {
            return FStepOutcome::Fail(TEXT("no lane-life subsystem"));
        }
        const PerfectWorld::SchoolSessionsState& Sessions = PWSchoolLive::SessionsOf(Life->GetWorldState());
        if (Sessions.Active.has_value())
        {
            return FStepOutcome::Fail(TEXT("the core still holds an open lesson"));
        }
        const std::string Id = TCHAR_TO_UTF8(*Run->BindingId);
        for (const PerfectWorld::SchoolPeriodRecord& Record : Sessions.Periods)
        {
            if (Record.BindingId == Id)
            {
                if (Record.Completed != Run->bReady || Record.Cancelled == Run->bReady)
                {
                    return FStepOutcome::Fail(TEXT("the period record does not match how the lesson was closed"));
                }
                return FStepOutcome::Pass(FString::Printf(TEXT("the core's SchoolSessions holds the %s period for %s (mode foreground)"),
                    Record.Completed ? TEXT("completed") : TEXT("cancelled"), *Run->BindingId));
            }
        }
        return FStepOutcome::Fail(TEXT("the core's SchoolSessions holds no period record for the lesson"));
    }));
}
} // namespace PWAcceptance
