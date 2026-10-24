#include "PerfectWorldAwakeningActors.h"

#include "Components/SceneComponent.h"

// The interaction point of a pad. It asks the floor for its verb and hands the interaction back; it decides nothing.

APWAwakeningPadActor::APWAwakeningPadActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Root->SetMobility(EComponentMobility::Movable);
    SetRootComponent(Root);
    Tags.AddUnique(FName(TEXT("interaction.awakening.pad")));
}

void APWAwakeningPadActor::Configure(APWAwakeningFloorActor* InFloor, const PWAwakening::FAwPadSpec& InSpec)
{
    Floor = InFloor;
    Spec = InSpec;
    Tags.AddUnique(Spec.Id);
    SetActorLocation(Spec.Point, false, nullptr, ETeleportType::TeleportPhysics);
}

FText APWAwakeningPadActor::GetInteractionVerb(const APawn* /*By*/) const
{
    const APWAwakeningFloorActor* Owner = Floor.Get();
    return Owner != nullptr ? Owner->GetPadVerb(Spec) : FText::GetEmpty();
}

int32 APWAwakeningPadActor::GetInteractionPriority() const
{
    const APWAwakeningFloorActor* Owner = Floor.Get();
    return Owner != nullptr ? Owner->GetPadPriority(Spec) : 21;
}

void APWAwakeningPadActor::Interact(APawn* By)
{
    if (APWAwakeningFloorActor* Owner = Floor.Get())
    {
        Owner->HandlePad(Spec, By);
    }
}
