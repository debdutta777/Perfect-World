#include "PerfectWorldCommerceCounter.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "PerfectWorldBuyPresenter.h"
#include "PerfectWorldCommerceText.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldShopDisplayActor.h"

#define LOCTEXT_NAMESPACE "PerfectWorldCommerceCounter"

namespace PWCommerce
{
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
    PrimaryActorTick.bCanEverTick = false;
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
        Say(By, PWCommerce::CmRefusalText(Offer.Error, Offer.ShopName));
        return;
    }
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

#undef LOCTEXT_NAMESPACE
