#include "PerfectWorldNpcMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "NpcBehaviour.h"
#include "PerfectWorldConversationPresenter.h"

#include <string>

namespace
{
FName NpcMoveToName(const std::string_view Text)
{
    return Text.empty() ? FName() : FName(UTF8_TO_TCHAR(std::string(Text).c_str()));
}

EPWNpcState NpcMoveToView(const PerfectWorld::NpcState State)
{
    switch (State)
    {
    case PerfectWorld::NpcState::Work: return EPWNpcState::Work;
    case PerfectWorld::NpcState::Travel: return EPWNpcState::Travel;
    case PerfectWorld::NpcState::Chat: return EPWNpcState::Chat;
    case PerfectWorld::NpcState::Sleep: return EPWNpcState::Sleep;
    case PerfectWorld::NpcState::Yield: return EPWNpcState::Yield;
    default: return EPWNpcState::Idle;
    }
}

/** The intent that is suspended behind a Chat or Yield, else the intent itself. */
EPWNpcState NpcMoveUnderlying(const FPWNpcIntentView& I)
{
    return (I.State == EPWNpcState::Chat || I.State == EPWNpcState::Yield) ? I.ResumeState : I.State;
}
} // namespace

FPWNpcIntentView MakeNpcIntentView(const PerfectWorld::NpcIntent& Core, const bool bPreferSit)
{
    FPWNpcIntentView View;
    View.State = NpcMoveToView(Core.State);
    View.AnchorId = NpcMoveToName(Core.AnchorId);
    View.RouteId = NpcMoveToName(Core.RouteId);
    View.ProgressPermille = Core.ProgressPermille;
    View.WorkActionId = NpcMoveToName(Core.WorkActionId);
    View.bTalkable = Core.bTalkable;
    View.DeclineReasonKey = NpcMoveToName(Core.DeclineReasonKey);
    View.ResumeState = NpcMoveToView(Core.ResumeState);
    View.ResumeWorkActionId = NpcMoveToName(Core.ResumeWorkActionId);
    View.bPreferSit = bPreferSit;
    return View;
}

void FPWNpcPolyline::Set(const TArray<FVector>& InPoints)
{
    TArray<FVector> Kept;
    for (const FVector& P : InPoints)
    {
        if (!Kept.IsEmpty() && FVector::Dist(Kept.Last(), P) < 1.0)
        {
            continue;
        }
        Kept.Add(P);
        // A -> B -> C where B -> C runs straight back along A -> B (within about 2 degrees): B is the tip of a spur the journey
        // only passes through. Walking it means turning on the spot at its tip, where every point ahead lies behind the walker.
        // Drop B; A -> C stays on the same authored line. Repeat, since C may also fold back past A.
        while (Kept.Num() >= 3)
        {
            const FVector A = Kept[Kept.Num() - 3];
            const FVector B = Kept[Kept.Num() - 2];
            const FVector C = Kept.Last();
            if (FVector::DotProduct((B - A).GetSafeNormal(), (C - B).GetSafeNormal()) > -0.9995)
            {
                break;
            }
            Kept.RemoveAt(Kept.Num() - 2);
            if (FVector::Dist(Kept[Kept.Num() - 2], Kept.Last()) < 1.0)
            {
                Kept.Pop(); // the spur returned exactly where it left
            }
        }
    }
    Points.Reset();
    Cumulative.Reset();
    for (const FVector& P : Kept)
    {
        Cumulative.Add(Points.IsEmpty() ? 0.0 : Cumulative.Last() + FVector::Dist(Points.Last(), P));
        Points.Add(P);
    }
}

// --- component: intent in -----------------------------------------------------------------------------------------

UPWNpcMovementComponent::UPWNpcMovementComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = static_cast<float>(PWNpcMove::MoverTickIntervalSeconds);
}

void UPWNpcMovementComponent::BeginPlay()
{
    Super::BeginPlay();
    HomeFeet = FeetLocation();
    HomeYaw = GetOwner() != nullptr ? GetOwner()->GetActorRotation().Yaw : 0.0;
    StandFeet = HomeFeet;
    StandYaw = HomeYaw;
}

double UPWNpcMovementComponent::HalfHeight() const
{
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    return Character != nullptr ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0;
}

FVector UPWNpcMovementComponent::FeetLocation() const
{
    return GetOwner() != nullptr ? GetOwner()->GetActorLocation() - FVector(0.0, 0.0, HalfHeight()) : FVector::ZeroVector;
}

APawn* UPWNpcMovementComponent::LocalPlayer() const
{
    return UGameplayStatics::GetPlayerPawn(this, 0);
}

void UPWNpcMovementComponent::SetIntent(const FPWNpcIntentView& NewIntent)
{
    const FPWNpcIntentView Old = Intent;
    const bool bHad = bHasIntent;
    const EPWNpcState WasPresented = GetPresentedState();
    Intent = NewIntent;
    bHasIntent = true;

    const bool bWasTravel = bHad && NpcMoveUnderlying(Old) == EPWNpcState::Travel;
    const bool bIsTravel = NpcMoveUnderlying(NewIntent) == EPWNpcState::Travel;
    if (bWasTravel && !bIsTravel && Route.IsValid())
    {
        FinishRoute = Route; // a person still on the way finishes along it, never straight through a wall
        FinishDistanceCm = NpcDistanceCm;
    }
    else if (bIsTravel)
    {
        FinishRoute.Set({});
    }
    const bool bRouteChanged = !bHad || Old.RouteId != NewIntent.RouteId;
    if (bRouteChanged)
    {
        ResolveRoute();
    }
    if (!bHad || Old.AnchorId != NewIntent.AnchorId)
    {
        ResolveAnchor();
    }
    if (bHad && (NpcMoveUnderlying(Old) == EPWNpcState::Travel) != (NpcMoveUnderlying(NewIntent) == EPWNpcState::Travel))
    {
        bArrived = false; // walking and standing are different jobs: arrival is judged afresh
    }
    UpdateProgress(bRouteChanged);
    if (!bHad || Old.WorkActionId != NewIntent.WorkActionId)
    {
        GestureSeconds = 0.0;
    }
    const EPWNpcState Now = GetPresentedState();
    if (Now == EPWNpcState::Chat && WasPresented != EPWNpcState::Chat)
    {
        BeginChatSettle(NewIntent.ResumeState == EPWNpcState::Work ? NewIntent.ResumeWorkActionId
                                                                    : (WasPresented == EPWNpcState::Work ? Old.WorkActionId : FName()));
    }
    SetTickRate();
}

void UPWNpcMovementComponent::SetRoute(const TArray<FVector>& WorldPoints)
{
    Route.Set(WorldPoints);
    RouteKey = bHasIntent && !Intent.RouteId.IsNone() ? Intent.RouteId : FName();
    if (bHasIntent && Route.IsValid() && !RouteKey.IsNone())
    {
        UpdateProgress(true);
    }
}

void UPWNpcMovementComponent::ResolveRoute()
{
    if (Intent.RouteId.IsNone())
    {
        if (!RouteKey.IsNone())
        {
            Route.Set({});
            RouteKey = FName();
        }
        return; // a route set ahead of its intent (RouteKey None) waits to be adopted
    }
    if (RouteKey.IsNone() && Route.IsValid())
    {
        RouteKey = Intent.RouteId; // adopt a route given by SetRoute before the intent
        return;
    }
    TArray<FVector> Points;
    Route.Set({});
    RouteKey = Intent.RouteId;
    if (RouteResolver && RouteResolver(Intent.RouteId, Points))
    {
        Route.Set(Points);
    }
}

void UPWNpcMovementComponent::ResolveAnchor()
{
    bAnchorKnown = false;
    FTransform Feet;
    if (!Intent.AnchorId.IsNone() && AnchorResolver && AnchorResolver(Intent.AnchorId, Feet))
    {
        StandFeet = Feet.GetLocation();
        StandYaw = Feet.Rotator().Yaw;
        bAnchorKnown = true;
    }
    bArrived = false;
}

void UPWNpcMovementComponent::UpdateProgress(const bool bReset)
{
    if (!Route.IsValid())
    {
        return;
    }
    const double NewDistance = Route.Length() * FMath::Clamp(Intent.ProgressPermille, 0, 1000) / 1000.0;
    if (bReset)
    {
        TargetDistanceCm = NewDistance;
        TargetChangedAt = ClockSeconds;
        TargetSpeedEstimate = PWNpcMove::WalkSpeedCmPerSec;
        NpcDistanceCm = -1.0;
        return;
    }
    if (FMath::IsNearlyEqual(NewDistance, TargetDistanceCm, 0.5))
    {
        return; // the core has not moved on: the extrapolation clock keeps running from the last change
    }
    const double Elapsed = ClockSeconds - TargetChangedAt;
    if (NewDistance > TargetDistanceCm && Elapsed > 0.2 && Elapsed < 3.0)
    {
        const double Rate = (NewDistance - TargetDistanceCm) / Elapsed;
        TargetSpeedEstimate = 0.5 * TargetSpeedEstimate + 0.5 * FMath::Clamp(Rate, 0.0, PWNpcMove::CatchUpSpeedCmPerSec);
    }
    else if (NewDistance < TargetDistanceCm || Elapsed >= 3.0)
    {
        TargetSpeedEstimate = PWNpcMove::WalkSpeedCmPerSec; // a jump (a skip, a conversation's cost) says nothing about pace
    }
    TargetDistanceCm = NewDistance;
    TargetChangedAt = ClockSeconds;
}

double UPWNpcMovementComponent::ExtrapolatedTargetDistance() const
{
    const double Elapsed = FMath::Min(ClockSeconds - TargetChangedAt, PWNpcMove::ExtrapolationCapSeconds);
    return FMath::Min(Route.Length(), TargetDistanceCm + TargetSpeedEstimate * FMath::Max(Elapsed, 0.0));
}

double UPWNpcMovementComponent::TargetSpeedNow() const
{
    return ClockSeconds - TargetChangedAt >= PWNpcMove::ExtrapolationCapSeconds ? 0.0 : TargetSpeedEstimate;
}

FVector UPWNpcMovementComponent::GetTravelTarget() const
{
    return Route.IsValid() ? Route.PointAt(ExtrapolatedTargetDistance()) : FeetLocation();
}

void UPWNpcMovementComponent::SetLocalConversationActive(const bool bActive)
{
    if (bLocalChat == bActive)
    {
        return;
    }
    const EPWNpcState Was = GetPresentedState();
    bLocalChat = bActive;
    const UPWConversationPresenter* Presenter = UPWConversationPresenter::Get(this);
    bChatPanelSeen = bActive && Presenter != nullptr && Presenter->IsOpen();
    if (!bActive)
    {
        ChatSettleRemaining = 0.0;
    }
    if (bActive && Was == EPWNpcState::Work)
    {
        BeginChatSettle(Intent.WorkActionId);
    }
    SetTickRate();
}

void UPWNpcMovementComponent::BeginChatSettle(const FName WorkAction)
{
    ChatSettleRemaining = 0.0;
    if (bReduced || WorkAction.IsNone() || !bArrived)
    {
        return;
    }
    const double Cycle = PWNpcGesture::CycleSeconds(WorkAction);
    ChatSettleRemaining = FMath::Min(PWNpcMove::GestureFinishSeconds, Cycle - FMath::Fmod(GestureSeconds, Cycle));
}

EPWNpcState UPWNpcMovementComponent::GetPresentedState() const
{
    if (bLocalChat)
    {
        return EPWNpcState::Chat;
    }
    if (!bHasIntent)
    {
        return EPWNpcState::Idle;
    }
    if (bLocallyYielding && (Intent.State == EPWNpcState::Travel || Intent.State == EPWNpcState::Work || Intent.State == EPWNpcState::Idle))
    {
        return EPWNpcState::Yield;
    }
    if (Intent.State == EPWNpcState::Yield && Intent.ResumeState == EPWNpcState::Travel && !bLocallyYielding)
    {
        return EPWNpcState::Travel; // the core has not yet heard the way is clear again; walk on
    }
    return Intent.State;
}

void UPWNpcMovementComponent::SetTickRate()
{
    double Interval = 0.1;
    const EPWNpcState P = GetPresentedState();
    const EPWNpcState Under = (Intent.State == EPWNpcState::Chat || Intent.State == EPWNpcState::Yield) ? Intent.ResumeState : Intent.State;
    if (bHasIntent && (Under == EPWNpcState::Travel || !bArrived || bMovedThisTick))
    {
        Interval = PWNpcMove::MoverTickIntervalSeconds;
    }
    else if (P == EPWNpcState::Chat || (P == EPWNpcState::Work && !bReduced))
    {
        Interval = 0.05;
    }
    if (!FMath::IsNearlyEqual(Interval, LastTickInterval))
    {
        LastTickInterval = Interval;
        SetComponentTickInterval(static_cast<float>(Interval));
    }
}
