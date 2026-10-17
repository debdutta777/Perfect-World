#pragma once

#include "UI/PWSigilPainter.h"
#include "Widgets/SLeafWidget.h"

/** Decorative only: no input, focus or accessible reading-order entry. */
class SPWAwakeningSigil final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SPWAwakeningSigil)
        : _SizeClass(EPWSigilSizeClass::Aperture), _Pictogram(EPWSigilPictogram::None), _Diameter(400.0f),
          _FitRing(false), _Reveal(false), _Rotation(false), _Motes(false) {}
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
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);
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
    bool IsPresented() const;
    bool IsFitted() const;

    EPWSigilSizeClass SizeClass = EPWSigilSizeClass::Aperture;
    EPWSigilPictogram Pictogram = EPWSigilPictogram::None;
    TAttribute<float> Diameter;
    TAttribute<bool> Motes;
    TAttribute<bool> FitRing;
    bool bReveal = false, bRotation = false;
    float RevealSeconds = 0, RotationRadians = 0;
    double LastAnimationTime = 0;
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
