#include "UI/PWAttributeRadarMotion.h"

#include "UI/PWAttributeRadarModel.h"

namespace PWRadar
{
void FMotion::Snap(const TArray<float>& Fractions, const TArray<int32>& Points)
{
    From = Fractions;
    To = Fractions;
    LastPoints = Points;
    Deltas.Init(0, Fractions.Num());
    SignalLeft.Init(0.0f, Fractions.Num());
    MorphElapsed = MorphSeconds;
}

void FMotion::Retarget(const TArray<float>& Fractions, const TArray<int32>& Points, bool bReducedMotion)
{
    if (Fractions.Num() != To.Num() || Points.Num() != LastPoints.Num())
    {
        Snap(Fractions, Points);
        return;
    }
    // Start from what is on screen so an edit made mid-morph continues smoothly.
    From = DisplayedAll();
    To = Fractions;
    for (int32 Axis = 0; Axis < Points.Num(); ++Axis)
    {
        const int32 Change = Points[Axis] - LastPoints[Axis];
        if (Change == 0) continue;
        Deltas[Axis] = Change;
        SignalLeft[Axis] = SignalSeconds;
    }
    LastPoints = Points;
    bReducedSignals = bReducedMotion;
    MorphElapsed = bReducedMotion ? MorphSeconds : 0.0f;
}

void FMotion::Reject(bool bReducedMotion)
{
    RejectLeft = RejectSeconds;
    bReducedReject = bReducedMotion;
}

void FMotion::SettleForReducedMotion()
{
    MorphElapsed = MorphSeconds;
    bReducedSignals = true;
    bReducedReject = true;
}

void FMotion::Advance(float DeltaSeconds)
{
    const float Step = FMath::Max(0.0f, DeltaSeconds);
    MorphElapsed = FMath::Min(MorphSeconds, MorphElapsed + Step);
    for (float& Left : SignalLeft) Left = FMath::Max(0.0f, Left - Step);
    RejectLeft = FMath::Max(0.0f, RejectLeft - Step);
}

float FMotion::MorphAlpha() const
{
    return MorphSeconds > 0.0f ? FMath::Clamp(MorphElapsed / MorphSeconds, 0.0f, 1.0f) : 1.0f;
}

float FMotion::Displayed(int32 Axis) const
{
    if (!To.IsValidIndex(Axis)) return 0.0f;
    const float Source = From.IsValidIndex(Axis) ? From[Axis] : To[Axis];
    return FMath::Lerp(Source, To[Axis], EaseOut(MorphAlpha()));
}

TArray<float> FMotion::DisplayedAll() const
{
    TArray<float> Result;
    for (int32 Axis = 0; Axis < To.Num(); ++Axis) Result.Add(Displayed(Axis));
    return Result;
}

EAxisSignal FMotion::Signal(int32 Axis) const
{
    if (!SignalLeft.IsValidIndex(Axis) || SignalLeft[Axis] <= 0.0f) return EAxisSignal::None;
    return Deltas[Axis] > 0 ? EAxisSignal::Rose : EAxisSignal::Fell;
}

float FMotion::SignalStrength(int32 Axis) const
{
    if (Signal(Axis) == EAxisSignal::None) return 0.0f;
    return bReducedSignals ? 1.0f : FMath::Clamp(SignalLeft[Axis] / SignalSeconds, 0.0f, 1.0f);
}

float FMotion::ShakeOffset() const
{
    if (RejectLeft <= 0.0f || bReducedReject) return 0.0f;
    const float Progress = 1.0f - RejectLeft / RejectSeconds;
    // Three damped swings.
    return ShakeAmplitude * (1.0f - Progress) * FMath::Sin(Progress * 6.0f * PI);
}

float FMotion::RejectFlash() const
{
    if (RejectLeft <= 0.0f) return 0.0f;
    return bReducedReject ? 1.0f : FMath::Clamp(RejectLeft / RejectSeconds, 0.0f, 1.0f);
}

bool FMotion::IsActive() const
{
    if (IsMorphing() || RejectLeft > 0.0f) return true;
    for (const float Left : SignalLeft)
        if (Left > 0.0f) return true;
    return false;
}
} // namespace PWRadar
