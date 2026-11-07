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
    TArray<FVector> ToSeat;
    ToSeat.Add(FVector(P.GateCentre.X, P.GateCentre.Y + 1000.0, ScAFloor));
    ToSeat.Add(FVector(P.ClassroomDoor.X, P.ClassroomDoor.Y - 150.0, ScAFloor));
    ToSeat.Add(FVector(P.ClassroomDoor.X, P.ClassroomDoor.Y - 40.0, ScAFloor));
    ToSeat.Add(FVector(P.ClassroomDoor.X, P.ClassroomDoor.Y + 60.0, ScAFloor));
    ToSeat.Add(P.ClassroomAisle);
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

    // Open a lesson from the panel. Routed: a foreground lesson opens and the lesson screen takes over. Unrouted: the core path refuses in words
    // and changes nothing, then the lesson screen is opened read-only for the period (Back leaves it).
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
        if (PWSchoolLive::CommandsRouted())
        {
            if (!Day->RequestOpen(PerfectWorld::SchoolLessonMode::Foreground))
            {
                return FStepOutcome::Fail(TEXT("the core refused the foreground lesson: ") + Day->GetNotice().ToString());
            }
            Run->bLessonScreen = S03LessonScreen(World).IsValid();
            return Run->bLessonScreen ? FStepOutcome::Pass(TEXT("a foreground lesson opened and the lesson screen took over")) : FStepOutcome::Fail(TEXT("the lesson screen did not open"));
        }
        const PerfectWorld::GameInstant Before = Life->GetNow();
        if (Day->RequestOpen(PerfectWorld::SchoolLessonMode::Attend))
        {
            return FStepOutcome::Fail(TEXT("the open was accepted although school.lesson.open is not routed"));
        }
        if (Day->GetNotice().IsEmpty() || Life->GetNow() != Before)
        {
            return FStepOutcome::Fail(TEXT("the refusal had no words, or the clock moved"));
        }
        if (!Presenter->OpenLesson(Run->SelectedId) || !S03LessonScreen(World).IsValid())
        {
            return FStepOutcome::Fail(TEXT("the read-only lesson screen did not open for the chosen period"));
        }
        Run->bLessonScreen = true;
        return FStepOutcome::Pass(TEXT("UNWIRED: open refused in words (") + Day->GetNotice().ToString() + TEXT("); the lesson screen opened read-only"));
    }));
    Out.Add(MakeShotStep(TEXT("shot_lesson_screen")));

    // One choice. Routed: a real structure decision; unrouted: the start is refused with the unwired code and nothing moves.
    Out.Add(S03Action(TEXT("make_one_lesson_choice"), 15.0, [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        UWorld* World = Sub.GetPlayWorld();
        const TSharedPtr<SPWClassLesson> Lesson = S03LessonScreen(World);
        const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(World);
        if (!Lesson.IsValid() || Life == nullptr)
        {
            return FStepOutcome::Fail(TEXT("no lesson screen"));
        }
        const PerfectWorld::GameInstant Before = Life->GetNow();
        if (PWSchoolLive::CommandsRouted())
        {
            if (!Lesson->GetFrame().bHasRun && !Lesson->ExecuteOpen(PerfectWorld::SchoolLessonMode::Foreground).Ok())
            {
                return FStepOutcome::Fail(TEXT("the lesson would not start: ") + Lesson->GetLastIssueCode());
            }
            for (const FString& Key : Lesson->GetActionKeys())
            {
                if (Lesson->IsActionEnabled(Key) && !Key.Contains(TEXT("back")) && !Key.Contains(TEXT("leave")) && Lesson->PressAction(Key))
                {
                    return Lesson->GetLastResult().Ok() ? FStepOutcome::Pass(TEXT("one real lesson choice accepted: ") + Key) : FStepOutcome::Fail(TEXT("the core refused the choice: ") + Lesson->GetLastIssueCode());
                }
            }
            return FStepOutcome::Fail(TEXT("the lesson screen offered no enabled choice"));
        }
        const PerfectWorld::SchoolSessionResult Result = Lesson->ExecuteOpen(PerfectWorld::SchoolLessonMode::Attend);
        if (Result.Ok() || Lesson->GetLastIssueCode() != PWSchoolLive::UnwiredCode || Life->GetNow() != Before)
        {
            return FStepOutcome::Fail(FString::Printf(TEXT("expected the unwired refusal with no time moved; got code '%s'"), *Lesson->GetLastIssueCode()));
        }
        return FStepOutcome::Pass(TEXT("UNWIRED: the lesson start was refused (") + Lesson->GetLastIssueCode() + TEXT("); no choice could be made and no time moved"));
    }));

    // Close: Back leaves the lesson screen (to the day panel), then the panel closes; input and the clock come back.
    Out.Add(S03Action(TEXT("close_the_lesson_and_the_panel"), 15.0, [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
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

    // What the core recorded.
    Out.Add(S03Action(TEXT("core_recorded_the_lesson"), 10.0, [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (Life == nullptr)
        {
            return FStepOutcome::Fail(TEXT("no lane-life subsystem"));
        }
        const PerfectWorld::SchoolSessionsState& Sessions = PWSchoolLive::SessionsOf(Life->GetWorldState());
        if (PWSchoolLive::CommandsRouted())
        {
            const bool bRecorded = !Sessions.Periods.empty() && Sessions.Periods.back().BindingId == std::string(TCHAR_TO_UTF8(*Run->SelectedId));
            return bRecorded ? FStepOutcome::Pass(TEXT("the core's SchoolSessions holds the lesson's period record"))
                             : FStepOutcome::Fail(TEXT("the core's SchoolSessions holds no record for the lesson"));
        }
        const bool bUnchanged = Sessions.Periods.empty() && !Sessions.Active.has_value() && !Life->GetWorldState().DayLoop.Attendance.Days.empty();
        return bUnchanged ? FStepOutcome::Pass(TEXT("UNWIRED: the core recorded the school day's arrival and no lesson (none could be started); nothing was fabricated"))
                          : FStepOutcome::Fail(TEXT("UNWIRED: unexpected school state"));
    }));
}
} // namespace PWAcceptance
