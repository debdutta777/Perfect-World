#pragma once

#include "CoreMinimal.h"

class SWeakWidget;
class SWidget;

namespace PWAwakening
{
/**
 * R9-06: the floor's on-screen layer: the transfer fade, subtitles (every announcement and consequence is also text) and the
 * shape-and-text hazard cue. It shows what the floor tells it and nothing else; it takes no input (HitTestInvisible) so keyboard
 * and controller navigation stay with the interaction component's prompt. Text sizes scale with TextScale (1.0..2.0). Without a game
 * viewport (tests) the state still works; only the Slate overlay is skipped.
 */
class FAwHud : public TSharedFromThis<FAwHud>
{
public:
    FAwHud() = default;
    ~FAwHud();
    FAwHud(const FAwHud&) = delete;
    FAwHud& operator=(const FAwHud&) = delete;

    /** Adds the overlay to the game viewport once. False when there is no viewport (state still usable). */
    bool Ensure(float InTextScale);
    void Remove();

    void SetTextScale(float InTextScale);
    float GetTextScale() const { return TextScale; }

    /** 0 clear .. 1 black. */
    void SetFade(float Alpha) { Fade = FMath::Clamp(Alpha, 0.0f, 1.0f); }
    float GetFade() const { return Fade; }

    /** Shows a subtitle until Now + Seconds (world seconds). A newer one replaces the older. */
    void ShowCaption(const FText& Text, double NowSeconds, float Seconds);
    void Update(double NowSeconds);
    bool IsCaptionVisible() const { return !Caption.IsEmpty() && CaptionVisible; }
    const FText& GetCaption() const { return Caption; }

    /** A hazard cue: a shape glyph and words, e.g. "[!!!] GAP AHEAD - ...". Empty hides it. */
    void SetCue(const FText& Text) { Cue = Text; }
    const FText& GetCue() const { return Cue; }

    bool HasOverlay() const { return Holder.IsValid(); }

private:
    float TextScale = 1.0f;
    float Fade = 0.0f;
    FText Caption;
    bool CaptionVisible = false;
    double CaptionUntil = 0.0;
    FText Cue;
    TSharedPtr<SWidget> Content;
    TSharedPtr<SWeakWidget> Holder;
};
} // namespace PWAwakening
