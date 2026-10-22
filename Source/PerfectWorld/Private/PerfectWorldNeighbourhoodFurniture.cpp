#include "PerfectWorldNeighbourhoodFacade.h"
#include "PerfectWorldNeighbourhoodProps.h"
#include "PerfectWorldNeighbourhoodRoads.h"
#include "PerfectWorldNeighbourhoodStreets.h"

namespace PWNs
{
namespace
{
using NsKind = PerfectWorld::NeighbourhoodFeatureKind;
using NsLinkKind = PerfectWorld::NeighbourhoodLinkKind;

constexpr double NsKerbInset = 15.0;   // a post's centre stands this far in from the kerb edge
constexpr double NsRouteClear = 60.0;  // a post stays at least this far from every pedestrian route centre line
constexpr double NsPoleTop = 820.0;    // tram pole wire height above the road

double NsDistanceToSegment(double X, double Y, double AX, double AY, double BX, double BY)
{
    const double DX = BX - AX;
    const double DY = BY - AY;
    const double Len2 = DX * DX + DY * DY;
    const double T = Len2 <= 0.0 ? 0.0 : FMath::Clamp(((X - AX) * DX + (Y - AY) * DY) / Len2, 0.0, 1.0);
    const double PX = AX + T * DX - X;
    const double PY = AY + T * DY - Y;
    return FMath::Sqrt(PX * PX + PY * PY);
}

/** True when a post at (X, Y) keeps clear of every outdoor pedestrian route, ramp, crossing, building footprint and site. */
bool NsPostClear(const FNsContext& Ctx, double X, double Y)
{
    for (const PerfectWorld::NeighbourhoodLink& Link : Ctx.Plan.Links)
    {
        if (Link.Kind == NsLinkKind::Interior || Link.Kind == NsLinkKind::Stair)
        {
            continue;
        }
        for (std::size_t I = 1; I < Link.Points.size(); ++I)
        {
            if (NsDistanceToSegment(X, Y, Link.Points[I - 1].X, Link.Points[I - 1].Y, Link.Points[I].X, Link.Points[I].Y) < NsRouteClear)
            {
                return false;
            }
        }
    }
    const FNsRect Post(X - 40.0, Y - 40.0, X + 40.0, Y + 40.0);
    if (NsAnyOverlap(Ctx.Ramps, Post))
    {
        return false;
    }
    for (const PerfectWorld::NeighbourhoodFeature& F : Ctx.Plan.Features)
    {
        const bool bBlocks = F.Kind == NsKind::Crossing || F.Kind == NsKind::Footprint || F.Kind == NsKind::Site;
        if (bBlocks && FNsRect(F.Bounds.X0, F.Bounds.Y0, F.Bounds.X1, F.Bounds.Y1).Grown(F.Kind == NsKind::Crossing ? 160.0 : 0.0).Overlaps(Post))
        {
            return false;
        }
    }
    return true;
}

/** Stops the clear checks lamps and trees share also keeping out of the bus shelters and stop signs. */
bool NsClearOfList(const TArray<FNsRect>& Keep, double X, double Y)
{
    return !NsAnyOverlap(Keep, FNsRect(X - 60.0, Y - 60.0, X + 60.0, Y + 60.0));
}

// ---- Bus stops -----------------------------------------------------------------------------------------------------------------------

/** A bus shelter against the building line on the stop's own footpath: roof, back panel, bench, posts and a route board beside it. */
void NsBusStop(FNsSink& K, FNsContext& Ctx, const PerfectWorld::NeighbourhoodFeature& Stop, TArray<FNsRect>& Keep)
{
    if (Stop.Geometry.empty())
    {
        return;
    }
    const FNsRect Main = NsCarriageOf(Ctx, "street.nabapara.main");
    const double StopX = Stop.Geometry[0].X;
    const double StopY = Stop.Geometry[0].Y;
    const bool bWest = StopX < Main.CentreX();
    constexpr double Depth = 160.0;
    constexpr double Length = 300.0;
    // The shelter fills the apron and 60 cm of the footpath, leaving at least 190 cm clear along the path (the plan route runs on its centre).
    const FNsRect Path = bWest ? Ctx.Bounds("street.nabapara.main.footpath_left_north") : Ctx.Bounds("street.nabapara.main.footpath_right");
    const double Line = bWest ? Path.X0 - 100.0 : Path.X1 + 100.0; // the building line
    const FNsRect Shelter = bWest ? FNsRect(Line, StopY - 0.5 * Length, Line + Depth, StopY + 0.5 * Length)
                                  : FNsRect(Line - Depth, StopY - 0.5 * Length, Line, StopY + 0.5 * Length);
    double W = 0.0;
    double D = 0.0;
    const FLaneFrame F = NsFaceFrame(Shelter, bWest ? ENsFace::PlusX : ENsFace::MinusX, W, D);
    // Back panel (glass in a steel frame), bench, roof and posts.
    K.Add(F, ELaneColour::TrimDark, 0.0, W, D - 8.0, D, PWLane::FootpathHeight, 250.0);
    K.Add(F, ELaneColour::GlassDusk, 10.0, W - 10.0, D - 6.0, D - 2.0, 60.0, 240.0);
    K.AddProp(F, 0.0, W, D - 8.0, D, PWLane::FootpathHeight, 250.0);
    K.Add(F, ELaneColour::WoodLight, 30.0, W - 30.0, D - 60.0, D - 14.0, 45.0, 52.0);
    K.Add(F, ELaneColour::Rail, 34.0, 42.0, D - 56.0, D - 18.0, PWLane::FootpathHeight, 45.0);
    K.Add(F, ELaneColour::Rail, W - 42.0, W - 34.0, D - 56.0, D - 18.0, PWLane::FootpathHeight, 45.0);
    K.AddProp(F, 30.0, W - 30.0, D - 60.0, D - 14.0, PWLane::FootpathHeight, 52.0);
    K.Add(F, ELaneColour::ShutterSteel, -10.0, W + 10.0, -20.0, D, 262.0, 270.0);
    for (const double U : {8.0, W - 8.0})
    {
        K.Add(F, ELaneColour::Rail, U - 4.0, U + 4.0, 4.0, 12.0, PWLane::FootpathHeight, 262.0);
        K.AddProp(F, U - 4.0, U + 4.0, 4.0, 12.0, PWLane::FootpathHeight, 262.0);
    }
    FLaneFrame Fascia = F;
    Fascia.Origin = F.Pt(0.0, -20.0, 0.0);
    Ctx.SignCount += NsSignboard(K, Fascia, 30.0, W - 30.0, 230.0, 266.0, ELaneColour::SignBlue, ELaneColour::TrimLight, "BUS STOP") > 0 ? 1 : 0;
    Keep.Add(Shelter.Grown(60.0));

    // The route board: a post on the kerb edge, upstream of the shelter, with a two-sided panel of block lettering above head height.
    const double BoardY = StopY + 260.0;
    const double KerbX = bWest ? Path.X1 - NsKerbInset : Path.X0 + NsKerbInset;
    K.Box(ELaneColour::TrimDark, KerbX - 5.0, BoardY - 5.0, NsPath, KerbX + 5.0, BoardY + 5.0, NsPath + 300.0);
    K.Prop(KerbX - 6.0, BoardY - 6.0, NsPath, KerbX + 6.0, BoardY + 6.0, NsPath + 300.0);
    const double PanelX0 = bWest ? KerbX - 150.0 : KerbX - 10.0;
    const FNsRect Panel(PanelX0, BoardY - 4.0, PanelX0 + 160.0, BoardY + 4.0);
    K.Slab(ELaneColour::SignBlue, Panel, NsGround + 215.0, NsGround + 315.0);
    for (const ENsFace Side : {ENsFace::PlusY, ENsFace::MinusY})
    {
        double PW = 0.0;
        double PD = 0.0;
        const FLaneFrame PF = NsFaceFrame(Panel, Side, PW, PD);
        Ctx.SignCount += NsSignboard(K, PF, 6.0, PW - 6.0, 266.0, 310.0, ELaneColour::SignBlue, ELaneColour::TrimLight, "BUS STOP") > 0 ? 1 : 0;
        NsSignboard(K, PF, 6.0, PW - 6.0, 218.0, 262.0, ELaneColour::SignBlue, ELaneColour::GoodsYellow, bWest ? "BUS 3C/1" : "BUS S10");
    }
    Keep.Add(FNsRect(KerbX - 60.0, BoardY - 60.0, KerbX + 60.0, BoardY + 60.0));
}
} // namespace

void NsBuildStreetFurniture(FNsSink& K, FNsContext& Ctx)
{
    const FNsRect ParaRoad = NsCarriageOf(Ctx, "street.nabapara.para");
    const FNsRect MainRoad = NsCarriageOf(Ctx, "street.nabapara.main");
    const FNsRect CrossRoad = NsCarriageOf(Ctx, "street.nabapara.cross");
    const double TramX = NsTramCentreX(Ctx);

    TArray<FNsRect> Keep; // the bus shelters and boards: lamps, poles and trees keep out
    for (const PerfectWorld::NeighbourhoodFeature& F : Ctx.Plan.Features)
    {
        if (F.Kind == NsKind::BusStop)
        {
            NsBusStop(K, Ctx, F, Keep);
        }
    }

    // Para road: lamp posts down both kerbs, power poles with sagging wires on the south side, and trees on the north.
    const double SouthKerb = ParaRoad.Y0 - NsKerbInset;
    const double NorthKerb = ParaRoad.Y1 + NsKerbInset;
    const double ParaEnd = MainRoad.X0 - 400.0;
    double LastPoleX = 0.0;
    bool bHavePole = false;
    for (double X = ParaRoad.X0 + 400.0; X < ParaEnd; X += 1200.0)
    {
        const int32 Slot = FMath::RoundToInt((X - ParaRoad.X0) / 1200.0);
        const double Cx = X + 0.0;
        if (Slot % 2 == 0)
        {
            if (NsPostClear(Ctx, Cx, SouthKerb) && NsClearOfList(Keep, Cx, SouthKerb))
            {
                if (Slot % 6 == 0)
                {
                    NsPowerPole(K, Cx, SouthKerb, false, Slot % 12 == 0);
                    if (bHavePole)
                    {
                        for (const double Off : {-60.0, 0.0, 60.0})
                        {
                            const FVector From(LastPoleX, SouthKerb + Off, NsGround + 849.0);
                            const FVector To(Cx, SouthKerb + Off, NsGround + 849.0);
                            K.SaggingWire(ELaneColour::WireBlack, From, To, 0.035 * (To - From).Size(), 2.5, 10);
                        }
                    }
                    LastPoleX = Cx;
                    bHavePole = true;
                }
                else
                {
                    NsLampPost(K, Cx, SouthKerb, NsPath, ENsFace::PlusY);
                }
            }
        }
        else if (Slot % 4 == 1)
        {
            if (NsPostClear(Ctx, Cx, NorthKerb) && NsClearOfList(Keep, Cx, NorthKerb))
            {
                NsLampPost(K, Cx, NorthKerb, NsPath, ENsFace::MinusY);
            }
        }
        else if (Slot % 4 == 3)
        {
            if (NsPostClear(Ctx, Cx, NorthKerb) && NsClearOfList(Keep, Cx, NorthKerb))
            {
                NsTree(K, Cx, NorthKerb, NsPath);
            }
        }
    }

    // Main road: paired tram poles on both kerbs every 2600 cm carry a span wire across and a contact wire down the tram bed.
    const FNsRect WestPath = Ctx.Bounds("street.nabapara.main.footpath_left_north");
    const FNsRect EastPath = Ctx.Bounds("street.nabapara.main.footpath_right");
    const double WestX = WestPath.X1 - NsKerbInset;
    const double EastX = EastPath.X0 + NsKerbInset;
    double PreviousY = 0.0;
    bool bHaveSpan = false;
    for (double Y = MainRoad.Y0 + 600.0; Y < MainRoad.Y1 - 400.0; Y += 2600.0)
    {
        if (!NsPostClear(Ctx, WestX, Y) || !NsPostClear(Ctx, EastX, Y) || !NsClearOfList(Keep, WestX, Y) || !NsClearOfList(Keep, EastX, Y) ||
            (Y > ParaRoad.Y0 - 120.0 && Y < ParaRoad.Y1 + 120.0))
        {
            continue; // no pole pair here (a junction, crossing or shelter); the contact wire carries on to the next pair
        }
        NsPowerPole(K, WestX, Y, true, false);
        NsPowerPole(K, EastX, Y, true, false);
        K.Slab(ELaneColour::GlassLit, FNsRect(WestX + 40.0, Y - 14.0, WestX + 80.0, Y + 14.0), NsGround + 818.0, NsGround + 824.0);
        K.Slab(ELaneColour::GlassLit, FNsRect(EastX - 80.0, Y - 14.0, EastX - 40.0, Y + 14.0), NsGround + 818.0, NsGround + 824.0);
        const FVector WestTop(WestX, Y, NsGround + NsPoleTop);
        const FVector EastTop(EastX, Y, NsGround + NsPoleTop);
        K.SaggingWire(ELaneColour::WireBlack, WestTop, FVector(TramX, Y, NsGround + 640.0), 14.0, 2.2, 8);
        K.SaggingWire(ELaneColour::WireBlack, FVector(TramX, Y, NsGround + 640.0), EastTop, 14.0, 2.2, 8);
        K.Wire(ELaneColour::WireBlack, FVector(TramX, Y, NsGround + 640.0), FVector(TramX, Y, NsGround + 600.0), 1.8); // the dropper
        if (bHaveSpan)
        {
            K.SaggingWire(ELaneColour::WireBlack, FVector(TramX, PreviousY, NsGround + 600.0), FVector(TramX, Y, NsGround + 600.0), 16.0, 2.5, 10);
        }
        PreviousY = Y;
        bHaveSpan = true;
    }
    // Trees along the main road's kerbs between the pole pairs.
    for (double Y = MainRoad.Y0 + 1900.0; Y < MainRoad.Y1 - 400.0; Y += 5200.0)
    {
        for (const double Side : {-1.0, 1.0})
        {
            const double X = Side < 0.0 ? WestX : EastX;
            if (NsPostClear(Ctx, X, Y) && NsClearOfList(Keep, X, Y) && !(Y > ParaRoad.Y0 - 200.0 && Y < ParaRoad.Y1 + 200.0))
            {
                NsTree(K, X, Y, NsPath);
            }
        }
    }
    (void)CrossRoad;
}
} // namespace PWNs
