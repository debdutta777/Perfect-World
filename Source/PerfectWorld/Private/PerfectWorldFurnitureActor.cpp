#include "PerfectWorldFurnitureActor.h"

#include "Components/SceneComponent.h"
#include "PerfectWorldLaneHomeFurniture.h"

DEFINE_LOG_CATEGORY_STATIC(LogPWFurniture, Log, All);

namespace
{
/** Actor yaw in degrees so that +X points along a horizontal direction. */
double FurnitureYawOf(const FVector& Direction)
{
    return FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
}

/** A building-local direction (U, V) in the world. */
FVector FurnitureWorldDirection(const PWLane::FLaneFrame& Frame, const double U, const double V)
{
    return (Frame.U * U + Frame.V * V).GetSafeNormal2D();
}
} // namespace

// ---- Seat ----

APWSeatActor::APWSeatActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Root->SetMobility(EComponentMobility::Movable);
    SetRootComponent(Root);
    Verb = NSLOCTEXT("PerfectWorldFurniture", "Sit", "Sit");
}

void APWSeatActor::Configure(const FName InSeatId, const FText& InVerb, const FVector& SeatFloor, const FVector& Facing,
                             const float InSeatHeightCm, const FVector& InStandFloor, const float InStandClearCm)
{
    SeatId = InSeatId;
    Verb = InVerb.IsEmpty() ? NSLOCTEXT("PerfectWorldFurniture", "Sit", "Sit") : InVerb;
    SeatHeightCm = InSeatHeightCm;
    StandFloor = InStandFloor;
    StandClearCm = InStandClearCm;
    const FVector Direction = Facing.GetSafeNormal2D();
    SetActorLocationAndRotation(SeatFloor, FRotator(0.0, Direction.IsNearlyZero() ? 0.0 : FurnitureYawOf(Direction), 0.0),
                                false, nullptr, ETeleportType::TeleportPhysics);
}

void APWSeatActor::ConfigureFromSpec(const PWLane::FHomeSeatSpec& Spec, const PWLane::FLaneFrame& Frame)
{
    Configure(Spec.Id, FText::FromString(FString(Spec.Verb)), PWLane::HomeWorldPoint(Frame, Spec.SeatU, Spec.SeatV, 0.0),
              FurnitureWorldDirection(Frame, Spec.FaceU, Spec.FaceV), static_cast<float>(Spec.SeatHeightCm),
              PWLane::HomeWorldPoint(Frame, Spec.StandU, Spec.StandV, 0.0), static_cast<float>(Spec.StandClearCm));
}

FPWSeatPlacement APWSeatActor::MakePlacement() const
{
    FPWSeatPlacement Placement;
    const FVector Floor = GetActorLocation();
    Placement.SeatTop = Floor + FVector(0.0, 0.0, SeatHeightCm);
    Placement.StandPoint = StandFloor;
    Placement.Facing = GetActorForwardVector().GetSafeNormal2D();
    Placement.SeatHeightCm = SeatHeightCm;
    Placement.StandClearCm = StandClearCm;
    return Placement;
}

FText APWSeatActor::GetInteractionVerb(const APawn* By) const
{
    const APerfectWorldPlayerCharacter* Player = Cast<APerfectWorldPlayerCharacter>(By);
    if (Player == nullptr)
    {
        return Verb;
    }
    if (Player->IsSeatedOn(this))
    {
        return Player->CanStandUp() ? NSLOCTEXT("PerfectWorldFurniture", "StandUp", "Stand up") : FText::GetEmpty();
    }
    return Player->IsSeated() ? FText::GetEmpty() : Verb;
}

FVector APWSeatActor::GetInteractionPoint() const
{
    // Above the cushion, so the line of sight from a standing eye clears a chair back.
    return GetActorLocation() + FVector(0.0, 0.0, SeatHeightCm + PWFurniture::SeatPromptAboveSeatCm);
}

int32 APWSeatActor::GetInteractionPriority() const
{
    const APerfectWorldPlayerCharacter* Player = Cast<APerfectWorldPlayerCharacter>(Occupant.Get());
    return (Player != nullptr && Player->IsSeatedOn(this)) ? PWFurniture::StandUpPriority : PWFurniture::InteractionPriority;
}

void APWSeatActor::Interact(APawn* By)
{
    APerfectWorldPlayerCharacter* Player = Cast<APerfectWorldPlayerCharacter>(By);
    if (Player == nullptr)
    {
        return;
    }
    if (Player->IsSeatedOn(this))
    {
        Player->StandUp(); // E again; refused with a notice if there is no room, and ignored while gliding
        return;
    }
    if (Player->IsSeated())
    {
        return;
    }
    if (Player->TrySit(this, MakePlacement()) == EPWSitResult::Started)
    {
        Occupant = Player;
    }
}

// ---- Readable ----

APWReadableActor::APWReadableActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Root->SetMobility(EComponentMobility::Movable);
    SetRootComponent(Root);
    Verb = NSLOCTEXT("PerfectWorldFurniture", "Read", "Read");
    Title = NSLOCTEXT("PerfectWorldFurniture", "ReadableTitle", "Reading");
}

FPWReadRequested& APWReadableActor::GlobalReadRequested()
{
    static FPWReadRequested Delegate;
    return Delegate;
}

void APWReadableActor::Configure(const FName InReadableId, const FName InContentId, const FText& InVerb, const FText& InTitle,
                                 const FVector& SheetWorld, const FVector& InStandFloor)
{
    ReadableId = InReadableId;
    ContentId = InContentId;
    Verb = InVerb.IsEmpty() ? NSLOCTEXT("PerfectWorldFurniture", "Read", "Read") : InVerb;
    Title = InTitle.IsEmpty() ? NSLOCTEXT("PerfectWorldFurniture", "ReadableTitle", "Reading") : InTitle;
    StandFloor = InStandFloor;
    SetActorLocation(SheetWorld, false, nullptr, ETeleportType::TeleportPhysics);
}

void APWReadableActor::ConfigureFromSpec(const PWLane::FHomeReadableSpec& Spec, const PWLane::FLaneFrame& Frame, const FText& InTitle)
{
    Configure(Spec.Id, Spec.ContentId, FText::FromString(FString(Spec.Verb)), InTitle,
              PWLane::HomeWorldPoint(Frame, Spec.U, Spec.V, Spec.HeightCm), PWLane::HomeWorldPoint(Frame, Spec.StandU, Spec.StandV, 0.0));
}

FText APWReadableActor::GetInteractionVerb(const APawn* By) const
{
    // Reading is allowed standing or seated, but not while the player is gliding to or from a seat.
    const APerfectWorldPlayerCharacter* Player = Cast<APerfectWorldPlayerCharacter>(By);
    const bool bGliding = Player != nullptr && Player->IsSeated() && !Player->CanStandUp();
    return bGliding ? FText::GetEmpty() : Verb;
}

FVector APWReadableActor::GetInteractionPoint() const
{
    return GetActorLocation() + FVector(0.0, 0.0, PWFurniture::ReadablePromptAboveCm);
}

void APWReadableActor::Interact(APawn* By)
{
    if (By == nullptr)
    {
        return;
    }
    const FPWReadRequested& Global = GlobalReadRequested();
    if (!OnReadRequested.IsBound() && !Global.IsBound())
    {
        UE_LOG(LogPWFurniture, Display, TEXT("Readable %s (content %s, \"%s\"): no reading presenter is bound; nothing to show."),
               *ReadableId.ToString(), *ContentId.ToString(), *Title.ToString());
        return;
    }
    OnReadRequested.Broadcast(ReadableId, ContentId, Title);
    Global.Broadcast(ReadableId, ContentId, Title);
}
