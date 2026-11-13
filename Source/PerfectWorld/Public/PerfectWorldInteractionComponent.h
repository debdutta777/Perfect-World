#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PerfectWorldInteractionComponent.generated.h"

class APawn;
class SPWInteractionPrompt;
class SWeakWidget;
class SWidget;
class UWorld;

/**
 * The numbers behind interaction targeting, in centimetres and degrees (LANE_LIFE_PLAN section B, "Conversation UX").
 *
 * Reach is measured from the player's CAPSULE axis (the closest point on the line down the middle of the capsule) to the
 * target's interaction point, never from a camera, so a third-person boom of any length cannot extend it. The view cone
 * is measured on the ground plane from the capsule's eye point, along the controller's facing (which both cameras share
 * and the body follows); looking up or down does not change it.
 */
namespace PWInteraction
{
constexpr double ReachCm = 200.0;               // 2 m
constexpr double ConeHalfAngleDegrees = 50.0;   // forgiving: 100 degrees wide
constexpr double NearZoneCm = 100.0;            // inside this a target counts regardless of where the player faces
constexpr double ReachHysteresisCm = 30.0;      // the current target survives this much beyond the reach
constexpr double ConeHysteresisDegrees = 15.0;  // ... and this much beyond the cone
constexpr double SwitchMargin = 0.2;            // a same-priority rival must be this much better (score 0 is best, 1 is the edge)
constexpr double SightSlackCm = 15.0;           // a hit this close to the target point is the target's own frame, not an occluder
constexpr int32 OccludedEvaluationsToDrop = 3;  // a person walking across the line of sight must stay there 0.3 s to drop the target
constexpr float EvaluationIntervalSeconds = 0.1f;
constexpr float CandidateRefreshSeconds = 0.5f; // how often the list of interactable actors in the world is rebuilt
} // namespace PWInteraction

/**
 * Finds what the player can act on and acts on it. It lives on the walking player and does three things:
 *
 *  1. Targeting, evaluated at 10 Hz. Candidates are actors that implement IPWInteractable. A candidate qualifies when it is
 *     within PWInteraction::ReachCm of the capsule axis, inside the view cone (or within the near zone), has a non-empty
 *     verb, and a visibility trace from the eye reaches it. The winner has the highest priority, then the best score
 *     (nearer and more centred). The current target is sticky: it keeps the prompt while it stays inside a slightly larger
 *     reach and cone, and a rival of the same priority must be clearly better to replace it.
 *  2. Cycling. CycleTarget picks the next qualifying candidate and pins it until it stops qualifying (the larger bounds).
 *  3. The prompt ("E / X - verb") drawn with Slate in the lower centre, only while there is a target and no menu or
 *     conversation is open.
 *
 * Interact and cycle are ignored while a menu or a conversation is open (IsWorldInputBlocked).
 */
UCLASS()
class PERFECTWORLD_API UPWInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPWInteractionComponent();

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** Acts on the current target, if it still qualifies. Returns true when Interact was called. */
    bool TryInteract();

    /** Moves the target to the next qualifying candidate, in priority-then-score order, and pins it. */
    void CycleTarget();

    /**
     * Shows a short message just above the prompt for a few seconds ("It's locked."), in the same toast style. A newer
     * notice replaces the older. It is hidden while a menu or a conversation is open.
     */
    void ShowNotice(const FText& Message, float Seconds = 2.5f);
    bool IsNoticeVisible() const;
    FText GetNotice() const { return Notice; }

    /** True while a front-end menu or the conversation panel owns the input; play input (movement, interaction, camera) is ignored. */
    static bool IsWorldInputBlocked(const APawn* Pawn);

    AActor* GetTarget() const { return Target.Get(); }
    bool IsPromptVisible() const;
    FText GetPromptVerb() const { return PromptVerb; }
    FText GetCycleHint() const;

private:
    void Evaluate();
    void RefreshActorList();
    void SetTarget(AActor* NewTarget, const FText& Verb, bool bPinnedByPlayer);
    void EnsurePrompt();
    void EnsureNotice();
    void RemovePrompt();

    /** Every actor implementing IPWInteractable, rebuilt twice a second so spawned people and doors appear. */
    TArray<TWeakObjectPtr<AActor>> Interactables;
    TWeakObjectPtr<AActor> Target;
    FText PromptVerb;
    int32 CandidateCount = 0;
    int32 OccludedCount = 0;
    bool bPinned = false;
    float SecondsToActorRefresh = 0.0f;

    FText Notice;
    double NoticeUntilSeconds = 0.0;

    TSharedPtr<SPWInteractionPrompt> Prompt;
    TSharedPtr<SWeakWidget> PromptHolder;
    TSharedPtr<SWidget> NoticeContent; // SWeakWidget holds its content weakly, so the component keeps it alive
    TSharedPtr<SWeakWidget> NoticeHolder;
};
