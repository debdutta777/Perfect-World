#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Geometry.h"
#include "UI/PWMapPlan.h"

class FSlateWindowElementList;


/**
 * Turns a map plan into Slate draw elements (order F2). Shared by the minimap, the full map and the legend, so a door, a person or a kind of
 * building looks the same everywhere. Every colour is a blend of the Fivefold Aperture tokens (PWStyle.h); no new hues are introduced.
 * Shape carries the meaning as well as colour: a door that is open is a disc, one closed for now is a square, a private one a diamond and a
 * locked one a hollow square.
 */
namespace PWMap
{
struct FMapPalette
{
    FLinearColor Road, Alley, Footpath;
    FLinearColor Home, Shop, School, Workshop, Sealed;
    FLinearColor BuildingEdge;
    FLinearColor DoorOpen, DoorClosed, DoorPrivate, DoorLocked;
    FLinearColor Person, Player;
    FLinearColor Halo;
    FLinearColor Label, LabelShadow;
};

const FMapPalette& GetPalette();
FLinearColor QuadColour(EQuadRole Role);
FLinearColor DotColour(EDotRole Role);

/** The widest text the map labels use, and the size of its font. */
FSlateFontInfo LabelFont(int32 Size);
FVector2f MeasureText(const FText& Text, const FSlateFontInfo& Font);

/** Filled rectangles: surfaces then buildings (the plan keeps that order). Returns the highest layer used. */
int32 PaintQuads(const FMapPlan& Plan, const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer, const FLinearColor& Tint, bool bEdges);

/** An outline round one plan rectangle, for a hovered or highlighted building. */
void PaintQuadOutline(const FPlanQuad& Quad, const FLinearColor& Colour, float Thickness, const FGeometry& Geometry, FSlateWindowElementList& Elements,
                      int32 Layer);

/** Door and person marks. DoorPx and PersonPx are the marks' widths in widget units. */
int32 PaintDots(const FMapPlan& Plan, const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer, const FLinearColor& Tint, float DoorPx,
                float PersonPx);

/** One mark at a point; also used by the legend. */
void PaintMark(EDotRole Role, const FVector2f& Centre, float Size, const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
               const FLinearColor& Tint);

/** The "you are here" marker: a dot, a heading stick and arrowhead; Pulse in 0..1 adds an expanding ring (a negative value draws none). */
int32 PaintPlayer(const FPlanArrow& Arrow, float Scale, float Pulse, const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
                  const FLinearColor& Tint);

/** Text centred on a point with a one-pixel shadow so it reads over any fill. */
void PaintLabel(const FText& Text, const FVector2f& Centre, const FSlateFontInfo& Font, const FGeometry& Geometry, FSlateWindowElementList& Elements,
                int32 Layer, const FLinearColor& Tint);

/** A square legend swatch for a building or surface role, and a mark swatch is PaintMark. */
void PaintSwatch(EQuadRole Role, const FVector2f& Centre, float Size, const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
                 const FLinearColor& Tint);
} // namespace PWMap
