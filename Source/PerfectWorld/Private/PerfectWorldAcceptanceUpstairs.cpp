#include "PerfectWorldLaneTallFlats.h"

#include "Doors.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PerfectWorldAcceptanceHomes.h"
#include "PerfectWorldAcceptanceDoorBell.h"
#include "PerfectWorldAcceptanceSubsystem.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneLifePersistence.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldResidentsR5Bindings.h"
#include "GameFramework/CharacterMovementComponent.h"

// tall_flats_upstairs: the public stair, the top landing and the furnished upper dwelling, in both cameras, then a save inside the
// dwelling for a fresh-process Continue.
//
// R6-03: since R5-02 the upper dwelling's side opening is hung with the private leaf door.nabapara.tall_flats_upper_a, and its
// resident (Ishita Banerjee) receives visitors 16:00-19:30 only when she is home. The old unhung walk-in is therefore gone, and nothing
// here bypasses the leaf. Each visit is a lawful one through the production interaction path:
//   1. a declared clock setup (the same detached reducer advance resident_homes and homes_round5 use) to the next 16:30, when her
//      authored schedule has her home and awake on every day of the week;
//   2. E on the closed leaf: the knock, which must be answered ("Someone answers"), and the leaf must stay shut;
//   3. E again: the admitted knock opens the leaf, which must finish its sweep;
//   4. the original walk through the sleeping room and the internal doorway, unchanged, with all collision intact;
//   5. the leaf is shut again from the landing, so the next visit starts from the closed state.
// The very first arrival (new-life phase only, Monday morning, Ishita at work) also knocks once and requires "Nobody answers" with the
// leaf still shut, proving the closed leaf is a real barrier before any lawful setup.

namespace
{
using T6StepDef = PWAcceptance::FStepDef;
using T6StepOutcome = PWAcceptance::FStepOutcome;
using T6StepRuntime = PWAcceptance::FStepRuntime;

struct T6Restart
{
    bool Continue = false;
    PerfectWorld::PlayerPlacement Saved;
};

FVector T6World(double U, double V, double Feet)
{
    return FVector(-1300 + U, 460 + V, PWLane::GroundZ + Feet);
}

T6StepDef T6Action(const FString& Name, TFunction<T6StepOutcome(UPWAcceptanceSubsystem&, T6StepRuntime&)> Run)
{
    T6StepDef Def;
    Def.Name = Name;
    Def.TimeoutSeconds = 15;
    Def.Run = MoveTemp(Run);
    return Def;
}

// TickWalk drives real movement. Every waypoint additionally requires the correct floor height:
// an XY success on the ground below the upper dwelling can never pass this scenario.
T6StepDef T6Walk(const FString& Name, double U, double V, double Feet, double HeightTolerance = 4)
{
    T6StepDef Def;
    Def.Name = Name;
    Def.TimeoutSeconds = 20;
    Def.Run = [U, V, Feet, HeightTolerance](UPWAcceptanceSubsystem& Sub, T6StepRuntime& Rt)
    {
        const auto* Pawn = Sub.GetPawn();
        if (!Pawn) return T6StepOutcome::Fail(TEXT("no pawn"));
        const double Height = Pawn->GetActorLocation().Z - PWPlayer::CapsuleHalfHeight - PWLane::GroundZ;
        if (Height < 12 || Height > 360) return T6StepOutcome::Fail(TEXT("feet outside stair floor envelope"));
        const auto Outcome = Sub.TickWalk(T6World(U, V, Feet), 8, Rt);
        if (Outcome.State != PWAcceptance::EStepState::Passed) return Outcome;
        if (!Pawn->GetCharacterMovement()->IsMovingOnGround() || FMath::Abs(Height - Feet) > HeightTolerance)
        {
            Rt.Note = FString::Printf(TEXT("XY reached; waiting for supported feet %.1f (wanted %.1f)"), Height, Feet);
            return T6StepOutcome::Wait();
        }
        return T6StepOutcome::Pass(FString::Printf(TEXT("walked to U%.0f V%.0f; supported feet %.1f cm"), U, V, Height));
    };
    return Def;
}

APWDoorActor* T6FindUpperDoor(UWorld* World)
{
    for (TActorIterator<APWDoorActor> It(World); It; ++It)
    {
        if (It->GetDoorId() == FName(PWLane::R52UpperDoor)) return *It;
    }
    return nullptr;
}

// A step that only the new-life process runs; the Continue process passes it, so both processes plan the same step names.
T6StepDef T6NewPhaseOnly(T6StepDef Def, TSharedRef<T6Restart> State)
{
    auto Run = MoveTemp(Def.Run);
    Def.Run = [Run = MoveTemp(Run), State](UPWAcceptanceSubsystem& Sub, T6StepRuntime& Rt)
    {
        return State->Continue ? T6StepOutcome::Pass(TEXT("new-life-phase only; the Continue process starts later in the week")) : Run(Sub, Rt);
    };
    return Def;
}

// E on the closed upper leaf (the call the E key makes). bAnswered selects the expected answer: "Someone answers" with a present host,
// "Nobody answers" otherwise. The core's own host truth must agree with the words and the leaf must still be shut either way.
T6StepDef T6Knock(const FString& Name, bool bAnswered)
{
    // F5: a visitor rings the bell beside the leaf; pressing the leaf itself only says "Ring the bell first."
    return PWAcceptance::MakeInteractStep(Name,
        [](UPWAcceptanceSubsystem& Sub) -> AActor* { return PWAcceptance::DoorBell::FindBell(Sub.GetPlayWorld(), FName(PWLane::R52UpperDoor)); },
        [bAnswered](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
            const auto* Door = T6FindUpperDoor(Sub.GetPlayWorld());
            const auto* Def = PerfectWorld::FindDoor(TCHAR_TO_UTF8(PWLane::R52UpperDoor));
            if (!Life || !Door || !Def || !Sub.GetInteraction()) return false;
            Detail = Sub.GetInteraction()->GetNotice().ToString();
            return !Door->IsOpen() && Life->IsHostHomeForAcceptance(*Def) == bAnswered &&
                (bAnswered ? PWAcceptance::DoorBell::IsAnsweredNotice(Detail) : Detail.Contains(TEXT("Nobody answers")));
        }, nullptr, 15);
}

// Declared setup: the next 16:30 (today's if still ahead, else tomorrow's) through the detached reducer advance that resident_homes
// uses; it refuses unless the world is at rest. Ishita's schedule has her home and awake at 16:30 on every day of the week.
T6StepDef T6VisitingClock(const FString& Name)
{
    T6StepDef Def;
    Def.Name = Name;
    Def.TimeoutSeconds = 15;
    Def.Run = [Name](UPWAcceptanceSubsystem& Sub, T6StepRuntime& Rt)
    {
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!Life) return T6StepOutcome::Fail(TEXT("no lane life for the visiting clock"));
        // Instant 0 is Monday 2 June 2042 06:00; HmInstant takes days after 2 June.
        int32 Day = static_cast<int32>((Life->GetNow() + 360) / 1440);
        if (PWAcceptance::Homes::HmInstant(Day, 16, 30) <= Life->GetNow()) ++Day;
        auto Moved = PWAcceptance::Homes::HmClockStep(Name, Day, 16, 30);
        return Moved.Run(Sub, Rt);
    };
    return Def;
}

void T6Visit(TArray<T6StepDef>& Out, const FString& Prefix, const TSharedRef<T6Restart>& State, bool bRefusalFirst)
{
    if (bRefusalFirst)
    {
        // Morning of the first day: Ishita is at work. The closed leaf must stay closed and the knock go unanswered.
        Out.Add(T6NewPhaseOnly(T6Knock(Prefix + TEXT("_knock_unanswered"), false), State));
    }
    Out.Add(T6VisitingClock(Prefix + TEXT("_visiting_clock")));
    Out.Add(T6Knock(Prefix + TEXT("_knock_answered"), true));
    Out.Add(PWAcceptance::Homes::HmDoorStep(Prefix + TEXT("_admitted_open"), PWLane::R52UpperDoor, true,
        TEXT("the admitted knock opened the upper leaf through the door interaction")));
    // From the top landing, through the upper leaf doorway, then through the internal doorway.
    Out.Add(T6Walk(Prefix + TEXT("_enter_sleeping"), 715, 715, 315));
    Out.Add(T6Walk(Prefix + TEXT("_sleeping_column"), 715, 500, 315));
    Out.Add(T6Walk(Prefix + TEXT("_internal_approach"), 965, 500, 315));
    Out.Add(T6Walk(Prefix + TEXT("_enter_living"), 965, 300, 315));
    Out.Add(PWAcceptance::MakeShotStep(Prefix + TEXT("_shot_living")));
    Out.Add(T6Walk(Prefix + TEXT("_return_sleeping"), 965, 500, 315));
    Out.Add(T6Walk(Prefix + TEXT("_return_column"), 715, 500, 315));
    Out.Add(T6Walk(Prefix + TEXT("_return_door"), 715, 715, 315));
    Out.Add(T6Walk(Prefix + TEXT("_return_landing"), 550, 715, 315));
    // Shut the leaf from outside (closing never needs admission) so the next visit starts from a closed, private door.
    Out.Add(PWAcceptance::Homes::HmDoorStep(Prefix + TEXT("_shut_upper"), PWLane::R52UpperDoor, false,
        TEXT("the upper leaf was shut from the landing")));
}
} // namespace

namespace PWAcceptance
{
void AppendTallFlatsUpstairsSteps(TArray<T6StepDef>& Out)
{
    const auto State = MakeShared<T6Restart>();
    Out.Add(MakeWorldReadyStep());
    Out.Add(T6Action(TEXT("upstairs_phase"), [State](UPWAcceptanceSubsystem& Sub, T6StepRuntime&)
    {
        State->Continue = Sub.GetArgs().Start == TEXT("continue") || (Sub.GetArgs().Start == TEXT("auto") && UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot() >= 0);
        if (State->Continue)
        {
            const int32 Slot = Sub.GetArgs().Slot >= 0 ? Sub.GetArgs().Slot : UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot();
            if (Slot < 0) return T6StepOutcome::Fail(TEXT("Continue phase has no slot"));
            const auto Saved = PWLaneLifePersistence::ReadWorldSlot(TCHAR_TO_UTF8(*UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotPath(Slot)));
            if (!Saved.Succeeded()) return T6StepOutcome::Fail(TEXT("cannot read upstairs saved placement"));
            State->Saved = Saved.Data->Player;
            const auto& P = State->Saved.PositionCm;
            const FVector Position(static_cast<double>(P[0]), static_cast<double>(P[1]), static_cast<double>(P[2]));
            if (FVector::Dist(Position, T6World(715, 715, 315 + PWPlayer::CapsuleHalfHeight)) > 10)
                return T6StepOutcome::Fail(TEXT("Continue slot was not saved at the upstairs scenario endpoint"));
        }
        return T6StepOutcome::Pass(State->Continue ? TEXT("fresh-process Continue phase; expected upstairs placement read before loading") : TEXT("new walk phase"));
    }));
    Out.Add(MakeEnterWorldStep());
    Out.Add(T6Action(TEXT("upstairs_start_or_resume"), [State](UPWAcceptanceSubsystem& Sub, T6StepRuntime& Rt)
    {
        auto* Pawn = Sub.GetPawn();
        if (!Pawn) return T6StepOutcome::Fail(TEXT("no pawn"));
        if (State->Continue)
        {
            const auto& P = State->Saved.PositionCm;
            const FVector Expected(static_cast<double>(P[0]), static_cast<double>(P[1]), static_cast<double>(P[2]));
            if (FVector::Dist(Pawn->GetActorLocation(), Expected) > 5)
                return T6StepOutcome::Fail(TEXT("Continue did not restore the saved upper-floor position"));
            const auto ExpectedCamera = State->Saved.Camera == PerfectWorld::CameraMode::FirstPerson
                ? EPWCameraMode::FirstPerson : EPWCameraMode::ThirdPerson;
            if (Pawn->GetCameraMode() != ExpectedCamera)
                return T6StepOutcome::Fail(TEXT("Continue did not restore the saved camera"));
        }
        else if (Rt.Phase == 0)
        {
            // The only teleport: a declared start at the stair foot inside the shared passage.
            if (!Pawn->TeleportTo(T6World(550, 205, 15 + PWPlayer::CapsuleHalfHeight + 2), FRotator(0, 90, 0)))
                return T6StepOutcome::Fail(TEXT("declared stair-foot placement refused"));
            Pawn->GetCharacterMovement()->Velocity = FVector::ZeroVector;
            Rt.NextPhase();
        }
        if (!Pawn->GetCharacterMovement()->IsMovingOnGround()) return T6StepOutcome::Wait();
        return T6StepOutcome::Pass(State->Continue ? TEXT("saved upstairs position restored without teleport or in-process reload") : TEXT("declared stair-foot start supported on ground"));
    }));
    // Bring a freshly continued pawn out of the dwelling and down, before the two complete camera walks. The slot was saved with the
    // leaf open, so it is shut from the landing first: every visit then begins with a knock on a closed private door.
    for (auto Def : {T6Walk(TEXT("continue_to_landing"), 550, 715, 315),
                     PWAcceptance::Homes::HmDoorStep(TEXT("continue_shut_upper"), PWLane::R52UpperDoor, false,
                         TEXT("the restored open upper leaf was shut from the landing")),
                     T6Walk(TEXT("continue_down"), 550, 205, 15)})
    {
        auto Run = MoveTemp(Def.Run);
        Def.Run = [Run = MoveTemp(Run), State](UPWAcceptanceSubsystem& Sub, T6StepRuntime& Rt)
        {
            return State->Continue ? Run(Sub, Rt) : T6StepOutcome::Pass(TEXT("Continue-only descent; new phase starts at foot"));
        };
        Out.Add(MoveTemp(Def));
    }
    for (const auto Mode : {EPWCameraMode::FirstPerson, EPWCameraMode::ThirdPerson})
    {
        const FString Prefix = Mode == EPWCameraMode::FirstPerson ? TEXT("first") : TEXT("third");
        Out.Add(T6Action(Prefix + TEXT("_camera"), [Mode](UPWAcceptanceSubsystem& Sub, T6StepRuntime&)
        {
            auto* Pawn = Sub.GetPawn();
            if (!Pawn) return T6StepOutcome::Fail(TEXT("no pawn"));
            Pawn->SetCameraMode(Mode);
            return Pawn->GetCameraMode() == Mode ? T6StepOutcome::Pass(TEXT("requested camera active")) : T6StepOutcome::Fail(TEXT("camera mode differs"));
        }));
        Out.Add(MakeShotStep(Prefix + TEXT("_shot_foot")));
        // The capsule straddles adjacent nosings here; the endpoint floor tests remain within 4 cm.
        Out.Add(T6Walk(Prefix + TEXT("_up_lower"), 550, 355, 15 + 5 * 300.0 / 17, 40));
        Out.Add(MakeShotStep(Prefix + TEXT("_shot_ascent")));
        Out.Add(T6Walk(Prefix + TEXT("_up_landing"), 550, 715, 315));
        Out.Add(MakeShotStep(Prefix + TEXT("_shot_top")));
        T6Visit(Out, Prefix, State, Mode == EPWCameraMode::FirstPerson);
        Out.Add(T6Walk(Prefix + TEXT("_down_lower"), 550, 355, 15 + 5 * 300.0 / 17, 40));
        Out.Add(MakeShotStep(Prefix + TEXT("_shot_descent")));
        Out.Add(T6Walk(Prefix + TEXT("_down_foot"), 550, 205, 15));
        Out.Add(MakeShotStep(Prefix + TEXT("_shot_returned")));
    }
    Out.Add(T6Walk(TEXT("save_walk_up"), 550, 715, 315));
    Out.Add(T6VisitingClock(TEXT("save_visiting_clock")));
    Out.Add(T6Knock(TEXT("save_knock_answered"), true));
    Out.Add(PWAcceptance::Homes::HmDoorStep(TEXT("save_admitted_open"), PWLane::R52UpperDoor, true,
        TEXT("the admitted knock opened the upper leaf through the door interaction")));
    Out.Add(T6Walk(TEXT("save_enter_upper"), 715, 715, 315));
    Out.Add(T6Action(TEXT("save_upstairs"), [](UPWAcceptanceSubsystem& Sub, T6StepRuntime&)
    {
        if (!Sub.GetHud()) return T6StepOutcome::Fail(TEXT("no HUD"));
        bool Saved = false;
        const FText Why = Sub.GetHud()->SaveActiveGame(Saved);
        return Saved ? T6StepOutcome::Pass(TEXT("upper-floor placement saved; use a second process with -PWAcceptanceStart=continue and the same private UserDir")) : T6StepOutcome::Fail(Why.ToString());
    }));
    Out.Add(MakeShotStep(TEXT("shot_saved_upstairs")));
}
} // namespace PWAcceptance
