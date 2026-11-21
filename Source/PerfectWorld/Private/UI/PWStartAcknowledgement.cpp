#include "UI/PWStartAcknowledgement.h"

namespace PWUI
{
bool FStartAcknowledgement::ShouldPlay(const bool bFreshLifeSaved, const bool bPlayerPlaced)
{
    return bFreshLifeSaved && bPlayerPlaced;
}

bool FStartAcknowledgement::Begin(const bool bReducedMotion)
{
    Cancel();
    if (bReducedMotion)
    {
        return false;
    }
    bActive = true;
    Elapsed = 0.0f;
    ++StartCount;
    return true;
}

bool FStartAcknowledgement::Advance(const float DeltaSeconds)
{
    if (!bActive)
    {
        return false;
    }
    Elapsed += FMath::Max(0.0f, DeltaSeconds);
    if (Elapsed >= DurationSeconds)
    {
        Cancel();
    }
    return bActive;
}

void FStartAcknowledgement::OnMotionPreference(const bool bReducedMotion)
{
    if (bReducedMotion)
    {
        Cancel();
    }
}

void FStartAcknowledgement::Cancel()
{
    bActive = false;
    Elapsed = 0.0f;
}

float FStartAcknowledgement::GetScale() const
{
    if (!bActive)
    {
        return 1.0f;
    }
    const float T = FMath::Clamp(Elapsed / DurationSeconds, 0.0f, 1.0f);
    return 1.0f + Expansion * (1.0f - FMath::Pow(1.0f - T, 3.0f)); // cubic ease-out
}

float FStartAcknowledgement::GetOpacity() const
{
    if (!bActive)
    {
        return 0.0f;
    }
    const float T = FMath::Clamp(Elapsed / DurationSeconds, 0.0f, 1.0f);
    return PeakOpacity * (1.0f - T * T * (3.0f - 2.0f * T)); // smoothstep fall, monotonic: no flicker
}
} // namespace PWUI
