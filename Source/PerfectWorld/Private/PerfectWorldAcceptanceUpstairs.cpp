#include "PerfectWorldLaneTallFlats.h"

#include "PerfectWorldAcceptanceSubsystem.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldLaneLifePersistence.h"
#include "PerfectWorldPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

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

void T6Visit(TArray<T6StepDef>& Out, const FString& Prefix)
{
    // From the top landing, through the planned but unhung upper doorway, then through the internal doorway.
    Out.Add(T6Walk(Prefix + TEXT("_enter_sleeping"), 715, 715, 315));
    Out.Add(T6Walk(Prefix + TEXT("_sleeping_column"), 715, 500, 315));
    Out.Add(T6Walk(Prefix + TEXT("_internal_approach"), 965, 500, 315));
    Out.Add(T6Walk(Prefix + TEXT("_enter_living"), 965, 300, 315));
    Out.Add(PWAcceptance::MakeShotStep(Prefix + TEXT("_shot_living")));
    Out.Add(T6Walk(Prefix + TEXT("_return_sleeping"), 965, 500, 315));
    Out.Add(T6Walk(Prefix + TEXT("_return_column"), 715, 500, 315));
    Out.Add(T6Walk(Prefix + TEXT("_return_door"), 715, 715, 315));
    Out.Add(T6Walk(Prefix + TEXT("_return_landing"), 550, 715, 315));
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
    // Bring a freshly continued pawn out of the dwelling and down, before the two complete camera walks.
    for (auto Def : {T6Walk(TEXT("continue_to_landing"), 550, 715, 315), T6Walk(TEXT("continue_down"), 550, 205, 15)})
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
        T6Visit(Out, Prefix);
        Out.Add(T6Walk(Prefix + TEXT("_down_lower"), 550, 355, 15 + 5 * 300.0 / 17, 40));
        Out.Add(MakeShotStep(Prefix + TEXT("_shot_descent")));
        Out.Add(T6Walk(Prefix + TEXT("_down_foot"), 550, 205, 15));
        Out.Add(MakeShotStep(Prefix + TEXT("_shot_returned")));
    }
    Out.Add(T6Walk(TEXT("save_walk_up"), 550, 715, 315));
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
