#include "PerfectWorldNeighbourhoodFacade.h"
#include "PerfectWorldNeighbourhoodStreets.h"
#include "PerfectWorldNeighbourhoodText.h"

namespace PWNs
{
namespace
{
constexpr double NsWallThickness = 40.0;
constexpr double NsCeilingW = 300.0; // underside of the first-floor slab above GroundZ (15 cm under the first floor level)

enum class ENsShopKind : uint8
{
    Sweets,
    Pharmacy
};

struct FNsShopSpec
{
    const char* BuildingId;
    const char* CounterId;
    ENsShopKind Kind;
    int32 Storeys;
    double Opening; // width of the open front
    ELaneColour Wall;
    ELaneColour Board;
    ELaneColour Ink;
    ELaneColour AwningA;
    ELaneColour AwningB;
    const char* Name;
    uint32 Seed;
};

const FNsShopSpec NsShops[] = {
    {"building.nabapara.sweet_shop", "anchor.nabapara.sweet_shop_counter", ENsShopKind::Sweets, 2, 1100.0, ELaneColour::WallButter,
     ELaneColour::TrimLight, ELaneColour::SignRed, ELaneColour::AwningRed, ELaneColour::TrimLight, "MITALI MISHTI BHANDAR", 7001u},
    {"building.nabapara.pharmacy", "anchor.nabapara.pharmacy_counter", ENsShopKind::Pharmacy, 2, 900.0, ELaneColour::WallSage,
     ELaneColour::SignGreen, ELaneColour::TrimLight, ELaneColour::AwningGreen, ELaneColour::TrimLight, "NABAPARA MEDICAL HALL", 7002u},
};

/** The hollow ground storey: walls, a floor and ceiling, and an open front whose width is Spec.Opening. */
void NsShopShell(FNsSink& K, const FLaneFrame& F, double W, double D, const FNsShopSpec& Spec)
{
    const double Wt = NsWallThickness;
    const double P = 0.5 * (W - Spec.Opening);
    K.Add(F, Spec.Wall, 0.0, Wt, 0.0, D, 0.0, NsCeilingW);
    K.Add(F, Spec.Wall, W - Wt, W, 0.0, D, 0.0, NsCeilingW);
    K.Add(F, Spec.Wall, Wt, W - Wt, D - Wt, D, 0.0, NsCeilingW);
    K.Add(F, Spec.Wall, 0.0, P, 0.0, Wt, 0.0, NsCeilingW);
    K.Add(F, Spec.Wall, W - P, W, 0.0, Wt, 0.0, NsCeilingW);
    K.Add(F, Spec.Wall, P, W - P, 0.0, Wt, 255.0, NsCeilingW); // the lintel over the open front
    K.Add(F, ELaneColour::Plinth, 0.0, P, -2.0, 0.0, PWLane::FootpathHeight, 75.0);
    K.Add(F, ELaneColour::Plinth, W - P, W, -2.0, 0.0, PWLane::FootpathHeight, 75.0);
    // A floor at footpath level that the walking player stands on, and interior finishes that carry no collision.
    K.Add(F, ELaneColour::Footpath, Wt, W - Wt, 0.0, D - Wt, 0.0, PWLane::FootpathHeight);
    K.Add(F, ELaneColour::FloorTile, Wt, W - Wt, Wt, D - Wt, PWLane::FootpathHeight, PWLane::FootpathHeight + 1.0);
    K.Add(F, ELaneColour::CeilingWhite, Wt, W - Wt, Wt, D - Wt, NsCeilingW - 4.0, NsCeilingW);
    K.Add(F, ELaneColour::PlasterWarm, Wt, W - Wt, D - Wt - 3.0, D - Wt, 16.0, NsCeilingW - 4.0);
    K.Add(F, ELaneColour::PlasterWarm, Wt, Wt + 3.0, Wt, D - Wt, 16.0, NsCeilingW - 4.0);
    K.Add(F, ELaneColour::PlasterWarm, W - Wt - 3.0, W - Wt, Wt, D - Wt, 16.0, NsCeilingW - 4.0);
    // The rolled-up shutter's housing above the opening.
    K.Add(F, ELaneColour::ShutterStripe, P, W - P, -4.0, 0.0, 255.0, 275.0);
}

/** A striped awning over the front, above head height, in the shop's two colours. */
void NsAwning(FNsSink& K, const FLaneFrame& F, double W, double Opening, const FNsShopSpec& Spec)
{
    const double P = 0.5 * (W - Opening);
    const double U0 = P - 30.0;
    const double U1 = W - P + 30.0;
    int32 I = 0;
    for (double U = U0; U < U1 - 1.0; U += 60.0, ++I)
    {
        const double Ue = FMath::Min(U + 60.0, U1);
        const ELaneColour C = (I % 2 == 0) ? Spec.AwningA : Spec.AwningB;
        K.Add(F, C, U, Ue, -110.0, 0.0, 266.0, 274.0);
        K.Add(F, C, U, Ue, -112.0, -108.0, 252.0, 274.0);
    }
}

/** The counter with its case, goods and the shelves behind: the anchor stands behind the counter at V = Va. */
void NsCounterAndGoods(FNsSink& K, const FLaneFrame& F, double W, double D, double Anchor, double Va, const FNsShopSpec& Spec)
{
    const double Wt = NsWallThickness;
    const bool bSweets = Spec.Kind == ENsShopKind::Sweets;
    const double Cw = bSweets ? 700.0 : 500.0;
    const double U0 = Anchor - 0.5 * Cw;
    const double U1 = Anchor + 0.5 * Cw;
    // Counter body and top; the service anchor stands 130 cm behind its back face.
    K.Add(F, ELaneColour::WoodLight, U0, U1, 80.0, 170.0, 15.0, 105.0);
    K.Add(F, ELaneColour::TrimLight, U0 - 4.0, U1 + 4.0, 74.0, 176.0, 105.0, 110.0);
    K.AddProp(F, U0, U1, 80.0, 170.0, 15.0, 105.0);
    K.AddProp(F, U0 - 4.0, U1 + 4.0, 74.0, 176.0, 105.0, 110.0);
    if (bSweets)
    {
        // A glass display case of trays: orange jalebi, white rasgulla, yellow laddu, red sandesh.
        K.Add(F, ELaneColour::GlassDusk, U0 + 6.0, U1 - 6.0, 84.0, 164.0, 110.0, 152.0);
        K.AddProp(F, U0 + 6.0, U1 - 6.0, 84.0, 164.0, 110.0, 152.0);
        static const ELaneColour Trays[] = {ELaneColour::GoodsOrange, ELaneColour::TrimLight, ELaneColour::GoodsYellow, ELaneColour::GoodsRed};
        int32 I = 0;
        for (double U = U0 + 14.0; U + 90.0 < U1 - 10.0; U += 104.0, ++I)
        {
            K.Add(F, Trays[I % 4], U, U + 90.0, 100.0, 150.0, 110.0, 119.0);
            K.Add(F, Trays[(I + 1) % 4], U + 12.0, U + 78.0, 108.0, 142.0, 119.0, 128.0);
        }
    }
    else
    {
        // A glass-fronted medicine counter: a scale, boxes of strips and a green cross stand.
        K.Add(F, ELaneColour::GlassDusk, U0 + 6.0, U1 - 6.0, 84.0, 164.0, 110.0, 140.0);
        K.AddProp(F, U0 + 6.0, U1 - 6.0, 84.0, 164.0, 110.0, 140.0);
        for (double U = U0 + 20.0; U + 40.0 < U1 - 20.0; U += 56.0)
        {
            K.Add(F, ELaneColour::TrimLight, U, U + 40.0, 100.0, 140.0, 110.0, 128.0);
            K.Add(F, ELaneColour::SignGreen, U + 4.0, U + 36.0, 98.0, 100.0, 114.0, 124.0);
        }
    }
    K.Add(F, ELaneColour::TrimDark, Anchor + 120.0, Anchor + 160.0, 96.0, 128.0, 110.0, 122.0); // the cash box

    // Shelves on the back wall: four tiers of jars (sweets) or boxes (medicines).
    const double ShelfV0 = D - Wt - 56.0;
    const double ShelfV1 = D - Wt - 6.0;
    const double SU0 = Wt + 50.0;
    const double SU1 = W - Wt - 50.0;
    K.Add(F, ELaneColour::WoodLight, SU0, SU1, ShelfV0, ShelfV1, 16.0, 232.0);
    K.AddProp(F, SU0, SU1, ShelfV0, ShelfV1, 16.0, 232.0);
    static const ELaneColour Jars[] = {ELaneColour::GoodsRed, ELaneColour::GoodsYellow, ELaneColour::GoodsGreen, ELaneColour::GoodsOrange,
                                       ELaneColour::TrimLight};
    for (int32 Tier = 0; Tier < 4; ++Tier)
    {
        const double Z = 60.0 + Tier * 48.0;
        K.Add(F, ELaneColour::TrimDark, SU0, SU1, ShelfV0 - 4.0, ShelfV1, Z, Z + 4.0);
        int32 J = 0;
        for (double U = SU0 + 10.0; U + 24.0 < SU1 - 6.0; U += 36.0, ++J)
        {
            const uint32 H = NsHash(Spec.Seed, Tier, J);
            K.Add(F, bSweets ? Jars[H % 4u] : Jars[4], U, U + 24.0, ShelfV0 - 2.0, ShelfV0 + 22.0, Z + 4.0, Z + 4.0 + 20.0 + (H >> 8) % 12u);
        }
    }
    (void)Va;
}

/** The first-floor signboard: board colour, lettering, and for the pharmacy a green cross badge. */
int32 NsShopSign(FNsSink& K, const FLaneFrame& F, double W, const FNsShopSpec& Spec)
{
    const int32 Boxes = NsSignboard(K, F, 130.0, W - 130.0, 328.0, 380.0, Spec.Board, Spec.Ink, Spec.Name);
    if (Spec.Kind == ENsShopKind::Pharmacy)
    {
        K.Add(F, ELaneColour::TrimLight, 40.0, 100.0, -5.0, -3.0, 324.0, 384.0);
        K.Add(F, ELaneColour::SignGreen, 64.0, 76.0, -7.0, -5.0, 330.0, 378.0);
        K.Add(F, ELaneColour::SignGreen, 46.0, 94.0, -7.0, -5.0, 348.0, 360.0);
    }
    return Boxes;
}

void NsBuildOneShop(FNsSink& K, FNsContext& Ctx, const FNsShopSpec& Spec)
{
    const PerfectWorld::NeighbourhoodFeature* Building = Ctx.Feature(Spec.BuildingId);
    const PerfectWorld::NeighbourhoodFeature* Counter = Ctx.Feature(Spec.CounterId);
    if (Building == nullptr || Counter == nullptr || Counter->Geometry.empty())
    {
        return; // the plan changed; there is nothing to place the shop from
    }
    const FNsRect R(Building->Bounds.X0, Building->Bounds.Y0, Building->Bounds.X1, Building->Bounds.Y1);
    double W = 0.0;
    double D = 0.0;
    const FLaneFrame F = NsFaceFrame(R, ENsFace::PlusY, W, D);
    double Anchor = 0.0;
    double Va = 0.0;
    NsLocal(F, Counter->Geometry[0].X, Counter->Geometry[0].Y, Anchor, Va);

    FNsFacade S;
    S.Width = W;
    S.Depth = D;
    S.Storeys = FMath::Max(2, Building->Storeys);
    S.Seed = Spec.Seed;
    S.Wall = Spec.Wall;
    S.Shutter = NsShutterColour(Spec.Seed >> 3);
    S.bBalconies = false;

    NsShopShell(K, F, W, D, Spec);
    NsCounterAndGoods(K, F, W, D, Anchor, Va, Spec);
    NsAwning(K, F, W, Spec.Opening, Spec);
    NsUpperVolume(K, F, S, NsCeilingW);
    NsUpperFloors(K, F, S);
    NsRoof(K, F, S, true);
    Ctx.SignCount += NsShopSign(K, F, W, Spec) > 0 ? 1 : 0;
    // Flank windows where the side wall lies on a street are not needed: both sides adjoin generated buildings.
}
} // namespace

void NsBuildShopLots(FNsSink& K, FNsContext& Ctx)
{
    for (const FNsShopSpec& Spec : NsShops)
    {
        NsBuildOneShop(K, Ctx, Spec);
    }
    NsBuildStalls(K, Ctx);
}
} // namespace PWNs
