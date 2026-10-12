#include "PerfectWorldLaneProps.h"

namespace PWLane
{
namespace
{
constexpr double GZ = GroundZ;
constexpr double PoleHeight = 900.0;  // a 9 m street pole
constexpr double PoleTopWire = 849.0; // wire height above the ground at the cross-arm
constexpr double PoleOffsetY = 268.0; // pole centre from the road centre line, on the footpath edge

struct FPole
{
    double X;
    int32 Side;
    bool bTransformer;
};

// Poles alternate sides so the wires run diagonally across the lane. None stands in an alley mouth.
const FPole Poles[] = {{-2100.0, 1, false}, {-700.0, -1, true},  {700.0, 1, false},   {2100.0, -1, true},
                       {3500.0, 1, false},  {5200.0, -1, false}, {6000.0, 1, false}};

const double ArmOffsets[3] = {-60.0, 0.0, 60.0};

void AddPole(FLaneBlocks& K, const FPole& P)
{
    const double Y = P.Side * PoleOffsetY;
    K.Box(ELaneColour::Concrete, FVector(P.X - 14.0, Y - 14.0, GZ), FVector(P.X + 14.0, Y + 14.0, GZ + PoleHeight));
    K.Box(ELaneColour::TrimDark, FVector(P.X - 7.0, Y - 75.0, GZ + 825.0), FVector(P.X + 7.0, Y + 75.0, GZ + 837.0));
    for (const double Offset : ArmOffsets)
    {
        K.Box(ELaneColour::TrimLight, FVector(P.X - 4.0, Y + Offset - 4.0, GZ + 837.0),
              FVector(P.X + 4.0, Y + Offset + 4.0, GZ + 851.0));
    }
    if (P.bTransformer)
    {
        K.Box(ELaneColour::TankLid, FVector(P.X - 30.0, Y - 30.0, GZ + 590.0), FVector(P.X + 30.0, Y + 30.0, GZ + 690.0));
    }
    // The pole stands on the footpath edge and is solid to the walking player.
    K.Barrier(FVector(P.X - 16.0, Y - 16.0, GZ), FVector(P.X + 16.0, Y + 16.0, GZ + 300.0));
}

void AddPoleWires(FLaneScene& S)
{
    FLaneBlocks& K = S.Blocks;
    for (const FPole& P : Poles)
    {
        AddPole(K, P);
    }

    // Three lines from pole to pole, each sagging a few percent of its span.
    for (int32 I = 0; I + 1 < static_cast<int32>(UE_ARRAY_COUNT(Poles)); ++I)
    {
        const FPole& A = Poles[I];
        const FPole& B = Poles[I + 1];
        for (const double Offset : ArmOffsets)
        {
            const FVector From(A.X, A.Side * PoleOffsetY + Offset, GZ + PoleTopWire);
            const FVector To(B.X, B.Side * PoleOffsetY + Offset, GZ + PoleTopWire);
            K.SaggingWire(ELaneColour::WireBlack, From, To, 0.035 * (To - From).Size(), 2.5, 10);
        }
    }

    // Service drops from each pole to a wall on the opposite side of the lane.
    const double DropX[3] = {-250.0, 100.0, 380.0};
    const double DropZ[3] = {520.0, 600.0, 680.0};
    for (const FPole& P : Poles)
    {
        for (int32 I = 0; I < 3; ++I)
        {
            const double TargetX = FMath::Clamp(P.X + DropX[I], LaneX0 + 50.0, LaneX1 - 50.0);
            const double WallY = -P.Side * S.FrontAbsY(-P.Side, TargetX);
            const FVector From(P.X, P.Side * PoleOffsetY + ArmOffsets[I], GZ + PoleTopWire);
            K.SaggingWire(ELaneColour::WireBlack, From, FVector(TargetX, WallY, GZ + DropZ[I]), 28.0, 2.0, 8);
        }
    }
}

/** Cable-television style wires strung straight between the two facades. */
void AddFacadeWires(FLaneScene& S)
{
    static const double Xs[] = {1200.0, 2400.0, 3700.0, 5600.0};
    for (const double X : Xs)
    {
        const double LeftY = -S.FrontAbsY(-1, X);
        const double RightY = S.FrontAbsY(1, X + 150.0);
        S.Blocks.SaggingWire(ELaneColour::WireBlack, FVector(X, LeftY, GZ + 740.0), FVector(X + 150.0, RightY, GZ + 700.0), 50.0, 2.2, 8);
        S.Blocks.SaggingWire(ELaneColour::WireBlack, FVector(X + 20.0, LeftY, GZ + 690.0), FVector(X + 170.0, RightY, GZ + 650.0), 45.0, 2.0, 8);
    }
}

/** A line of festival bunting across the lane: small diamonds hanging from a sagging wire. */
void AddBunting(FLaneScene& S, double X)
{
    static const ELaneColour Flags[] = {ELaneColour::AwningRed, ELaneColour::GoodsYellow, ELaneColour::AwningGreen,
                                        ELaneColour::AwningBlue, ELaneColour::ClothMagenta, ELaneColour::TrimLight};
    const FVector From(X, -S.FrontAbsY(-1, X), GZ + 610.0);
    const FVector To(X + 60.0, S.FrontAbsY(1, X + 60.0), GZ + 590.0);
    const double Sag = 55.0;
    S.Blocks.SaggingWire(ELaneColour::WireBlack, From, To, Sag, 1.6, 12);
    const int32 Count = 17;
    for (int32 I = 0; I < Count; ++I)
    {
        const double T = (I + 0.5) / Count;
        FVector Point = From + (To - From) * T;
        Point.Z -= 4.0 * Sag * T * (1.0 - T) + 14.0;
        S.Blocks.Tilted(Flags[static_cast<uint32>(I) % UE_ARRAY_COUNT(Flags)], Point, FVector(1.5, 22.0, 22.0), FRotator(0.0, 0.0, 45.0));
    }
}

/** A street tree on the footpath: trunk and stacked, turned canopy blocks, clear of the head height. */
void AddTree(FLaneBlocks& K, double X, int32 Side)
{
    const double Y = Side * 335.0;
    K.Box(ELaneColour::Trunk, FVector(X - 13.0, Y - 13.0, GZ), FVector(X + 13.0, Y + 13.0, GZ + 330.0));
    K.Barrier(FVector(X - 15.0, Y - 15.0, GZ), FVector(X + 15.0, Y + 15.0, GZ + 300.0)); // the canopy starts above head height
    K.Box(ELaneColour::LeafDark, FVector(X - 120.0, Y - 120.0, GZ + 300.0), FVector(X + 120.0, Y + 120.0, GZ + 410.0));
    K.Box(ELaneColour::LeafLight, FVector(X - 90.0, Y - 90.0, GZ + 400.0), FVector(X + 90.0, Y + 90.0, GZ + 500.0));
    K.Box(ELaneColour::LeafDark, FVector(X - 60.0, Y - 60.0, GZ + 495.0), FVector(X + 60.0, Y + 60.0, GZ + 560.0));
    K.Tilted(ELaneColour::LeafDark, FVector(X, Y, GZ + 355.0), FVector(220.0, 220.0, 100.0), FRotator(0.0, 45.0, 0.0));
    K.Tilted(ELaneColour::LeafLight, FVector(X, Y, GZ + 450.0), FVector(170.0, 170.0, 90.0), FRotator(0.0, 30.0, 0.0));
}

/** A yellow taxi of the classic shape (an Ambassador-like saloon, about 436 x 165 x 160), parked along the lane facing +X. */
void AddTaxi(FLaneBlocks& K, double X, double Y)
{
    K.Box(ELaneColour::TaxiYellow, FVector(X - 210.0, Y - 82.5, GZ + 25.0), FVector(X + 210.0, Y + 82.5, GZ + 85.0));
    K.Box(ELaneColour::TaxiGlass, FVector(X - 120.0, Y - 77.0, GZ + 85.0), FVector(X + 95.0, Y + 77.0, GZ + 138.0));
    K.Box(ELaneColour::TaxiYellow, FVector(X - 125.0, Y - 79.0, GZ + 138.0), FVector(X + 100.0, Y + 79.0, GZ + 146.0));
    K.Box(ELaneColour::TrimLight, FVector(X - 10.0, Y - 25.0, GZ + 146.0), FVector(X + 10.0, Y + 25.0, GZ + 160.0));
    // Wheels: about 62 cm tall on a 236 cm wheelbase, tucked just inside the body width.
    for (const double Sx : {-118.0, 118.0})
    {
        for (const double Sy : {-72.0, 72.0})
        {
            K.Box(ELaneColour::TankBlack, FVector(X + Sx - 31.0, Y + Sy - 10.0, GZ), FVector(X + Sx + 31.0, Y + Sy + 10.0, GZ + 62.0));
        }
    }
    K.Box(ELaneColour::Chrome, FVector(X + 210.0, Y - 82.0, GZ + 30.0), FVector(X + 218.0, Y + 82.0, GZ + 46.0));
    K.Box(ELaneColour::Chrome, FVector(X - 218.0, Y - 82.0, GZ + 30.0), FVector(X - 210.0, Y + 82.0, GZ + 46.0));
    K.Box(ELaneColour::TrimLight, FVector(X + 208.0, Y + 42.0, GZ + 58.0), FVector(X + 214.0, Y + 70.0, GZ + 74.0));
    K.Box(ELaneColour::TrimLight, FVector(X + 208.0, Y - 70.0, GZ + 58.0), FVector(X + 214.0, Y - 42.0, GZ + 74.0));
    K.Barrier(FVector(X - 218.0, Y - 84.0, GZ), FVector(X + 218.0, Y + 84.0, GZ + 170.0));
}

/** A scooter of the Activa class (about 190 x 70 x 112), parked facing +X. */
void AddScooter(FLaneBlocks& K, double X, double Y, ELaneColour Body)
{
    K.Box(Body, FVector(X - 85.0, Y - 18.0, GZ + 28.0), FVector(X + 65.0, Y + 18.0, GZ + 62.0));
    K.Box(ELaneColour::TankBlack, FVector(X - 70.0, Y - 16.0, GZ + 62.0), FVector(X - 5.0, Y + 16.0, GZ + 74.0));
    K.Box(Body, FVector(X + 55.0, Y - 14.0, GZ + 62.0), FVector(X + 66.0, Y + 14.0, GZ + 110.0));
    K.Box(ELaneColour::TankLid, FVector(X + 52.0, Y - 34.0, GZ + 104.0), FVector(X + 62.0, Y + 34.0, GZ + 112.0));
    // Wheels: 12 inch, about 50 cm.
    K.Box(ELaneColour::TankBlack, FVector(X + 45.0, Y - 6.0, GZ), FVector(X + 95.0, Y + 6.0, GZ + 50.0));
    K.Box(ELaneColour::TankBlack, FVector(X - 95.0, Y - 6.0, GZ), FVector(X - 45.0, Y + 6.0, GZ + 50.0));
    K.Barrier(FVector(X - 97.0, Y - 36.0, GZ), FVector(X + 97.0, Y + 36.0, GZ + 115.0));
}

/** A blue handcart loaded with crates of produce. */
void AddHandcart(FLaneBlocks& K, double X, double Y)
{
    K.Box(ELaneColour::ShutterBlue, FVector(X - 90.0, Y - 45.0, GZ + 45.0), FVector(X + 90.0, Y + 45.0, GZ + 60.0));
    K.Box(ELaneColour::TankBlack, FVector(X - 30.0, Y - 52.0, GZ), FVector(X + 30.0, Y - 46.0, GZ + 60.0));
    K.Box(ELaneColour::TankBlack, FVector(X - 30.0, Y + 46.0, GZ), FVector(X + 30.0, Y + 52.0, GZ + 60.0));
    K.Box(ELaneColour::WoodLight, FVector(X - 150.0, Y - 30.0, GZ + 56.0), FVector(X - 90.0, Y - 24.0, GZ + 62.0));
    K.Box(ELaneColour::WoodLight, FVector(X - 150.0, Y + 24.0, GZ + 56.0), FVector(X - 90.0, Y + 30.0, GZ + 62.0));
    static const ELaneColour Goods[] = {ELaneColour::GoodsOrange, ELaneColour::GoodsGreen, ELaneColour::GoodsYellow};
    for (int32 I = 0; I < 3; ++I)
    {
        const double A = X - 84.0 + I * 58.0;
        K.Box(Goods[I], FVector(A, Y - 38.0, GZ + 60.0), FVector(A + 52.0, Y + 38.0, GZ + 92.0));
    }
    K.Barrier(FVector(X - 150.0, Y - 52.0, GZ), FVector(X + 92.0, Y + 52.0, GZ + 95.0));
}
} // namespace

void BuildLaneProps(FLaneScene& S)
{
    AddPoleWires(S);
    AddFacadeWires(S);
    AddBunting(S, 1600.0);
    AddBunting(S, 4000.0);

    AddTree(S.Blocks, 1500.0, -1);
    AddTree(S.Blocks, 3900.0, 1);
    AddTree(S.Blocks, -1300.0, 1);
    AddTree(S.Blocks, 5700.0, -1);

    AddTaxi(S.Blocks, 1900.0, -140.0);
    AddScooter(S.Blocks, 3100.0, 170.0, ELaneColour::AwningRed);
    AddScooter(S.Blocks, 1200.0, -190.0, ELaneColour::ClothTeal);
    AddHandcart(S.Blocks, 4100.0, 175.0);
}
} // namespace PWLane
