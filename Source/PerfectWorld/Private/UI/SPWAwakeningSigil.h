#pragma once

#include "UI/PWSigilPainter.h"
#include "UI/PWStartAcknowledgement.h"
#include "Widgets/SLeafWidget.h"

/** Decorative only: no input, focus or accessible reading-order entry. */
class SPWAwakeningSigil final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SPWAwakeningSigil)
        : _SizeClass(EPWSigilSizeClass::Aperture), _Pictogram(EPWSigilPictogram::None), _Diameter(400.0f),
          _FitRing(false), _Reveal(false), _Rotation(false), _Motes(false), _Acknowledgement(false) {}
        SLATE_ARGUMENT(EPWSigilSizeClass, SizeClass)
        SLATE_ARGUMENT(EPWSigilPictogram, Pictogram)
        SLATE_ATTRIBUTE(float, Diameter)
        /**
         * Reserve only the ring and its calibration ticks (1.2 x the diameter) instead of the full 1.5 x envelope that
         * the drifting motes need, so a narrow slot yields a larger aperture. Motes are not drawn in this mode.
         */
        SLATE_ATTRIBUTE(bool, FitRing)
        SLATE_ARGUMENT(bool, Reveal)
        SLATE_ARGUMENT(bool, Rotation)
        SLATE_ATTRIBUTE(bool, Motes)
        /**
         * A one-shot successful-start cue (LL-04): the widget stays collapsed and paints nothing until PlayAcknowledgement,
         * then every sector expands together by about 12% and fades over 450 ms, and it collapses itself again. Use with
         * FitRing. It is never shown under reduced motion and ends at once if reduced motion is switched on while it runs.
         */
        SLATE_ARGUMENT(bool, Acknowledgement)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);

    /** Starts the cue. False (and nothing shown) if this is not an acknowledgement widget or reduced motion is on. */
    bool PlayAcknowledgement();
    void CancelAcknowledgement();
    bool IsAcknowledging() const { return Ack.IsActive(); }
    /** The cue's clock step. The active timer calls it; it is public so a test can advance time deterministically. */
    void AdvanceAcknowledgement(float DeltaSeconds);
    const PWUI::FStartAcknowledgement& GetAcknowledgement() const { return Ack; }
    virtual ~SPWAwakeningSigil() override;
    virtual FVector2D ComputeDesiredSize(float) const override;
    virtual bool SupportsKeyboardFocus() const override { return false; }
    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
                          FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override;

private:
    void MotionChanged();
    void SlatePostTick(float Delta);
    void StopMotionTimer();
    EActiveTimerReturnType Animate(double Time, float Delta);
    EActiveTimerReturnType AnimateAcknowledgement(double Time, float Delta);
    void StopAcknowledgementTimer();
    bool IsPresented() const;
    bool IsFitted() const;

    EPWSigilSizeClass SizeClass = EPWSigilSizeClass::Aperture;
    EPWSigilPictogram Pictogram = EPWSigilPictogram::None;
    TAttribute<float> Diameter;
    TAttribute<bool> Motes;
    TAttribute<bool> FitRing;
    bool bReveal = false, bRotation = false, bAcknowledgement = false;
    float RevealSeconds = 0, RotationRadians = 0;
    double LastAnimationTime = 0, LastAcknowledgementTime = 0;
    PWUI::FStartAcknowledgement Ack;
    TWeakPtr<FActiveTimerHandle> AckTimer;
    mutable FPWSigilGeometry Paths;
    mutable bool bPaintedSincePostTick = false;
    TWeakPtr<FActiveTimerHandle> MotionTimer;
    FDelegateHandle MotionSubscription;
    FDelegateHandle VisibilitySubscription;
};

class SPWCornerBrackets final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SPWCornerBrackets) {}
    SLATE_END_ARGS()
    void Construct(const FArguments&);
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
                          FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool) const override;
private:
    mutable FPWBracketGeometry Paths;
};
