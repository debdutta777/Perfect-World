#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PerfectWorldAwakeningBridge.h"
#include "PerfectWorldAwakeningHud.h"
#include "PerfectWorldAwakeningLayout.h"
#include "PerfectWorldInteractable.h"

#include "PerfectWorldAwakeningActors.generated.h"

class ACharacter;
class APWShopHoursSignActor;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;

class APWAwakeningFloorActor;

/**
 * R9-06: one thing to act on at the tutorial floor (a choice pad, the withdrawal, advance, reset, the campus terminal, the return
 * pad). It is a person-free interaction point like the school register: no mesh of its own (the slab and its raised shape are
 * floor blocks). Verb and effect come from APWAwakeningFloorActor::GetPadVerb / HandlePad; the pad decides nothing.
 */
UCLASS()
class PERFECTWORLD_API APWAwakeningPadActor : public AActor, public IPWInteractable
{
    GENERATED_BODY()

public:
    APWAwakeningPadActor();

    void Configure(APWAwakeningFloorActor* InFloor, const PWAwakening::FAwPadSpec& InSpec);
    const PWAwakening::FAwPadSpec& GetSpec() const { return Spec; }

    // IPWInteractable
    virtual FText GetInteractionVerb(const APawn* By) const override;
    virtual FVector GetInteractionPoint() const override { return Spec.Point; }
    virtual int32 GetInteractionPriority() const override;
    virtual void Interact(APawn* By) override;

private:
    UPROPERTY()
    TObjectPtr<USceneComponent> Root;

    TWeakObjectPtr<APWAwakeningFloorActor> Floor;
    PWAwakening::FAwPadSpec Spec;
};

/**
 * A block that moves between authored poses: the stalled platform, the shifting wall, a barrier, crates, a marker. Smooth glide
 * normally; under reduced motion it never glides, never cycles on its own and never flickers: a requested step is announced by the
 * floor's caption, waits AwReducedStepAnnounceSeconds and then jumps once. Collision (when solid) blocks the pawn, camera and visibility.
 */
UCLASS()
class PERFECTWORLD_API APWAwakeningMoverActor : public AActor
{
    GENERATED_BODY()

public:
    APWAwakeningMoverActor();

    void Configure(const PWAwakening::FAwMoverSpec& InSpec, UStaticMesh* Cube, UMaterialInterface* BaseMaterial);
    void SetReducedMotionOverride(const TOptional<bool>& Override) { ReducedOverride = Override; }

    virtual void Tick(float DeltaSeconds) override;
    /** The whole of Tick, callable from tests. */
    void AdvanceTime(float DeltaSeconds);

    /** Requests a pose: glide (normal) or announce-wait-jump (reduced). */
    void GoTo(int32 Pose);
    /** Immediately, whatever the motion setting (resets and tests). */
    void SnapTo(int32 Pose);
    /** The other of poses 0 and 1. */
    void StepNext();

    void SetHold(bool bHold);
    bool IsHeld() const { return bHeld; }
    void SetFlickering(bool bOn);
    bool IsFlickering() const { return bFlickering; }

    /** The pose last reached. */
    int32 GetPose() const { return Pose; }
    /** The pose being moved to (equals GetPose when at rest). */
    int32 GetTargetPose() const { return TargetPose; }
    bool IsMoving() const { return bGliding || PendingPose != INDEX_NONE; }
    const PWAwakening::FAwMoverSpec& GetSpec() const { return Spec; }
    FVector GetCentre() const { return GetActorLocation(); }
    PWAwakening::EAwColour GetShownColour() const { return ShownColour; }
    bool IsReduced() const;

private:
    void ShowColour(PWAwakening::EAwColour Colour);

    UPROPERTY()
    TObjectPtr<USceneComponent> Root;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Block;
    UPROPERTY()
    TObjectPtr<UStaticMesh> CubeMesh;
    UPROPERTY()
    TObjectPtr<UMaterialInterface> BaseMaterial;
    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> Paint;

    PWAwakening::FAwMoverSpec Spec;
    TOptional<bool> ReducedOverride;
    int32 Pose = 0;
    int32 TargetPose = 0;
    int32 PendingPose = INDEX_NONE;
    float PendingDelay = 0.0f;
    bool bGliding = false;
    float GlideT = 0.0f;
    FVector GlideFrom = FVector::ZeroVector;
    bool bHeld = false;
    bool bFlickering = false;
    float CycleTimer = 0.0f;
    float FlickerTimer = 0.0f;
    bool bFlickerOff = false;
    PWAwakening::EAwColour ShownColour = PWAwakening::EAwColour::GateTeal;
};

/**
 * The director of the tutorial floor (prefix Aw / PWAwakening). It owns the floor's presentation state: whether the player is on the
 * floor, the fade/announce transfers, the subtitles, the hazard cue, the people, the movers and the status boards. It FORWARDS every
 * choice to the installed IAwFloorBridge and shows the answer; it never decides what a choice means, never scores, and never sends
 * time or position to the bridge (the only positional event is a gentle reset, reported as informational assistance).
 */
UCLASS()
class PERFECTWORLD_API APWAwakeningFloorActor : public AActor
{
    GENERATED_BODY()

public:
    APWAwakeningFloorActor();

    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    // ---- Set-up (SpawnAwakeningFloor) ----
    void Initialise(const PWAwakening::FAwFloorLayout& InLayout, const PWAwakening::FAwFloorConfig& InConfig);
    void RegisterPad(APWAwakeningPadActor* Pad);
    void RegisterMover(int32 Station, bool bSecondary, APWAwakeningMoverActor* Mover);
    void RegisterStatusSign(int32 Station, APWShopHoursSignActor* Sign);

    const PWAwakening::FAwFloorLayout& GetLayout() const { return Layout; }
    const PWAwakening::FAwFloorConfig& GetConfig() const { return Config; }
    void SetReducedMotionOverride(const TOptional<bool>& Override);
    void SetTextScale(float Scale);
    void SetInstantTransitions(bool bInstant) { Config.bInstantTransitions = bInstant; }

    // ---- State (tests and acceptance read these) ----
    bool IsOccupied() const { return bOccupied; }
    bool IsTransitioning() const { return Stage != EStage::None; }
    bool IsSessionComplete() const { return bSessionComplete; }
    PWAwakening::EAwPhase GetPhase(int32 Station) const;
    FText GetLastCaption() const { return LastCaption; }
    FString GetLastCode() const { return LastCode; }
    PWAwakening::FAwHud& GetHud() { return *Hud; }
    float GetFadeAlpha() const { return Hud->GetFade(); }
    bool IsReducedMotion() const;
    APWAwakeningMoverActor* GetMover(int32 Station, bool bSecondary) const;
    APWShopHoursSignActor* GetStatusSign(int32 Station) const;
    const TArray<TWeakObjectPtr<APWAwakeningPadActor>>& GetPads() const { return Pads; }
    APWAwakeningPadActor* FindPad(FName PadId) const;
    int32 GetFigureCount() const;

    // ---- Interaction ----
    FText GetPadVerb(const PWAwakening::FAwPadSpec& Pad) const;
    int32 GetPadPriority(const PWAwakening::FAwPadSpec& Pad) const;
    void HandlePad(const PWAwakening::FAwPadSpec& Pad, APawn* By);

    /** Asks the bridge, then steps onto the floor (fade, or announced cut under reduced motion). False when refused or busy. */
    bool TryEnter(APawn* By);
    /** Steps back onto the campus. False when not on the floor or busy. */
    bool TryLeave(APawn* By);
    /** Forwards (situation of Station, Choice) to the bridge and shows the answer. The withdrawal also moves the player to the safe area. */
    PWAwakening::FAwChoiceResult SubmitChoice(APawn* By, int32 Station, FName Choice);
    /** A gentle reset: back to the station's reset pad. Informs the bridge only when bTellBridge. */
    void Recover(APawn* By, int32 Station, FName Reason, bool bTellBridge);
    void AdvanceStation(int32 Station);
    void ResetStation(int32 Station, APawn* By);

    void SpawnFigures();
    void DespawnFigures();

    /** True when Feet is on a station's marked hazard and not on its aligned platform. */
    bool IsInHazard(int32 Station, const FVector& Feet) const;
    /** Finishes any transfer in progress at once (tests, acceptance). */
    void FinishTransition();

private:
    enum class EStage : uint8
    {
        None,
        FadeOut,
        Hold,
        FadeIn
    };

    void Announce(const FText& Text, float Seconds = 6.0f);
    void StartTransfer(const FText& Caption, TFunction<void()> Move);
    void TickTransfer(float DeltaSeconds);
    void TickHazards(float DeltaSeconds);
    void Place(APawn* Pawn, const FVector& Feet, double Yaw) const;
    void ApplyStatus(const PWAwakening::FAwFloorStatus& Status);
    void RefreshStatusSign(int32 Station);
    void ApplyEffect(int32 Station, const PWAwakening::FAwEffect& Effect);
    void ResetMovers(int32 Station);
    void RecoverMove(APawn* By, int32 Station, const FText& Caption);
    void DoEnterMove(APawn* By);
    void DoLeaveMove(APawn* By);
    FText HazardCue(int32 Station) const;
    APawn* FindPlayerPawn() const;

    UPROPERTY()
    TObjectPtr<USceneComponent> Root;

    PWAwakening::FAwFloorLayout Layout;
    PWAwakening::FAwFloorConfig Config;
    TSharedPtr<PWAwakening::FAwHud> Hud;

    TArray<TWeakObjectPtr<APWAwakeningPadActor>> Pads;
    TWeakObjectPtr<APWAwakeningMoverActor> Movers[PWAwakening::SituationCount][2];
    TWeakObjectPtr<APWShopHoursSignActor> StatusSigns[PWAwakening::SituationCount];
    TArray<TWeakObjectPtr<ACharacter>> Figures;

    bool bOccupied = false;
    bool bSessionComplete = false;
    PWAwakening::EAwPhase Phases[PWAwakening::SituationCount] = {PWAwakening::EAwPhase::Available, PWAwakening::EAwPhase::Available,
                                                                 PWAwakening::EAwPhase::Available};
    FText LastCaption;
    FString LastCode;

    EStage Stage = EStage::None;
    float StageElapsed = 0.0f;
    PWAwakening::FAwTransitionPlan Plan;
    TFunction<void()> MidAction;
    FText PendingCaption;
    bool bMoved = false;

    float PollTimer = 0.0f;
};
