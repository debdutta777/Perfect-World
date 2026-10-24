#include "PerfectWorldNeighbourhoodFacade.h"
#include "PerfectWorldNeighbourhoodRoads.h"
#include "PerfectWorldNeighbourhoodStreets.h"

namespace PWNs
{
namespace
{
constexpr double NsGroundCut = 255.0;  // the ground storey of a closed frontage (shutter lintel height)
constexpr double NsMinBuilding = 420.0; // narrower than this is not a building: a hoarding wall closes the gap
constexpr double NsIdealWidth = 780.0;
constexpr double NsMaxBuilding = 1150.0;

/** One frontage line: buildings stand on one side of a street with their faces on Line, Depth deep, from From to To along it. */
struct FNsEdge
{
    ENsFace Face = ENsFace::PlusY; // the outward normal, toward the street
    double Line = 0.0;             // Y of the face line for PlusY/MinusY faces, X for PlusX/MinusX
    double From = 0.0;             // along the edge (X for Y-facing edges, Y for X-facing ones)
    double To = 0.0;
    double Depth = 1000.0;
    double Apron = 0.0;            // paving between the footpath and the face line
    bool bFull = true;             // full-detail frontage (first row) or medium-detail second row
    bool bSeal = true;             // close unbuilt stretches with hoardings (first rows only)
    int32 MinStoreys = 2;
    int32 StoreyRange = 3;
    int32 Salt = 0;
};

bool NsAlongX(ENsFace Face)
{
    return Face == ENsFace::PlusY || Face == ENsFace::MinusY;
}

FNsRect NsEdgeRect(const FNsEdge& E, double A0, double A1, double Depth)
{
    switch (E.Face)
    {
    case ENsFace::PlusY:
        return FNsRect(A0, E.Line - Depth, A1, E.Line);
    case ENsFace::MinusY:
        return FNsRect(A0, E.Line, A1, E.Line + Depth);
    case ENsFace::PlusX:
        return FNsRect(E.Line - Depth, A0, E.Line, A1);
    case ENsFace::MinusX:
        return FNsRect(E.Line, A0, E.Line + Depth, A1);
    }
    return FNsRect();
}

/** The paving strip between a footpath's outer edge and the face line. */
FNsRect NsApronRect(const FNsEdge& E)
{
    switch (E.Face)
    {
    case ENsFace::PlusY:
        return FNsRect(E.From, E.Line, E.To, E.Line + E.Apron);
    case ENsFace::MinusY:
        return FNsRect(E.From, E.Line - E.Apron, E.To, E.Line);
    case ENsFace::PlusX:
        return FNsRect(E.Line, E.From, E.Line + E.Apron, E.To);
    case ENsFace::MinusX:
        return FNsRect(E.Line - E.Apron, E.From, E.Line, E.To);
    }
    return FNsRect();
}

/** The stretches of the edge no obstacle covers, ordered along it. */
TArray<FNsRect> NsFreeSpans(const FNsContext& Ctx, const FNsEdge& E)
{
    const FNsRect Slab = NsEdgeRect(E, E.From, E.To, E.Depth);
    TArray<FNsRect> Obstacles = Ctx.PlanSolids();
    for (const FNsLot& Lot : Ctx.Lots)
    {
        Obstacles.Add(Lot.R);
    }
    for (const FNsWall& Wall : Ctx.Walls)
    {
        Obstacles.Add(Wall.R);
    }
    // Along-edge intervals blocked by anything that touches the slab, merged.
    TArray<FNsRect> Blocks;
    for (const FNsRect& O : Obstacles)
    {
        if (O.Overlaps(Slab))
        {
            Blocks.Add(O);
        }
    }
    const bool bX = NsAlongX(E.Face);
    Blocks.Sort([bX](const FNsRect& A, const FNsRect& B) { return bX ? A.X0 < B.X0 : A.Y0 < B.Y0; });
    TArray<FNsRect> Free;
    double Cursor = E.From;
    for (const FNsRect& B : Blocks)
    {
        const double B0 = bX ? B.X0 : B.Y0;
        const double B1 = bX ? B.X1 : B.Y1;
        if (B0 > Cursor + 0.5)
        {
            Free.Add(NsEdgeRect(E, Cursor, FMath::Min(B0, E.To), E.Depth));
        }
        Cursor = FMath::Max(Cursor, B1);
        if (Cursor >= E.To)
        {
            break;
        }
    }
    if (Cursor < E.To - 0.5)
    {
        Free.Add(NsEdgeRect(E, Cursor, E.To, E.Depth));
    }
    return Free;
}

/** Widths that sum to Len, each between about 420 and 1150, varied by the seed. */
TArray<double> NsPartition(double Len, uint32 Seed)
{
    int32 N = FMath::Max(1, FMath::RoundToInt(Len / NsIdealWidth));
    N = FMath::Max(N, FMath::CeilToInt(Len / NsMaxBuilding));
    while (N > 1 && Len / N < NsMinBuilding)
    {
        --N;
    }
    TArray<double> Factors;
    double Sum = 0.0;
    for (int32 I = 0; I < N; ++I)
    {
        const double F = 0.78 + 0.44 * NsUnit(NsHash(Seed, I, 7));
        Factors.Add(F);
        Sum += F;
    }
    TArray<double> Widths;
    for (const double F : Factors)
    {
        Widths.Add(Len * F / Sum);
    }
    return Widths;
}

/** True when the side of the lot facing Side touches a street, so it shows and gets windows. */
bool NsSideShows(const FNsContext& Ctx, const FNsRect& R, ENsFace Side)
{
    constexpr double Probe = 60.0;
    FNsRect Strip;
    switch (Side)
    {
    case ENsFace::PlusX:
        Strip = FNsRect(R.X1, R.Y0 + 1.0, R.X1 + Probe, R.Y1 - 1.0);
        break;
    case ENsFace::MinusX:
        Strip = FNsRect(R.X0 - Probe, R.Y0 + 1.0, R.X0, R.Y1 - 1.0);
        break;
    case ENsFace::PlusY:
        Strip = FNsRect(R.X0 + 1.0, R.Y1, R.X1 - 1.0, R.Y1 + Probe);
        break;
    case ENsFace::MinusY:
        Strip = FNsRect(R.X0 + 1.0, R.Y0 - Probe, R.X1 - 1.0, R.Y0);
        break;
    }
    return Ctx.TouchesStreet(Strip);
}

void NsBuildLot(FNsSink& K, FNsContext& Ctx, const FNsLot& Lot)
{
    double W = 0.0;
    double D = 0.0;
    const FLaneFrame F = NsFaceFrame(Lot.R, Lot.Face, W, D);
    FNsFacade S;
    S.Width = W;
    S.Depth = D;
    S.Storeys = Lot.Storeys;
    S.Seed = Lot.Seed;
    S.Wall = Lot.Wall;
    S.Shutter = NsShutterColour(Lot.Seed >> 11);
    if (!Lot.bFull)
    {
        NsMediumBlock(K, F, S);
        return;
    }
    const bool bShop = W >= NsMinBuilding && ((Lot.Seed >> 2) % 3u) != 0u;
    if (bShop)
    {
        if (NsGroundShutterShop(K, F, S) > 0)
        {
            ++Ctx.SignCount;
        }
    }
    else
    {
        NsGroundHouse(K, F, S);
    }
    NsUpperVolume(K, F, S, NsGroundCut);
    NsUpperFloors(K, F, S);
    NsRoof(K, F, S, true);

    // A side wall that lies on another street (a corner building) gets windows too.
    for (const ENsFace Side : {ENsFace::PlusX, ENsFace::MinusX, ENsFace::PlusY, ENsFace::MinusY})
    {
        const bool bFace = Side == Lot.Face;
        const bool bBack = NsNormal(Side) == -NsNormal(Lot.Face);
        if (bFace || bBack || !NsSideShows(Ctx, Lot.R, Side))
        {
            continue;
        }
        double SideW = 0.0;
        double SideD = 0.0;
        const FLaneFrame SideFrame = NsFaceFrame(Lot.R, Side, SideW, SideD);
        FNsFacade Flank = S;
        Flank.Width = SideW;
        Flank.Depth = SideD;
        NsFlankFace(K, SideFrame, Flank);
    }
}

void NsRunEdge(FNsSink& K, FNsContext& Ctx, const FNsEdge& E)
{
    if (E.Apron > 0.0)
    {
        NsEmitPaving(K, Ctx, NsApronRect(E));
    }
    const bool bX = NsAlongX(E.Face);
    int32 Index = 0;
    ELaneColour Previous = ELaneColour::Count;
    for (const FNsRect& Span : NsFreeSpans(Ctx, E))
    {
        const double A0 = bX ? Span.X0 : Span.Y0;
        const double A1 = bX ? Span.X1 : Span.Y1;
        const double Len = A1 - A0;
        if (Len < NsMinBuilding)
        {
            if (E.bSeal && Len > 1.0)
            {
                // A stretch too narrow to build on: a painted compound wall keeps the walker out of the land behind.
                const FNsRect WallRect = NsEdgeRect(E, A0, A1, 30.0);
                double WallW = 0.0;
                double WallD = 0.0;
                const FLaneFrame WallFrame = NsFaceFrame(WallRect, E.Face, WallW, WallD);
                Ctx.SignCount += NsHoarding(K, WallFrame, WallW, WallD, NsHash(static_cast<uint32>(E.Salt), Index, 5)) > 0 ? 1 : 0;
                Ctx.Walls.Add({WallRect, 240.0});
            }
            continue;
        }
        const uint32 SpanSeed = NsHash(static_cast<uint32>(E.Salt), static_cast<int32>(A0), 3);
        double Cursor = A0;
        const TArray<double> Widths = NsPartition(Len, SpanSeed);
        for (int32 WidthIndex = 0; WidthIndex < Widths.Num(); ++WidthIndex)
        {
            // The last building ends exactly on the span's end, so accumulated rounding never leaves a sliver or an overlap.
            const double Width = WidthIndex == Widths.Num() - 1 ? A1 - Cursor : Widths[WidthIndex];
            const uint32 H = NsHash(static_cast<uint32>(E.Salt), Index, static_cast<int32>(Cursor));
            FNsLot Lot;
            Lot.R = NsEdgeRect(E, Cursor, Cursor + Width, E.Depth);
            Lot.Face = E.Face;
            Lot.Storeys = E.MinStoreys + static_cast<int32>((H >> 8) % static_cast<uint32>(E.StoreyRange));
            Lot.Seed = H;
            Lot.Wall = NsWallColour(H >> 20, Previous);
            Lot.bFull = E.bFull;
            Previous = Lot.Wall;
            NsBuildLot(K, Ctx, Lot);
            Ctx.Lots.Add(Lot);
            Cursor = WidthIndex == Widths.Num() - 1 ? A1 : Cursor + Width;
            ++Index;
        }
    }
}
} // namespace

void NsBuildFrontages(FNsSink& K, FNsContext& Ctx)
{
    const FNsRect ParaSouthWest = Ctx.Bounds("street.nabapara.para.footpath_right_west");
    const FNsRect ParaSouthEast = Ctx.Bounds("street.nabapara.para.footpath_right_east");
    const FNsRect ParaNorth = Ctx.Bounds("street.nabapara.para.footpath_left");
    const FNsRect MainWestSouth = Ctx.Bounds("street.nabapara.main.footpath_left_south");
    const FNsRect MainWestNorth = Ctx.Bounds("street.nabapara.main.footpath_left_north");
    const FNsRect MainEast = Ctx.Bounds("street.nabapara.main.footpath_right");
    const FNsRect CrossWest = Ctx.Bounds("street.nabapara.cross.footpath_left");
    const FNsRect CrossEast = Ctx.Bounds("street.nabapara.cross.footpath_right");
    const FNsRect LaneMass = Ctx.Bounds("reserve.nabapara.original_lane");
    const FNsRect MainRoad = NsCarriageOf(Ctx, "street.nabapara.main");

    // The plan's shop lots front the para road at Y = footpath edge - 100 (their entrances are at Y 3500); every frontage keeps that line.
    constexpr double SouthApron = 100.0;
    constexpr double NorthApron = 50.0; // the campus wall stands 50 cm back from the north footpath
    constexpr double MainApron = 100.0;
    const double ParaSouthLine = ParaSouthWest.Y0 - SouthApron;
    const double ParaNorthLine = ParaNorth.Y1 + NorthApron;
    const double MainWestLine = MainWestSouth.X0 - MainApron;
    const double MainEastLine = MainEast.X1 + MainApron;
    const double ParaFrom = ParaSouthWest.X0;
    (void)ParaSouthEast;

    FNsEdge E;
    // First rows: full-detail frontages that close the streets.
    E.Face = ENsFace::PlusY;
    E.Line = ParaSouthLine;
    E.From = ParaFrom;
    E.To = MainWestLine;
    E.Apron = SouthApron;
    E.Salt = 11;
    NsRunEdge(K, Ctx, E);

    E.Face = ENsFace::MinusY;
    E.Line = ParaNorthLine;
    E.Apron = NorthApron;
    E.Salt = 12;
    NsRunEdge(K, Ctx, E);

    E.Face = ENsFace::PlusX;
    E.Line = MainWestLine;
    E.Apron = MainApron;
    E.From = MainRoad.Y0;
    E.To = ParaSouthLine;
    E.Salt = 13;
    NsRunEdge(K, Ctx, E);

    E.From = ParaNorthLine;
    E.To = MainRoad.Y1;
    E.Salt = 14;
    NsRunEdge(K, Ctx, E);

    E.Face = ENsFace::MinusX;
    E.Line = MainEastLine;
    E.From = MainRoad.Y0;
    E.To = MainRoad.Y1;
    E.Salt = 15;
    NsRunEdge(K, Ctx, E);

    // The cross street: buildings stand against its footpaths with no apron, from the lane's reserved massing to the para row.
    E.Apron = 0.0;
    E.From = LaneMass.Y1;
    E.To = ParaSouthLine;
    E.Face = ENsFace::PlusX;
    E.Line = CrossWest.X0;
    E.Salt = 16;
    NsRunEdge(K, Ctx, E);
    E.Face = ENsFace::MinusX;
    E.Line = CrossEast.X1;
    E.Salt = 17;
    NsRunEdge(K, Ctx, E);

    // The two corners where an apron meets the main road's footpath have no lot: pave them.
    NsEmitPaving(K, Ctx, FNsRect(MainWestLine, ParaSouthLine, MainWestSouth.X0, ParaSouthWest.Y0));
    NsEmitPaving(K, Ctx, FNsRect(MainWestLine, ParaNorth.Y1, MainWestNorth.X0, ParaNorthLine));

    // Second rows: taller medium-detail blocks seen over the first row's roofs, giving the skyline back.
    FNsEdge B;
    B.bFull = false;
    B.bSeal = false;
    B.MinStoreys = 5;
    B.StoreyRange = 3;
    B.Depth = 800.0;
    B.Face = ENsFace::PlusY;
    B.Line = ParaSouthLine - 1000.0 - 100.0;
    B.From = ParaFrom;
    B.To = MainWestLine;
    B.Salt = 21;
    NsRunEdge(K, Ctx, B);
    B.Face = ENsFace::MinusY;
    B.Line = ParaNorthLine + 1000.0 + 100.0;
    B.Depth = 900.0;
    B.Salt = 22;
    NsRunEdge(K, Ctx, B);
    B.Depth = 800.0;
    B.Face = ENsFace::PlusX;
    B.Line = MainWestLine - 1000.0 - 100.0;
    B.From = MainRoad.Y0;
    B.To = ParaSouthLine;
    B.Salt = 23;
    NsRunEdge(K, Ctx, B);
    B.From = ParaNorthLine;
    B.To = MainRoad.Y1;
    B.Salt = 24;
    NsRunEdge(K, Ctx, B);
    B.Face = ENsFace::MinusX;
    B.Line = MainEastLine + 1000.0 + 100.0;
    B.From = MainRoad.Y0;
    B.To = MainRoad.Y1;
    B.Salt = 25;
    NsRunEdge(K, Ctx, B);
}
} // namespace PWNs
