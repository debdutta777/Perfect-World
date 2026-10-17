#include "PerfectWorldNpcYield.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "PerfectWorldNpcPresenceInternal.h"
#include "PerfectWorldNpcPresenceSubsystem.h"

namespace
{
double NpDoorDistance(const FBox& Bounds, const FVector& Feet)
{
    const FVector Closest = Bounds.GetClosestPointTo(Feet);
    return FVector::Dist2D(Feet, Closest);
}

bool NpCapsuleInDoor(const FNpDoorwayZone& Door, const FVector& Feet, const double Radius, const double HalfHeight)
{
    return Feet.Z < Door.Bounds.Max.Z && Feet.Z + 2.0 * HalfHeight > Door.Bounds.Min.Z
        && NpDoorDistance(Door.Bounds, Feet) <= Radius;
}
}

bool NpYield::Update(FNpYieldState& State, const TConstArrayView<FNpDoorwayZone> Doors, const FVector& NpcFeet,
                     const double Radius, const double HalfHeight, const FVector* PlayerFeet, const bool bEligible,
                     const double Dt, const FPointAllowed& PointAllowed)
{
    if (!bEligible) { State = {}; return false; }
    if (State.IsActive())
    {
        const FNpDoorwayZone* Active = nullptr;
        for (const auto& Door : Doors) { if (Door.Id == State.DoorId) { Active = &Door; break; } }
        if (Active == nullptr) { State = {}; return false; }
        const bool bClear = PlayerFeet == nullptr || NpDoorDistance(Active->Bounds, *PlayerFeet) > ReleaseCm;
        State.ClearSeconds = bClear ? State.ClearSeconds + FMath::Max(0.0, Dt) : 0.0;
        if (State.ClearSeconds >= ReleaseSeconds) { State = {}; return false; }
        return true; // Latch even after the NPC is outside the exclusion volume.
    }
    if (PlayerFeet == nullptr) { return false; }
    const FNpDoorwayZone* NearestDoor = nullptr;
    FVector NearestPoint = FVector::ZeroVector;
    double NearestSq = TNumericLimits<double>::Max();
    for (const auto& Door : Doors)
    {
        if (!Door.Bounds.IsValid || !NpCapsuleInDoor(Door, NpcFeet, Radius, HalfHeight)
            || NpDoorDistance(Door.Bounds, *PlayerFeet) > ApproachCm) { continue; }
        for (const FVector& Point : Door.WaitingFeet)
        {
            if (FVector::DistSquared2D(NpcFeet, Point) > FMath::Square(MaxDetourCm)) { continue; }
            bool bOutside = true;
            for (const auto& Other : Doors)
            {
                if (NpCapsuleInDoor(Other, Point, Radius + 10.0, HalfHeight)) { bOutside = false; break; }
            }
            if (!bOutside || (PointAllowed && !PointAllowed(Point))) { continue; }
            const double Sq = FVector::DistSquared2D(NpcFeet, Point);
            if (Sq < NearestSq)
            {
                NearestSq = Sq;
                NearestDoor = &Door;
                NearestPoint = Point;
            }
        }
    }
    if (NearestDoor == nullptr) { return false; }
    State.DoorId = NearestDoor->Id;
    State.WaitingFeet = NearestPoint;
    State.ClearSeconds = 0.0;
    return true;
}

void UPWNpcPresenceSubsystem::UpdateYield(const FName PersonId, FNpPresenceRecord& Record, const float Dt)
{
    APWNpcCharacter* Actor = Record.Actor.Get();
    if (Actor == nullptr) { return; }
    UPWNpcMovementComponent* Movement = Actor->GetNpcMovement();
    // Conversations hold the current physical point; preserve the detour until the panel closes.
    if (Movement->GetPresentedState() == EPWNpcState::Chat) { return; }
    const UCapsuleComponent* Capsule = Actor->GetCapsuleComponent();
    const double Half = Capsule->GetScaledCapsuleHalfHeight();
    const double Radius = Capsule->GetScaledCapsuleRadius();
    const FVector Feet = Actor->GetActorLocation() - FVector(0.0, 0.0, Half);
    const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    const APawn* Player = Controller != nullptr ? Controller->GetPawn() : nullptr;
    FVector PlayerFeet = Player != nullptr ? Player->GetActorLocation() : FVector::ZeroVector;
    const auto* PlayerCapsule = Player != nullptr ? Player->FindComponentByClass<UCapsuleComponent>() : nullptr;
    if (PlayerCapsule != nullptr) { PlayerFeet.Z -= PlayerCapsule->GetScaledCapsuleHalfHeight(); }
    const bool bEligible = Record.Binding.Kind == ENpBindingKind::Local && Record.bIntentValid
        && (Record.Intent.State == PerfectWorld::NpcState::Work || Record.Intent.State == PerfectWorld::NpcState::Idle);
    const TArray<FNpDoorwayZone> Doors = Bindings().Doorways();
    const bool bWasActive = Record.Yield.IsActive();
    const auto NpPointAllowed = [this, Actor, Feet, Half, Radius, Player](const FVector& Point)
    {
        const FVector Lift(0.0, 0.0, Half + 2.0);
        FCollisionQueryParams Params(SCENE_QUERY_STAT(NpDoorwayWait), false, Actor);
        const auto Shape = FCollisionShape::MakeCapsule(static_cast<float>(Radius), static_cast<float>(Half));
        if (GetWorld()->OverlapBlockingTestByChannel(Point + Lift, FQuat::Identity, ECC_Pawn, Shape, Params)) { return false; }
        FHitResult Hit;
        // A waiting point across a wall or player is not a sidestep. Never push through them to clear the doorway.
        if (GetWorld()->SweepSingleByChannel(Hit, Feet + Lift, Point + Lift, FQuat::Identity, ECC_Pawn, Shape, Params)) { return false; }
        if (Player != nullptr && FVector::Dist2D(Point, Player->GetActorLocation()) < Radius + 50.0) { return false; }
        // Refuse a drop or an unsupported point; the character mover remains responsible for walking the clear path.
        return GetWorld()->LineTraceSingleByChannel(Hit, Point + FVector(0.0, 0.0, 10.0),
            Point - FVector(0.0, 0.0, 25.0), ECC_WorldStatic, Params) && Hit.ImpactNormal.Z >= 0.7;
    };
    NpYield::Update(Record.Yield, Doors, Feet, Radius, Half, Player != nullptr ? &PlayerFeet : nullptr,
                    bEligible, Dt, NpPointAllowed);
    if (Record.Yield.IsActive())
    {
        if (!bWasActive)
        {
            // Idle tells the existing mover to WALK to this temporary anchor; core Yield deliberately only holds.
            FPWNpcIntentView Detour = MakeNpcIntentView(Record.Intent);
            Detour.State = EPWNpcState::Idle;
            Detour.AnchorId = FName(TEXT("anchor.presentation.doorway_wait"));
            Detour.WorkActionId = NAME_None;
            Detour.bPreferSit = false;
            Movement->SetIntent(Detour);
        }
    }
    else if (bWasActive)
    {
        Present(PersonId, Record); // Walk back to the newly resolved schedule anchor.
    }
}
