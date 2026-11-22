#include "PerfectWorldNeighbourhoodFacade.h"
#include "PerfectWorldNeighbourhoodProps.h"
#include "PerfectWorldNeighbourhoodRoads.h"
#include "PerfectWorldNeighbourhoodStreets.h"
#include "PerfectWorldNeighbourhoodText.h"

namespace PWNs
{
namespace
{
constexpr double NsClubHallHeight = 435.0; // storey height of the one-storey hall (roof slab at 450 above the road)

/** The club building: a one-storey hall facing the para road, closed, with a verandah, a name board and a flag. */
void NsClubHall(FNsSink& K, FNsContext& Ctx)
{
    const PerfectWorld::NeighbourhoodFeature* Hall = Ctx.Feature("building.nabapara.club");
    const PerfectWorld::NeighbourhoodFeature* Door = Ctx.Feature("entrance.nabapara.club_entrance");
    if (Hall == nullptr || Door == nullptr || Door->Geometry.empty())
    {
        return;
    }
    const FNsRect R(Hall->Bounds.X0, Hall->Bounds.Y0, Hall->Bounds.X1, Hall->Bounds.Y1);
    double W = 0.0;
    double D = 0.0;
    const FLaneFrame F = NsFaceFrame(R, ENsFace::MinusY, W, D);
    double Uc = 0.0;
    double Vc = 0.0;
    NsLocal(F, Door->Geometry[0].X, Door->Geometry[0].Y, Uc, Vc);

    FNsFacade S;
    S.Width = W;
    S.Depth = D;
    S.Storeys = 1;
    S.StoreyHeight = NsClubHallHeight;
    S.Seed = 9101u;
    S.Wall = ELaneColour::WallDustyBlue;
    S.Shutter = ELaneColour::ShutterBlue;
    const double H = S.RoofZ();

    K.Add(F, S.Wall, 0.0, W, 0.0, D, 0.0, H);
    K.Add(F, ELaneColour::Plinth, 0.0, W, -3.0, 0.0, PWLane::FootpathHeight, 90.0);

    // Closed double doors at the plan's entrance, a transom over them, and a step.
    K.Add(F, ELaneColour::TrimDark, Uc - 92.0, Uc + 92.0, -5.0, 0.0, PWLane::FootpathHeight, 252.0);
    K.Add(F, ELaneColour::DoorWood, Uc - 84.0, Uc + 84.0, -7.0, -5.0, PWLane::FootpathHeight, 238.0);
    K.Add(F, ELaneColour::TrimDark, Uc - 1.5, Uc + 1.5, -9.0, -7.0, PWLane::FootpathHeight, 238.0);
    K.Add(F, ELaneColour::GlassDusk, Uc - 84.0, Uc + 84.0, -7.0, -5.0, 240.0, 250.0);
    K.Add(F, ELaneColour::Concrete, Uc - 120.0, Uc + 120.0, -45.0, 0.0, 0.0, PWLane::FootpathHeight + 7.0);
    // A shuttered window each side.
    for (const double Side : {-1.0, 1.0})
    {
        const double Uw = Uc + Side * 300.0;
        K.Add(F, ELaneColour::TrimDark, Uw - 85.0, Uw + 85.0, -3.0, 0.0, 110.0, 262.0);
        K.Add(F, S.Shutter, Uw - 80.0, Uw + 80.0, -8.0, -3.0, 114.0, 258.0);
        K.Add(F, ELaneColour::ShutterStripe, Uw - 1.5, Uw + 1.5, -9.0, -8.0, 114.0, 258.0);
        for (int32 Slat = 1; Slat < 6; ++Slat)
        {
            const double Z = 114.0 + Slat * 24.0;
            K.Add(F, ELaneColour::ShutterStripe, Uw - 78.0, Uw + 78.0, -9.0, -8.0, Z - 0.8, Z + 0.8);
        }
        K.Add(F, ELaneColour::TrimLight, Uw - 95.0, Uw + 95.0, -12.0, -2.0, 100.0, 110.0);
    }
    // The verandah: a concrete canopy over the door on two posts that stand on the forecourt apron.
    K.Add(F, ELaneColour::Concrete, 40.0, W - 40.0, -170.0, 0.0, 292.0, 304.0);
    K.Add(F, ELaneColour::TrimDark, 40.0, W - 40.0, -172.0, -168.0, 280.0, 304.0);
    for (const double U : {70.0, W - 70.0})
    {
        K.Add(F, ELaneColour::Concrete, U - 10.0, U + 10.0, -160.0, -140.0, PWLane::FootpathHeight, 292.0);
        K.AddProp(F, U - 10.0, U + 10.0, -160.0, -140.0, PWLane::FootpathHeight, 292.0);
    }
    // The name board over the verandah, the founding year above it, and a flag on the roof.
    Ctx.SignCount += NsSignboard(K, F, 100.0, W - 100.0, 335.0, 405.0, ELaneColour::SignBlue, ELaneColour::TrimLight, "SABUJ SANGHA CLUB") > 0 ? 1 : 0;
    NsDrawText(K, F, ELaneColour::TrimLight, 0.5 * (W - NsTextWidth("EST 1974", 3.0)), 438.0, 3.0, -3.0, 0.0, "EST 1974");
    K.Add(F, ELaneColour::Rail, 70.0, 76.0, 60.0, 66.0, H, H + 300.0);
    K.Add(F, ELaneColour::SignRed, 76.0, 190.0, 61.0, 65.0, H + 230.0, H + 292.0);
    NsRoof(K, F, S, false);
}

/** Paving, compound walls and furniture of the club yard and the pandal ground: everything in the site but the hall. */
void NsClubGrounds(FNsSink& K, FNsContext& Ctx)
{
    const FNsRect Site = Ctx.Bounds("site.nabapara.club_pandal");
    const FNsRect Hall = Ctx.Bounds("building.nabapara.club");
    const FNsRect Pandal = Ctx.Bounds("space.nabapara.pandal");
    const FNsRect Reserve = Ctx.Bounds("reserve.nabapara.club_pandal");
    if (Site.IsEmpty() || Hall.IsEmpty() || Pandal.IsEmpty())
    {
        return;
    }
    // The paved ground at footpath level: the forecourt across the front, the west detour, the yard behind the hall and the pandal ground.
    NsEmitPaving(K, Ctx, FNsRect(Reserve.X0, Site.Y0 - 150.0, Reserve.X1, Site.Y0));            // forecourt, from the footpath apron
    NsEmitPaving(K, Ctx, FNsRect(Reserve.X0, Site.Y0, Hall.X0, Site.Y1));                      // the west detour
    NsEmitPaving(K, Ctx, FNsRect(Hall.X0, Hall.Y1, Hall.X1, Site.Y1));                         // the yard behind the hall
    NsEmitPaving(K, Ctx, FNsRect(Hall.X1, Site.Y0, Site.X1, Site.Y1));                         // the strip beside the hall and the pandal ground

    // Compound walls close the site on its west, north and east sides (the front stays open to the para road).
    constexpr double Thick = 30.0;
    NsCompoundWall(K, FNsRect(Reserve.X0, Reserve.Y0 + 150.0, Reserve.X0 + Thick, Site.Y1), 230.0, ELaneColour::WallTerracotta);
    NsCompoundWall(K, FNsRect(Reserve.X0, Site.Y1 - Thick, Reserve.X1, Site.Y1), 230.0, ELaneColour::WallTerracotta);
    NsCompoundWall(K, FNsRect(Reserve.X1 - Thick, Reserve.Y0 + 150.0, Reserve.X1, Site.Y1), 230.0, ELaneColour::WallTerracotta);
    Ctx.Walls.Add({FNsRect(Reserve.X0, Reserve.Y0 + 150.0, Reserve.X0 + Thick, Site.Y1), 230.0});
    Ctx.Walls.Add({FNsRect(Reserve.X0, Site.Y1 - Thick, Reserve.X1, Site.Y1), 230.0});
    Ctx.Walls.Add({FNsRect(Reserve.X1 - Thick, Reserve.Y0 + 150.0, Reserve.X1, Site.Y1), 230.0});

    // Benches in the yard behind the hall, clear of the route along the anchor's line (|Y - benches| > 80), and the notice board.
    const PerfectWorld::NeighbourhoodFeature* Benches = Ctx.Feature("anchor.nabapara.club_benches");
    const double RouteY = (Benches != nullptr && !Benches->Geometry.empty()) ? static_cast<double>(Benches->Geometry[0].Y) : Hall.Y1 + 250.0;
    const double X0 = Hall.X0;
    for (const double X : {X0 + 250.0, X0 + 750.0})
    {
        NsBench(K, X, RouteY - 160.0, 180.0, true, ENsFace::MinusY);
        NsBench(K, X, RouteY + 160.0, 180.0, true, ENsFace::PlusY);
    }
    NsNoticeBoard(K, X0 + 500.0, Site.Y1 - Thick - 8.0, 300.0, ENsFace::MinusY, 9102u);

    // The pandal ground is left open and swept; only its four corner bamboo marker posts stand, as the committee marks the plot.
    for (const double X : {Pandal.X0 + 40.0, Pandal.X1 - 60.0})
    {
        for (const double Y : {Pandal.Y0 + 60.0, Pandal.Y1 - 60.0})
        {
            K.Box(ELaneColour::WoodLight, X - 6.0, Y - 6.0, NsPath, X + 6.0, Y + 6.0, NsPath + 300.0);
            K.Prop(X - 6.0, Y - 6.0, NsPath, X + 6.0, Y + 6.0, NsPath + 300.0);
        }
    }
    NsTree(K, Hall.X1 + 60.0, Site.Y1 - 120.0, NsPath); // the neem tree in the corner of the yard
}
} // namespace

void NsBuildClubAndPandal(FNsSink& K, FNsContext& Ctx)
{
    NsClubHall(K, Ctx);
    NsClubGrounds(K, Ctx);
}
} // namespace PWNs
