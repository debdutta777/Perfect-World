#pragma once

#include "CoreMinimal.h"
#include "UI/PWAttributeRadarModel.h"
#include "UI/PWAttributeRadarMotion.h"
#include "Widgets/SLeafWidget.h"

class FPWCreationModel;

/** A trial change the player is looking at but has not applied: one domain and the signed points it would add. */
struct FPWRadarPreview
{
    FName Group;
    int32 Delta = 0;
};

/**
 * The allocation radar: one axis per trait domain the core publishes (seven, so a heptagon), the live allocation as
 * a filled polygon, a faint polygon for the allocation when the player last chose a starting background, and the
 * points left in the middle. It is display only: it never takes focus or input and holds no rules. Every value comes
 * from the creation model, which reads the core, and is scaled by the core's own cost table (see PWAttributeRadarModel.h).
 *
 * When an edit changes a domain the polygon morphs there in about 200 ms (at once under reduced motion), the axis that
 * rose is tinted with the accent and one that fell with the warning tone and each shows its signed change. When the core
 * refuses an edit the points left shake and flash.
 */
class SPWAttributeRadar final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SPWAttributeRadar) : _AvailableWidth(520.0f), _MaxRadius(150.0f), _Compact(false) {}
        /** The widest the chart may be, labels included. The page binds this to the room it has. */
        SLATE_ATTRIBUTE(float, AvailableWidth)
        SLATE_ATTRIBUTE(float, MaxRadius)
        SLATE_ATTRIBUTE(bool, Compact)
        /** A proposed change to draw as an outline before it is applied. */
        SLATE_ATTRIBUTE(FPWRadarPreview, Preview)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args, const TSharedRef<FPWCreationModel>& InModel);

    virtual void Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime) override;
    virtual FVector2D ComputeDesiredSize(float) const override;
    virtual bool SupportsKeyboardFocus() const override { return false; }
    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
                          FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override;

    /** Reads the model and starts a morph, highlight or shake if anything changed. Tick calls it; tests call it directly. */
    void Sync();
    /** Steps the animation clock. Tick calls it; tests call it with exact steps. */
    void Advance(float DeltaSeconds);
    /** Makes the faint reference polygon the current allocation, as when the player chooses a starting background. */
    void CaptureReference();

    const PWRadar::FSnapshot& GetSnapshot() const { return Current; }
    const PWRadar::FSnapshot& GetReference() const { return Reference; }
    const PWRadar::FMotion& GetMotion() const { return Motion; }
    PWRadar::FPlan GetPlan() const;
    /** Sideways shake for the budget (and the radar's own centre); 0 under reduced motion. */
    float GetShakeOffset() const { return Motion.ShakeOffset(); }
    /** The sentence a screen reader gets: each domain's points against its cap, and the points left. */
    FText GetSummaryText() const;

private:
    void PaintFill(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, const TArray<FVector2f>& Poly,
                   FVector2f Centre, const FLinearColor& EdgeColour, const FLinearColor& CentreColour) const;
    void PaintLabels(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, FVector2f Centre,
                     const PWRadar::FPlan& Plan, float Scale, const FWidgetStyle& Style) const;

    TSharedPtr<FPWCreationModel> Model;
    TAttribute<float> AvailableWidth, MaxRadius;
    TAttribute<bool> Compact;
    TAttribute<FPWRadarPreview> Preview;
    PWRadar::FSnapshot Current, Reference;
    PWRadar::FMotion Motion;
    int32 SeenRejection = 0;
    FPWRadarPreview LastPreview;
    FVector2f LastPlannedSize = FVector2f::ZeroVector;
    double LastTickTime = 0.0;
    bool bHasTicked = false;
};
