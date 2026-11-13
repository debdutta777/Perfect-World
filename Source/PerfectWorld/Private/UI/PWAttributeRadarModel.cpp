#include "UI/PWAttributeRadarModel.h"

#include "CharacterCreation.h"
#include "CreationPresentation.h"
#include "UI/PWCreationModel.h"

namespace PWRadar
{
namespace
{
/** Labels push outward along an axis; the factor makes the side axes clear the vertex and the top and bottom ones centre. */
constexpr float PgPushFactor = 1.6f;
constexpr float PgDisplayExponent = 0.6f;
constexpr int32 PgBisectSteps = 24;

FVector2f PgLabelOffset(const FPlan& Plan, int32 Index, int32 Count)
{
    const FVector2f Dir = Direction(Index, Count);
    const FVector2f Anchor = Dir * (Plan.Radius + Plan.Metrics.Gap);
    const float PushX = FMath::Clamp(Dir.X * PgPushFactor, -1.0f, 1.0f);
    const float PushY = FMath::Clamp(Dir.Y * PgPushFactor, -1.0f, 1.0f);
    return Anchor + FVector2f(PushX * Plan.Metrics.Width * 0.5f, PushY * Plan.Metrics.Height * 0.5f);
}
} // namespace

bool FSnapshot::SameValues(const FSnapshot& Other) const
{
    if (Remaining != Other.Remaining || Budget != Other.Budget || Axes.Num() != Other.Axes.Num()) return false;
    for (int32 Index = 0; Index < Axes.Num(); ++Index)
    {
        const FAxis& A = Axes[Index];
        const FAxis& B = Other.Axes[Index];
        if (A.Id != B.Id || A.Points != B.Points || A.Cap != B.Cap) return false;
    }
    return true;
}

TArray<float> FSnapshot::Fractions() const
{
    TArray<float> Result;
    for (const FAxis& Axis : Axes) Result.Add(Fraction(Axis.Points, Axis.Cap));
    return Result;
}

TArray<int32> FSnapshot::Points() const
{
    TArray<int32> Result;
    for (const FAxis& Axis : Axes) Result.Add(Axis.Points);
    return Result;
}

FText ShortLabel(FName GroupId, const FText& CoreName)
{
    static const TMap<FName, FText> Labels = {
        {FName(TEXT("appearance_presentation")), NSLOCTEXT("PerfectWorldAttributeRadar", "Appearance", "Appearance")},
        {FName(TEXT("family_resources_support")), NSLOCTEXT("PerfectWorldAttributeRadar", "Family", "Family")},
        {FName(TEXT("physical")), NSLOCTEXT("PerfectWorldAttributeRadar", "Physical", "Physical")},
        {FName(TEXT("cognition")), NSLOCTEXT("PerfectWorldAttributeRadar", "Cognition", "Cognition")},
        {FName(TEXT("talents")), NSLOCTEXT("PerfectWorldAttributeRadar", "Talents", "Talents")},
        {FName(TEXT("social")), NSLOCTEXT("PerfectWorldAttributeRadar", "Social", "Social")},
        {FName(TEXT("prior_experiences")), NSLOCTEXT("PerfectWorldAttributeRadar", "Experience", "Experience")}};
    const FText* Found = Labels.Find(GroupId);
    return Found != nullptr ? *Found : CoreName;
}

int32 AxisCap(int32 SumOfTraitMaxCosts, int32 Budget)
{
    return FMath::Max(0, FMath::Min(SumOfTraitMaxCosts, Budget));
}

FSnapshot BuildSnapshot(const TArray<FPWTraitGroupView>& Groups, const TArray<FPWTraitView>& Traits,
                        TFunctionRef<int32(FName)> TraitMaxCost, int32 Budget, int32 Remaining)
{
    FSnapshot Result;
    Result.Budget = Budget;
    Result.Remaining = Remaining;
    for (const FPWTraitGroupView& Group : Groups)
    {
        FAxis& Axis = Result.Axes.AddDefaulted_GetRef();
        Axis.Id = Group.Id;
        Axis.Label = ShortLabel(Group.Id, Group.DisplayName);
        Axis.FullName = Group.DisplayName;
        Axis.Points = Group.AllocatedPoints;
        int32 Sum = 0;
        for (const FPWTraitView& Trait : Traits)
            if (Trait.Group == Group.Id) Sum += TraitMaxCost(Trait.Id);
        Axis.Cap = AxisCap(Sum, Budget);
    }
    return Result;
}

FSnapshot ReadSnapshot(const FPWCreationModel& Model)
{
    // The cost table is the core's; every trait tops out at the same rank, so one lookup serves all of them.
    const int32 TopRankCost = FMath::Max(0, PerfectWorld::CumulativeCostAtRank(PerfectWorld::MaxRank));
    return BuildSnapshot(Model.TraitGroups, Model.Traits, [TopRankCost](FName) { return TopRankCost; },
                         Model.Summary.TotalBudget, Model.Summary.Remaining);
}

float Fraction(int32 Points, int32 Cap)
{
    return Cap > 0 ? FMath::Clamp(static_cast<float>(Points) / static_cast<float>(Cap), 0.0f, 1.0f) : 0.0f;
}

float DisplayRadius(float InFraction)
{
    return FMath::Pow(FMath::Clamp(InFraction, 0.0f, 1.0f), PgDisplayExponent);
}

FVector2f Direction(int32 Index, int32 Count)
{
    if (Count <= 0) return FVector2f::ZeroVector;
    const float Angle = 2.0f * PI * static_cast<float>(Index) / static_cast<float>(Count);
    return FVector2f(FMath::Sin(Angle), -FMath::Cos(Angle));
}

FVector2f Vertex(FVector2f Centre, float Radius, int32 Index, int32 Count, float InFraction)
{
    return Centre + Direction(Index, Count) * (Radius * DisplayRadius(InFraction));
}

TArray<FVector2f> Polygon(FVector2f Centre, float Radius, const TArray<float>& Fractions)
{
    TArray<FVector2f> Result;
    for (int32 Index = 0; Index < Fractions.Num(); ++Index)
        Result.Add(Vertex(Centre, Radius, Index, Fractions.Num(), Fractions[Index]));
    return Result;
}

float EaseOut(float T)
{
    const float Clamped = FMath::Clamp(T, 0.0f, 1.0f);
    const float Inverse = 1.0f - Clamped;
    return 1.0f - Inverse * Inverse * Inverse;
}

FLabelMetrics MetricsFor(bool bCompact)
{
    FLabelMetrics Metrics;
    if (bCompact)
    {
        Metrics.Width = 76.0f;
        Metrics.Height = 32.0f;
        Metrics.Gap = 5.0f;
        Metrics.NameFont = 12;
        Metrics.ValueFont = 13;
    }
    return Metrics;
}

bool FRect::IsInside(FVector2f Size, float Tolerance) const
{
    return Min.X >= -Tolerance && Min.Y >= -Tolerance && Max.X <= Size.X + Tolerance && Max.Y <= Size.Y + Tolerance;
}

FRect FPlan::LabelBox(int32 Index, int32 Count) const
{
    const FVector2f Centre2 = Centre() + PgLabelOffset(*this, Index, Count);
    const FVector2f Half(Metrics.Width * 0.5f, Metrics.Height * 0.5f);
    FRect Box;
    Box.Min = Centre2 - Half;
    Box.Max = Centre2 + Half;
    return Box;
}

FPlan PlanForRadius(float Radius, int32 AxisCount, bool bCompact)
{
    FPlan Plan;
    Plan.Radius = FMath::Max(0.0f, Radius);
    Plan.bCompact = bCompact;
    Plan.Metrics = MetricsFor(bCompact);
    // The labels decide the extent: measure them around the centre, then make the widget symmetric about it.
    float ExtentX = Plan.Radius, ExtentY = Plan.Radius;
    for (int32 Index = 0; Index < AxisCount; ++Index)
    {
        const FVector2f Offset = PgLabelOffset(Plan, Index, AxisCount);
        ExtentX = FMath::Max(ExtentX, FMath::Abs(Offset.X) + Plan.Metrics.Width * 0.5f);
        ExtentY = FMath::Max(ExtentY, FMath::Abs(Offset.Y) + Plan.Metrics.Height * 0.5f);
    }
    Plan.Size = FVector2f(2.0f * ExtentX, 2.0f * ExtentY);
    return Plan;
}

FPlan PlanForWidth(float AvailableWidth, float MaxRadius, int32 AxisCount, bool bCompact)
{
    FPlan Best = PlanForRadius(0.0f, AxisCount, bCompact);
    if (Best.Size.X > AvailableWidth) return Best;
    float Low = 0.0f, High = FMath::Max(0.0f, MaxRadius);
    if (PlanForRadius(High, AxisCount, bCompact).Size.X <= AvailableWidth) return PlanForRadius(High, AxisCount, bCompact);
    for (int32 Step = 0; Step < PgBisectSteps; ++Step)
    {
        const float Mid = (Low + High) * 0.5f;
        if (PlanForRadius(Mid, AxisCount, bCompact).Size.X <= AvailableWidth) Low = Mid;
        else High = Mid;
    }
    return PlanForRadius(Low, AxisCount, bCompact);
}

FPageSlot PageSlot(float PageWidth, bool bNarrow)
{
    const float Width = FMath::Max(0.0f, PageWidth);
    FPageSlot Slot;
    Slot.bCompact = bNarrow;
    // Wide: the chart has its own left column, 30% of the page less the 24-unit gutter. Narrow: a row of the full width.
    Slot.AvailableWidth = bNarrow ? Width : FMath::Max(0.0f, Width * 0.30f - 24.0f);
    Slot.MaxRadius = bNarrow ? 84.0f : 150.0f;
    return Slot;
}

FPlan PlanForPage(float PageWidth, bool bNarrow, int32 AxisCount)
{
    const FPageSlot Slot = PageSlot(PageWidth, bNarrow);
    return PlanForWidth(Slot.AvailableWidth, Slot.MaxRadius, AxisCount, Slot.bCompact);
}
} // namespace PWRadar
