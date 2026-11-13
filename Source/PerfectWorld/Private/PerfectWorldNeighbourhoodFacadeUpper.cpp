#include "PerfectWorldNeighbourhoodFacade.h"

namespace PWNs
{
namespace
{
/**
 * A window to the contract in ASSET_PIPELINE.md (a 120 x 135 opening, sill 75 above the floor, a chhajja above), with the
 * para's shutter habits: an open pair beside it, a closed pair across it, or none. Z is the room's finished floor level.
 */
void NsWindow(FNsSink& K, const FLaneFrame& F, double Uc, double Z, bool bLit, uint32 Mode, ELaneColour Shutter)
{
    constexpr double SillZ = 75.0;
    constexpr double TopZ = SillZ + 135.0;
    K.Add(F, ELaneColour::TrimDark, Uc - 60.0, Uc + 60.0, -3.0, 0.0, Z + SillZ, Z + TopZ);
    K.Add(F, bLit ? ELaneColour::GlassLit : ELaneColour::GlassDusk, Uc - 53.0, Uc + 53.0, -5.0, -3.0, Z + SillZ + 7.0, Z + TopZ - 7.0);
    K.Add(F, ELaneColour::TrimDark, Uc - 2.0, Uc + 2.0, -7.0, -4.0, Z + SillZ + 7.0, Z + TopZ - 7.0);
    K.Add(F, ELaneColour::TrimLight, Uc - 66.0, Uc + 66.0, -9.0, -2.0, Z + SillZ - 8.0, Z + SillZ);
    K.Add(F, ELaneColour::TrimLight, Uc - 70.0, Uc + 70.0, -45.0, 0.0, Z + TopZ + 2.0, Z + TopZ + 8.0);
    if (Mode == 1u)
    {
        K.Add(F, Shutter, Uc - 96.0, Uc - 60.0, -5.0, 0.0, Z + SillZ, Z + TopZ);
        K.Add(F, Shutter, Uc + 60.0, Uc + 96.0, -5.0, 0.0, Z + SillZ, Z + TopZ);
        K.Add(F, ELaneColour::ShutterStripe, Uc - 80.0, Uc - 76.0, -6.0, -5.0, Z + SillZ, Z + TopZ);
        K.Add(F, ELaneColour::ShutterStripe, Uc + 76.0, Uc + 80.0, -6.0, -5.0, Z + SillZ, Z + TopZ);
    }
    else if (Mode == 2u)
    {
        K.Add(F, Shutter, Uc - 58.0, Uc + 58.0, -9.0, -5.0, Z + SillZ, Z + TopZ);
        K.Add(F, ELaneColour::ShutterStripe, Uc - 1.5, Uc + 1.5, -10.0, -9.0, Z + SillZ, Z + TopZ);
        for (int32 Slat = 1; Slat < 6; ++Slat)
        {
            const double Zs = Z + SillZ + Slat * 22.0;
            K.Add(F, ELaneColour::ShutterStripe, Uc - 56.0, Uc + 56.0, -10.0, -9.0, Zs - 0.8, Zs + 0.8);
        }
    }
}

/** A balcony: door, slab, railing with balusters, and sometimes a pot plant and washing on the rail. */
void NsBalcony(FNsSink& K, const FLaneFrame& F, double Uc, double Z, uint32 R, ELaneColour Shutter)
{
    K.Add(F, ELaneColour::TrimDark, Uc - 52.0, Uc + 52.0, -3.0, 0.0, Z + 2.0, Z + 219.0);
    K.Add(F, ELaneColour::DoorWood, Uc - 45.0, Uc + 45.0, -5.0, -3.0, Z + 2.0, Z + 212.0);
    K.Add(F, ELaneColour::TrimDark, Uc - 2.0, Uc + 2.0, -8.0, -5.0, Z + 2.0, Z + 212.0);
    K.Add(F, (R & 4u) != 0u ? ELaneColour::GlassLit : ELaneColour::GlassDusk, Uc - 34.0, Uc + 34.0, -7.0, -5.0, Z + 120.0, Z + 198.0);
    // A louvred shutter leaf folded back beside the door, in the building's shutter colour.
    K.Add(F, Shutter, Uc + 54.0, Uc + 88.0, -4.0, 0.0, Z + 2.0, Z + 212.0);

    K.Add(F, ELaneColour::Concrete, Uc - 88.0, Uc + 88.0, -100.0, 0.0, Z - 14.0, Z);
    K.Add(F, ELaneColour::Rail, Uc - 88.0, Uc + 88.0, -100.0, -94.0, Z + 92.0, Z + 100.0);
    K.Add(F, ELaneColour::Rail, Uc - 88.0, Uc + 88.0, -100.0, -96.0, Z + 50.0, Z + 55.0);
    K.Add(F, ELaneColour::Rail, Uc - 88.0, Uc - 82.0, -100.0, 0.0, Z + 92.0, Z + 100.0);
    K.Add(F, ELaneColour::Rail, Uc + 82.0, Uc + 88.0, -100.0, 0.0, Z + 92.0, Z + 100.0);
    for (int32 I = 0; I < 6; ++I)
    {
        const double A = Uc - 80.0 + I * 31.0;
        K.Add(F, ELaneColour::Rail, A, A + 4.0, -100.0, -96.0, Z, Z + 92.0);
    }
    if ((R & 1u) != 0u)
    {
        K.Add(F, ELaneColour::PotTerracotta, Uc - 80.0, Uc - 52.0, -90.0, -62.0, Z, Z + 28.0);
        K.Add(F, ELaneColour::LeafDark, Uc - 84.0, Uc - 48.0, -94.0, -58.0, Z + 28.0, Z + 76.0);
    }
    if (((R >> 3) & 3u) == 1u)
    {
        static const ELaneColour Cloth[] = {ELaneColour::ClothMagenta, ELaneColour::ClothMustard, ELaneColour::ClothTeal, ELaneColour::TrimLight};
        K.Add(F, Cloth[(R >> 6) % UE_ARRAY_COUNT(Cloth)], Uc - 40.0, Uc - 4.0, -103.0, -101.0, Z + 20.0, Z + 100.0);
        K.Add(F, Cloth[(R >> 9) % UE_ARRAY_COUNT(Cloth)], Uc + 8.0, Uc + 60.0, -103.0, -101.0, Z + 30.0, Z + 100.0);
    }
}
} // namespace

void NsUpperFloors(FNsSink& K, const FLaneFrame& F, const FNsFacade& S)
{
    const double W = S.Width;
    const int32 Bays = FMath::Max(1, FMath::FloorToInt(W / 260.0));
    const double Pitch = W / Bays;
    const bool bBalconies = S.bBalconies && (S.Seed % 10u) < 5u && Pitch >= 200.0;
    for (int32 Floor = 1; Floor < S.Storeys; ++Floor)
    {
        const double Z = S.FloorZ(Floor);
        K.Add(F, ELaneColour::TrimLight, 0.0, W, -5.0, 0.0, Z - 6.0, Z + 6.0);
        for (int32 Bay = 0; Bay < Bays; ++Bay)
        {
            const double Uc = (Bay + 0.5) * Pitch;
            const uint32 R = NsHash(S.Seed, Floor, Bay);
            if (bBalconies && ((Bay + Floor) % 2 == 0) && (R % 4u != 0u))
            {
                NsBalcony(K, F, Uc, Z, R >> 2, S.Shutter);
                continue;
            }
            NsWindow(K, F, Uc, Z, ((R >> 8) % 100u) < 28u, (R >> 16) % 3u, S.Shutter);
            if (R % 7u == 0u)
            {
                K.Add(F, ELaneColour::AcUnit, Uc - 30.0, Uc + 30.0, -34.0, 0.0, Z + 22.0, Z + 67.0);
            }
        }
    }
    // Cornice, a dark damp line at the base and stains running down from the cornice.
    const double H = S.RoofZ();
    K.Add(F, ELaneColour::TrimLight, 0.0, W, -14.0, 0.0, H - 22.0, H);
    for (int32 I = 0; I < 3; ++I)
    {
        const uint32 R = NsHash(S.Seed, 77, I);
        const double Us = 20.0 + NsUnit(R) * (W - 40.0);
        const double Length = 100.0 + static_cast<double>((R >> 8) % 140u);
        K.Add(F, ELaneColour::Stain, Us - 12.0, Us + 12.0, -1.5, 0.0, H - 30.0 - Length, H - 30.0);
    }
}

void NsRoof(FNsSink& K, const FLaneFrame& F, const FNsFacade& S, bool bClutter)
{
    const double W = S.Width;
    const double D = S.Depth;
    const double H = S.RoofZ();
    const uint32 R = S.Seed;
    K.Add(F, S.Wall, 0.0, W, -2.0, 12.0, H, H + 90.0);
    K.Add(F, S.Wall, 0.0, W, D - 14.0, D, H, H + 90.0);
    K.Add(F, S.Wall, 0.0, 14.0, 12.0, D - 14.0, H, H + 90.0);
    K.Add(F, S.Wall, W - 14.0, W, 12.0, D - 14.0, H, H + 90.0);
    K.Add(F, ELaneColour::TrimLight, 0.0, W, -8.0, 18.0, H + 90.0, H + 100.0);

    // One or two 1000-litre block tanks on concrete stands, set back from the face.
    const int32 Tanks = ((R >> 3) % 3u == 0u && W >= 500.0) ? 2 : 1;
    for (int32 T = 0; T < Tanks; ++T)
    {
        const double Uc = Tanks == 1 ? W * (0.25 + 0.5 * NsUnit(R >> 5)) : W * (0.25 + 0.5 * T);
        const double Vc = 120.0 + 60.0 * NsUnit(PWLane::LaneHash(static_cast<int32>(R), T));
        K.Add(F, ELaneColour::Concrete, Uc - 65.0, Uc + 65.0, Vc - 65.0, Vc + 65.0, H, H + 40.0);
        K.Add(F, ELaneColour::TankBlack, Uc - 55.0, Uc + 55.0, Vc - 55.0, Vc + 55.0, H + 40.0, H + 150.0);
        K.Add(F, ELaneColour::TankLid, Uc - 35.0, Uc + 35.0, Vc - 35.0, Vc + 35.0, H + 150.0, H + 160.0);
    }
    if (!bClutter)
    {
        return;
    }
    if (W >= 500.0 && ((R >> 7) % 3u) == 0u)
    {
        const double Um = W * 0.62;
        K.Add(F, S.Wall, Um - 130.0, Um + 130.0, D - 300.0, D - 60.0, H, H + 235.0);
        K.Add(F, ELaneColour::TrimDark, Um - 140.0, Um + 140.0, D - 310.0, D - 50.0, H + 235.0, H + 247.0);
        K.Add(F, ELaneColour::DoorWood, Um - 45.0, Um + 45.0, D - 302.0, D - 300.0, H, H + 210.0);
    }
    if (((R >> 11) & 1u) != 0u)
    {
        const double Ua = W * 0.2;
        const double Va = D * 0.5;
        K.Add(F, ELaneColour::WireBlack, Ua - 2.0, Ua + 2.0, Va - 2.0, Va + 2.0, H, H + 260.0);
        K.Add(F, ELaneColour::WireBlack, Ua - 35.0, Ua + 35.0, Va - 1.5, Va + 1.5, H + 220.0, H + 224.0);
        K.Add(F, ELaneColour::WireBlack, Ua - 28.0, Ua + 28.0, Va - 1.5, Va + 1.5, H + 245.0, H + 249.0);
    }
}

void NsMediumBlock(FNsSink& K, const FLaneFrame& F, const FNsFacade& S)
{
    const double W = S.Width;
    const double H = S.RoofZ();
    K.Add(F, S.Wall, 0.0, W, 0.0, S.Depth, 0.0, H);
    const int32 Bays = FMath::Max(1, FMath::FloorToInt(W / 300.0));
    const double Pitch = W / Bays;
    for (int32 Floor = 1; Floor < S.Storeys; ++Floor)
    {
        const double Z = S.FloorZ(Floor);
        K.Add(F, ELaneColour::TrimLight, 0.0, W, -5.0, 0.0, Z - 6.0, Z + 6.0);
        for (int32 Bay = 0; Bay < Bays; ++Bay)
        {
            const double Uc = (Bay + 0.5) * Pitch;
            const uint32 R = NsHash(S.Seed, Floor, Bay);
            K.Add(F, ELaneColour::TrimDark, Uc - 60.0, Uc + 60.0, -3.0, 0.0, Z + 75.0, Z + 210.0);
            K.Add(F, ((R >> 4) % 100u) < 22u ? ELaneColour::GlassLit : ELaneColour::GlassDusk, Uc - 53.0, Uc + 53.0, -5.0, -3.0, Z + 82.0,
                  Z + 203.0);
            if ((R >> 12) % 4u == 0u)
            {
                K.Add(F, S.Shutter, Uc - 90.0, Uc - 62.0, -5.0, 0.0, Z + 82.0, Z + 203.0);
            }
        }
    }
    K.Add(F, ELaneColour::TrimLight, 0.0, W, -12.0, 0.0, H - 20.0, H);
    NsRoof(K, F, S, false);
}

void NsFlankFace(FNsSink& K, const FLaneFrame& F, const FNsFacade& S)
{
    const double W = S.Width;
    if (W < 200.0)
    {
        return;
    }
    const int32 Bays = FMath::Max(1, FMath::FloorToInt(W / 340.0));
    const double Pitch = W / Bays;
    for (int32 Floor = 1; Floor < S.Storeys; ++Floor)
    {
        const double Z = S.FloorZ(Floor);
        K.Add(F, ELaneColour::TrimLight, 0.0, W, -4.0, 0.0, Z - 5.0, Z + 5.0);
        for (int32 Bay = 0; Bay < Bays; ++Bay)
        {
            const uint32 R = NsHash(S.Seed, 900 + Floor, Bay);
            if (R % 3u == 0u)
            {
                continue; // a blank stretch of wall
            }
            NsWindow(K, F, (Bay + 0.5) * Pitch, Z, (R >> 8) % 100u < 20u, (R >> 16) % 3u, S.Shutter);
        }
    }
    // A rainwater pipe down one edge and stains from the parapet.
    K.Add(F, ELaneColour::Concrete, W - 24.0, W - 16.0, -8.0, 0.0, PWLane::FootpathHeight, S.RoofZ() - 20.0);
    const double H = S.RoofZ();
    K.Add(F, ELaneColour::Stain, 0.3 * W, 0.3 * W + 22.0, -1.5, 0.0, H - 220.0, H - 20.0);
}
} // namespace PWNs
