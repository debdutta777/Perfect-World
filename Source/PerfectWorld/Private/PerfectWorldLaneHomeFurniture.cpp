#include "PerfectWorldLaneHomeFurniture.h"

namespace PWLane
{
namespace
{
constexpr double LightW = 272.0;      // lights hang just under the ceiling fixture

/** Builds in building coordinates (U, V) with heights measured from the finished floor. */
struct FFit
{
    FLaneBlocks& K;
    const FLaneFrame& F;

    /** Decoration: visual only, no collision. */
    void Deco(ELaneColour C, double U0, double U1, double V0, double V1, double H0, double H1) const
    {
        F.Add(K, C, U0, U1, V0, V1, HomeFloorW + H0, HomeFloorW + H1);
    }

    /** A solid piece: the visible box and an explicit collision box of the same size (Pawn, Camera, Visibility). */
    void Solid(ELaneColour C, double U0, double U1, double V0, double V1, double H0, double H1) const
    {
        Deco(C, U0, U1, V0, V1, H0, H1);
        K.PropCollision(F.Pt(U0, V0, HomeFloorW + H0), F.Pt(U1, V1, HomeFloorW + H1));
    }
};

/** A ceiling fan: rod, hub, and three blades whose underside is 244 cm above the floor (the contract needs 240 or more). */
void AddFan(const FFit& T, double U, double V)
{
    T.K.Box(ELaneColour::TrimDark, T.F.Pt(U - 2.0, V - 2.0, 268.0), T.F.Pt(U + 2.0, V + 2.0, HomeCeilingW));
    T.K.Box(ELaneColour::TrimDark, T.F.Pt(U - 9.0, V - 9.0, 255.0), T.F.Pt(U + 9.0, V + 9.0, 268.0));
    for (int32 Blade = 0; Blade < 3; ++Blade)
    {
        const double Yaw = 20.0 + 120.0 * static_cast<double>(Blade);
        const double Radians = FMath::DegreesToRadians(Yaw);
        const FVector Dir(FMath::Cos(Radians), FMath::Sin(Radians), 0.0);
        const FVector Centre = T.F.Pt(U, V, 260.0) + Dir * 39.0;
        T.K.Tilted(ELaneColour::WoodLight, Centre, FVector(60.0, 12.0, 2.0), FRotator(0.0, Yaw, 0.0));
    }
}

/** A ceiling fitting and its point light. */
void AddCeilingLight(FLaneScene& S, const FFit& T, double U, double V, float Radius, float Lumens)
{
    T.K.Box(ELaneColour::TrimLight, T.F.Pt(U - 11.0, V - 11.0, 285.0), T.F.Pt(U + 11.0, V + 11.0, HomeCeilingW));
    T.K.Box(ELaneColour::GlassLit, T.F.Pt(U - 8.0, V - 8.0, 281.0), T.F.Pt(U + 8.0, V + 8.0, 285.0));
    FLaneRoomLight Light;
    Light.Position = T.F.Pt(U, V, LightW);
    Light.Radius = Radius;
    Light.Lumens = Lumens;
    S.RoomLights.Add(Light);
}

/** Living and eating room: U 25-614, V 25-431. Door zones kept clear: the living door (U 530-614, V 145-235) and the sleeping door (U 155-245, V 347-431). */
void FurnishLiving(FLaneScene& S, const FFit& T)
{
    // Divan along the west wall, with a cushion; family photograph above it (the room's personal object).
    T.Solid(ELaneColour::ClothTeal, 27.0, 97.0, 110.0, 260.0, 0.0, 42.0);
    T.Solid(ELaneColour::ClothTeal, 27.0, 45.0, 110.0, 260.0, 42.0, 85.0);
    T.Deco(ELaneColour::ClothMustard, 50.0, 88.0, 125.0, 165.0, 42.0, 50.0);
    T.Deco(ELaneColour::GoodsOrange, 25.0, 28.0, 150.0, 210.0, 125.0, 165.0);
    T.Deco(ELaneColour::TrimLight, 28.0, 29.0, 156.0, 204.0, 131.0, 159.0);

    // The meal surface by the front windows: a 120 x 70 table at 75 high, with two chairs on its south side.
    T.Solid(ELaneColour::WoodLight, 300.0, 420.0, 50.0, 120.0, 70.0, 76.0);
    for (const double U : {303.0, 411.0})
    {
        for (const double V : {53.0, 111.0})
        {
            T.Solid(ELaneColour::DoorWood, U, U + 6.0, V, V + 6.0, 0.0, 70.0);
        }
    }
    T.Deco(ELaneColour::GoodsGreen, 345.0, 375.0, 70.0, 100.0, 76.0, 82.0);
    for (const double U : {308.0, 374.0})
    {
        T.Solid(ELaneColour::DoorWood, U, U + 38.0, 128.0, 166.0, 0.0, 45.0);
        T.Solid(ELaneColour::DoorWood, U, U + 38.0, 162.0, 166.0, 45.0, 90.0);
    }

    // Steel almirah on the west wall, a rug, a ceiling fan and two lights over the two windows.
    T.Solid(ELaneColour::ShutterSteel, 27.0, 87.0, 300.0, 400.0, 0.0, 180.0);
    T.Deco(ELaneColour::TrimDark, 87.0, 88.0, 349.0, 351.0, 5.0, 175.0);
    T.Deco(ELaneColour::ClothMagenta, 130.0, 330.0, 200.0, 290.0, 0.0, 1.0);
    AddFan(T, 300.0, 215.0);
    AddCeilingLight(S, T, 170.0, 215.0, 240.0f, 900.0f);
    AddCeilingLight(S, T, 450.0, 215.0, 240.0f, 900.0f);
}

/** Entry and utility: U 626-975, V 25-431. The front door swings through U 655-745, V 25-115, which stays clear. */
void FurnishEntry(FLaneScene& S, const FFit& T)
{
    // Shoe rack along the front wall under the window; a doormat inside the door.
    T.Solid(ELaneColour::WoodLight, 765.0, 895.0, 26.0, 56.0, 0.0, 38.0);
    T.Deco(ELaneColour::TrimDark, 775.0, 795.0, 30.0, 48.0, 38.0, 44.0);
    T.Deco(ELaneColour::GoodsRed, 830.0, 852.0, 30.0, 48.0, 38.0, 44.0);
    T.Deco(ELaneColour::ClothMustard, 660.0, 740.0, 125.0, 160.0, 0.0, 1.0);

    // The bag hook on the east wall, with the school bag hanging from it (the entry's personal object).
    T.Deco(ELaneColour::DoorWood, 970.0, 975.0, 60.0, 125.0, 148.0, 158.0);
    T.Deco(ELaneColour::Chrome, 966.0, 970.0, 70.0, 74.0, 150.0, 154.0);
    T.Deco(ELaneColour::Chrome, 966.0, 970.0, 108.0, 112.0, 150.0, 154.0);
    T.Deco(ELaneColour::ClothTeal, 956.0, 966.0, 78.0, 108.0, 100.0, 152.0);

    // Utility along the east wall: a washing machine on a concrete stand, a water pot, and a steel storage cupboard.
    T.Solid(ELaneColour::AcUnit, 918.0, 972.0, 150.0, 210.0, 0.0, 85.0);
    T.Solid(ELaneColour::Concrete, 915.0, 973.0, 212.0, 270.0, 0.0, 85.0);
    T.Deco(ELaneColour::PotTerracotta, 930.0, 960.0, 225.0, 255.0, 85.0, 115.0);
    T.Solid(ELaneColour::ShutterSteel, 928.0, 973.0, 290.0, 380.0, 0.0, 180.0);

    AddCeilingLight(S, T, 800.0, 215.0, 240.0f, 800.0f);
}

/** Sleeping and study room: U 25-374, V 443-875. The sleeping door swings through U 155-245, V 443-527. */
void FurnishSleeping(FLaneScene& S, const FFit& T)
{
    // A plank bed on the west wall with a bedspread and pillow, and a cricket bat leaning at its foot (the personal object).
    T.Solid(ELaneColour::DoorWood, 27.0, 117.0, 600.0, 790.0, 0.0, 40.0);
    T.Solid(ELaneColour::ClothMagenta, 29.0, 115.0, 602.0, 788.0, 40.0, 48.0);
    T.Deco(ELaneColour::TrimLight, 32.0, 112.0, 750.0, 786.0, 48.0, 56.0);
    T.Deco(ELaneColour::WoodLight, 119.0, 125.0, 604.0, 616.0, 0.0, 72.0);

    // The study desk under the back window (top 76, below the 90 sill), a chair, books and a lamp.
    T.Solid(ELaneColour::WoodLight, 230.0, 370.0, 817.0, 873.0, 70.0, 76.0);
    T.Solid(ELaneColour::DoorWood, 232.0, 238.0, 819.0, 871.0, 0.0, 70.0);
    T.Solid(ELaneColour::DoorWood, 362.0, 368.0, 819.0, 871.0, 0.0, 70.0);
    T.Deco(ELaneColour::GoodsRed, 330.0, 360.0, 830.0, 860.0, 76.0, 88.0);
    T.Deco(ELaneColour::GoodsYellow, 240.0, 256.0, 830.0, 846.0, 76.0, 100.0);
    T.Solid(ELaneColour::DoorWood, 275.0, 317.0, 760.0, 802.0, 0.0, 45.0);
    T.Solid(ELaneColour::DoorWood, 275.0, 317.0, 756.0, 760.0, 45.0, 90.0);

    // A steel wardrobe against the kitchen partition, and a low bookshelf by the west wall.
    T.Solid(ELaneColour::ShutterSteel, 316.0, 372.0, 470.0, 590.0, 0.0, 185.0);
    T.Solid(ELaneColour::WoodLight, 27.0, 52.0, 460.0, 560.0, 0.0, 120.0);
    T.Deco(ELaneColour::GoodsGreen, 33.0, 49.0, 470.0, 500.0, 120.0, 134.0);

    AddFan(T, 200.0, 600.0);
    AddCeilingLight(S, T, 200.0, 700.0, 280.0f, 800.0f);
}

/** Kitchen: U 386-734, V 443-875. The kitchen door swings through U 455-545, V 443-533. Counters are 90 high. */
void FurnishKitchen(FLaneScene& S, const FFit& T)
{
    // The cooking counter along the east wall: stove, sink and tap, a brass kettle (personal object), a wall shelf.
    T.Solid(ELaneColour::WoodLight, 674.0, 733.0, 545.0, 870.0, 0.0, 85.0);
    T.Solid(ELaneColour::Concrete, 672.0, 733.0, 543.0, 872.0, 85.0, 90.0);
    T.Deco(ELaneColour::TankBlack, 685.0, 725.0, 600.0, 680.0, 90.0, 94.0);
    T.Deco(ELaneColour::SignBlue, 690.0, 700.0, 615.0, 625.0, 94.0, 96.0);
    T.Deco(ELaneColour::Chrome, 685.0, 725.0, 745.0, 805.0, 90.0, 93.0);
    T.Deco(ELaneColour::Chrome, 725.0, 729.0, 770.0, 776.0, 93.0, 115.0);
    T.Deco(ELaneColour::GoodsYellow, 690.0, 712.0, 830.0, 852.0, 90.0, 108.0);
    T.Deco(ELaneColour::WoodLight, 718.0, 733.0, 560.0, 860.0, 150.0, 155.0);
    T.Deco(ELaneColour::GoodsOrange, 722.0, 732.0, 580.0, 600.0, 155.0, 175.0);

    // Storage cabinets and a fridge along the west wall.
    T.Solid(ELaneColour::WoodLight, 388.0, 448.0, 560.0, 740.0, 0.0, 85.0);
    T.Solid(ELaneColour::TrimLight, 388.0, 450.0, 558.0, 742.0, 85.0, 90.0);
    T.Solid(ELaneColour::AcUnit, 388.0, 450.0, 770.0, 860.0, 0.0, 175.0);

    AddCeilingLight(S, T, 560.0, 660.0, 290.0f, 800.0f);
}

/** Washroom: U 746-975, V 443-875. Its door swings through U 815-905, V 443-533. */
void FurnishWashroom(FLaneScene& S, const FFit& T)
{
    // Pedestal basin on the east wall with a mirror and a tumbler (the personal object).
    T.Solid(ELaneColour::AcUnit, 950.0, 968.0, 545.0, 565.0, 0.0, 78.0);
    T.Solid(ELaneColour::AcUnit, 935.0, 973.0, 530.0, 580.0, 78.0, 90.0);
    T.Deco(ELaneColour::Chrome, 973.0, 975.0, 535.0, 575.0, 120.0, 165.0);
    T.Deco(ELaneColour::GoodsGreen, 940.0, 948.0, 545.0, 553.0, 90.0, 100.0);

    // A toilet against the back wall below the ventilator.
    T.Solid(ELaneColour::AcUnit, 850.0, 880.0, 800.0, 840.0, 0.0, 40.0);
    T.Solid(ELaneColour::AcUnit, 848.0, 882.0, 845.0, 873.0, 40.0, 95.0);

    // A shower corner: pipe and head on the east wall, a bucket and a mug.
    T.Deco(ELaneColour::Chrome, 971.0, 974.0, 832.0, 835.0, 100.0, 200.0);
    T.Deco(ELaneColour::Chrome, 958.0, 974.0, 827.0, 839.0, 198.0, 203.0);
    T.Solid(ELaneColour::ShutterBlue, 935.0, 965.0, 800.0, 830.0, 0.0, 30.0);
    T.Deco(ELaneColour::GoodsRed, 945.0, 955.0, 810.0, 820.0, 30.0, 38.0);

    AddCeilingLight(S, T, 860.0, 660.0, 270.0f, 700.0f);
}
} // namespace

void FurnishOpeningHome(FLaneScene& S, const FLaneBuilding& B)
{
    const FFit T{S.Blocks, B.Frame};
    FurnishLiving(S, T);
    FurnishEntry(S, T);
    FurnishSleeping(S, T);
    FurnishKitchen(S, T);
    FurnishWashroom(S, T);
}
} // namespace PWLane
