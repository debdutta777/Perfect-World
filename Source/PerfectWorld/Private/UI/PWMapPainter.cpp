#include "UI/PWMapPainter.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "UI/PWStyle.h"

namespace
{
constexpr float MpHalfPi = 1.57079632679f;
constexpr float MpEdgeThickness = 1.0f;

FLinearColor MpBlend(const FLinearColor& From, const FLinearColor& To, float Amount)
{
    return From + (To - From) * Amount;
}

/** One solid rectangle, turned about its centre. The brush is a function-local static so the element never points at a dead object. */
void MpBox(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, const FVector2f& Centre, const FVector2f& Size, float AngleRadians,
           const FLinearColor& Colour)
{
    static const FSlateColorBrush Solid(FLinearColor::White);
    if (Size.X <= 0.0f || Size.Y <= 0.0f || Colour.A <= 0.0f)
    {
        return;
    }
    FSlateDrawElement::MakeRotatedBox(Elements, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Centre - Size * 0.5f)), &Solid,
                                      ESlateDrawEffect::None, AngleRadians, TOptional<FVector2f>(), FSlateDrawElement::RelativeToElement, Colour);
}

/** A filled disc of the given diameter. */
void MpDisc(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, const FVector2f& Centre, float Diameter, const FLinearColor& Colour)
{
    if (Diameter <= 0.0f || Colour.A <= 0.0f)
    {
        return;
    }
    const FSlateRoundedBoxBrush Disc(FLinearColor::White, Diameter * 0.5f);
    FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(FVector2f(Diameter, Diameter), FSlateLayoutTransform(Centre - FVector2f(Diameter, Diameter) * 0.5f)),
                               &Disc, ESlateDrawEffect::None, Colour);
}

/** A circle outline of the given diameter and stroke. */
void MpRing(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, const FVector2f& Centre, float Diameter, float Stroke,
            const FLinearColor& Colour)
{
    if (Diameter <= 0.0f || Colour.A <= 0.0f)
    {
        return;
    }
    const FSlateRoundedBoxBrush Ring(FLinearColor::Transparent, Diameter * 0.5f, Colour, Stroke);
    FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(FVector2f(Diameter, Diameter), FSlateLayoutTransform(Centre - FVector2f(Diameter, Diameter) * 0.5f)),
                               &Ring, ESlateDrawEffect::None, FLinearColor::White);
}

void MpLine(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, TArray<FVector2f> Points, const FLinearColor& Colour, float Thickness)
{
    if (Points.Num() < 2 || Colour.A <= 0.0f)
    {
        return;
    }
    FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), MoveTemp(Points), ESlateDrawEffect::None, Colour, true, Thickness);
}

FVector2f MpRotate(const FVector2f& Vector, float Radians)
{
    const float C = FMath::Cos(Radians);
    const float S = FMath::Sin(Radians);
    return FVector2f(Vector.X * C - Vector.Y * S, Vector.X * S + Vector.Y * C);
}
} // namespace

namespace PWMap
{
const FMapPalette& GetPalette()
{
    static const FMapPalette Palette = []()
    {
        const PWUI::FStyle& Style = PWUI::FStyle::Get();
        FMapPalette P;
        P.Road = MpBlend(Style.Background, Style.Raised, 0.85f);
        P.Alley = P.Road;
        P.Footpath = MpBlend(Style.Panel, Style.Divider, 0.55f);
        P.Home = MpBlend(Style.Panel, Style.Success, 0.55f);
        P.Shop = MpBlend(Style.Panel, Style.Accent, 0.62f);
        P.School = MpBlend(Style.Panel, Style.Focus, 0.55f);
        P.Workshop = MpBlend(MpBlend(Style.Error, Style.Accent, 0.4f), Style.Panel, 0.4f);
        P.Sealed = MpBlend(Style.Panel, Style.Divider, 0.35f);
        P.BuildingEdge = Style.Background.CopyWithNewOpacity(0.85f);
        P.DoorOpen = Style.Success;
        P.DoorClosed = Style.Error;
        P.DoorPrivate = Style.Accent;
        P.DoorLocked = Style.TextSecondary;
        P.Person = Style.TextPrimary;
        P.Player = Style.Focus;
        P.Halo = Style.Background.CopyWithNewOpacity(0.9f);
        P.Label = Style.TextPrimary;
        P.LabelShadow = Style.Background.CopyWithNewOpacity(0.9f);
        return P;
    }();
    return Palette;
}

FLinearColor QuadColour(EQuadRole Role)
{
    const FMapPalette& P = GetPalette();
    switch (Role)
    {
    case EQuadRole::Road: return P.Road;
    case EQuadRole::Alley: return P.Alley;
    case EQuadRole::Footpath: return P.Footpath;
    case EQuadRole::Home: return P.Home;
    case EQuadRole::Shop: return P.Shop;
    case EQuadRole::School: return P.School;
    case EQuadRole::Workshop: return P.Workshop;
    case EQuadRole::Sealed: return P.Sealed;
    }
    return P.Sealed;
}

FLinearColor DotColour(EDotRole Role)
{
    const FMapPalette& P = GetPalette();
    switch (Role)
    {
    case EDotRole::Person: return P.Person;
    case EDotRole::DoorOpen: return P.DoorOpen;
    case EDotRole::DoorClosed: return P.DoorClosed;
    case EDotRole::DoorPrivate: return P.DoorPrivate;
    case EDotRole::DoorLocked: return P.DoorLocked;
    }
    return P.Person;
}

FSlateFontInfo LabelFont(int32 Size)
{
    return PWUI::Font(Size, PWUI::EFontWeight::Medium);
}

FVector2f MeasureText(const FText& Text, const FSlateFontInfo& Font)
{
    if (FSlateApplication::IsInitialized())
    {
        if (FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer())
        {
            const FVector2D Measured = Renderer->GetFontMeasureService()->Measure(Text, Font);
            return FVector2f(static_cast<float>(Measured.X), static_cast<float>(Measured.Y));
        }
    }
    // No renderer (a headless test): a plausible width for a proportional face.
    return FVector2f(static_cast<float>(Text.ToString().Len()) * static_cast<float>(Font.Size) * 0.55f, static_cast<float>(Font.Size) * 1.3f);
}

int32 PaintQuads(const FMapPlan& Plan, const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer, const FLinearColor& Tint, bool bEdges)
{
    for (const FPlanQuad& Quad : Plan.Quads)
    {
        MpBox(Elements, Layer, Geometry, Quad.Centre, Quad.Size, Quad.AngleRadians, QuadColour(Quad.Role) * Tint);
    }
    if (!bEdges)
    {
        return Layer;
    }
    // A hairline round each building, so neighbours with the same fill still read as separate walls.
    const FLinearColor Edge = GetPalette().BuildingEdge * Tint;
    for (const FPlanQuad& Quad : Plan.Quads)
    {
        if (Quad.bBuilding)
        {
            PaintQuadOutline(Quad, Edge, MpEdgeThickness, Geometry, Elements, Layer + 1);
        }
    }
    return Layer + 1;
}

void PaintQuadOutline(const FPlanQuad& Quad, const FLinearColor& Colour, float Thickness, const FGeometry& Geometry, FSlateWindowElementList& Elements,
                      int32 Layer)
{
    const FVector2f Half = Quad.Size * 0.5f;
    const FVector2f Corners[4] = {FVector2f(-Half.X, -Half.Y), FVector2f(Half.X, -Half.Y), FVector2f(Half.X, Half.Y), FVector2f(-Half.X, Half.Y)};
    TArray<FVector2f> Outline;
    for (const FVector2f& Corner : Corners)
    {
        Outline.Add(Quad.Centre + MpRotate(Corner, Quad.AngleRadians));
    }
    Outline.Add(Outline[0]);
    MpLine(Elements, Layer, Geometry, MoveTemp(Outline), Colour, Thickness);
}

void PaintMark(EDotRole Role, const FVector2f& Centre, float Size, const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
               const FLinearColor& Tint)
{
    const FMapPalette& P = GetPalette();
    const FLinearColor Colour = DotColour(Role) * Tint;
    const FLinearColor Halo = P.Halo * Tint;
    const FVector2f Square(Size, Size);
    switch (Role)
    {
    case EDotRole::Person:
        MpDisc(Elements, Layer, Geometry, Centre, Size + 2.0f, Halo);
        MpDisc(Elements, Layer + 1, Geometry, Centre, Size, Colour);
        break;
    case EDotRole::DoorOpen:
        MpDisc(Elements, Layer, Geometry, Centre, Size + 2.0f, Halo);
        MpDisc(Elements, Layer + 1, Geometry, Centre, Size, Colour);
        break;
    case EDotRole::DoorClosed:
        MpBox(Elements, Layer, Geometry, Centre, Square + FVector2f(2.0f, 2.0f), 0.0f, Halo);
        MpBox(Elements, Layer + 1, Geometry, Centre, Square, 0.0f, Colour);
        break;
    case EDotRole::DoorPrivate:
        MpBox(Elements, Layer, Geometry, Centre, Square * 1.15f + FVector2f(2.0f, 2.0f), MpHalfPi * 0.5f, Halo);
        MpBox(Elements, Layer + 1, Geometry, Centre, Square * 1.15f, MpHalfPi * 0.5f, Colour);
        break;
    case EDotRole::DoorLocked:
        MpBox(Elements, Layer, Geometry, Centre, Square + FVector2f(2.0f, 2.0f), 0.0f, Halo);
        {
            const float H = Size * 0.5f;
            MpLine(Elements, Layer + 1, Geometry,
                   {Centre + FVector2f(-H, -H), Centre + FVector2f(H, -H), Centre + FVector2f(H, H), Centre + FVector2f(-H, H), Centre + FVector2f(-H, -H)},
                   Colour, 1.5f);
        }
        break;
    }
}

int32 PaintDots(const FMapPlan& Plan, const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer, const FLinearColor& Tint, float DoorPx,
                float PersonPx)
{
    // Doors first, people over them, so a person standing in a doorway stays visible.
    for (const FPlanDot& Dot : Plan.Dots)
    {
        if (Dot.Role != EDotRole::Person)
        {
            PaintMark(Dot.Role, Dot.Position, DoorPx, Geometry, Elements, Layer, Tint);
        }
    }
    for (const FPlanDot& Dot : Plan.Dots)
    {
        if (Dot.Role == EDotRole::Person)
        {
            PaintMark(Dot.Role, Dot.Position, PersonPx, Geometry, Elements, Layer + 2, Tint);
        }
    }
    return Layer + 3;
}

int32 PaintPlayer(const FPlanArrow& Arrow, float Scale, float Pulse, const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
                  const FLinearColor& Tint)
{
    if (!Arrow.bVisible)
    {
        return Layer;
    }
    const FMapPalette& P = GetPalette();
    const FLinearColor Colour = P.Player * Tint;
    const float R = 4.0f * Scale;
    const float Angle = FMath::DegreesToRadians(Arrow.AngleDegrees);
    const FVector2f Direction(FMath::Sin(Angle), -FMath::Cos(Angle));
    const FVector2f Tip = Arrow.Position + Direction * (R * 3.2f);
    const FVector2f Left = Tip + MpRotate(-Direction, 0.5f) * (R * 1.7f);
    const FVector2f Right = Tip + MpRotate(-Direction, -0.5f) * (R * 1.7f);

    if (Pulse >= 0.0f)
    {
        const float Spread = FMath::Clamp(Pulse, 0.0f, 1.0f);
        MpRing(Elements, Layer, Geometry, Arrow.Position, R * (3.0f + 5.0f * Spread), 1.5f, Colour.CopyWithNewOpacity(Colour.A * (1.0f - Spread) * 0.8f));
    }
    MpDisc(Elements, Layer + 1, Geometry, Arrow.Position, R * 2.0f + 3.0f, P.Halo * Tint);
    MpLine(Elements, Layer + 1, Geometry, {Arrow.Position, Tip}, P.Halo * Tint, R * 0.9f + 3.0f);
    MpLine(Elements, Layer + 1, Geometry, {Left, Tip, Right}, P.Halo * Tint, 4.5f);
    MpDisc(Elements, Layer + 2, Geometry, Arrow.Position, R * 2.0f, Colour);
    MpLine(Elements, Layer + 2, Geometry, {Arrow.Position, Tip}, Colour, R * 0.9f);
    MpLine(Elements, Layer + 2, Geometry, {Left, Tip, Right}, Colour, 2.5f);
    return Layer + 2;
}

void PaintLabel(const FText& Text, const FVector2f& Centre, const FSlateFontInfo& Font, const FGeometry& Geometry, FSlateWindowElementList& Elements,
                int32 Layer, const FLinearColor& Tint)
{
    if (Text.IsEmpty())
    {
        return;
    }
    const FVector2f Size = MeasureText(Text, Font);
    const FVector2f Origin = Centre - Size * 0.5f;
    const FMapPalette& P = GetPalette();
    FSlateDrawElement::MakeText(Elements, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Origin + FVector2f(1.0f, 1.0f))), Text, Font,
                                ESlateDrawEffect::None, P.LabelShadow * Tint);
    FSlateDrawElement::MakeText(Elements, Layer + 1, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Origin)), Text, Font, ESlateDrawEffect::None,
                                P.Label * Tint);
}

void PaintSwatch(EQuadRole Role, const FVector2f& Centre, float Size, const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
                 const FLinearColor& Tint)
{
    const FLinearColor Edge = GetPalette().BuildingEdge * Tint;
    MpBox(Elements, Layer, Geometry, Centre, FVector2f(Size, Size), 0.0f, QuadColour(Role) * Tint);
    const float H = Size * 0.5f;
    MpLine(Elements, Layer + 1, Geometry,
           {Centre + FVector2f(-H, -H), Centre + FVector2f(H, -H), Centre + FVector2f(H, H), Centre + FVector2f(-H, H), Centre + FVector2f(-H, -H)},
           Edge, MpEdgeThickness);
}
} // namespace PWMap
