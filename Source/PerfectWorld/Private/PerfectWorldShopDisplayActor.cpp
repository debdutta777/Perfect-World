#include "PerfectWorldShopDisplayActor.h"

#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "PerfectWorldInteractionComponent.h"

// ---- Browse ----

APWShopBrowseActor::APWShopBrowseActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Root->SetMobility(EComponentMobility::Movable);
    SetRootComponent(Root);
    Verb = NSLOCTEXT("PerfectWorldShopDisplay", "Browse", "Browse goods");
}

void APWShopBrowseActor::Configure(const FName InBuildingId, const FName InTag, const FText& InVerb, const FText& InNotice,
                                   const FVector& PointWorld)
{
    BuildingId = InBuildingId;
    Verb = InVerb.IsEmpty() ? NSLOCTEXT("PerfectWorldShopDisplay", "Browse", "Browse goods") : InVerb;
    Notice = InNotice;
    if (!InTag.IsNone())
    {
        Tags.AddUnique(InTag);
    }
    SetActorLocation(PointWorld, false, nullptr, ETeleportType::TeleportPhysics);
}

FText APWShopBrowseActor::GetInteractionVerb(const APawn* /*By*/) const
{
    return Verb;
}

FVector APWShopBrowseActor::GetInteractionPoint() const
{
    return GetActorLocation();
}

void APWShopBrowseActor::Interact(APawn* By)
{
    UPWInteractionComponent* Interaction = By != nullptr ? By->FindComponentByClass<UPWInteractionComponent>() : nullptr;
    if (Interaction != nullptr)
    {
        Interaction->ShowNotice(Notice, PWShopDisplay::NoticeSeconds);
    }
}

// ---- Hours sign ----

APWShopHoursSignActor::APWShopHoursSignActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Text = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Text"));
    SetRootComponent(Text);
    Text->SetMobility(EComponentMobility::Movable);
    Text->SetHorizontalAlignment(EHTA_Center);
    Text->SetVerticalAlignment(EVRTA_TextCenter);
    Text->SetTextRenderColor(FColor(244, 236, 214));
    Text->SetWorldSize(8.0f);
    Text->SetCastShadow(false);
    Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APWShopHoursSignActor::Configure(const FName InBuildingId, const FString& InText, const FVector& Location, const double YawDegrees,
                                      const float TextSizeCm)
{
    BuildingId = InBuildingId;
    PostedText = InText;
    Text->SetText(FText::FromString(InText));
    Text->SetWorldSize(TextSizeCm);
    SetActorLocationAndRotation(Location, FRotator(0.0, YawDegrees, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
}
