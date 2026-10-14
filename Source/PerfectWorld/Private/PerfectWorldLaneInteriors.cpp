#include "PerfectWorldLaneInteriors.h"

#include "PerfectWorldLaneHomeFurniture.h"

namespace PWLane
{
namespace
{
constexpr double CeilingTopW = 290.0;      // the 1 cm plaster ceiling ends here; the solid upper storeys start
constexpr double Proud = 1.5;              // door and window frames stand this far proud of each wall face
constexpr double DoorHalfClear = HomeDoorHalfClear;                      // 94 cm clear between the jamb strips (the leaf is 90)
constexpr double DoorClearTop = HomeFloorW + HomeDoorClearHeight;        // 212 cm clear above the floor (the leaf is 210)
constexpr double WindowHalfClear = 60.0;   // 120 x 135 clear, sill 75 above the floor
constexpr double WindowSill = HomeFloorW + 75.0;
constexpr double WindowHead = WindowSill + 135.0;

struct FColourSpan
{
    double A0;
    double A1;
    ELaneColour Colour;
};

/** A door or window, given by its clear opening. The wall is cut 3 cm wider each way and a 5 cm trim strip is laid over the edge. */
struct FOpening
{
    bool bWindow;
    double Centre;
    double Half;
    double W0;
    double W1;
};

struct FCut
{
    double A0;
    double A1;
    double W0;
    double W1;
};

/** A straight run of wall. A is the position along the run, P the thickness direction, W the height. */
struct FRun
{
    bool bAlongU = true;
    double P0 = 0.0;
    double P1 = 0.0;
    double A0 = 0.0;
    double A1 = 0.0;
    double W0 = 0.0;
    double W1 = 0.0;
    int32 OuterSide = 0; // -1: P0 is the street/outside face, +1: P1 is, 0: a partition painted on both faces
    TArray<FColourSpan> Low;  // colour of the P0 face along A (the inner face when OuterSide is +1)
    TArray<FColourSpan> High; // colour of the P1 face along A (the inner face when OuterSide is -1)
    TArray<FOpening> Openings;
};

ELaneColour ColourAt(const TArray<FColourSpan>& Spans, double A)
{
    for (const FColourSpan& Span : Spans)
    {
        if (A >= Span.A0 && A <= Span.A1)
        {
            return Span.Colour;
        }
    }
    return Spans.IsEmpty() ? ELaneColour::PlasterWarm : Spans.Last().Colour;
}

FOpening Door(double Centre)
{
    return FOpening{false, Centre, DoorHalfClear, HomeFloorW, DoorClearTop};
}

FOpening Window(double Centre, double Half = WindowHalfClear, double W0 = WindowSill, double W1 = WindowHead)
{
    return FOpening{true, Centre, Half, W0, W1};
}

/** Draws wall runs: visual boxes by colour, collision as explicit boxes. */
class FShell
{
public:
    FShell(FLaneBlocks& InK, const FLaneFrame& InF, ELaneColour InWall) : K(InK), F(InF), Wall(InWall) {}

    void BuildRun(const FRun& R) const;

private:
    void Put(const FRun& R, ELaneColour C, double A0, double A1, double P0, double P1, double W0, double W1) const
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

    void Solid(const FRun& R, double A0, double A1, double P0, double P1, double W0, double W1) const
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

    void Emit(const FRun& R, double A0, double A1, double W0, double W1) const;
    void Dress(const FRun& R, const FOpening& O) const;

    FLaneBlocks& K;
    const FLaneFrame& F;
    ELaneColour Wall;
};

/** One wall rectangle (already free of openings), split where the face colours change. */
void FShell::Emit(const FRun& R, double A0, double A1, double W0, double W1) const
{
    if (A1 - A0 < 0.5 || W1 - W0 < 0.5)
    {
        return;
    }
    TArray<double> Marks;
    Marks.Add(A0);
    Marks.Add(A1);
    for (const TArray<FColourSpan>* Spans : {&R.Low, &R.High})
    {
        for (const FColourSpan& Span : *Spans)
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

    const double VeneerW0 = FMath::Max(W0, HomeFloorW);
    const double VeneerW1 = FMath::Min(W1, HomeCeilingW);
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
            Put(R, Wall, X0, X1, R.P0, R.P1 - 1.0, W0, W1); // outer face keeps the street colour; blocks as a wall
            Put(R, ColourAt(R.High, Probe), X0, X1, R.P1 - 1.0, R.P1, VeneerW0, VeneerW1);
        }
        else if (R.OuterSide > 0)
        {
            Put(R, Wall, X0, X1, R.P0 + 1.0, R.P1, W0, W1);
            Put(R, ColourAt(R.Low, Probe), X0, X1, R.P0, R.P0 + 1.0, VeneerW0, VeneerW1);
        }
        else
        {
            Put(R, ColourAt(R.Low, Probe), X0, X1, R.P0, Mid, W0, W1);
            Put(R, ColourAt(R.High, Probe), X0, X1, Mid, R.P1, W0, W1);
        }
    }
    if (R.OuterSide == 0)
    {
        Solid(R, A0, A1, R.P0, R.P1, W0, W1); // partitions block through their own boxes; the painted halves have none
    }
}

/** Trim strips, window glass and bars for one opening, and the collision of the 3 cm the strips take from the cut. */
void FShell::Dress(const FRun& R, const FOpening& O) const
{
    const double Q0 = R.P0 - Proud;
    const double Q1 = R.P1 + Proud;
    const double Mid = 0.5 * (R.P0 + R.P1);
    const double L = O.Centre - O.Half;
    const double Rt = O.Centre + O.Half;
    const ELaneColour Trim = ELaneColour::TrimDark;

    if (!O.bWindow)
    {
        Put(R, Trim, L - 5.0, L, Q0, Q1, O.W0, O.W1 + 5.0);
        Put(R, Trim, Rt, Rt + 5.0, Q0, Q1, O.W0, O.W1 + 5.0);
        Put(R, Trim, L - 5.0, Rt + 5.0, Q0, Q1, O.W1, O.W1 + 5.0);
        Solid(R, L - 3.0, L, R.P0, R.P1, O.W0, O.W1);
        Solid(R, Rt, Rt + 3.0, R.P0, R.P1, O.W0, O.W1);
        Solid(R, L - 3.0, Rt + 3.0, R.P0, R.P1, O.W1, O.W1 + 3.0);
        return;
    }

    Put(R, Trim, L - 5.0, L, Q0, Q1, O.W0 - 5.0, O.W1 + 5.0);
    Put(R, Trim, Rt, Rt + 5.0, Q0, Q1, O.W0 - 5.0, O.W1 + 5.0);
    Put(R, Trim, L - 5.0, Rt + 5.0, Q0, Q1, O.W0 - 5.0, O.W0);
    Put(R, Trim, L - 5.0, Rt + 5.0, Q0, Q1, O.W1, O.W1 + 5.0);
    Solid(R, L - 3.0, L, R.P0, R.P1, O.W0, O.W1);
    Solid(R, Rt, Rt + 3.0, R.P0, R.P1, O.W0, O.W1);
    Solid(R, L - 3.0, Rt + 3.0, R.P0, R.P1, O.W0 - 3.0, O.W0);
    Solid(R, L - 3.0, Rt + 3.0, R.P0, R.P1, O.W1, O.W1 + 3.0);
    // The opening is open to the eye (no pane is drawn, so you see out and in). An invisible glass box keeps the walking
    // player, the camera boom and traces from passing through it.
    Solid(R, L, Rt, Mid - 1.0, Mid + 1.0, O.W0, O.W1);
    if (O.Half >= 50.0)
    {
        Put(R, Trim, O.Centre - 2.0, O.Centre + 2.0, Mid - 1.5, Mid + 1.5, O.W0, O.W1);
        for (const double Offset : {-40.0, -20.0, 20.0, 40.0})
        {
            Put(R, Trim, O.Centre + Offset - 1.0, O.Centre + Offset + 1.0, Mid - 1.0, Mid + 1.0, O.W0, O.W1);
        }
    }
}

void FShell::BuildRun(const FRun& R) const
{
    TArray<FCut> Cuts;
    for (const FOpening& O : R.Openings)
    {
        FCut Cut;
        Cut.A0 = O.Centre - O.Half - 3.0;
        Cut.A1 = O.Centre + O.Half + 3.0;
        Cut.W0 = O.bWindow ? O.W0 - 3.0 : O.W0;
        Cut.W1 = O.W1 + 3.0;
        Cuts.Add(Cut);
    }
    Cuts.Sort([](const FCut& A, const FCut& B) { return A.A0 < B.A0; });

    double Cursor = R.A0;
    for (const FCut& Cut : Cuts)
    {
        Emit(R, Cursor, Cut.A0, R.W0, R.W1);
        Emit(R, Cut.A0, Cut.A1, R.W0, Cut.W0);
        Emit(R, Cut.A0, Cut.A1, Cut.W1, R.W1);
        Cursor = Cut.A1;
    }
    Emit(R, Cursor, R.A1, R.W0, R.W1);

    for (const FOpening& O : R.Openings)
    {
        Dress(R, O);
    }
}

FRun PerimeterRun(bool bAlongU, int32 OuterSide, double P0, double P1, double A0, double A1)
{
    FRun R;
    R.bAlongU = bAlongU;
    R.OuterSide = OuterSide;
    R.P0 = P0;
    R.P1 = P1;
    R.A0 = A0;
    R.A1 = A1;
    R.W0 = 0.0;
    R.W1 = CeilingTopW;
    return R;
}

FRun Partition(bool bAlongU, double Line, double A0, double A1)
{
    FRun R;
    R.bAlongU = bAlongU;
    R.P0 = Line - 6.0;
    R.P1 = Line + 6.0;
    R.A0 = A0;
    R.A1 = A1;
    R.W0 = HomeFloorW;
    R.W1 = HomeCeilingW;
    return R;
}

void AddDoorSpec(FLaneScene& S, const FLaneFrame& F, const TCHAR* Id, double HingeU, double HingeV, double CloseU, double CloseV,
                 double SwingU, double SwingV, bool bTwoWay)
{
    FLaneDoorSpec Spec;
    Spec.Id = FName(Id);
    Spec.Hinge = F.Pt(HingeU, HingeV, HomeFloorW);
    const FVector Dir = (F.U * CloseU + F.V * CloseV).GetSafeNormal();
    Spec.ClosedYaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
    // The actor's local +Y is its closed direction turned by +90 degrees of yaw.
    const FVector LocalY(-Dir.Y, Dir.X, 0.0);
    Spec.PreferredSwing = FVector::DotProduct(F.U * SwingU + F.V * SwingV, LocalY) >= 0.0 ? 1.0 : -1.0;
    Spec.bTwoWay = bTwoWay;
    S.Doors.Add(Spec);
}

/** The shell: perimeter and partition walls, floors, ceiling. */
void BuildWalls(FLaneBlocks& K, const FLaneFrame& F, const FLaneBuilding& B)
{
    const FShell Shell(K, F, B.Wall);
    const double W = B.Width;
    const double D = B.Depth;
    const ELaneColour Warm = ELaneColour::PlasterWarm;
    const ELaneColour Pale = ELaneColour::PlasterPale;
    const ELaneColour Kit = ELaneColour::TileKitchen;
    const ELaneColour Wash = ELaneColour::TileWash;

    // Front wall (the street face): door at U 700, windows for the living room and the entry.
    FRun Front = PerimeterRun(true, -1, 0.0, 25.0, 0.0, W);
    Front.High = {{0.0, 620.0, Warm}, {620.0, W, Pale}};
    Front.Openings = {Window(170.0), Window(450.0), Door(HomeFrontDoorU), Window(880.0)};
    Shell.BuildRun(Front);

    // Back wall: a window each for the sleeping room and the kitchen, a high ventilator for the washroom.
    FRun Back = PerimeterRun(true, 1, D - 25.0, D, 0.0, W);
    Back.Low = {{0.0, 380.0, Warm}, {380.0, 740.0, Kit}, {740.0, W, Wash}};
    Back.Openings = {Window(300.0), Window(560.0), Window(860.0, 30.0, HomeFloorW + 150.0, HomeFloorW + 210.0)};
    Shell.BuildRun(Back);

    // Side walls (shared with the neighbours): no openings.
    FRun Left = PerimeterRun(false, -1, 0.0, 25.0, 25.0, D - 25.0);
    Left.High = {{25.0, 437.0, Warm}, {437.0, D, Warm}};
    Shell.BuildRun(Left);
    FRun Right = PerimeterRun(false, 1, W - 25.0, W, 25.0, D - 25.0);
    Right.Low = {{25.0, 437.0, Pale}, {437.0, D, Wash}};
    Shell.BuildRun(Right);

    // Partitions, 12 cm thick, each face painted for its room.
    FRun Across = Partition(true, HomeAcrossPartitionV, 25.0, W - 25.0); // front rooms | back rooms
    Across.Low = {{25.0, 620.0, Warm}, {620.0, W, Pale}};
    Across.High = {{25.0, 380.0, Warm}, {380.0, 740.0, Kit}, {740.0, W, Wash}};
    Across.Openings = {Door(HomeBedroomDoorU), Door(HomeKitchenDoorU), Door(HomeWashroomDoorU)};
    Shell.BuildRun(Across);

    // Entry | living room. The doorway is at the back of the partition (V 318-412), 16 cm from the back partition's face.
    FRun LivingEntry = Partition(false, HomeLivingPartitionU, 25.0, 431.0);
    LivingEntry.Low.Add(FColourSpan{25.0, 431.0, Warm});
    LivingEntry.High.Add(FColourSpan{25.0, 431.0, Pale});
    LivingEntry.Openings.Add(Door(HomeLivingDoorV));
    Shell.BuildRun(LivingEntry);

    FRun SleepKitchen = Partition(false, 380.0, 443.0, D - 25.0);
    SleepKitchen.Low.Add(FColourSpan{443.0, D, Warm});
    SleepKitchen.High.Add(FColourSpan{443.0, D, Kit});
    Shell.BuildRun(SleepKitchen);

    FRun KitchenWash = Partition(false, 740.0, 443.0, D - 25.0);
    KitchenWash.Low.Add(FColourSpan{443.0, D, Kit});
    KitchenWash.High.Add(FColourSpan{443.0, D, Wash});
    Shell.BuildRun(KitchenWash);

    // Floors: red oxide in the living, entry and sleeping rooms, pale tile in the kitchen and washroom (the slabs meet under the partitions).
    F.Add(K, ELaneColour::FloorOxide, 25.0, 620.0, 25.0, 437.0, 0.0, HomeFloorW);
    F.Add(K, ELaneColour::FloorOxide, 620.0, W - 25.0, 25.0, 437.0, 0.0, HomeFloorW);
    F.Add(K, ELaneColour::FloorOxide, 25.0, 380.0, 437.0, D - 25.0, 0.0, HomeFloorW);
    F.Add(K, ELaneColour::FloorTile, 380.0, 740.0, 437.0, D - 25.0, 0.0, HomeFloorW);
    F.Add(K, ELaneColour::FloorTile, 740.0, W - 25.0, 437.0, D - 25.0, 0.0, HomeFloorW);
    K.PropCollision(F.Pt(25.0, 25.0, 0.0), F.Pt(W - 25.0, D - 25.0, HomeFloorW)); // one slab under every room

    // Ceiling: 1 cm of plaster under the solid upper storeys, which also close the top of every wall.
    F.Add(K, ELaneColour::CeilingWhite, 25.0, W - 25.0, 25.0, D - 25.0, HomeCeilingW, CeilingTopW);
    F.Add(K, B.Wall, 0.0, W, 0.0, D, CeilingTopW, B.RoofZ());
}

/** The street face: plinth, window sills and sunshades, and the lamp over the door. */
void BuildStreetDressing(FLaneBlocks& K, const FLaneFrame& F, const FLaneBuilding& B)
{
    const double DoorCut = DoorHalfClear + 5.0;
    F.Add(K, ELaneColour::Plinth, 0.0, HomeFrontDoorU - DoorCut, -2.0, 0.0, HomeFloorW, 75.0);
    F.Add(K, ELaneColour::Plinth, HomeFrontDoorU + DoorCut, B.Width, -2.0, 0.0, HomeFloorW, 75.0);
    for (const double Centre : {170.0, 450.0, 880.0})
    {
        // A sill slab under the trim (80 to 85), and a 45 cm chhajja over it, as on the upper windows.
        F.Add(K, ELaneColour::TrimLight, Centre - 66.0, Centre + 66.0, -9.0, 0.0, WindowSill - 10.0, WindowSill - 5.0);
        F.Add(K, ELaneColour::TrimLight, Centre - 70.0, Centre + 70.0, -45.0, 0.0, WindowHead + 8.0, WindowHead + 14.0);
    }
    F.Add(K, ELaneColour::GlassLit, HomeFrontDoorU - 8.0, HomeFrontDoorU + 8.0, -12.0, -2.0, 240.0, 258.0); // the lamp over the door
}
} // namespace

bool IsOpeningHome(const FLaneBuilding& B)
{
    return B.Side == -1 && B.Index == 2 && B.Detail == ELaneDetail::Full && FMath::IsNearlyEqual(B.Width, 1000.0) &&
           FMath::IsNearlyEqual(B.Depth, 900.0) && FMath::IsNearlyEqual(B.Setback, 0.0) &&
           FMath::IsNearlyEqual(B.Frame.Origin.X, -300.0, 1.0) && B.Storeys >= 2;
}

void BuildOpeningHome(FLaneScene& S, const FLaneBuilding& B)
{
    FLaneBlocks& K = S.Blocks;
    const FLaneFrame& F = B.Frame;
    BuildWalls(K, F, B);
    BuildStreetDressing(K, F, B);
    FurnishOpeningHome(S, B);

    // Doors: hinge (U, V), the closed leaf's direction, and the side it swings to by default (all in building coordinates).
    AddDoorSpec(S, F, TEXT("door.nabapara.home_front"), 745.0, 12.5, -1.0, 0.0, 0.0, 1.0, false);   // swings inward only
    // The living door: hinged on its back jamb (V 412), closed leaf toward the front (-V), swinging into the entry (+U) only.
    // Open, the leaf lies against the back partition; the living room never has a leaf in it.
    AddDoorSpec(S, F, TEXT("door.nabapara.home_living"), HomeLivingPartitionU, HomeLivingDoorV + HomeDoorHalfClear, 0.0, -1.0, 1.0, 0.0, false);
    AddDoorSpec(S, F, TEXT("door.nabapara.home_bedroom"), 155.0, 437.0, 1.0, 0.0, 0.0, 1.0, true);
    AddDoorSpec(S, F, TEXT("door.nabapara.home_kitchen"), 455.0, 437.0, 1.0, 0.0, 0.0, 1.0, true);
    AddDoorSpec(S, F, TEXT("door.nabapara.home_washroom"), 905.0, 437.0, -1.0, 0.0, 0.0, 1.0, true);
    S.bOpeningHomeBuilt = true;
}
} // namespace PWLane
