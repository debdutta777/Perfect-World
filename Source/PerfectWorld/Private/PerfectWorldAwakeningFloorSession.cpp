#include "PerfectWorldAwakeningActors.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "PerfectWorldAwakeningCatalogue.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldLaneLifeSubsystem.h" // P38
#include "PerfectWorldSchoolPresenter.h"

// The floor's session: the people of the floor, stepping on (asking the bridge first) and stepping back onto the campus.

#define LOCTEXT_NAMESPACE "PerfectWorldAwakeningSession"

using namespace PWAwakening;

namespace
{
const FName AwFlFigureTag(TEXT("awakening.floor.figure"));
const FName AwFlTeacherTag(TEXT("awakening.floor.teacher"));
} // namespace

// ---- People ----

void APWAwakeningFloorActor::SpawnFigures()
{
    UWorld* World = GetWorld();
    if (World == nullptr || GetFigureCount() > 0)
    {
        return;
    }
    Figures.Reset();
    for (const FAwFigureSpec& Spec : Layout.Figures)
    {
        APWNpcCharacter* Npc = APWNpcCharacter::SpawnAt(*World, FTransform(FRotator(0.0, Spec.Yaw, 0.0), Spec.Feet), Spec.PersonId,
                                                        FText::FromString(Spec.Name), EPWNpcOutfit::HouseholdAdult);
        if (Npc != nullptr)
        {
            Npc->Tags.AddUnique(AwFlFigureTag);
            if (Spec.bTeacher)
            {
                Npc->Tags.AddUnique(AwFlTeacherTag);
            }
            Figures.Add(Npc);
        }
    }
}

void APWAwakeningFloorActor::DespawnFigures()
{
    for (const TWeakObjectPtr<ACharacter>& Figure : Figures)
    {
        if (Figure.IsValid())
        {
            Figure->Destroy();
        }
    }
    Figures.Reset();
}

// ---- Entering and leaving ----

bool APWAwakeningFloorActor::TryEnter(APawn* By)
{
    // P38: the campus terminal remains useful after the immutable grant.
    if (const auto* P38Life = UPWLaneLifeSubsystem::Get(this); P38Life != nullptr && P38Life->GetWorldState().Awakening.Granted && !bOccupied)
    {
        auto* P38Presenter = UPWSchoolPresenter::Get(this);
        return P38Presenter != nullptr && P38Presenter->OpenAwakeningCountdown();
    }
    if (bOccupied || IsTransitioning())
    {
        return false;
    }
    const TSharedPtr<IAwFloorBridge> Bridge = AwBridge::Resolve(GetWorld());
    if (!Bridge.IsValid())
    {
        LastCode = TEXT("awakening.floor.no_bridge");
        Announce(LOCTEXT("NoBridge", "The supervised floor is not open right now."));
        return false;
    }
    FString Why;
    if (!Bridge->CanEnter(Why))
    {
        LastCode = TEXT("awakening.floor.not_open");
        Announce(Why.IsEmpty() ? LOCTEXT("NotOpen", "The supervised floor is not open right now.") : FText::FromString(Why));
        return false;
    }
    const FAwEnterResult Entered = Bridge->Enter();
    LastCode = Entered.Code;
    if (!Entered.bAccepted)
    {
        Announce(Entered.Message.IsEmpty() ? LOCTEXT("EnterRefused", "The supervised floor is not open right now.") : FText::FromString(Entered.Message));
        return false;
    }
    const TWeakObjectPtr<APawn> Weak(By);
    StartTransfer(FText::Format(LOCTEXT("Welcome", "{0}: Welcome. This is a supervised practice floor. Take your time: nothing here can hurt you, and you may withdraw to the safe area whenever you like."),
                                Config.Cast.TeacherName),
                  [this, Weak]() { DoEnterMove(Weak.Get()); });
    return true;
}

void APWAwakeningFloorActor::DoEnterMove(APawn* By)
{
    bOccupied = true;
    SpawnFigures();
    const TSharedPtr<IAwFloorBridge> Bridge = AwBridge::Resolve(GetWorld());
    if (Bridge.IsValid())
    {
        ApplyStatus(Bridge->GetStatus());
    }
    for (int32 I = 0; I < SituationCount; ++I)
    {
        if (Phases[I] != EAwPhase::Complete)
        {
            ResetMovers(I);
        }
    }
    Place(By, Layout.Arrival, Layout.ArrivalYaw);
}

bool APWAwakeningFloorActor::TryLeave(APawn* By)
{
    if (!bOccupied || IsTransitioning())
    {
        return false;
    }
    const TSharedPtr<IAwFloorBridge> Bridge = AwBridge::Resolve(GetWorld());
    if (Bridge.IsValid())
    {
        Bridge->Leave();
        if (Bridge->GetStatus().bSessionOpen) // P38: a refused withdrawal must not abandon a supervised core session.
        {
            Announce(LOCTEXT("P38LeaveRefused", "The supervised session could not close. Try the safe-area withdrawal again."));
            return false;
        }
    }
    const TWeakObjectPtr<APawn> Weak(By);
    StartTransfer(LOCTEXT("Back", "You step back onto the campus."), [this, Weak]() { DoLeaveMove(Weak.Get()); });
    return true;
}

void APWAwakeningFloorActor::DoLeaveMove(APawn* By)
{
    bOccupied = false;
    DespawnFigures();
    Hud->SetCue(FText::GetEmpty());
    Place(By, Layout.Site.EntryStand, Layout.Site.EntryYaw);
}

#undef LOCTEXT_NAMESPACE
