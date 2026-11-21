#include "PerfectWorldNeighbourhoodFacade.h"
#include "PerfectWorldNeighbourhoodStreets.h"

namespace PWNs
{
namespace
{
constexpr double NsSideWall = 14.0;

/** A frame whose origin is moved V cm into the building (so fascia lettering can be drawn on a roof's front edge). */
FLaneFrame NsShifted(const FLaneFrame& F, double V)
{
    FLaneFrame Out = F;
    Out.Origin = F.Pt(0.0, V, 0.0);
    return Out;
}

/** A pitch's three painted-sheet walls and its level floor; the front stays open to the street. */
void NsPitchShell(FNsSink& K, const FLaneFrame& F, double W, double D, double Height, ELaneColour Wall)
{
    K.Add(F, Wall, 0.0, NsSideWall, 0.0, D, 0.0, Height);
    K.Add(F, Wall, W - NsSideWall, W, 0.0, D, 0.0, Height);
    K.Add(F, Wall, NsSideWall, W - NsSideWall, D - NsSideWall, D, 0.0, Height);
    K.Add(F, ELaneColour::Footpath, NsSideWall, W - NsSideWall, 0.0, D - NsSideWall, 0.0, PWLane::FootpathHeight);
    K.Add(F, ELaneColour::FloorTile, NsSideWall, W - NsSideWall, 0.0, D - NsSideWall, PWLane::FootpathHeight, PWLane::FootpathHeight + 1.0);
}

/** A front counter of Depth cm, Height cm tall, with a lighter top; collision hugs it. The anchor stands behind it. */
void NsCounter(FNsSink& K, const FLaneFrame& F, double U0, double U1, double Depth, double Height)
{
    K.Add(F, ELaneColour::WoodLight, U0, U1, 6.0, Depth, PWLane::FootpathHeight, Height);
    K.Add(F, ELaneColour::TrimLight, U0 - 3.0, U1 + 3.0, 2.0, Depth + 4.0, Height, Height + 4.0);
    K.AddProp(F, U0, U1, 6.0, Depth, PWLane::FootpathHeight, Height);
    K.AddProp(F, U0 - 3.0, U1 + 3.0, 2.0, Depth + 4.0, Height, Height + 4.0);
}

/** A roof of the given colour over [U0, U1] x [V0, V1] at height Z, with two front posts that carry invisible collision. */
void NsRoofAndPosts(FNsSink& K, const FLaneFrame& F, double U0, double U1, double V0, double V1, double Z, ELaneColour Cloth,
                    double PostV)
{
    K.Add(F, Cloth, U0, U1, V0, V1, Z, Z + 8.0);
    K.Add(F, ELaneColour::TrimDark, U0, U1, V0 - 2.0, V0 + 2.0, Z - 14.0, Z + 8.0); // front valance
    for (const double U : {U0 + 12.0, U1 - 12.0})
    {
        K.Add(F, ELaneColour::WoodLight, U - 6.0, U + 6.0, PostV - 6.0, PostV + 6.0, PWLane::FootpathHeight, Z);
        K.AddProp(F, U - 6.0, U + 6.0, PostV - 6.0, PostV + 6.0, PWLane::FootpathHeight, Z);
    }
}

void NsTeaStall(FNsSink& K, FNsContext& Ctx, const PerfectWorld::NeighbourhoodFeature& Pitch, const PerfectWorld::NeighbourhoodFeature& Anchor)
{
    const FNsRect R(Pitch.Bounds.X0, Pitch.Bounds.Y0, Pitch.Bounds.X1, Pitch.Bounds.Y1);
    double W = 0.0;
    double D = 0.0;
    const FLaneFrame F = NsFaceFrame(R, ENsFace::PlusY, W, D);
    double Ua = 0.0;
    double Va = 0.0;
    NsLocal(F, Anchor.Geometry[0].X, Anchor.Geometry[0].Y, Ua, Va);

    NsPitchShell(K, F, W, D, 230.0, ELaneColour::WallTeal);
    NsCounter(K, F, 30.0, W - 30.0, 52.0, 100.0);
    NsRoofAndPosts(K, F, -25.0, W + 25.0, -70.0, D + 10.0, 232.0, ELaneColour::AwningBlue, -54.0);
    Ctx.SignCount += NsSignboard(K, NsShifted(F, -70.0), 25.0, W - 25.0, 198.0, 236.0, ELaneColour::SignRed, ELaneColour::TrimLight,
                                 "NABIN'S TEA STALL") > 0 ? 1 : 0;

    // The tea maker's side: a coal stove with a kettle, a shelf of jars, cups on the counter and a stool. The anchor (Ua, Va) stays clear.
    K.Add(F, ELaneColour::TankBlack, Ua - 160.0, Ua - 80.0, 150.0, 210.0, PWLane::FootpathHeight, 70.0);
    K.AddProp(F, Ua - 160.0, Ua - 80.0, 150.0, 210.0, PWLane::FootpathHeight, 70.0);
    K.Add(F, ELaneColour::Chrome, Ua - 138.0, Ua - 102.0, 165.0, 195.0, 70.0, 110.0);
    K.Add(F, ELaneColour::WoodLight, 40.0, W - 40.0, D - 45.0, D - NsSideWall, 120.0, 124.0);
    static const ELaneColour Jars[] = {ELaneColour::GoodsYellow, ELaneColour::GoodsRed, ELaneColour::GoodsGreen};
    for (int32 I = 0; I < 7; ++I)
    {
        const double U = 60.0 + I * 54.0;
        K.Add(F, Jars[I % 3], U, U + 26.0, D - 40.0, D - 18.0, 124.0, 150.0);
    }
    for (int32 I = 0; I < 6; ++I)
    {
        const double U = 60.0 + I * 62.0;
        K.Add(F, ELaneColour::TrimLight, U, U + 12.0, 18.0, 30.0, 104.0, 114.0);
    }
    K.Add(F, ELaneColour::WoodLight, Ua + 130.0, Ua + 160.0, 160.0, 190.0, PWLane::FootpathHeight, 52.0);
    K.AddProp(F, Ua + 130.0, Ua + 160.0, 160.0, 190.0, PWLane::FootpathHeight, 52.0);
    (void)Va;
}

void NsVendorStall(FNsSink& K, FNsContext& Ctx, const PerfectWorld::NeighbourhoodFeature& Pitch, const PerfectWorld::NeighbourhoodFeature& Anchor)
{
    const FNsRect R(Pitch.Bounds.X0, Pitch.Bounds.Y0, Pitch.Bounds.X1, Pitch.Bounds.Y1);
    double W = 0.0;
    double D = 0.0;
    const FLaneFrame F = NsFaceFrame(R, ENsFace::PlusY, W, D);
    double Ua = 0.0;
    double Va = 0.0;
    NsLocal(F, Anchor.Geometry[0].X, Anchor.Geometry[0].Y, Ua, Va);

    NsPitchShell(K, F, W, D, 210.0, ELaneColour::WallTerracotta);
    // A low table of crates: bananas, oranges, apples and greens in heaps, behind a plank edge.
    NsCounter(K, F, 24.0, W - 24.0, 56.0, 80.0);
    static const ELaneColour Produce[] = {ELaneColour::GoodsYellow, ELaneColour::GoodsOrange, ELaneColour::GoodsRed, ELaneColour::GoodsGreen};
    for (int32 I = 0; I < 8; ++I)
    {
        const double U = 36.0 + I * 52.0;
        K.Add(F, ELaneColour::WoodLight, U, U + 46.0, 12.0, 52.0, 84.0, 94.0);
        K.Add(F, Produce[I % 4], U + 4.0, U + 42.0, 16.0, 48.0, 94.0, 108.0 + (NsHash(9301u, I) % 8u));
    }
    // A big cloth parasol on two front poles.
    NsRoofAndPosts(K, F, -30.0, W + 30.0, -80.0, D, 236.0, ELaneColour::AwningGreen, -60.0);
    Ctx.SignCount += NsSignboard(K, NsShifted(F, -80.0), 40.0, W - 40.0, 204.0, 240.0, ELaneColour::SignGreen, ELaneColour::TrimLight,
                                 "TAJA PHAL") > 0 ? 1 : 0;
    K.Add(F, ELaneColour::WoodLight, Ua - 150.0, Ua - 110.0, 140.0, 180.0, PWLane::FootpathHeight, 55.0); // the seller's stool
    K.AddProp(F, Ua - 150.0, Ua - 110.0, 140.0, 180.0, PWLane::FootpathHeight, 55.0);
    (void)Va;
}

/** A three-wheeler auto at U (its centre), nose toward the street (V small): 130 wide, 264 long, 132 high. */
void NsAuto(FNsSink& K, const FLaneFrame& F, double U, double V0, ELaneColour Body)
{
    const double V1 = V0 + 264.0;
    K.Add(F, Body, U - 62.0, U + 62.0, V0 + 10.0, V1, 22.0, 72.0);
    K.Add(F, ELaneColour::ShutterGreen, U - 60.0, U + 60.0, V0 + 60.0, V1 - 6.0, 124.0, 132.0);
    for (const double Du : {-56.0, 56.0})
    {
        K.Add(F, ELaneColour::Rail, U + Du - 3.0, U + Du + 3.0, V0 + 62.0, V0 + 68.0, 72.0, 124.0);
        K.Add(F, ELaneColour::Rail, U + Du - 3.0, U + Du + 3.0, V1 - 12.0, V1 - 6.0, 72.0, 124.0);
    }
    K.Add(F, ELaneColour::TaxiGlass, U - 54.0, U + 54.0, V0 + 54.0, V0 + 58.0, 74.0, 122.0);
    K.Add(F, ELaneColour::WoodLight, U - 50.0, U + 50.0, V0 + 150.0, V1 - 12.0, 72.0, 90.0); // the bench
    K.Add(F, ELaneColour::TankBlack, U - 6.0, U + 6.0, V0 + 10.0, V0 + 50.0, PWLane::FootpathHeight, 60.0);
    for (const double Du : {-56.0, 56.0})
    {
        K.Add(F, ELaneColour::TankBlack, U + Du - 6.0, U + Du + 6.0, V0 + 170.0, V0 + 214.0, PWLane::FootpathHeight, 60.0);
    }
    K.AddProp(F, U - 62.0, U + 62.0, V0 + 10.0, V1, PWLane::FootpathHeight, 72.0);
    K.AddProp(F, U - 60.0, U + 60.0, V0 + 60.0, V1 - 6.0, 72.0, 132.0);
}

/** A cycle rickshaw van: a front wheel and saddle, a passenger bench under a hood at the back. */
void NsRickshaw(FNsSink& K, const FLaneFrame& F, double U, double V0)
{
    K.Add(F, ELaneColour::Rail, U - 3.0, U + 3.0, V0, V0 + 150.0, 62.0, 68.0);           // the frame bar
    K.Add(F, ELaneColour::TankBlack, U - 3.0, U + 3.0, V0, V0 + 46.0, PWLane::FootpathHeight, 62.0); // front wheel
    K.Add(F, ELaneColour::TankBlack, U - 10.0, U + 10.0, V0 + 70.0, V0 + 96.0, 68.0, 76.0);          // saddle
    K.Add(F, ELaneColour::AwningBlue, U - 46.0, U + 46.0, V0 + 160.0, V0 + 330.0, 34.0, 44.0);       // the platform
    K.Add(F, ELaneColour::WoodLight, U - 42.0, U + 42.0, V0 + 250.0, V0 + 326.0, 44.0, 80.0);        // the bench
    K.Add(F, ELaneColour::AwningRed, U - 46.0, U + 46.0, V0 + 232.0, V0 + 334.0, 140.0, 148.0);      // the hood
    for (const double Du : {-44.0, 44.0})
    {
        K.Add(F, ELaneColour::Rail, U + Du - 2.0, U + Du + 2.0, V0 + 236.0, V0 + 240.0, 80.0, 140.0);
        K.Add(F, ELaneColour::TankBlack, U + Du - 3.0, U + Du + 3.0, V0 + 250.0, V0 + 300.0, PWLane::FootpathHeight, 62.0);
    }
    K.AddProp(F, U - 46.0, U + 46.0, V0 + 160.0, V0 + 330.0, PWLane::FootpathHeight, 80.0);
    K.AddProp(F, U - 46.0, U + 46.0, V0 + 232.0, V0 + 334.0, 140.0, 148.0);
}

void NsAutoStand(FNsSink& K, FNsContext& Ctx, const PerfectWorld::NeighbourhoodFeature& Stand, const PerfectWorld::NeighbourhoodFeature& Anchor)
{
    const FNsRect R(Stand.Bounds.X0, Stand.Bounds.Y0, Stand.Bounds.X1, Stand.Bounds.Y1);
    double W = 0.0;
    double D = 0.0;
    const FLaneFrame F = NsFaceFrame(R, ENsFace::PlusY, W, D);
    double Ua = 0.0;
    double Va = 0.0;
    NsLocal(F, Anchor.Geometry[0].X, Anchor.Geometry[0].Y, Ua, Va);

    // A walled bay: level floor, side walls, a painted back hoarding and a tin shelter over the back third.
    K.Add(F, ELaneColour::Footpath, NsSideWall, W - NsSideWall, 0.0, D - NsSideWall, 0.0, PWLane::FootpathHeight);
    K.Add(F, ELaneColour::WallPink, 0.0, NsSideWall, 0.0, D, 0.0, 250.0);
    K.Add(F, ELaneColour::WallPink, W - NsSideWall, W, 0.0, D, 0.0, 250.0);
    Ctx.SignCount += NsHoarding(K, NsShifted(F, D - NsSideWall), W, NsSideWall, 8801u) > 0 ? 1 : 0;
    const double ShelterV = D - 190.0;
    K.Add(F, ELaneColour::ShutterSteel, 60.0, W - 60.0, ShelterV, D - NsSideWall, 252.0, 260.0);
    for (const double U : {110.0, W - 110.0})
    {
        K.Add(F, ELaneColour::Rail, U - 5.0, U + 5.0, ShelterV + 4.0, ShelterV + 14.0, PWLane::FootpathHeight, 252.0);
        K.AddProp(F, U - 5.0, U + 5.0, ShelterV + 4.0, ShelterV + 14.0, PWLane::FootpathHeight, 252.0);
    }
    Ctx.SignCount += NsSignboard(K, NsShifted(F, ShelterV), 140.0, W - 140.0, 262.0, 302.0, ELaneColour::SignBlue, ELaneColour::GoodsYellow,
                                 "AUTO & RICKSHAW STAND") > 0 ? 1 : 0;

    // The dispatcher's desk beside the anchor, two autos on one side of it and two cycle rickshaws on the other.
    K.Add(F, ELaneColour::WoodLight, Ua + 60.0, Ua + 240.0, 62.0, 112.0, PWLane::FootpathHeight, 90.0);
    K.Add(F, ELaneColour::TrimLight, Ua + 56.0, Ua + 244.0, 58.0, 116.0, 90.0, 94.0);
    K.AddProp(F, Ua + 56.0, Ua + 244.0, 58.0, 116.0, PWLane::FootpathHeight, 94.0);
    K.Add(F, ELaneColour::TrimDark, Ua + 140.0, Ua + 180.0, 70.0, 100.0, 94.0, 104.0); // the ledger
    NsAuto(K, F, 200.0, 100.0, ELaneColour::TaxiYellow);
    NsAuto(K, F, 400.0, 100.0, ELaneColour::ShutterGreen);
    NsRickshaw(K, F, Ua + 380.0, 40.0);
    NsRickshaw(K, F, Ua + 520.0, 40.0);
    (void)Va;
}
} // namespace

void NsBuildStalls(FNsSink& K, FNsContext& Ctx)
{
    const PerfectWorld::NeighbourhoodFeature* Tea = Ctx.Feature("pitch.nabapara.tea");
    const PerfectWorld::NeighbourhoodFeature* TeaAnchor = Ctx.Feature("anchor.nabapara.tea_counter");
    if (Tea != nullptr && TeaAnchor != nullptr && !TeaAnchor->Geometry.empty())
    {
        NsTeaStall(K, Ctx, *Tea, *TeaAnchor);
    }
    const PerfectWorld::NeighbourhoodFeature* Vendor = Ctx.Feature("pitch.nabapara.vendor");
    const PerfectWorld::NeighbourhoodFeature* VendorAnchor = Ctx.Feature("anchor.nabapara.vendor_counter");
    if (Vendor != nullptr && VendorAnchor != nullptr && !VendorAnchor->Geometry.empty())
    {
        NsVendorStall(K, Ctx, *Vendor, *VendorAnchor);
    }
    const PerfectWorld::NeighbourhoodFeature* Stand = Ctx.Feature("stand.nabapara.rickshaw_auto");
    const PerfectWorld::NeighbourhoodFeature* StandAnchor = Ctx.Feature("anchor.nabapara.rickshaw_auto_counter");
    if (Stand != nullptr && StandAnchor != nullptr && !StandAnchor->Geometry.empty())
    {
        NsAutoStand(K, Ctx, *Stand, *StandAnchor);
    }
}
} // namespace PWNs
