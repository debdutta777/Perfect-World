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
    TArray<FHomeSolid>* Log; // receives the footprint of every solid piece (may be null)

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
        if (Log != nullptr)
        {
            FHomeSolid Footprint;
            Footprint.U0 = U0;
            Footprint.U1 = U1;
            Footprint.V0 = V0;
            Footprint.V1 = V1;
            Footprint.H0 = H0;
            Footprint.H1 = H1;
            Log->Add(Footprint);
        }
    }
};

/** A ceiling fan: rod, hub, and three blades whose underside is 244 cm above the floor (the hub's is 240, the contract's minimum). */
void AddFan(const FFit& T, const FHomeFanSpec& Fan)
{
    const double U = Fan.U;
    const double V = Fan.V;
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

/** A ceiling fitting and its point light (the spec goes into Scene.RoomLights; the integrator spawns the actor). */
void AddCeilingLight(FLaneScene& S, const FFit& T, const FHomeLightSpec& Spec)
{
    const double U = Spec.U;
    const double V = Spec.V;
    T.K.Box(ELaneColour::TrimLight, T.F.Pt(U - 11.0, V - 11.0, 285.0), T.F.Pt(U + 11.0, V + 11.0, HomeCeilingW));
    T.K.Box(ELaneColour::GlassLit, T.F.Pt(U - 8.0, V - 8.0, 281.0), T.F.Pt(U + 8.0, V + 8.0, 285.0));
    FLaneRoomLight Light;
    Light.Position = T.F.Pt(U, V, LightW);
    Light.Radius = Spec.AttenuationCm;
    Light.Lumens = Spec.Lumens;
    S.RoomLights.Add(Light);
}

FHomeLightSpec MakeLightSpec(const TCHAR* Room, double U, double V, float AttenuationCm, float Lumens, float ColourTempK,
                             double RoomU0, double RoomU1, double RoomV0, double RoomV1)
{
    FHomeLightSpec Spec;
    Spec.Room = FName(Room);
    Spec.U = U;
    Spec.V = V;
    Spec.AttenuationCm = AttenuationCm;
    Spec.Lumens = Lumens;
    Spec.ColourTempK = ColourTempK;
    Spec.RoomU0 = RoomU0;
    Spec.RoomU1 = RoomU1;
    Spec.RoomV0 = RoomV0;
    Spec.RoomV1 = RoomV1;
    return Spec;
}

/**
 * Living and eating room: U 25-614, V 25-431. Door zones kept clear: the sleeping door (U 145-247, V 345-437) and the
 * kitchen door (U 445-547, V 345-437) swing into it; the living door no longer does (it opens into the entry).
 */
void FurnishLiving(const FFit& T)
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
    // On the table: a tiffin box by the west chair, and today's newspaper in front of the east chair (the readable object).
    T.Deco(ELaneColour::GoodsGreen, 322.0, 348.0, 72.0, 98.0, 76.0, 82.0);
    T.Deco(ELaneColour::TrimLight, 372.0, 412.0, 62.0, 94.0, 76.0, 77.2);
    T.Deco(ELaneColour::TrimDark, 375.0, 409.0, 84.0, 91.0, 77.2, 77.8);
    for (const double U : {308.0, 374.0})
    {
        T.Solid(ELaneColour::DoorWood, U, U + 38.0, 128.0, 166.0, 0.0, 45.0);
        T.Solid(ELaneColour::DoorWood, U, U + 38.0, 162.0, 166.0, 45.0, 90.0);
    }

    // Steel almirah on the west wall and a rug. (The fan and the lights come from the specs.)
    T.Solid(ELaneColour::ShutterSteel, 27.0, 87.0, 300.0, 400.0, 0.0, 180.0);
    T.Deco(ELaneColour::TrimDark, 87.0, 88.0, 349.0, 351.0, 5.0, 175.0);
    T.Deco(ELaneColour::ClothMagenta, 130.0, 330.0, 200.0, 290.0, 0.0, 1.0);
}

/**
 * Entry and utility: U 626-975, V 25-431. The front door swings through U 653-755, V 10-105. The upper-floor stair's
 * footprint (HomeStairSpec: U 795-975, V 25-315) is kept empty and marked by a darker floor patch; the route from the
 * front door to the living doorway runs between the shoe rack and the stair, 133 cm wide.
 */
void FurnishEntry(const FFit& T)
{
    const FHomeStairSpec Stair = HomeStairSpec();
    T.Deco(ELaneColour::Plinth, Stair.U0, Stair.U1, Stair.V0, Stair.V1, 0.0, 0.6); // the stair's floor patch; nothing stands on it

    // A shoe rack on the west wall beyond the door's swing, and a doormat inside the door.
    T.Solid(ELaneColour::WoodLight, 628.0, 662.0, 125.0, 215.0, 0.0, 38.0);
    T.Deco(ELaneColour::TrimDark, 633.0, 653.0, 135.0, 155.0, 38.0, 44.0);
    T.Deco(ELaneColour::GoodsRed, 633.0, 653.0, 175.0, 195.0, 38.0, 44.0);
    T.Deco(ELaneColour::ClothMustard, 665.0, 735.0, 35.0, 75.0, 0.0, 1.0);

    // The bag hook on the west wall, with the school bag hanging from it (the entry's personal object).
    T.Deco(ELaneColour::DoorWood, 626.0, 631.0, 225.0, 290.0, 148.0, 158.0);
    T.Deco(ELaneColour::Chrome, 631.0, 635.0, 235.0, 239.0, 150.0, 154.0);
    T.Deco(ELaneColour::Chrome, 631.0, 635.0, 273.0, 277.0, 150.0, 154.0);
    T.Deco(ELaneColour::ClothTeal, 635.0, 645.0, 243.0, 273.0, 100.0, 152.0);

    // Utility on the east wall below the stair, clear of the washroom door's swing: a washing machine, and a water pot on a concrete stand.
    T.Solid(ELaneColour::AcUnit, 920.0, 972.0, 322.0, 382.0, 0.0, 85.0);
    T.Solid(ELaneColour::Concrete, 918.0, 973.0, 386.0, 428.0, 0.0, 85.0);
    T.Deco(ELaneColour::PotTerracotta, 930.0, 960.0, 392.0, 422.0, 85.0, 115.0);
}

/** Sleeping and study room: U 25-374, V 443-875. The sleeping door swings through U 145-247, V 437-529. */
void FurnishSleeping(const FFit& T)
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
}

/** Kitchen: U 386-734, V 443-875. The kitchen door swings through U 445-547, V 437-529. Counters are 90 high. */
void FurnishKitchen(const FFit& T)
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
}

/** Washroom: U 746-975, V 443-875. Its door swings through U 813-915, V 437-529. */
void FurnishWashroom(const FFit& T)
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
}
} // namespace

FHomeAdultSpot HomeAdultSpot()
{
    FHomeAdultSpot Spot;
    Spot.U = 520.0;
    Spot.V = 290.0;
    Spot.FaceU = 620.0; // the living doorway's threshold centre
    Spot.FaceV = 365.0;
    return Spot;
}

FVector HomeWorldPoint(const FLaneFrame& Frame, double U, double V, double HeightCm)
{
    return Frame.Pt(U, V, HomeFloorW + HeightCm);
}

FVector HomeAdultSpotWorld(const FLaneFrame& Frame)
{
    const FHomeAdultSpot Spot = HomeAdultSpot();
    return HomeWorldPoint(Frame, Spot.U, Spot.V, 0.0);
}

double HomeAdultSpotYaw(const FLaneFrame& Frame)
{
    const FHomeAdultSpot Spot = HomeAdultSpot();
    const FVector Direction = Frame.U * (Spot.FaceU - Spot.U) + Frame.V * (Spot.FaceV - Spot.V);
    return FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
}

TArray<FHomeSeatSpec> HomeSeatSpecs()
{
    TArray<FHomeSeatSpec> Seats;

    // The divan on the west wall: sit facing the room (+U), stand up into the open floor in front of it.
    FHomeSeatSpec Divan;
    Divan.Id = FName(TEXT("seat.nabapara.home_divan"));
    Divan.Verb = TEXT("Sit on the divan");
    Divan.SeatU = 72.0;
    Divan.SeatV = 185.0;
    Divan.SeatHeightCm = 42.0;
    Divan.FaceU = 1.0;
    Divan.FaceV = 0.0;
    Divan.StandU = 145.0;
    Divan.StandV = 185.0;
    Divan.StandClearCm = 45.0;
    Seats.Add(Divan);

    // The east chair at the table: sit facing the table (-V) with the newspaper in front; stand up backwards into the room.
    FHomeSeatSpec Chair;
    Chair.Id = FName(TEXT("seat.nabapara.home_table_chair"));
    Chair.Verb = TEXT("Sit at the table");
    Chair.SeatU = 393.0;
    Chair.SeatV = 147.0;
    Chair.SeatHeightCm = 45.0;
    Chair.FaceU = 0.0;
    Chair.FaceV = -1.0;
    Chair.StandU = 393.0;
    Chair.StandV = 215.0;
    Chair.StandClearCm = 45.0;
    Seats.Add(Chair);
    return Seats;
}

FHomeReadableSpec HomeReadableSpec()
{
    FHomeReadableSpec Paper;
    Paper.Id = FName(TEXT("readable.nabapara.home_newspaper"));
    Paper.ContentId = FName(TEXT("text.nabapara.home_newspaper"));
    Paper.Verb = TEXT("Read the newspaper");
    Paper.U = 392.0;
    Paper.V = 78.0;
    Paper.HeightCm = 77.8;
    Paper.StandU = 470.0; // at the table's east end, 50 cm from its edge
    Paper.StandV = 90.0;
    Paper.StandClearCm = 45.0;
    return Paper;
}

FHomeStairSpec HomeStairSpec()
{
    FHomeStairSpec Stair;
    Stair.Id = FName(TEXT("stair.nabapara.home_upper"));
    Stair.U0 = 795.0;
    Stair.U1 = 975.0;
    Stair.V0 = 25.0;
    Stair.V1 = 315.0;
    Stair.FloorToFloorCm = StoreyHeight;
    Stair.Risers = 17;
    Stair.RiserCm = StoreyHeight / 17.0; // 17.65, the nearest equal risers to the 17.5 reference
    Stair.TreadCm = 25.0;
    Stair.LaneWidthCm = 90.0;
    Stair.FootV = 315.0;
    Stair.LandingV0 = 25.0;
    Stair.LandingV1 = 115.0;
    Stair.UpperEdgeV = 290.0;
    return Stair;
}

TArray<FHomeFanSpec> HomeFanSpecs()
{
    TArray<FHomeFanSpec> Fans;
    Fans.Add(FHomeFanSpec{300.0, 215.0, 70.0, 240.0}); // living and eating room
    Fans.Add(FHomeFanSpec{200.0, 600.0, 70.0, 240.0}); // sleeping and study room
    return Fans;
}

TArray<FHomeLightSpec> HomeLightSpecs()
{
    // One shadow-casting light per fitting. Each reaches no farther than its room's shorter side; the shadows (and the
    // 12 and 25 cm walls) keep it in the room, which the old unshadowed spheres did not.
    TArray<FHomeLightSpec> Lights;
    Lights.Add(MakeLightSpec(TEXT("living"), 170.0, 215.0, 300.0f, 900.0f, 2900.0f, 25.0, 614.0, 25.0, 431.0));
    Lights.Add(MakeLightSpec(TEXT("living"), 450.0, 215.0, 300.0f, 900.0f, 2900.0f, 25.0, 614.0, 25.0, 431.0));
    Lights.Add(MakeLightSpec(TEXT("entry"), 740.0, 235.0, 270.0f, 800.0f, 2900.0f, 626.0, 975.0, 25.0, 431.0));
    Lights.Add(MakeLightSpec(TEXT("sleeping"), 200.0, 700.0, 300.0f, 800.0f, 2900.0f, 25.0, 374.0, 443.0, 875.0));
    Lights.Add(MakeLightSpec(TEXT("kitchen"), 560.0, 660.0, 300.0f, 900.0f, 4000.0f, 386.0, 734.0, 443.0, 875.0));
    Lights.Add(MakeLightSpec(TEXT("washroom"), 860.0, 660.0, 225.0f, 700.0f, 4000.0f, 746.0, 975.0, 443.0, 875.0));
    return Lights;
}

void FurnishOpeningHome(FLaneScene& S, const FLaneBuilding& B, TArray<FHomeSolid>* OutSolids)
{
    const FFit T{S.Blocks, B.Frame, OutSolids};
    FurnishLiving(T);
    FurnishEntry(T);
    FurnishSleeping(T);
    FurnishKitchen(T);
    FurnishWashroom(T);
    for (const FHomeFanSpec& Fan : HomeFanSpecs())
    {
        AddFan(T, Fan);
    }
    for (const FHomeLightSpec& Light : HomeLightSpecs())
    {
        AddCeilingLight(S, T, Light);
    }
}
} // namespace PWLane
