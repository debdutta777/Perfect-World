#pragma once

#include "CoreMinimal.h"
#include "Layout/Geometry.h"

class FSlateWindowElementList;
class FWidgetStyle;

enum class EPWSigilSizeClass : uint8 { Aperture, Seal };
enum class EPWSigilPictogram : uint8 { None, Doorway, Ledger, Person, Paths, FirstMorning };

/** Paths are authored in radii, then cached in local units only when the allotted size changes. */
struct FPWSigilGeometry
{
    FVector2f Size = FVector2f(-1, -1);
    FVector2f Centre = FVector2f::ZeroVector;
    TArray<TArray<FVector2f>> Outer, Inner, Ends, Chevrons, Ticks;
    TArray<TArray<FVector2f>> Glyphs[6];
    TArray<FVector2f> Motes;
    void Rebuild(FVector2f InSize, EPWSigilSizeClass SizeClass);
};

struct FPWBracketGeometry
{
    FVector2f Size = FVector2f(-1, -1);
    TArray<TArray<FVector2f>> Lines;
    void Rebuild(FVector2f InSize);
};

struct FPWSigilPainter
{
    static int32 Paint(const FPWSigilGeometry& Paths, EPWSigilSizeClass SizeClass, EPWSigilPictogram Pictogram,
                       const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
                       const FWidgetStyle& Style, float Reveal, float Rotation, bool bMotes, bool bEnabled);
    static int32 PaintBrackets(const FPWBracketGeometry& Paths, const FGeometry& Geometry,
                               FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style);
};
