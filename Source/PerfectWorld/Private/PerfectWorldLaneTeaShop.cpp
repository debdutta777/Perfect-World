#include "PerfectWorldLaneTeaShop.h"

#include "Math/RotationMatrix.h"

namespace PWLane
{
namespace
{
// All names in this anonymous namespace start with Ts: the module builds in unity blobs, where another .cpp's file-local
// helper of the same name and signature would collide.
constexpr double TsFloorW = TeaShopLayout::FloorW;
constexpr double TsCeilingW = TeaShopLayout::CeilingW;
constexpr double TsCeilingTopW = 290.0;      // the 1 cm plaster ceiling ends here; the solid upper storeys start
constexpr double TsProud = 1.5;              // trim stands this far proud of the wall face
constexpr double TsDoorTopW = TsFloorW + 212.0; // 212 cm clear above the floor (the leaf is 210)
constexpr double TsHatchSillW = TsFloorW + 90.0;  // barred hatch windows: 170 wide, sill 90, head 195 above the floor
constexpr double TsHatchHeadW = TsFloorW + 195.0;
constexpr double TsBackSillW = TsFloorW + 75.0;   // the storage window follows the 120 x 135 window contract
constexpr double TsBackHeadW = TsBackSillW + 135.0;
constexpr double TsLightW = 272.0;
constexpr double TsAwningWallZ = 285.0;      // where the awning slope meets the wall
constexpr double TsAwningEdgeZ = 247.0;      // the slope's lower edge
constexpr double TsValanceBottomZ = FootpathHeight + 210.0; // 225: 210 above the footpath
constexpr double TsAwningReach = 110.0;      // leaves 160 cm of the 270 cm footpath beyond it; no poles, only stay rods

struct FTsColourSpan
{
    double A0;
    double A1;
    ELaneColour Colour;
};

/** A door or window, by its clear opening along the run (A0, A1) and in height (W0, W1). */
struct FTsOpening
{
    double A0;
    double A1;
    double W0;
    double W1;
    bool bWindow;
};

/** A straight run of wall. A is the position along the run, P the thickness direction, W the height. */
struct FTsRun
{
    bool bAlongU = true;
    double P0 = 0.0;
    double P1 = 0.0;
    double A0 = 0.0;
    double A1 = 0.0;
    double W0 = 0.0;
    double W1 = TsCeilingTopW;
    int32 OuterSide = 0; // -1: P0 is the street/outside face, +1: P1 is, 0: a partition painted on both faces
    TArray<FTsColourSpan> Low;  // colour of the P0 face along A
    TArray<FTsColourSpan> High; // colour of the P1 face along A
    TArray<FTsOpening> Openings;
};

ELaneColour TsColourAt(const TArray<FTsColourSpan>& Spans, double A)
{
    for (const FTsColourSpan& Span : Spans)
    {
        if (A >= Span.A0 && A <= Span.A1)
        {
            return Span.Colour;
        }
    }
    return Spans.IsEmpty() ? ELaneColour::PlasterWarm : Spans.Last().Colour;
}

/** Draws wall runs: the street colour as the structural body (a World colour, so it blocks), a 1 cm painted veneer inside. */
class FTsShell
{
public:
    FTsShell(FLaneBlocks& InK, const FLaneFrame& InF, ELaneColour InWall) : K(InK), F(InF), Wall(InWall) {}

    void Build(const FTsRun& R) const
    {
        TArray<FTsOpening> Sorted = R.Openings;
        Sorted.Sort([](const FTsOpening& A, const FTsOpening& B) { return A.A0 < B.A0; });
        double Cursor = R.A0;
        for (const FTsOpening& O : Sorted)
        {
            Emit(R, Cursor, O.A0, R.W0, R.W1);
            Emit(R, O.A0, O.A1, R.W0, O.W0);
            Emit(R, O.A0, O.A1, O.W1, R.W1);
            Cursor = O.A1;
        }
        Emit(R, Cursor, R.A1, R.W0, R.W1);
        for (const FTsOpening& O : Sorted)
        {
            Dress(R, O);
        }
    }

private:
    void Put(const FTsRun& R, ELaneColour C, double A0, double A1, double P0, double P1, double W0, double W1) const
    {
        if (R.bAlongU)
        {
            F.Add(K, C, A0, A1, P0, P1, W0, W1);
        }
        else
        {
            F.Add(K, C, P0, P1, A0, A1, W0, W1);
        }
    }

    void Solid(const FTsRun& R, double A0, double A1, double P0, double P1, double W0, double W1) const
    {
        if (R.bAlongU)
        {
            K.PropCollision(F.Pt(A0, P0, W0), F.Pt(A1, P1, W1));
        }
        else
        {
            K.PropCollision(F.Pt(P0, A0, W0), F.Pt(P1, A1, W1));
        }
    }

    void Emit(const FTsRun& R, double A0, double A1, double W0, double W1) const
    {
        if (A1 - A0 < 0.5 || W1 - W0 < 0.5)
        {
            return;
        }
        TArray<double> Marks;
        Marks.Add(A0);
        Marks.Add(A1);
        for (const TArray<FTsColourSpan>* Spans : {&R.Low, &R.High})
        {
            for (const FTsColourSpan& Span : *Spans)
            {
                for (const double Edge : {Span.A0, Span.A1})
                {
                    if (Edge > A0 + 0.5 && Edge < A1 - 0.5)
                    {
                        Marks.Add(Edge);
                    }
                }
            }
        }
        Marks.Sort();
        const double VeneerW0 = FMath::Max(W0, TsFloorW);
        const double VeneerW1 = FMath::Min(W1, TsCeilingW);
        const double Mid = 0.5 * (R.P0 + R.P1);
        for (int32 I = 0; I + 1 < Marks.Num(); ++I)
        {
            const double X0 = Marks[I];
            const double X1 = Marks[I + 1];
            if (X1 - X0 < 0.5)
            {
                continue;
            }
            const double Probe = 0.5 * (X0 + X1);
            if (R.OuterSide < 0)
            {
                Put(R, Wall, X0, X1, R.P0, R.P1 - 1.0, W0, W1);
                Put(R, TsColourAt(R.High, Probe), X0, X1, R.P1 - 1.0, R.P1, VeneerW0, VeneerW1);
            }
            else if (R.OuterSide > 0)
            {
                Put(R, Wall, X0, X1, R.P0 + 1.0, R.P1, W0, W1);
                Put(R, TsColourAt(R.Low, Probe), X0, X1, R.P0, R.P0 + 1.0, VeneerW0, VeneerW1);
            }
            else
            {
                Put(R, TsColourAt(R.Low, Probe), X0, X1, R.P0, Mid, W0, W1);
                Put(R, TsColourAt(R.High, Probe), X0, X1, Mid, R.P1, W0, W1);
            }
        }
        if (R.OuterSide == 0)
        {
            Solid(R, A0, A1, R.P0, R.P1, W0, W1); // partitions block through their own boxes; the painted halves have none
        }
    }

    /** Dark trim strips around one opening; a window also gets bars and an invisible glass box so nobody walks through it. */
    void Dress(const FTsRun& R, const FTsOpening& O) const
    {
        const double Q0 = R.P0 - TsProud;
        const double Q1 = R.P1 + TsProud;
        const double Mid = 0.5 * (R.P0 + R.P1);
        const ELaneColour Trim = ELaneColour::TrimDark;
        const double Sill = O.bWindow ? 5.0 : 0.0;
        Put(R, Trim, O.A0 - 5.0, O.A0, Q0, Q1, O.W0 - Sill, O.W1 + 5.0);
        Put(R, Trim, O.A1, O.A1 + 5.0, Q0, Q1, O.W0 - Sill, O.W1 + 5.0);
        Put(R, Trim, O.A0 - 5.0, O.A1 + 5.0, Q0, Q1, O.W1, O.W1 + 5.0);
        if (!O.bWindow)
        {
            return;
        }
        Put(R, Trim, O.A0 - 5.0, O.A1 + 5.0, Q0, Q1, O.W0 - 5.0, O.W0);
        Solid(R, O.A0, O.A1, Mid - 1.0, Mid + 1.0, O.W0, O.W1);
        for (double A = O.A0 + 14.0; A < O.A1 - 6.0; A += 14.0)
        {
            Put(R, Trim, A - 1.0, A + 1.0, Mid - 1.0, Mid + 1.0, O.W0, O.W1);
        }
    }

    FLaneBlocks& K;
    const FLaneFrame& F;
    ELaneColour Wall;
};

FTsRun TsPerimeter(bool bAlongU, int32 OuterSide, double P0, double P1, double A0, double A1)
{
    FTsRun R;
    R.bAlongU = bAlongU;
    R.OuterSide = OuterSide;
    R.P0 = P0;
    R.P1 = P1;
    R.A0 = A0;
    R.A1 = A1;
    return R;
}

/** The shell: perimeter walls with openings, the partition to the back room, floors, ceiling and the solid storeys above. */
void TsBuildShell(FLaneBlocks& K, const FLaneFrame& F, const FLaneBuilding& B)
{
    using namespace TeaShopLayout;
    const FTsShell Shell(K, F, B.Wall);
    const ELaneColour Warm = ELaneColour::PlasterWarm;
    const ELaneColour Pale = ELaneColour::PlasterPale;
    const ELaneColour Kit = ELaneColour::TileKitchen;
    const TArray<FTsColourSpan> SideFace = {{25.0, 330.0, Warm}, {330.0, 600.0, Kit}, {600.0, 875.0, Pale}};

    // Front wall (street face): two barred hatch windows either side of the entrance.
    FTsRun Front = TsPerimeter(true, -1, 0.0, 25.0, 0.0, B.Width);
    Front.High.Add(FTsColourSpan{0.0, B.Width, Warm});
    Front.Openings = {{60.0, 230.0, TsHatchSillW, TsHatchHeadW, true},
                      {DoorCentreU - DoorHalfClear, DoorCentreU + DoorHalfClear, TsFloorW, TsDoorTopW, false},
                      {470.0, 640.0, TsHatchSillW, TsHatchHeadW, true}};
    Shell.Build(Front);

    // Back wall: one window for the storage room.
    FTsRun Back = TsPerimeter(true, 1, B.Depth - 25.0, B.Depth, 0.0, B.Width);
    Back.Low.Add(FTsColourSpan{0.0, B.Width, Pale});
    Back.Openings.Add(FTsOpening{140.0, 260.0, TsBackSillW, TsBackHeadW, true});
    Shell.Build(Back);

    // Side walls (shared with the neighbours): no openings.
    FTsRun Left = TsPerimeter(false, -1, 0.0, 25.0, 25.0, B.Depth - 25.0);
    Left.High = SideFace;
    Shell.Build(Left);
    FTsRun Right = TsPerimeter(false, 1, B.Width - 25.0, B.Width, 25.0, B.Depth - 25.0);
    Right.Low = SideFace;
    Shell.Build(Right);

    // The partition between the prep area and the back room: 12 thick, an open doorway for the staff (no leaf).
    FTsRun Across = TsPerimeter(true, 0, PartitionV0, PartitionV1, 25.0, B.Width - 25.0);
    Across.W0 = TsFloorW;
    Across.W1 = TsCeilingW;
    Across.Low.Add(FTsColourSpan{25.0, B.Width, Kit});
    Across.High.Add(FTsColourSpan{25.0, B.Width, Pale});
    Across.Openings.Add(FTsOpening{PartitionDoorCentreU - PartitionDoorHalfClear, PartitionDoorCentreU + PartitionDoorHalfClear, TsFloorW, TsDoorTopW, false});
    Shell.Build(Across);

    // Floors: red oxide in the customer room and the back room, pale tile from the counter line back (the staff floor).
    F.Add(K, ELaneColour::FloorOxide, 25.0, B.Width - 25.0, 25.0, ServiceCounter.V0, 0.0, TsFloorW);
    F.Add(K, ELaneColour::FloorTile, 25.0, B.Width - 25.0, ServiceCounter.V0, PartitionV0, 0.0, TsFloorW);
    F.Add(K, ELaneColour::FloorOxide, 25.0, B.Width - 25.0, PartitionV0, B.Depth - 25.0, 0.0, TsFloorW);
    K.PropCollision(F.Pt(25.0, 25.0, 0.0), F.Pt(B.Width - 25.0, B.Depth - 25.0, TsFloorW)); // one slab under every room

    // Ceiling: 1 cm of plaster under the solid upper storeys, which also close the top of every wall.
    F.Add(K, ELaneColour::CeilingWhite, 25.0, B.Width - 25.0, 25.0, B.Depth - 25.0, TsCeilingW, TsCeilingTopW);
    F.Add(K, B.Wall, 0.0, B.Width, 0.0, B.Depth, TsCeilingTopW, B.RoofZ());
}

/** The street face: plinth, the shop sign, the striped awning on stay rods, and the sign letters. */
void TsBuildFront(FLaneBlocks& K, const FLaneFrame& F, const FLaneBuilding& B)
{
    using namespace TeaShopLayout;
    const double DoorCut = DoorHalfClear + 5.0;
    F.Add(K, ELaneColour::Plinth, 0.0, DoorCentreU - DoorCut, -2.0, 0.0, FootpathHeight, 75.0);
    F.Add(K, ELaneColour::Plinth, DoorCentreU + DoorCut, B.Width, -2.0, 0.0, FootpathHeight, 75.0);

    // Signboard over the entrance and hatches (W 238-280), with "SHANTI CHAI" in pale letter blocks centred on the door.
    F.Add(K, ELaneColour::SignGreen, 40.0, B.Width - 40.0, -8.0, 0.0, 238.0, 280.0);
    static const double Letters[] = {22.0, 18.0, 24.0, 16.0, 20.0, 0.0, 22.0, 24.0, 20.0, 16.0, 10.0}; // 0 is the word gap
    double Total = 0.0;
    for (const double L : Letters)
    {
        Total += (L > 0.0 ? L : 22.0) + 8.0;
    }
    double U = DoorCentreU - 0.5 * Total;
    for (const double L : Letters)
    {
        if (L > 0.0)
        {
            F.Add(K, ELaneColour::TrimLight, U, U + L, -10.0, -8.0, 250.0, 268.0);
        }
        U += (L > 0.0 ? L : 22.0) + 8.0;
    }

    // The awning: alternating stripes on a gentle slope; the valance's lower edge is 210 above the footpath.
    const double Run = TsAwningReach - 6.0;
    const FVector Slope = (F.V * -Run + FVector(0.0, 0.0, TsAwningEdgeZ - TsAwningWallZ)).GetSafeNormal();
    const double SlopeLength = FMath::Sqrt(Run * Run + (TsAwningWallZ - TsAwningEdgeZ) * (TsAwningWallZ - TsAwningEdgeZ));
    const FRotator Tilt = FRotationMatrix::MakeFromXY(F.U, Slope).Rotator();
    constexpr double AwningU0 = 20.0;
    const double AwningU1 = B.Width - 20.0;
    const int32 Stripes = FMath::Max(2, FMath::CeilToInt((AwningU1 - AwningU0) / 36.0));
    const double StripeWidth = (AwningU1 - AwningU0) / Stripes;
    for (int32 I = 0; I < Stripes; ++I)
    {
        K.Tilted((I % 2 == 0) ? ELaneColour::AwningBlue : ELaneColour::TrimLight,
                 F.Pt(AwningU0 + (I + 0.5) * StripeWidth, -(TsAwningReach + 6.0) * 0.5, 0.5 * (TsAwningWallZ + TsAwningEdgeZ)),
                 FVector(StripeWidth, SlopeLength, 4.0), Tilt);
    }
    F.Add(K, ELaneColour::AwningBlue, AwningU0, AwningU1, -TsAwningReach - 4.0, -TsAwningReach, TsValanceBottomZ, TsAwningEdgeZ + 2.0);
    for (const double RodU : {AwningU0, AwningU1})
    {
        K.Wire(ELaneColour::TrimDark, F.Pt(RodU, -3.0, 262.0), F.Pt(RodU, -TsAwningReach + 3.0, 232.0), 3.0); // all above 230: nothing on the footpath
    }
}

/** A ceiling fan: rod, hub and three blades whose underside is 244 cm above the floor. */
void TsAddFan(FLaneBlocks& K, const FLaneFrame& F, double U, double V)
{
    K.Box(ELaneColour::TrimDark, F.Pt(U - 2.0, V - 2.0, 268.0), F.Pt(U + 2.0, V + 2.0, TsCeilingW));
    K.Box(ELaneColour::TrimDark, F.Pt(U - 9.0, V - 9.0, 255.0), F.Pt(U + 9.0, V + 9.0, 268.0));
    for (int32 Blade = 0; Blade < 3; ++Blade)
    {
        const double Yaw = 20.0 + 120.0 * static_cast<double>(Blade);
        const double Radians = FMath::DegreesToRadians(Yaw);
        const FVector Dir(FMath::Cos(Radians), FMath::Sin(Radians), 0.0);
        K.Tilted(ELaneColour::WoodLight, F.Pt(U, V, 260.0) + Dir * 39.0, FVector(60.0, 12.0, 2.0), FRotator(0.0, Yaw, 0.0));
    }
}

/** A ceiling fitting and its warm point light. */
void TsAddLight(FLaneScene& S, const FLaneFrame& F, double U, double V, float Radius, float Lumens)
{
    S.Blocks.Box(ELaneColour::TrimLight, F.Pt(U - 11.0, V - 11.0, 285.0), F.Pt(U + 11.0, V + 11.0, TsCeilingW));
    S.Blocks.Box(ELaneColour::GlassLit, F.Pt(U - 8.0, V - 8.0, 281.0), F.Pt(U + 8.0, V + 8.0, 285.0));
    FLaneRoomLight Light;
    Light.Position = F.Pt(U, V, TsLightW);
    Light.Radius = Radius;
    Light.Lumens = Lumens;
    S.RoomLights.Add(Light);
}

/** The shutter door spec: hinge, closed direction and default swing (into the shop) in the same convention as the home's doors. */
void TsAddShutterDoor(FLaneScene& S, const FLaneFrame& F)
{
    FLaneDoorSpec Spec;
    Spec.Id = FName(TeaShopShutterDoorId);
    Spec.Hinge = F.Pt(TeaShopLayout::ShutterHingeU, TeaShopLayout::ShutterHingeV, TsFloorW);
    const FVector Dir = (-F.U).GetSafeNormal(); // the closed leaf runs from the hinge toward -U, across the doorway
    Spec.ClosedYaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
    const FVector LocalY(-Dir.Y, Dir.X, 0.0); // the actor's local +Y is its closed direction turned by +90 degrees of yaw
    Spec.PreferredSwing = FVector::DotProduct(F.V, LocalY) >= 0.0 ? 1.0 : -1.0; // default swing is into the shop (+V)
    Spec.bTwoWay = true; // a customer must be able to leave after closing time
    S.Doors.Add(Spec);
}

FVector2D TsUnit(double U, double V)
{
    return FVector2D(U, V).GetSafeNormal();
}
} // namespace

bool IsTeaShopBuilding(const FLaneBuilding& B)
{
    return B.Side == 1 && B.Index == 3 && B.Detail == ELaneDetail::Full && FMath::IsNearlyEqual(B.Width, 700.0) &&
           FMath::IsNearlyEqual(B.Depth, 900.0) && FMath::IsNearlyEqual(B.Setback, 120.0) && B.Storeys >= 2 &&
           FMath::IsNearlyEqual(B.Frame.Origin.X, 600.0, 1.0) && FMath::IsNearlyEqual(B.Frame.Origin.Y, 520.0, 1.0);
}

void BuildTeaShop(FLaneScene& S, const FLaneBuilding& B)
{
    FLaneBlocks& K = S.Blocks;
    const FLaneFrame& F = B.Frame;
    TsBuildShell(K, F, B);
    TsBuildFront(K, F, B);

    for (const FTeaShopPiece& P : TeaShopData::PieceTable)
    {
        F.Add(K, P.Colour, P.U0, P.U1, P.V0, P.V1, TsFloorW + P.H0, TsFloorW + P.H1);
        if (P.bSolid)
        {
            K.PropCollision(F.Pt(P.U0, P.V0, TsFloorW + P.H0), F.Pt(P.U1, P.V1, TsFloorW + P.H1));
        }
    }

    // Two fans and the warm ceiling lights: customer room (two), prep area (two, one over the staff corridor), storage.
    TsAddFan(K, F, 350.0, 180.0);
    TsAddFan(K, F, 350.0, 470.0);
    TsAddLight(S, F, 150.0, 180.0, 240.0f, 900.0f);
    TsAddLight(S, F, 550.0, 180.0, 240.0f, 900.0f);
    TsAddLight(S, F, 200.0, 470.0, 250.0f, 800.0f);
    TsAddLight(S, F, 560.0, 470.0, 220.0f, 700.0f);
    TsAddLight(S, F, 330.0, 740.0, 280.0f, 700.0f);
    if (S.LightSpots.Num() < 40)
    {
        S.LightSpots.Add(F.Pt(TeaShopLayout::DoorCentreU, -40.0, 215.0)); // a bulb under the awning, like the open stalls
    }
    TsAddShutterDoor(S, F);
}

bool HasTeaShop(const FLaneScene& S)
{
    const FName Id(TeaShopShutterDoorId);
    for (const FLaneDoorSpec& Door : S.Doors)
    {
        if (Door.Id == Id)
        {
            return true;
        }
    }
    return false;
}

TConstArrayView<FTeaShopPiece> GetTeaShopPieces()
{
    return TConstArrayView<FTeaShopPiece>(TeaShopData::PieceTable, UE_ARRAY_COUNT(TeaShopData::PieceTable));
}

void GetTeaShopAnchors(const FLaneBuilding& B, TArray<FTeaShopAnchor>& Out)
{
    Out.Reset();
    for (const TeaShopData::FTeaShopAnchorSpec& Spec : TeaShopData::AnchorTable)
    {
        FTeaShopAnchor Anchor;
        Anchor.Id = FName(Spec.Id);
        Anchor.Local = FVector2D(Spec.U, Spec.V);
        Anchor.Facing = TsUnit(Spec.FaceU, Spec.FaceV);
        Anchor.World = B.Frame.Pt(Spec.U, Spec.V, TsFloorW);
        const FVector Dir = (B.Frame.U * Anchor.Facing.X + B.Frame.V * Anchor.Facing.Y).GetSafeNormal();
        Anchor.WorldYawDegrees = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
        Out.Add(Anchor);
    }
}

void GetTeaShopRoutes(const FLaneBuilding& B, TArray<FTeaShopRoute>& Out)
{
    Out.Reset();
    for (const TeaShopData::FTeaShopRouteSpec& Spec : TeaShopData::RouteTable)
    {
        FTeaShopRoute Route;
        Route.Id = FName(Spec.Id);
        for (int32 I = 0; I < Spec.Count; ++I)
        {
            Route.Local.Add(FVector2D(Spec.Pts[I][0], Spec.Pts[I][1]));
            Route.World.Add(B.Frame.Pt(Spec.Pts[I][0], Spec.Pts[I][1], TsFloorW));
        }
        Out.Add(Route);
    }
}
} // namespace PWLane
