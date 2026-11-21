#include "PerfectWorldLaneBuildings.h"

#include "NeighbourhoodPlan.h"

#include "PerfectWorldLaneEndInteriors.h"
#include "PerfectWorldLaneInteriors.h"
#include "PerfectWorldLaneShops.h"
#include "PerfectWorldLaneCareInterior.h"
#include "PerfectWorldLaneKitFrontages.h"
#include "PerfectWorldLaneKitStalls.h"
#include "PerfectWorldLaneTallFlats.h"
#include "PerfectWorldLaneStationeryShop.h"
#include "PerfectWorldLaneTeaShop.h"

namespace PWLane
{
namespace
{
ELaneColour WallColour(int32 Index)
{
    return static_cast<ELaneColour>(FMath::Clamp(Index, 0, WallColourCount - 1));
}

/**
 * A window, to the contract in ASSET_PIPELINE.md: a 120 x 135 opening with its sill 75 above the room floor, a pale
 * sill slab, a 45 cm chhajja (concrete sunshade) above, glass (warm if someone is up), mullions, and green shutters
 * on some buildings. Z is the finished floor level of the room (FLaneBuilding::FloorZ).
 */
void AddWindow(FLaneBlocks& K, const FLaneFrame& F, double Uc, double Z, bool bLit, bool bShutters)
{
    constexpr double SillZ = 75.0;
    constexpr double TopZ = SillZ + 135.0;
    F.Add(K, ELaneColour::TrimDark, Uc - 60.0, Uc + 60.0, -3.0, 0.0, Z + SillZ, Z + TopZ);
    F.Add(K, bLit ? ELaneColour::GlassLit : ELaneColour::GlassDusk, Uc - 53.0, Uc + 53.0, -5.0, -3.0, Z + SillZ + 7.0, Z + TopZ - 7.0);
    F.Add(K, ELaneColour::TrimDark, Uc - 2.0, Uc + 2.0, -7.0, -4.0, Z + SillZ + 7.0, Z + TopZ - 7.0);
    F.Add(K, ELaneColour::TrimDark, Uc - 53.0, Uc + 53.0, -7.0, -4.0, Z + 140.0, Z + 144.0);
    F.Add(K, ELaneColour::TrimLight, Uc - 66.0, Uc + 66.0, -9.0, -2.0, Z + SillZ - 8.0, Z + SillZ);
    F.Add(K, ELaneColour::TrimLight, Uc - 70.0, Uc + 70.0, -45.0, 0.0, Z + TopZ + 2.0, Z + TopZ + 8.0);
    if (bShutters)
    {
        F.Add(K, ELaneColour::ShutterGreen, Uc - 96.0, Uc - 60.0, -5.0, 0.0, Z + SillZ, Z + TopZ);
        F.Add(K, ELaneColour::ShutterGreen, Uc + 60.0, Uc + 96.0, -5.0, 0.0, Z + SillZ, Z + TopZ);
    }
}

/** A balcony: door, slab, railing with balusters, and sometimes a pot plant and washing on the rail. Z is the room's floor level. */
void AddBalcony(FLaneBlocks& K, const FLaneFrame& F, double Uc, double Z, uint32 R)
{
    // A 90 x 210 door (the leaf is the clear opening) in a 7 cm frame, standing on the slab.
    F.Add(K, ELaneColour::TrimDark, Uc - 52.0, Uc + 52.0, -3.0, 0.0, Z + 2.0, Z + 219.0);
    F.Add(K, ELaneColour::DoorWood, Uc - 45.0, Uc + 45.0, -5.0, -3.0, Z + 2.0, Z + 212.0);
    F.Add(K, ELaneColour::TrimDark, Uc - 2.0, Uc + 2.0, -8.0, -5.0, Z + 2.0, Z + 212.0);
    F.Add(K, (R & 4u) != 0u ? ELaneColour::GlassLit : ELaneColour::GlassDusk, Uc - 34.0, Uc + 34.0, -7.0, -5.0, Z + 120.0, Z + 198.0);

    // The slab, and a 100 cm railing: top rail at 100, a mid rail, and balusters.
    F.Add(K, ELaneColour::Concrete, Uc - 88.0, Uc + 88.0, -100.0, 0.0, Z - 14.0, Z);
    F.Add(K, ELaneColour::Rail, Uc - 88.0, Uc + 88.0, -100.0, -94.0, Z + 92.0, Z + 100.0);
    F.Add(K, ELaneColour::Rail, Uc - 88.0, Uc + 88.0, -100.0, -96.0, Z + 50.0, Z + 55.0);
    F.Add(K, ELaneColour::Rail, Uc - 88.0, Uc - 82.0, -100.0, 0.0, Z + 92.0, Z + 100.0);
    F.Add(K, ELaneColour::Rail, Uc + 82.0, Uc + 88.0, -100.0, 0.0, Z + 92.0, Z + 100.0);
    for (int32 I = 0; I < 6; ++I)
    {
        const double A = Uc - 80.0 + I * 31.0;
        F.Add(K, ELaneColour::Rail, A, A + 4.0, -100.0, -96.0, Z, Z + 92.0);
    }

    if ((R & 1u) != 0u)
    {
        F.Add(K, ELaneColour::PotTerracotta, Uc - 80.0, Uc - 52.0, -90.0, -62.0, Z, Z + 28.0);
        F.Add(K, ELaneColour::LeafDark, Uc - 84.0, Uc - 48.0, -94.0, -58.0, Z + 28.0, Z + 76.0);
    }
    if (((R >> 3) & 3u) == 1u)
    {
        static const ELaneColour Cloth[] = {ELaneColour::ClothMagenta, ELaneColour::ClothMustard, ELaneColour::ClothTeal,
                                            ELaneColour::TrimLight};
        F.Add(K, Cloth[(R >> 6) % UE_ARRAY_COUNT(Cloth)], Uc - 40.0, Uc - 4.0, -103.0, -101.0, Z + 20.0, Z + 100.0);
        F.Add(K, Cloth[(R >> 9) % UE_ARRAY_COUNT(Cloth)], Uc + 8.0, Uc + 60.0, -103.0, -101.0, Z + 30.0, Z + 100.0);
    }
}

/** Parapet and coping, and (for the near buildings) black water tanks, a stair room and a TV aerial. */
void AddRoof(FLaneBlocks& K, const FLaneFrame& F, const FLaneBuilding& B, double H, bool bClutter)
{
    const double W = B.Width;
    const double D = B.Depth;
    const uint32 R = B.Seed;

    // A roof parapet 100 cm high (90 of wall and a 10 cm coping), the same as a railing.
    F.Add(K, B.Wall, 0.0, W, -2.0, 12.0, H, H + 90.0);
    F.Add(K, B.Wall, 0.0, W, D - 14.0, D, H, H + 90.0);
    F.Add(K, B.Wall, 0.0, 14.0, 12.0, D - 14.0, H, H + 90.0);
    F.Add(K, B.Wall, W - 14.0, W, 12.0, D - 14.0, H, H + 90.0);
    F.Add(K, ELaneColour::TrimLight, 0.0, W, -8.0, 18.0, H + 90.0, H + 100.0);

    // Tanks: a 1000 litre block tank on a concrete stand, 110 x 110 and 110 high with a 10 cm lid (120 above its stand).
    const int32 Tanks = ((R >> 3) % 3u == 0u) ? 2 : 1;
    if (W >= 400.0 || Tanks == 1)
    {
        for (int32 T = 0; T < Tanks; ++T)
        {
            const double Uc = (Tanks == 1) ? W * (0.25 + 0.5 * LaneUnit(R >> 5)) : W * (0.25 + 0.5 * T);
            const double Vc = 120.0 + 60.0 * LaneUnit(LaneHash(static_cast<int32>(R), T));
            F.Add(K, ELaneColour::Concrete, Uc - 65.0, Uc + 65.0, Vc - 65.0, Vc + 65.0, H, H + 40.0);
            F.Add(K, ELaneColour::TankBlack, Uc - 55.0, Uc + 55.0, Vc - 55.0, Vc + 55.0, H + 40.0, H + 150.0);
            F.Add(K, ELaneColour::TankLid, Uc - 35.0, Uc + 35.0, Vc - 35.0, Vc + 35.0, H + 150.0, H + 160.0);
        }
    }

    if (!bClutter)
    {
        return;
    }
    if (W >= 500.0 && ((R >> 7) % 3u) == 0u)
    {
        const double Um = W * 0.62;
        F.Add(K, B.Wall, Um - 130.0, Um + 130.0, D - 300.0, D - 60.0, H, H + 235.0);
        F.Add(K, ELaneColour::TrimDark, Um - 140.0, Um + 140.0, D - 310.0, D - 50.0, H + 235.0, H + 247.0);
        F.Add(K, ELaneColour::DoorWood, Um - 45.0, Um + 45.0, D - 302.0, D - 300.0, H, H + 210.0);
    }
    if (((R >> 11) & 1u) != 0u)
    {
        const double Ua = W * 0.2;
        const double Va = D * 0.5;
        F.Add(K, ELaneColour::WireBlack, Ua - 2.0, Ua + 2.0, Va - 2.0, Va + 2.0, H, H + 260.0);
        F.Add(K, ELaneColour::WireBlack, Ua - 35.0, Ua + 35.0, Va - 1.5, Va + 1.5, H + 220.0, H + 224.0);
        F.Add(K, ELaneColour::WireBlack, Ua - 28.0, Ua + 28.0, Va - 1.5, Va + 1.5, H + 245.0, H + 249.0);
    }
}

/** Storeys above the ground floor with window bays, balconies, and the weathering of an old block. */
void AddUpperFloors(FLaneBlocks& K, const FLaneFrame& F, const FLaneBuilding& B)
{
    const double W = B.Width;
    const uint32 Seed = B.Seed;
    const int32 Bays = FMath::Max(1, FMath::FloorToInt(W / 230.0));
    const double Pitch = W / Bays;
    const bool bBalconies = (Seed % 10u) < 6u && Pitch >= 190.0;
    const bool bShutters = ((Seed >> 4) % 10u) < 5u && Pitch >= 200.0;

    for (int32 Floor = 1; Floor < B.Storeys; ++Floor)
    {
        const double Z = B.FloorZ(Floor);
        for (int32 Bay = 0; Bay < Bays; ++Bay)
        {
            const double Uc = (Bay + 0.5) * Pitch;
            const uint32 R = LaneHash(static_cast<int32>(Seed), Floor, Bay);
            if (bBalconies && ((Bay + Floor) % 2 == 0) && (R % 4u != 0u))
            {
                AddBalcony(K, F, Uc, Z, R >> 2);
                continue;
            }
            AddWindow(K, F, Uc, Z, ((R >> 8) % 100u) < 30u, bShutters);
            if (R % 7u == 0u)
            {
                // A window air-conditioner hung under the sill.
                F.Add(K, ELaneColour::AcUnit, Uc - 30.0, Uc + 30.0, -34.0, 0.0, Z + 22.0, Z + 67.0);
            }
        }
    }

    // Damp stains running down from the cornice.
    const double H = B.RoofZ();
    for (int32 I = 0; I < 2; ++I)
    {
        const uint32 R = LaneHash(static_cast<int32>(Seed), 77, I);
        const double Us = 20.0 + LaneUnit(R) * (W - 40.0);
        const double Length = 100.0 + static_cast<double>((R >> 8) % 120u);
        F.Add(K, ELaneColour::Stain, Us - 12.0, Us + 12.0, -1.5, 0.0, H - 30.0 - Length, H - 30.0);
    }
}

/**
 * The solid wall volume of a near building, with a niche cut into the ground floor wherever an open stall has
 * its counter (Recesses, in increasing U). Solid spans are full-height boxes; each niche keeps a footpath-coloured
 * floor at the footpath level, a back block StallRecessDepth in, and a lintel block above StallRecessTop.
 * The decorative 75 cm plinth fascia runs along the solid spans only.
 */
void AddWallVolume(FLaneBlocks& K, const FLaneFrame& F, const FLaneBuilding& B, double H, const TArray<FLaneSpan>& Recesses)
{
    double Cursor = 0.0;
    auto AddSolid = [&](double U0, double U1)
    {
        if (U1 - U0 < 0.5)
        {
            return;
        }
        F.Add(K, B.Wall, U0, U1, 0.0, B.Depth, 0.0, H);
        F.Add(K, ELaneColour::Plinth, U0, U1, -2.0, 0.0, FootpathHeight, 75.0);
    };
    for (const FLaneSpan& Niche : Recesses)
    {
        AddSolid(Cursor, Niche.U0);
        F.Add(K, ELaneColour::Footpath, Niche.U0, Niche.U1, 0.0, StallRecessDepth, 0.0, FootpathHeight);
        F.Add(K, B.Wall, Niche.U0, Niche.U1, StallRecessDepth, B.Depth, 0.0, H);
        F.Add(K, B.Wall, Niche.U0, Niche.U1, 0.0, StallRecessDepth, StallRecessTop, H);
        Cursor = Niche.U1;
    }
    AddSolid(Cursor, B.Width);
}

/** A near building: forecourt, shopfront, window and balcony floors, trims and the roof. */
void BuildFull(FLaneScene& S, const FLaneBuilding& B)
{
    FLaneBlocks& K = S.Blocks;
    const FLaneFrame& F = B.Frame;
    const double W = B.Width;
    const double H = B.RoofZ();
    const double Sb = B.Setback;

    // The footpath in front of this building, with a kerb stone on the road edge.
    F.Add(K, ELaneColour::Footpath, 0.0, W, -(150.0 + Sb), 0.0, -20.0, FootpathHeight);
    F.Add(K, ELaneColour::Concrete, 0.0, W, -(150.0 + Sb), -(138.0 + Sb), -20.0, FootpathHeight + 2.0);

    // The ground floor goes first: it reports the niches cut into the wall for open stalls, so that their counters
    // stand inside the building and leave the footpath clear.
    TArray<FLaneSpan> Recesses;
    if (IsTeaShopBuilding(B))
    {
        // Shanti Chai Corner (first row, +Y side, Index 3, X 600 to 1300) builds its own frontage, interior and the solid
        // storeys above, in place of both the generic ground floor and the wall volume. See PerfectWorldLaneTeaShop.h.
        BuildTeaShop(S, B);
    }
    else if (IsCareHomeBuilding(B))
    {
        // The children's home (first row, -Y side, Index 1, X -1400 to -300) builds its own frontage, rooms and the
        // solid storeys above. Every origin's home is built; who lives where comes from the core (LaneOrigin).
        BuildCareHome(S, B);
    }
    else if (IsStationeryBuilding(B))
    {
        // The stationery shop (first row, +Y side, Index 2, X -100 to 600) is the interior kit's public variant: sales
        // room, counter, stock room and packing desk, with the solid storeys above. See PerfectWorldLaneStationeryShop.h.
        BuildStationeryShop(S, B);
    }
    else if (IsOpeningHome(B))
    {
        BuildLaneGroundFloor(S, B, Recesses);
        // The opening home (first row, -Y side, Index 2, X -300 to 700) is hollow: walls with openings, rooms, a ceiling,
        // and the solid upper storeys above it. See PerfectWorldLaneInteriors.h.
        BuildOpeningHome(S, B);
    }
    else if (IsTallFlatsUpstairs(B))
    {
        // The tall flats (NR-06): the kit's shared-entry ground floor plus the first real stair and an upper dwelling.
        BuildTallFlatsUpstairs(S, B);
    }
    else if (FLaneKitFrontage KitFrontage; IsKitFrontage(B, KitFrontage))
    {
        // A frontage pinned in the portable frontage catalogue (LaneFrontageCatalogue.h) gets an enterable ground floor
        // from the interior kit, with the solid storeys above. See Docs/LANE_BUILDING_INVENTORY.md.
        BuildKitFrontage(S, B, KitFrontage);
        // R4-01: the unstaffed browsing shops (market_stalls_a, market_stalls_b, end_shop) keep their open stalls, brought forward out of
        // the niche the kit's wall no longer allows, and post their hours. A no-op for every other kit frontage. See PerfectWorldLaneKitStalls.h.
        BuildKitPublicFace(S, B, KitFrontage);
    }
    else if (LE::EndHall EndHall; IsLaneEndHall(B, EndHall))
    {
        // R6-05: the two masses that close the lane are public halls (community hall west, market hall east), entered through a
        // doorless archway on the lane axis. The generic sealed ground floor is replaced for these two buildings only. See
        // PerfectWorldLaneEndInteriors.h.
        BuildLaneEndHall(S, B, EndHall);
    }
    else
    {
        BuildLaneGroundFloor(S, B, Recesses);
        AddWallVolume(K, F, B, H, Recesses);
    }

    for (int32 Floor = 1; Floor < B.Storeys; ++Floor)
    {
        F.Add(K, ELaneColour::TrimLight, 0.0, W, -5.0, 0.0, B.FloorZ(Floor) - 6.0, B.FloorZ(Floor) + 6.0);
    }
    F.Add(K, ELaneColour::TrimLight, 0.0, W, -14.0, 0.0, H - 22.0, H);

    AddUpperFloors(K, F, B);
    AddRoof(K, F, B, H, true);
}

/** A second-row block, seen over the roofs: window frames and glass, bands, parapet. */
void BuildMedium(FLaneScene& S, const FLaneBuilding& B)
{
    FLaneBlocks& K = S.Blocks;
    const FLaneFrame& F = B.Frame;
    const double W = B.Width;
    const double H = B.RoofZ();

    F.Add(K, B.Wall, 0.0, W, 0.0, B.Depth, 0.0, H);
    const int32 Bays = FMath::Max(1, FMath::FloorToInt(W / 300.0));
    const double Pitch = W / Bays;
    for (int32 Floor = 1; Floor < B.Storeys; ++Floor)
    {
        const double Z = B.FloorZ(Floor);
        F.Add(K, ELaneColour::TrimLight, 0.0, W, -5.0, 0.0, Z - 6.0, Z + 6.0);
        for (int32 Bay = 0; Bay < Bays; ++Bay)
        {
            const double Uc = (Bay + 0.5) * Pitch;
            const uint32 R = LaneHash(static_cast<int32>(B.Seed), Floor, Bay);
            F.Add(K, ELaneColour::TrimDark, Uc - 60.0, Uc + 60.0, -3.0, 0.0, Z + 75.0, Z + 210.0);
            F.Add(K, ((R >> 4) % 100u) < 22u ? ELaneColour::GlassLit : ELaneColour::GlassDusk, Uc - 53.0, Uc + 53.0, -5.0, -3.0,
                  Z + 82.0, Z + 203.0);        }
    }
    F.Add(K, ELaneColour::TrimLight, 0.0, W, -12.0, 0.0, H - 20.0, H);
    AddRoof(K, F, B, H, false);
}

/** A far block: a box, floor bands and single strips of glass. */
void BuildLow(FLaneScene& S, const FLaneBuilding& B)
{
    FLaneBlocks& K = S.Blocks;
    const FLaneFrame& F = B.Frame;
    const double W = B.Width;
    const double H = B.RoofZ();

    F.Add(K, B.Wall, 0.0, W, 0.0, B.Depth, 0.0, H);
    const int32 Bays = FMath::Max(1, FMath::FloorToInt(W / 420.0));
    const double Pitch = W / Bays;
    for (int32 Floor = 0; Floor < B.Storeys; ++Floor)
    {
        const double Z = B.FloorZ(Floor);
        if (Floor > 0)
        {
            F.Add(K, ELaneColour::TrimLight, 0.0, W, -5.0, 0.0, Z - 6.0, Z + 6.0);
        }
        for (int32 Bay = 0; Bay < Bays; ++Bay)
        {
            const uint32 R = LaneHash(static_cast<int32>(B.Seed), Floor, Bay);
            if (R % 100u < 30u)
            {
                continue;
            }
            const double Uc = (Bay + 0.5) * Pitch;
            F.Add(K, ((R >> 8) % 100u) < 12u ? ELaneColour::GlassLit : ELaneColour::GlassDusk, Uc - 80.0, Uc + 80.0, -2.0, 0.0,
                  Z + 90.0, Z + 210.0);
        }
    }
    AddRoof(K, F, B, H, false);
}

/** Back rows and the skyline: a strip of blocks of varied width along a line, facing the lane. */
void BuildStrip(FLaneScene& S, bool bAlongX, int32 Sign, double Distance, double From, double To, double MinWidth, double WidthRange,
                int32 MinStoreys, int32 StoreyRange, double Depth, ELaneDetail Detail, int32 SeedBase)
{
    // R7-02: decorative back rows and skyline give way to the neighbourhood overlay (streets, campus, club, new lots). The cursor,
    // index and hash advance exactly as before even for a skipped building, so no later massing shifts.
    static const PerfectWorld::NeighbourhoodPlan R71Plan = PerfectWorld::BuildNeighbourhoodPlan(0); // immutable; built once, not per strip
    double Cursor = From;
    int32 Index = 0;
    int32 Previous = -1;
    while (Cursor < To)
    {
        const uint32 H = LaneHash(SeedBase + Sign * 17, Index, 5);
        FLaneBuilding B;
        B.Width = MinWidth + WidthRange * LaneUnit(H);
        B.Depth = Depth;
        B.Storeys = MinStoreys + static_cast<int32>((H >> 8) % static_cast<uint32>(StoreyRange));
        B.Index = Index;
        B.Seed = H;
        B.Detail = Detail;
        int32 Wall = static_cast<int32>((H >> 20) % static_cast<uint32>(WallColourCount));
        if (Wall == Previous)
        {
            Wall = (Wall + 3) % WallColourCount;
        }
        Previous = Wall;
        B.Wall = WallColour(Wall);
        const double SignD = static_cast<double>(Sign);
        B.Frame = bAlongX ? MakeLaneFrame(Cursor, SignD * Distance, FVector(1.0, 0.0, 0.0), FVector(0.0, SignD, 0.0))
                          : MakeLaneFrame(SignD * Distance, Cursor, FVector(0.0, 1.0, 0.0), FVector(SignD, 0.0, 0.0));
        const FVector R71A = B.Frame.Pt(0.0, 0.0, 0.0);
        const FVector R71B = B.Frame.Pt(B.Width, B.Depth, 0.0);
        const PerfectWorld::NeighbourhoodRect R71Lot{
            static_cast<int>(FMath::Min(R71A.X, R71B.X)) - 101, static_cast<int>(FMath::Min(R71A.Y, R71B.Y)) - 101,
            static_cast<int>(FMath::Max(R71A.X, R71B.X)) + 101, static_cast<int>(FMath::Max(R71A.Y, R71B.Y)) + 101};
        if (!PerfectWorld::NeighbourhoodReservesRect(R71Plan, R71Lot))
        {
            BuildLaneBuilding(S, B);
        }
        Cursor += B.Width + 150.0 * LaneUnit(H >> 4);
        ++Index;
    }
}

/** The two rows of near buildings along the lane, broken by one side alley each. */
void BuildFirstRows(FLaneScene& S)
{
    for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        const int32 Side = SideIndex == 0 ? -1 : 1;
        const double AlleyA = Side < 0 ? AlleyMinusA : AlleyPlusA;
        const double AlleyB = Side < 0 ? AlleyMinusB : AlleyPlusB;
        double Cursor = LaneX0;
        int32 Index = 0;
        int32 Previous = -1;
        while (Cursor < LaneX1 - 250.0)
        {
            if (Cursor >= AlleyA && Cursor < AlleyB)
            {
                Cursor = AlleyB;
                continue;
            }
            const uint32 H = LaneHash(Side * 131 + 7, Index, 11);
            double Width = 600.0 + 100.0 * static_cast<double>(H % 6u);
            if (Cursor < AlleyA && Cursor + Width > AlleyA)
            {
                Width = AlleyA - Cursor;
            }
            if (LaneX1 - (Cursor + Width) < 500.0)
            {
                Width = LaneX1 - Cursor;
            }
            if (Width < 300.0)
            {
                Cursor += Width; // too narrow to be a building: leave a gap
                continue;
            }

            const uint32 SetbackKind = (H >> 12) % 4u;
            FLaneBuilding B;
            B.Width = Width;
            B.Depth = 700.0 + 100.0 * static_cast<double>((H >> 16) % 3u);
            B.Setback = SetbackKind == 0u ? 120.0 : (SetbackKind == 1u ? 60.0 : 0.0);
            B.Storeys = 3 + static_cast<int32>((H >> 8) % 3u);
            B.Side = Side;
            B.Index = Index;
            B.Seed = H;
            B.Detail = ELaneDetail::Full;
            int32 Wall = static_cast<int32>((H >> 20) % static_cast<uint32>(WallColourCount));
            if (Wall == Previous)
            {
                Wall = (Wall + 1) % WallColourCount;
            }
            Previous = Wall;
            B.Wall = WallColour(Wall);
            B.Frame = MakeLaneFrame(Cursor, Side * (FootpathOuter + B.Setback), FVector(1.0, 0.0, 0.0),
                                    FVector(0.0, static_cast<double>(Side), 0.0));
            BuildLaneBuilding(S, B);

            Cursor += Width + (((H >> 24) % 7u == 0u) ? 100.0 : 0.0);
            ++Index;
        }
    }
}

/** The shopfront row that closes the lane ahead, and the one behind the starting camera. */
void BuildClosingRows(FLaneScene& S)
{
    for (int32 End = 0; End < 2; ++End)
    {
        FLaneBuilding B;
        B.Width = 2600.0;
        B.Depth = 900.0;
        B.Storeys = End == 0 ? 5 : 4;
        B.Index = End == 0 ? 1 : 0; // the far end gets open stalls and awnings, the near end shutters
        B.Seed = LaneHash(900 + End, 3, 3);
        B.Wall = End == 0 ? ELaneColour::WallTerracotta : ELaneColour::WallCream;
        B.Detail = ELaneDetail::Full;
        B.Frame = MakeLaneFrame(End == 0 ? LaneX1 : LaneX0, -1300.0, FVector(0.0, 1.0, 0.0),
                                FVector(End == 0 ? 1.0 : -1.0, 0.0, 0.0));
        BuildLaneBuilding(S, B);
    }
}
} // namespace

void BuildLaneBuilding(FLaneScene& S, const FLaneBuilding& B)
{
    switch (B.Detail)
    {
    case ELaneDetail::Full:
        BuildFull(S, B);
        if (B.Side != 0)
        {
            FLaneFront Front;
            Front.X0 = B.Frame.Pt(0.0, 0.0, 0.0).X;
            Front.X1 = B.Frame.Pt(B.Width, 0.0, 0.0).X;
            Front.AbsY = FMath::Abs(B.Frame.Pt(0.0, 0.0, 0.0).Y);
            S.Fronts[B.Side > 0 ? 1 : 0].Add(Front);
        }
        break;
    case ELaneDetail::Medium:
        BuildMedium(S, B);
        break;
    case ELaneDetail::Low:
        BuildLow(S, B);
        break;
    }
}

void BuildLaneRows(FLaneScene& S)
{
    BuildFirstRows(S);
    BuildClosingRows(S);
    for (int32 Sign = -1; Sign <= 1; Sign += 2)
    {
        // Second row: taller blocks behind the roofs. Third row: tall blocks in the haze.
        BuildStrip(S, true, Sign, 1500.0, -3600.0, 7400.0, 800.0, 700.0, 6, 4, 900.0, ELaneDetail::Medium, 2000);
        BuildStrip(S, true, Sign, 2600.0, -3700.0, 7500.0, 1000.0, 800.0, 8, 5, 1000.0, ELaneDetail::Low, 3000);
    }
    // Skyline beyond each end of the lane: ahead (the view) and behind the start camera.
    BuildStrip(S, false, 1, 7600.0, -5500.0, 5500.0, 900.0, 600.0, 8, 6, 1100.0, ELaneDetail::Medium, 4000);
    BuildStrip(S, false, 1, 10200.0, -6500.0, 6500.0, 1000.0, 700.0, 11, 6, 1200.0, ELaneDetail::Low, 5000);
    BuildStrip(S, false, -1, 3800.0, -5500.0, 5500.0, 900.0, 600.0, 7, 5, 1100.0, ELaneDetail::Medium, 6000);
    BuildStrip(S, false, -1, 6400.0, -6500.0, 6500.0, 1000.0, 700.0, 10, 6, 1200.0, ELaneDetail::Low, 7000);
}
} // namespace PWLane
