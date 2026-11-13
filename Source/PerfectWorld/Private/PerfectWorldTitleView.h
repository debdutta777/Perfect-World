#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "PerfectWorldTitleView.generated.h"

class APawn;
class APlayerController;
class UWorld;

/**
 * Where the title camera stands and what it sees. Pure values, so the framing can be tested and tuned without a world.
 *
 * The frame (units are centimetres, X along the lane): a low, wide view from the road a little behind the opening point,
 * turned about 12 degrees towards the +Y row so that the Shanti Chai shopfront (X 600 to 1300, with its awning and warm
 * windows) fills the right third of the screen, where the dark menu surface has faded away, and the row of facades runs
 * back along the lane behind the menu. The street itself, the kerb and the receding rooflines give the depth.
 */
namespace PWTitleView
{
struct FTvPose
{
    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    float FovDegrees = 56.0f;
};

/** The composed frame at rest. Reduced motion shows exactly this and nothing else. */
FTvPose RestPose();

/** The rest frame moved by the slow drift after Seconds: a few degrees of yaw and a shallow truck along the lane. */
FTvPose DriftPose(float Seconds);

/** The largest distance and yaw the drift can ever take the camera from the rest frame. */
double MaxDriftDistance();
float MaxDriftYaw();
} // namespace PWTitleView

/**
 * The title screen's own viewpoint over the morning lane. It is a camera that is not the player's pawn: presenting it
 * makes it the view target of the local controller, and dismissing it gives the view back to the pawn. It never moves,
 * turns, possesses or saves the pawn, and it never touches the gameplay clock (the front end's Title mode already holds
 * the lane clock; see UPWLaneLifeSubsystem::UpdateClockHold). While it is presented the pawn is hidden, because the
 * title frame can contain the spot where the pawn stands; the pawn's own hidden flag is restored exactly on dismissal.
 */
UCLASS(NotBlueprintable, NotPlaceable, Transient)
class APWTitleView : public ACameraActor
{
    GENERATED_BODY()

public:
    APWTitleView();

    /** The world's title view, or null when none has been spawned. */
    static APWTitleView* Find(const UWorld* World);

    /** Spawns the view when needed and makes it the controller's view target at once. Returns it, or null on failure. */
    static APWTitleView* Present(APlayerController& Controller);

    /** Returns the controller's view to its pawn and the pawn to how it was. Safe to call when nothing is presented. */
    static void Dismiss(APlayerController& Controller);

    /** True when the controller currently looks through this view. */
    bool IsViewedBy(const APlayerController& Controller) const;

    /** Reduced motion holds the rest frame and stops ticking at once; otherwise the drift runs while presented. */
    void ApplyMotionPreference(bool bReducedMotion);

    /** Starts or stops the drift (the view target change itself is Present and Dismiss). */
    void SetPresenting(bool bValue);
    bool IsPresenting() const { return bPresenting; }

    /** Hides the pawn behind the title, remembering its previous state. */
    void HoldPawn(APawn* Pawn);
    /** Restores the held pawn's hidden flag. Never moves it. */
    void ReleasePawn();

    float GetDriftSeconds() const { return DriftSeconds; }

    virtual void Tick(float DeltaSeconds) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void ApplyPose(const PWTitleView::FTvPose& Pose);
    void HandleMotionChanged();

    float DriftSeconds = 0.0f;
    bool bPresenting = false;
    bool bHeldPawnWasHidden = false;
    TWeakObjectPtr<APawn> HeldPawn;
    FDelegateHandle MotionHandle;
};
