#include "UI/PWSigilPainter.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "Styling/WidgetStyle.h"
#include "UI/PWStyle.h"

namespace
{
FVector2f Polar(float Radius, float Degrees)
{
    const float Angle = FMath::DegreesToRadians(Degrees);
    return FVector2f(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle));
}

TArray<FVector2f> Arc(FVector2f Centre, float Radius, float Angle, float Span)
{
    TArray<FVector2f> Points;
    for (int32 Index = 0; Index <= 24; ++Index)
        Points.Add(Centre + Polar(Radius, Angle - Span * .5f + Span * Index / 24.0f));
    return Points;
}

void Line(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry,
          TArray<FVector2f> Points, FLinearColor Colour, float Width, ESlateDrawEffect Effects)
{
    FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), MoveTemp(Points), Effects, Colour, true, Width);
}

TArray<TArray<FVector2f>> Glyph(EPWSigilPictogram Pictogram)
{
    TArray<TArray<FVector2f>> Lines;
    switch (Pictogram)
    {
    case EPWSigilPictogram::Doorway:
    case EPWSigilPictogram::FirstMorning:
        Lines.Add({FVector2f(-.65f, .7f), FVector2f(-.65f, -.85f), FVector2f(.65f, -.85f), FVector2f(.65f, .7f)});
        if (Pictogram == EPWSigilPictogram::FirstMorning) Lines.Add({FVector2f(-1, 1), FVector2f(1, 1)});
        break;
    case EPWSigilPictogram::Ledger:
        for (float Y : {-.65f, 0.0f, .65f}) Lines.Add({FVector2f(-1, Y), FVector2f(1, Y)});
        break;
    case EPWSigilPictogram::Paths:
        for (float X : {-.65f, 0.0f, .65f}) Lines.Add({FVector2f(X, -1), FVector2f(X, 1)});
        break;
    case EPWSigilPictogram::Person:
        Lines.Add(Arc(FVector2f(0, -.55f), .4f, 0, 360));
        Lines.Add({FVector2f(-1, 1), FVector2f(-1, .3f), FVector2f(1, .3f), FVector2f(1, 1)});
        break;
    default: break;
    }
    return Lines;
}
}

void FPWSigilGeometry::Rebuild(FVector2f InSize, EPWSigilSizeClass SizeClass)
{
    if (Size == InSize) return;
    Size = InSize;
    Centre = Size * .5f;
    Outer.Reset(); Inner.Reset(); Ends.Reset(); Chevrons.Reset(); Ticks.Reset(); Motes.Reset();
    // Full apertures reserve the entire 1.5R mote envelope; seals reserve the 1.10R tick envelope.
    const float R = FMath::Min(Size.X, Size.Y) / (SizeClass == EPWSigilSizeClass::Seal ? 2.2f : 3.0f);
    for (int32 Sector = 0; Sector < 5; ++Sector)
    {
        const float Angle = -90.0f + 72.0f * Sector;
        Outer.Add(Arc(Centre, R, Angle, 48.0f));
        Inner.Add(Arc(Centre, .88f * R, Angle, 48.0f));
        for (float Offset : {-24.0f, 24.0f})
            Ends.Add({Centre + Polar(.88f * R, Angle + Offset), Centre + Polar(R, Angle + Offset)});
        Chevrons.Add({Centre + Polar(.59f * R, Angle - 12.0f), Centre + Polar(.76f * R, Angle),
                     Centre + Polar(.59f * R, Angle + 12.0f)});
        for (int32 Tick = -2; Tick <= 2; ++Tick)
            Ticks.Add({Centre + Polar(1.03f * R, Angle + 8.0f * Tick),
                       Centre + Polar((Tick == 0 ? 1.10f : 1.07f) * R, Angle + 8.0f * Tick)});
        Motes.Add(Centre + Polar(1.20f * R, Angle - 10.0f));
        Motes.Add(Centre + Polar(1.42f * R, Angle + 10.0f));
    }
    const float Unit = FMath::Min(Size.X, Size.Y) * (SizeClass == EPWSigilSizeClass::Seal ? .15f : .10f);
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Glyphs); ++Index)
    {
        Glyphs[Index] = Glyph(static_cast<EPWSigilPictogram>(Index));
        for (auto& Path : Glyphs[Index]) for (auto& Point : Path) Point = Centre + Point * Unit;
    }
}

int32 FPWSigilPainter::Paint(const FPWSigilGeometry& Paths, EPWSigilSizeClass SizeClass, EPWSigilPictogram Pictogram,
                            const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
                            const FWidgetStyle& WidgetStyle, float Reveal, float Rotation, bool bMotes, bool bEnabled)
{
    const auto& Palette = PWUI::FStyle::Get();
    const FLinearColor Tint = WidgetStyle.GetColorAndOpacityTint().CopyWithNewOpacity(
        WidgetStyle.GetColorAndOpacityTint().A * Reveal);
    const auto Colour = [&Tint](FLinearColor Token, float Alpha) { return Token.CopyWithNewOpacity(Alpha) * Tint; };
    const ESlateDrawEffect Effects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
    const bool bSmall = FMath::Min(Paths.Size.X, Paths.Size.Y) < 48.0f;
    const bool bAperture = SizeClass == EPWSigilSizeClass::Aperture;
    // Over the live street a 40% amber hairline vanished against sunlit walls (packaged capture, 2026-10-13), so the
    // full aperture sits on a soft dark lens of the background token and draws heavier, stronger strokes. Seals sit on
    // panels and keep the specified weights.
    if (bAperture && !bSmall)
    {
        const float R = FMath::Min(Paths.Size.X, Paths.Size.Y) / 3.0f;
        for (const float Scale : {1.32f, 1.16f, 1.0f})
        {
            const float Side = 2.0f * Scale * R;
            const FSlateRoundedBoxBrush Disc(FLinearColor::White, Side * .5f);
            FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(FVector2f(Side, Side),
                FSlateLayoutTransform(Paths.Centre - FVector2f(Side * .5f, Side * .5f))), &Disc, Effects,
                Colour(Palette.Background, .20f));
        }
    }
    const float AccentAlpha = bAperture ? .72f : .70f;
    const float Stroke = bAperture ? 2.25f : 1.5f;
    for (const auto& Path : Paths.Outer) Line(Elements, Layer, Geometry, Path, Colour(Palette.Accent, AccentAlpha), Stroke, Effects);
    if (!bSmall)
    {
        for (const auto& Path : Paths.Inner) Line(Elements, Layer, Geometry, Path, Colour(Palette.TextSecondary, bAperture ? .50f : .45f), bAperture ? 1.5f : 1.0f, Effects);
        for (const auto& Path : Paths.Ends) Line(Elements, Layer, Geometry, Path, Colour(Palette.Accent, AccentAlpha), Stroke, Effects);
        for (const auto& Path : Paths.Chevrons) Line(Elements, Layer, Geometry, Path, Colour(Palette.Accent, AccentAlpha), Stroke, Effects);
        const float Cos = FMath::Cos(Rotation), Sin = FMath::Sin(Rotation);
        for (const auto& Path : Paths.Ticks)
        {
            TArray<FVector2f> Rotated;
            for (const auto& Point : Path)
            {
                const FVector2f P = Point - Paths.Centre;
                Rotated.Add(Paths.Centre + FVector2f(P.X * Cos - P.Y * Sin, P.X * Sin + P.Y * Cos));
            }
            Line(Elements, Layer, Geometry, MoveTemp(Rotated), Colour(Palette.TextSecondary, bAperture ? .55f : .28f), bAperture ? 1.5f : 1.0f, Effects);
        }
        if (bMotes && SizeClass == EPWSigilSizeClass::Aperture)
        {
            static const FSlateColorBrush Solid(FLinearColor::White);
            for (int32 Index = 0; Index < Paths.Motes.Num(); ++Index)
            {
                const float Side = Index % 2 == 0 ? 2.0f : 3.0f;
                const FVector2f Offset = Paths.Motes[Index] - FVector2f(Side * .5f, Side * .5f);
                FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(FVector2f(Side, Side), FSlateLayoutTransform(Offset)),
                                          &Solid, Effects, Colour(Palette.TextPrimary, .45f));
            }
        }
    }
    // Pictograms use the same neutral foreground treatment for every page and origin.
    for (const auto& Path : Paths.Glyphs[static_cast<uint8>(Pictogram)])
        Line(Elements, Layer, Geometry, Path, Colour(Palette.TextPrimary, .55f), 1.0f, Effects);
    return Layer;
}

void FPWBracketGeometry::Rebuild(FVector2f InSize)
{
    if (Size == InSize) return;
    Size = InSize;
    Lines.Reset();
    for (int32 X = 0; X < 2; ++X) for (int32 Y = 0; Y < 2; ++Y)
    {
        const FVector2f Corner(X ? Size.X - 8 : 8, Y ? Size.Y - 8 : 8);
        const FVector2f H(X ? -1 : 1, 0), V(0, Y ? -1 : 1);
        Lines.Add({Corner + H * 12, Corner, Corner + V * 12});
        const FVector2f Inset = Corner + (H + V) * 4;
        Lines.Add({Inset, Inset + H * 4});
    }
}

int32 FPWSigilPainter::PaintBrackets(const FPWBracketGeometry& Paths, const FGeometry& Geometry,
                                    FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& WidgetStyle)
{
    const FLinearColor Colour = PWUI::FStyle::Get().Divider.CopyWithNewOpacity(.45f) * WidgetStyle.GetColorAndOpacityTint();
    for (const auto& Path : Paths.Lines) Line(Elements, Layer, Geometry, Path, Colour, 1, ESlateDrawEffect::None);
    return Layer;
}
