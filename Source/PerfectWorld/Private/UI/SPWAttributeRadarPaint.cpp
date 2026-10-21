#include "UI/SPWAttributeRadar.h"

#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Textures/SlateShaderResource.h"
#include "UI/PWCreationModel.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"

#define LOCTEXT_NAMESPACE "PerfectWorldAttributeRadar"

namespace
{
constexpr int32 PgDiscSides = 14;
constexpr float PgDashLength = 5.0f;
constexpr float PgGapLength = 4.0f;

FLinearColor PgTint(const FLinearColor& Colour, float Alpha, const FWidgetStyle& Style)
{
    FLinearColor Result = Colour;
    Result.A = FMath::Clamp(Colour.A * Alpha * Style.GetColorAndOpacityTint().A, 0.0f, 1.0f);
    return Result;
}

void PgLines(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, TArray<FVector2f> Points,
             const FLinearColor& Colour, float Width)
{
    FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), MoveTemp(Points), ESlateDrawEffect::None, Colour, true, Width);
}

void PgClosed(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, TArray<FVector2f> Poly,
              const FLinearColor& Colour, float Width)
{
    if (Poly.Num() < 2) return;
    Poly.Add(Poly[0]);
    PgLines(Elements, Layer, Geometry, MoveTemp(Poly), Colour, Width);
}

void PgDashed(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, const TArray<FVector2f>& Poly,
              const FLinearColor& Colour, float Width)
{
    for (int32 Index = 0; Index < Poly.Num(); ++Index)
    {
        const FVector2f A = Poly[Index], B = Poly[(Index + 1) % Poly.Num()];
        const float Length = FVector2f::Distance(A, B);
        if (Length < 1.0f) continue;
        const FVector2f Step = (B - A) / Length;
        for (float At = 0.0f; At < Length; At += PgDashLength + PgGapLength)
            PgLines(Elements, Layer, Geometry, {A + Step * At, A + Step * FMath::Min(Length, At + PgDashLength)}, Colour, Width);
    }
}

TArray<FVector2f> PgDisc(FVector2f Centre, float Radius)
{
    TArray<FVector2f> Ring;
    for (int32 Index = 0; Index < PgDiscSides; ++Index)
    {
        const float Angle = 2.0f * PI * static_cast<float>(Index) / static_cast<float>(PgDiscSides);
        Ring.Add(Centre + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
    }
    return Ring;
}

FVector2f PgMeasure(const FText& Text, const FSlateFontInfo& Font)
{
    if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer() != nullptr)
    {
        const auto Measured = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font);
        return FVector2f(Measured.X, Measured.Y);
    }
    return FVector2f(static_cast<float>(Text.ToString().Len() * Font.Size) * 0.55f, static_cast<float>(Font.Size) * 1.3f);
}

/** Draws text with its middle at Middle; returns its height. */
float PgCentredText(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, FVector2f Middle,
                    const FText& Text, const FSlateFontInfo& Font, const FLinearColor& Colour)
{
    const FVector2f Size = PgMeasure(Text, Font);
    FSlateDrawElement::MakeText(Elements, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Middle - Size * 0.5f)), Text, Font,
                                ESlateDrawEffect::None, Colour);
    return Size.Y;
}
} // namespace

void SPWAttributeRadar::PaintFill(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry,
                                  const TArray<FVector2f>& Poly, FVector2f Centre, const FLinearColor& EdgeColour,
                                  const FLinearColor& CentreColour) const
{
    if (Poly.Num() < 3) return;
    // A triangle fan from the middle, sampled from the white texel of the core style so only the vertex colours show.
    const FSlateBrush* White = FCoreStyle::Get().GetBrush("GenericWhiteBox");
    if (White == nullptr) return;
    const FSlateResourceHandle Handle = White->GetRenderingResource();
    const FSlateShaderResourceProxy* Proxy = Handle.GetResourceProxy();
    if (Proxy == nullptr) return;
    const FVector2f UV = Proxy->StartUV + Proxy->SizeUV * 0.5f;
    const FSlateRenderTransform& Transform = Geometry.GetAccumulatedRenderTransform();
    TArray<FSlateVertex> Vertices;
    Vertices.Reserve(Poly.Num() + 1);
    Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, Centre, UV, CentreColour.ToFColor(true)));
    for (const FVector2f& Point : Poly)
        Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, Point, UV, EdgeColour.ToFColor(true)));
    TArray<SlateIndex> Indices;
    for (int32 Index = 1; Index <= Poly.Num(); ++Index)
    {
        Indices.Add(0);
        Indices.Add(static_cast<SlateIndex>(Index));
        Indices.Add(static_cast<SlateIndex>(Index == Poly.Num() ? 1 : Index + 1));
    }
    FSlateDrawElement::MakeCustomVerts(Elements, Layer, Handle, Vertices, Indices, nullptr, 0, 0);
}

void SPWAttributeRadar::PaintLabels(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, FVector2f Centre,
                                    const PWRadar::FPlan& Plan, float Scale, const FWidgetStyle& Style) const
{
    const PWUI::FStyle& Look = PWUI::FStyle::Get();
    const int32 Count = Current.Axes.Num();
    const FSlateFontInfo NameFont = PWUI::Font(Plan.Metrics.NameFont, false);
    const FSlateFontInfo ValueFont = PWUI::Font(Plan.Metrics.ValueFont, true);
    FNumberFormattingOptions Signed;
    Signed.AlwaysSign = true;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const PWRadar::FAxis& Axis = Current.Axes[Index];
        const PWRadar::FRect Box = Plan.LabelBox(Index, Count);
        const FVector2f Middle = Centre + ((Box.Min + Box.Max) * 0.5f - Plan.Centre()) * Scale;
        const PWRadar::EAxisSignal Signal = Motion.Signal(Index);
        const float Strength = Motion.SignalStrength(Index);
        const FLinearColor Tone = Signal == PWRadar::EAxisSignal::Fell ? Look.Warn : Look.Accent;
        const FLinearColor NameColour = PgTint(FMath::Lerp(Look.TextSecondary, Tone, Strength), 1.0f, Style);
        const FLinearColor ValueColour = PgTint(FMath::Lerp(Look.TextPrimary, Tone, Strength), 1.0f, Style);
        FText Value = FText::AsNumber(Axis.Points);
        // The change is spelled out as well as tinted, so the cue never depends on colour alone.
        if (Signal != PWRadar::EAxisSignal::None)
            Value = FText::Format(LOCTEXT("ValueWithChange", "{0}  {1}"), Value, FText::AsNumber(Motion.SignalDelta(Index), &Signed));
        const float NameHeight = Plan.Metrics.NameFont * 1.25f;
        const float ValueHeight = Plan.Metrics.ValueFont * 1.25f;
        const float Top = Middle.Y - (NameHeight + ValueHeight) * 0.5f;
        PgCentredText(Elements, Layer, Geometry, FVector2f(Middle.X, Top + NameHeight * 0.5f), Axis.Label, NameFont, NameColour);
        PgCentredText(Elements, Layer, Geometry, FVector2f(Middle.X, Top + NameHeight + ValueHeight * 0.5f), Value, ValueFont, ValueColour);
    }
}

int32 SPWAttributeRadar::OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect& CullingRect,
                                 FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool) const
{
    const int32 Count = Current.Axes.Num();
    if (Count < 3 || !FSlateRect::DoRectanglesIntersect(Geometry.GetRenderBoundingRect(), CullingRect)) return Layer;
    const PWRadar::FPlan Plan = GetPlan();
    const FVector2f Local(Geometry.GetLocalSize());
    if (Plan.Size.X <= 0.0f || Plan.Size.Y <= 0.0f || Local.X <= 0.0f || Local.Y <= 0.0f) return Layer;
    // A parent that gives less room than the chart asked for scales the whole drawing down to fit.
    const float Scale = FMath::Min(1.0f, FMath::Min(Local.X / Plan.Size.X, Local.Y / Plan.Size.Y));
    const float Radius = Plan.Radius * Scale;
    const FVector2f Centre = Local * 0.5f;
    const PWUI::FStyle& Look = PWUI::FStyle::Get();
    const bool bReduced = PWUI::IsReducedMotion();

    // Guides: concentric rings at quarter steps of each domain's cap, and one spoke per domain.
    for (const float Ring : {0.25f, 0.5f, 0.75f, 1.0f})
    {
        TArray<float> Level;
        Level.Init(Ring, Count);
        PgClosed(Elements, Layer, Geometry, PWRadar::Polygon(Centre, Radius, Level),
                 PgTint(Ring >= 1.0f ? FMath::Lerp(Look.Divider, Look.TextSecondary, 0.45f) : Look.Divider, 1.0f, Style),
                 Ring >= 1.0f ? 1.5f : 1.0f);
    }
    for (int32 Index = 0; Index < Count; ++Index)
        PgLines(Elements, Layer, Geometry, {Centre, PWRadar::Vertex(Centre, Radius, Index, Count, 1.0f)}, PgTint(Look.Divider, 1.0f, Style), 1.0f);

    // The allocation when the player last chose a starting background: a faint outline to compare against.
    TArray<float> ReferenceFractions = Reference.Fractions();
    if (ReferenceFractions.Num() == Count)
    {
        const TArray<FVector2f> Poly = PWRadar::Polygon(Centre, Radius, ReferenceFractions);
        PaintFill(Elements, Layer + 1, Geometry, Poly, Centre, PgTint(Look.TextSecondary, 0.10f, Style), PgTint(Look.TextSecondary, 0.04f, Style));
        PgClosed(Elements, Layer + 2, Geometry, Poly, PgTint(Look.TextSecondary, 0.55f, Style), 1.0f);
    }

    // The live allocation, at the morph's current shape.
    const TArray<FVector2f> Shape = PWRadar::Polygon(Centre, Radius, Motion.DisplayedAll());
    PaintFill(Elements, Layer + 1, Geometry, Shape, Centre, PgTint(Look.Accent, 0.38f, Style), PgTint(Look.Accent, 0.12f, Style));
    PgClosed(Elements, Layer + 3, Geometry, Shape, PgTint(Look.Accent, 1.0f, Style), 2.5f);

    // A proposal the player has not applied yet, as a dashed outline on the one domain it touches.
    const FPWRadarPreview Trial = Preview.Get(FPWRadarPreview());
    int32 TrialAxis = INDEX_NONE;
    if (Trial.Delta != 0)
        for (int32 Index = 0; Index < Count; ++Index)
            if (Current.Axes[Index].Id == Trial.Group) TrialAxis = Index;
    if (TrialAxis != INDEX_NONE)
    {
        TArray<float> Trying = Motion.DisplayedAll();
        const PWRadar::FAxis& Axis = Current.Axes[TrialAxis];
        Trying[TrialAxis] = PWRadar::Fraction(Axis.Points + Trial.Delta, Axis.Cap);
        PgDashed(Elements, Layer + 3, Geometry, PWRadar::Polygon(Centre, Radius, Trying),
                 PgTint(Trial.Delta > 0 ? Look.Accent : Look.Warn, 1.0f, Style), 1.5f);
    }

    // Highlights: a heavier spoke and a larger node on the axes that just rose (accent) or fell (warning).
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const PWRadar::EAxisSignal Signal = Motion.Signal(Index);
        const FVector2f Node = Shape[Index];
        if (Signal != PWRadar::EAxisSignal::None)
        {
            const float Strength = Motion.SignalStrength(Index);
            const FLinearColor Tone = PgTint(Signal == PWRadar::EAxisSignal::Fell ? Look.Warn : Look.Accent, 0.35f + 0.65f * Strength, Style);
            PgLines(Elements, Layer + 3, Geometry, {Centre, PWRadar::Vertex(Centre, Radius, Index, Count, 1.0f)}, Tone, 3.0f);
            const TArray<FVector2f> Dot = PgDisc(Node, 6.0f * Scale + 1.0f);
            PaintFill(Elements, Layer + 4, Geometry, Dot, Node, Tone, Tone);
        }
        else
        {
            const TArray<FVector2f> Dot = PgDisc(Node, 3.0f);
            PaintFill(Elements, Layer + 4, Geometry, Dot, Node, PgTint(Look.Accent, 1.0f, Style), PgTint(Look.Accent, 1.0f, Style));
        }
    }

    // The points left, in the middle; it shakes and flashes when the core refuses an edit.
    const float Flash = Motion.RejectFlash();
    const float Shake = bReduced ? 0.0f : Motion.ShakeOffset();
    const FVector2f Heart = Centre + FVector2f(Shake, 0.0f);
    const float DiscRadius = FMath::Max(22.0f, Radius * 0.30f);
    const TArray<FVector2f> Disc = PgDisc(Heart, DiscRadius);
    PaintFill(Elements, Layer + 5, Geometry, Disc, Heart, PgTint(Look.Panel, 0.72f, Style), PgTint(Look.Panel, 0.72f, Style));
    PgClosed(Elements, Layer + 6, Geometry, Disc, PgTint(FMath::Lerp(Look.Divider, Look.Danger, Flash), 1.0f, Style), 1.0f + Flash);
    const int32 Remaining = Current.Remaining;
    const FLinearColor Number = FMath::Lerp(Remaining == 0 ? Look.Good : Look.TextPrimary, Look.Danger, Flash);
    const int32 NumberSize = Plan.bCompact ? 18 : 22;
    const int32 CaptionSize = Plan.bCompact ? 10 : 11;
    FText Caption = Plan.bCompact ? LOCTEXT("LeftShort", "left") : LOCTEXT("Left", "points left");
    if (TrialAxis != INDEX_NONE)
        Caption = FText::Format(LOCTEXT("LeftAfter", "{0} after"), FText::AsNumber(Remaining - Trial.Delta));
    const float NumberHeight = NumberSize * 1.25f, CaptionHeight = CaptionSize * 1.25f;
    const float Top = Heart.Y - (NumberHeight + CaptionHeight) * 0.5f;
    PgCentredText(Elements, Layer + 7, Geometry, FVector2f(Heart.X, Top + NumberHeight * 0.5f), FText::AsNumber(Remaining),
                  PWUI::Font(NumberSize, true), PgTint(Number, 1.0f, Style));
    PgCentredText(Elements, Layer + 7, Geometry, FVector2f(Heart.X, Top + NumberHeight + CaptionHeight * 0.5f), Caption,
                  PWUI::Font(CaptionSize, false), PgTint(Look.TextSecondary, 1.0f, Style));

    PaintLabels(Elements, Layer + 7, Geometry, Centre, Plan, Scale, Style);
    return Layer + 8;
}

#undef LOCTEXT_NAMESPACE
