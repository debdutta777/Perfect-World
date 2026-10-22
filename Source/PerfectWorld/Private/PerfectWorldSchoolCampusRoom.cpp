#include "PerfectWorldSchoolCampusParts.h"

// The enterable classroom (Class XI Science): double desks with benches for forty, the teacher's table, a blackboard, ceiling fans,
// windows, posters and a cupboard. Also the lining, floor and fan helpers the office shares.

namespace PWSchool
{
namespace
{
constexpr double ScLining = 1.5;
constexpr double ScDado = 100.0;
constexpr double ScDeskWidth = 100.0;
constexpr double ScDeskDepth = 45.0;
constexpr double ScDeskHeight = 76.0;
constexpr double ScBenchDepth = 28.0;
constexpr double ScBenchHeight = 44.0;
constexpr int32 ScDeskRows = 5;
constexpr double ScRowPitch = 150.0;
constexpr double ScCentreAisle = 120.0;

void ScDeskUnit(FScBuilt& B, int32 Row, int32 Column, double X0, double YDesk)
{
    const double G = ScGroundZ;
    const double X1 = X0 + ScDeskWidth;
    const FString Id = FString::Printf(TEXT("class.desk.r%d_c%d"), Row, Column);
    ScBox(B, *(Id + TEXT(".top")), ELaneColour::WoodLight, X0, YDesk, G + ScDeskHeight - 4.0, X1, YDesk + ScDeskDepth, G + ScDeskHeight);
    ScBox(B, *(Id + TEXT(".side_a")), ELaneColour::WoodLight, X0, YDesk, G, X0 + 4.0, YDesk + ScDeskDepth, G + ScDeskHeight - 4.0);
    ScBox(B, *(Id + TEXT(".side_b")), ELaneColour::WoodLight, X1 - 4.0, YDesk, G, X1, YDesk + ScDeskDepth, G + ScDeskHeight - 4.0);
    ScBox(B, *(Id + TEXT(".shelf")), ELaneColour::WoodLight, X0 + 4.0, YDesk + 4.0, G + 24.0, X1 - 4.0, YDesk + ScDeskDepth - 4.0, G + 27.0);
    // The bench sits just south of its desk (students face the blackboard to the north).
    const double BenchY1 = YDesk - 10.0;
    const double BenchY0 = BenchY1 - ScBenchDepth;
    ScBox(B, *(Id + TEXT(".bench")), ELaneColour::WoodLight, X0 + 2.0, BenchY0, G + ScBenchHeight - 4.0, X1 - 2.0, BenchY1, G + ScBenchHeight);
    ScBox(B, *(Id + TEXT(".bench_leg_a")), ELaneColour::TrimDark, X0 + 8.0, BenchY0 + 2.0, G, X0 + 13.0, BenchY1 - 2.0, G + ScBenchHeight - 4.0);
    ScBox(B, *(Id + TEXT(".bench_leg_b")), ELaneColour::TrimDark, X1 - 13.0, BenchY0 + 2.0, G, X1 - 8.0, BenchY1 - 2.0, G + ScBenchHeight - 4.0);
    ScProp(B, *(Id + TEXT(".desk_collision")), X0, YDesk, G, X1, YDesk + ScDeskDepth, G + ScDeskHeight);
    ScProp(B, *(Id + TEXT(".bench_collision")), X0 + 2.0, BenchY0, G, X1 - 2.0, BenchY1, G + ScBenchHeight);
}

void ScBuildDesks(FScBuilt& B, const FScCampus& C, double Y0)
{
    // Four columns round a 120 cm centre aisle on the doorway's line: two double desks each side, a 40 cm gap between a pair.
    const double Offsets[] = {-300.0, -160.0, ScCentreAisle * 0.5, ScCentreAisle * 0.5 + 140.0};
    for (int32 Row = 0; Row < ScDeskRows; ++Row)
    {
        const double YDesk = Y0 + 70.0 + ScRowPitch * static_cast<double>(Row);
        for (int32 Column = 0; Column < 4; ++Column)
        {
            ScDeskUnit(B, Row, Column, C.ClassroomDoorX + Offsets[Column], YDesk);
            ++B.Classroom.DeskUnits;
            B.Classroom.Seats += 2;
        }
    }
}

void ScBuildTeacherTable(FScBuilt& B, const FScCampus& C, double Y1)
{
    const double G = ScGroundZ;
    const double Cx = C.ClassroomDoorX;
    const double Ya = Y1 - 190.0; // 7380 for the classroom's 7570 north wall
    const double Yb = Ya + 70.0;
    ScBox(B, TEXT("class.table.top"), ELaneColour::WoodLight, Cx - 140.0, Ya, G + 72.0, Cx + 140.0, Yb, G + 78.0);
    ScBox(B, TEXT("class.table.front"), ELaneColour::WoodLight, Cx - 138.0, Ya, G + 24.0, Cx + 138.0, Ya + 3.0, G + 72.0);
    for (const double Dx : {-134.0, 128.0})
    {
        for (const double Y : {Ya + 3.0, Yb - 9.0})
        {
            ScBox(B, *FString::Printf(TEXT("class.table.leg_%d_%d"), Dx < 0 ? 0 : 1, Y < Ya + 10.0 ? 0 : 1), ELaneColour::TrimDark, Cx + Dx, Y, G, Cx + Dx + 6.0, Y + 6.0, G + 72.0);
        }
    }
    ScProp(B, TEXT("class.table.collision"), Cx - 140.0, Ya, G, Cx + 140.0, Yb, G + 78.0);
    // On the table: a register, a stack of exercise books, a handbell and a chalk box.
    ScBox(B, TEXT("class.table.register"), ELaneColour::SignBlue, Cx - 110.0, Ya + 12.0, G + 78.0, Cx - 70.0, Ya + 42.0, G + 82.0);
    ScBox(B, TEXT("class.table.books"), ELaneColour::GoodsRed, Cx + 30.0, Ya + 15.0, G + 78.0, Cx + 62.0, Ya + 38.0, G + 90.0);
    ScBox(B, TEXT("class.table.books_b"), ELaneColour::GoodsGreen, Cx + 32.0, Ya + 17.0, G + 90.0, Cx + 60.0, Ya + 36.0, G + 96.0);
    ScColumn(B, TEXT("class.table.bell"), ELaneColour::Chrome, Cx + 100.0, Ya + 35.0, 10.0, 10.0, G + 78.0, G + 88.0);
    ScBox(B, TEXT("class.table.chalk"), ELaneColour::TrimLight, Cx + 70.0, Ya + 8.0, G + 78.0, Cx + 88.0, Ya + 18.0, G + 83.0);
    // The teacher's chair behind it.
    const double ChairY = Yb + 22.0;
    ScBox(B, TEXT("class.chair.seat"), ELaneColour::WoodLight, Cx - 22.0, ChairY - 22.0, G + 42.0, Cx + 22.0, ChairY + 22.0, G + 46.0);
    ScBox(B, TEXT("class.chair.back"), ELaneColour::WoodLight, Cx - 22.0, ChairY + 19.0, G + 46.0, Cx + 22.0, ChairY + 23.0, G + 92.0);
    for (const double Dx : {-20.0, 16.0})
    {
        for (const double Dy : {-20.0, 16.0})
        {
            ScBox(B, *FString::Printf(TEXT("class.chair.leg_%d_%d"), Dx < 0 ? 0 : 1, Dy < 0 ? 0 : 1), ELaneColour::TrimDark, Cx + Dx, ChairY + Dy, G, Cx + Dx + 4.0, ChairY + Dy + 4.0, G + 42.0);
        }
    }
    ScProp(B, TEXT("class.chair.collision"), Cx - 23.0, ChairY - 23.0, G, Cx + 23.0, ChairY + 24.0, G + 92.0);
}

void ScBuildBoard(FScBuilt& B, const FScCampus& C, double Y1)
{
    const double G = ScGroundZ;
    const double Cx = C.ClassroomDoorX;
    const double Y = Y1 - ScLining;
    ScBox(B, TEXT("class.blackboard"), ELaneColour::TankBlack, Cx - 350.0, Y - 2.0, G + 92.0, Cx + 350.0, Y, G + 248.0);
    ScBox(B, TEXT("class.blackboard_frame_top"), ELaneColour::WoodLight, Cx - 356.0, Y - 4.0, G + 248.0, Cx + 356.0, Y, G + 256.0);
    ScBox(B, TEXT("class.blackboard_frame_bottom"), ELaneColour::WoodLight, Cx - 356.0, Y - 4.0, G + 84.0, Cx + 356.0, Y, G + 92.0);
    ScBox(B, TEXT("class.blackboard_frame_west"), ELaneColour::WoodLight, Cx - 356.0, Y - 4.0, G + 92.0, Cx - 350.0, Y, G + 248.0);
    ScBox(B, TEXT("class.blackboard_frame_east"), ELaneColour::WoodLight, Cx + 350.0, Y - 4.0, G + 92.0, Cx + 356.0, Y, G + 248.0);
    ScBox(B, TEXT("class.chalk_tray"), ELaneColour::WoodLight, Cx - 350.0, Y - 9.0, G + 86.0, Cx + 350.0, Y - 2.0, G + 92.0);
    ScText(B, TEXT("class.blackboard_text"), TEXT("Class XI - Science"), FVector(Cx, Y - 2.5, G + 205.0), 15.0f);
    // A wall clock over the board.
    ScBox(B, TEXT("class.clock"), ELaneColour::TrimLight, Cx - 15.0, Y - 4.0, G + 272.0, Cx + 15.0, Y, G + 302.0);
    ScBox(B, TEXT("class.clock_hand_a"), ELaneColour::TrimDark, Cx - 1.0, Y - 5.5, G + 287.0, Cx + 1.0, Y - 4.0, G + 299.0);
    ScBox(B, TEXT("class.clock_hand_b"), ELaneColour::TrimDark, Cx, Y - 5.5, G + 286.0, Cx + 10.0, Y - 4.0, G + 288.0);
}

void ScBuildClassFittings(FScBuilt& B, const FScCampus& C, double X0, double Y0, double X1, double Y1)
{
    const double G = ScGroundZ;
    // Windows from inside: bright panels where the south and east windows are outside.
    const double Cz = ScFloorZ(1) - ScSlab;
    for (const double Dx : {-240.0, 240.0})
    {
        ScBox(B, *FString::Printf(TEXT("class.window_south%d"), Dx < 0 ? 0 : 1), ELaneColour::GlassLit, C.ClassroomDoorX + Dx - 100.0, Y0 + ScLining, G + 100.0, C.ClassroomDoorX + Dx + 100.0, Y0 + ScLining + 1.5, G + 230.0);
        ScBox(B, *FString::Printf(TEXT("class.window_south_sill%d"), Dx < 0 ? 0 : 1), ELaneColour::TrimLight, C.ClassroomDoorX + Dx - 108.0, Y0 + ScLining, G + 94.0, C.ClassroomDoorX + Dx + 108.0, Y0 + ScLining + 8.0, G + 100.0);
    }
    ScBox(B, TEXT("class.window_east"), ELaneColour::GlassLit, X1 - ScLining - 1.5, 7130.0, G + 100.0, X1 - ScLining, 7270.0, G + 230.0);
    // Posters on the party wall.
    ScBox(B, TEXT("class.poster_a"), ELaneColour::SignRed, X0 + ScLining, Y0 + 280.0, G + 120.0, X0 + ScLining + 1.5, Y0 + 400.0, G + 200.0);
    ScBox(B, TEXT("class.poster_b"), ELaneColour::SignGreen, X0 + ScLining, Y0 + 460.0, G + 120.0, X0 + ScLining + 1.5, Y0 + 560.0, G + 200.0);
    ScBox(B, TEXT("class.poster_c"), ELaneColour::GoodsYellow, X0 + ScLining, Y0 + 620.0, G + 130.0, X0 + ScLining + 1.5, Y0 + 720.0, G + 200.0);
    // A cupboard in the north-east corner.
    ScBox(B, TEXT("class.cupboard"), ELaneColour::ShutterSteel, X1 - 70.0, Y1 - 150.0, G, X1 - ScLining, Y1 - 15.0, G + 180.0);
    ScBox(B, TEXT("class.cupboard_door"), ELaneColour::ShutterStripe, X1 - 72.0, Y1 - 148.0, G + 5.0, X1 - 70.0, Y1 - 17.0, G + 175.0);
    ScProp(B, TEXT("class.cupboard_collision"), X1 - 72.0, Y1 - 152.0, G, X1 - ScLining, Y1 - 13.0, G + 180.0);
    // Tube lights and fans on the ceiling, with three warm lights that stay inside the room.
    for (const double Dx : {-250.0, 250.0})
    {
        ScBox(B, *FString::Printf(TEXT("class.tube%d"), Dx < 0 ? 0 : 1), ELaneColour::GlassLit, C.ClassroomDoorX + Dx - 60.0, 6960.0 - 4.0, Cz - 4.0, C.ClassroomDoorX + Dx + 60.0, 6960.0 + 4.0, Cz - 1.5);
    }
    int32 Fan = 0;
    for (const double Dy : {Y0 + 330.0, Y0 + 730.0})
    {
        for (const double Dx : {-250.0, 250.0})
        {
            ScFan(B, FString::Printf(TEXT("class.fan%d"), Fan++), C.ClassroomDoorX + Dx, Dy);
        }
    }
    B.Classroom.Fans = Fan;
    const double LightZ = Cz - 30.0;
    for (const FVector2D& At : {FVector2D(C.ClassroomDoorX - 250.0, Y0 + 330.0), FVector2D(C.ClassroomDoorX + 250.0, Y0 + 330.0), FVector2D(C.ClassroomDoorX, Y0 + 750.0)})
    {
        PWLane::FLaneRoomLight Light;
        Light.Position = FVector(At.X, At.Y, LightZ);
        Light.Radius = 700.0f;
        Light.Lumens = 900.0f;
        B.IndoorLights.Add(Light);
    }
}
} // namespace

void ScLineRoom(FScBuilt& B, const FString& Id, double X0, double Y0, double X1, double Y1, double DoorX0, double DoorX1, ELaneColour Plaster)
{
    const double G = ScGroundZ;
    const double Cz = ScFloorZ(1) - ScSlab; // under the ceiling slab
    const double T = ScLining;
    // The floor's tile skin and the ceiling's white skin.
    ScBox(B, *(Id + TEXT(".floor_skin")), ELaneColour::FloorTile, X0, Y0, G, X1, Y1, G + 1.0);
    ScBox(B, *(Id + TEXT(".ceiling_skin")), ELaneColour::CeilingWhite, X0, Y0, Cz - T, X1, Y1, Cz);
    // The four walls: a green dado and plaster above. The south wall is cut at the doorway, with a head above it.
    for (int32 Band = 0; Band < 2; ++Band)
    {
        const double Z0 = Band == 0 ? G : G + ScDado;
        const double Z1 = Band == 0 ? G + ScDado : Cz;
        const ELaneColour Colour = Band == 0 ? ELaneColour::ShutterGreen : Plaster;
        const FString B_ = Id + (Band == 0 ? TEXT(".dado") : TEXT(".plaster"));
        ScBox(B, *(B_ + TEXT("_south_a")), Colour, X0, Y0, Z0, DoorX0, Y0 + T, Z1);
        ScBox(B, *(B_ + TEXT("_south_b")), Colour, DoorX1, Y0, Z0, X1, Y0 + T, Z1);
        ScBox(B, *(B_ + TEXT("_north")), Colour, X0, Y1 - T, Z0, X1, Y1, Z1);
        ScBox(B, *(B_ + TEXT("_west")), Colour, X0, Y0 + T, Z0, X0 + T, Y1 - T, Z1);
        ScBox(B, *(B_ + TEXT("_east")), Colour, X1 - T, Y0 + T, Z0, X1, Y1 - T, Z1);
    }
    ScBox(B, *(Id + TEXT(".plaster_door_head")), Plaster, DoorX0, Y0, G + ScDoorHeight, DoorX1, Y0 + T, Cz);
    // The doorway's leaf, folded back flat against the inside of the south wall (it never closes; see the header).
    ScBox(B, *(Id + TEXT(".door_leaf")), ELaneColour::DoorWood, DoorX1, Y0 + T, G, DoorX1 + ScDoorWidth, Y0 + T + 4.0, G + ScDoorHeight - 5.0);
    ScBox(B, *(Id + TEXT(".door_hinge_a")), ELaneColour::Chrome, DoorX1 - 1.0, Y0 + T, G + 25.0, DoorX1 + 3.0, Y0 + T + 5.0, G + 40.0);
    ScBox(B, *(Id + TEXT(".door_hinge_b")), ELaneColour::Chrome, DoorX1 - 1.0, Y0 + T, G + 175.0, DoorX1 + 3.0, Y0 + T + 5.0, G + 190.0);
}

void ScFan(FScBuilt& B, const FString& Id, double X, double Y)
{
    const double Cz = ScFloorZ(1) - ScSlab;
    ScColumn(B, *(Id + TEXT(".rod")), ELaneColour::TrimDark, X, Y, 6.0, 6.0, Cz - 38.0, Cz);
    ScColumn(B, *(Id + TEXT(".hub")), ELaneColour::TrimDark, X, Y, 18.0, 18.0, Cz - 46.0, Cz - 38.0);
    for (int32 I = 0; I < 3; ++I)
    {
        const double Angle = 120.0 * static_cast<double>(I) + 20.0;
        const double Rad = FMath::DegreesToRadians(Angle);
        B.Blocks.Tilted(ELaneColour::AcUnit, FVector(X + 40.0 * FMath::Cos(Rad), Y + 40.0 * FMath::Sin(Rad), Cz - 41.0), FVector(70.0, 13.0, 1.5), FRotator(0.0, Angle, 0.0));
    }
}

void ScRoomFloor(FScBuilt& B, const FString& Id, const FScCampus& C, double X0, double Y0, double X1, double Y1, double DoorX)
{
    const double G = ScGroundZ;
    ScBox(B, *(Id + TEXT(".floor")), ELaneColour::Footpath, X0, Y0, G - 15.0, X1, Y1, G);
    // The sill across the wall's own depth, so the doorway is level and nothing dips between verandah and room.
    ScBox(B, *(Id + TEXT(".threshold")), ELaneColour::Footpath, DoorX - 0.5 * ScDoorWidth, C.Teaching.Y0, G - 15.0, DoorX + 0.5 * ScDoorWidth, Y0, G);
}

void ScBuildClassroom(FScBuilt& B, const FScCampus& C)
{
    const double G = ScGroundZ;
    const double F1 = ScFloorZ(1);
    // Interior: X 7030..7970 (the party wall on the west, the block's east wall on the east), Y 6530..7570.
    const double X0 = ScRoom::PartyX + ScWall;
    const double X1 = C.Teaching.X1 - ScWall;
    const double Y0 = C.Teaching.Y0 + ScWall;
    const double Y1 = ScRoom::ClassNorth;
    // Walls: the party wall (shared with the office) and the north wall. The south wall and east wall belong to the block.
    ScBox(B, TEXT("class.party_wall"), ELaneColour::WallButter, ScRoom::PartyX, Y0, G, ScRoom::PartyX + ScWall, Y1 + ScWall, F1);
    ScBox(B, TEXT("class.north_wall"), ELaneColour::WallButter, X0, Y1, G, X1, Y1 + ScWall, F1);
    ScRoomFloor(B, TEXT("class"), C, X0, Y0, X1, Y1, C.ClassroomDoorX);
    ScLineRoom(B, TEXT("class.lining"), X0, Y0, X1, Y1, C.ClassroomDoorX - 0.5 * ScDoorWidth, C.ClassroomDoorX + 0.5 * ScDoorWidth, ELaneColour::PlasterPale);
    ScBuildDesks(B, C, Y0);
    ScBuildTeacherTable(B, C, Y1);
    ScBuildBoard(B, C, Y1);
    ScBuildClassFittings(B, C, X0, Y0, X1, Y1);

    B.Points.ClassroomDoor = FVector(C.ClassroomDoorX, C.Teaching.Y0, G);
    B.Points.ClassroomAisle = FVector(C.ClassroomDoorX, Y0 + 70.0 + ScRowPitch * 2.0 - 30.0, G);
    B.Points.ClassroomFront = FVector(C.ClassroomDoorX, Y1 - 270.0, G);
}
} // namespace PWSchool
