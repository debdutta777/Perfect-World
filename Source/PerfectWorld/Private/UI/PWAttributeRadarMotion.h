#pragma once

#include "CoreMinimal.h"

namespace PWRadar
{
enum class EAxisSignal : uint8 { None, Rose, Fell };

/**
 * The radar's time-dependent state, with no Slate in it: where each axis is drawn while the polygon morphs, which
 * axes just rose or fell, and the shake of a rejected edit. It is advanced by the widget (or by a test) with an
 * explicit time step, so it is deterministic.
 *
 * Reduced motion: a retarget lands on its final shape at once, and the highlight and rejection cue are held as a
 * steady tint for the same length of time instead of fading or moving. Nothing in this class moves under it.
 */
class FMotion
{
public:
    /** Polygon morph length, about 200 ms. */
    static constexpr float MorphSeconds = 0.2f;
    /** How long a rose / fell highlight stays. */
    static constexpr float SignalSeconds = 1.2f;
    /** How long a rejection shakes (or, under reduced motion, stays flashed). */
    static constexpr float RejectSeconds = 0.36f;
    /** Largest sideways displacement of a shake, in slate units. */
    static constexpr float ShakeAmplitude = 5.0f;

    /** Jumps to these fractions with no animation and no highlight. */
    void Snap(const TArray<float>& Fractions, const TArray<int32>& Points);

    /**
     * Moves toward new fractions. Axes whose points changed are marked as risen or fallen. Starts from what is
     * drawn now, so a change that arrives mid-morph does not jump. A different axis count snaps.
     */
    void Retarget(const TArray<float>& Fractions, const TArray<int32>& Points, bool bReducedMotion);

    /** A refused edit: shake, or under reduced motion a steady flash. */
    void Reject(bool bReducedMotion);

    /** Finishes the morph and holds any fade at full strength; call when reduced motion switches on. */
    void SettleForReducedMotion();

    void Advance(float DeltaSeconds);

    int32 Num() const { return To.Num(); }
    /** The eased fraction drawn for an axis right now. */
    float Displayed(int32 Axis) const;
    float Target(int32 Axis) const { return To.IsValidIndex(Axis) ? To[Axis] : 0.0f; }
    TArray<float> DisplayedAll() const;
    /** 0 at the start of a morph, 1 once it has landed. */
    float MorphAlpha() const;
    bool IsMorphing() const { return MorphElapsed < MorphSeconds && Num() > 0; }

    EAxisSignal Signal(int32 Axis) const;
    /** 1 down to 0 over SignalSeconds; held at 1 until the end under reduced motion. */
    float SignalStrength(int32 Axis) const;
    /** The signed points change that raised the highlight. */
    int32 SignalDelta(int32 Axis) const { return Deltas.IsValidIndex(Axis) ? Deltas[Axis] : 0; }

    /** Sideways offset of the budget for the rejection cue; always 0 under reduced motion. */
    float ShakeOffset() const;
    /** 1 down to 0 for the red flash of a rejection; held at 1 under reduced motion until it ends. */
    float RejectFlash() const;
    bool IsActive() const;

private:
    TArray<float> From, To;
    TArray<int32> LastPoints, Deltas;
    TArray<float> SignalLeft;
    float MorphElapsed = MorphSeconds;
    float RejectLeft = 0.0f;
    bool bReducedSignals = false;
    bool bReducedReject = false;
};
} // namespace PWRadar
