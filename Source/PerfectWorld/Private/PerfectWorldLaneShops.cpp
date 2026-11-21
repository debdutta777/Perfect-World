#include "PerfectWorldLaneShops.h"

#include "Math/RotationMatrix.h"
#include "PerfectWorldLaneInteriors.h"

namespace PWLane
{
namespace
{
constexpr double FootTop = FootpathHeight;
constexpr double ShutterTop = FootTop + 240.0; // a 240 cm clear opening above the footpath; the drum box above it ends at 270
constexpr double StallCounterTop = FootTop + 90.0; // a stall counter is about 90 cm above the pavement

// Awning, to the headroom contract: the lowest edge (valance bottom) is 210 above the footpath, 225 above the road.
constexpr double AwningWallZ = 285.0;                   // where the slope meets the wall
constexpr double AwningEdgeZ = 247.0;                   // the slope's lower edge; the slab's underside there is about 245
constexpr double ValanceBottomZ = FootTop + 210.0;      // 225
constexpr double ValanceTopZ = AwningEdgeZ + 2.0;       // 249

constexpr double PotMinSetback = 60.0;   // pots only on footpaths of 210 cm or more, so a route of 128 cm stays beside them
constexpr double PotHalfWidth = 22.0;
constexpr double PotApproach = 60.0;     // pots keep this far from a stall or a door approach

FLaneSpan MakeSpan(double U0, double U1)
{
    FLaneSpan Span;
    Span.U0 = U0;
    Span.U1 = U1;
    return Span;
}

bool OverlapsAny(const TArray<FLaneSpan>& Spans, double U0, double U1)
{
    for (const FLaneSpan& Span : Spans)
    {
        if (U1 > Span.U0 && U0 < Span.U1)
        {
            return true;
        }
    }
    return false;
}

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
    F.Add(K, Panels[R % UE_ARRAY_COUNT(Panels)], U0, U1, -4.0, 0.0, FootTop, ShutterTop);
    for (double Z = 30.0; Z < ShutterTop - 8.0; Z += 26.0)
    {
        F.Add(K, ELaneColour::ShutterStripe, U0 + 2.0, U1 - 2.0, -6.0, -4.0, Z, Z + 5.0);
    }
    F.Add(K, ELaneColour::ShutterStripe, U0, U1, -9.0, -4.0, ShutterTop - 2.0, ShutterTop + 15.0);
    AddSign(K, F, U0 - 10.0, U1 + 10.0, R >> 4);
}

/** A potted plant on the footpath: terracotta pot, dark leaf block, a lighter turned block on top. */
void AddPot(FLaneBlocks& K, const FLaneFrame& F, double U, uint32 R)
{
    F.Add(K, ELaneColour::PotTerracotta, U - 16.0, U + 16.0, -46.0, -14.0, FootTop, FootTop + 32.0);
    F.Add(K, ELaneColour::LeafDark, U - PotHalfWidth, U + PotHalfWidth, -52.0, -8.0, FootTop + 32.0, FootTop + 78.0);
    K.Tilted(ELaneColour::LeafLight, F.Pt(U, -30.0, FootTop + 88.0), FVector(30.0, 30.0, 22.0),
             FRotator(0.0, 20.0 + static_cast<double>(R % 50u), 0.0));
    // Solid to the walking player and the camera: the pot itself, and the leaf block over it (the turned top block is inside its height).
    K.PropCollision(F.Pt(U - 16.0, -46.0, FootTop), F.Pt(U + 16.0, -14.0, FootTop + 32.0));
    K.PropCollision(F.Pt(U - PotHalfWidth, -52.0, FootTop + 32.0), F.Pt(U + PotHalfWidth, -8.0, FootTop + 99.0));
}

/**
 * An open stall: a niche cut into the building holds the dark interior with shelves and goods, and the counter
 * with its cloth and crates. Nothing stands on the footpath except the awning, and its poles only when it is
 * deep enough to leave a wall-side route behind them.
 */
void AddOpenStall(FLaneScene& S, const FLaneBuilding& B, double U0, double U1, uint32 R, TArray<FLaneSpan>& Recesses,
                  TArray<FLaneSpan>& Reserved)
{
    FLaneBlocks& K = S.Blocks;
    const FLaneFrame& F = B.Frame;
    const double Mid = 0.5 * (U0 + U1);
    constexpr double Rd = StallRecessDepth;

    Recesses.Add(MakeSpan(U0, U1));
    Reserved.Add(MakeSpan(U0 - PotApproach, U1 + PotApproach));

    F.Add(K, ELaneColour::ShopInterior, U0, U1, Rd - 4.0, Rd, FootTop, 255.0);
    for (int32 Row = 0; Row < 3; ++Row)
    {
        const double Z = 85.0 + Row * 62.0;
        F.Add(K, ELaneColour::WoodLight, U0 + 6.0, U1 - 6.0, Rd - 30.0, Rd - 4.0, Z, Z + 5.0);
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
            F.Add(K, GoodsColour(G >> 3), A + 3.0, A + SlotWidth - 3.0, Rd - 26.0, Rd - 8.0, Z + 5.0,
                  Z + 23.0 + static_cast<double>((G >> 6) % 14u));
        }
    }

    // The counter stands inside the niche, its front 8 cm behind the building line, with a cloth on top and crates.
    F.Add(K, ELaneColour::WoodLight, U0 + 10.0, U1 - 10.0, 12.0, 59.0, FootTop, StallCounterTop - 5.0);
    F.Add(K, ELaneColour::TrimLight, U0 + 6.0, U1 - 6.0, 8.0, 63.0, StallCounterTop - 5.0, StallCounterTop);
    K.PropCollision(F.Pt(U0 + 6.0, 8.0, FootTop), F.Pt(U1 - 6.0, 63.0, StallCounterTop));
    const int32 Crates = FMath::Clamp(FMath::FloorToInt((U1 - U0 - 20.0) / 50.0), 2, 6);
    const double CrateWidth = (U1 - U0 - 20.0) / Crates;
    for (int32 I = 0; I < Crates; ++I)
    {
        const uint32 G = LaneHash(static_cast<int32>(R), 40, I);
        const double A = U0 + 10.0 + I * CrateWidth;
        const double Top = StallCounterTop + 20.0 + static_cast<double>((G >> 5) % 14u);
        F.Add(K, GoodsColour(G), A + 3.0, A + CrateWidth - 3.0, 15.0, 53.0, StallCounterTop, Top);
        K.PropCollision(F.Pt(A + 3.0, 15.0, StallCounterTop), F.Pt(A + CrateWidth - 3.0, 53.0, Top));
    }

    // A bare bulb in the niche.
    F.Add(K, ELaneColour::GlassLit, Mid - 6.0, Mid + 6.0, Rd - 26.0, Rd - 14.0, 232.0, 246.0);
    if (B.Side != 0 && S.LightSpots.Num() < 40)
    {
        S.LightSpots.Add(F.Pt(Mid, Rd - 40.0, 215.0));
    }

    // The awning: alternating stripes on a gentle slope, a valance whose lower edge is 210 cm above the footpath
    // (the slab's underside is higher still), and either two poles on the footpath or, for a shallow awning, two stay rods.
    static const ELaneColour Stripe[] = {ELaneColour::AwningRed, ELaneColour::AwningBlue, ELaneColour::AwningGreen};
    const ELaneColour Awning = Stripe[(R >> 2) % UE_ARRAY_COUNT(Stripe)];
    const double Projection = FMath::Min(165.0, 125.0 + B.Setback);
    const double Run = Projection - 6.0;
    const FVector Slope = (F.V * -Run + FVector(0.0, 0.0, AwningEdgeZ - AwningWallZ)).GetSafeNormal();
    const double SlopeLength = FMath::Sqrt(Run * Run + (AwningWallZ - AwningEdgeZ) * (AwningWallZ - AwningEdgeZ));
    const FRotator Tilt = FRotationMatrix::MakeFromXY(F.U, Slope).Rotator();
    const int32 Stripes = FMath::Max(2, FMath::CeilToInt((U1 - U0 + 20.0) / 36.0));
    const double StripeWidth = (U1 - U0 + 20.0) / Stripes;
    for (int32 I = 0; I < Stripes; ++I)
    {
        const double Uc = U0 - 10.0 + (I + 0.5) * StripeWidth;
        K.Tilted((I % 2 == 0) ? Awning : ELaneColour::TrimLight,
                 F.Pt(Uc, -(Projection + 6.0) * 0.5, 0.5 * (AwningWallZ + AwningEdgeZ)), FVector(StripeWidth, SlopeLength, 4.0), Tilt);
    }
    F.Add(K, Awning, U0 - 10.0, U1 + 10.0, -Projection - 4.0, -Projection, ValanceBottomZ, ValanceTopZ);
    if (Run >= MinFootpathRoute + 20.0)
    {
        // The poles stand 159 cm or more from the wall, so a route of at least 120 cm stays between them and the building.
        for (const double PoleU : {U0 - 8.0, U1 + 4.0})
        {
            F.Add(K, ELaneColour::TrimDark, PoleU, PoleU + 4.0, -Projection + 2.0, -Projection + 6.0, FootTop, AwningEdgeZ);
            K.PropCollision(F.Pt(PoleU, -Projection + 2.0, FootTop), F.Pt(PoleU + 4.0, -Projection + 6.0, AwningEdgeZ));
        }
    }
    else
    {
        // A short awning is held by stay rods from the wall (all above 230 cm), with nothing standing on the footpath.
        for (const double RodU : {U0 - 6.0, U1 + 6.0})
        {
            K.Wire(ELaneColour::TrimDark, F.Pt(RodU, -3.0, 262.0), F.Pt(RodU, -Projection + 3.0, 232.0), 3.0);
        }
    }
}

/** A house frontage: a wood door with a fanlight and a lamp on a threshold, and a barred window or one shuttered bay. */
void AddResidence(FLaneScene& S, const FLaneBuilding& B, TArray<FLaneSpan>& Reserved)
{
    FLaneBlocks& K = S.Blocks;
    const FLaneFrame& F = B.Frame;
    const double W = B.Width;
    const bool bDoorLeft = (B.Seed & 1u) != 0u;
    const double DoorU = W * (bDoorLeft ? 0.30 : 0.70);

    if (IsOpeningHome(B))
    {
        // The opening home draws its own real door, windows and lamp (PerfectWorldLaneInteriors.cpp); the flat decorative
        // door and window here would sit in front of the openings and close them. Keep the approach clear of pots.
        Reserved.Add(MakeSpan(HomeFrontDoorU - 90.0, HomeFrontDoorU + 90.0));
        return;
    }

    // The entrance floor is the footpath top (+15), so a 90 x 210 leaf (the clear opening) in a 7 cm frame stands on it
    // with its full 210 cm above the threshold. The threshold plate is flat with the footpath (0 to 15.5, a 5 mm lip).
    F.Add(K, ELaneColour::TrimDark, DoorU - 52.0, DoorU + 52.0, -3.0, 0.0, FootTop, FootTop + 217.0);
    F.Add(K, ELaneColour::DoorWood, DoorU - 45.0, DoorU + 45.0, -5.0, -3.0, FootTop, FootTop + 210.0);
    F.Add(K, ELaneColour::TrimDark, DoorU - 2.0, DoorU + 2.0, -7.0, -5.0, FootTop, FootTop + 210.0);
    F.Add(K, ELaneColour::GlassDusk, DoorU - 28.0, DoorU + 28.0, -7.0, -5.0, 145.0, 205.0);
    F.Add(K, ELaneColour::Concrete, DoorU - 62.0, DoorU + 62.0, -42.0, 0.0, 0.0, FootTop + 0.5);
    F.Add(K, ELaneColour::GlassLit, DoorU - 8.0, DoorU + 8.0, -12.0, -2.0, 240.0, 258.0);
    Reserved.Add(MakeSpan(DoorU - 90.0, DoorU + 90.0)); // the door approach stays clear of pots

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
        // A 120 x 135 opening with the sill 75 above the floor (the floor sits on the footpath), barred, with a 45 cm chhajja.
        constexpr double SillZ = FootTop + 75.0;
        constexpr double TopZ = SillZ + 135.0;
        F.Add(K, ELaneColour::TrimDark, WinU - 60.0, WinU + 60.0, -3.0, 0.0, SillZ, TopZ);
        F.Add(K, bLit ? ELaneColour::GlassLit : ELaneColour::GlassDusk, WinU - 53.0, WinU + 53.0, -5.0, -3.0, SillZ + 7.0, TopZ - 7.0);
        for (int32 I = 0; I < 6; ++I)
        {
            const double A = WinU - 45.0 + I * 18.0;
            F.Add(K, ELaneColour::TrimDark, A, A + 3.0, -9.0, -5.0, SillZ + 7.0, TopZ - 7.0);
        }
        F.Add(K, ELaneColour::TrimLight, WinU - 66.0, WinU + 66.0, -12.0, -2.0, SillZ - 8.0, SillZ);
        F.Add(K, ELaneColour::TrimLight, WinU - 70.0, WinU + 70.0, -45.0, 0.0, TopZ + 2.0, TopZ + 8.0);
    }
}
} // namespace

void BuildLaneGroundFloor(FLaneScene& S, const FLaneBuilding& B, TArray<FLaneSpan>& OutRecesses)
{
    FLaneBlocks& K = S.Blocks;
    const FLaneFrame& F = B.Frame;
    const double W = B.Width;
    const int32 Mode = (B.Index + (B.Side > 0 ? 1 : 0)) % 3; // 0 shutters, 1 stalls, 2 house door
    OutRecesses.Reset();
    TArray<FLaneSpan> Reserved; // parts of the frontage that stay free of pots: doors and stalls

    if (Mode == 2)
    {
        AddResidence(S, B, Reserved);
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
                AddOpenStall(S, B, U0, U0 + BayWidth, R, OutRecesses, Reserved);
            }
            else
            {
                AddShutterBay(K, F, U0, U0 + BayWidth, R);
            }
        }
    }

    // Zero to two pots standing against the wall. They are placed last, after the doors and stalls have claimed their
    // frontage, and only where the footpath is wide enough to keep a 120 cm route beside them (setback 60 or more).
    if (B.Setback >= PotMinSetback)
    {
        TArray<FLaneSpan> Placed;
        const int32 Pots = static_cast<int32>((B.Seed >> 5) % 3u);
        for (int32 I = 0; I < Pots; ++I)
        {
            for (int32 Try = 0; Try < 4; ++Try)
            {
                const uint32 R = LaneHash(static_cast<int32>(B.Seed), I, 9 + Try * 13);
                const double U = 40.0 + LaneUnit(R) * (W - 80.0);
                if (OverlapsAny(Reserved, U - PotHalfWidth, U + PotHalfWidth) || OverlapsAny(Placed, U - PotHalfWidth - 30.0, U + PotHalfWidth + 30.0))
                {
                    continue;
                }
                Placed.Add(MakeSpan(U - PotHalfWidth, U + PotHalfWidth));
                AddPot(K, F, U, R >> 8);
                break;
            }
        }
    }
}
} // namespace PWLane
