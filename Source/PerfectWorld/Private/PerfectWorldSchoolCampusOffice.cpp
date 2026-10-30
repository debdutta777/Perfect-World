#include "PerfectWorldSchoolCampusParts.h"

// The school office on the ground floor beside the classroom. Step free from the verandah, a 130 cm doorway, an enquiry counter with a
// 240 cm opening and a low section at wheelchair height, two staff desks, steel almirahs, a waiting bench and a notice board.

namespace PWSchool
{
namespace
{
void ScOfficeCounter(FScBuilt& B, const FScCampus& C, double Y)
{
    const double G = ScGroundZ;
    const double Cx = C.OfficeDoorX;
    constexpr double Depth = 60.0;
    // High section (standing height) on the west, low section (75 cm) on the east; 240 cm open between them in line with the door.
    ScBox(B, TEXT("office.counter_high"), ELaneColour::WoodLight, Cx - 380.0, Y, G + 91.0, Cx - 100.0, Y + Depth, G + 96.0);
    ScBox(B, TEXT("office.counter_high_body"), ELaneColour::ShopInterior, Cx - 376.0, Y + 4.0, G, Cx - 104.0, Y + Depth - 4.0, G + 91.0);
    ScBox(B, TEXT("office.counter_low"), ELaneColour::WoodLight, Cx + 140.0, Y, G + 71.0, Cx + 430.0, Y + Depth, G + 76.0);
    ScBox(B, TEXT("office.counter_low_body"), ELaneColour::ShopInterior, Cx + 144.0, Y + 4.0, G, Cx + 426.0, Y + Depth - 4.0, G + 71.0);
    ScProp(B, TEXT("office.counter_high_collision"), Cx - 380.0, Y, G, Cx - 100.0, Y + Depth, G + 96.0);
    ScProp(B, TEXT("office.counter_low_collision"), Cx + 140.0, Y, G, Cx + 430.0, Y + Depth, G + 76.0);
    // The sign on the low section: a small plate, not text (the office makes no promise about hours).
    ScBox(B, TEXT("office.counter_plate"), ELaneColour::SignGreen, Cx + 250.0, Y - 1.5, G + 40.0, Cx + 330.0, Y, G + 62.0);
}

void ScOfficeDesk(FScBuilt& B, const FString& Id, double Cx, double Y)
{
    const double G = ScGroundZ;
    ScBox(B, *(Id + TEXT(".top")), ELaneColour::WoodLight, Cx - 70.0, Y, G + 71.0, Cx + 70.0, Y + 70.0, G + 76.0);
    ScBox(B, *(Id + TEXT(".drawers")), ELaneColour::ShopInterior, Cx - 66.0, Y + 4.0, G, Cx - 16.0, Y + 66.0, G + 71.0);
    ScBox(B, *(Id + TEXT(".leg")), ELaneColour::TrimDark, Cx + 62.0, Y + 4.0, G, Cx + 68.0, Y + 10.0, G + 71.0);
    ScBox(B, *(Id + TEXT(".leg_b")), ELaneColour::TrimDark, Cx + 62.0, Y + 60.0, G, Cx + 68.0, Y + 66.0, G + 71.0);
    ScBox(B, *(Id + TEXT(".files")), ELaneColour::TrimLight, Cx + 10.0, Y + 12.0, G + 76.0, Cx + 40.0, Y + 34.0, G + 86.0);
    ScBox(B, *(Id + TEXT(".phone")), ELaneColour::TrimDark, Cx - 50.0, Y + 20.0, G + 76.0, Cx - 30.0, Y + 34.0, G + 82.0);
    ScProp(B, *(Id + TEXT(".collision")), Cx - 70.0, Y, G, Cx + 70.0, Y + 70.0, G + 76.0);
    // The chair on the north side (staff face the counter).
    const double ChairY = Y + 95.0;
    ScBox(B, *(Id + TEXT(".chair_seat")), ELaneColour::WoodLight, Cx - 20.0, ChairY - 20.0, G + 42.0, Cx + 20.0, ChairY + 20.0, G + 46.0);
    ScBox(B, *(Id + TEXT(".chair_back")), ELaneColour::WoodLight, Cx - 20.0, ChairY + 17.0, G + 46.0, Cx + 20.0, ChairY + 21.0, G + 90.0);
    ScProp(B, *(Id + TEXT(".chair_collision")), Cx - 21.0, ChairY - 21.0, G, Cx + 21.0, ChairY + 22.0, G + 90.0);
}

void ScBuildOfficeFittings(FScBuilt& B, const FScCampus& C, double X0, double Y0, double X1, double Y1)
{
    const double G = ScGroundZ;
    const double Cz = ScFloorZ(1) - ScSlab;
    ScOfficeCounter(B, C, Y0 + 370.0);
    ScOfficeDesk(B, TEXT("office.desk_a"), C.OfficeDoorX - 290.0, Y0 + 600.0);
    ScOfficeDesk(B, TEXT("office.desk_b"), C.OfficeDoorX + 290.0, Y0 + 600.0);
    // Two steel almirahs along the north wall.
    for (int32 I = 0; I < 2; ++I)
    {
        const double Xa = C.OfficeDoorX - 260.0 + 150.0 * static_cast<double>(I);
        ScBox(B, *FString::Printf(TEXT("office.almirah%d"), I), ELaneColour::ShutterSteel, Xa, Y1 - 50.0, G, Xa + 90.0, Y1 - 1.5, G + 180.0);
        ScBox(B, *FString::Printf(TEXT("office.almirah%d_doors"), I), ELaneColour::ShutterStripe, Xa + 3.0, Y1 - 52.0, G + 6.0, Xa + 87.0, Y1 - 50.0, G + 174.0);
        ScProp(B, *FString::Printf(TEXT("office.almirah%d_collision"), I), Xa - 1.0, Y1 - 52.0, G, Xa + 91.0, Y1 - 1.5, G + 180.0);
    }
    // A waiting bench along the west wall, near the door, and a notice board above it.
    ScBox(B, TEXT("office.bench_seat"), ELaneColour::WoodLight, X0 + 2.0, Y0 + 160.0, G + 40.0, X0 + 44.0, Y0 + 330.0, G + 45.0);
    ScBox(B, TEXT("office.bench_back"), ELaneColour::WoodLight, X0 + 2.0, Y0 + 160.0, G + 45.0, X0 + 6.0, Y0 + 330.0, G + 90.0);
    ScBox(B, TEXT("office.bench_leg_a"), ELaneColour::TrimDark, X0 + 20.0, Y0 + 168.0, G, X0 + 26.0, Y0 + 174.0, G + 40.0);
    ScBox(B, TEXT("office.bench_leg_b"), ELaneColour::TrimDark, X0 + 20.0, Y0 + 316.0, G, X0 + 26.0, Y0 + 322.0, G + 40.0);
    ScProp(B, TEXT("office.bench_collision"), X0 + 1.0, Y0 + 159.0, G, X0 + 45.0, Y0 + 331.0, G + 90.0);
    ScBox(B, TEXT("office.notice_board"), ELaneColour::WoodLight, X0 + 1.5, Y0 + 130.0, G + 110.0, X0 + 4.0, Y0 + 360.0, G + 200.0);
    ScBox(B, TEXT("office.notice_a"), ELaneColour::TrimLight, X0 + 4.0, Y0 + 150.0, G + 130.0, X0 + 5.5, Y0 + 220.0, G + 190.0);
    ScBox(B, TEXT("office.notice_b"), ELaneColour::GoodsYellow, X0 + 4.0, Y0 + 245.0, G + 140.0, X0 + 5.5, Y0 + 335.0, G + 185.0);
    // Windows from inside: bright panels behind the staff side of the counter.
    for (const double Dx : {-260.0, 260.0})
    {
        ScBox(B, *FString::Printf(TEXT("office.window_south%d"), Dx < 0 ? 0 : 1), ELaneColour::GlassLit, C.OfficeDoorX + Dx - 100.0, Y0 + 1.5, G + 100.0, C.OfficeDoorX + Dx + 100.0, Y0 + 3.0, G + 230.0);
    }
    ScFan(B, TEXT("office.fan0"), C.OfficeDoorX - 250.0, Y0 + 300.0);
    ScFan(B, TEXT("office.fan1"), C.OfficeDoorX + 250.0, Y0 + 300.0);
    PWLane::FLaneRoomLight Light;
    Light.Position = FVector(C.OfficeDoorX, Y0 + 300.0, Cz - 30.0);
    Light.Radius = 650.0f;
    Light.Lumens = 900.0f;
    B.IndoorLights.Add(Light);
}
} // namespace

void ScBuildOffice(FScBuilt& B, const FScCampus& C)
{
    const double G = ScGroundZ;
    const double F1 = ScFloorZ(1);
    // Interior X 6030..7000, Y 6530..7500: the stair tower's wall on the west (6000..6030), the party wall to the classroom on the east.
    const double X0 = ScRoom::OfficeX0 + ScWall;
    const double X1 = ScRoom::PartyX;
    const double Y0 = C.Teaching.Y0 + ScWall;
    const double Y1 = ScRoom::OfficeNorth;
    ScBox(B, TEXT("office.west_wall"), ELaneColour::WallButter, ScRoom::OfficeX0, Y0, G, X0, Y1 + ScWall, F1);
    ScBox(B, TEXT("office.north_wall"), ELaneColour::WallButter, X0, Y1, G, X1, Y1 + ScWall, F1);
    ScRoomFloor(B, TEXT("office"), C, X0, Y0, X1, Y1, C.OfficeDoorX);
    ScLineRoom(B, TEXT("office.lining"), X0, Y0, X1, Y1, C.OfficeDoorX - 0.5 * ScDoorWidth, C.OfficeDoorX + 0.5 * ScDoorWidth, ELaneColour::PlasterWarm);
    ScBuildOfficeFittings(B, C, X0, Y0, X1, Y1);

    B.Points.OfficeDoor = FVector(C.OfficeDoorX, C.Teaching.Y0, G);
    B.Points.OfficeInside = FVector(C.OfficeDoorX, Y0 + 220.0, G);
}
} // namespace PWSchool
