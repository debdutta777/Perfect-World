#include "PerfectWorldNeighbourhoodRoads.h"

namespace PWNs
{
namespace
{
using NsKind = PerfectWorld::NeighbourhoodFeatureKind;

constexpr double NsRampLength = 100.0; // 15 cm of rise in 100 cm of run: a kerb ramp, not a step
constexpr double NsKerbWidth = 12.0;
constexpr double NsDrainWidth = 25.0;

/** Which way a ramp falls from the footpath toward the carriageway. */
enum class ENsFall : uint8
{
    PlusX,
    MinusX,
    PlusY,
    MinusY
};

struct FNsRampSpec
{
    FNsRect Cut;
    ENsFall Fall = ENsFall::PlusX;
    bool bOnRoad = false; // no footpath of ours here (the lane's own paths): the ramp rises out of the road instead
};

FNsRect NsFeatureRect(const PerfectWorld::NeighbourhoodRect& R)
{
    return FNsRect(R.X0, R.Y0, R.X1, R.Y1);
}

/** A sloped slab: its top runs from ZHigh at the high end of Cut to ZLow at the low end, in the direction Fall. */
void NsRamp(FNsSink& K, const FNsRect& Cut, ENsFall Fall, double ZHigh, double ZLow)
{
    const bool bAlongX = Fall == ENsFall::PlusX || Fall == ENsFall::MinusX;
    const double Run = bAlongX ? Cut.Width() : Cut.Height();
    const double Across = bAlongX ? Cut.Height() : Cut.Width();
    const double Rise = ZHigh - ZLow;
    const double Slope = FMath::Sqrt(Run * Run + Rise * Rise);
    constexpr double Thickness = 20.0;
    const double AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Rise, Run));
    const double Sign = (Fall == ENsFall::PlusX || Fall == ENsFall::PlusY) ? 1.0 : -1.0;
    // The normal leans toward the downhill side; the slab sits half its thickness under its top surface.
    const double Shift = -0.5 * Thickness * (Rise / Slope) * Sign;
    const double ZShift = -0.5 * Thickness * (Run / Slope);
    const double Yaw = Fall == ENsFall::PlusX ? 0.0 : (Fall == ENsFall::PlusY ? 90.0 : (Fall == ENsFall::MinusX ? 180.0 : -90.0));
    FVector Centre(Cut.CentreX(), Cut.CentreY(), 0.5 * (ZHigh + ZLow) + ZShift);
    if (bAlongX)
    {
        Centre.X += Shift;
    }
    else
    {
        Centre.Y += Shift;
    }
    K.Tilted(ELaneColour::Footpath, Centre, FVector(Slope, Across, Thickness), FRotator(-AngleDeg, Yaw, 0.0));
}

/** Whichever edge of a path rectangle meets the carriageway: PlusY means the road lies on its +Y side, and so on. */
bool NsRoadEdge(const FNsRect& Path, const FNsRect& Road, ENsFace& OutEdge)
{
    constexpr double Tol = 1.0;
    if (FMath::Abs(Path.Y1 - Road.Y0) < Tol && Path.X0 < Road.X1 && Path.X1 > Road.X0)
    {
        OutEdge = ENsFace::PlusY;
        return true;
    }
    if (FMath::Abs(Path.Y0 - Road.Y1) < Tol && Path.X0 < Road.X1 && Path.X1 > Road.X0)
    {
        OutEdge = ENsFace::MinusY;
        return true;
    }
    if (FMath::Abs(Path.X1 - Road.X0) < Tol && Path.Y0 < Road.Y1 && Path.Y1 > Road.Y0)
    {
        OutEdge = ENsFace::PlusX;
        return true;
    }
    if (FMath::Abs(Path.X0 - Road.X1) < Tol && Path.Y0 < Road.Y1 && Path.Y1 > Road.Y0)
    {
        OutEdge = ENsFace::MinusX;
        return true;
    }
    return false;
}

FNsRect NsKerbStrip(const FNsRect& Path, ENsFace Edge)
{
    switch (Edge)
    {
    case ENsFace::PlusY:
        return FNsRect(Path.X0, Path.Y1 - NsKerbWidth, Path.X1, Path.Y1);
    case ENsFace::MinusY:
        return FNsRect(Path.X0, Path.Y0, Path.X1, Path.Y0 + NsKerbWidth);
    case ENsFace::PlusX:
        return FNsRect(Path.X1 - NsKerbWidth, Path.Y0, Path.X1, Path.Y1);
    case ENsFace::MinusX:
        return FNsRect(Path.X0, Path.Y0, Path.X0 + NsKerbWidth, Path.Y1);
    }
    return Path;
}

/** The ramp on each side of a crossing: on the footpath where one of ours is, on the road where only the lane's own path is. */
TArray<FNsRampSpec> NsRampsOf(const FNsCrossing& C, const TArray<FNsRect>& PathRects)
{
    struct FSide
    {
        FNsRect FootCut;
        FNsRect RoadCut;
        ENsFall FootFall;
        ENsFall RoadFall;
    };
    const FNsRect& X = C.Cross;
    const FNsRect& R = C.Carriage;
    const double L = NsRampLength;
    TArray<FSide> Sides;
    if (C.bStreetAlongX)
    {
        Sides.Add({FNsRect(X.X0, R.Y0 - L, X.X1, R.Y0), FNsRect(X.X0, R.Y0, X.X1, R.Y0 + L), ENsFall::PlusY, ENsFall::MinusY});
        Sides.Add({FNsRect(X.X0, R.Y1, X.X1, R.Y1 + L), FNsRect(X.X0, R.Y1 - L, X.X1, R.Y1), ENsFall::MinusY, ENsFall::PlusY});
    }
    else
    {
        Sides.Add({FNsRect(R.X0 - L, X.Y0, R.X0, X.Y1), FNsRect(R.X0, X.Y0, R.X0 + L, X.Y1), ENsFall::PlusX, ENsFall::MinusX});
        Sides.Add({FNsRect(R.X1, X.Y0, R.X1 + L, X.Y1), FNsRect(R.X1 - L, X.Y0, R.X1, X.Y1), ENsFall::MinusX, ENsFall::PlusX});
    }
    TArray<FNsRampSpec> Out;
    for (const FSide& S : Sides)
    {
        FNsRampSpec Spec;
        Spec.bOnRoad = !NsAnyOverlap(PathRects, S.FootCut);
        Spec.Cut = Spec.bOnRoad ? S.RoadCut : S.FootCut;
        Spec.Fall = Spec.bOnRoad ? S.RoadFall : S.FootFall;
        Out.Add(Spec);
    }
    return Out;
}

/** Open drains along both kerbs of the new carriageways, broken at junction mouths and crossings. */
void NsDrains(FNsSink& K, const FNsContext& Ctx, const TArray<FNsRect>& Carriageways, const TArray<FNsCrossing>& Crossings)
{
    for (int32 I = 0; I < Carriageways.Num(); ++I)
    {
        const FNsRect& R = Carriageways[I];
        TArray<FNsRect> Cuts;
        for (int32 J = 0; J < Carriageways.Num(); ++J)
        {
            if (J != I)
            {
                Cuts.Add(Carriageways[J].Grown(30.0)); // the mouths of the streets that join this one
            }
        }
        for (const FNsCrossing& C : Crossings)
        {
            Cuts.Add(C.Cross.Grown(25.0));
        }
        Cuts.Append(Ctx.Ramps);
        TArray<FNsRect> Strips;
        if (R.Width() > R.Height())
        {
            Strips.Add(FNsRect(R.X0, R.Y0, R.X1, R.Y0 + NsDrainWidth));
            Strips.Add(FNsRect(R.X0, R.Y1 - NsDrainWidth, R.X1, R.Y1));
        }
        else
        {
            Strips.Add(FNsRect(R.X0, R.Y0, R.X0 + NsDrainWidth, R.Y1));
            Strips.Add(FNsRect(R.X1 - NsDrainWidth, R.Y0, R.X1, R.Y1));
        }
        for (const FNsRect& Strip : Strips)
        {
            for (const FNsRect& Piece : NsSubtract(Strip, Cuts))
            {
                K.Slab(ELaneColour::Drain, Piece, NsGround, NsGround + 2.0);
            }
        }
    }
}

/** Worn tarmac patches and manhole covers scattered by the lane's hash, clear of every marking, ramp and junction. */
void NsPatches(FNsSink& K, const TArray<FNsRect>& Carriageways, const TArray<FNsRect>& Avoid)
{
    for (int32 I = 0; I < Carriageways.Num(); ++I)
    {
        const FNsRect& R = Carriageways[I];
        if (R.Width() < 600.0 && R.Height() < 600.0)
        {
            continue; // the narrow cross street stays clean
        }
        const bool bAlongX = R.Width() > R.Height();
        const double Length = bAlongX ? R.Width() : R.Height();
        const int32 Count = FMath::Clamp(static_cast<int32>(Length / 1400.0), 4, 14);
        for (int32 N = 0; N < Count; ++N)
        {
            const uint32 H = PWLane::LaneHash(7200 + I, N);
            const double Along = 400.0 + PWLane::LaneUnit(H) * (Length - 800.0);
            const double Across = (PWLane::LaneUnit(H >> 8) - 0.5) * (bAlongX ? R.Height() : R.Width()) * 0.6;
            const double Sa = 160.0 + PWLane::LaneUnit(H >> 14) * 260.0;
            const double Sb = 80.0 + PWLane::LaneUnit(H >> 18) * 160.0;
            const double Cx = bAlongX ? R.X0 + Along : R.CentreX() + Across;
            const double Cy = bAlongX ? R.CentreY() + Across : R.Y0 + Along;
            const FNsRect Patch(Cx - (bAlongX ? Sa : Sb) * 0.5, Cy - (bAlongX ? Sb : Sa) * 0.5, Cx + (bAlongX ? Sa : Sb) * 0.5,
                                Cy + (bAlongX ? Sb : Sa) * 0.5);
            bool bClear = !NsAnyOverlap(Avoid, Patch);
            for (int32 J = 0; J < Carriageways.Num() && bClear; ++J)
            {
                bClear = J == I || !Carriageways[J].Grown(140.0).Overlaps(Patch); // keep clear of the junction mouths
            }
            if (bClear)
            {
                K.Slab(((H >> 22) % 5u == 0u) ? ELaneColour::Drain : ELaneColour::RoadPatch, Patch, NsGround, NsGround + 1.0);
            }
        }
    }
}
} // namespace

bool NsAnyOverlap(const TArray<FNsRect>& Rects, const FNsRect& R)
{
    for (const FNsRect& P : Rects)
    {
        if (P.Overlaps(R))
        {
            return true;
        }
    }
    return false;
}

TArray<FNsCrossing> NsCollectCrossings(const FNsContext& Ctx)
{
    TArray<FNsCrossing> Out;
    for (const PerfectWorld::NeighbourhoodFeature& F : Ctx.Plan.Features)
    {
        if (F.Kind != NsKind::Crossing)
        {
            continue;
        }
        for (const PerfectWorld::NeighbourhoodFeature& G : Ctx.Plan.Features)
        {
            if (G.Kind == NsKind::Carriageway && G.ParentId == F.ParentId)
            {
                FNsCrossing C;
                C.Cross = NsFeatureRect(F.Bounds);
                C.Carriage = NsFeatureRect(G.Bounds);
                C.bStreetAlongX = C.Carriage.Width() > C.Carriage.Height();
                Out.Add(C);
                break;
            }
        }
    }
    return Out;
}

FNsRect NsCarriageOf(const FNsContext& Ctx, const char* StreetId)
{
    for (const PerfectWorld::NeighbourhoodFeature& F : Ctx.Plan.Features)
    {
        if (F.Kind == NsKind::Carriageway && F.ParentId == StreetId)
        {
            return NsFeatureRect(F.Bounds);
        }
    }
    return FNsRect();
}

double NsTramCentreX(const FNsContext& Ctx)
{
    return NsCarriageOf(Ctx, "street.nabapara.main").CentreX();
}

void NsEmitPaving(FNsSink& K, FNsContext& Ctx, const FNsRect& R)
{
    TArray<FNsRect> Cuts = Ctx.Surfaces;
    Cuts.Append(Ctx.Ramps);
    Cuts.Append(Ctx.Roads);
    Cuts.Add(NsCarriageOf(Ctx, "street.nabapara.lane"));
    for (const FNsRect& Piece : NsSubtract(R, Cuts))
    {
        K.Slab(ELaneColour::Footpath, Piece, NsGround - 20.0, NsPath);
        Ctx.Surfaces.Add(Piece);
    }
}

void NsBuildRoads(FNsSink& K, FNsContext& Ctx)
{
    const FNsRect LaneRoad = NsCarriageOf(Ctx, "street.nabapara.lane");
    const FNsRect CrossRoad = NsCarriageOf(Ctx, "street.nabapara.cross");
    const FNsRect ParaRoad = NsCarriageOf(Ctx, "street.nabapara.para");
    const FNsRect MainRoad = NsCarriageOf(Ctx, "street.nabapara.main");

    // Carriageways. The lane's own asphalt already exists, so each new one is trimmed against it and against the others
    // (the para road ends inside the main road's width; the cross street ends inside the para road's).
    TArray<FNsRect> Planned;
    Planned.Add(MainRoad);
    Planned.Add(ParaRoad);
    Planned.Add(CrossRoad);
    TArray<FNsRect> Done;
    Done.Add(LaneRoad);
    for (const FNsRect& R : Planned)
    {
        for (const FNsRect& Piece : NsSubtract(R, Done))
        {
            K.Slab(ELaneColour::Asphalt, Piece, NsGround - 20.0, NsGround);
            Ctx.Roads.Add(Piece);
        }
        Done.Add(R);
    }
    // The cross street meets the lane's +Y drain strip (the lane draws it along its whole road edge). Cover it across the
    // mouth so the junction reads as one surface; the 3 cm lip is far below the 45 cm step the walking player clears.
    K.Slab(ELaneColour::Asphalt, FNsRect(CrossRoad.X0, LaneRoad.Y1 - NsDrainWidth, CrossRoad.X1, LaneRoad.Y1), NsGround, NsGround + 3.0);

    // Footpaths of the new streets with the edge that meets each carriageway (the lane's own paths are the buildings').
    TArray<FNsRect> PathRects;
    TArray<ENsFace> PathEdges;
    TArray<bool> PathHasEdge;
    for (const PerfectWorld::NeighbourhoodFeature& F : Ctx.Plan.Features)
    {
        if (F.Kind != NsKind::Footpath || F.ParentId == "street.nabapara.lane")
        {
            continue;
        }
        const FNsRect Rect = NsFeatureRect(F.Bounds);
        const FNsRect Road = NsCarriageOf(Ctx, F.ParentId.c_str());
        ENsFace Edge = ENsFace::PlusY;
        PathRects.Add(Rect);
        PathHasEdge.Add(NsRoadEdge(Rect, Road, Edge));
        PathEdges.Add(Edge);
    }

    // Crossings: cut the footpath and lay the ramp (or raise a ramp from the road where the lane's own footpath is).
    const TArray<FNsCrossing> Crossings = NsCollectCrossings(Ctx);
    for (const FNsCrossing& C : Crossings)
    {
        for (const FNsRampSpec& Spec : NsRampsOf(C, PathRects))
        {
            NsRamp(K, Spec.Cut, Spec.Fall, NsPath, NsGround);
            Ctx.Ramps.Add(Spec.Cut);
        }
    }

    // Surfaces with their kerbs. Kerb stones are decoration (the 15 cm edge itself is the footpath box's).
    for (int32 I = 0; I < PathRects.Num(); ++I)
    {
        NsEmitPaving(K, Ctx, PathRects[I]);
        if (!PathHasEdge[I])
        {
            continue;
        }
        const FNsRect Kerb = NsKerbStrip(PathRects[I], PathEdges[I]);
        for (const FNsRect& Piece : NsSubtract(Kerb, Ctx.Ramps))
        {
            K.Slab(ELaneColour::Concrete, Piece, NsGround - 20.0, NsPath + 2.0);
        }
    }

    TArray<FNsRect> Carriageways;
    Carriageways.Add(MainRoad);
    Carriageways.Add(ParaRoad);
    Carriageways.Add(CrossRoad);
    NsDrains(K, Ctx, Carriageways, Crossings);

    TArray<FNsRect> Avoid = Ctx.Ramps;
    for (const FNsCrossing& C : Crossings)
    {
        Avoid.Add(C.Cross.Grown(120.0));
    }
    Avoid.Add(FNsRect(NsTramCentreX(Ctx) - NsTramHalfWidth - 20.0, MainRoad.Y0, NsTramCentreX(Ctx) + NsTramHalfWidth + 20.0, MainRoad.Y1));
    NsPatches(K, Carriageways, Avoid);

    NsBuildMarkings(K, Ctx);
}
} // namespace PWNs
