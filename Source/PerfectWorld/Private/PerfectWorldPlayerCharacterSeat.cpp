#include "PerfectWorldPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "PerfectWorldInteractionComponent.h"

// Seating: the state machine, the safety sweeps and the camera. The block body's sitting pose and the eye height live in
// PerfectWorldPlayerCharacter.cpp beside the body table.

namespace
{
float SeatYawOf(const FVector& Direction)
{
    return FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
}

/**
 * True if something blocks the torso capsule on its way between two points, apart from the furniture being sat on: a blocking
 * hit whose impact point lies within `Footprint` of the seat axis is the seat itself (the chair back, the table's edge) unless
 * it is a person. Hits that start inside the capsule count where the capsule starts.
 */
bool IsSeatPathBlocked(const UWorld& World, const FVector& Start, const FVector& End, const FVector& SeatAxis, const float Footprint,
                       const FCollisionQueryParams& Params)
{
    TArray<FHitResult> Hits;
    World.SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Pawn,
                              FCollisionShape::MakeCapsule(PWSeat::BodyRadius, PWSeat::BodyHalfHeight), Params);
    for (const FHitResult& Hit : Hits)
    {
        if (!Hit.bBlockingHit)
        {
            continue;
        }
        const UPrimitiveComponent* Component = Hit.GetComponent();
        const bool bPerson = Component != nullptr && Component->GetCollisionObjectType() == ECC_Pawn;
        const FVector Where = Hit.bStartPenetrating ? Start : FVector(Hit.ImpactPoint);
        if (bPerson || FVector::Dist2D(Where, SeatAxis) > Footprint)
        {
            return true;
        }
    }
    return false;
}

/** Another person within the torso capsule at this point. */
bool IsPersonAtSeat(const UWorld& World, const FVector& Centre, const float Radius, const float HalfHeight, const FCollisionQueryParams& Params)
{
    return World.OverlapAnyTestByObjectType(Centre, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
                                            FCollisionShape::MakeCapsule(Radius, HalfHeight), Params);
}
} // namespace

void APerfectWorldPlayerCharacter::ShowNotice(const FText& Message)
{
    if (Interaction != nullptr)
    {
        Interaction->ShowNotice(Message);
    }
}

FVector APerfectWorldPlayerCharacter::GetEyeLocation() const
{
    return FirstPersonCamera != nullptr ? FirstPersonCamera->GetComponentLocation() : GetActorLocation();
}

void APerfectWorldPlayerCharacter::Jump()
{
    if (SeatState == EPWSeatState::Seated)
    {
        StandUp();
    }
    else if (SeatState == EPWSeatState::Standing)
    {
        Super::Jump();
    }
}

EPWSitResult APerfectWorldPlayerCharacter::TrySit(AActor* Seat, const FPWSeatPlacement& Place)
{
    const UWorld* World = GetWorld();
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    if (Seat == nullptr || World == nullptr || SeatState != EPWSeatState::Standing)
    {
        return EPWSitResult::Busy;
    }
    auto Refuse = [this](EPWSitResult Why, const FText& Message)
    {
        ShowNotice(Message);
        return Why;
    };
    if (IsInputBlocked() || !Movement->IsMovingOnGround())
    {
        return Refuse(EPWSitResult::NotReady, NSLOCTEXT("PerfectWorldSeat", "NotReady", "You can't sit down right now."));
    }
    const FVector Start = GetActorLocation();
    const float StartFloor = Start.Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    if (FMath::Abs(StartFloor - (Place.SeatTop.Z - Place.SeatHeightCm)) > PWSeat::MaxFloorDifferenceCm)
    {
        return Refuse(EPWSitResult::WrongLevel, NSLOCTEXT("PerfectWorldSeat", "WrongLevel", "You can't reach the seat from here."));
    }
    FVector Facing = Place.Facing.GetSafeNormal2D();
    if (Facing.IsNearlyZero())
    {
        Facing = FVector::ForwardVector;
    }

    // The torso capsule at its seated height: nobody else may be at the seat, and the way there must be clear of everything but the furniture.
    FCollisionQueryParams Params(TEXT("PWSit"), false, this);
    Params.AddIgnoredActor(Seat);
    const float BodyZ = Place.SeatTop.Z + PWSeat::BodyAboveSeatCm + PWSeat::BodyHalfHeight;
    const FVector SeatBody(Place.SeatTop.X, Place.SeatTop.Y, BodyZ);
    if (IsPersonAtSeat(*World, SeatBody, PWSeat::CapsuleRadius, PWSeat::BodyHalfHeight, Params))
    {
        return Refuse(EPWSitResult::Occupied, NSLOCTEXT("PerfectWorldSeat", "Occupied", "Someone is already there."));
    }
    if (IsSeatPathBlocked(*World, FVector(Start.X, Start.Y, BodyZ), SeatBody, Place.SeatTop, Place.FootprintCm, Params))
    {
        return Refuse(EPWSitResult::Obstructed, NSLOCTEXT("PerfectWorldSeat", "Obstructed", "Something is in the way."));
    }

    SeatActor = Seat;
    SeatPlacement = Place;
    SeatPlacement.Facing = Facing;
    GlideFrom = Start;
    GlideTo = FVector(Place.SeatTop.X, Place.SeatTop.Y, Place.SeatTop.Z + PWSeat::CapsuleHalfHeight);
    EyeFromZ = GetEyeLocation().Z;
    EyeToZ = Place.SeatTop.Z + PWSeat::EyeAboveSeatCm;
    YawFrom = Controller != nullptr ? Controller->GetControlRotation().Yaw : GetActorRotation().Yaw;
    FacingYaw = SeatYawOf(Facing);
    SeatElapsed = 0.0f;
    bMoveInputThisFrame = false;
    bMoveReleasedSinceSeated = false;
    bUseControllerRotationYaw = false; // while seated the body stays on the seat; only the view turns
    GetCapsuleComponent()->SetCapsuleSize(PWSeat::CapsuleRadius, PWSeat::CapsuleHalfHeight);
    Movement->StopMovementImmediately();
    Movement->DisableMovement();
    SeatState = EPWSeatState::Entering;
    return EPWSitResult::Started;
}

bool APerfectWorldPlayerCharacter::FindStandLocation(const FCollisionQueryParams& Params, FVector& OutCentre) const
{
    const UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return false;
    }
    const FPWSeatPlacement& P = SeatPlacement;
    FVector Dir = (P.StandPoint - P.SeatTop).GetSafeNormal2D();
    if (Dir.IsNearlyZero())
    {
        Dir = -P.Facing;
    }
    const FVector Perp(-Dir.Y, Dir.X, 0.0);
    const double Spread = FMath::Max(20.0f, P.StandClearCm * 0.6f);
    // The stand point first, then a little to either side, then further out along the way off the seat.
    const FVector Offsets[] = {FVector::ZeroVector, Perp * Spread, -Perp * Spread, Dir * Spread, Dir * (2.0 * Spread)};
    const FCollisionShape Standing = FCollisionShape::MakeCapsule(PWPlayer::CapsuleRadius, PWPlayer::CapsuleHalfHeight);
    const float BodyZ = P.SeatTop.Z + PWSeat::BodyAboveSeatCm + PWSeat::BodyHalfHeight;
    const FVector SeatBody(P.SeatTop.X, P.SeatTop.Y, BodyZ);
    for (const FVector& Offset : Offsets)
    {
        const FVector Floor = P.StandPoint + Offset;
        const FVector Centre(Floor.X, Floor.Y, Floor.Z + PWPlayer::CapsuleHalfHeight + 0.5);
        if (World->OverlapBlockingTestByChannel(Centre, FQuat::Identity, ECC_Pawn, Standing, Params) ||
            IsSeatPathBlocked(*World, SeatBody, FVector(Floor.X, Floor.Y, BodyZ), P.SeatTop, P.FootprintCm, Params))
        {
            continue;
        }
        OutCentre = Centre;
        return true;
    }
    return false;
}

bool APerfectWorldPlayerCharacter::StandUp()
{
    if (SeatState != EPWSeatState::Seated || GetWorld() == nullptr)
    {
        return false;
    }
    FCollisionQueryParams Params(TEXT("PWStand"), false, this);
    if (const AActor* Seat = SeatActor.Get())
    {
        Params.AddIgnoredActor(Seat);
    }
    FVector Centre;
    if (!FindStandLocation(Params, Centre))
    {
        ShowNotice(NSLOCTEXT("PerfectWorldSeat", "NoRoom", "There isn't room to stand up here."));
        return false;
    }
    GlideFrom = GetActorLocation();
    GlideTo = Centre;
    EyeFromZ = SeatPlacement.SeatTop.Z + PWSeat::EyeAboveSeatCm;
    EyeToZ = Centre.Z + (PWPlayer::EyeHeightAboveFloor - PWPlayer::CapsuleHalfHeight);
    SeatElapsed = 0.0f;
    SeatState = EPWSeatState::Leaving;
    return true;
}

void APerfectWorldPlayerCharacter::FinishStandUp()
{
    GetCapsuleComponent()->SetCapsuleSize(PWPlayer::CapsuleRadius, PWPlayer::CapsuleHalfHeight);
    SetActorLocation(GlideTo, false, nullptr, ETeleportType::TeleportPhysics);
    ApplySeatPose(0.0f);
    SetEyeWorldHeight(EyeToZ);
    if (Controller != nullptr)
    {
        SetActorRotation(FRotator(0.0, Controller->GetControlRotation().Yaw, 0.0));
    }
    bUseControllerRotationYaw = true;
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->Velocity = FVector::ZeroVector;
    Movement->SetMovementMode(MOVE_Walking); // the floor is 0.5 cm under the capsule
    SeatState = EPWSeatState::Standing;
    SeatActor.Reset();
}

void APerfectWorldPlayerCharacter::TickSeat(float DeltaSeconds)
{
    if (SeatState == EPWSeatState::Standing)
    {
        return;
    }
    if (SeatState == EPWSeatState::Seated)
    {
        // A frame without movement input means the keys have been let go since sitting.
        bMoveReleasedSinceSeated = bMoveReleasedSinceSeated || !bMoveInputThisFrame;
        bMoveInputThisFrame = false;
        ClampSeatedLook();
        return;
    }
    SeatElapsed += DeltaSeconds;
    const float T = FMath::Clamp(SeatElapsed / PWSeat::GlideSeconds, 0.0f, 1.0f);
    const float S = PWSeat::SmoothStep(T);
    const bool bEntering = SeatState == EPWSeatState::Entering;
    SetActorLocation(FMath::Lerp(GlideFrom, GlideTo, S), false, nullptr, ETeleportType::TeleportPhysics);
    SetEyeWorldHeight(FMath::Lerp(EyeFromZ, EyeToZ, S));
    ApplySeatPose(bEntering ? S : 1.0f - S);
    if (bEntering)
    {
        const float Yaw = YawFrom + FMath::FindDeltaAngleDegrees(YawFrom, FacingYaw) * S;
        SetActorRotation(FRotator(0.0, Yaw, 0.0));
        if (Controller != nullptr)
        {
            Controller->SetControlRotation(FRotator(PWSeat::ClampLookPitch(Controller->GetControlRotation().Pitch), Yaw, 0.0f));
        }
    }
    if (T >= 1.0f)
    {
        if (bEntering)
        {
            SeatState = EPWSeatState::Seated;
            bMoveInputThisFrame = false;
            bMoveReleasedSinceSeated = false;
            ClampSeatedLook();
        }
        else
        {
            FinishStandUp();
        }
    }
}

void APerfectWorldPlayerCharacter::ClampSeatedLook()
{
    if (SeatState != EPWSeatState::Seated || Controller == nullptr)
    {
        return;
    }
    const FRotator Look = Controller->GetControlRotation();
    Controller->SetControlRotation(FRotator(PWSeat::ClampLookPitch(Look.Pitch), PWSeat::ClampLookYaw(Look.Yaw, FacingYaw), Look.Roll));
}
