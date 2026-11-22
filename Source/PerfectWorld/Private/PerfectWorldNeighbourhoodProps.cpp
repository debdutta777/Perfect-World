#include "PerfectWorldNeighbourhoodProps.h"

namespace PWNs
{
namespace
{
constexpr double NsPoleHeight = 900.0; // a utility pole, measured from the road top

/** A footprint rectangle from a centre and half sizes. */
FNsRect NsCentred(double X, double Y, double HalfX, double HalfY)
{
    return FNsRect(X - HalfX, Y - HalfY, X + HalfX, Y + HalfY);
}
} // namespace

void NsBench(FNsSink& K, double CentreX, double CentreY, double Length, bool bAlongX, ENsFace SitterFaces)
{
    const double HalfL = 0.5 * Length;
    const double HalfD = 21.0;
    const double Z0 = NsPath;
    const FVector Facing = NsNormal(SitterFaces);
    // The backrest sits on the side opposite the way the sitter faces, in the across direction.
    const double BackSign = bAlongX ? -Facing.Y : -Facing.X;
    const FNsRect Seat = bAlongX ? NsCentred(CentreX, CentreY, HalfL, HalfD) : NsCentred(CentreX, CentreY, HalfD, HalfL);
    K.Slab(ELaneColour::WoodLight, Seat, Z0 + 42.0, Z0 + 47.0);
    // Two end supports, solid to the player together with the seat.
    for (const double End : {-1.0, 1.0})
    {
        const double Offset = End * (HalfL - 8.0);
        const FNsRect Leg = bAlongX ? NsCentred(CentreX + Offset, CentreY, 4.0, HalfD - 3.0) : NsCentred(CentreX, CentreY + Offset, HalfD - 3.0, 4.0);
        K.Slab(ELaneColour::Rail, Leg, Z0, Z0 + 42.0);
    }
    K.Prop(Seat.X0, Seat.Y0, Z0, Seat.X1, Seat.Y1, Z0 + 47.0);
    // Backrest: a thin slatted panel along the back edge.
    FNsRect Back;
    if (bAlongX)
    {
        const double Y = CentreY + BackSign * (HalfD - 2.0);
        Back = FNsRect(CentreX - HalfL, Y - 2.0, CentreX + HalfL, Y + 2.0);
    }
    else
    {
        const double X = CentreX + BackSign * (HalfD - 2.0);
        Back = FNsRect(X - 2.0, CentreY - HalfL, X + 2.0, CentreY + HalfL);
    }
    K.Slab(ELaneColour::WoodLight, Back, Z0 + 47.0, Z0 + 88.0);
    K.Slab(ELaneColour::TrimDark, Back, Z0 + 62.0, Z0 + 66.0);
    K.Prop(Back.X0, Back.Y0, Z0 + 47.0, Back.X1, Back.Y1, Z0 + 88.0);
}

void NsTree(FNsSink& K, double X, double Y, double BaseZ)
{
    K.Box(ELaneColour::Trunk, X - 13.0, Y - 13.0, BaseZ, X + 13.0, Y + 13.0, BaseZ + 330.0);
    K.Prop(X - 13.0, Y - 13.0, BaseZ, X + 13.0, Y + 13.0, BaseZ + 330.0);
    K.Box(ELaneColour::LeafDark, X - 120.0, Y - 120.0, BaseZ + 300.0, X + 120.0, Y + 120.0, BaseZ + 410.0);
    K.Box(ELaneColour::LeafLight, X - 90.0, Y - 90.0, BaseZ + 400.0, X + 90.0, Y + 90.0, BaseZ + 500.0);
    K.Box(ELaneColour::LeafDark, X - 60.0, Y - 60.0, BaseZ + 495.0, X + 60.0, Y + 60.0, BaseZ + 560.0);
    K.Tilted(ELaneColour::LeafDark, FVector(X, Y, BaseZ + 355.0), FVector(220.0, 220.0, 100.0), FRotator(0.0, 45.0, 0.0));
    K.Tilted(ELaneColour::LeafLight, FVector(X, Y, BaseZ + 450.0), FVector(170.0, 170.0, 90.0), FRotator(0.0, 30.0, 0.0));
}

void NsLampPost(FNsSink& K, double X, double Y, double BaseZ, ENsFace ArmToward)
{
    constexpr double Height = 600.0;
    K.Box(ELaneColour::TrimDark, X - 7.0, Y - 7.0, BaseZ, X + 7.0, Y + 7.0, BaseZ + Height);
    K.Box(ELaneColour::Concrete, X - 12.0, Y - 12.0, BaseZ, X + 12.0, Y + 12.0, BaseZ + 40.0);
    K.Prop(X - 12.0, Y - 12.0, BaseZ, X + 12.0, Y + 12.0, BaseZ + Height);
    const FVector Dir = NsNormal(ArmToward);
    const double Ex = X + Dir.X * 90.0;
    const double Ey = Y + Dir.Y * 90.0;
    const FNsRect Arm = FNsRect(FMath::Min(X, Ex) - 3.0, FMath::Min(Y, Ey) - 3.0, FMath::Max(X, Ex) + 3.0, FMath::Max(Y, Ey) + 3.0);
    K.Slab(ELaneColour::TrimDark, Arm, BaseZ + Height - 8.0, BaseZ + Height - 2.0);
    K.Slab(ELaneColour::GlassLit, NsCentred(Ex, Ey, 20.0, 14.0), BaseZ + Height - 14.0, BaseZ + Height - 8.0);
    K.Slab(ELaneColour::TankLid, NsCentred(Ex, Ey, 24.0, 18.0), BaseZ + Height - 8.0, BaseZ + Height - 4.0);
}

double NsPowerPole(FNsSink& K, double X, double Y, bool bArmAlongX, bool bTransformer)
{
    const double Z0 = NsGround;
    K.Box(ELaneColour::Concrete, X - 14.0, Y - 14.0, Z0, X + 14.0, Y + 14.0, Z0 + NsPoleHeight);
    K.Prop(X - 15.0, Y - 15.0, Z0, X + 15.0, Y + 15.0, Z0 + NsPoleHeight);
    const double ArmHalf = 75.0;
    const FNsRect Arm = bArmAlongX ? NsCentred(X, Y, ArmHalf, 7.0) : NsCentred(X, Y, 7.0, ArmHalf);
    K.Slab(ELaneColour::TrimDark, Arm, Z0 + 825.0, Z0 + 837.0);
    for (const double Offset : {-60.0, 0.0, 60.0})
    {
        const FNsRect Stub = bArmAlongX ? NsCentred(X + Offset, Y, 4.0, 4.0) : NsCentred(X, Y + Offset, 4.0, 4.0);
        K.Slab(ELaneColour::TrimLight, Stub, Z0 + 837.0, Z0 + 851.0);
    }
    if (bTransformer)
    {
        K.Slab(ELaneColour::TankLid, NsCentred(X, Y, 30.0, 30.0), Z0 + 590.0, Z0 + 690.0);
    }
    return Z0 + 849.0;
}

void NsCompoundWall(FNsSink& K, const FNsRect& R, double Height, ELaneColour Colour)
{
    K.Slab(Colour, R, NsGround, NsGround + Height);
    K.Slab(ELaneColour::TrimLight, R.Grown(3.0), NsGround + Height, NsGround + Height + 8.0);
}

void NsNoticeBoard(FNsSink& K, double CentreX, double CentreY, double Width, ENsFace Face, uint32 Seed)
{
    const bool bAlongX = Face == ENsFace::PlusY || Face == ENsFace::MinusY;
    const FNsRect Panel = bAlongX ? NsCentred(CentreX, CentreY, 0.5 * Width, 3.0) : NsCentred(CentreX, CentreY, 3.0, 0.5 * Width);
    const double Z0 = NsPath;
    K.Slab(ELaneColour::WoodLight, Panel, Z0 + 110.0, Z0 + 210.0);
    K.Slab(ELaneColour::TrimDark, Panel.Grown(3.0), Z0 + 106.0, Z0 + 110.0);
    K.Slab(ELaneColour::TrimDark, Panel.Grown(3.0), Z0 + 210.0, Z0 + 214.0);
    for (const double End : {-1.0, 1.0})
    {
        const double Offset = End * (0.5 * Width - 8.0);
        const FNsRect Post = bAlongX ? NsCentred(CentreX + Offset, CentreY, 5.0, 5.0) : NsCentred(CentreX, CentreY + Offset, 5.0, 5.0);
        K.Slab(ELaneColour::TrimDark, Post, Z0, Z0 + 214.0);
        K.Prop(Post.X0, Post.Y0, Z0, Post.X1, Post.Y1, Z0 + 214.0);
    }
    K.Prop(Panel.X0, Panel.Y0, Z0 + 106.0, Panel.X1, Panel.Y1, Z0 + 214.0);
    // Paper notices pinned in rows on the facing side; each sheet a few centimetres proud.
    const FVector Facing = NsNormal(Face);
    static const ELaneColour Paper[] = {ELaneColour::TrimLight, ELaneColour::ClothMustard, ELaneColour::TrimLight, ELaneColour::ClothTeal};
    const int32 Columns = FMath::Max(2, static_cast<int32>(Width / 40.0));
    for (int32 Row = 0; Row < 2; ++Row)
    {
        for (int32 Col = 0; Col < Columns; ++Col)
        {
            const uint32 H = NsHash(Seed, Row, Col);
            if (H % 7u == 0u)
            {
                continue;
            }
            const double Along = -0.5 * Width + 14.0 + (Col + 0.5) * ((Width - 28.0) / Columns);
            const double SheetW = 22.0;
            const double Z = Z0 + 118.0 + Row * 46.0;
            const double Px = CentreX + (bAlongX ? Along : 0.0) + Facing.X * 4.0;
            const double Py = CentreY + (bAlongX ? 0.0 : Along) + Facing.Y * 4.0;
            const FNsRect Sheet = bAlongX ? NsCentred(Px, Py, 0.5 * SheetW, 1.0) : NsCentred(Px, Py, 1.0, 0.5 * SheetW);
            K.Slab(Paper[H % 4u], Sheet, Z, Z + 32.0);
        }
    }
}
} // namespace PWNs
