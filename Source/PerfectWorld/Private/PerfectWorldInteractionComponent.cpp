#include "PerfectWorldInteractionComponent.h"

#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldInteractable.h"
#include "UI/SPWInteractionPrompt.h"
#include "Widgets/SWeakWidget.h"

#define LOCTEXT_NAMESPACE "PerfectWorldInteractionComponent"

namespace
{
/** Below the front end (100) and the conversation panel (110): the prompt is hidden whenever either is up. */
constexpr int32 PromptZOrder = 40;
constexpr double LooseNearZoneBonusCm = 20.0;

/** What the player's body gives the targeting rules, computed once per evaluation. */
struct FReachFrame
{
    FVector Eye = FVector::ZeroVector;
    FVector AxisBottom = FVector::ZeroVector;
    FVector AxisTop = FVector::ZeroVector;
    FVector2D Forward2D = FVector2D(1.0, 0.0);
};

FReachFrame MakeFrame(const APawn& Pawn)
{
    FReachFrame Frame;
    Frame.Eye = Pawn.GetPawnViewLocation(); // the capsule's eye point, not any camera
    Frame.AxisBottom = Pawn.GetActorLocation();
    Frame.AxisTop = Pawn.GetActorLocation();
    if (const ACharacter* Character = Cast<ACharacter>(&Pawn))
    {
        if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
        {
            const double Half = FMath::Max(0.0, static_cast<double>(Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleRadius()));
            Frame.AxisBottom = Capsule->GetComponentLocation() - FVector(0.0, 0.0, Half);
            Frame.AxisTop = Capsule->GetComponentLocation() + FVector(0.0, 0.0, Half);
        }
    }
    const double YawRadians = FMath::DegreesToRadians(static_cast<double>(Pawn.GetControlRotation().Yaw));
    Frame.Forward2D = FVector2D(FMath::Cos(YawRadians), FMath::Sin(YawRadians));
    return Frame;
}

struct FMeasure
{
    double ReachCm = 0.0;      // from the capsule axis
    double GroundCm = 0.0;     // from the eye, on the ground plane
    double AngleDegrees = 0.0; // from the facing, on the ground plane
};

FMeasure Measure(const FReachFrame& Frame, const FVector& Point)
{
    FMeasure Result;
    Result.ReachCm = FVector::Dist(FMath::ClosestPointOnSegment(Point, Frame.AxisBottom, Frame.AxisTop), Point);
    const FVector2D ToPoint(Point.X - Frame.Eye.X, Point.Y - Frame.Eye.Y);
    Result.GroundCm = ToPoint.Size();
    if (Result.GroundCm > 1.0)
    {
        const double Cosine = FMath::Clamp(FVector2D::DotProduct(Frame.Forward2D, ToPoint / Result.GroundCm), -1.0, 1.0);
        Result.AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(Cosine));
    }
    return Result;
}

/** Strict bounds admit a new target; loose bounds (a little larger) let the current target stay. */
bool InBounds(const FMeasure& Measured, const bool bLoose)
{
    const double Reach = PWInteraction::ReachCm + (bLoose ? PWInteraction::ReachHysteresisCm : 0.0);
    const double Cone = PWInteraction::ConeHalfAngleDegrees + (bLoose ? PWInteraction::ConeHysteresisDegrees : 0.0);
    const double Near = PWInteraction::NearZoneCm + (bLoose ? LooseNearZoneBonusCm : 0.0);
    return Measured.ReachCm <= Reach && (Measured.GroundCm <= Near || Measured.AngleDegrees <= Cone);
}

/** 0 is dead centre and touching; about 1 is the edge of the reach and cone. Lower is better. */
double Score(const FMeasure& Measured)
{
    const double Angle = FMath::Min(Measured.AngleDegrees, PWInteraction::ConeHalfAngleDegrees);
    return 0.5 * (Measured.ReachCm / PWInteraction::ReachCm) + 0.5 * (Angle / PWInteraction::ConeHalfAngleDegrees);
}

bool IsOccluded(const UWorld& World, const APawn& Pawn, const AActor& TargetActor, const FVector& Eye, const FVector& Point)
{
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PWInteractionSight), false, &Pawn);
    Params.AddIgnoredActor(&TargetActor); // its own collision (a door leaf, a person's capsule) is not in the way
    FHitResult Hit;
    if (!World.LineTraceSingleByChannel(Hit, Eye, Point, ECC_Visibility, Params))
    {
        return false;
    }
    return static_cast<double>(Hit.Distance) < FVector::Dist(Eye, Point) - PWInteraction::SightSlackCm;
}

struct FCandidate
{
    AActor* Actor = nullptr;
    int32 Priority = 0;
    double Score = 0.0;
    FText Verb;
};
} // namespace

UPWInteractionComponent::UPWInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = PWInteraction::EvaluationIntervalSeconds;
}

bool UPWInteractionComponent::IsWorldInputBlocked(const APawn* Pawn)
{
    if (Pawn == nullptr)
    {
        return true;
    }
    if (const UPWConversationPresenter* Presenter = UPWConversationPresenter::Get(Pawn))
    {
        if (Presenter->IsOpen())
        {
            return true;
        }
    }
    const APlayerController* PlayerController = Cast<APlayerController>(Pawn->GetController());
    const APerfectWorldFrontEndHUD* Hud = PlayerController != nullptr ? Cast<APerfectWorldFrontEndHUD>(PlayerController->GetHUD()) : nullptr;
    return Hud != nullptr && Hud->IsMenuOpen();
}

bool UPWInteractionComponent::IsPromptVisible() const
{
    return Target.IsValid() && !PromptVerb.IsEmpty() && !IsWorldInputBlocked(GetOwner<APawn>());
}

FText UPWInteractionComponent::GetCycleHint() const
{
    return CandidateCount > 1 ? LOCTEXT("CycleHint", "Tab / RB for the next") : FText::GetEmpty();
}

void UPWInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RemovePrompt();
    Super::EndPlay(EndPlayReason);
}

void UPWInteractionComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    EnsurePrompt();

    SecondsToActorRefresh -= DeltaTime;
    if (SecondsToActorRefresh <= 0.0f)
    {
        RefreshActorList();
        SecondsToActorRefresh = PWInteraction::CandidateRefreshSeconds;
    }
    Evaluate();
}

void UPWInteractionComponent::RefreshActorList()
{
    Interactables.Reset();
    TArray<AActor*> Found;
    UGameplayStatics::GetAllActorsWithInterface(this, UPWInteractable::StaticClass(), Found);
    for (AActor* Actor : Found)
    {
        Interactables.Add(Actor);
    }
}

void UPWInteractionComponent::SetTarget(AActor* NewTarget, const FText& Verb, const bool bPinnedByPlayer)
{
    if (Target.Get() != NewTarget)
    {
        OccludedCount = 0;
    }
    Target = NewTarget;
    PromptVerb = NewTarget != nullptr ? Verb : FText::GetEmpty();
    bPinned = NewTarget != nullptr && bPinnedByPlayer;
}

namespace
{
/** Every interactable that qualifies under the strict bounds, best first: higher priority, then lower score. */
void CollectCandidates(const UWorld& World, const APawn& Pawn, const FReachFrame& Frame,
                       const TArray<TWeakObjectPtr<AActor>>& Interactables, TArray<FCandidate>& Out)
{
    for (const TWeakObjectPtr<AActor>& Weak : Interactables)
    {
        AActor* Actor = Weak.Get();
        IPWInteractable* Interactable = Actor != nullptr && Actor != &Pawn ? Cast<IPWInteractable>(Actor) : nullptr;
        if (Interactable == nullptr)
        {
            continue;
        }
        const FText Verb = Interactable->GetInteractionVerb(&Pawn);
        if (Verb.IsEmpty())
        {
            continue;
        }
        const FVector Point = Interactable->GetInteractionPoint();
        const FMeasure Measured = Measure(Frame, Point);
        if (!InBounds(Measured, false) || IsOccluded(World, Pawn, *Actor, Frame.Eye, Point))
        {
            continue;
        }
        FCandidate Candidate;
        Candidate.Actor = Actor;
        Candidate.Priority = Interactable->GetInteractionPriority();
        Candidate.Score = Score(Measured);
        Candidate.Verb = Verb;
        Out.Add(Candidate);
    }
    Out.Sort([](const FCandidate& A, const FCandidate& B)
    {
        return A.Priority != B.Priority ? A.Priority > B.Priority : A.Score < B.Score;
    });
}
} // namespace

void UPWInteractionComponent::Evaluate()
{
    const APawn* Pawn = GetOwner<APawn>();
    const UWorld* World = GetWorld();
    if (Pawn == nullptr || World == nullptr || IsWorldInputBlocked(Pawn))
    {
        SetTarget(nullptr, FText::GetEmpty(), false);
        CandidateCount = 0;
        return;
    }

    const FReachFrame Frame = MakeFrame(*Pawn);
    TArray<FCandidate> Candidates;
    CollectCandidates(*World, *Pawn, Frame, Interactables, Candidates);
    CandidateCount = Candidates.Num();

    // Does the current target still hold? It is judged by the larger bounds, and a brief occlusion is forgiven.
    AActor* Current = Target.Get();
    bool bKeepCurrent = false;
    int32 CurrentPriority = 0;
    double CurrentScore = 0.0;
    FText CurrentVerb;
    if (Current != nullptr)
    {
        if (IPWInteractable* Interactable = Cast<IPWInteractable>(Current))
        {
            CurrentVerb = Interactable->GetInteractionVerb(Pawn);
            const FVector Point = Interactable->GetInteractionPoint();
            const FMeasure Measured = Measure(Frame, Point);
            if (!CurrentVerb.IsEmpty() && InBounds(Measured, true))
            {
                OccludedCount = IsOccluded(*World, *Pawn, *Current, Frame.Eye, Point) ? OccludedCount + 1 : 0;
                bKeepCurrent = OccludedCount < PWInteraction::OccludedEvaluationsToDrop;
                CurrentPriority = Interactable->GetInteractionPriority();
                CurrentScore = Score(Measured);
            }
        }
    }

    if (!bKeepCurrent)
    {
        if (Candidates.IsEmpty())
        {
            SetTarget(nullptr, FText::GetEmpty(), false);
        }
        else
        {
            SetTarget(Candidates[0].Actor, Candidates[0].Verb, false);
        }
        return;
    }

    // Keep it, unless the player did not choose it and something clearly better has appeared.
    if (!bPinned && !Candidates.IsEmpty() && Candidates[0].Actor != Current)
    {
        const FCandidate& Best = Candidates[0];
        const bool bHigherPriority = Best.Priority > CurrentPriority;
        const bool bClearlyBetter = Best.Priority == CurrentPriority && Best.Score + PWInteraction::SwitchMargin < CurrentScore;
        if (bHigherPriority || bClearlyBetter)
        {
            SetTarget(Best.Actor, Best.Verb, false);
            return;
        }
    }
    PromptVerb = CurrentVerb; // a verb can change under the same target ("Open" becomes "Close")
}

bool UPWInteractionComponent::TryInteract()
{
    APawn* Pawn = GetOwner<APawn>();
    AActor* Current = Target.Get();
    if (Pawn == nullptr || Current == nullptr || IsWorldInputBlocked(Pawn))
    {
        return false;
    }
    IPWInteractable* Interactable = Cast<IPWInteractable>(Current);
    if (Interactable == nullptr || Interactable->GetInteractionVerb(Pawn).IsEmpty())
    {
        return false;
    }
    // The target was chosen up to 0.1 s ago; make sure it is still within reach before acting on it.
    if (!InBounds(Measure(MakeFrame(*Pawn), Interactable->GetInteractionPoint()), true))
    {
        return false;
    }
    Interactable->Interact(Pawn);
    if (Target.IsValid())
    {
        PromptVerb = Interactable->GetInteractionVerb(Pawn);
    }
    return true;
}

void UPWInteractionComponent::CycleTarget()
{
    const APawn* Pawn = GetOwner<APawn>();
    const UWorld* World = GetWorld();
    if (Pawn == nullptr || World == nullptr || IsWorldInputBlocked(Pawn))
    {
        return;
    }
    TArray<FCandidate> Candidates;
    CollectCandidates(*World, *Pawn, MakeFrame(*Pawn), Interactables, Candidates);
    CandidateCount = Candidates.Num();
    if (Candidates.Num() < 2)
    {
        return; // nothing to cycle to
    }
    const AActor* Current = Target.Get();
    int32 CurrentIndex = INDEX_NONE;
    for (int32 Index = 0; Index < Candidates.Num(); ++Index)
    {
        if (Candidates[Index].Actor == Current)
        {
            CurrentIndex = Index;
            break;
        }
    }
    const FCandidate& Next = Candidates[(CurrentIndex + 1) % Candidates.Num()];
    SetTarget(Next.Actor, Next.Verb, true);
}

void UPWInteractionComponent::EnsurePrompt()
{
    if (PromptHolder.IsValid() || GEngine == nullptr || GEngine->GameViewport == nullptr)
    {
        return;
    }
    const APawn* Pawn = GetOwner<APawn>();
    if (Pawn == nullptr || !Pawn->IsLocallyControlled())
    {
        return;
    }
    // The widget reads this component through a weak pointer only; it never keeps the component or a widget alive.
    const TWeakObjectPtr<UPWInteractionComponent> Weak(this);
    Prompt = SNew(SPWInteractionPrompt)
        .ShowPrompt(TAttribute<bool>::CreateLambda([Weak]() { return Weak.IsValid() && Weak->IsPromptVisible(); }))
        .Verb(TAttribute<FText>::CreateLambda([Weak]() { return Weak.IsValid() ? Weak->GetPromptVerb() : FText::GetEmpty(); }))
        .CycleHint(TAttribute<FText>::CreateLambda([Weak]() { return Weak.IsValid() ? Weak->GetCycleHint() : FText::GetEmpty(); }));
    PromptHolder = SNew(SWeakWidget).PossiblyNullContent(Prompt);
    GEngine->GameViewport->AddViewportWidgetContent(PromptHolder.ToSharedRef(), PromptZOrder);
}

void UPWInteractionComponent::RemovePrompt()
{
    if (PromptHolder.IsValid() && GEngine != nullptr && GEngine->GameViewport != nullptr)
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(PromptHolder.ToSharedRef());
    }
    PromptHolder.Reset();
    Prompt.Reset();
}

#undef LOCTEXT_NAMESPACE
