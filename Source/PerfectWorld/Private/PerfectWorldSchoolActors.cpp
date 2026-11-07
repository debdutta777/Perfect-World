#include "PerfectWorldSchoolActors.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldSchoolPresenter.h"

#define LOCTEXT_NAMESPACE "PerfectWorldSchoolActors"

APWSchoolSeatActor::APWSchoolSeatActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Root->SetMobility(EComponentMobility::Movable);
    SetRootComponent(Root);
    Tags.AddUnique(FName(TEXT("interaction.nabapara.school_seat")));
}

void APWSchoolSeatActor::Configure(const FVector& PointWorld)
{
    SetActorLocation(PointWorld, false, nullptr, ETeleportType::TeleportPhysics);
}

FText APWSchoolSeatActor::GetInteractionVerb(const APawn* /*By*/) const
{
    return LOCTEXT("TakeYourSeat", "Take your seat");
}

void APWSchoolSeatActor::Interact(APawn* By)
{
    bOpenedPanel = false;
    UPWSchoolPresenter* Presenter = UPWSchoolPresenter::Get(this);
    if (Presenter != nullptr && Presenter->OpenDay())
    {
        ++OpenCount;
        bOpenedPanel = true;
        return;
    }
    if (UPWInteractionComponent* Interaction = By != nullptr ? By->FindComponentByClass<UPWInteractionComponent>() : nullptr)
    {
        Interaction->ShowNotice(LOCTEXT("SeatBusy", "You cannot take your seat right now. Finish what you are doing first."), 4.0f);
    }
}

APWStudyDeskActor::APWStudyDeskActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Root->SetMobility(EComponentMobility::Movable);
    SetRootComponent(Root);
    Tags.AddUnique(FName(TEXT("interaction.nabapara.home_study_desk")));
}

void APWStudyDeskActor::Configure(const FVector& PointWorld)
{
    SetActorLocation(PointWorld, false, nullptr, ETeleportType::TeleportPhysics);
}

FText APWStudyDeskActor::GetInteractionVerb(const APawn* /*By*/) const
{
    return LOCTEXT("StudyAtDesk", "Study at the desk");
}

void APWStudyDeskActor::Interact(APawn* By)
{
    bOpenedPanel = false;
    UPWSchoolPresenter* Presenter = UPWSchoolPresenter::Get(this);
    if (Presenter != nullptr && Presenter->OpenDesk())
    {
        bOpenedPanel = true;
        return;
    }
    if (UPWInteractionComponent* Interaction = By != nullptr ? By->FindComponentByClass<UPWInteractionComponent>() : nullptr)
    {
        Interaction->ShowNotice(LOCTEXT("DeskBusy", "You cannot sit down to study right now."), 4.0f);
    }
}

#undef LOCTEXT_NAMESPACE
