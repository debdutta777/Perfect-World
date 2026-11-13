#include "PerfectWorldAcceptanceCrowdR7.h"

#include "Components/CapsuleComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PerfectWorldAcceptanceHomes.h"
#include "PerfectWorldContentSubsystem.h"
#include "PerfectWorldCrowdPresenter.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldPlayerCharacter.h"
#include "WorldHash.h"

namespace
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

struct FR75PScenario
{
    TWeakObjectPtr<APWCrowdPresenter> Presenter;
    TWeakObjectPtr<APWCrowdFigure> Student;
    PerfectWorld::SchoolCalendar Calendar;
    PerfectWorld::CalendarDay Day = 0;
};

FStepDef R75PAction(const FString& Name,
    TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run, const double Timeout = 20)
{
    FStepDef Def;
    Def.Name = Name;
    Def.TimeoutSeconds = Timeout;
    Def.Run = MoveTemp(Run);
    return Def;
}

FStepOutcome R75PCensus(UPWAcceptanceSubsystem& Sub, const FR75PScenario& State, const bool bInView)
{
    const auto* Presenter = State.Presenter.Get();
    const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
    if (!Presenter || !Life) return FStepOutcome::Fail(TEXT("R75P missing presenter/clock"));
    const auto& Target = Presenter->Snapshot().Target;
    if (Target.Band != PerfectWorld::CrowdTimeBand::SchoolArrival ||
        PerfectWorld::CalendarDayOfInstant(Life->GetNow()) != State.Day)
        return FStepOutcome::Fail(TEXT("R75P census left the authoritative school arrival window"));
    int32 Near = 0, Students = 0, Visible = 0;
    const auto Gate = PWCrowd::Point(Target.NearSchoolCentre);
    for (const auto& Person : Presenter->ObservedPeople())
    {
        if (FVector::DistSquared2D(Person.Feet, Gate) > FMath::Square(double(Target.NearSchoolRadiusCm))) continue;
        ++Near;
        if (Person.Cohort == PerfectWorld::CrowdCohort::Student)
        {
            ++Students;
            Visible += Presenter->IsPointVisible(Person.Feet);
        }
    }
    int32 Named = 0;
    for (TActorIterator<APWNpcCharacter> It(Sub.GetPlayWorld()); It; ++It) ++Named;
    if (Presenter->ActorCount() > 36 || Presenter->ActorCount() + Named > 48 || Presenter->FarFigureCount() > 80)
        return FStepOutcome::Fail(TEXT("R75P person actor/ISM cap exceeded"));
    const FString Detail = FString::Printf(TEXT("school day %lld %s: near=%d band=%d..%d students=%d visible_students=%d actors=%d named=%d far=%d"),
        static_cast<long long>(State.Day), *Life->GetClockText().ToString(), Near, Target.NearSchoolMinimum,
        Target.NearSchoolMaximum, Students, Visible, Presenter->ActorCount(), Named, Presenter->FarFigureCount());
    if (Near < Target.NearSchoolMinimum || Near > Target.NearSchoolMaximum || Students < 25 || Students > 35 ||
        (bInView && (Visible < 25 || Visible > 35)))
        return FStepOutcome::Fail(Detail);
    return FStepOutcome::Pass(Detail);
}

FStepDef R75PCamera(const FString& Name, const TSharedRef<FR75PScenario> State, const EPWCameraMode Mode)
{
    return R75PAction(Name, [State, Mode](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* Pawn = Sub.GetPawn();
        if (!Pawn || !State->Presenter.IsValid()) return FStepOutcome::Fail(TEXT("R75P no pawn/presenter"));
        if (Rt.Phase == 0)
        {
            Pawn->SetCameraMode(Mode);
            Sub.FaceWorldPoint(FVector(6900, 6200, 0));
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        if (Rt.InPhase() < .5) return FStepOutcome::Wait();
        if (Pawn->GetCameraMode() != Mode) return FStepOutcome::Fail(TEXT("R75P camera mode did not switch"));
        return R75PCensus(Sub, *State, true);
    });
}
}

namespace PWAcceptance
{
void AppendCrowdRound7Steps(TArray<FStepDef>& Out)
{
    const auto State = MakeShared<FR75PScenario>();
    Out.Add(MakeWorldReadyStep());
    Out.Add(MakeEnterWorldStep());
    Out.Add(R75PAction(TEXT("crowd_declared_start_at_school_gate"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* World = Sub.GetPlayWorld();
        auto* Pawn = Sub.GetPawn();
        auto* Life = UPWLaneLifeSubsystem::Get(World);
        auto* Game = World ? World->GetGameInstance() : nullptr;
        auto* Content = Game ? Game->GetSubsystem<UPWContentSubsystem>() : nullptr;
        if (!World || !Pawn || !Life || !Content || !Content->GetDb())
            return FStepOutcome::Fail(TEXT("R75P missing played world/calendar content"));
        if (Rt.Phase == 0)
        {
            int32 Presenters = 0;
            for (TActorIterator<APWCrowdPresenter> It(World); It; ++It) { State->Presenter = *It; ++Presenters; }
            if (Presenters != 1) return FStepOutcome::Fail(TEXT("R75P lane must install exactly one SpawnCrowdPresenter hook"));
            const auto Calendars = PerfectWorld::LoadSchoolCalendars(*Content->GetDb());
            const auto* Calendar = Calendars.Ok() ? Calendars.Set.Find(
                PerfectWorld::NeighbourhoodSchoolInstitutionId, "stage.higher_secondary") : nullptr;
            if (!Calendar) return FStepOutcome::Fail(TEXT("R75P authoritative calendar unavailable"));
            State->Calendar = *Calendar;
            const auto Day = PerfectWorld::CalendarSchoolDayOnOrAfter(*Calendar,
                PerfectWorld::CalendarDayOfInstant(Life->GetNow()) + 1);
            if (!Day) return FStepOutcome::Fail(TEXT("R75P no future school day in calendar"));
            State->Day = *Day;
            const FVector Start(6100, 4700, PerfectWorld::NeighbourhoodFootpathZCm +
                Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2);
            if (!Pawn->TeleportTo(Start, FRotator(0, -90, 0)))
                return FStepOutcome::Fail(TEXT("R75P declared gate start obstructed"));
            Pawn->GetCharacterMovement()->StopMovementImmediately();
            Pawn->SetCameraMode(EPWCameraMode::FirstPerson);
            Sub.FaceWorldPoint(FVector(6500, 2000, 0)); // campus remains out of view during reconstruction
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        if (Rt.InPhase() < .8 || !Pawn->GetCharacterMovement()->IsMovingOnGround()) return FStepOutcome::Wait();
        const double Feet = Pawn->GetActorLocation().Z - Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        if (FMath::Abs(Feet - PerfectWorld::NeighbourhoodFootpathZCm) > 5)
            return FStepOutcome::Fail(TEXT("R75P school approach lacks plan-height collision; apply R71/R73 hooks"));
        return FStepOutcome::Pass(TEXT("single declared teleport; grounded at gate, looking away for time reconciliation"));
    }));
    Out.Add(R75PAction(TEXT("crowd_schoolday_0940_census"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto Def = Homes::HmClockStep(TEXT("R75P_clock"), static_cast<int32>(State->Day), 9, 40); // bell_schedule: gates 09:30, bell 10:00
        const auto Result = Def.Run(Sub, Rt);
        if (Result.State != EStepState::Passed) return Result;
        State->Presenter->RefreshNow();
        return R75PCensus(Sub, *State, false);
    }));
    Out.Add(R75PCamera(TEXT("crowd_first_person_visible_band"), State, EPWCameraMode::FirstPerson));
    Out.Add(MakeShotStep(TEXT("crowd_schoolgate_first")));
    Out.Add(R75PCamera(TEXT("crowd_third_person_visible_band"), State, EPWCameraMode::ThirdPerson));
    Out.Add(MakeShotStep(TEXT("crowd_schoolgate_third")));
    Out.Add(R75PAction(TEXT("crowd_walk_to_anonymous_student"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* Pawn = Sub.GetPawn();
        auto* Presenter = State->Presenter.Get();
        if (!Pawn || !Presenter) return FStepOutcome::Fail(TEXT("R75P no pawn/presenter"));
        if (!State->Student.IsValid()) State->Student = Presenter->NearestStudent(Pawn->GetActorLocation());
        const auto* Student = State->Student.Get();
        if (!Student) return FStepOutcome::Fail(TEXT("R75P no nearby student actor for Talk"));
        FVector Toward = Student->Feet() - Pawn->GetActorLocation();
        Toward.Z = 0;
        const auto Stand = Student->Feet() - Toward.GetSafeNormal() * 125;
        const auto Walk = Sub.TickWalk(Stand, 25, Rt);
        Sub.FaceWorldPoint(Student->GetInteractionPoint());
        return Walk;
    }, 30));
    Out.Add(R75PAction(TEXT("crowd_talk_records_nothing"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* Student = State->Student.Get();
        auto* Interaction = Sub.GetInteraction();
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!Student || !Interaction || !Life) return FStepOutcome::Fail(TEXT("R75P lost student/interaction/world"));
        Sub.FaceWorldPoint(Student->GetInteractionPoint());
        if (Interaction->GetTarget() != Student || !Interaction->IsPromptVisible())
        {
            Interaction->CycleTarget();
            Rt.Note = TEXT("waiting for real interaction targeting to select the student");
            return FStepOutcome::Wait();
        }
        const auto Before = PerfectWorld::HashWorldState(Life->GetWorldState());
        if (Interaction->GetPromptVerb().ToString() != TEXT("Talk") || !Interaction->TryInteract())
            return FStepOutcome::Fail(TEXT("R75P real Talk input refused"));
        if (!Student->IsTalkHeld() || !Interaction->IsNoticeVisible() ||
            !Interaction->GetNotice().EqualTo(PWCrowd::ExchangeLine(Student->Person().ShortExchangeTextKey)))
            return FStepOutcome::Fail(TEXT("R75P model's short exchange notice was not shown"));
        if (PerfectWorld::HashWorldState(Life->GetWorldState()) != Before)
            return FStepOutcome::Fail(TEXT("R75P anonymous Talk mutated the authoritative world"));
        return FStepOutcome::Pass(TEXT("real Talk showed model text; world hash unchanged, including journal/conversations/RNG/clock"));
    }, 10));
    Out.Add(MakeShotStep(TEXT("crowd_short_exchange")));
}
}
