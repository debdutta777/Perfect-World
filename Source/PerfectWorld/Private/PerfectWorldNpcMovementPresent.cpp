#include "PerfectWorldNpcMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NpcBehaviour.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldPresentationSettings.h"
#include "PerfectWorldVendorCart.h"

namespace
{
/** The intent that is suspended behind a Chat or Yield, else the intent itself. */
EPWNpcState NpcPresentUnderlying(const FPWNpcIntentView& I)
{
    return (I.State == EPWNpcState::Chat || I.State == EPWNpcState::Yield) ? I.ResumeState : I.State;
}

double NpcPresentYawToward(const FVector& From, const FVector& To)
{
    return FMath::RadiansToDegrees(FMath::Atan2(To.Y - From.Y, To.X - From.X));
}
} // namespace

// --- component: presenting ----------------------------------------------------------------------------------------

void UPWNpcMovementComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    TickPresentation(DeltaTime);
}

void UPWNpcMovementComponent::TickPresentation(const float DeltaSeconds)
{
    const double Dt = FMath::Clamp<double>(DeltaSeconds, 0.0, 0.25);
    ClockSeconds += Dt;
    DecisionAccumulator += Dt;
    if (DecisionAccumulator >= PWNpcMove::DecisionIntervalSeconds - 1e-6)
    {
        const double Elapsed = DecisionAccumulator;
        DecisionAccumulator = 0.0;
        Decide(Elapsed);
    }
    UpdateMotion(static_cast<float>(Dt));
    UpdatePose(static_cast<float>(Dt));
}

void UPWNpcMovementComponent::SetBlockedState(const bool bPersistent)
{
    bPersistentlyBlocked = bPersistent;
    OnBlockChanged.Broadcast(bPersistent, Intent.RouteId);
}

/** 10 Hz: settings, distance and visibility, the avoidance sweep, block timers and the one teleport rule. */
void UPWNpcMovementComponent::Decide(const double DecisionSeconds)
{
    AActor* Owner = GetOwner();
    if (Owner == nullptr)
    {
        return;
    }
    bReduced = UPerfectWorldPresentationSettings::Get().IsReducedMotion();
    const APawn* Player = LocalPlayer();
    bNearPlayer = Player == nullptr || FVector::DistSquared(Player->GetActorLocation(), Owner->GetActorLocation()) <= FMath::Square(PWNpcMove::PoseNearRangeCm);
    bRecentlyRendered = Owner->WasRecentlyRendered(0.5f);
    if (bLocalChat)
    {
        const UPWConversationPresenter* Presenter = UPWConversationPresenter::Get(this);
        if (Presenter != nullptr && bChatPanelSeen && !Presenter->IsOpen())
        {
            SetLocalConversationActive(false); // the panel closed: resume the supplied intent
        }
    }

    // Avoidance: a capsule sweep along the way we are steering. A player in the way is waited for; anything else is
    // sidestepped if that is clear; otherwise we hold, visibly, and after four seconds tell the integrator.
    bBlockedAhead = false;
    bBlockedByPlayer = false;
    AActor* Hit = nullptr;
    if (!SteerDirection.IsNearlyZero() && SweepBlocked(SteerDirection, Hit))
    {
        bBlockedAhead = !TryAvoid(SteerDirection, Hit);
    }
    else
    {
        AvoidYawDegrees = 0.0;
    }
    if (bBlockedAhead)
    {
        BlockedSeconds += DecisionSeconds;
    }
    else
    {
        BlockedSeconds = 0.0;
        Blocker.Reset();
    }
    bLocallyYielding = BlockedSeconds >= PWNpcMove::YieldAfterSeconds;
    if (BlockedSeconds >= PWNpcMove::PersistentBlockSeconds && !bPersistentlyBlocked)
    {
        SetBlockedState(true);
    }
    else if (BlockedSeconds <= 0.0 && bPersistentlyBlocked)
    {
        SetBlockedState(false);
    }

    // The only teleport: far off and unseen. Never while blocked, never into an occupied spot.
    if (bHasIntent && GetPresentedState() != EPWNpcState::Chat)
    {
        const bool bBlocked = bBlockedAhead || bPersistentlyBlocked;
        const EPWNpcState Under = NpcPresentUnderlying(Intent);
        const FVector Feet = FeetLocation();
        if (Under == EPWNpcState::Travel && Route.IsValid())
        {
            const double Where = ExtrapolatedTargetDistance();
            const FVector Target = Route.PointAt(Where);
            if (PWNpcMove::ShouldSnap(FVector::Dist(Feet, Target), bRecentlyRendered, bBlocked, false))
            {
                TrySnap(Target, Route.DirectionAt(Where).Rotation().Yaw);
            }
        }
        else if (Under != EPWNpcState::Travel && bAnchorKnown && PWNpcMove::ShouldSnap(FVector::Dist(Feet, StandFeet), bRecentlyRendered, bBlocked, false))
        {
            TrySnap(StandFeet, StandYaw);
        }
    }
    SetTickRate();
}

bool UPWNpcMovementComponent::SweepBlocked(const FVector& Direction, AActor*& OutBlocker) const
{
    const AActor* Owner = GetOwner();
    if (Owner == nullptr || GetWorld() == nullptr)
    {
        return false;
    }
    const float Half = static_cast<float>(FMath::Max(PWNpcMove::SweepRadiusCm, HalfHeight() - 30.0)); // clears kerbs, stays under the head
    const FVector Start = Owner->GetActorLocation();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(NpcAvoid), false, Owner);
    PWLane::AmbientMoveIgnoreParams(*Owner, Params); // a vendor never avoids her own cart (R4-03)
    FHitResult Hit;
    if (!GetWorld()->SweepSingleByChannel(Hit, Start, Start + Direction.GetSafeNormal() * PWNpcMove::SweepAheadCm, FQuat::Identity, ECC_Pawn,
                                          FCollisionShape::MakeCapsule(static_cast<float>(PWNpcMove::SweepRadiusCm), Half), Params))
    {
        return false;
    }
    OutBlocker = Hit.GetActor();
    return true;
}

/** True when the way is clear after avoidance (a sidestep chosen, or the other person has to give way to us). */
bool UPWNpcMovementComponent::TryAvoid(const FVector& Direction, AActor* Hit)
{
    Blocker = Hit;
    if (Hit != nullptr && Hit == LocalPlayer())
    {
        bBlockedByPlayer = true; // yield to the player, never sidestep past them
        AvoidYawDegrees = 0.0;
        return false;
    }
    if (const APawn* Other = Cast<APawn>(Hit))
    {
        const UPWNpcMovementComponent* Mover = Other->FindComponentByClass<UPWNpcMovementComponent>();
        if (Mover != nullptr && Mover->IsMoving() && !GetOwner()->GetFName().FastLess(Other->GetFName()))
        {
            AvoidYawDegrees = 0.0;
            return true; // two walkers: the one with the lesser name gives way
        }
    }
    if (LateralCm < 90.0)
    {
        const double Sign = AvoidYawDegrees < 0.0 ? -1.0 : 1.0; // keep to the side already chosen
        for (const double Angle : {35.0, -35.0, 70.0, -70.0})
        {
            AActor* Unused = nullptr;
            if (!SweepBlocked(Direction.RotateAngleAxis(Angle * Sign, FVector::UpVector), Unused))
            {
                AvoidYawDegrees = Angle * Sign;
                return true;
            }
        }
    }
    AvoidYawDegrees = 0.0;
    return false;
}

bool UPWNpcMovementComponent::TrySnap(const FVector& FeetDestination, const double FeetYaw)
{
    AActor* Owner = GetOwner();
    if (Owner == nullptr || GetWorld() == nullptr)
    {
        return false;
    }
    if (!PWLane::AmbientAllowsSnap(*Owner)) // a cart-led vendor walks; she is never snapped (R4-03)
    {
        return false;
    }
    const double Half = HalfHeight();
    const FVector Centre = FeetDestination + FVector(0.0, 0.0, Half + 2.0);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(NpcSnap), false, Owner);
    PWLane::AmbientMoveIgnoreParams(*Owner, Params);
    if (GetWorld()->OverlapBlockingTestByChannel(Centre, FQuat::Identity, ECC_Pawn,
                                                 FCollisionShape::MakeCapsule(PWNpc::CapsuleRadius, static_cast<float>(Half)), Params))
    {
        return false; // a person, the player or a wall is there: never land inside them
    }
    Owner->SetActorLocationAndRotation(Centre, FRotator(0.0, FeetYaw, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
    if (const ACharacter* Character = Cast<ACharacter>(Owner))
    {
        Character->GetCharacterMovement()->Velocity = FVector::ZeroVector;
    }
    NpcDistanceCm = -1.0;
    return true;
}

bool UPWNpcMovementComponent::ForceSnapToIntent()
{
    if (!bHasIntent)
    {
        return false;
    }
    if (NpcPresentUnderlying(Intent) == EPWNpcState::Travel)
    {
        const double Where = ExtrapolatedTargetDistance();
        return Route.IsValid() && TrySnap(Route.PointAt(Where), Route.DirectionAt(Where).Rotation().Yaw);
    }
    return bAnchorKnown && TrySnap(StandFeet, StandYaw);
}

void UPWNpcMovementComponent::FaceYaw(const double TargetYaw, const double DegreesPerSecond, const float Dt)
{
    AActor* Owner = GetOwner();
    if (Owner == nullptr)
    {
        return;
    }
    FRotator Rotation = Owner->GetActorRotation();
    const double Delta = FMath::FindDeltaAngleDegrees(Rotation.Yaw, TargetYaw);
    if (FMath::Abs(Delta) > 0.01)
    {
        const double Step = DegreesPerSecond * Dt;
        Rotation.Yaw += FMath::Clamp(Delta, -Step, Step);
        Owner->SetActorRotation(Rotation);
    }
}

void UPWNpcMovementComponent::UpdateMotion(const float Dt)
{
    bMovedThisTick = false;
    SteerDirection = FVector::ZeroVector; // set again below when steering; Decide sweeps along it
    const EPWNpcState Presented = GetPresentedState();
    if (!bHasIntent || Presented == EPWNpcState::Chat)
    {
        Hold(Dt, Presented);
    }
    else if (NpcPresentUnderlying(Intent) == EPWNpcState::Travel)
    {
        MoveAlongRoute(Dt);
    }
    else if (Presented == EPWNpcState::Yield)
    {
        Hold(Dt, Presented);
    }
    else
    {
        MoveToStand(Dt);
    }
}

/** Stops steering and faces whatever the presented state faces. Braking is the character movement's. */
void UPWNpcMovementComponent::Hold(const float Dt, const EPWNpcState Presented)
{
    AActor* Owner = GetOwner();
    if (Owner == nullptr)
    {
        return;
    }
    const APawn* Player = LocalPlayer();
    const FVector Here = Owner->GetActorLocation();
    const bool bPlayerClose = Player != nullptr && FVector::DistSquared2D(Player->GetActorLocation(), Here) <= FMath::Square(static_cast<double>(PWNpc::LookAtRangeCm));
    const double RestYaw = bHasIntent && bAnchorKnown ? StandYaw : HomeYaw;
    switch (Presented)
    {
    case EPWNpcState::Chat:
        if (Player != nullptr)
        {
            const double Yaw = NpcPresentYawToward(Here, Player->GetActorLocation());
            if (bReduced)
            {
                FRotator Rotation = Owner->GetActorRotation();
                Rotation.Yaw = Yaw; // reduced motion: no turn to watch, they simply face you
                Owner->SetActorRotation(Rotation);
            }
            else
            {
                FaceYaw(Yaw, 140.0, Dt);
            }
        }
        break;
    case EPWNpcState::Yield:
        if (bBlockedByPlayer && Player != nullptr)
        {
            FaceYaw(NpcPresentYawToward(Here, Player->GetActorLocation()), 140.0, Dt);
        }
        break;
    case EPWNpcState::Travel:
        break; // waiting on a route: stay as turned
    case EPWNpcState::Idle:
        FaceYaw(!bReduced && bPlayerClose ? NpcPresentYawToward(Here, Player->GetActorLocation()) : RestYaw, PWNpc::TurnDegreesPerSecond, Dt);
        break;
    default:
        FaceYaw(RestYaw, PWNpc::TurnDegreesPerSecond, Dt); // work and sleep face their anchor, not the passer-by
        break;
    }
}

void UPWNpcMovementComponent::ApplySteering(const FVector& Direction, const double Speed, const float Dt)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (Character == nullptr)
    {
        return;
    }
    if (bLocallyYielding)
    {
        Hold(Dt, EPWNpcState::Yield); // stopped for someone in the way: visibly waiting, facing the player if it is them
        return;
    }
    if (Speed < 1.0)
    {
        return;
    }
    const FVector Aim = Direction.RotateAngleAxis(AvoidYawDegrees, FVector::UpVector);
    Character->GetCharacterMovement()->MaxWalkSpeed = static_cast<float>(Speed);
    Character->AddMovementInput(Aim, 1.0f);
    bMovedThisTick = true;
    FaceYaw(Aim.Rotation().Yaw, 240.0, Dt);
}

void UPWNpcMovementComponent::MoveAlongRoute(const float Dt)
{
    if (!Route.IsValid())
    {
        Hold(Dt, EPWNpcState::Travel); // no route known: wait where we stand; nothing is invented
        return;
    }
    const FVector Feet = FeetLocation();
    NpcDistanceCm = Route.Project(Feet, NpcDistanceCm, LateralCm);
    const double Target = ExtrapolatedTargetDistance();
    const double Lag = Target - NpcDistanceCm;
    if (NpcDistanceCm >= Route.Length() - PWNpcMove::ArrivalToleranceCm && Target >= Route.Length() - 1.0)
    {
        Hold(Dt, EPWNpcState::Travel); // arrived; the next intent (Work) takes over
        return;
    }
    const double Speed = PWNpcMove::SpeedForLag(Lag, TargetSpeedNow());
    // A short, fixed look-ahead: how far behind the core the person is sets the speed, never how wide a corner is cut.
    const FVector Aim = Route.PointAt(FMath::Min(NpcDistanceCm + PWNpcMove::RouteLookAheadCm, Route.Length()));
    FVector Direction = (Aim - Feet).GetSafeNormal2D();
    if (Direction.IsNearlyZero())
    {
        Direction = Route.DirectionAt(NpcDistanceCm).GetSafeNormal2D();
    }
    SteerDirection = Direction;
    ApplySteering(Direction, Speed, Dt);
}

void UPWNpcMovementComponent::MoveToStand(const float Dt)
{
    const FVector Stand = bAnchorKnown ? StandFeet : HomeFeet;
    const FVector Feet = FeetLocation();
    const double Distance = FVector::Dist2D(Feet, Stand);
    LateralCm = 0.0;
    bArrived = !bAnchorKnown || (bArrived ? Distance <= 40.0 : Distance <= PWNpcMove::ArrivalToleranceCm);
    if (bArrived)
    {
        FinishRoute.Set({});
        Hold(Dt, NpcPresentUnderlying(Intent));
        return;
    }
    FVector Direction = (Stand - Feet).GetSafeNormal2D();
    double Speed = Distance > PWNpcMove::SnapDistanceCm ? PWNpcMove::CatchUpSpeedCmPerSec : FMath::Clamp(Distance * 3.0, 40.0, PWNpcMove::WalkSpeedCmPerSec);
    AimAlongFinishRoute(Stand, Feet, Direction, Speed); // still on the way: the rest of the route, not a straight line
    SteerDirection = Direction;
    ApplySteering(Direction, Speed, Dt);
}

bool UPWNpcMovementComponent::AimAlongFinishRoute(const FVector& Stand, const FVector& Feet, FVector& OutDirection, double& OutSpeed)
{
    // Kept while a detour (a doorway wait) points elsewhere; used again once the stand is the route's end.
    if (!FinishRoute.IsValid() || FVector::Dist2D(FinishRoute.Points.Last(), Stand) > PWNpcMove::FinishRouteJoinCm)
    {
        return false;
    }
    FinishDistanceCm = FinishRoute.Project(Feet, FinishDistanceCm, LateralCm);
    const double Remaining = FinishRoute.Length() - FinishDistanceCm;
    if (Remaining <= PWNpcMove::ArrivalToleranceCm)
    {
        FinishRoute.Set({}); // at the route's end: the last steps to the stand are the plain approach
        LateralCm = 0.0;
        return false;
    }
    const FVector Aim = FinishRoute.PointAt(FMath::Min(FinishDistanceCm + PWNpcMove::RouteLookAheadCm, FinishRoute.Length()));
    OutDirection = (Aim - Feet).GetSafeNormal2D();
    if (OutDirection.IsNearlyZero())
    {
        OutDirection = FinishRoute.DirectionAt(FinishDistanceCm).GetSafeNormal2D();
    }
    // Late already: a jog while there is ground to make up, never a run; the core's next intent does not wait for us.
    OutSpeed = Remaining > PWNpcMove::CatchUpLagCm ? PWNpcMove::CatchUpSpeedCmPerSec : PWNpcMove::WalkSpeedCmPerSec;
    return true;
}

FPWNpcPose UPWNpcMovementComponent::DesiredPose() const
{
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    const double Speed = Character != nullptr ? Character->GetVelocity().Size2D() : 0.0;
    if (Speed > 10.0)
    {
        return PWNpcGesture::Walk(static_cast<float>(WalkedCm), static_cast<float>(Speed / PWNpcMove::WalkSpeedCmPerSec));
    }
    if (!bHasIntent)
    {
        return PWNpcGesture::Idle(static_cast<float>(GestureSeconds), bReduced);
    }
    const FName Action = Intent.State == EPWNpcState::Work ? Intent.WorkActionId : Intent.ResumeWorkActionId;
    const float Seconds = static_cast<float>(GestureSeconds);
    switch (GetPresentedState())
    {
    case EPWNpcState::Yield: return PWNpcGesture::Yielding();
    case EPWNpcState::Chat:
        return ChatSettleRemaining > 0.0 && !Action.IsNone() ? PWNpcGesture::Work(Action, Seconds, bReduced) : PWNpcGesture::Idle(0.0f, true);
    case EPWNpcState::Sleep: return bArrived ? PWNpcGesture::Sleeping() : PWNpcGesture::Idle(0.0f, true);
    case EPWNpcState::Work: return bArrived ? PWNpcGesture::Work(Action, Seconds, bReduced) : PWNpcGesture::Idle(0.0f, true);
    case EPWNpcState::Idle: return bArrived && Intent.bPreferSit ? PWNpcGesture::Sitting() : PWNpcGesture::Idle(Seconds, bReduced);
    default: return PWNpcGesture::Idle(0.0f, true);
    }
}

/** 20 Hz within 15 m, 5 Hz beyond, and none at all when beyond 15 m and unseen. */
void UPWNpcMovementComponent::UpdatePose(const float Dt)
{
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    WalkedCm += (Character != nullptr ? Character->GetVelocity().Size2D() : 0.0) * Dt;
    if (!bNearPlayer && !bRecentlyRendered)
    {
        PoseAccumulator = 0.0;
        return;
    }
    PoseAccumulator += Dt;
    if (PoseAccumulator < (bNearPlayer ? PWNpcMove::PoseNearSeconds : PWNpcMove::PoseFarSeconds) - 1e-6)
    {
        return;
    }
    const double Step = PoseAccumulator;
    PoseAccumulator = 0.0;
    const EPWNpcState Presented = GetPresentedState();
    const bool bSettling = Presented == EPWNpcState::Chat && ChatSettleRemaining > 0.0;
    if (!bHasIntent || bSettling || (bArrived && (Presented == EPWNpcState::Work || Presented == EPWNpcState::Idle)))
    {
        GestureSeconds += Step; // the stroke only advances while it is visibly being worked
    }
    if (bSettling)
    {
        ChatSettleRemaining = FMath::Max(0.0, ChatSettleRemaining - Step);
    }
    const float Alpha = bReduced ? 1.0f : static_cast<float>(1.0 - FMath::Exp(-12.0 * Step));
    const float PostureAlpha = bReduced ? 1.0f : static_cast<float>(1.0 - FMath::Exp(-3.0 * Step));
    CurrentPose.BlendToward(DesiredPose(), Alpha, PostureAlpha);
    if (!CurrentPose.NearlyEquals(BroadcastPose))
    {
        BroadcastPose = CurrentPose;
        OnPose.Broadcast(CurrentPose);
    }
}
