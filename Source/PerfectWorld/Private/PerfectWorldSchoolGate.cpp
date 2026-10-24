#include "PerfectWorldSchoolGate.h"

#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "PerfectWorldInteractionComponent.h"

// The two interactive things at the school gate: the attendance register ("Sign in at the gate") and the timetable board.

// ---- Register ----

APWSchoolGateActor::APWSchoolGateActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Root->SetMobility(EComponentMobility::Movable);
    SetRootComponent(Root);
    Tags.AddUnique(FName(TEXT("interaction.nabapara.school_gate")));
}

void APWSchoolGateActor::Configure(const FVector& PointWorld)
{
    Point = PointWorld;
    SetActorLocation(PointWorld, false, nullptr, ETeleportType::TeleportPhysics);
}

FText APWSchoolGateActor::GetInteractionVerb(const APawn* /*By*/) const
{
    return NSLOCTEXT("PerfectWorldSchool", "SignInVerb", "Sign in at the gate");
}

FVector APWSchoolGateActor::GetInteractionPoint() const
{
    return Point;
}

void APWSchoolGateActor::Interact(APawn* By)
{
    UPWInteractionComponent* Interaction = By != nullptr ? By->FindComponentByClass<UPWInteractionComponent>() : nullptr;
    const TSharedPtr<PWSchool::IScGateWorld> Gate = PWSchool::ScGate::Resolve(GetWorld());
    FText Message;
    if (Gate.IsValid())
    {
        // The portable core answers; this actor only presents what it said.
        LastResult = Gate->SignIn();
        Message = PWSchool::ScSignInMessage(LastResult);
    }
    else
    {
        LastResult = PWSchool::FScSignInResult();
        LastResult.Code = TEXT("school.no_world");
        Message = NSLOCTEXT("PerfectWorldSchool", "NoRegister", "The attendance register is not available here.");
    }
    LastMessage = Message;
    ++SignInCount;
    if (Interaction != nullptr)
    {
        Interaction->ShowNotice(Message, 6.0f);
    }
}

// ---- Timetable board ----

APWSchoolBoardActor::APWSchoolBoardActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 1.0f;
    Text = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Text"));
    SetRootComponent(Text);
    Text->SetMobility(EComponentMobility::Movable);
    Text->SetHorizontalAlignment(EHTA_Center);
    Text->SetVerticalAlignment(EVRTA_TextCenter);
    Text->SetTextRenderColor(FColor(244, 236, 214));
    Text->SetWorldSize(8.0f);
    Text->SetCastShadow(false);
    Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Tags.AddUnique(FName(TEXT("interaction.nabapara.school_timetable")));
}

void APWSchoolBoardActor::Configure(const FVector& PointWorld, const FVector& TextLocation, double TextYaw, float TextSizeCm)
{
    Point = PointWorld;
    Text->SetWorldSize(TextSizeCm);
    SetActorLocationAndRotation(TextLocation, FRotator(0.0, TextYaw, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
    Refresh();
}

bool APWSchoolBoardActor::Refresh()
{
    if (!PostedText.IsEmpty())
    {
        return true;
    }
    const TSharedPtr<PWSchool::IScGateWorld> Gate = PWSchool::ScGate::Resolve(GetWorld());
    const PerfectWorld::BellSchedule* Schedule = Gate.IsValid() ? Gate->GetBellSchedule() : nullptr;
    if (Schedule == nullptr)
    {
        return false;
    }
    PostedText = FString::Join(PWSchool::ScBellBoardLines(*Schedule), TEXT("\n"));
    Text->SetText(FText::FromString(PostedText));
    SetActorTickEnabled(false);
    return true;
}

void APWSchoolBoardActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Refresh();
}

FText APWSchoolBoardActor::GetInteractionVerb(const APawn* /*By*/) const
{
    return NSLOCTEXT("PerfectWorldSchool", "ReadTimetable", "Read the timetable");
}

FVector APWSchoolBoardActor::GetInteractionPoint() const
{
    return Point;
}

void APWSchoolBoardActor::Interact(APawn* By)
{
    UPWInteractionComponent* Interaction = By != nullptr ? By->FindComponentByClass<UPWInteractionComponent>() : nullptr;
    if (Interaction == nullptr)
    {
        return;
    }
    if (!Refresh())
    {
        Interaction->ShowNotice(NSLOCTEXT("PerfectWorldSchool", "NoTimetable", "The timetable has not been put up yet."), 4.0f);
        return;
    }
    FString Shown = PostedText;
    const TSharedPtr<PWSchool::IScGateWorld> Gate = PWSchool::ScGate::Resolve(GetWorld());
    PerfectWorld::SchoolArrivalView View;
    FString Why;
    if (Gate.IsValid() && Gate->QueryArrival(View, Why))
    {
        const FText Today = PWSchool::ScTodayNote(View);
        if (!Today.IsEmpty())
        {
            Shown += TEXT("\n") + Today.ToString();
        }
    }
    Interaction->ShowNotice(FText::FromString(Shown), 12.0f);
}
