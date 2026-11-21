#include "PerfectWorldLaneCareInterior.h"

namespace PWLane
{
namespace
{
// Every name in this anonymous namespace starts with Care: the module builds in unity blobs, where another .cpp's file-local
// helper of the same name would collide (the home and the tea shop each define their own shell builder).
constexpr double CareCeilingTopW = 290.0;   // the 1 cm plaster ceiling ends here; the solid upper storeys start
constexpr double CareProud = 1.5;           // door and window trim stands this far proud of each wall face
constexpr double CareDoorTopW = HomeFloorW + CareLayout::DoorClearHeight;
constexpr double CareWindowSill = HomeFloorW + 75.0; // 120 x 135 clear, sill 75 above the floor
constexpr double CareWindowHead = CareWindowSill + 135.0;
constexpr double CareLightW = 272.0;        // lights hang just under the ceiling fixture

/** A door or window, by its clear opening. The wall is cut 3 cm wider each way and a 5 cm trim strip is laid over the edge. */
struct FCareOpening
{
    bool bWindow;
    double Centre;
    double Half;
    double W0;
    double W1;
};

/** A straight run of wall; A is the position along the run, P the thickness direction, W the height. One colour per face. */
struct FCareRun
{
    bool bAlongU;
    int32 OuterSide; // -1: P0 is the street/outside face, +1: P1 is, 0: a partition painted on both faces
    double P0;
    double P1;
    double A0;
    double A1;
    ELaneColour Low;  // the P0 face (the inner face when OuterSide is +1)
    ELaneColour High; // the P1 face (the inner face when OuterSide is -1)
    TArray<FCareOpening> Openings;
};

FCareOpening CareDoor(double Centre)
{
    return FCareOpening{false, Centre, CareLayout::DoorHalfClear, HomeFloorW, CareDoorTopW};
}

FCareOpening CareWindow(double Centre, double Half = 60.0, double W0 = CareWindowSill, double W1 = CareWindowHead)
{
    return FCareOpening{true, Centre, Half, W0, W1};
}

/** A perimeter run (P0 to P1 is the 25 cm wall) or, with OuterSide 0, a 12 cm partition (P0 to P1 centred on its line). */
FCareRun CareRun(bool bAlongU, int32 OuterSide, double P0, double P1, double A0, double A1, ELaneColour Low, ELaneColour High,
                 const TArray<FCareOpening>& Openings = TArray<FCareOpening>())
{
    return FCareRun{bAlongU, OuterSide, P0, P1, A0, A1, Low, High, Openings};
}

/** Draws wall runs: the street colour as the structural body (it blocks), a 1 cm painted veneer inside; partitions get their own boxes. */
class FCareShell
{
public:
    FCareShell(FLaneBlocks& InK, const FLaneFrame& InF, ELaneColour InWall) : K(InK), F(InF), Wall(InWall) {}

    void BuildRun(const FCareRun& R) const
    {
        const double W0 = R.OuterSide == 0 ? HomeFloorW : 0.0;
        const double W1 = R.OuterSide == 0 ? HomeCeilingW : CareCeilingTopW;
        TArray<FCareOpening> Sorted = R.Openings;
        Sorted.Sort([](const FCareOpening& A, const FCareOpening& B) { return A.Centre < B.Centre; });
        double Cursor = R.A0;
        for (const FCareOpening& O : Sorted)
        {
            const double Cut0 = O.Centre - O.Half - 3.0;
            const double Cut1 = O.Centre + O.Half + 3.0;
            Emit(R, Cursor, Cut0, W0, W1);
            Emit(R, Cut0, Cut1, W0, O.bWindow ? O.W0 - 3.0 : O.W0);
            Emit(R, Cut0, Cut1, O.W1 + 3.0, W1);
            Cursor = Cut1;
        }
        Emit(R, Cursor, R.A1, W0, W1);
        for (const FCareOpening& O : Sorted)
        {
            Dress(R, O);
        }
    }

private:
    void Put(const FCareRun& R, ELaneColour C, double A0, double A1, double P0, double P1, double W0, double W1) const
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

    void Solid(const FCareRun& R, double A0, double A1, double P0, double P1, double W0, double W1) const
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

    /** One wall rectangle (already free of openings). */
    void Emit(const FCareRun& R, double A0, double A1, double W0, double W1) const
    {
        if (A1 - A0 < 0.5 || W1 - W0 < 0.5)
        {
            return;
        }
        const double VeneerW0 = FMath::Max(W0, HomeFloorW);
        const double VeneerW1 = FMath::Min(W1, HomeCeilingW);
        const double Mid = 0.5 * (R.P0 + R.P1);
        if (R.OuterSide < 0)
        {
            Put(R, Wall, A0, A1, R.P0, R.P1 - 1.0, W0, W1); // the outer face keeps the street colour; it blocks as a wall
            Put(R, R.High, A0, A1, R.P1 - 1.0, R.P1, VeneerW0, VeneerW1);
        }
        else if (R.OuterSide > 0)
        {
            Put(R, Wall, A0, A1, R.P0 + 1.0, R.P1, W0, W1);
            Put(R, R.Low, A0, A1, R.P0, R.P0 + 1.0, VeneerW0, VeneerW1);
        }
        else
        {
            Put(R, R.Low, A0, A1, R.P0, Mid, W0, W1);
            Put(R, R.High, A0, A1, Mid, R.P1, W0, W1);
            Solid(R, A0, A1, R.P0, R.P1, W0, W1); // partitions block through their own boxes; the painted halves have none
        }
    }

    /** Trim strips, window bars and an invisible glass box for one opening, and the collision of the 3 cm the strips take from the cut. */
    void Dress(const FCareRun& R, const FCareOpening& O) const
    {
        const double Q0 = R.P0 - CareProud;
        const double Q1 = R.P1 + CareProud;
        const double Mid = 0.5 * (R.P0 + R.P1);
        const double L = O.Centre - O.Half;
        const double Rt = O.Centre + O.Half;
        const ELaneColour Trim = ELaneColour::TrimDark;
        const double Sill = O.bWindow ? 5.0 : 0.0;
        Put(R, Trim, L - 5.0, L, Q0, Q1, O.W0 - Sill, O.W1 + 5.0);
        Put(R, Trim, Rt, Rt + 5.0, Q0, Q1, O.W0 - Sill, O.W1 + 5.0);
        Put(R, Trim, L - 5.0, Rt + 5.0, Q0, Q1, O.W1, O.W1 + 5.0);
        Solid(R, L - 3.0, L, R.P0, R.P1, O.W0, O.W1);
        Solid(R, Rt, Rt + 3.0, R.P0, R.P1, O.W0, O.W1);
        Solid(R, L - 3.0, Rt + 3.0, R.P0, R.P1, O.W1, O.W1 + 3.0);
        if (!O.bWindow)
        {
            return;
        }
        Put(R, Trim, L - 5.0, Rt + 5.0, Q0, Q1, O.W0 - 5.0, O.W0);
        Solid(R, L - 3.0, Rt + 3.0, R.P0, R.P1, O.W0 - 3.0, O.W0);
        // The opening is open to the eye (no pane is drawn); an invisible glass box keeps the walking player and the camera boom out.
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

    FLaneBlocks& K;
    const FLaneFrame& F;
    ELaneColour Wall;
};

/** Hinge (U, V), the closed leaf's direction and the side it swings to by default, in building coordinates (the home's convention). */
void CareAddDoorSpec(FLaneScene& S, const FLaneFrame& F, const TCHAR* Id, double HingeU, double HingeV, double CloseU, double CloseV,
                     double SwingU, double SwingV, bool bTwoWay)
{
    FLaneDoorSpec Spec;
    Spec.Id = FName(Id);
    Spec.Hinge = F.Pt(HingeU, HingeV, HomeFloorW);
    const FVector Dir = (F.U * CloseU + F.V * CloseV).GetSafeNormal();
    Spec.ClosedYaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
    const FVector LocalY(-Dir.Y, Dir.X, 0.0); // the actor's local +Y is its closed direction turned by +90 degrees of yaw
    Spec.PreferredSwing = FVector::DotProduct(F.U * SwingU + F.V * SwingV, LocalY) >= 0.0 ? 1.0 : -1.0;
    Spec.bTwoWay = bTwoWay;
    S.Doors.Add(Spec);
}

/** The shell: perimeter and partition walls with their openings, floors, ceiling and the solid storeys above. */
void CareBuildWalls(FLaneBlocks& K, const FLaneFrame& F, const FLaneBuilding& B)
{
    using namespace CareLayout;
    const FCareShell Shell(K, F, B.Wall);
    const double W = B.Width;
    const double D = B.Depth;
    const double InnerD = D - 25.0;
    const ELaneColour Warm = ELaneColour::PlasterWarm;
    const ELaneColour Pale = ELaneColour::PlasterPale;
    const ELaneColour Wash = ELaneColour::TileWash;

    // Front wall (the street face): a window for the entry, the front door, two windows over the study tables.
    Shell.BuildRun(CareRun(true, -1, 0.0, 25.0, 0.0, EntryEastU, Warm, Pale, {CareWindow(95.0), CareDoor(FrontDoorU)}));
    Shell.BuildRun(CareRun(true, -1, 0.0, 25.0, EntryEastU, W, Warm, Warm, {CareWindow(600.0), CareWindow(755.0)}));
    // Back wall: two windows for the dormitory, a high ventilator for the washroom.
    Shell.BuildRun(CareRun(true, 1, InnerD, D, 0.0, WashWestU, Warm, Warm, {CareWindow(250.0), CareWindow(620.0)}));
    Shell.BuildRun(CareRun(true, 1, InnerD, D, WashWestU, W, Wash, Wash, {CareWindow(WashDoorU, 30.0, HomeFloorW + 150.0, HomeFloorW + 210.0)}));
    // Side walls (shared with the neighbours): no openings.
    Shell.BuildRun(CareRun(false, -1, 0.0, 25.0, 25.0, AcrossV, Warm, Pale));
    Shell.BuildRun(CareRun(false, -1, 0.0, 25.0, AcrossV, InnerD, Warm, Warm));
    Shell.BuildRun(CareRun(false, 1, W - 25.0, W, 25.0, AcrossV, Warm, Warm));
    Shell.BuildRun(CareRun(false, 1, W - 25.0, W, AcrossV, InnerD, Wash, Wash));

    // Partitions, 12 cm thick, each face painted for its room. The across partition runs under both of the others.
    Shell.BuildRun(CareRun(true, 0, AcrossV - 6.0, AcrossV + 6.0, 25.0, EntryEastU, Pale, Warm, {CareDoor(DormDoorU)}));
    Shell.BuildRun(CareRun(true, 0, AcrossV - 6.0, AcrossV + 6.0, EntryEastU, WashWestU, Warm, Warm));
    Shell.BuildRun(CareRun(true, 0, AcrossV - 6.0, AcrossV + 6.0, WashWestU, W - 25.0, Warm, Wash, {CareDoor(WashDoorU)}));
    // Entry | common room. The doorway is at the back of the partition (V 388-482), 19 cm from the across partition's face.
    Shell.BuildRun(CareRun(false, 0, EntryEastU - 6.0, EntryEastU + 6.0, 25.0, AcrossV - 6.0, Pale, Warm, {CareDoor(CommonDoorV)}));
    Shell.BuildRun(CareRun(false, 0, WashWestU - 6.0, WashWestU + 6.0, AcrossV + 6.0, InnerD, Warm, Wash));

    // Floors: red oxide in the entry, common room and dormitory, pale tile in the washroom (the slabs meet under the partitions).
    F.Add(K, ELaneColour::FloorOxide, 25.0, W - 25.0, 25.0, AcrossV, 0.0, HomeFloorW);
    F.Add(K, ELaneColour::FloorOxide, 25.0, WashWestU, AcrossV, InnerD, 0.0, HomeFloorW);
    F.Add(K, ELaneColour::FloorTile, WashWestU, W - 25.0, AcrossV, InnerD, 0.0, HomeFloorW);
    K.PropCollision(F.Pt(25.0, 25.0, 0.0), F.Pt(W - 25.0, InnerD, HomeFloorW)); // one slab under every room

    // Ceiling: 1 cm of plaster under the solid upper storeys, which also close the top of every wall.
    F.Add(K, ELaneColour::CeilingWhite, 25.0, W - 25.0, 25.0, InnerD, HomeCeilingW, CareCeilingTopW);
    F.Add(K, B.Wall, 0.0, W, 0.0, D, CareCeilingTopW, B.RoofZ());
}

/** The street face: plinth, window sills and sunshades, a name board over the door and the lamp under it. */
void CareBuildStreet(FLaneBlocks& K, const FLaneFrame& F, const FLaneBuilding& B)
{
    using namespace CareLayout;
    const double DoorCut = DoorHalfClear + 5.0;
    F.Add(K, ELaneColour::Plinth, 0.0, FrontDoorU - DoorCut, -2.0, 0.0, HomeFloorW, 75.0);
    F.Add(K, ELaneColour::Plinth, FrontDoorU + DoorCut, B.Width, -2.0, 0.0, HomeFloorW, 75.0);
    for (const double Centre : {95.0, 600.0, 755.0})
    {
        // A sill slab under the trim (80 to 85), and a 45 cm chhajja over it, as on the upper windows.
        F.Add(K, ELaneColour::TrimLight, Centre - 66.0, Centre + 66.0, -9.0, 0.0, CareWindowSill - 10.0, CareWindowSill - 5.0);
        F.Add(K, ELaneColour::TrimLight, Centre - 70.0, Centre + 70.0, -45.0, 0.0, CareWindowHead + 8.0, CareWindowHead + 14.0);
    }
    // The name board over the entrance (W 250-286, flush with the wall) with two painted lines, and the lamp below it.
    F.Add(K, ELaneColour::SignBlue, FrontDoorU - 110.0, FrontDoorU + 110.0, -8.0, 0.0, 250.0, 286.0);
    F.Add(K, ELaneColour::TrimLight, FrontDoorU - 90.0, FrontDoorU + 90.0, -10.0, -8.0, 270.0, 278.0);
    F.Add(K, ELaneColour::TrimLight, FrontDoorU - 60.0, FrontDoorU + 60.0, -10.0, -8.0, 257.0, 263.0);
    F.Add(K, ELaneColour::GlassLit, FrontDoorU - 8.0, FrontDoorU + 8.0, -12.0, -2.0, 236.0, 248.0);
}

/** A ceiling fan: rod, hub and three blades whose underside is 244 cm above the floor (the hub's is 240, the contract's minimum). */
void CareAddFan(FLaneBlocks& K, const FLaneFrame& F, const FHomeFanSpec& Fan)
{
    K.Box(ELaneColour::TrimDark, F.Pt(Fan.U - 2.0, Fan.V - 2.0, 268.0), F.Pt(Fan.U + 2.0, Fan.V + 2.0, HomeCeilingW));
    K.Box(ELaneColour::TrimDark, F.Pt(Fan.U - 9.0, Fan.V - 9.0, 255.0), F.Pt(Fan.U + 9.0, Fan.V + 9.0, 268.0));
    for (int32 Blade = 0; Blade < 3; ++Blade)
    {
        const double Yaw = 20.0 + 120.0 * static_cast<double>(Blade);
        const FVector Dir(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)), 0.0);
        K.Tilted(ELaneColour::WoodLight, F.Pt(Fan.U, Fan.V, 260.0) + Dir * 39.0, FVector(60.0, 12.0, 2.0), FRotator(0.0, Yaw, 0.0));
    }
}

/** A ceiling fitting and its published light (the spec goes into Scene.RoomLights; the integrator spawns the actor). */
void CareAddCeilingLight(FLaneScene& S, const FLaneFrame& F, const FHomeLightSpec& Spec)
{
    S.Blocks.Box(ELaneColour::TrimLight, F.Pt(Spec.U - 11.0, Spec.V - 11.0, 285.0), F.Pt(Spec.U + 11.0, Spec.V + 11.0, HomeCeilingW));
    S.Blocks.Box(ELaneColour::GlassLit, F.Pt(Spec.U - 8.0, Spec.V - 8.0, 281.0), F.Pt(Spec.U + 8.0, Spec.V + 8.0, 285.0));
    FLaneRoomLight Light;
    Light.Position = F.Pt(Spec.U, Spec.V, CareLightW);
    Light.Radius = Spec.AttenuationCm;
    Light.Lumens = Spec.Lumens;
    S.RoomLights.Add(Light);
}

/** The furniture and fixtures from the piece table, the stair's floor patch, the fans and the lights. */
void CareFurnish(FLaneScene& S, const FLaneFrame& F, TArray<FHomeSolid>* Log)
{
    FLaneBlocks& K = S.Blocks;
    for (const FCarePiece& P : CareData::PieceTable)
    {
        F.Add(K, P.Colour, P.U0, P.U1, P.V0, P.V1, HomeFloorW + P.H0, HomeFloorW + P.H1);
        if (!P.bSolid)
        {
            continue;
        }
        K.PropCollision(F.Pt(P.U0, P.V0, HomeFloorW + P.H0), F.Pt(P.U1, P.V1, HomeFloorW + P.H1));
        if (Log != nullptr)
        {
            FHomeSolid Footprint;
            Footprint.U0 = P.U0;
            Footprint.U1 = P.U1;
            Footprint.V0 = P.V0;
            Footprint.V1 = P.V1;
            Footprint.H0 = P.H0;
            Footprint.H1 = P.H1;
            Log->Add(Footprint);
        }
    }
    const FHomeStairSpec Stair = CareStairSpec();
    F.Add(K, ELaneColour::Plinth, Stair.U0, Stair.U1, Stair.V0, Stair.V1, HomeFloorW, HomeFloorW + 0.6); // the stair's floor patch; nothing stands on it
    for (const FHomeFanSpec& Fan : CareFanSpecs())
    {
        CareAddFan(K, F, Fan);
    }
    for (const FHomeLightSpec& Light : CareLightSpecs())
    {
        CareAddCeilingLight(S, F, Light);
    }
}
} // namespace

bool IsCareHomeBuilding(const FLaneBuilding& B)
{
    return B.Side == -1 && B.Index == 1 && B.Detail == ELaneDetail::Full && FMath::IsNearlyEqual(B.Width, CareLayout::Width) &&
           FMath::IsNearlyEqual(B.Depth, CareLayout::Depth) && FMath::IsNearlyEqual(B.Setback, 0.0) && B.Storeys >= 2 &&
           FMath::IsNearlyEqual(B.Frame.Origin.X, -1400.0, 1.0) && FMath::IsNearlyEqual(B.Frame.Origin.Y, -400.0, 1.0);
}

void BuildCareHome(FLaneScene& S, const FLaneBuilding& B, TArray<FHomeSolid>* OutSolids)
{
    using namespace CareLayout;
    const FLaneFrame& F = B.Frame;
    CareBuildWalls(S.Blocks, F, B);
    CareBuildStreet(S.Blocks, F, B);
    CareFurnish(S, F, OutSolids);

    // Doors: hinge (U, V), the closed leaf's direction, and the side it swings to by default (all in building coordinates).
    // The front door swings inward only (the core registers it one-way). The common door is hinged on its back jamb and swings
    // into the entry only, so that, open, its leaf lies against the across partition and never between the care worker and
    // a person coming through; the dormitory and washroom doors swing either way (their zones are kept clear on both sides).
    CareAddDoorSpec(S, F, CareDoorFrontId, FrontDoorU + 45.0, 12.5, -1.0, 0.0, 0.0, 1.0, false);
    CareAddDoorSpec(S, F, CareDoorCommonId, EntryEastU, CommonDoorV + DoorHalfClear, 0.0, -1.0, -1.0, 0.0, false);
    CareAddDoorSpec(S, F, CareDoorDormitoryId, DormDoorU - 45.0, AcrossV, 1.0, 0.0, 0.0, 1.0, true);
    CareAddDoorSpec(S, F, CareDoorWashroomId, WashDoorU + 45.0, AcrossV, -1.0, 0.0, 0.0, 1.0, true);
}

bool HasCareHome(const FLaneScene& S)
{
    const FName Id(CareDoorFrontId);
    for (const FLaneDoorSpec& Door : S.Doors)
    {
        if (Door.Id == Id)
        {
            return true;
        }
    }
    return false;
}

TConstArrayView<FCarePiece> GetCarePieces()
{
    return TConstArrayView<FCarePiece>(CareData::PieceTable, UE_ARRAY_COUNT(CareData::PieceTable));
}

TConstArrayView<FCarePersonalObject> GetCarePersonalObjects()
{
    return TConstArrayView<FCarePersonalObject>(CareData::PersonalTable, UE_ARRAY_COUNT(CareData::PersonalTable));
}

FVector CareWorldPoint(const FLaneFrame& Frame, double U, double V, double HeightCm)
{
    return Frame.Pt(U, V, HomeFloorW + HeightCm);
}

FCareStaffSpot CareStaffSpotLocal()
{
    return CareData::StaffSpot;
}

FVector CareStaffSpot(const FLaneFrame& Frame)
{
    return CareWorldPoint(Frame, CareData::StaffSpot.U, CareData::StaffSpot.V, 0.0);
}

double CareStaffSpotYaw(const FLaneFrame& Frame)
{
    const FCareStaffSpot& Spot = CareData::StaffSpot;
    const FVector Direction = Frame.U * (Spot.FaceU - Spot.U) + Frame.V * (Spot.FaceV - Spot.V);
    return FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
}

TArray<FHomeSeatSpec> CareSeatSpecs()
{
    TArray<FHomeSeatSpec> Seats;
    for (const CareData::FCareSeatRow& Row : CareData::SeatTable)
    {
        FHomeSeatSpec Seat;
        Seat.Id = FName(Row.Id);
        Seat.Verb = Row.Verb;
        Seat.SeatU = Row.SeatU;
        Seat.SeatV = Row.SeatV;
        Seat.SeatHeightCm = Row.SeatHeightCm;
        Seat.FaceU = Row.FaceU;
        Seat.FaceV = Row.FaceV;
        Seat.StandU = Row.StandU;
        Seat.StandV = Row.StandV;
        Seat.StandClearCm = 45.0;
        Seats.Add(Seat);
    }
    return Seats;
}

TArray<FHomeReadableSpec> CareReadableSpecs()
{
    TArray<FHomeReadableSpec> Papers;
    for (const CareData::FCareReadableRow& Row : CareData::ReadableTable)
    {
        FHomeReadableSpec Paper;
        Paper.Id = FName(Row.Id);
        Paper.ContentId = FName(Row.ContentId);
        Paper.Verb = Row.Verb;
        Paper.U = Row.U;
        Paper.V = Row.V;
        Paper.HeightCm = Row.HeightCm;
        Paper.StandU = Row.StandU;
        Paper.StandV = Row.StandV;
        Paper.StandClearCm = 45.0;
        Papers.Add(Paper);
    }
    return Papers;
}

FCareStorageSpec CareStorageSpec()
{
    FCareStorageSpec Trunk;
    Trunk.Id = FName(TEXT("storage.nabapara.care_trunk"));
    Trunk.Verb = TEXT("Open your trunk");
    Trunk.U = 250.0; // the green trunk at the foot of bed 2, with its padlock
    Trunk.V = 660.0;
    Trunk.HeightCm = 50.0;
    Trunk.StandU = 250.0;
    Trunk.StandV = 590.0;
    Trunk.StandClearCm = 45.0;
    return Trunk;
}

FHomeStairSpec CareStairSpec()
{
    FHomeStairSpec Stair; // the home's dog-leg, 180 x 290: lower flight 8 treads from the foot line, a 90 cm landing, upper flight 7 treads
    Stair.Id = FName(TEXT("stair.nabapara.care_upper"));
    Stair.U0 = 320.0;
    Stair.U1 = 500.0;
    Stair.V0 = 25.0;
    Stair.V1 = 315.0;
    Stair.FloorToFloorCm = StoreyHeight;
    Stair.Risers = 17;
    Stair.RiserCm = StoreyHeight / 17.0;
    Stair.TreadCm = 25.0;
    Stair.LaneWidthCm = 90.0;
    Stair.FootV = 315.0;
    Stair.LandingV0 = 25.0;
    Stair.LandingV1 = 115.0;
    Stair.UpperEdgeV = 290.0;
    return Stair;
}

TArray<FHomeFanSpec> CareFanSpecs()
{
    return TArray<FHomeFanSpec>(CareData::FanTable, UE_ARRAY_COUNT(CareData::FanTable));
}

TArray<FHomeLightSpec> CareLightSpecs()
{
    TArray<FHomeLightSpec> Lights;
    for (const CareData::FCareLightRow& Row : CareData::LightTable)
    {
        FHomeLightSpec Spec;
        Spec.Room = FName(Row.Room);
        Spec.U = Row.U;
        Spec.V = Row.V;
        Spec.AttenuationCm = Row.AttenuationCm;
        Spec.Lumens = Row.Lumens;
        Spec.ColourTempK = Row.ColourTempK;
        Spec.RoomU0 = Row.RoomRect->U0;
        Spec.RoomU1 = Row.RoomRect->U1;
        Spec.RoomV0 = Row.RoomRect->V0;
        Spec.RoomV1 = Row.RoomRect->V1;
        Lights.Add(Spec);
    }
    return Lights;
}
} // namespace PWLane
