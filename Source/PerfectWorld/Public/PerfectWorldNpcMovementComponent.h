#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PerfectWorldNpcMovementComponent.generated.h"

namespace PerfectWorld
{
struct NpcIntent;
}

/** The six states of a person's small AI (core NpcState, NpcBehaviour.h). Presentation only: the portable core decides. */
UENUM()
enum class EPWNpcState : uint8
{
    Work,
    Travel,
    Idle,
    Chat,
    Sleep,
    Yield
};

/**
 * What the portable core says a person is doing, in Unreal types (a plain copy of PerfectWorld::NpcIntent). The integrator
 * builds it with MakeNpcIntentView and hands it to SetIntent whenever the core resolves a new one (a minute passes, a
 * conversation opens or closes). The component never reads a schedule, a clock or a save.
 */
USTRUCT()
struct PERFECTWORLD_API FPWNpcIntentView
{
    GENERATED_BODY()

    UPROPERTY() EPWNpcState State = EPWNpcState::Idle;
    /** Where the person works, rests or sleeps; for Travel the destination zone. Looked up by the anchor resolver. */
    UPROPERTY() FName AnchorId;
    /** Travel (and a Chat or Yield that suspended one): the leg. Looked up by the route resolver. */
    UPROPERTY() FName RouteId;
    /** 0..1000. For Travel, how far along the route the person is according to the core. */
    UPROPERTY() int32 ProgressPermille = 0;
    /** The "work." id the gesture is chosen by; empty unless the person is working. Never awards anything. */
    UPROPERTY() FName WorkActionId;
    /** False means Interact gives the polite decline instead of opening a conversation (an Asleep person). */
    UPROPERTY() bool bTalkable = true;
    /** Stable "npc.reason." key; non-empty when talking would only get the polite decline. Never shown as text itself. */
    UPROPERTY() FName DeclineReasonKey;
    /** For Chat and Yield: the intent that is suspended and resumes. */
    UPROPERTY() EPWNpcState ResumeState = EPWNpcState::Idle;
    UPROPERTY() FName ResumeWorkActionId;
    /** Idle only: sit rather than stand (the integrator sets it when the anchor is a seat). */
    UPROPERTY() bool bPreferSit = false;
};

/** Converts the core's intent to the Unreal view. Pure; no state. */
PERFECTWORLD_API FPWNpcIntentView MakeNpcIntentView(const PerfectWorld::NpcIntent& Intent, bool bPreferSit = false);

/** Resolves an anchor id to where the person's feet stand and which way they face (yaw). False means unknown. */
using FPWNpcAnchorResolver = TFunction<bool(FName AnchorId, FTransform& OutFeetTransform)>;
/** Resolves a route id to a world-space polyline of feet points (floor level, like anchors). False means unknown. */
using FPWNpcRouteResolver = TFunction<bool(FName RouteId, TArray<FVector>& OutPoints)>;

/**
 * The pose of the block body, as small angles. Positive pitch swings a hanging limb forward (+X, the way the face looks).
 * Left is the -Y side. It is applied to the cubes by APWNpcCharacter; nothing here reads or affects gameplay.
 */
struct PERFECTWORLD_API FPWNpcPose
{
    float ArmPitch[2] = {0.0f, 0.0f}; // [0] left, [1] right
    float LegPitch[2] = {0.0f, 0.0f};
    float BobCm = 0.0f;
    float LeanPitch = 0.0f;  // upper body forward (+) from the hips
    float SwayRoll = 0.0f;   // whole body, from the feet
    float Sit = 0.0f;        // 0..1
    float Lie = 0.0f;        // 0..1, on the back, head towards -X

    /** Moves each field toward Target; Alpha for the limbs and lean, PostureAlpha for sit and lie. */
    void BlendToward(const FPWNpcPose& Target, const float Alpha, const float PostureAlpha)
    {
        for (int32 Side = 0; Side < 2; ++Side)
        {
            ArmPitch[Side] = FMath::Lerp(ArmPitch[Side], Target.ArmPitch[Side], Alpha);
            LegPitch[Side] = FMath::Lerp(LegPitch[Side], Target.LegPitch[Side], Alpha);
        }
        BobCm = FMath::Lerp(BobCm, Target.BobCm, Alpha);
        LeanPitch = FMath::Lerp(LeanPitch, Target.LeanPitch, Alpha);
        SwayRoll = FMath::Lerp(SwayRoll, Target.SwayRoll, Alpha);
        Sit = FMath::Lerp(Sit, Target.Sit, PostureAlpha);
        Lie = FMath::Lerp(Lie, Target.Lie, PostureAlpha);
    }

    bool NearlyEquals(const FPWNpcPose& Other, const float Tolerance = 0.05f) const
    {
        for (int32 Side = 0; Side < 2; ++Side)
        {
            if (FMath::Abs(ArmPitch[Side] - Other.ArmPitch[Side]) > Tolerance || FMath::Abs(LegPitch[Side] - Other.LegPitch[Side]) > Tolerance)
            {
                return false;
            }
        }
        return FMath::Abs(BobCm - Other.BobCm) <= Tolerance && FMath::Abs(LeanPitch - Other.LeanPitch) <= Tolerance
            && FMath::Abs(SwayRoll - Other.SwayRoll) <= Tolerance && FMath::Abs(Sit - Other.Sit) <= 0.005f
            && FMath::Abs(Lie - Other.Lie) <= 0.005f;
    }
};

/** Fixed numbers, in centimetres, seconds and degrees. Public so tests and the integrator read the same values. */
namespace PWNpcMove
{
constexpr double WalkSpeedCmPerSec = 140.0;       // the project clock: 140 cm per game minute (LANE_LIFE_PLAN section B)
constexpr double CatchUpSpeedCmPerSec = 200.0;    // modest: a jog, never a run
constexpr double CatchUpLagCm = 60.0;             // further behind the core's target than this, catch up
constexpr double SnapDistanceCm = 500.0;          // further off than this AND unseen: the only teleport
constexpr double ArrivalToleranceCm = 15.0;
constexpr double ExtrapolationCapSeconds = 1.25;  // the target is extrapolated no further past the last core update
constexpr double SweepAheadCm = 110.0;
constexpr double SweepRadiusCm = 30.0;            // a little under the 34 cm capsule: routes keep 34 cm to every wall
constexpr double YieldAfterSeconds = 0.25;        // blocked this long: show the Yield state
constexpr double PersistentBlockSeconds = 4.0;    // blocked this long: tell the integrator (a bounded core command, not us)
constexpr double GestureFinishSeconds = 0.8;      // a worker finishes the stroke in hand before attending a conversation
constexpr double DecisionIntervalSeconds = 0.1;   // 10 Hz decisions (sweeps, snap checks, visibility)
// Every frame while walking. The character movement consumes its input vector each frame and brakes on a frame without one,
// so input given at 30 Hz to a game running at 60 or 120 fps leaves people crawling at a fraction of the commanded pace.
constexpr double MoverTickIntervalSeconds = 0.0;
constexpr double RouteLookAheadCm = 50.0;         // steer at the route this far ahead: cuts a right-angle corner by under 18 cm
constexpr double FinishRouteJoinCm = 60.0;        // a remembered route ends this close to the stand: the rest of the walk is it
constexpr double PoseNearSeconds = 0.05;          // 20 Hz pose updates nearby
constexpr double PoseFarSeconds = 0.2;
constexpr double PoseNearRangeCm = 1500.0;        // beyond 15 m and unseen: no pose work at all

/** Speed to walk at, given how far behind the (extrapolated) core target the person is. Pure. Negative lag = ahead. */
inline double SpeedForLag(const double LagCm, const double TargetSpeedCmPerSec)
{
    if (LagCm < -10.0)
    {
        return 0.0; // ahead of the core's target: wait for it
    }
    const double Desired = TargetSpeedCmPerSec + 2.0 * FMath::Max(LagCm, 0.0);
    return FMath::Clamp(Desired, 0.0, LagCm > CatchUpLagCm ? CatchUpSpeedCmPerSec : WalkSpeedCmPerSec);
}

/** The one teleport rule: more than SnapDistanceCm off, not seen recently, not blocked, destination free. Pure. */
inline bool ShouldSnap(const double DistanceCm, const bool bRecentlyRendered, const bool bBlocked, const bool bDestinationOccupied)
{
    return DistanceCm > SnapDistanceCm && !bRecentlyRendered && !bBlocked && !bDestinationOccupied;
}
} // namespace PWNpcMove

/** The procedural work gestures and idle motion, table driven. Pure; the same inputs give the same pose. */
namespace PWNpcGesture
{
/** One stroke of a work gesture, degrees. Amplitudes swing with sin over Cycle; a negative amplitude moves opposite. */
struct FGesture
{
    const TCHAR* Id;
    float Cycle, RightBase, RightAmp, LeftBase, LeftAmp, LeanBase, LeanAmp, Roll, BobCm;
};
inline constexpr FGesture Table[] = {
    {TEXT("work.serve"), 3.0f, 45.0f, 25.0f, 8.0f, 0.0f, 3.0f, 0.0f, 0.0f, 0.0f},     // hands a cup across
    {TEXT("work.cook"), 2.0f, 55.0f, 12.0f, 25.0f, 0.0f, 6.0f, 2.0f, 0.0f, 0.0f},     // stirs
    {TEXT("work.sweep"), 1.6f, 35.0f, 15.0f, 35.0f, -15.0f, 12.0f, 4.0f, 6.0f, 0.4f}, // both hands on a broom, leaning
    {TEXT("work.stock"), 3.5f, 55.0f, 40.0f, 30.0f, -40.0f, 4.0f, 2.0f, 0.0f, 0.8f},  // reaches up and down to a shelf
    {TEXT("work.wash"), 1.0f, 48.0f, 6.0f, 48.0f, -6.0f, 8.0f, 0.0f, 3.0f, 0.0f},     // scrubs
    {TEXT("work.care"), 4.0f, 40.0f, 15.0f, 10.0f, 0.0f, 4.0f, 0.0f, 0.0f, 0.0f},
    {TEXT("work.teach"), 4.5f, 55.0f, 20.0f, 10.0f, 0.0f, 2.0f, 0.0f, 0.0f, 0.0f},    // an explaining hand
    {TEXT("work.study"), 5.0f, 50.0f, 3.0f, 50.0f, 0.0f, 8.0f, 1.0f, 0.0f, 0.0f},     // holds a book
    {TEXT("work.inspect"), 3.0f, 50.0f, 10.0f, 20.0f, 0.0f, 10.0f, 3.0f, 0.0f, 0.0f},
    {TEXT("work.repair"), 1.2f, 50.0f, 28.0f, 30.0f, -10.0f, 8.0f, 3.0f, 0.0f, 0.3f},
    {TEXT("work.test"), 2.5f, 38.0f, 8.0f, 42.0f, -5.0f, 12.0f, 2.0f, 0.0f, 0.0f},
    {TEXT("work.collect"), 3.0f, 60.0f, 20.0f, 35.0f, 12.0f, 2.0f, 1.0f, 0.0f, 0.0f},
    {TEXT("work.weigh"), 4.0f, 42.0f, 12.0f, 55.0f, -8.0f, 7.0f, 2.0f, 0.0f, 0.0f},
    {TEXT("work.measure"), 4.0f, 38.0f, 18.0f, 38.0f, -18.0f, 5.0f, 1.0f, 0.0f, 0.0f},
    {TEXT("work.cut"), 2.0f, 48.0f, 16.0f, 44.0f, 0.0f, 10.0f, 2.0f, 0.0f, 0.0f},
    {TEXT("work.stitch"), 1.0f, 50.0f, 5.0f, 50.0f, -3.0f, 7.0f, 1.0f, 0.0f, 0.0f},
    {TEXT("work.press"), 3.0f, 55.0f, 14.0f, 15.0f, 0.0f, 9.0f, 2.0f, 0.0f, 0.0f},
};
inline constexpr FGesture Generic = {TEXT("work.activity"), 3.5f, 35.0f, 14.0f, 8.0f, 0.0f, 3.0f, 0.0f, 0.0f, 0.0f};
inline constexpr double TwoPi = 6.283185307179586;

/** An unknown or empty id gets the generic gesture (also what "work.activity" is). */
inline const FGesture& Find(const FName Id)
{
    for (const FGesture& Row : Table)
    {
        if (Id == FName(Row.Id))
        {
            return Row;
        }
    }
    return Generic;
}

/** Seconds one stroke takes. */
inline float CycleSeconds(const FName WorkActionId) { return Find(WorkActionId).Cycle; }

/** Arms, lean and sway for a work action at Seconds into it. bReduced holds the mid-stroke pose and moves nothing. */
inline FPWNpcPose Work(const FName WorkActionId, const float Seconds, const bool bReduced)
{
    const FGesture& G = Find(WorkActionId);
    const float S = bReduced ? 0.0f : static_cast<float>(FMath::Sin(TwoPi * Seconds / G.Cycle));
    FPWNpcPose Pose;
    Pose.ArmPitch[0] = G.LeftBase + G.LeftAmp * S;
    Pose.ArmPitch[1] = G.RightBase + G.RightAmp * S;
    Pose.LeanPitch = G.LeanBase + G.LeanAmp * S;
    Pose.SwayRoll = G.Roll * S;
    Pose.BobCm = G.BobCm * FMath::Abs(S);
    return Pose;
}

inline FPWNpcPose Idle(const float Seconds, const bool bReduced)
{
    FPWNpcPose Pose;
    Pose.SwayRoll = bReduced ? 0.0f : static_cast<float>(1.1 * FMath::Sin(TwoPi * Seconds / 5.5));
    return Pose;
}

/** Legs and arms swing once per 140 cm walked, so the feet keep pace with the speed. */
inline FPWNpcPose Walk(const float DistanceWalkedCm, const float SpeedFraction)
{
    const float Frac = FMath::Clamp(SpeedFraction, 0.0f, 1.4f);
    const float S = static_cast<float>(FMath::Sin(TwoPi * DistanceWalkedCm / PWNpcMove::WalkSpeedCmPerSec));
    FPWNpcPose Pose;
    Pose.LegPitch[0] = 28.0f * S * Frac;
    Pose.LegPitch[1] = -28.0f * S * Frac;
    Pose.ArmPitch[0] = -20.0f * S * Frac;
    Pose.ArmPitch[1] = 20.0f * S * Frac;
    Pose.BobCm = 1.2f * (1.0f - FMath::Abs(S)) * Frac;
    return Pose;
}

/** A raised hand: "after you". */
inline FPWNpcPose Yielding()
{
    FPWNpcPose Pose;
    Pose.ArmPitch[1] = 75.0f;
    Pose.LeanPitch = -3.0f;
    return Pose;
}

inline FPWNpcPose Sitting()
{
    FPWNpcPose Pose;
    Pose.Sit = 1.0f;
    Pose.ArmPitch[0] = Pose.ArmPitch[1] = 20.0f;
    Pose.LeanPitch = 4.0f;
    return Pose;
}

inline FPWNpcPose Sleeping()
{
    FPWNpcPose Pose;
    Pose.Lie = 1.0f;
    return Pose;
}
} // namespace PWNpcGesture

/** A world-space polyline with the arithmetic route following needs. Pure. */
struct PERFECTWORLD_API FPWNpcPolyline
{
    TArray<FVector> Points;
    TArray<double> Cumulative; // distance along the line to each point, 3D

    /**
     * Replaces the line; points closer than 1 cm to the previous one are dropped. A leg that turns straight back along the
     * one before it (a journey composed through a hub it only passes, such as door -> counter -> packing desk) is collapsed
     * to the line it folds onto: the dead-end spur would leave the way ahead behind the walker. Defined in the .cpp.
     */
    void Set(const TArray<FVector>& InPoints);

    bool IsValid() const { return Points.Num() >= 2; }
    double Length() const { return Cumulative.IsEmpty() ? 0.0 : Cumulative.Last(); }

    /** The point Distance along the line, clamped to its ends. */
    FVector PointAt(const double Distance) const
    {
        if (Points.IsEmpty())
        {
            return FVector::ZeroVector;
        }
        const double D = FMath::Clamp(Distance, 0.0, Length());
        for (int32 I = 0; I + 1 < Points.Num(); ++I)
        {
            if (D <= Cumulative[I + 1])
            {
                const double Span = Cumulative[I + 1] - Cumulative[I];
                return FMath::Lerp(Points[I], Points[I + 1], Span > 0.0 ? (D - Cumulative[I]) / Span : 0.0);
            }
        }
        return Points.Last();
    }

    /** Unit direction of the segment containing Distance. */
    FVector DirectionAt(const double Distance) const
    {
        for (int32 I = 0; I + 1 < Points.Num(); ++I)
        {
            if (Distance <= Cumulative[I + 1] || I + 2 == Points.Num())
            {
                return (Points[I + 1] - Points[I]).GetSafeNormal();
            }
        }
        return FVector::ForwardVector;
    }

    /**
     * Distance along the line of the point nearest to P in the horizontal plane. With Hint >= 0 only segments within
     * [Hint - 150, Hint + 500] are considered, so a line that doubles back cannot make the person jump along it.
     * OutLateralCm receives the horizontal distance from P to that point.
     */
    double Project(const FVector& P, const double Hint, double& OutLateralCm) const
    {
        double BestAlong = 0.0;
        double BestSq = TNumericLimits<double>::Max();
        for (int32 I = 0; I + 1 < Points.Num(); ++I)
        {
            if (Hint >= 0.0 && (Cumulative[I + 1] < Hint - 150.0 || Cumulative[I] > Hint + 500.0))
            {
                continue;
            }
            const FVector2D A(Points[I]), B(Points[I + 1]), Q(P);
            const FVector2D AB = B - A;
            const double Len2 = AB.SizeSquared();
            const double T = Len2 > 1e-6 ? FMath::Clamp(FVector2D::DotProduct(Q - A, AB) / Len2, 0.0, 1.0) : 0.0;
            const double Sq = FVector2D::DistSquared(Q, A + AB * T);
            if (Sq < BestSq)
            {
                BestSq = Sq;
                BestAlong = Cumulative[I] + T * (Cumulative[I + 1] - Cumulative[I]);
            }
        }
        if (BestSq == TNumericLimits<double>::Max())
        {
            return Hint >= 0.0 && IsValid() ? Project(P, -1.0, OutLateralCm) : 0.0;
        }
        OutLateralCm = FMath::Sqrt(BestSq);
        return BestAlong;
    }
};

DECLARE_MULTICAST_DELEGATE_OneParam(FPWNpcPoseSignature, const FPWNpcPose&);
/** (bPersistentlyBlocked, RouteId). True once the way has been blocked for PersistentBlockSeconds; false when it clears. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FPWNpcBlockSignature, bool, FName);

/**
 * Presents a NpcIntent as walking, working and waiting (LANE_LIFE_PLAN section B "small AI"; LANE_LIFE_IMPLEMENTATION
 * "Navigation: choose authored waypoint steering"). Owned by APWNpcCharacter.
 *
 * It decides nothing about the person's life. SetIntent is fed from outside; the component only makes that intent visible:
 *  - Travel: follows the route polyline at the core's pace by steering the character movement (collision respected).
 *    The target is the point at ProgressPermille, extrapolated for at most 1.25 s between core updates. Behind by more
 *    than 60 cm: walks up to 200 cm/s to catch up. The ONLY teleport: more than 5 m from the target, not rendered
 *    recently, not blocked, destination free. Ahead of the target: waits. Never teleports to pass a wall or a door.
 *  - Local avoidance (10 Hz): a capsule sweep ahead. A player in the way: stop, face them, show the Yield pose. Another
 *    person or a fixture: try two sidesteps, else Yield. Blocked 4 s: OnBlockChanged(true) so the integrator can report it
 *    through a bounded core command; the person keeps waiting visibly (no hidden teleport, no animation-awarded outcome).
 *  - Work, Idle, Sleep: walk the last few steps to the anchor, then stand there and play a block-arm gesture chosen by
 *    WorkActionId (serving, washing, sweeping, stocking, cooking, ...), a seat pose, or lie down. Presentation only.
 *    A person still on the way when the intent stops being Travel (late, held up, or a skipped clock) finishes along the
 *    route they were walking, which ends at the anchor; only the last steps from its end are a straight line.
 *  - Chat: stop, face the player, hold; a worker finishes the stroke in hand first (at most 0.8 s). The next SetIntent
 *    resumes. Interact on the character starts a local Chat at once, ended when the conversation panel closes.
 *  - Reduced motion: no sway, no gesture strokes (a still mid-stroke pose), no slow look-at turns (facing snaps to the player
 *    only while spoken to). Walking itself is the world's state, not ambience, and is unchanged.
 *
 * Budget: ticks every frame while moving, 20 Hz while a gesture plays, 10 Hz when still; decisions at 10 Hz; pose at 20 Hz
 * within 15 m and 5 Hz beyond; nothing at all when beyond 15 m and unseen.
 */
UCLASS(ClassGroup = (PerfectWorld))
class PERFECTWORLD_API UPWNpcMovementComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPWNpcMovementComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** Presents this intent. Cheap; call it as often as the core resolves one. An identical call changes nothing. */
    void SetIntent(const FPWNpcIntentView& NewIntent);
    /** A world-space polyline of feet points for the current Travel intent's route (replaces the resolver for it). */
    void SetRoute(const TArray<FVector>& WorldPoints);
    void SetAnchorResolver(FPWNpcAnchorResolver Resolver) { AnchorResolver = MoveTemp(Resolver); }
    void SetRouteResolver(FPWNpcRouteResolver Resolver) { RouteResolver = MoveTemp(Resolver); }
    /** The player is being spoken to by this person (set by Interact, cleared when the panel closes by itself). */
    void SetLocalConversationActive(bool bActive);
    void SetCoreInterruptions(bool bEnabled);
    void SetCoreDoorwayWait(bool bActive, const FVector& Feet) { bR55DoorwayWait = bActive; R55WaitingFeet = Feet; }
    /**
     * Places the person at the intent's target at once: for a load or a first spawn the integrator has decided, never for
     * ordinary play. Does nothing when the destination overlaps a pawn.
     */
    bool ForceSnapToIntent();

    /** Advances everything by DeltaSeconds. TickComponent calls it; tests call it directly. */
    void TickPresentation(float DeltaSeconds);

    const FPWNpcIntentView& GetIntent() const { return Intent; }
    bool HasIntent() const { return bHasIntent; }
    /** What is shown right now: Chat if a conversation is open here, Yield while physically waiting, else the intent's state. */
    EPWNpcState GetPresentedState() const;
    bool IsTalkable() const { return !bHasIntent || Intent.bTalkable; }
    FName GetDeclineReasonKey() const { return bHasIntent ? Intent.DeclineReasonKey : FName(); }
    bool IsYielding() const { return bLocallyYielding; }
    bool IsPersistentlyBlocked() const { return bPersistentlyBlocked; }
    double GetBlockedSeconds() const { return BlockedSeconds; }
    bool IsMoving() const { return bMovedThisTick; }
    bool HasRoute() const { return Route.IsValid(); }
    const FPWNpcPose& GetPose() const { return CurrentPose; }
    /** Where the core's target for a Travel intent is right now (extrapolated), for tests and debugging. */
    FVector GetTravelTarget() const;

    FPWNpcPoseSignature OnPose;
    FPWNpcBlockSignature OnBlockChanged;

private:
    void ResolveRoute();
    void ResolveAnchor();
    void UpdateProgress(bool bReset);
    void BeginChatSettle(FName WorkAction);
    void Decide(double DecisionSeconds);
    void Hold(float Dt, EPWNpcState Presented);
    void UpdateMotion(float Dt);
    void MoveAlongRoute(float Dt);
    void MoveToStand(float Dt);
    bool AimAlongFinishRoute(const FVector& Stand, const FVector& Feet, FVector& OutDirection, double& OutSpeed);
    void ApplySteering(const FVector& Direction, double Speed, float Dt);
    void FaceYaw(double TargetYaw, double DegreesPerSecond, float Dt);
    void UpdatePose(float Dt);
    FPWNpcPose DesiredPose() const;
    double ExtrapolatedTargetDistance() const;
    double TargetSpeedNow() const;
    bool SweepBlocked(const FVector& Direction, AActor*& OutBlocker) const;
    bool TrySnap(const FVector& FeetDestination, double FeetYaw);
    bool TryAvoid(const FVector& Direction, AActor* Hit);
    FVector FeetLocation() const;
    double HalfHeight() const;
    APawn* LocalPlayer() const;
    void SetTickRate();
    void SetBlockedState(bool bPersistent);
    bool R55PresentCoreInterruption(float Dt);

    FPWNpcIntentView Intent;
    bool bHasIntent = false;
    bool bR55CoreInterruptions = false;
    bool bR55DoorwayWait = false;
    FVector R55WaitingFeet = FVector::ZeroVector;
    FPWNpcAnchorResolver AnchorResolver;
    FPWNpcRouteResolver RouteResolver;

    FPWNpcPolyline Route;
    FName RouteKey; // the route id Route belongs to; None with a non-empty Route means "adopt for the next Travel intent"
    /** The Travel route that was being walked when the intent stopped being Travel; cleared on arrival or a new Travel. */
    FPWNpcPolyline FinishRoute;
    double FinishDistanceCm = -1.0;
    bool bAnchorKnown = false;
    FVector StandFeet = FVector::ZeroVector;
    double StandYaw = 0.0;
    FVector HomeFeet = FVector::ZeroVector;
    double HomeYaw = 0.0;

    // Travel tracking: the core's progress, when it last changed, and how fast it has been changing.
    double ClockSeconds = 0.0;
    double TargetDistanceCm = 0.0;
    double TargetChangedAt = 0.0;
    double TargetSpeedEstimate = PWNpcMove::WalkSpeedCmPerSec;
    double NpcDistanceCm = 0.0;
    double LateralCm = 0.0;
    FVector SteerDirection = FVector::ZeroVector;
    double AvoidYawDegrees = 0.0;

    double DecisionAccumulator = 0.0;
    double PoseAccumulator = 0.0;
    double BlockedSeconds = 0.0;
    bool bBlockedAhead = false;
    bool bBlockedByPlayer = false;
    bool bLocallyYielding = false;
    bool bPersistentlyBlocked = false;
    bool bMovedThisTick = false;
    bool bArrived = false;
    bool bLocalChat = false;
    bool bChatPanelSeen = false; // the conversation panel was open when the local chat began, so its closing ends the chat
    bool bReduced = false;
    bool bNearPlayer = true;
    bool bRecentlyRendered = true;
    TWeakObjectPtr<AActor> Blocker;

    double WalkedCm = 0.0;
    double GestureSeconds = 0.0;
    double ChatSettleRemaining = 0.0;
    FPWNpcPose CurrentPose;
    FPWNpcPose BroadcastPose;
    double LastTickInterval = -1.0;
};
