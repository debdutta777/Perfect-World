#include "PerfectWorldLaneShops.h"

#include "Math/RotationMatrix.h"

namespace PWLane
{
namespace
{
constexpr double FootTop = FootpathHeight;

ELaneColour GoodsColour(uint32 R)
{
    static const ELaneColour Palette[] = {ELaneColour::GoodsYellow, ELaneColour::GoodsOrange, ELaneColour::GoodsRed,
                                          ELaneColour::GoodsGreen, ELaneColour::TrimLight};
    return Palette[R % UE_ARRAY_COUNT(Palette)];
}

/** A signboard over a shopfront: a coloured board with a row of pale letter blocks. */
void AddSign(FLaneBlocks& K, const FLaneFrame& F, double U0, double U1, uint32 R)
{
    static const ELaneColour Boards[] = {ELaneColour::SignBlue, ELaneColour::SignRed, ELaneColour::SignGreen};
    F.Add(K, Boards[R % UE_ARRAY_COUNT(Boards)], U0, U1, -8.0, 0.0, 272.0, 298.0);
    double U = U0 + 12.0;
    for (int32 I = 0; I < 7; ++I)
    {
        const double LetterWidth = 10.0 + static_cast<double>(LaneHash(static_cast<int32>(R), I) % 22u);
        if (U + LetterWidth > U1 - 12.0)
        {
            break;
        }
        F.Add(K, ELaneColour::TrimLight, U, U + LetterWidth, -10.0, -8.0, 279.0, 291.0);
        U += LetterWidth + 8.0;
    }
}

/** A closed roller-shutter bay: a steel panel, stripes for the slats, the drum box on top, and a sign. */
void AddShutterBay(FLaneBlocks& K, const FLaneFrame& F, double U0, double U1, uint32 R)
{
    static const ELaneColour Panels[] = {ELaneColour::ShutterSteel, ELaneColour::ShutterBlue, ELaneColour::ShutterGreen,
                                         ELaneColour::ShutterSteel};
    F.Add(K, Panels[R % UE_ARRAY_COUNT(Panels)], U0, U1, -4.0, 0.0, FootTop, 252.0);
    for (double Z = 30.0; Z < 244.0; Z += 26.0)
    {
        F.Add(K, ELaneColour::ShutterStripe, U0 + 2.0, U1 - 2.0, -6.0, -4.0, Z, Z + 5.0);
    }
    F.Add(K, ELaneColour::ShutterStripe, U0, U1, -9.0, -4.0, 250.0, 268.0);
    AddSign(K, F, U0 - 10.0, U1 + 10.0, R >> 4);
}

/** A potted plant on the footpath: terracotta pot, dark leaf block, a lighter turned block on top. */
void AddPot(FLaneBlocks& K, const FLaneFrame& F, double U, uint32 R)
{
    F.Add(K, ELaneColour::PotTerracotta, U - 16.0, U + 16.0, -46.0, -14.0, FootTop, FootTop + 32.0);
    F.Add(K, ELaneColour::LeafDark, U - 22.0, U + 22.0, -52.0, -8.0, FootTop + 32.0, FootTop + 78.0);
    K.Tilted(ELaneColour::LeafLight, F.Pt(U, -30.0, FootTop + 88.0), FVector(30.0, 30.0, 22.0),
             FRotator(0.0, 20.0 + static_cast<double>(R % 50u), 0.0));
}

/** An open stall: dark interior with shelves and goods, a counter on the footpath, a bulb, a striped awning. */
void AddOpenStall(FLaneScene& S, const FLaneBuilding& B, double U0, double U1, uint32 R)
{
    FLaneBlocks& K = S.Blocks;
    const FLaneFrame& F = B.Frame;
    const double Mid = 0.5 * (U0 + U1);

    F.Add(K, ELaneColour::ShopInterior, U0, U1, -4.0, 0.0, FootTop, 255.0);
    for (int32 Row = 0; Row < 3; ++Row)
    {
        const double Z = 85.0 + Row * 62.0;
        F.Add(K, ELaneColour::WoodLight, U0 + 6.0, U1 - 6.0, -30.0, -4.0, Z, Z + 5.0);
        const int32 Slots = 5;
        const double SlotWidth = (U1 - U0 - 24.0) / Slots;
        for (int32 Slot = 0; Slot < Slots; ++Slot)
        {
            const uint32 G = LaneHash(static_cast<int32>(R), Row, Slot);
            if (G % 4u == 0u)
            {
                continue;
            }
            const double A = U0 + 12.0 + Slot * SlotWidth;
            F.Add(K, GoodsColour(G >> 3), A + 3.0, A + SlotWidth - 3.0, -26.0, -8.0, Z + 5.0,
                  Z + 23.0 + static_cast<double>((G >> 6) % 14u));
        }
    }

    // The counter stands on the footpath, with a cloth on top and crates of fruit and sacks.
    F.Add(K, ELaneColour::WoodLight, U0 + 10.0, U1 - 10.0, -85.0, -38.0, FootTop, 70.0);
    F.Add(K, ELaneColour::TrimLight, U0 + 6.0, U1 - 6.0, -89.0, -34.0, 70.0, 75.0);
    const int32 Crates = FMath::Clamp(FMath::FloorToInt((U1 - U0 - 20.0) / 50.0), 2, 6);
    const double CrateWidth = (U1 - U0 - 20.0) / Crates;
    for (int32 I = 0; I < Crates; ++I)
    {
        const uint32 G = LaneHash(static_cast<int32>(R), 40, I);
        const double A = U0 + 10.0 + I * CrateWidth;
        F.Add(K, GoodsColour(G), A + 3.0, A + CrateWidth - 3.0, -82.0, -44.0, 75.0, 95.0 + static_cast<double>((G >> 5) % 14u));
    }

    // A bare bulb in the opening.
    F.Add(K, ELaneColour::GlassLit, Mid - 6.0, Mid + 6.0, -26.0, -14.0, 232.0, 246.0);
    if (B.Side != 0 && S.LightSpots.Num() < 40)
    {
        S.LightSpots.Add(F.Pt(Mid, -40.0, 215.0));
    }

    // The awning: alternating stripes on a gentle slope, a valance, and two poles standing on the footpath.
    static const ELaneColour Stripe[] = {ELaneColour::AwningRed, ELaneColour::AwningBlue, ELaneColour::AwningGreen};
    const ELaneColour Awning = Stripe[(R >> 2) % UE_ARRAY_COUNT(Stripe)];
    const double Projection = FMath::Min(165.0, 125.0 + B.Setback);
    const double WallZ = 285.0;
    const double EdgeZ = 228.0;
    const double Run = Projection - 6.0;
    const FVector Slope = (F.V * -Run + FVector(0.0, 0.0, EdgeZ - WallZ)).GetSafeNormal();
    const double SlopeLength = FMath::Sqrt(Run * Run + (WallZ - EdgeZ) * (WallZ - EdgeZ));
    const FRotator Tilt = FRotationMatrix::MakeFromXY(F.U, Slope).Rotator();
    const int32 Stripes = FMath::Max(2, FMath::CeilToInt((U1 - U0 + 20.0) / 36.0));
    const double StripeWidth = (U1 - U0 + 20.0) / Stripes;
    for (int32 I = 0; I < Stripes; ++I)
    {
        const double Uc = U0 - 10.0 + (I + 0.5) * StripeWidth;
        K.Tilted((I % 2 == 0) ? Awning : ELaneColour::TrimLight,
                 F.Pt(Uc, -(Projection + 6.0) * 0.5, 0.5 * (WallZ + EdgeZ)), FVector(StripeWidth, SlopeLength, 4.0), Tilt);
    }
    F.Add(K, Awning, U0 - 10.0, U1 + 10.0, -Projection - 4.0, -Projection, 206.0, 230.0);
    F.Add(K, ELaneColour::TrimDark, U0 - 8.0, U0 - 4.0, -Projection + 2.0, -Projection + 6.0, FootTop, EdgeZ);
    F.Add(K, ELaneColour::TrimDark, U1 + 4.0, U1 + 8.0, -Projection + 2.0, -Projection + 6.0, FootTop, EdgeZ);
}

/** A house frontage: a wood door with a fanlight and a lamp on a step, and a barred window or one shuttered bay. */
void AddResidence(FLaneScene& S, const FLaneBuilding& B)
{
    FLaneBlocks& K = S.Blocks;
    const FLaneFrame& F = B.Frame;
    const double W = B.Width;
    const bool bDoorLeft = (B.Seed & 1u) != 0u;
    const double DoorU = W * (bDoorLeft ? 0.30 : 0.70);

    F.Add(K, ELaneColour::TrimDark, DoorU - 50.0, DoorU + 50.0, -3.0, 0.0, FootTop, 238.0);
    F.Add(K, ELaneColour::DoorWood, DoorU - 40.0, DoorU + 40.0, -5.0, -3.0, FootTop, 226.0);
    F.Add(K, ELaneColour::TrimDark, DoorU - 2.0, DoorU + 2.0, -7.0, -5.0, FootTop, 226.0);
    F.Add(K, ELaneColour::GlassDusk, DoorU - 24.0, DoorU + 24.0, -7.0, -5.0, 150.0, 206.0);
    F.Add(K, ELaneColour::Concrete, DoorU - 62.0, DoorU + 62.0, -42.0, 0.0, FootTop, FootTop + 13.0);
    F.Add(K, ELaneColour::GlassLit, DoorU - 8.0, DoorU + 8.0, -12.0, -2.0, 244.0, 262.0);

    if (W >= 700.0)
    {
        const double ShutterWidth = FMath::Min(320.0, 0.5 * W - 80.0);
        const double U0 = bDoorLeft ? W - 40.0 - ShutterWidth : 40.0;
        AddShutterBay(K, F, U0, U0 + ShutterWidth, B.Seed >> 3);
    }
    else
    {
        const double WinU = bDoorLeft ? W - 95.0 : 95.0;
        const bool bLit = ((B.Seed >> 9) % 3u) == 0u;
        F.Add(K, ELaneColour::TrimDark, WinU - 58.0, WinU + 58.0, -3.0, 0.0, 95.0, 225.0);
        F.Add(K, bLit ? ELaneColour::GlassLit : ELaneColour::GlassDusk, WinU - 48.0, WinU + 48.0, -5.0, -3.0, 105.0, 215.0);
        for (int32 I = 0; I < 5; ++I)
        {
            const double A = WinU - 40.0 + I * 20.0;
            F.Add(K, ELaneColour::TrimDark, A, A + 3.0, -9.0, -5.0, 105.0, 215.0);
        }
        F.Add(K, ELaneColour::TrimLight, WinU - 66.0, WinU + 66.0, -12.0, -2.0, 89.0, 97.0);
    }
}
} // namespace

void BuildLaneGroundFloor(FLaneScene& S, const FLaneBuilding& B)
{
    FLaneBlocks& K = S.Blocks;
    const FLaneFrame& F = B.Frame;
    const double W = B.Width;
    const int32 Mode = (B.Index + (B.Side > 0 ? 1 : 0)) % 3; // 0 shutters, 1 stalls, 2 house door

    if (Mode == 2)
    {
        AddResidence(S, B);
    }
    else
    {
        const int32 Bays = FMath::Clamp(FMath::FloorToInt((W - 40.0) / 300.0), 1, 9);
        const double Pillar = 40.0;
        const double BayWidth = (W - Pillar * (Bays + 1)) / Bays;
        for (int32 I = 0; I < Bays; ++I)
        {
            const double U0 = Pillar + I * (BayWidth + Pillar);
            const uint32 R = LaneHash(static_cast<int32>(B.Seed), I, 3);
            const bool bOpen = Mode == 1 && (I == 0 || (R & 1u) != 0u);
            if (bOpen)
            {
                AddOpenStall(S, B, U0, U0 + BayWidth, R);
            }
            else
            {
                AddShutterBay(K, F, U0, U0 + BayWidth, R);
            }
        }
    }

    // Zero to two pots standing on the footpath.
    const int32 Pots = static_cast<int32>((B.Seed >> 5) % 3u);
    for (int32 I = 0; I < Pots; ++I)
    {
        const uint32 R = LaneHash(static_cast<int32>(B.Seed), I, 9);
        AddPot(K, F, 40.0 + LaneUnit(R) * (W - 80.0), R >> 8);
    }
}
} // namespace PWLane
