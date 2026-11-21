#include "PerfectWorldSchoolCampusParts.h"

// The boundary wall, the gate (pillars, arched signboard in English and block-letter Bengali, folded iron leaves), the guard booth, the
// attendance register stand and the timetable board mounted on the wall beside the gate.

namespace PWSchool
{
namespace
{
constexpr double ScPillarWidth = 100.0;
constexpr double ScPillarHeight = 330.0;
constexpr double ScLintelBottom = 285.0; // clear height under the gate beam: well over the 165 cm player and a jump

void ScBuildBoundary(FScBuilt& B, const FScCampus& C)
{
    const double G = ScGroundZ;
    const FScRect& S = C.Site;
    const double Top = G + ScBoundaryWallHeight;
    const double GateWest = C.GateX - 0.5 * C.GateWidth - ScPillarWidth; // outer face of the west pillar
    const double GateEast = C.GateX + 0.5 * C.GateWidth + ScPillarWidth;
    // The south wall is cut at the gate: the pillars stand where it would run through.
    ScBox(B, TEXT("wall.south_west"), ELaneColour::WallCream, S.X0, S.Y0, G, GateWest, S.Y0 + ScWall, Top);
    ScBox(B, TEXT("wall.south_east"), ELaneColour::WallCream, GateEast, S.Y0, G, S.X1, S.Y0 + ScWall, Top);
    ScBox(B, TEXT("wall.west"), ELaneColour::WallCream, S.X0, S.Y0 + ScWall, G, S.X0 + ScWall, S.Y1, Top);
    ScBox(B, TEXT("wall.east"), ELaneColour::WallCream, S.X1 - ScWall, S.Y0 + ScWall, G, S.X1, S.Y1, Top);
    ScBox(B, TEXT("wall.north"), ELaneColour::WallCream, S.X0 + ScWall, S.Y1 - ScWall, G, S.X1 - ScWall, S.Y1, Top);
    // A pale cap on every run, a plinth band at the foot and piers inside every 700 cm (all inside the line, so the road side stays flat).
    ScBox(B, TEXT("wall.cap_south_west"), ELaneColour::TrimLight, S.X0, S.Y0 - 3.0, Top, GateWest, S.Y0 + ScWall + 3.0, Top + 6.0);
    ScBox(B, TEXT("wall.cap_south_east"), ELaneColour::TrimLight, GateEast, S.Y0 - 3.0, Top, S.X1, S.Y0 + ScWall + 3.0, Top + 6.0);
    ScBox(B, TEXT("wall.cap_west"), ELaneColour::TrimLight, S.X0 - 3.0, S.Y0, Top, S.X0 + ScWall + 3.0, S.Y1, Top + 6.0);
    ScBox(B, TEXT("wall.cap_east"), ELaneColour::TrimLight, S.X1 - ScWall - 3.0, S.Y0, Top, S.X1 + 3.0, S.Y1, Top + 6.0);
    ScBox(B, TEXT("wall.cap_north"), ELaneColour::TrimLight, S.X0, S.Y1 - ScWall - 3.0, Top, S.X1, S.Y1 + 3.0, Top + 6.0);
    ScBox(B, TEXT("wall.plinth_south_west"), ELaneColour::Plinth, S.X0, S.Y0 - 2.0, G, GateWest, S.Y0, G + 40.0);
    ScBox(B, TEXT("wall.plinth_south_east"), ELaneColour::Plinth, GateEast, S.Y0 - 2.0, G, S.X1, S.Y0, G + 40.0);
    int32 Pier = 0;
    for (double X = S.X0 + 700.0; X < S.X1 - 300.0; X += 700.0)
    {
        if (X > GateWest - 150.0 && X < GateEast + 150.0)
        {
            continue;
        }
        ScBox(B, *FString::Printf(TEXT("wall.pier_south%d"), Pier++), ELaneColour::WallCream, X - 20.0, S.Y0 + ScWall, G, X + 20.0, S.Y0 + ScWall + 12.0, Top + 25.0);
    }
    int32 Side = 0;
    for (double Y = S.Y0 + 1400.0; Y < S.Y1 - 300.0; Y += 1400.0)
    {
        ScBox(B, *FString::Printf(TEXT("wall.pier_west%d"), Side), ELaneColour::WallCream, S.X0 + ScWall, Y - 20.0, G, S.X0 + ScWall + 12.0, Y + 20.0, Top + 25.0);
        ScBox(B, *FString::Printf(TEXT("wall.pier_east%d"), Side), ELaneColour::WallCream, S.X1 - ScWall - 12.0, Y - 20.0, G, S.X1 - ScWall, Y + 20.0, Top + 25.0);
        ++Side;
    }
}

/** A folded iron leaf lying along the inside of the south wall (decoration: it opens outside the 400 cm clear opening). */
void ScBuildLeaf(FScBuilt& B, const TCHAR* Id, double X0, double X1, double Y)
{
    const double G = ScGroundZ;
    const FString Name(Id);
    ScBox(B, *(Name + TEXT(".top")), ELaneColour::Rail, X0, Y, G + 195.0, X1, Y + 4.0, G + 205.0);
    ScBox(B, *(Name + TEXT(".bottom")), ELaneColour::Rail, X0, Y, G + 20.0, X1, Y + 4.0, G + 30.0);
    ScBox(B, *(Name + TEXT(".diagonal_a")), ELaneColour::Rail, X0, Y, G + 100.0, X1, Y + 2.0, G + 106.0);
    for (int32 I = 0; I < 7; ++I)
    {
        const double X = X0 + 8.0 + (X1 - X0 - 16.0) * static_cast<double>(I) / 6.0;
        ScBox(B, *FString::Printf(TEXT("%s.bar%d"), Id, I), ELaneColour::Rail, X - 2.0, Y, G + 30.0, X + 2.0, Y + 3.0, G + 195.0);
    }
}

void ScBuildGatework(FScBuilt& B, const FScCampus& C)
{
    const double G = ScGroundZ;
    const double Y0 = C.Site.Y0;
    const double OpenW = 0.5 * C.GateWidth;
    const double WestX0 = C.GateX - OpenW - ScPillarWidth;
    const double WestX1 = C.GateX - OpenW;
    const double EastX0 = C.GateX + OpenW;
    const double EastX1 = C.GateX + OpenW + ScPillarWidth;
    // Pillars: a little proud of the wall on both faces, capped and lit.
    ScBox(B, TEXT("gate.pillar_west"), ELaneColour::WallCream, WestX0, Y0 - 10.0, G, WestX1, Y0 + 50.0, G + ScPillarHeight);
    ScBox(B, TEXT("gate.pillar_east"), ELaneColour::WallCream, EastX0, Y0 - 10.0, G, EastX1, Y0 + 50.0, G + ScPillarHeight);
    ScBox(B, TEXT("gate.pillar_west_cap"), ELaneColour::TrimLight, WestX0 - 8.0, Y0 - 18.0, G + ScPillarHeight, WestX1 + 8.0, Y0 + 58.0, G + ScPillarHeight + 14.0);
    ScBox(B, TEXT("gate.pillar_east_cap"), ELaneColour::TrimLight, EastX0 - 8.0, Y0 - 18.0, G + ScPillarHeight, EastX1 + 8.0, Y0 + 58.0, G + ScPillarHeight + 14.0);
    ScColumn(B, TEXT("gate.lamp_west"), ELaneColour::GoodsYellow, 0.5 * (WestX0 + WestX1), Y0 + 20.0, 36.0, 36.0, G + ScPillarHeight + 14.0, G + ScPillarHeight + 50.0);
    ScColumn(B, TEXT("gate.lamp_east"), ELaneColour::GoodsYellow, 0.5 * (EastX0 + EastX1), Y0 + 20.0, 36.0, 36.0, G + ScPillarHeight + 14.0, G + ScPillarHeight + 50.0);
    ScBox(B, TEXT("gate.base_west"), ELaneColour::Plinth, WestX0 - 4.0, Y0 - 14.0, G, WestX1 + 4.0, Y0 + 54.0, G + 45.0);
    ScBox(B, TEXT("gate.base_east"), ELaneColour::Plinth, EastX0 - 4.0, Y0 - 14.0, G, EastX1 + 4.0, Y0 + 54.0, G + 45.0);

    // The beam over the opening: solid, with 285 cm clear under it.
    ScBox(B, TEXT("gate.lintel"), ELaneColour::WallCream, WestX0, Y0 - 10.0, G + ScLintelBottom, EastX1, Y0 + 50.0, G + ScLintelBottom + 35.0);

    // The signboard: a deep-blue plate with a pale frame, a stepped arch crowning it, English above and block letters below.
    const double PlateFront = Y0 - 18.0;
    const double BoardBottom = G + ScLintelBottom + 37.0;
    const double BoardTop = G + ScLintelBottom + 137.0;
    ScBox(B, TEXT("gate.board"), ELaneColour::SignBlue, WestX0 + 6.0, PlateFront, BoardBottom, EastX1 - 6.0, Y0 - 10.0, BoardTop);
    ScBox(B, TEXT("gate.board_frame_top"), ELaneColour::TrimLight, WestX0, PlateFront - 2.0, BoardTop - 2.0, EastX1, Y0 - 8.0, BoardTop + 6.0);
    ScBox(B, TEXT("gate.board_frame_bottom"), ELaneColour::TrimLight, WestX0, PlateFront - 2.0, BoardBottom - 6.0, EastX1, Y0 - 8.0, BoardBottom + 2.0);
    ScBox(B, TEXT("gate.board_frame_west"), ELaneColour::TrimLight, WestX0, PlateFront - 2.0, BoardBottom, WestX0 + 8.0, Y0 - 8.0, BoardTop);
    ScBox(B, TEXT("gate.board_frame_east"), ELaneColour::TrimLight, EastX1 - 8.0, PlateFront - 2.0, BoardBottom, EastX1, Y0 - 8.0, BoardTop);
    const double CentreX = C.GateX;
    ScBox(B, TEXT("gate.arch_1"), ELaneColour::TrimLight, CentreX - 190.0, PlateFront - 2.0, BoardTop + 6.0, CentreX + 190.0, Y0 - 8.0, BoardTop + 26.0);
    ScBox(B, TEXT("gate.arch_2"), ELaneColour::TrimLight, CentreX - 130.0, PlateFront - 2.0, BoardTop + 26.0, CentreX + 130.0, Y0 - 8.0, BoardTop + 44.0);
    ScBox(B, TEXT("gate.arch_3"), ELaneColour::TrimLight, CentreX - 70.0, PlateFront - 2.0, BoardTop + 44.0, CentreX + 70.0, Y0 - 8.0, BoardTop + 58.0);
    ScBox(B, TEXT("gate.arch_crest"), ELaneColour::GoodsOrange, CentreX - 14.0, PlateFront - 3.0, BoardTop + 58.0, CentreX + 14.0, Y0 - 8.0, BoardTop + 80.0);
    // "Nabapara Vidyalaya" in English (a text actor), then the Bengali line in blocks beneath it.
    ScText(B, TEXT("gate.sign_english"), TEXT("Nabapara Vidyalaya"), FVector(CentreX, PlateFront - 0.5, BoardBottom + 70.0), 21.0f);
    ScBuildBengaliLine(B, CentreX, PlateFront, BoardBottom + 14.0, 3.8);

    // The folded iron leaves rest against the inside of the wall, either side.
    ScBuildLeaf(B, TEXT("gate.leaf_west"), WestX0 - 210.0, WestX0 - 10.0, Y0 + ScWall + 1.0);
    ScBuildLeaf(B, TEXT("gate.leaf_east"), EastX1 + 10.0, EastX1 + 210.0, Y0 + ScWall + 1.0);

    // A lamp over the gate, warm, no shadows.
    PWLane::FLaneRoomLight Lamp;
    Lamp.Position = FVector(CentreX, Y0 + 90.0, G + 290.0);
    Lamp.Radius = 900.0f;
    Lamp.Lumens = 1400.0f;
    B.OutdoorLights.Add(Lamp);

    B.Points.GateCentre = FVector(C.GateX, Y0, G);
}

void ScBuildBooth(FScBuilt& B, const FScCampus& C)
{
    const double G = ScGroundZ;
    const double Y0 = C.Site.Y0;
    const double X0 = C.GateX + 0.5 * C.GateWidth + ScPillarWidth + 100.0; // 7000 for the 6500 gate: beside the east leaf
    const double X1 = X0 + 300.0;
    const double Ya = Y0 + 70.0;
    const double Yb = Ya + 220.0;
    ScBox(B, TEXT("booth.body"), ELaneColour::WallButter, X0, Ya, G, X1, Yb, G + 250.0);
    ScBox(B, TEXT("booth.plinth"), ELaneColour::Plinth, X0 - 2.0, Ya - 2.0, G, X1 + 2.0, Yb + 2.0, G + 30.0);
    ScBox(B, TEXT("booth.roof"), ELaneColour::TrimDark, X0 - 25.0, Ya - 25.0, G + 250.0, X1 + 25.0, Yb + 25.0, G + 264.0);
    // Window and door face the passage (west): a counter window and a door further along.
    ScBox(B, TEXT("booth.window"), ELaneColour::GlassDusk, X0 - 2.0, Ya + 120.0, G + 110.0, X0, Yb - 20.0, G + 215.0);
    ScBox(B, TEXT("booth.window_sill"), ELaneColour::TrimLight, X0 - 6.0, Ya + 114.0, G + 104.0, X0, Yb - 14.0, G + 110.0);
    ScBox(B, TEXT("booth.door"), ELaneColour::DoorWood, X0 - 2.0, Ya + 20.0, G + 4.0, X0, Ya + 100.0, G + 215.0);
    ScProp(B, TEXT("booth.collision"), X0 - 3.0, Ya - 3.0, G, X1 + 3.0, Yb + 3.0, G + 250.0);
    // The notice at the window: where late arrivals and visitors sign in.
    ScBox(B, TEXT("booth.sign_plate"), ELaneColour::SignGreen, X0 - 4.0, Ya + 118.0, G + 218.0, X0 - 2.0, Yb - 16.0, G + 250.0);
    ScText(B, TEXT("booth.sign_text"), TEXT("Attendance register"), FVector(X0 - 4.5, 0.5 * (Ya + 118.0 + Yb - 16.0), G + 234.0), 6.0f, 180.0);
}

void ScBuildRegister(FScBuilt& B, const FScCampus& C)
{
    const double G = ScGroundZ;
    const double Y0 = C.Site.Y0;
    const double Cx = C.GateX + 0.5 * C.GateWidth + 50.0; // just east of the clear opening: the way in stays 400 cm wide
    const double Cy = Y0 + 150.0;
    ScBox(B, TEXT("register.stand"), ELaneColour::WoodLight, Cx - 22.0, Cy - 18.0, G, Cx + 22.0, Cy + 18.0, G + 96.0);
    ScBox(B, TEXT("register.slope"), ELaneColour::WoodLight, Cx - 32.0, Cy - 26.0, G + 96.0, Cx + 32.0, Cy + 26.0, G + 104.0);
    ScBox(B, TEXT("register.book"), ELaneColour::TrimLight, Cx - 24.0, Cy - 18.0, G + 104.0, Cx + 24.0, Cy + 18.0, G + 107.0);
    ScBox(B, TEXT("register.pen"), ELaneColour::TrimDark, Cx + 4.0, Cy - 3.0, G + 107.0, Cx + 20.0, Cy + 0.0, G + 109.0);
    ScProp(B, TEXT("register.collision"), Cx - 33.0, Cy - 27.0, G, Cx + 33.0, Cy + 27.0, G + 105.0);
    B.Points.SignInPoint = FVector(Cx, Cy, G + 107.0);
    // 110 cm in front, centred on the way in: within the 200 cm reach, in the gate's clear line.
    B.Points.SignInStand = FVector(Cx - 110.0, Cy, G);
}

void ScBuildTimetableBoard(FScBuilt& B, const FScCampus& C)
{
    const double G = ScGroundZ;
    const double Y0 = C.Site.Y0;
    const double X0 = C.GateX - 0.5 * C.GateWidth - ScPillarWidth - 650.0; // 5550 for the 6500 gate; clear of the plinth and pier line
    const double X1 = X0 + 500.0;
    const double Face = Y0 - 14.0;
    ScBox(B, TEXT("board.plate"), ELaneColour::WoodLight, X0, Face, G + 100.0, X1, Y0, G + 270.0);
    ScBox(B, TEXT("board.glass"), ELaneColour::ShopInterior, X0 + 10.0, Face - 2.0, G + 110.0, X1 - 10.0, Face, G + 260.0);
    ScBox(B, TEXT("board.header"), ELaneColour::SignGreen, X0, Face - 3.0, G + 270.0, X1, Y0, G + 290.0);
    ScText(B, TEXT("board.header_text"), TEXT("Notice board"), FVector(0.5 * (X0 + X1), Face - 3.5, G + 280.0), 11.0f);
    B.Points.BoardPoint = FVector(0.5 * (X0 + X1), Face - 6.0, G + 185.0);
    B.Points.BoardTextLocation = FVector(0.5 * (X0 + X1), Face - 2.5, G + 185.0);
    B.Points.BoardTextYaw = -90.0;
    B.Points.BoardTextSizeCm = 8.0f;
    // On the footpath of the para road, 150 cm from the board.
    B.Points.BoardStand = FVector(0.5 * (X0 + X1), Y0 - 165.0, G);
}
} // namespace

void ScBuildGate(FScBuilt& B, const FScCampus& C)
{
    ScBuildBoundary(B, C);
    ScBuildGatework(B, C);
    ScBuildBooth(B, C);
    ScBuildRegister(B, C);
    ScBuildTimetableBoard(B, C);
}
} // namespace PWSchool
