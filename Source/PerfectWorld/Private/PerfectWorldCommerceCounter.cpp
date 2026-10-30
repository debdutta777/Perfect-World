#include "PerfectWorldCommerceCounter.h"

#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h" // R88
#include "PerfectWorldBuyPresenter.h"
#include "PerfectWorldCommerceText.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldShopDisplayActor.h"

#define LOCTEXT_NAMESPACE "PerfectWorldCommerceCounter"

namespace PWCommerce
{
bool R89WithinCounterReach(const FVector& AxisBottom, const FVector& AxisTop, const FVector& Point)
{
    return FVector::Dist(FMath::ClosestPointOnSegment(Point, AxisBottom, AxisTop), Point) <=
        PWInteraction::ReachCm + PWInteraction::ReachHysteresisCm;
}

bool R89WithinCounterReach(const APawn& Pawn, const FVector& Point)
{
    FVector Bottom = Pawn.GetActorLocation();
    FVector Top = Bottom;
    if (const ACharacter* Character = Cast<ACharacter>(&Pawn))
    {
        if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
        {
            const double Half = FMath::Max(0.0, static_cast<double>(
                Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleRadius()));
            Bottom = Capsule->GetComponentLocation() - FVector(0.0, 0.0, Half);
            Top = Capsule->GetComponentLocation() + FVector(0.0, 0.0, Half);
        }
    }
    return R89WithinCounterReach(Bottom, Top, Point);
}

int32 SpawnCommerceCounters(UWorld& World, const TArray<FCmCounterSpec>& Specs)
{
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    int32 Spawned = 0;
    for (const FCmCounterSpec& Spec : Specs)
    {
        if (Spec.ShopId.IsNone() || Spec.Tag.IsNone())
        {
            UE_LOG(LogTemp, Error, TEXT("A commerce counter had no shop id or tag; no Buy counter was made."));
            continue;
        }
        APWCounterBuyActor* Counter = World.SpawnActor<APWCounterBuyActor>(Spec.PointWorld, FRotator::ZeroRotator, Params);
        if (Counter != nullptr)
        {
            Counter->Configure(Spec.ShopId, Spec.Tag, LOCTEXT("BuyVerb", "Buy"), Spec.PointWorld);
            ++Spawned;
        }
    }
    return Spawned;
}
} // namespace PWCommerce

APWCounterBuyActor::APWCounterBuyActor()
{
    PrimaryActorTick.bCanEverTick = true; // R88: only enabled while awaiting a safe pause
    PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.TickInterval = 0.1f; // R88
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Root->SetMobility(EComponentMobility::Movable);
    SetRootComponent(Root);
    Verb = LOCTEXT("BuyVerb", "Buy");
}

void APWCounterBuyActor::Configure(const FName InShopId, const FName InTag, const FText& InVerb, const FVector& PointWorld)
{
    ShopId = InShopId;
    Verb = InVerb.IsEmpty() ? LOCTEXT("BuyVerb", "Buy") : InVerb;
    if (!InTag.IsNone())
    {
        Tags.AddUnique(InTag);
    }
    SetActorLocation(PointWorld, false, nullptr, ETeleportType::TeleportPhysics);
}

FText APWCounterBuyActor::GetInteractionVerb(const APawn* /*By*/) const
{
    return Verb;
}

FVector APWCounterBuyActor::GetInteractionPoint() const
{
    return GetActorLocation();
}

int32 APWCounterBuyActor::GetInteractionPriority() const
{
    return PWShopDisplay::InteractionPriority;
}

void APWCounterBuyActor::Say(APawn* By, const FText& Words)
{
    LastNotice = Words;
    UPWInteractionComponent* Interaction = By != nullptr ? By->FindComponentByClass<UPWInteractionComponent>() : nullptr;
    if (Interaction != nullptr)
    {
        Interaction->ShowNotice(Words, PWShopDisplay::NoticeSeconds);
    }
}

void APWCounterBuyActor::Interact(APawn* By)
{
    LastError = PerfectWorld::CommerceError::None;
    bOpenedPanel = false;
    LastNotice = FText::GetEmpty();
    const TSharedPtr<PWCommerce::ICmCommerceWorld> Bridge = PWCommerce::CmBridge::Resolve(GetWorld());
    if (!Bridge.IsValid())
    {
        // No commerce seam in this world: say so plainly. This is a missing feature, not a refusal of the core.
        Say(By, LOCTEXT("NoCounter", "Nothing can be bought here right now."));
        return;
    }
    const PWCommerce::FCmServiceView Offer = Bridge->BeginService(ShopId);
    if (!Offer.Ok())
    {
        // The core's reason, in words: closed, keeper away, finishing a step, not enough money, bag full.
        LastError = Offer.Error;
        Say(By, PWCommerce::CmRefusalText(Offer.Error, Offer.ShopName, Offer.Code)); // R88
        // R88: an accepted pending request advances naturally; retrying queries the same core service.
        bR88Pending = Offer.bPending;
        R88Customer = bR88Pending ? By : nullptr;
        SetActorTickEnabled(bR88Pending);
        if (!bR88Pending) Bridge->EndService(ShopId);
        return;
    }
    bR88Pending = false; // R88
    R88Customer.Reset();
    SetActorTickEnabled(false);
    UPWBuyPresenter* Presenter = UPWBuyPresenter::Get(this);
    if (Presenter == nullptr || !Presenter->Open(Bridge.ToSharedRef(), Offer))
    {
        // The panel could not open (another panel or a menu owns the screen). Release the service the world just granted.
        Bridge->EndService(ShopId);
        Say(By, LOCTEXT("CannotOpen", "You cannot browse the counter right now. Finish what you are doing first."));
        return;
    }
    bOpenedPanel = true;
}

void APWCounterBuyActor::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    APawn* By = R88Customer.Get();
    if (!bR88Pending) return;
    if (By == nullptr || UPWInteractionComponent::IsWorldInputBlocked(By) ||
        !PWCommerce::R89WithinCounterReach(*By, GetInteractionPoint()))
    {
        if (const auto Bridge = PWCommerce::CmBridge::Resolve(GetWorld())) Bridge->EndService(ShopId);
        bR88Pending = false;
        R88Customer.Reset();
        SetActorTickEnabled(false);
        return;
    }
    Interact(By);
}

void APWCounterBuyActor::EndPlay(const EEndPlayReason::Type Reason)
{
    if (bR88Pending)
        if (const auto Bridge = PWCommerce::CmBridge::Resolve(GetWorld())) Bridge->EndService(ShopId);
    Super::EndPlay(Reason);
}

#undef LOCTEXT_NAMESPACE
