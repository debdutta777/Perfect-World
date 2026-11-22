#include "PerfectWorldNeighbourhoodFacade.h"
#include "PerfectWorldNeighbourhoodRoads.h"
#include "PerfectWorldNeighbourhoodStreets.h"
#include "PerfectWorldNeighbourhoodText.h"

namespace PWNs
{
namespace
{
constexpr double NsBarrierTop = 400.0;
constexpr double NsBarrierThickness = 60.0;

/** Striped road-work boards across a span, with invisible Pawn-only barrier behind and a hazard sign above. */
void NsRoadEnd(FNsSink& K, FNsContext& Ctx, const FNsRect& Span, ENsFace Faces, const char* Text, uint32 Seed)
{
    const bool bAlongX = Faces == ENsFace::PlusY || Faces == ENsFace::MinusY; // boards run along X when the street runs along Y
    constexpr double Thick = 40.0;
    constexpr double Stripe = 100.0;
    const double From = bAlongX ? Span.X0 : Span.Y0;
    const double To = bAlongX ? Span.X1 : Span.Y1;
    int32 I = 0;
    for (double A = From; A < To - 1.0; A += Stripe, ++I)
    {
        const double B = FMath::Min(A + Stripe, To);
        FNsRect Board;
        switch (Faces)
        {
        case ENsFace::PlusX:
            Board = FNsRect(Span.X0, A, Span.X0 + Thick, B);
            break;
        case ENsFace::MinusX:
            Board = FNsRect(Span.X1 - Thick, A, Span.X1, B);
            break;
        case ENsFace::PlusY:
            Board = FNsRect(A, Span.Y0, B, Span.Y0 + Thick);
            break;
        case ENsFace::MinusY:
            Board = FNsRect(A, Span.Y1 - Thick, B, Span.Y1);
            break;
        }
        K.Slab((I % 2 == 0) ? ELaneColour::AwningRed : ELaneColour::TrimLight, Board, NsPath, NsPath + 110.0);
        K.Prop(Board.X0, Board.Y0, NsPath, Board.X1, Board.Y1, NsPath + 110.0);
    }
    // The invisible boundary behind the boards: the walking player never gets past the end of the neighbourhood.
    FNsRect Fence;
    switch (Faces)
    {
    case ENsFace::PlusX:
        Fence = FNsRect(Span.X0 - NsBarrierThickness, Span.Y0, Span.X0, Span.Y1);
        break;
    case ENsFace::MinusX:
        Fence = FNsRect(Span.X1, Span.Y0, Span.X1 + NsBarrierThickness, Span.Y1);
        break;
    case ENsFace::PlusY:
        Fence = FNsRect(Span.X0, Span.Y0 - NsBarrierThickness, Span.X1, Span.Y0);
        break;
    case ENsFace::MinusY:
        Fence = FNsRect(Span.X0, Span.Y1, Span.X1, Span.Y1 + NsBarrierThickness);
        break;
    }
    K.Barrier(Fence.X0, Fence.Y0, NsGround - 20.0, Fence.X1, Fence.Y1, NsGround + NsBarrierTop);

    // A yellow hazard sign on two posts above the boards, lettered for the viewer on the street side.
    const double Mid = bAlongX ? Span.CentreX() : Span.CentreY();
    const double SignHalf = 230.0;
    FNsRect Panel;
    switch (Faces)
    {
    case ENsFace::PlusX:
        Panel = FNsRect(Span.X0 + 14.0, Mid - SignHalf, Span.X0 + 22.0, Mid + SignHalf);
        break;
    case ENsFace::MinusX:
        Panel = FNsRect(Span.X1 - 22.0, Mid - SignHalf, Span.X1 - 14.0, Mid + SignHalf);
        break;
    case ENsFace::PlusY:
        Panel = FNsRect(Mid - SignHalf, Span.Y0 + 14.0, Mid + SignHalf, Span.Y0 + 22.0);
        break;
    case ENsFace::MinusY:
        Panel = FNsRect(Mid - SignHalf, Span.Y1 - 22.0, Mid + SignHalf, Span.Y1 - 14.0);
        break;
    }
    double W = 0.0;
    double D = 0.0;
    const FLaneFrame F = NsFaceFrame(Panel, Faces, W, D);
    K.Add(F, ELaneColour::TrimDark, 10.0, 22.0, 0.0, D, 20.0, 190.0);
    K.Add(F, ELaneColour::TrimDark, W - 22.0, W - 10.0, 0.0, D, 20.0, 190.0);
    K.AddProp(F, 10.0, 22.0, 0.0, D, 20.0, 190.0);
    K.AddProp(F, W - 22.0, W - 10.0, 0.0, D, 20.0, 190.0);
    Ctx.SignCount += NsSignboard(K, F, 24.0, W - 24.0, 130.0, 190.0, ELaneColour::GoodsYellow, ELaneColour::TrimDark, Text) > 0 ? 1 : 0;
    (void)Seed;
}

/** The union of everything the overlay occupies, grown a little, as four outer Pawn barriers. */
void NsPerimeter(FNsSink& K, const FNsContext& Ctx)
{
    double MinX = 1.0e9;
    double MinY = 1.0e9;
    double MaxX = -1.0e9;
    double MaxY = -1.0e9;
    const auto Grow = [&](const FNsRect& R)
    {
        MinX = FMath::Min(MinX, R.X0);
        MinY = FMath::Min(MinY, R.Y0);
        MaxX = FMath::Max(MaxX, R.X1);
        MaxY = FMath::Max(MaxY, R.Y1);
    };
    for (const FNsRect& R : Ctx.PlanSolids())
    {
        Grow(R);
    }
    for (const FNsLot& Lot : Ctx.Lots)
    {
        Grow(Lot.R);
    }
    for (const FNsWall& Wall : Ctx.Walls)
    {
        Grow(Wall.R);
    }
    if (MinX > MaxX)
    {
        return;
    }
    const double T = NsBarrierThickness;
    const double Z0 = NsGround - 20.0;
    const double Z1 = NsGround + NsBarrierTop;
    K.Barrier(MinX - T, MinY - T, Z0, MinX, MaxY + T, Z1);
    K.Barrier(MaxX, MinY - T, Z0, MaxX + T, MaxY + T, Z1);
    K.Barrier(MinX, MinY - T, Z0, MaxX, MinY, Z1);
    K.Barrier(MinX, MaxY, Z0, MaxX, MaxY + T, Z1);
}
} // namespace

void NsBuildBoundaries(FNsSink& K, FNsContext& Ctx)
{
    const FNsRect ParaRoad = NsCarriageOf(Ctx, "street.nabapara.para");
    const FNsRect MainRoad = NsCarriageOf(Ctx, "street.nabapara.main");
    const FNsRect CrossWest = Ctx.Bounds("street.nabapara.cross.footpath_left");
    const FNsRect CrossEast = Ctx.Bounds("street.nabapara.cross.footpath_right");
    const FNsRect ParaSouth = Ctx.Bounds("street.nabapara.para.footpath_right_west");
    const FNsRect ParaNorth = Ctx.Bounds("street.nabapara.para.footpath_left");
    const FNsRect MainWest = Ctx.Bounds("street.nabapara.main.footpath_left_south");
    const FNsRect MainEast = Ctx.Bounds("street.nabapara.main.footpath_right");

    // The old north-alley stopping barrier no longer closes the cross street, so the walls of the buildings that flank its
    // mouth need to continue to the first new frontage: plug the gap behind their side walls, 2 cm inside the alley wall plane
    // (hidden wherever a lane building stands there, and no coplanar faces to fight with it).
    const double PlugY0 = 1060.0; // the shallowest first-row building's rear (400 front + 700 deep, less nothing)
    K.Slab(ELaneColour::WallCream, FNsRect(CrossWest.X0 - 40.0, PlugY0, CrossWest.X0 - 2.0, Ctx.Bounds("reserve.nabapara.original_lane").Y1 + 40.0),
           NsGround, NsGround + 520.0);
    K.Slab(ELaneColour::WallCream, FNsRect(CrossEast.X1 + 2.0, PlugY0, CrossEast.X1 + 40.0, Ctx.Bounds("reserve.nabapara.original_lane").Y1 + 40.0),
           NsGround, NsGround + 520.0);

    // The ends of the para road and the main road are road works: boards, a hazard sign and a Pawn-only boundary behind them.
    NsRoadEnd(K, Ctx, FNsRect(ParaRoad.X0, ParaSouth.Y0 - 100.0, ParaRoad.X0 + 40.0, ParaNorth.Y1 + 50.0), ENsFace::PlusX, "ROAD WORK", 1u);
    const double MainW = MainWest.X0 - 100.0;
    const double MainE = MainEast.X1 + 100.0;
    NsRoadEnd(K, Ctx, FNsRect(MainW, MainRoad.Y0, MainE, MainRoad.Y0 + 40.0), ENsFace::PlusY, "ROAD WORK", 2u);
    NsRoadEnd(K, Ctx, FNsRect(MainW, MainRoad.Y1 - 40.0, MainE, MainRoad.Y1), ENsFace::MinusY, "ROAD WORK", 3u);

    NsPerimeter(K, Ctx);
}
} // namespace PWNs
