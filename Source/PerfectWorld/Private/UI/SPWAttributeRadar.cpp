#include "UI/SPWAttributeRadar.h"

#include "UI/PWCreationModel.h"
#include "UI/PWWidgets.h"

#define LOCTEXT_NAMESPACE "PerfectWorldAttributeRadar"

void SPWAttributeRadar::Construct(const FArguments& Args, const TSharedRef<FPWCreationModel>& InModel)
{
    Model = InModel;
    AvailableWidth = Args._AvailableWidth;
    MaxRadius = Args._MaxRadius;
    Compact = Args._Compact;
    Preview = Args._Preview;
    SetVisibility(EVisibility::HitTestInvisible);
    // Display only: no hit, no focus; the allocation controls keep the focus. Screen readers get one summary.
#if WITH_ACCESSIBILITY
    SetAccessibleBehavior(EAccessibleBehavior::Custom, TAttribute<FText>::CreateSP(this, &SPWAttributeRadar::GetSummaryText));
#endif
    Current = PWRadar::ReadSnapshot(*Model);
    Reference = Current;
    Motion.Snap(Current.Fractions(), Current.Points());
    SeenRejection = Model->RejectionSerial;
}

PWRadar::FPlan SPWAttributeRadar::GetPlan() const
{
    return PWRadar::PlanForWidth(AvailableWidth.Get(520.0f), MaxRadius.Get(150.0f), Current.Axes.Num(), Compact.Get(false));
}

FVector2D SPWAttributeRadar::ComputeDesiredSize(float) const
{
    const FVector2f Size = GetPlan().Size;
    return FVector2D(Size.X, Size.Y);
}

void SPWAttributeRadar::CaptureReference()
{
    Reference = Current;
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SPWAttributeRadar::Sync()
{
    if (!Model.IsValid()) return;
    const bool bReduced = PWUI::IsReducedMotion();
    if (bReduced) Motion.SettleForReducedMotion();
    const PWRadar::FSnapshot Fresh = PWRadar::ReadSnapshot(*Model);
    if (!Fresh.SameValues(Current))
    {
        const bool bWasEmpty = Current.Axes.IsEmpty();
        Current = Fresh;
        if (bWasEmpty)
        {
            Reference = Fresh;
            Motion.Snap(Fresh.Fractions(), Fresh.Points());
        }
        else
        {
            Motion.Retarget(Fresh.Fractions(), Fresh.Points(), bReduced);
        }
        Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
    }
    if (Model->RejectionSerial != SeenRejection)
    {
        SeenRejection = Model->RejectionSerial;
        Motion.Reject(bReduced);
    }
}

void SPWAttributeRadar::Advance(float DeltaSeconds)
{
    Motion.Advance(DeltaSeconds);
    if (Motion.IsActive()) Invalidate(EInvalidateWidgetReason::Paint);
}

void SPWAttributeRadar::Tick(const FGeometry&, double CurrentTime, float DeltaTime)
{
    // A page that was hidden is not ticked. After such a gap, show what is true now without replaying old news.
    const bool bResumed = !bHasTicked || (CurrentTime - LastTickTime) > 0.25;
    bHasTicked = true;
    LastTickTime = CurrentTime;
    if (bResumed && Model.IsValid())
    {
        Current = PWRadar::ReadSnapshot(*Model);
        Motion.Snap(Current.Fractions(), Current.Points());
        SeenRejection = Model->RejectionSerial;
        Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
    }
    else
    {
        Sync();
    }
    const FVector2f Planned = GetPlan().Size;
    if (!Planned.Equals(LastPlannedSize, 0.5f))
    {
        LastPlannedSize = Planned;
        Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
    }
    const FPWRadarPreview Trial = Preview.Get(FPWRadarPreview());
    if (Trial.Group != LastPreview.Group || Trial.Delta != LastPreview.Delta)
    {
        LastPreview = Trial;
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    Advance(DeltaTime);
}

FText SPWAttributeRadar::GetSummaryText() const
{
    FString Parts;
    for (const PWRadar::FAxis& Axis : Current.Axes)
    {
        if (!Parts.IsEmpty()) Parts += TEXT("; ");
        Parts += FText::Format(LOCTEXT("SummaryAxis", "{0}: {1} of {2} points"), Axis.FullName, FText::AsNumber(Axis.Points),
                               FText::AsNumber(Axis.Cap)).ToString();
    }
    return FText::Format(LOCTEXT("Summary", "Points by trait domain. {0}. {1} points remaining."), FText::FromString(Parts),
                         FText::AsNumber(Current.Remaining));
}

#undef LOCTEXT_NAMESPACE
