#include "PerfectWorldAwakeningActors.h"

#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "PerfectWorldAwakeningCatalogue.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldShopDisplayActor.h"

// The floor director: set-up, state, transfers (fade, or an announced cut under reduced motion), the hazard poll, the people.
// Choices, effects and resets are in PerfectWorldAwakeningFloorChoice.cpp.

#define LOCTEXT_NAMESPACE "PerfectWorldAwakeningFloor"

using namespace PWAwakening;

namespace
{
constexpr float AwFlHazardPollSeconds = 0.2f;
} // namespace

APWAwakeningFloorActor::APWAwakeningFloorActor()
{
    PrimaryActorTick.bCanEverTick = true;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Root->SetMobility(EComponentMobility::Movable);
    SetRootComponent(Root);
    Tags.AddUnique(FName(TEXT("awakening.floor.director")));
    Hud = MakeShared<FAwHud>();
}

void APWAwakeningFloorActor::Initialise(const FAwFloorLayout& InLayout, const FAwFloorConfig& InConfig)
{
    Layout = InLayout;
    Config = InConfig;
    Config.TextScale = FMath::Clamp(Config.TextScale, 1.0f, 2.0f);
    Hud->SetTextScale(Config.TextScale);
    for (int32 I = 0; I < SituationCount; ++I)
    {
        Phases[I] = EAwPhase::Available;
    }
    SetActorLocation(Layout.Site.Origin, false, nullptr, ETeleportType::TeleportPhysics);
}

void APWAwakeningFloorActor::RegisterPad(APWAwakeningPadActor* Pad)
{
    if (Pad != nullptr)
    {
        Pads.Add(Pad);
    }
}

void APWAwakeningFloorActor::RegisterMover(const int32 Station, const bool bSecondary, APWAwakeningMoverActor* Mover)
{
    if (Station >= 0 && Station < SituationCount && Mover != nullptr)
    {
        Movers[Station][bSecondary ? 1 : 0] = Mover;
        Mover->SetReducedMotionOverride(Config.ReducedMotion);
    }
}

void APWAwakeningFloorActor::RegisterStatusSign(const int32 Station, APWShopHoursSignActor* Sign)
{
    if (Station >= 0 && Station < SituationCount)
    {
        StatusSigns[Station] = Sign;
        RefreshStatusSign(Station);
    }
}

void APWAwakeningFloorActor::SetReducedMotionOverride(const TOptional<bool>& Override)
{
    Config.ReducedMotion = Override;
    for (int32 S = 0; S < SituationCount; ++S)
    {
        for (int32 K = 0; K < 2; ++K)
        {
            if (APWAwakeningMoverActor* Mover = Movers[S][K].Get())
            {
                Mover->SetReducedMotionOverride(Override);
            }
        }
    }
}

void APWAwakeningFloorActor::SetTextScale(const float Scale)
{
    Config.TextScale = FMath::Clamp(Scale, 1.0f, 2.0f);
    if (Hud->HasOverlay())
    {
        Hud->Ensure(Config.TextScale);
    }
    else
    {
        Hud->SetTextScale(Config.TextScale);
    }
    for (int32 S = 0; S < SituationCount; ++S)
    {
        RefreshStatusSign(S);
    }
}

bool APWAwakeningFloorActor::IsReducedMotion() const
{
    return AwIsReducedMotion(Config.ReducedMotion);
}

EAwPhase APWAwakeningFloorActor::GetPhase(const int32 Station) const
{
    return (Station >= 0 && Station < SituationCount) ? Phases[Station] : EAwPhase::Locked;
}

APWAwakeningMoverActor* APWAwakeningFloorActor::GetMover(const int32 Station, const bool bSecondary) const
{
    return (Station >= 0 && Station < SituationCount) ? Movers[Station][bSecondary ? 1 : 0].Get() : nullptr;
}

APWShopHoursSignActor* APWAwakeningFloorActor::GetStatusSign(const int32 Station) const
{
    return (Station >= 0 && Station < SituationCount) ? StatusSigns[Station].Get() : nullptr;
}

APWAwakeningPadActor* APWAwakeningFloorActor::FindPad(const FName PadId) const
{
    for (const TWeakObjectPtr<APWAwakeningPadActor>& Pad : Pads)
    {
        if (Pad.IsValid() && Pad->GetSpec().Id == PadId)
        {
            return Pad.Get();
        }
    }
    return nullptr;
}

int32 APWAwakeningFloorActor::GetFigureCount() const
{
    int32 Count = 0;
    for (const TWeakObjectPtr<ACharacter>& Figure : Figures)
    {
        Count += Figure.IsValid() ? 1 : 0;
    }
    return Count;
}

APawn* APWAwakeningFloorActor::FindPlayerPawn() const
{
    return UGameplayStatics::GetPlayerPawn(this, 0);
}

// ---- Text ----

void APWAwakeningFloorActor::Announce(const FText& Text, const float Seconds)
{
    LastCaption = Text;
    const UWorld* World = GetWorld();
    Hud->ShowCaption(Text, World != nullptr ? World->GetTimeSeconds() : 0.0, Seconds);
    if (Config.bUseHud)
    {
        Hud->Ensure(Config.TextScale);
    }
}

void APWAwakeningFloorActor::RefreshStatusSign(const int32 Station)
{
    APWShopHoursSignActor* Sign = GetStatusSign(Station);
    if (Sign == nullptr || !Layout.Stations.IsValidIndex(Station))
    {
        return;
    }
    const TCHAR* Text = TEXT("[ ] OPEN");
    switch (Phases[Station])
    {
    case EAwPhase::Locked:
        Text = TEXT("[-] NOT YET OPEN");
        break;
    case EAwPhase::Recovering:
        Text = TEXT("[~] RESET WITH HELP - TRY AGAIN");
        break;
    case EAwPhase::Complete:
        Text = TEXT("[#] DONE");
        break;
    case EAwPhase::Available:
    case EAwPhase::Active:
        break;
    }
    Sign->Configure(FName(TEXT("awakening.status")), Text, Layout.Stations[Station].StatusAt, -90.0, 6.0f * Config.TextScale);
}

void APWAwakeningFloorActor::ApplyStatus(const FAwFloorStatus& Status)
{
    for (int32 I = 0; I < SituationCount; ++I)
    {
        Phases[I] = Status.Phase[I];
        RefreshStatusSign(I);
    }
    bSessionComplete = Status.bSessionComplete;
}

FText APWAwakeningFloorActor::HazardCue(const int32 Station) const
{
    if (Station == 0)
    {
        return LOCTEXT("CueGap", "[!!!] GAP AHEAD - cross by the platform only. Stepping in just asks Ms Basu to reset you; nothing is lost.");
    }
    return LOCTEXT("CueWall", "[!!!] MOVING WALL - keep off the striped strip. The wall shifts only when announced.");
}

// ---- Transfers ----

void APWAwakeningFloorActor::Place(APawn* Pawn, const FVector& Feet, const double Yaw) const
{
    if (Pawn == nullptr)
    {
        return;
    }
    float HalfHeight = 82.5f;
    const ACharacter* Character = Cast<ACharacter>(Pawn);
    if (Character != nullptr && Character->GetCapsuleComponent() != nullptr)
    {
        HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    }
    Pawn->TeleportTo(Feet + FVector(0.0, 0.0, HalfHeight + 3.0), FRotator(0.0, Yaw, 0.0));
    if (AController* Controller = Pawn->GetController())
    {
        Controller->SetControlRotation(FRotator(0.0, Yaw, 0.0));
    }
    if (Character != nullptr && Character->GetCharacterMovement() != nullptr)
    {
        Character->GetCharacterMovement()->Velocity = FVector::ZeroVector;
    }
}

void APWAwakeningFloorActor::StartTransfer(const FText& Caption, TFunction<void()> Move)
{
    Plan = AwPlanTransition(IsReducedMotion());
    if (Config.bInstantTransitions)
    {
        Move();
        Announce(Caption);
        return;
    }
    MidAction = MoveTemp(Move);
    PendingCaption = Caption;
    bMoved = false;
    StageElapsed = 0.0f;
    if (Plan.bFade)
    {
        Stage = EStage::FadeOut;
        if (Config.bUseHud)
        {
            Hud->Ensure(Config.TextScale);
        }
    }
    else
    {
        // Reduced motion: announce first, hold still, then cut once. No alpha animation at all.
        Stage = EStage::Hold;
        Announce(Caption, Plan.HoldSeconds + 5.0f);
    }
}

void APWAwakeningFloorActor::FinishTransition()
{
    if (Stage == EStage::None)
    {
        return;
    }
    if (!bMoved && MidAction)
    {
        MidAction();
    }
    bMoved = true;
    MidAction = nullptr;
    Stage = EStage::None;
    Hud->SetFade(0.0f);
    Announce(PendingCaption);
}

void APWAwakeningFloorActor::TickTransfer(const float DeltaSeconds)
{
    if (Stage == EStage::None)
    {
        return;
    }
    StageElapsed += DeltaSeconds;
    const auto RunMove = [this]()
    {
        if (!bMoved && MidAction)
        {
            TFunction<void()> Action = MoveTemp(MidAction);
            MidAction = nullptr;
            bMoved = true;
            Action();
        }
        bMoved = true;
    };
    switch (Stage)
    {
    case EStage::FadeOut:
        Hud->SetFade(Plan.FadeOutSeconds > 0.0f ? FMath::Clamp(StageElapsed / Plan.FadeOutSeconds, 0.0f, 1.0f) : 1.0f);
        if (StageElapsed >= Plan.FadeOutSeconds)
        {
            RunMove();
            Hud->SetFade(1.0f);
            Stage = EStage::Hold;
            StageElapsed = 0.0f;
        }
        break;
    case EStage::Hold:
        if (StageElapsed >= Plan.HoldSeconds)
        {
            RunMove();
            StageElapsed = 0.0f;
            if (Plan.bFade)
            {
                Stage = EStage::FadeIn;
                Announce(PendingCaption);
            }
            else
            {
                Stage = EStage::None;
            }
        }
        break;
    case EStage::FadeIn:
        Hud->SetFade(Plan.FadeInSeconds > 0.0f ? 1.0f - FMath::Clamp(StageElapsed / Plan.FadeInSeconds, 0.0f, 1.0f) : 0.0f);
        if (StageElapsed >= Plan.FadeInSeconds)
        {
            Hud->SetFade(0.0f);
            Stage = EStage::None;
        }
        break;
    case EStage::None:
        break;
    }
}

void APWAwakeningFloorActor::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const UWorld* World = GetWorld();
    if (World != nullptr)
    {
        Hud->Update(World->GetTimeSeconds());
    }
    TickTransfer(DeltaSeconds);
    TickHazards(DeltaSeconds);
}

void APWAwakeningFloorActor::EndPlay(const EEndPlayReason::Type Reason)
{
    MidAction = nullptr;
    Hud->Remove();
    Super::EndPlay(Reason);
}

// ---- Hazards ----

bool APWAwakeningFloorActor::IsInHazard(const int32 Station, const FVector& Feet) const
{
    if (!Layout.Stations.IsValidIndex(Station))
    {
        return false;
    }
    const FAwStationSpec& S = Layout.Stations[Station];
    if (!S.Hazard.IsValid || !S.Hazard.IsInside(Feet))
    {
        return false;
    }
    if (S.SafeWhenAligned.IsValid && S.SafeWhenAligned.IsInside(Feet))
    {
        const APWAwakeningMoverActor* Platform = GetMover(Station, false);
        if (Platform != nullptr && Platform->GetPose() == 0 && !Platform->IsMoving())
        {
            return false;
        }
    }
    return true;
}

void APWAwakeningFloorActor::TickHazards(const float DeltaSeconds)
{
    PollTimer += DeltaSeconds;
    if (PollTimer < AwFlHazardPollSeconds)
    {
        return;
    }
    PollTimer = 0.0f;
    if (!bOccupied)
    {
        Hud->SetCue(FText::GetEmpty());
        return;
    }
    APawn* Pawn = IsTransitioning() ? nullptr : FindPlayerPawn();
    if (Pawn == nullptr)
    {
        return;
    }
    float HalfHeight = 82.5f;
    if (const ACharacter* Character = Cast<ACharacter>(Pawn))
    {
        HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    }
    const FVector Feet = Pawn->GetActorLocation() - FVector(0.0, 0.0, HalfHeight);
    FText Cue;
    for (int32 I = 0; I < Layout.Stations.Num(); ++I)
    {
        if (IsInHazard(I, Feet))
        {
            Recover(Pawn, I, FName(Ids::ReasonLeftTheRoute), true);
            return;
        }
        if (Layout.Stations[I].Hazard.IsValid && Layout.Stations[I].Hazard.ExpandBy(FVector(150.0, 150.0, 200.0)).IsInside(Feet))
        {
            Cue = HazardCue(I);
        }
    }
    Hud->SetCue(Cue);
}

#undef LOCTEXT_NAMESPACE
