#include "PerfectWorldAcceptanceTrafficR7.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PerfectWorldAcceptanceStationery.h"
#include "PerfectWorldAcceptanceSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldTrafficPresenter.h"
#include "PerfectWorldTrafficVehicles.h"

namespace
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

APWTrafficPresenter* R72PAccBridge(UPWAcceptanceSubsystem& Sub)
{
    if (auto* World = Sub.GetPlayWorld())
        for (TActorIterator<APWTrafficPresenter> It(World); It; ++It) return *It;
    return nullptr;
}

bool R72PCrossPoint(UPWAcceptanceSubsystem& Sub, bool bEast, FVector& Out)
{
    const auto* Bridge = R72PAccBridge(Sub);
    const auto* Node = Bridge ? PerfectWorld::FindNeighbourhoodNode(Bridge->GetPlan(),
        bEast ? "walk.nabapara.main_cross_east" : "walk.nabapara.main_cross_west") : nullptr;
    if (!Node) return false;
    Out = FVector(Node->Position.X, Node->Position.Y, Node->Position.Z);
    return true;
}

FStepDef R72PAction(const FString& Name, double Timeout,
    TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run)
{
    FStepDef Step;
    Step.Name = Name; Step.TimeoutSeconds = Timeout; Step.Run = MoveTemp(Run);
    return Step;
}

FStepOutcome R72PSafety(UPWAcceptanceSubsystem& Sub)
{
    const auto* Bridge = R72PAccBridge(Sub);
    const auto* Pawn = Sub.GetPawn();
    if (!Bridge || !Pawn) return FStepOutcome::Fail(TEXT("missing production traffic hook or player"));
    if (Bridge->GetVehicleCount() > 12) return FStepOutcome::Fail(TEXT("presented actor cap exceeded"));
    const auto* Capsule = Pawn->GetCapsuleComponent();
    const FVector Centre = Capsule->GetComponentLocation();
    const double Radius = Capsule->GetScaledCapsuleRadius();
    for (const auto& Slot : Bridge->GetSlots()) if (const auto* Vehicle = Slot.Actor.Get())
    {
        const auto& B = Vehicle->GetBody()->Bounds;
        if (FMath::Abs(Centre.Z - B.Origin.Z) > Capsule->GetScaledCapsuleHalfHeight() + B.BoxExtent.Z) continue;
        const double Dx = FMath::Max(0.0, FMath::Abs(Centre.X - B.Origin.X) - B.BoxExtent.X);
        const double Dy = FMath::Max(0.0, FMath::Abs(Centre.Y - B.Origin.Y) - B.BoxExtent.Y);
        if (Dx * Dx + Dy * Dy < Radius * Radius - 0.01)
            return FStepOutcome::Fail(TEXT("vehicle body overlaps the walking player capsule"));
    }
    return FStepOutcome::Pass(TEXT("cap and capsule separation observed"));
}

int32 R72PHeldVehicles(const APWTrafficPresenter& Bridge)
{
    int32 Count = 0;
    for (const auto& Slot : Bridge.GetSlots()) if (const auto* Vehicle = Slot.Actor.Get())
    {
        if (Slot.Target.LaneId.find("street.nabapara.main.") != 0 ||
            Slot.Target.State == PerfectWorld::NeighbourhoodVehicleState::Moving) continue;
        if (FMath::Abs(Vehicle->GetActorLocation().Y - 5200) < 3000 && Slot.From.Equals(Slot.To, 0.01)) ++Count;
    }
    return Count;
}

FStepDef R72PWaitWalk(const FString& Name)
{
    return R72PAction(Name, 200, [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        const auto Safety = R72PSafety(Sub);
        if (Safety.State == PWAcceptance::EStepState::Failed) return Safety;
        auto* Bridge = R72PAccBridge(Sub);
        const auto Phase = Bridge->CrossingPhase("crossing.nabapara.main");
        // Observe Drive first, then the beginning of Walk; leave enough time for a full crossing.
        if (Phase == PerfectWorld::NeighbourhoodCrossingPhase::Drive) Rt.Phase = 1;
        if (Rt.Phase == 1 && Phase == PerfectWorld::NeighbourhoodCrossingPhase::Walk)
        {
            const int32 Held = R72PHeldVehicles(*Bridge);
            if (!Bridge->IsCrossingClear("crossing.nabapara.main"))
                return FStepOutcome::Fail(TEXT("vehicle occupies zebra crossing during Walk"));
            if (Held > 0) return FStepOutcome::Pass(FString::Printf(TEXT("fresh Walk; %d main-road vehicles hold; crossing clear"), Held));
            Rt.Phase = 0; // no vacuous empty-road success; observe another complete phase
        }
        Rt.Note = TEXT("waiting for a fresh Walk with a main-road vehicle visibly held");
        return FStepOutcome::Wait();
    });
}

FStepDef R72PCross(const FString& Name, bool bEast)
{
    return R72PAction(Name, 20, [bEast](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        const auto Safety = R72PSafety(Sub);
        if (Safety.State == PWAcceptance::EStepState::Failed) return Safety;
        auto* Bridge = R72PAccBridge(Sub);
        if (Bridge->CrossingPhase("crossing.nabapara.main") != PerfectWorld::NeighbourhoodCrossingPhase::Walk)
            return FStepOutcome::Fail(TEXT("Walk ended before the pawn reached the opposite footpath"));
        if (!Bridge->IsCrossingClear("crossing.nabapara.main"))
            return FStepOutcome::Fail(TEXT("vehicle entered the occupied zebra crossing"));
        FVector Target;
        if (!R72PCrossPoint(Sub, bEast, Target)) return FStepOutcome::Fail(TEXT("authored crossing node missing"));
        return Sub.TickWalk(Target, 12, Rt);
    });
}

FStepDef R72PShot(const FString& Name)
{
    return R72PAction(Name, 15, [Name](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        const auto Safety = R72PSafety(Sub);
        if (Safety.State == PWAcceptance::EStepState::Failed) return Safety;
        const auto* Bridge = R72PAccBridge(Sub);
        if (Bridge->CrossingPhase("crossing.nabapara.main") != PerfectWorld::NeighbourhoodCrossingPhase::Walk ||
            !Bridge->IsCrossingClear("crossing.nabapara.main") || R72PHeldVehicles(*Bridge) == 0)
            return FStepOutcome::Fail(TEXT("holding screenshot requires Walk, a clear crossing and stopped main-road traffic"));
        if (Rt.Phase == 0)
        {
            for (const auto& Slot : Bridge->GetSlots()) if (const auto* Vehicle = Slot.Actor.Get())
                if (Slot.Target.LaneId.find("street.nabapara.main.") == 0 &&
                    Slot.Target.State != PerfectWorld::NeighbourhoodVehicleState::Moving &&
                    FMath::Abs(Vehicle->GetActorLocation().Y - 5200) < 3000 && Slot.From.Equals(Slot.To, 0.01))
                {
                    Sub.FaceWorldPoint(Vehicle->GetActorLocation());
                    break;
                }
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        if (Rt.Phase == 1)
        {
            if (Rt.InPhase() < 0.15) return FStepOutcome::Wait();
            Sub.RequestShot(Name, Rt); Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        return Sub.TickShot(Rt);
    });
}
}

namespace PWAcceptance
{
void AppendTrafficRound7Steps(TArray<FStepDef>& Out)
{
    Out.Add(MakeWorldReadyStep());
    Out.Add(MakeEnterWorldStep());
    Out.Add(Stationery::JumpClockStep(TEXT("traffic_daytime"), 8, 0));
    Out.Add(R72PAction(TEXT("traffic_declared_start"), 10, [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* Pawn = Sub.GetPawn();
        FVector Start;
        if (!Pawn || !R72PCrossPoint(Sub, false, Start)) return FStepOutcome::Fail(TEXT("traffic presenter/crossing not integrated"));
        if (Rt.Phase == 0)
        {
            Pawn->SetCameraMode(EPWCameraMode::FirstPerson);
            Start.Z += Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 3;
            if (!Pawn->TeleportTo(Start, FRotator(0, 0, 0))) return FStepOutcome::Fail(TEXT("declared west footpath start blocked"));
            Pawn->GetCharacterMovement()->StopMovementImmediately();
            Rt.NextPhase();
        }
        if (Rt.InPhase() < 0.8 || !Pawn->GetCharacterMovement()->IsMovingOnGround()) return FStepOutcome::Wait();
        return R72PSafety(Sub);
    }));
    Out.Add(R72PWaitWalk(TEXT("traffic_first_wait_walk")));
    Out.Add(R72PShot(TEXT("traffic_first_held")));
    Out.Add(R72PWaitWalk(TEXT("traffic_first_cross_walk")));
    Out.Add(R72PCross(TEXT("traffic_first_cross"), true));
    Out.Add(R72PAction(TEXT("traffic_third_camera"), 5, [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (!Sub.GetPawn()) return FStepOutcome::Fail(TEXT("player missing"));
        Sub.GetPawn()->SetCameraMode(EPWCameraMode::ThirdPerson);
        return FStepOutcome::Pass(TEXT("third-person return crossing; no further teleport"));
    }));
    Out.Add(R72PWaitWalk(TEXT("traffic_third_wait_walk")));
    Out.Add(R72PShot(TEXT("traffic_third_held")));
    Out.Add(R72PWaitWalk(TEXT("traffic_third_cross_walk")));
    Out.Add(R72PCross(TEXT("traffic_third_cross"), false));
}
}
