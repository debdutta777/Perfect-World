#include "PerfectWorldAcceptanceSubsystem.h"
#include "PerfectWorldAcceptanceFrontages.h"
#include "PerfectWorldLaneTallFlats.h"

#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PerfectWorldBlockTestGameMode.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldFurnitureActor.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneBlocks.h"
#include "PerfectWorldLaneHomeFurniture.h"
#include "PerfectWorldLaneInteriors.h"
#include "PerfectWorldPlayerCharacter.h"
#include "UnrealClient.h"

// The scenario steps every scenario shares (boot, enter the world, walk, interact, screenshot) and the first scenario,
// boot_walk_interact: from the family home's entry to the living-room doorway and to the table chair, sit, and stand again.
// See PerfectWorldAcceptanceSubsystem.h for the launch line and the frozen result contract.

namespace
{
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

constexpr double AwEntryTolerance = 220.0;   // how far from the authored entry point a family start may stand
constexpr double AwDoorwayTolerance = 40.0;
constexpr double AwStandTolerance = 30.0;
constexpr double AwWaypointTolerance = 35.0; // intermediate route points
constexpr double AwAdultClearanceCm = 75.0;  // a route point must stay this far from the household adult (two capsules of 34 cm plus slack)
constexpr double AwSettleSeconds = 0.8;      // after entering the world, before the first assertion

/** The opening home's frame, as PerfectWorldKolkataLane.cpp builds it: origin (-300, -400), U = +X, V = -Y. */
PWLane::FLaneFrame AwHomeFrame()
{
    return PWLane::MakeLaneFrame(-300.0, -400.0, FVector(1.0, 0.0, 0.0), FVector(0.0, -1.0, 0.0));
}

FVector AwHomeEntryPoint()
{
    return PWLane::HomeWorldPoint(AwHomeFrame(), PWLane::HomeFrontDoorU + 35.0, 180.0, 0.0);
}

APWSeatActor* AwFindSeat(UWorld* World, const FName SeatId)
{
    if (World == nullptr)
    {
        return nullptr;
    }
    for (TActorIterator<APWSeatActor> It(World); It; ++It)
    {
        if (It->GetSeatId() == SeatId)
        {
            return *It;
        }
    }
    return nullptr;
}

const FName AwTableChairId(TEXT("seat.nabapara.home_table_chair"));

bool AwFindSeatSpec(const FName SeatId, PWLane::FHomeSeatSpec& Out)
{
    for (const PWLane::FHomeSeatSpec& Spec : PWLane::HomeSeatSpecs())
    {
        if (Spec.Id == SeatId)
        {
            Out = Spec;
            return true;
        }
    }
    return false;
}

template <typename TActor>
int32 AwCountActors(UWorld* World)
{
    int32 Count = 0;
    for (TActorIterator<TActor> It(World); It; ++It)
    {
        ++Count;
    }
    return Count;
}
} // namespace

namespace PWAcceptance
{
FStepDef MakeWorldReadyStep()
{
    FStepDef Def;
    Def.Name = TEXT("world_ready");
    Def.TimeoutSeconds = 90.0;
    Def.Run = [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        if (FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
        {
            return FStepOutcome::Fail(TEXT("-NullRHI cannot render a screenshot; use a real RHI with -RenderOffScreen"));
        }
        UWorld* World = Sub.GetPlayWorld();
        if (World == nullptr || !World->HasBegunPlay())
        {
            Rt.Note = TEXT("the world has not begun play");
            return FStepOutcome::Wait();
        }
        const APerfectWorldFrontEndHUD* Hud = Sub.GetHud();
        const APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
        const UPerfectWorldCharacterCreatorSubsystem* Creator = Hud != nullptr ? Hud->GetCreator() : nullptr;
        const bool bProduction = Cast<APerfectWorldBlockTestGameMode>(World->GetAuthGameMode()) != nullptr;
        const int32 Seats = AwCountActors<APWSeatActor>(World);
        const int32 Doors = AwCountActors<APWDoorActor>(World);
        Rt.Note = FString::Printf(TEXT("hud=%d pawn=%d creator=%d production_game_mode=%d seats=%d doors=%d viewport=%d"), Hud != nullptr,
                                  Pawn != nullptr, Creator != nullptr, bProduction, Seats, Doors,
                                  GEngine != nullptr && GEngine->GameViewport != nullptr && GEngine->GameViewport->Viewport != nullptr);
        if (Hud == nullptr || Pawn == nullptr || Creator == nullptr || !bProduction || Seats == 0 || Doors == 0 || !Hud->IsMenuOpen() ||
            GEngine == nullptr || GEngine->GameViewport == nullptr || GEngine->GameViewport->Viewport == nullptr)
        {
            return FStepOutcome::Wait();
        }
        const FIntPoint View = GEngine->GameViewport->Viewport->GetSizeXY();
        return FStepOutcome::Pass(FString::Printf(TEXT("map %s, title up, %d seats and %d doors built, viewport %dx%d, can render %d, window focus %d, off-screen flag %d"),
                                                  *World->GetMapName(), Seats, Doors, View.X, View.Y, FApp::CanEverRender(), FApp::HasFocus(),
                                                  FParse::Param(FCommandLine::Get(), TEXT("RenderOffScreen"))));
    };
    return Def;
}

FStepDef MakeEnterWorldStep()
{
    FStepDef Def;
    Def.Name = TEXT("enter_world");
    Def.TimeoutSeconds = 40.0;
    Def.Run = [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        APerfectWorldFrontEndHUD* Hud = Sub.GetHud();
        APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
        if (Hud == nullptr || Pawn == nullptr)
        {
            return FStepOutcome::Fail(TEXT("the HUD or the player character vanished"));
        }
        if (Rt.Phase == 0)
        {
            const int32 SavedSlot = UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot();
            const FString& Start = Sub.GetArgs().Start;
            const bool bContinue = Start == TEXT("continue") || (Start == TEXT("auto") && SavedSlot >= 0);
            if (bContinue)
            {
                const FText Why = Hud->ContinueLatest(); // the same call the title's Continue button makes
                if (!Why.IsEmpty())
                {
                    return FStepOutcome::Fail(FString::Printf(TEXT("Continue refused: %s"), *Why.ToString()));
                }
                Rt.Memo = FString::Printf(TEXT("continued slot %d"), SavedSlot);
            }
            else
            {
                UPerfectWorldCharacterCreatorSubsystem* Creator = Hud->GetCreator();
                const int32 Slot = Sub.GetArgs().Slot >= 0 ? Sub.GetArgs().Slot : UPerfectWorldCharacterCreatorSubsystem::FindFirstEmptySaveSlot();
                if (Creator == nullptr || Slot < 0)
                {
                    return FStepOutcome::Fail(TEXT("no creator, or every save slot is occupied (use -PWAcceptanceSlot= or a clean save directory)"));
                }
                Hud->BeginNewCreation();
                if (!Creator->ApplyBalancedScholarPreset())
                {
                    return FStepOutcome::Fail(FString::Printf(TEXT("the balanced scholar preset was refused: %s"), *Creator->GetLastErrorMessage().ToString()));
                }
                const FText Why = Hud->StartNewLife(Slot); // the same call the review page's Start button makes
                if (!Why.IsEmpty())
                {
                    return FStepOutcome::Fail(FString::Printf(TEXT("starting a new life was refused: %s"), *Why.ToString()));
                }
                Rt.Memo = FString::Printf(TEXT("new life in slot %d"), Slot);
            }
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        const bool bGrounded = Pawn->GetCharacterMovement() != nullptr && Pawn->GetCharacterMovement()->IsMovingOnGround();
        Rt.Note = FString::Printf(TEXT("menu open %d, grounded %d, started %d"), Hud->IsMenuOpen(), bGrounded,
                                  Hud->GetCreator() != nullptr && Hud->GetCreator()->HasLifeStarted());
        if (Hud->IsMenuOpen() || !bGrounded || Rt.InPhase() < AwSettleSeconds)
        {
            return FStepOutcome::Wait();
        }
        const FVector Here = Pawn->GetActorLocation();
        const FPWLaneOriginView Origin = Hud->GetCreator()->GetLaneOrigin();
        return FStepOutcome::Pass(FString::Printf(TEXT("%s; in the world at (%.0f, %.0f, %.0f), %s origin"), *Rt.Memo, Here.X, Here.Y, Here.Z,
                                                  Origin.bGrewUpInCare ? TEXT("care") : TEXT("family")));
    };
    return Def;
}

FStepDef MakeShotStep(const FString& StepName)
{
    FStepDef Def;
    Def.Name = StepName;
    Def.TimeoutSeconds = 30.0;
    Def.Run = [StepName](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        if (Rt.Phase == 0)
        {
            if (Rt.Elapsed < 0.3)
            {
                return FStepOutcome::Wait(); // let the last frame of movement and the prompt settle
            }
            Sub.RequestShot(StepName, Rt);
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        return Sub.TickShot(Rt);
    };
    return Def;
}

FStepDef MakeWalkStep(const FString& StepName, TFunction<bool(UPWAcceptanceSubsystem&, FVector&, FString&)> FindTarget,
                      const double ToleranceCm, const double TimeoutSeconds)
{
    FStepDef Def;
    Def.Name = StepName;
    Def.TimeoutSeconds = TimeoutSeconds;
    Def.Run = [FindTarget, ToleranceCm](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        if (!Rt.bHaveTarget)
        {
            FString Why;
            if (!FindTarget(Sub, Rt.Target, Why))
            {
                return FStepOutcome::Fail(Why);
            }
            Rt.bHaveTarget = true;
        }
        return Sub.TickWalk(Rt.Target, ToleranceCm, Rt);
    };
    return Def;
}

FStepDef MakeInteractStep(const FString& StepName, TFunction<AActor*(UPWAcceptanceSubsystem&)> FindTarget,
                          TFunction<bool(UPWAcceptanceSubsystem&, FString&)> IsDone, TFunction<bool(UPWAcceptanceSubsystem&)> SkipIf,
                          const double TimeoutSeconds)
{
    FStepDef Def;
    Def.Name = StepName;
    Def.TimeoutSeconds = TimeoutSeconds;
    Def.Run = [FindTarget, IsDone, SkipIf](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        if (Rt.Phase == 0)
        {
            if (SkipIf && SkipIf(Sub))
            {
                return FStepOutcome::Pass(TEXT("nothing to do: the state the interaction would reach is already true"));
            }
            AActor* Target = FindTarget(Sub);
            UPWInteractionComponent* Interaction = Sub.GetInteraction();
            const IPWInteractable* Interactable = Cast<IPWInteractable>(Target);
            if (Target == nullptr || Interaction == nullptr || Interactable == nullptr)
            {
                return FStepOutcome::Fail(TEXT("the interaction target, or the player's interaction component, does not exist"));
            }
            Sub.FaceWorldPoint(Interactable->GetInteractionPoint());
            const AActor* Current = Interaction->GetTarget();
            Rt.Note = FString::Printf(TEXT("prompt target %s, wanted %s, notice '%s'"), Current != nullptr ? *Current->GetName() : TEXT("none"),
                                      *Target->GetName(), *Interaction->GetNotice().ToString());
            if (Current != Target || !Interaction->IsPromptVisible())
            {
                return FStepOutcome::Wait(); // the component re-evaluates ten times a second
            }
            Rt.Memo = Interaction->GetPromptVerb().ToString();
            if (UPWInteractionComponent::IsWorldInputBlocked(Sub.GetPawn())) // the E key is ignored while a menu or conversation is up
            {
                return FStepOutcome::Fail(TEXT("a menu or conversation owns the input, so the interact key would be ignored"));
            }
            if (!Interaction->TryInteract()) // the call the E key makes
            {
                return FStepOutcome::Fail(FString::Printf(TEXT("TryInteract refused on '%s'"), *Rt.Memo));
            }
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        FString Detail;
        if (IsDone(Sub, Detail))
        {
            return FStepOutcome::Pass(FString::Printf(TEXT("prompt '%s' used; %s"), *Rt.Memo, *Detail));
        }
        const UPWInteractionComponent* Interaction = Sub.GetInteraction();
        Rt.Note = FString::Printf(TEXT("interacted with '%s' but the expected result did not arrive; notice '%s'"), *Rt.Memo,
                                  Interaction != nullptr ? *Interaction->GetNotice().ToString() : TEXT(""));
        return FStepOutcome::Wait();
    };
    return Def;
}

namespace
{
/**
 * The way from the living doorway to the table chair's stand point, in building-local U, V. A straight line is blocked: the household
 * adult stands at HomeAdultSpot (U 520, V 290) facing the doorway, 125 cm in. So the route goes through the doorway's centre, keeps
 * V 370+ past the adult (the kitchen door's swing zone, U 445-547 V 345+, is closed by default), then turns down the room at
 * U 405, 120 cm west of the adult, to the stand point (U 393, V 215). Every point is checked against the adult's spot at run time.
 */
bool AwChairRoute(TArray<FVector>& OutRoute, FString& Why)
{
    PWLane::FHomeSeatSpec Spec;
    if (!AwFindSeatSpec(AwTableChairId, Spec))
    {
        Why = TEXT("the authored seat specs no longer name seat.nabapara.home_table_chair");
        return false;
    }
    const PWLane::FLaneFrame Frame = AwHomeFrame();
    const double Points[][2] = {{630.0, 365.0}, {560.0, 375.0}, {440.0, 370.0}, {405.0, 265.0}, {Spec.StandU, Spec.StandV}};
    const PWLane::FHomeAdultSpot Adult = PWLane::HomeAdultSpot();
    OutRoute.Reset();
    for (const double (&Point)[2] : Points)
    {
        if (FVector2D::Distance(FVector2D(Point[0], Point[1]), FVector2D(Adult.U, Adult.V)) < AwAdultClearanceCm)
        {
            Why = FString::Printf(TEXT("the route point (U %.0f, V %.0f) is within %.0f cm of the household adult's spot; the layout changed"),
                                  Point[0], Point[1], AwAdultClearanceCm);
            return false;
        }
        OutRoute.Add(PWLane::HomeWorldPoint(Frame, Point[0], Point[1], 0.0));
    }
    return true;
}

/** Walks AwChairRoute point by point with real movement input; the stuck detector restarts at every point. */
FStepDef AwRouteStep(const FString& StepName, const double TimeoutSeconds)
{
    FStepDef Def;
    Def.Name = StepName;
    Def.TimeoutSeconds = TimeoutSeconds;
    Def.Run = [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        TArray<FVector> Route;
        FString Why;
        if (!AwChairRoute(Route, Why))
        {
            return FStepOutcome::Fail(Why);
        }
        const bool bLast = Rt.Phase >= Route.Num() - 1;
        const FStepOutcome Outcome = Sub.TickWalk(Route[FMath::Min(Rt.Phase, Route.Num() - 1)], bLast ? AwStandTolerance : AwWaypointTolerance, Rt);
        if (Outcome.State == EStepState::Passed && !bLast)
        {
            Rt.NextPhase();
            Rt.LastProbe = 0.0;
            Rt.StuckSince = 0.0;
            return FStepOutcome::Wait();
        }
        if (Outcome.State == EStepState::Passed)
        {
            return FStepOutcome::Pass(FString::Printf(TEXT("%d route points; %s"), Route.Num(), *Outcome.Detail));
        }
        if (Outcome.State == EStepState::Failed)
        {
            return FStepOutcome::Fail(FString::Printf(TEXT("route point %d of %d: %s"), Rt.Phase + 1, Route.Num(), *Outcome.Detail));
        }
        return Outcome;
    };
    return Def;
}

void AwAppendBootWalkInteract(TArray<FStepDef>& Out)
{
    Out.Add(MakeWorldReadyStep());
    Out.Add(MakeEnterWorldStep());
    Out.Add(MakeWalkStep(TEXT("walk_to_living_doorway"),
        [](UPWAcceptanceSubsystem& Sub, FVector& OutTarget, FString& Why)
        {
            const APerfectWorldFrontEndHUD* Hud = Sub.GetHud();
            const APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
            if (Hud == nullptr || Hud->GetCreator() == nullptr || Pawn == nullptr || Hud->GetCreator()->GetLaneOrigin().bGrewUpInCare)
            {
                Why = TEXT("this scenario walks the family home; the life has a care origin (use -PWAcceptanceStart=new with a clean save directory)");
                return false;
            }
            const double FromEntry = FVector::Dist2D(Pawn->GetActorLocation(), AwHomeEntryPoint());
            if (FromEntry > AwEntryTolerance)
            {
                Why = FString::Printf(TEXT("the pawn starts %.0f cm from the family-home entry (the save placed it elsewhere); use -PWAcceptanceStart=new"), FromEntry);
                return false;
            }
            OutTarget = PWLane::HomeWorldPoint(AwHomeFrame(), PWLane::HomeLivingPartitionU, PWLane::HomeLivingDoorV, 0.0);
            return true;
        },
        AwDoorwayTolerance, 30.0));
    Out.Add(MakeShotStep(TEXT("shot_living_doorway")));
    Out.Add(AwRouteStep(TEXT("walk_to_table_chair"), 40.0));
    Out.Add(MakeInteractStep(TEXT("sit_at_table"),
        [](UPWAcceptanceSubsystem& Sub) -> AActor* { return AwFindSeat(Sub.GetPlayWorld(), AwTableChairId); },
        [](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
            const APWSeatActor* Seat = AwFindSeat(Sub.GetPlayWorld(), AwTableChairId);
            if (Pawn == nullptr || Seat == nullptr || Pawn->GetSeatState() != EPWSeatState::Seated || !Pawn->IsSeatedOn(Seat))
            {
                return false;
            }
            Detail = TEXT("the pawn is seated on seat.nabapara.home_table_chair");
            return true;
        },
        nullptr, 15.0));
    Out.Add(MakeShotStep(TEXT("shot_seated")));
    Out.Add(MakeInteractStep(TEXT("stand_up"),
        [](UPWAcceptanceSubsystem& Sub) -> AActor* { return AwFindSeat(Sub.GetPlayWorld(), AwTableChairId); },
        [](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
            if (Pawn == nullptr || Pawn->GetSeatState() != EPWSeatState::Standing)
            {
                return false;
            }
            Detail = TEXT("the pawn is standing again");
            return true;
        },
        nullptr, 15.0));
}
} // namespace

bool BuildScenario(const FString& Name, TArray<FStepDef>& OutSteps)
{
    OutSteps.Reset();
    if (Name == TEXT("boot_walk_interact"))
    {
        AwAppendBootWalkInteract(OutSteps);
        return true;
    }
    if (Name == TEXT("save_restart"))
    {
        AppendSaveRestartSteps(OutSteps);
        return true;
    }
    if (Name == TEXT("doors_knock"))
    {
        AppendDoorsKnockSteps(OutSteps);
        return true;
    }
    if (Name == TEXT("anindita_mira"))
    {
        AppendAninditaMiraSteps(OutSteps);
        return true;
    }
    if (Name == TEXT("origins_seating_cameras"))
    {
        AppendOriginsSeatingCamerasSteps(OutSteps);
        return true;
    }
    if (Name == TEXT("resident_homes"))
    {
        AppendResidentHomesSteps(OutSteps);
        return true;
    }
    if (Name == TEXT("frontage_three"))
    {
        AppendFrontageThreeSteps(OutSteps);
        return true;
    }
    if (Name == TEXT("tall_flats_upstairs"))
    {
        AppendTallFlatsUpstairsSteps(OutSteps);
        return true;
    }
    if (Name == TEXT("stationery_shift"))
    {
        AppendStationeryShiftSteps(OutSteps);
        return true;
    }
    return false;
}
} // namespace PWAcceptance
