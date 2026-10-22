#include "PerfectWorldNeighbourhoodRoads.h"

namespace PWNs
{
namespace
{
using NsKind = PerfectWorld::NeighbourhoodFeatureKind;

/** Paint over the road, skipping anything in Avoid. */
void NsPaint(FNsSink& K, const FNsRect& R, ELaneColour Colour, const TArray<FNsRect>& Avoid)
{
    for (const FNsRect& Piece : NsSubtract(R, Avoid))
    {
        K.Slab(Colour, Piece, NsGround, NsGround + NsMarkingHeight);
    }
}

/** A thin line between two plan points (axis aligned), Width cm wide. */
FNsRect NsLineRect(const PerfectWorld::NeighbourhoodFeature& F, double Width)
{
    const auto& A = F.Geometry[0];
    const auto& B = F.Geometry[F.Geometry.size() - 1];
    const double HalfW = 0.5 * Width;
    if (A.X == B.X)
    {
        return FNsRect(A.X - HalfW, FMath::Min(A.Y, B.Y), A.X + HalfW, FMath::Max(A.Y, B.Y));
    }
    return FNsRect(FMath::Min(A.X, B.X), A.Y - HalfW, FMath::Max(A.X, B.X), A.Y + HalfW);
}

/** Zebra bars of one crossing: long along the traffic, spaced along the walk, clear of the tram strip and of any ramp on the road. */
void NsZebra(FNsSink& K, const FNsCrossing& C, const TArray<FNsRect>& Avoid)
{
    constexpr double Bar = 40.0;
    constexpr double Pitch = 80.0;
    const FNsRect& R = C.Carriage;
    if (C.bStreetAlongX)
    {
        for (double Y = R.Y0 + 50.0; Y + Bar <= R.Y1 - 10.0; Y += Pitch)
        {
            NsPaint(K, FNsRect(C.Cross.X0 + 10.0, Y, C.Cross.X1 - 10.0, Y + Bar), ELaneColour::TrimLight, Avoid);
        }
    }
    else
    {
        for (double X = R.X0 + 50.0; X + Bar <= R.X1 - 10.0; X += Pitch)
        {
            NsPaint(K, FNsRect(X, C.Cross.Y0 + 10.0, X + Bar, C.Cross.Y1 - 10.0), ELaneColour::TrimLight, Avoid);
        }
    }
}

/** A direction arrow: a shaft and a four-step head, pointing along the unit axis (DirX, DirY). Left-running traffic is drawn by these. */
void NsArrow(FNsSink& K, double X, double Y, double DirX, double DirY, const TArray<FNsRect>& Avoid)
{
    const auto Place = [&](double A0, double A1, double HalfB)
    {
        const double Ax0 = X + DirX * A0;
        const double Ay0 = Y + DirY * A0;
        const double Ax1 = X + DirX * A1;
        const double Ay1 = Y + DirY * A1;
        const double Bx = DirY != 0.0 ? HalfB : 0.0;
        const double By = DirX != 0.0 ? HalfB : 0.0;
        const FNsRect R(FMath::Min(Ax0, Ax1) - Bx, FMath::Min(Ay0, Ay1) - By, FMath::Max(Ax0, Ax1) + Bx, FMath::Max(Ay0, Ay1) + By);
        NsPaint(K, R, ELaneColour::TrimLight, Avoid);
    };
    Place(-100.0, 50.0, 12.5);
    Place(50.0, 75.0, 45.0);
    Place(75.0, 100.0, 30.0);
    Place(100.0, 125.0, 15.0);
}

/** A dashed line along X (bAlongX) or Y at Fixed, 300 cm dashes every 600 cm, from From to To. */
void NsDashes(FNsSink& K, bool bAlongX, double Fixed, double From, double To, const TArray<FNsRect>& Avoid)
{
    for (double S = From; S + 300.0 <= To; S += 600.0)
    {
        const FNsRect Dash = bAlongX ? FNsRect(S, Fixed - 6.0, S + 300.0, Fixed + 6.0) : FNsRect(Fixed - 6.0, S, Fixed + 6.0, S + 300.0);
        NsPaint(K, Dash, ELaneColour::TrimLight, Avoid);
    }
}
} // namespace

void NsBuildMarkings(FNsSink& K, FNsContext& Ctx)
{
    const FNsRect CrossRoad = NsCarriageOf(Ctx, "street.nabapara.cross");
    const FNsRect ParaRoad = NsCarriageOf(Ctx, "street.nabapara.para");
    const FNsRect MainRoad = NsCarriageOf(Ctx, "street.nabapara.main");
    const TArray<FNsCrossing> Crossings = NsCollectCrossings(Ctx);
    const double TramX = NsTramCentreX(Ctx);
    const FNsRect Tram(TramX - NsTramHalfWidth, MainRoad.Y0, TramX + NsTramHalfWidth, MainRoad.Y1);

    // The fictional tram corridor: a slightly raised concrete bed with two steel rails (standard gauge, 143.5 cm).
    K.Slab(ELaneColour::Concrete, Tram, NsGround, NsGround + 2.0);
    for (const double Side : {-1.0, 1.0})
    {
        const double Rail = TramX + Side * 71.75;
        K.Slab(ELaneColour::Chrome, FNsRect(Rail - 3.0, MainRoad.Y0, Rail + 3.0, MainRoad.Y1), NsGround + 2.0, NsGround + 4.5);
    }

    TArray<FNsRect> Avoid = Ctx.Ramps;
    Avoid.Add(Tram);
    for (const FNsCrossing& C : Crossings)
    {
        NsZebra(K, C, Avoid);
    }

    // Stop lines and bus bays go first so dashes and arrows keep clear of them.
    TArray<FNsRect> Stops;
    for (const PerfectWorld::NeighbourhoodFeature& F : Ctx.Plan.Features)
    {
        if (F.Kind == NsKind::StopLine && F.Geometry.size() >= 2)
        {
            const FNsRect Line = NsLineRect(F, 30.0);
            NsPaint(K, Line, ELaneColour::TrimLight, Avoid);
            Stops.Add(Line.Grown(60.0));
        }
    }
    for (const PerfectWorld::NeighbourhoodFeature& F : Ctx.Plan.Features)
    {
        if (F.Kind != NsKind::BusStop || F.Geometry.empty())
        {
            continue;
        }
        // A yellow bay on the carriageway edge beside the stop, on the stop's own side of the road.
        const double Y = F.Geometry[0].Y;
        const bool bWest = F.Geometry[0].X < MainRoad.CentreX();
        const double Inner = bWest ? MainRoad.X0 + 300.0 : MainRoad.X1 - 300.0;
        const double Edge = bWest ? MainRoad.X0 : MainRoad.X1;
        TArray<FNsRect> BayAvoid = Avoid;
        for (const FNsCrossing& C : Crossings)
        {
            BayAvoid.Add(C.Cross.Grown(40.0));
        }
        NsPaint(K, FNsRect(FMath::Min(Inner, Edge), Y - 250.0, FMath::Max(Inner, Edge), Y - 235.0), ELaneColour::GoodsYellow, BayAvoid);
        NsPaint(K, FNsRect(FMath::Min(Inner, Edge), Y + 235.0, FMath::Max(Inner, Edge), Y + 250.0), ELaneColour::GoodsYellow, BayAvoid);
        NsPaint(K, FNsRect(Inner - 7.5, Y - 250.0, Inner + 7.5, Y + 250.0), ELaneColour::GoodsYellow, BayAvoid);
    }

    TArray<FNsRect> Clear = Avoid;
    Clear.Append(Stops);
    for (const FNsCrossing& C : Crossings)
    {
        Clear.Add(C.Cross.Grown(70.0));
    }

    // The para road: a dashed centre line, with a left-running arrow in each direction.
    TArray<FNsRect> ParaClear = Clear;
    ParaClear.Add(CrossRoad.Grown(110.0));
    ParaClear.Add(MainRoad.Grown(60.0));
    const double ParaCentre = ParaRoad.CentreY();
    NsDashes(K, true, ParaCentre, ParaRoad.X0 + 150.0, MainRoad.X0 - 150.0, ParaClear);
    for (const PerfectWorld::NeighbourhoodFeature& F : Ctx.Plan.Features)
    {
        if (F.Kind != NsKind::VehicleLane || F.ParentId != "street.nabapara.para" || F.Geometry.size() < 2)
        {
            continue;
        }
        const double Dir = F.Geometry[1].X > F.Geometry[0].X ? 1.0 : -1.0;
        const double Y = F.Geometry[0].Y;
        for (double X = ParaRoad.X0 + 1500.0; X < MainRoad.X0 - 800.0; X += 2600.0)
        {
            const FNsRect Footprint(X - 130.0, Y - 50.0, X + 130.0, Y + 50.0);
            if (!NsAnyOverlap(ParaClear, Footprint))
            {
                NsArrow(K, X, Y, Dir, 0.0, ParaClear);
            }
        }
    }

    // The main road: a dashed divider in each traffic half beside the tram bed, and arrows in the outer lanes.
    TArray<FNsRect> MainClear = Clear;
    MainClear.Add(ParaRoad.Grown(120.0));
    for (const bool bWestHalf : {true, false})
    {
        const double Outer = bWestHalf ? MainRoad.X0 : MainRoad.X1;
        const double Inner = bWestHalf ? Tram.X0 : Tram.X1;
        const double LaneX = 0.5 * (Outer + Inner);
        NsDashes(K, false, LaneX, MainRoad.Y0 + 150.0, MainRoad.Y1 - 150.0, MainClear);
        const double Dir = bWestHalf ? 1.0 : -1.0; // northbound in the -X half, southbound in the +X half (left-running)
        for (double Y = MainRoad.Y0 + 1300.0; Y < MainRoad.Y1 - 800.0; Y += 3000.0)
        {
            for (const double Offset : {-0.5, 0.5})
            {
                const double X = LaneX + Offset * (FMath::Abs(Inner - Outer) * 0.5);
                const FNsRect Footprint(X - 50.0, Y - 130.0, X + 50.0, Y + 130.0);
                if (!NsAnyOverlap(MainClear, Footprint))
                {
                    NsArrow(K, X, Y, 0.0, Dir, MainClear);
                }
            }
        }
    }
}
} // namespace PWNs
