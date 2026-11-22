#include "PerfectWorldAcceptanceAwakeningFloorSteps.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldPlayerCharacter.h"

// Step builders of awakening_floor. The scenario drives the REAL possessed pawn with movement input and the real interaction component's
// TryInteract; it teleports only at the declared start. What it concludes it reads from the floor director and the bridge, not from text.

namespace PWAcceptance::AwakeningFloor
{
using namespace PWAwakening;

namespace
{
FStepDef AwFlAction(const FString& Name, const double Timeout, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run)
{
    FStepDef Def;
    Def.Name = Name;
    Def.TimeoutSeconds = Timeout;
    Def.Run = MoveTemp(Run);
    return Def;
}

bool AwFlPawnInRoom(UPWAcceptanceSubsystem& Sub, const APWAwakeningFloorActor& Floor)
{
    const APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
    return Pawn != nullptr && Floor.GetLayout().Bounds.ExpandBy(FVector(20.0, 20.0, 300.0)).IsInside(Pawn->GetActorLocation());
}
} // namespace

APWAwakeningFloorActor* AwFlFindFloor(UPWAcceptanceSubsystem& Sub)
{
    UWorld* World = Sub.GetPlayWorld();
    if (World != nullptr)
    {
        for (TActorIterator<APWAwakeningFloorActor> It(World); It; ++It)
        {
            return *It;
        }
    }
    return nullptr;
}

FStepDef AwFlBuiltStep(const TSharedRef<FAwFlRun>& Run)
{
    return AwFlAction(TEXT("awakening_floor_built"), 20.0, [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const APWAwakeningFloorActor* Floor = AwFlFindFloor(Sub);
        if (Floor == nullptr)
        {
            return FStepOutcome::Fail(TEXT("the Awakening floor is not in the world: call PWAwakening::SpawnAwakeningFloor from BuildKolkataLane after SpawnSchoolCampus (see PerfectWorldAwakeningFloor.h)"));
        }
        UWorld* World = Sub.GetPlayWorld();
        if (!AwBridge::Resolve(World).IsValid())
        {
            Run->Scripted = MakeShared<FAwScriptedBridge>();
            AwBridge::Install(World, Run->Scripted);
        }
        const FAwFloorLayout& L = Floor->GetLayout();
        return FStepOutcome::Pass(FString::Printf(TEXT("floor spawned: %d stations, %d pads, %d people planned, terminal at (%.0f, %.0f); bridge: %s"), L.Stations.Num(),
                                                  L.AllPads().Num(), L.Figures.Num(), L.EntryPad.Point.X, L.EntryPad.Point.Y,
                                                  Run->Scripted.IsValid() ? TEXT("SCRIPTED fake (R9-02 is not installed)") : TEXT("the installed bridge")));
    });
}

FStepDef AwFlStartStep()
{
    return AwFlAction(TEXT("declared_start_forecourt_terminal"), 20.0, [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
        const APWAwakeningFloorActor* Floor = AwFlFindFloor(Sub);
        if (Pawn == nullptr || Floor == nullptr)
        {
            return FStepOutcome::Fail(TEXT("no pawn or no floor"));
        }
        if (Rt.Phase == 0)
        {
            // The scenario's sole teleport: the walk from the family home to the school is school_round7's.
            const FAwFloorSite& Site = Floor->GetLayout().Site;
            const FVector Start = Site.EntryStand + FVector(0.0, 0.0, PWPlayer::CapsuleHalfHeight + 5.0);
            if (!Pawn->TeleportTo(Start, FRotator(0.0, Site.EntryYaw, 0.0)))
            {
                return FStepOutcome::Fail(TEXT("the declared start at the terminal was blocked"));
            }
            Pawn->GetCharacterMovement()->Velocity = FVector::ZeroVector;
            if (Sub.GetController() != nullptr)
            {
                Sub.GetController()->SetControlRotation(FRotator(0.0, Site.EntryYaw, 0.0));
            }
            Rt.NextPhase();
        }
        if (Rt.InPhase() < 0.8 || !Pawn->GetCharacterMovement()->IsMovingOnGround())
        {
            return FStepOutcome::Wait();
        }
        return FStepOutcome::Pass(TEXT("standing at the forecourt terminal; every later move is walked or a transfer the floor itself makes"));
    });
}

FStepDef AwFlCameraStep(const FString& Name, const bool bThird)
{
    return AwFlAction(Name, 10.0, [bThird](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (Sub.GetPawn() == nullptr)
        {
            return FStepOutcome::Fail(TEXT("no pawn"));
        }
        Sub.GetPawn()->SetCameraMode(bThird ? EPWCameraMode::ThirdPerson : EPWCameraMode::FirstPerson);
        return FStepOutcome::Pass(bThird ? TEXT("third-person camera") : TEXT("first-person camera"));
    });
}

FStepDef AwFlEnterStep(const FString& Name, const TSharedRef<FAwFlRun>& Run, const int32 ExpectedEnters)
{
    return MakeInteractStep(Name,
        [](UPWAcceptanceSubsystem& Sub) -> AActor*
        {
            const APWAwakeningFloorActor* Floor = AwFlFindFloor(Sub);
            return Floor != nullptr ? Floor->FindPad(Floor->GetLayout().EntryPad.Id) : nullptr;
        },
        [Run, ExpectedEnters](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const APWAwakeningFloorActor* Floor = AwFlFindFloor(Sub);
            if (Floor == nullptr || !Floor->IsOccupied() || Floor->IsTransitioning() || !AwFlPawnInRoom(Sub, *Floor))
            {
                return false;
            }
            if (Run->Scripted.IsValid() && Run->Scripted->CountCalls(TEXT("enter")) < ExpectedEnters)
            {
                return false;
            }
            Detail = FString::Printf(TEXT("the pawn is in the room, the transfer is over, %d people are with the class; caption: %s"), Floor->GetFigureCount(),
                                     *Floor->GetLastCaption().ToString().Left(60));
            return true;
        },
        nullptr, 25.0);
}

FStepDef AwFlWalkToStationStep(const FString& Name, const int32 Station)
{
    return MakeWalkStep(Name,
        [Station](UPWAcceptanceSubsystem& Sub, FVector& OutTarget, FString& Why)
        {
            const APWAwakeningFloorActor* Floor = AwFlFindFloor(Sub);
            if (Floor == nullptr || !Floor->GetLayout().Stations.IsValidIndex(Station))
            {
                Why = TEXT("no floor or no such station");
                return false;
            }
            OutTarget = Floor->GetLayout().Stations[Station].Stand;
            return true;
        },
        35.0, 60.0);
}

FStepDef AwFlChoiceStep(const FString& Name, const TSharedRef<FAwFlRun>& Run, const int32 Station, const int32 PadIndex, const bool bExpectSafeArea)
{
    return MakeInteractStep(Name,
        [Station, PadIndex](UPWAcceptanceSubsystem& Sub) -> AActor*
        {
            const APWAwakeningFloorActor* Floor = AwFlFindFloor(Sub);
            if (Floor == nullptr || !Floor->GetLayout().Stations.IsValidIndex(Station))
            {
                return nullptr;
            }
            return Floor->FindPad(Floor->GetLayout().Stations[Station].Pads[PadIndex].Id);
        },
        [Run, Station, PadIndex, bExpectSafeArea](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const APWAwakeningFloorActor* Floor = AwFlFindFloor(Sub);
            if (Floor == nullptr || Floor->IsTransitioning())
            {
                return false;
            }
            const FAwPadSpec& Spec = Floor->GetLayout().Stations[Station].Pads[PadIndex];
            const FAwChoiceDef* Def = AwFindChoice(Spec.Situation, Spec.Choice);
            if (Def == nullptr)
            {
                return false;
            }
            if (Run->Scripted.IsValid() && Run->Scripted->CountSubmits(Spec.Situation, Spec.Choice) < 1)
            {
                return false; // the choice has not reached the bridge
            }
            if (Floor->GetPhase(Station) != EAwPhase::Complete)
            {
                return false;
            }
            // The consequence is observable: the moving parts the choice orders are at their poses and at rest.
            const APWAwakeningMoverActor* Primary = Floor->GetMover(Station, false);
            const APWAwakeningMoverActor* Secondary = Floor->GetMover(Station, true);
            if (Def->Effect.PrimaryPose >= 0 && (Primary == nullptr || Primary->GetPose() != Def->Effect.PrimaryPose || Primary->IsMoving()))
            {
                return false;
            }
            if (Def->Effect.SecondaryPose >= 0 && (Secondary == nullptr || Secondary->GetPose() != Def->Effect.SecondaryPose || Secondary->IsMoving()))
            {
                return false;
            }
            if (bExpectSafeArea)
            {
                const APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
                if (Pawn == nullptr || !Floor->GetLayout().SafeArea.ExpandBy(FVector(10.0, 10.0, 400.0)).IsInside(Pawn->GetActorLocation()))
                {
                    return false;
                }
            }
            Detail = FString::Printf(TEXT("%s reached the bridge, the situation is complete%s; caption: %s"), *Spec.Choice.ToString(),
                                     bExpectSafeArea ? TEXT(", the pawn is in the safe area") : TEXT(", the moving parts are at their poses"),
                                     *Floor->GetLastCaption().ToString().Left(80));
            return true;
        },
        nullptr, 30.0);
}

FStepDef AwFlSessionCompleteStep(const FString& Name, const TSharedRef<FAwFlRun>& Run)
{
    return AwFlAction(Name, 10.0, [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const APWAwakeningFloorActor* Floor = AwFlFindFloor(Sub);
        if (Floor == nullptr || !Floor->IsSessionComplete())
        {
            return FStepOutcome::Fail(TEXT("the bridge has not reported the session complete"));
        }
        for (int32 S = 0; S < SituationCount; ++S)
        {
            if (Floor->GetPhase(S) != EAwPhase::Complete)
            {
                return FStepOutcome::Fail(FString::Printf(TEXT("station %d is not complete"), S + 1));
            }
        }
        const FString Text = Floor->GetLastCaption().ToString();
        return FStepOutcome::Pass(FString::Printf(TEXT("all three situations complete%s; last caption: %s"), Run->Scripted.IsValid() ? TEXT(" (scripted bridge)") : TEXT(""),
                                                  *Text.Left(80)));
    });
}

FStepDef AwFlWalkToReturnStep(const FString& Name)
{
    return MakeWalkStep(Name,
        [](UPWAcceptanceSubsystem& Sub, FVector& OutTarget, FString& Why)
        {
            const APWAwakeningFloorActor* Floor = AwFlFindFloor(Sub);
            if (Floor == nullptr)
            {
                Why = TEXT("no floor");
                return false;
            }
            OutTarget = Floor->GetLayout().LeavePad.Centre - FVector(0.0, 70.0, 0.0);
            return true;
        },
        35.0, 60.0);
}

FStepDef AwFlLeaveStep(const FString& Name, const TSharedRef<FAwFlRun>& Run, const int32 ExpectedLeaves)
{
    return MakeInteractStep(Name,
        [](UPWAcceptanceSubsystem& Sub) -> AActor*
        {
            const APWAwakeningFloorActor* Floor = AwFlFindFloor(Sub);
            return Floor != nullptr ? Floor->FindPad(Floor->GetLayout().LeavePad.Id) : nullptr;
        },
        [Run, ExpectedLeaves](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const APWAwakeningFloorActor* Floor = AwFlFindFloor(Sub);
            const APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
            if (Floor == nullptr || Pawn == nullptr || Floor->IsOccupied() || Floor->IsTransitioning())
            {
                return false;
            }
            if (Run->Scripted.IsValid() && Run->Scripted->CountCalls(TEXT("leave")) < ExpectedLeaves)
            {
                return false;
            }
            if (FVector::Dist2D(Pawn->GetActorLocation(), Floor->GetLayout().Site.EntryStand) > 200.0)
            {
                return false;
            }
            Detail = FString::Printf(TEXT("back at the terminal (%d people left with the session)"), Floor->GetFigureCount());
            return Floor->GetFigureCount() == 0;
        },
        nullptr, 25.0);
}

FStepDef AwFlResetStep(const FString& Name, const TSharedRef<FAwFlRun>& Run)
{
    return AwFlAction(Name, 5.0, [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (Run->Scripted.IsValid())
        {
            Run->Scripted->ResetSession();
            return FStepOutcome::Pass(TEXT("a fresh scripted session for the withdrawal route (the fake allows replay; a real life does not)"));
        }
        Run->bSkipSecondPass = true;
        return FStepOutcome::Pass(TEXT("a real bridge is installed and cannot replay a completed session: the withdrawal pass is skipped; run the scenario on a fresh life with the helping pass disabled to cover it"));
    });
}

FStepDef AwFlSummaryStep(const FString& Name, const TSharedRef<FAwFlRun>& Run)
{
    return AwFlAction(Name, 5.0, [Run](UPWAcceptanceSubsystem&, FStepRuntime&)
    {
        if (!Run->Scripted.IsValid())
        {
            return FStepOutcome::Pass(TEXT("a real bridge was in use: its own records are the evidence (no scripted counts asserted)"));
        }
        const FAwScriptedBridge& B = *Run->Scripted;
        struct FExpected
        {
            const TCHAR* Situation;
            const TCHAR* Choice;
        };
        const FExpected Expected[] = {{Ids::StalledCrossing, Ids::HelpClassmate},     {Ids::ShiftingCorridor, Ids::ProtectBarrier},
                                      {Ids::SharedDelivery, Ids::CoordinateCarriers}, {Ids::StalledCrossing, Ids::WithdrawToSafety},
                                      {Ids::ShiftingCorridor, Ids::WithdrawToSafety}, {Ids::SharedDelivery, Ids::DeclineConfrontation}};
        for (const FExpected& E : Expected)
        {
            if (B.CountSubmits(FName(E.Situation), FName(E.Choice)) != 1)
            {
                return FStepOutcome::Fail(FString::Printf(TEXT("%s / %s reached the bridge %d times, wanted 1"), E.Situation, E.Choice,
                                                          B.CountSubmits(FName(E.Situation), FName(E.Choice))));
            }
        }
        if (B.CountCalls(TEXT("submit")) != 6 || B.CountCalls(TEXT("enter")) != 2 || B.CountCalls(TEXT("leave")) != 2)
        {
            return FStepOutcome::Fail(FString::Printf(TEXT("bridge calls: %d submits (6), %d enters (2), %d leaves (2)"), B.CountCalls(TEXT("submit")),
                                                      B.CountCalls(TEXT("enter")), B.CountCalls(TEXT("leave"))));
        }
        return FStepOutcome::Pass(TEXT("the bridge saw exactly: help, protect, coordinate; then withdraw, withdraw, decline the confrontation; two entries and two exits"));
    });
}

FStepDef AwFlSecondPass(FStepDef Step, const TSharedRef<FAwFlRun>& Run)
{
    const TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Inner = Step.Run;
    Step.Run = [Inner, Run](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        if (Run->bSkipSecondPass)
        {
            return FStepOutcome::Pass(TEXT("skipped: a real bridge cannot replay a completed session"));
        }
        return Inner(Sub, Rt);
    };
    return Step;
}
} // namespace PWAcceptance::AwakeningFloor
