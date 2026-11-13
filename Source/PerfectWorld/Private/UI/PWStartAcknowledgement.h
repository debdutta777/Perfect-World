#pragma once

#include "CoreMinimal.h"

namespace PWUI
{
/**
 * The clock and gate of the short successful-start acknowledgement (LL-04, second submission): about 450 ms, in which every
 * sector of the aperture expands together by about 12% while it fades. It is decoration only. It has no say in input, in
 * placement or in saving, and nothing waits for it; the front end plays it after the world is already in the player's hands.
 *
 * Pure state, no Slate and no world, so the rules are testable: it plays only after a fresh life was saved and the player
 * was placed, never under reduced motion, and it ends the moment reduced motion is switched on while it runs.
 */
struct FStartAcknowledgement
{
    static constexpr float DurationSeconds = 0.45f;
    /** All sectors grow together by this fraction of their size. */
    static constexpr float Expansion = 0.12f;
    /** The ring never appears brighter than this; the cue is quiet, not a flash. */
    static constexpr float PeakOpacity = 0.7f;

    /** True only for a fresh life that was saved AND placed in the world. A load, a failed save or a failed placement is false. */
    static bool ShouldPlay(bool bFreshLifeSaved, bool bPlayerPlaced);

    /** Starts (or restarts) the acknowledgement. Under reduced motion nothing animates and this returns false. */
    bool Begin(bool bReducedMotion);

    /** Moves the clock on. Returns true while the acknowledgement is still showing. */
    bool Advance(float DeltaSeconds);

    /** The player switched reduced motion on or off. Turning it on ends a running acknowledgement at once. */
    void OnMotionPreference(bool bReducedMotion);

    void Cancel();

    bool IsActive() const { return bActive; }
    /** How many times Begin has actually started an acknowledgement. */
    int32 GetStartCount() const { return StartCount; }
    float GetElapsedSeconds() const { return Elapsed; }

    /** 1 at the start, 1.12 at the end; every sector uses the same factor. */
    float GetScale() const;
    /** From PeakOpacity down to 0 over the duration; 0 when idle. */
    float GetOpacity() const;

private:
    bool bActive = false;
    float Elapsed = 0.0f;
    int32 StartCount = 0;
};
} // namespace PWUI
