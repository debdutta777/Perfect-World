#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldCreationViews.h"

class FPWCreationModel;

/**
 * Pure data and geometry for the allocation radar. Nothing here knows about Slate, so every rule the chart
 * relies on is testable without a window. The chart has one axis for each trait domain the core publishes
 * (the core has seven, so the polygon is a heptagon). Values are the core's own per-domain point totals and
 * each axis is scaled by a cap taken from the core's own rank cost table. The UI invents no rule.
 */
namespace PWRadar
{
/** One trait domain as the chart shows it. */
struct FAxis
{
    FName Id;
    /** The short chart label; the core's full domain name is kept for tooltips and the spoken summary. */
    FText Label;
    FText FullName;
    /** The core's allocated points for the domain. */
    int32 Points = 0;
    /** The most points the domain can hold by the core's cost table, never above the whole budget. */
    int32 Cap = 0;
};

struct FSnapshot
{
    TArray<FAxis> Axes;
    int32 Remaining = 0;
    int32 Budget = 0;

    /** Same domains, points, caps and remaining points; labels are ignored. */
    bool SameValues(const FSnapshot& Other) const;
    TArray<float> Fractions() const;
    TArray<int32> Points() const;
};

/** The chart's short name for a domain, falling back to the core's own name for one it does not know. */
FText ShortLabel(FName GroupId, const FText& CoreName);

/** The cap of a domain: the sum of its traits' top-rank cumulative costs, clamped to the budget. */
int32 AxisCap(int32 SumOfTraitMaxCosts, int32 Budget);

/**
 * Builds the chart's values. Group totals come from the core's group views; TraitMaxCost returns the core's
 * cumulative cost at the top rank for one trait id. Axes keep the core's display order.
 */
FSnapshot BuildSnapshot(const TArray<FPWTraitGroupView>& Groups, const TArray<FPWTraitView>& Traits,
                        TFunctionRef<int32(FName)> TraitMaxCost, int32 Budget, int32 Remaining);

/** Reads the live creator model. Needs the model's subsystem for the cost table; empty when it is gone. */
FSnapshot ReadSnapshot(const FPWCreationModel& Model);

/** Points over cap, 0 to 1; 0 when the cap is not positive. */
float Fraction(int32 Points, int32 Cap);

/**
 * How far out an axis value is drawn. A gentle curve (power 0.6) so that one rank step, which is a few percent of
 * a domain's cap, still moves the polygon visibly. It is monotonic, 0 at 0 and 1 at 1, and display only.
 */
float DisplayRadius(float Fraction);

/** Unit direction of axis Index of Count: the first axis points up and the rest follow clockwise. */
FVector2f Direction(int32 Index, int32 Count);

/** The polygon vertex of an axis at a value fraction, around Centre with the outer ring at Radius. */
FVector2f Vertex(FVector2f Centre, float Radius, int32 Index, int32 Count, float Fraction);

TArray<FVector2f> Polygon(FVector2f Centre, float Radius, const TArray<float>& Fractions);

/** Cubic ease-out, 0 to 1. */
float EaseOut(float T);

/** What the label text and its box look like at one density. */
struct FLabelMetrics
{
    float Width = 88.0f;
    float Height = 36.0f;
    float Gap = 6.0f;
    int32 NameFont = 13;
    int32 ValueFont = 15;
};

FLabelMetrics MetricsFor(bool bCompact);

struct FRect
{
    FVector2f Min = FVector2f::ZeroVector;
    FVector2f Max = FVector2f::ZeroVector;
    bool IsInside(FVector2f Size, float Tolerance = 0.01f) const;
};

/** Where the chart sits inside its widget. Size is the widget's desired size; the centre is its middle. */
struct FPlan
{
    float Radius = 0.0f;
    FVector2f Size = FVector2f::ZeroVector;
    bool bCompact = false;
    FLabelMetrics Metrics;

    FVector2f Centre() const { return Size * 0.5f; }
    /** The label box of an axis, in the widget's own space. Boxes sit just outside the outer ring. */
    FRect LabelBox(int32 Index, int32 Count) const;
};

/** The size a chart of Radius needs, labels included. */
FPlan PlanForRadius(float Radius, int32 AxisCount, bool bCompact);

/** The largest chart not wider than AvailableWidth and not larger than MaxRadius. */
FPlan PlanForWidth(float AvailableWidth, float MaxRadius, int32 AxisCount, bool bCompact);

/** What the traits page gives the chart: a column of the page when wide, the page's whole width as a row when narrow. */
struct FPageSlot
{
    float AvailableWidth = 0.0f;
    float MaxRadius = 0.0f;
    bool bCompact = false;
};

FPageSlot PageSlot(float PageWidth, bool bNarrow);

FPlan PlanForPage(float PageWidth, bool bNarrow, int32 AxisCount);
} // namespace PWRadar
