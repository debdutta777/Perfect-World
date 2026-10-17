#include "UI/SPWAwakeningSigil.h"

#include "CoreGlobals.h"
#include "Framework/Application/SlateApplication.h"
#include "PerfectWorldPresentationSettings.h"
#include "UObject/UObjectBase.h"
#include "UI/PWWidgets.h"

namespace
{
/** A fitted ring reserves its ticks (1.10 R, so 1.10 of the diameter) plus a margin: 1.2 of the diameter, against 1.5 with motes. */
constexpr float SgFitEnvelope = 1.2f;
/** The painter derives the ring from a 3 R square; a fitted ring paints inside a larger virtual square of the same centre. */
constexpr float SgPainterEnvelope = 1.5f;
} // namespace

void SPWAwakeningSigil::Construct(const FArguments& Args)
{
    SizeClass = Args._SizeClass;
    Pictogram = Args._Pictogram;
    Diameter = Args._Diameter;
    Motes = Args._Motes;
    bReveal = Args._Reveal;
    bRotation = Args._Rotation;
    FitRing = Args._FitRing;
    SetVisibility(EVisibility::HitTestInvisible);
    // Everything an aperture draws lies inside its box, except the soft lens behind a fitted ring, which is a little wider
    // than the ring's own box; clipping that would show a flat edge. Seals keep the clip.
    SetClipping(SizeClass == EPWSigilSizeClass::Aperture ? EWidgetClipping::Inherit : EWidgetClipping::ClipToBounds);
#if WITH_ACCESSIBILITY
    SetAccessibleBehavior(EAccessibleBehavior::NotAccessible);
#endif
    MotionSubscription = UPerfectWorldPresentationSettings::Get().OnMotionChanged.AddSP(this, &SPWAwakeningSigil::MotionChanged);
    if (bReveal || bRotation)
        VisibilitySubscription = FSlateApplication::Get().OnPostTick().AddSP(this, &SPWAwakeningSigil::SlatePostTick);
    if (PWUI::IsReducedMotion()) RevealSeconds = .8f;
    // TODO_StartLifeTransactionalFlourish: enable the 450ms expansion only after start-and-save is transactional.
}

SPWAwakeningSigil::~SPWAwakeningSigil()
{
    // A widget can outlive UObject shutdown on exit; the AddSP binding is weak anyway, so skip the removal then.
    if (UObjectInitialized() && !IsEngineExitRequested())
        UPerfectWorldPresentationSettings::Get().OnMotionChanged.Remove(MotionSubscription);
    if (FSlateApplication::IsInitialized()) FSlateApplication::Get().OnPostTick().Remove(VisibilitySubscription);
}

FVector2D SPWAwakeningSigil::ComputeDesiredSize(float) const
{
    const float Side = FMath::Max(1.0f, Diameter.Get(400.0f))
        * (SizeClass == EPWSigilSizeClass::Seal ? 1.0f : (IsFitted() ? SgFitEnvelope : SgPainterEnvelope));
    return FVector2D(Side, Side);
}

bool SPWAwakeningSigil::IsFitted() const
{
    return SizeClass == EPWSigilSizeClass::Aperture && FitRing.Get(false);
}

bool SPWAwakeningSigil::IsPresented() const
{
    if (!GetVisibility().IsVisible()) return false;
    for (auto Parent = GetParentWidget(); Parent.IsValid(); Parent = Parent->GetParentWidget())
        if (!Parent->GetVisibility().IsVisible()) return false;
    return true;
}

void SPWAwakeningSigil::StopMotionTimer()
{
    if (const auto Timer = MotionTimer.Pin()) UnRegisterActiveTimer(Timer.ToSharedRef());
    MotionTimer.Reset();
}

void SPWAwakeningSigil::MotionChanged()
{
    if (PWUI::IsReducedMotion())
    {
        RevealSeconds = .8f;
        StopMotionTimer();
    }
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SPWAwakeningSigil::SlatePostTick(float)
{
    // Active timers on hidden widgets are not executed by Slate. This weak application subscription
    // stops them after an unpainted frame without registering any hidden-widget wake-up timer.
    const bool bAnimate = bPaintedSincePostTick && !PWUI::IsReducedMotion() && IsPresented()
        && (bRotation || (bReveal && RevealSeconds < .8f));
    bPaintedSincePostTick = false;
    if (!bAnimate) StopMotionTimer();
    else if (!MotionTimer.IsValid())
    {
        LastAnimationTime = FSlateApplication::Get().GetCurrentTime();
        MotionTimer = RegisterActiveTimer(0, FWidgetActiveTimerDelegate::CreateSP(this, &SPWAwakeningSigil::Animate));
    }
}

EActiveTimerReturnType SPWAwakeningSigil::Animate(double Time, float)
{
    if (PWUI::IsReducedMotion() || !IsPresented())
    {
        MotionTimer.Reset();
        return EActiveTimerReturnType::Stop;
    }
    const float Seconds = static_cast<float>(FMath::Max(0.0, Time - LastAnimationTime));
    LastAnimationTime = Time;
    RevealSeconds = FMath::Min(.8f, RevealSeconds + Seconds);
    if (bRotation) RotationRadians = FMath::Fmod(RotationRadians + Seconds * (2.0f * PI / 90.0f), 2.0f * PI);
    Invalidate(EInvalidateWidgetReason::Paint);
    if (!bRotation && RevealSeconds >= .8f)
    {
        MotionTimer.Reset();
        return EActiveTimerReturnType::Stop;
    }
    return EActiveTimerReturnType::Continue;
}

int32 SPWAwakeningSigil::OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect& CullingRect,
                                FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
    if (!FSlateRect::DoRectanglesIntersect(Geometry.GetRenderBoundingRect(), CullingRect)) return Layer;
    bPaintedSincePostTick = true;
    const bool bReduced = PWUI::IsReducedMotion();
    const float T = FMath::Clamp(RevealSeconds / .8f, 0.0f, 1.0f);
    const float Reveal = bReduced || !bReveal ? 1.0f : 1.0f - FMath::Pow(1.0f - T, 3.0f);
    if (IsFitted())
    {
        // The painter sizes the ring from a square of 3 R. Give it the larger virtual square that makes the ring exactly
        // fill the allotted box less the tick margin, centred on the widget, and never draw motes (they would be cut).
        const FVector2f Local(Geometry.GetLocalSize());
        const float Virtual = FMath::Max(1.0f, FMath::Min(Local.X, Local.Y)) * SgPainterEnvelope / SgFitEnvelope;
        const FVector2f VirtualSize(Virtual, Virtual);
        Paths.Rebuild(VirtualSize, SizeClass);
        const FGeometry Frame = Geometry.MakeChild(VirtualSize, FSlateLayoutTransform((Local - VirtualSize) * .5f));
        return FPWSigilPainter::Paint(Paths, SizeClass, Pictogram, Frame, Elements, Layer, Style, Reveal,
                                      bRotation ? RotationRadians : 0.0f, false, bEnabled && IsEnabled());
    }
    Paths.Rebuild(FVector2f(Geometry.GetLocalSize()), SizeClass);
    return FPWSigilPainter::Paint(Paths, SizeClass, Pictogram, Geometry, Elements, Layer, Style, Reveal,
                                  bRotation ? RotationRadians : 0.0f, Motes.Get(false) && !bReduced, bEnabled && IsEnabled());
}

void SPWCornerBrackets::Construct(const FArguments&)
{
    SetVisibility(EVisibility::HitTestInvisible);
    SetClipping(EWidgetClipping::ClipToBounds);
#if WITH_ACCESSIBILITY
    SetAccessibleBehavior(EAccessibleBehavior::NotAccessible);
#endif
}

int32 SPWCornerBrackets::OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
                                FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool) const
{
    Paths.Rebuild(FVector2f(Geometry.GetLocalSize()));
    return FPWSigilPainter::PaintBrackets(Paths, Geometry, Elements, Layer, Style);
}
