#include "PerfectWorldNeighbourhoodFacade.h"

#include "PerfectWorldNeighbourhoodText.h"

namespace PWNs
{
namespace
{
constexpr double NsShutterHeight = 250.0; // a roller shutter fills the shop bay to 250 cm above the road (235 above the floor)
constexpr double NsGroundTop = 255.0;     // the upper volume starts here, 60 cm under the first floor slab

ELaneColour NsWallPalette(int32 Index)
{
    return static_cast<ELaneColour>(FMath::Clamp(Index, 0, PWLane::WallColourCount - 1));
}
} // namespace

ELaneColour NsShutterColour(uint32 Seed)
{
    const uint32 R = Seed % 20u;
    if (R < 8u)
    {
        return ELaneColour::ShutterBlue;
    }
    if (R < 14u)
    {
        return ELaneColour::AwningRed;
    }
    return R < 17u ? ELaneColour::ShutterGreen : ELaneColour::ShutterSteel;
}

ELaneColour NsWallColour(uint32 Seed, ELaneColour Avoid)
{
    int32 Index = static_cast<int32>(Seed % static_cast<uint32>(PWLane::WallColourCount));
    if (NsWallPalette(Index) == Avoid)
    {
        Index = (Index + 3) % PWLane::WallColourCount;
    }
    return NsWallPalette(Index);
}

int32 NsGroundShutterShop(FNsSink& K, const FLaneFrame& F, const FNsFacade& S)
{
    const double W = S.Width;
    const double D = S.Depth;
    const double P = FMath::Min(45.0, W * 0.1);
    const ELaneColour Shutter = S.Shutter;
    // Piers, the recessed back wall and the dado plinth.
    K.Add(F, S.Wall, 0.0, P, 0.0, D, 0.0, NsGroundTop);
    K.Add(F, S.Wall, W - P, W, 0.0, D, 0.0, NsGroundTop);
    K.Add(F, S.Wall, P, W - P, 18.0, D, 0.0, NsGroundTop);
    K.Add(F, ELaneColour::Plinth, 0.0, P, -2.0, 0.0, PWLane::FootpathHeight, 75.0);
    K.Add(F, ELaneColour::Plinth, W - P, W, -2.0, 0.0, PWLane::FootpathHeight, 75.0);
    // The closed roller shutter, its slats, its housing and a padlock hasp.
    K.Add(F, Shutter, P + 3.0, W - P - 3.0, 8.0, 15.0, PWLane::FootpathHeight, NsShutterHeight);
    for (int32 Slat = 1; Slat < 10; ++Slat)
    {
        const double Z = PWLane::FootpathHeight + Slat * 24.0;
        K.Add(F, ELaneColour::ShutterStripe, P + 3.0, W - P - 3.0, 6.0, 8.0, Z - 1.0, Z + 1.0);
    }
    K.Add(F, ELaneColour::ShutterStripe, P, W - P, -4.0, 0.0, NsShutterHeight, NsShutterHeight + 12.0);
    const double Mid = 0.5 * W;
    K.Add(F, ELaneColour::Chrome, Mid - 6.0, Mid + 6.0, 4.0, 8.0, 44.0, 52.0);
    K.Add(F, ELaneColour::TrimDark, Mid - 10.0, Mid + 10.0, 2.0, 4.0, 40.0, 56.0);

    // The signboard on the lintel: a board colour and an ink colour by seed, and a fictional name that fits.
    static const ELaneColour Boards[] = {ELaneColour::SignBlue, ELaneColour::SignRed, ELaneColour::SignGreen, ELaneColour::TrimLight};
    static const ELaneColour Inks[] = {ELaneColour::TrimLight, ELaneColour::TrimLight, ELaneColour::TrimLight, ELaneColour::SignRed};
    const uint32 Pick = (S.Seed >> 3) % 4u;
    const double BoardU0 = P + 6.0;
    const double BoardU1 = W - P - 6.0;
    const int32 MaxChars = FMath::Max(4, static_cast<int32>((BoardU1 - BoardU0 - 12.0) / 18.0));
    int32 Lettering = NsSignboard(K, F, BoardU0, BoardU1, 266.0, 310.0, Boards[Pick], Inks[Pick], NsShopName(S.Seed >> 5, MaxChars));

    // About a third of the shops also hang a blade sign out over the footpath, above head height.
    if ((S.Seed >> 9) % 3u == 0u)
    {
        const double U = P + 14.0;
        K.Add(F, Boards[(Pick + 1) % 4u], U, U + 5.0, -66.0, -4.0, 262.0, 306.0);
        K.Add(F, ELaneColour::TrimDark, U - 1.0, U + 6.0, -66.0, -4.0, 306.0, 309.0);
    }
    return Lettering;
}

void NsGroundHouse(FNsSink& K, const FLaneFrame& F, const FNsFacade& S)
{
    const double W = S.Width;
    const double D = S.Depth;
    K.Add(F, S.Wall, 0.0, W, 0.0, D, 0.0, NsGroundTop);
    K.Add(F, ELaneColour::Plinth, 0.0, W, -3.0, 0.0, PWLane::FootpathHeight, 80.0);

    // A wooden double door in a dark frame, with a transom, a lamp and a doorstep.
    const double Uc = W * (0.28 + 0.44 * NsUnit(NsHash(S.Seed, 31)));
    K.Add(F, ELaneColour::TrimDark, Uc - 55.0, Uc + 55.0, -4.0, 0.0, PWLane::FootpathHeight, 238.0);
    K.Add(F, ELaneColour::DoorWood, Uc - 48.0, Uc + 48.0, -6.0, -4.0, PWLane::FootpathHeight, 228.0);
    K.Add(F, ELaneColour::TrimDark, Uc - 1.5, Uc + 1.5, -8.0, -6.0, PWLane::FootpathHeight, 228.0);
    K.Add(F, ELaneColour::TrimDark, Uc - 38.0, Uc - 6.0, -7.0, -6.0, 120.0, 215.0);
    K.Add(F, ELaneColour::TrimDark, Uc + 6.0, Uc + 38.0, -7.0, -6.0, 120.0, 215.0);
    K.Add(F, ELaneColour::GlassDusk, Uc - 40.0, Uc + 40.0, -6.0, -4.0, 232.0, 250.0);
    K.Add(F, ELaneColour::GlassLit, Uc - 7.0, Uc + 7.0, -16.0, -6.0, 240.0, 252.0);
    K.Add(F, ELaneColour::Concrete, Uc - 62.0, Uc + 62.0, -14.0, 0.0, 0.0, PWLane::FootpathHeight + 6.0);

    // A barred window beside it when there is room.
    const double Ub = Uc > 0.5 * W ? Uc - 190.0 : Uc + 190.0;
    if (Ub > 80.0 && Ub < W - 80.0)
    {
        K.Add(F, ELaneColour::TrimDark, Ub - 55.0, Ub + 55.0, -3.0, 0.0, 100.0, 215.0);
        K.Add(F, ((S.Seed >> 6) & 1u) != 0u ? ELaneColour::GlassLit : ELaneColour::GlassDusk, Ub - 49.0, Ub + 49.0, -5.0, -3.0, 106.0, 209.0);
        for (int32 Bar = 0; Bar < 8; ++Bar)
        {
            const double U = Ub - 45.0 + Bar * 13.0;
            K.Add(F, ELaneColour::Rail, U, U + 3.0, -7.0, -5.0, 106.0, 209.0);
        }
        K.Add(F, ELaneColour::TrimLight, Ub - 62.0, Ub + 62.0, -9.0, -2.0, 92.0, 100.0);
    }
}

void NsUpperVolume(FNsSink& K, const FLaneFrame& F, const FNsFacade& S, double FromW)
{
    K.Add(F, S.Wall, 0.0, S.Width, 0.0, S.Depth, FromW, S.RoofZ());
}

int32 NsHoarding(FNsSink& K, const FLaneFrame& F, double Width, double Depth, uint32 Seed)
{
    const double Thickness = FMath::Min(Depth, 30.0);
    constexpr double Height = 240.0;
    const ELaneColour Wall = NsWallColour(Seed >> 4, ELaneColour::Count);
    K.Add(F, Wall, 0.0, Width, 0.0, Thickness, 0.0, Height);
    K.Add(F, ELaneColour::TrimLight, 0.0, Width, -3.0, Thickness + 3.0, Height, Height + 10.0);
    K.Add(F, ELaneColour::Plinth, 0.0, Width, -2.0, 0.0, PWLane::FootpathHeight, 60.0);
    int32 Lettering = 0;
    if (Width < 260.0)
    {
        return 0;
    }
    // Painted advertisement panels along the wall, each a different board colour.
    static const ELaneColour Boards[] = {ELaneColour::SignRed, ELaneColour::SignBlue, ELaneColour::ClothMustard, ELaneColour::ClothTeal};
    static const ELaneColour Inks[] = {ELaneColour::TrimLight, ELaneColour::TrimLight, ELaneColour::TrimDark, ELaneColour::TrimLight};
    const int32 Panels = FMath::Clamp(static_cast<int32>(Width / 330.0), 1, 4);
    const double Pitch = Width / Panels;
    for (int32 I = 0; I < Panels; ++I)
    {
        const uint32 H = NsHash(Seed, 100 + I);
        const uint32 Pick = H % 4u;
        const double U0 = I * Pitch + 14.0;
        const double U1 = (I + 1) * Pitch - 14.0;
        Lettering += NsSignboard(K, F, U0, U1, 70.0, 140.0, Boards[Pick], Inks[Pick], NsAdName(H >> 4));
    }
    if (Width >= 320.0)
    {
        Lettering += NsDrawText(K, F, ELaneColour::TrimDark, 0.5 * (Width - NsTextWidth("POST NO BILLS", 3.0)), 215.0, 3.0, -2.0, 0.0,
                                "POST NO BILLS");
    }
    return Lettering;
}
} // namespace PWNs
