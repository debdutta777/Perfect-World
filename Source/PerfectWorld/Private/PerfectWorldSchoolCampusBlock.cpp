#include "PerfectWorldSchoolCampusParts.h"

// The three-storey teaching block, seen from outside: ground-floor shell with the office and classroom doorways, the upper storeys as
// solid massing, a verandah corridor on every floor along the front (pillars, decks, balustrades), classroom doors and windows on the
// facade, the stair tower at the west end and the roof. The office and the classroom are built by their own files (they are the
// only rooms with an inside).

namespace PWSchool
{
namespace
{
constexpr double ScPillarSize = 40.0;
constexpr double ScWindowSill = 100.0;
constexpr double ScWindowHeight = 130.0;
constexpr double ScWindowWidth = 200.0;
constexpr double ScBalustrade = 100.0;

/** A window on the south face: pale frame, dusky glass and three bars. FaceY is the wall's outer face; everything sits proud of it. */
void ScWindowSouth(FScBuilt& B, const FString& Id, double Cx, double FaceY, double ZSill)
{
    const double Half = 0.5 * ScWindowWidth;
    ScBox(B, *(Id + TEXT(".frame")), ELaneColour::TrimLight, Cx - Half - 8.0, FaceY - 6.0, ZSill - 6.0, Cx + Half + 8.0, FaceY, ZSill + ScWindowHeight + 6.0);
    ScBox(B, *(Id + TEXT(".glass")), ELaneColour::GlassDusk, Cx - Half, FaceY - 8.0, ZSill, Cx + Half, FaceY - 6.0, ZSill + ScWindowHeight);
    for (int32 I = 1; I <= 3; ++I)
    {
        const double X = Cx - Half + ScWindowWidth * static_cast<double>(I) / 4.0;
        ScBox(B, *FString::Printf(TEXT("%s.bar%d"), *Id, I), ELaneColour::Rail, X - 1.5, FaceY - 10.0, ZSill, X + 1.5, FaceY - 8.0, ZSill + ScWindowHeight);
    }
    ScBox(B, *(Id + TEXT(".sill")), ELaneColour::TrimLight, Cx - Half - 12.0, FaceY - 12.0, ZSill - 10.0, Cx + Half + 12.0, FaceY - 6.0, ZSill - 6.0);
}

/** A window on the east face (X = FaceX), facing the playground side. */
void ScWindowEast(FScBuilt& B, const FString& Id, double Cy, double FaceX, double ZSill)
{
    const double Half = 70.0;
    ScBox(B, *(Id + TEXT(".frame")), ELaneColour::TrimLight, FaceX, Cy - Half - 7.0, ZSill - 6.0, FaceX + 6.0, Cy + Half + 7.0, ZSill + ScWindowHeight + 6.0);
    ScBox(B, *(Id + TEXT(".glass")), ELaneColour::GlassDusk, FaceX + 6.0, Cy - Half, ZSill, FaceX + 8.0, Cy + Half, ZSill + ScWindowHeight);
    ScBox(B, *(Id + TEXT(".bar")), ELaneColour::Rail, FaceX + 8.0, Cy - 1.5, ZSill, FaceX + 10.0, Cy + 1.5, ZSill + ScWindowHeight);
}

/** A closed classroom door on an upper floor: frame and leaf, decoration only. */
void ScClosedDoor(FScBuilt& B, const FString& Id, double Cx, double FaceY, double ZFloor)
{
    const double Half = 0.5 * ScDoorWidth;
    ScBox(B, *(Id + TEXT(".frame")), ELaneColour::TrimLight, Cx - Half - 10.0, FaceY - 6.0, ZFloor, Cx + Half + 10.0, FaceY, ZFloor + ScDoorHeight + 8.0);
    ScBox(B, *(Id + TEXT(".leaf")), ELaneColour::DoorWood, Cx - Half, FaceY - 8.0, ZFloor, Cx + Half, FaceY - 6.0, ZFloor + ScDoorHeight);
    ScBox(B, *(Id + TEXT(".fanlight")), ELaneColour::GlassDusk, Cx - Half, FaceY - 8.0, ZFloor + ScDoorHeight + 2.0, Cx + Half, FaceY - 6.0, ZFloor + ScDoorHeight + 30.0);
}

void ScBuildShell(FScBuilt& B, const FScCampus& C)
{
    const double G = ScGroundZ;
    const FScRect& T = C.Teaching;
    const double F1 = ScFloorZ(1);
    const double F3 = ScFloorZ(3);
    // Ground floor: the south wall is cut by the two doorways; the other three walls are solid.
    TArray<FScOpening> Openings;
    Openings.Add(FScOpening{C.OfficeDoorX - 0.5 * ScDoorWidth, C.OfficeDoorX + 0.5 * ScDoorWidth, G + ScDoorHeight});
    Openings.Add(FScOpening{C.ClassroomDoorX - 0.5 * ScDoorWidth, C.ClassroomDoorX + 0.5 * ScDoorWidth, G + ScDoorHeight});
    ScWallAlongX(B, TEXT("block.south_wall"), ELaneColour::WallButter, T.X0, T.X1, T.Y0, T.Y0 + ScWall, G, F1, Openings);
    ScBox(B, TEXT("block.north_wall"), ELaneColour::WallButter, T.X0, T.Y1 - ScWall, G, T.X1, T.Y1, F1);
    ScBox(B, TEXT("block.west_wall"), ELaneColour::WallButter, T.X0, T.Y0 + ScWall, G, T.X0 + ScWall, T.Y1 - ScWall, F1);
    ScBox(B, TEXT("block.east_wall"), ELaneColour::WallButter, T.X1 - ScWall, T.Y0 + ScWall, G, T.X1, T.Y1 - ScWall, F1);
    // The ground storey's ceiling slab, and above it storeys two and three as one solid mass.
    ScBox(B, TEXT("block.slab_ground"), ELaneColour::WallCream, T.X0, T.Y0, F1 - ScSlab, T.X1, T.Y1, F1);
    ScBox(B, TEXT("block.upper_mass"), ELaneColour::WallButter, T.X0, T.Y0, F1, T.X1, T.Y1, F3);
    // A plinth band at the foot and pale bands at the floor lines (the decks hide them on the south; they show on the other faces).
    ScBox(B, TEXT("block.plinth_east"), ELaneColour::Plinth, T.X1, T.Y0, G, T.X1 + 3.0, T.Y1, G + 60.0);
    ScBox(B, TEXT("block.plinth_west"), ELaneColour::Plinth, T.X0 - 3.0, T.Y0, G, T.X0, T.Y1, G + 60.0);
    for (int32 Storey = 1; Storey <= 2; ++Storey)
    {
        const double Z = ScFloorZ(Storey);
        ScBox(B, *FString::Printf(TEXT("block.band_east%d"), Storey), ELaneColour::TrimLight, T.X1, T.Y0, Z - 24.0, T.X1 + 5.0, T.Y1, Z + 4.0);
        ScBox(B, *FString::Printf(TEXT("block.band_west%d"), Storey), ELaneColour::TrimLight, T.X0 - 5.0, T.Y0, Z - 24.0, T.X0, T.Y1, Z + 4.0);
        ScBox(B, *FString::Printf(TEXT("block.band_north%d"), Storey), ELaneColour::TrimLight, T.X0, T.Y1, Z - 24.0, T.X1, T.Y1 + 5.0, Z + 4.0);
    }
}

void ScBuildVerandahs(FScBuilt& B, const FScCampus& C)
{
    const double G = ScGroundZ;
    const FScRect& T = C.Teaching;
    const double Y0 = T.Y0 - ScVerandahDepth;
    const double F3 = ScFloorZ(3);
    // Decks: the ground verandah's roof is also the first-floor corridor floor; the top deck is the canopy over the third storey.
    for (int32 Deck = 1; Deck <= 3; ++Deck)
    {
        const double Z = ScFloorZ(Deck);
        ScBox(B, *FString::Printf(TEXT("verandah.deck%d"), Deck), ELaneColour::WallCream, T.X0, Y0, Z - ScSlab, T.X1, T.Y0, Z);
        ScBox(B, *FString::Printf(TEXT("verandah.deck_edge%d"), Deck), ELaneColour::TrimLight, T.X0, Y0 - 4.0, Z - ScSlab, T.X1, Y0, Z + 3.0);
    }
    // Balustrades on the two walking decks (floors one and two), along the front and returned at the east end.
    for (int32 Deck = 1; Deck <= 2; ++Deck)
    {
        const double Z = ScFloorZ(Deck);
        ScBox(B, *FString::Printf(TEXT("verandah.parapet%d"), Deck), ELaneColour::WallCream, T.X0, Y0, Z, T.X1, Y0 + 25.0, Z + ScBalustrade);
        ScBox(B, *FString::Printf(TEXT("verandah.parapet_end%d"), Deck), ELaneColour::WallCream, T.X1 - 25.0, Y0 + 25.0, Z, T.X1, T.Y0, Z + ScBalustrade);
        ScBox(B, *FString::Printf(TEXT("verandah.parapet_cap%d"), Deck), ELaneColour::TrimLight, T.X0, Y0 - 3.0, Z + ScBalustrade, T.X1, Y0 + 28.0, Z + ScBalustrade + 5.0);
    }
    // Pillars at 500 cm: the bays between them are centred on the office door (6500) and the classroom door (7500).
    const double PillarY = Y0 + 25.0;
    int32 Index = 0;
    for (double X = T.X0 + 250.0; X < T.X1 - 100.0; X += 500.0)
    {
        ScColumn(B, *FString::Printf(TEXT("verandah.pillar%d"), Index), ELaneColour::WallCream, X, PillarY, ScPillarSize, ScPillarSize, G, F3 - ScSlab);
        ScColumn(B, *FString::Printf(TEXT("verandah.pillar_base%d"), Index), ELaneColour::Plinth, X, PillarY, ScPillarSize + 8.0, ScPillarSize + 8.0, G, G + 30.0);
        ++Index;
    }
    // An end column at the east, and the west end closed by the block's wall continuing out to the deck edge.
    ScColumn(B, TEXT("verandah.pillar_end"), ELaneColour::WallCream, T.X1 - 20.0, PillarY, ScPillarSize, ScPillarSize, G, F3 - ScSlab);
    ScBox(B, TEXT("verandah.west_wall"), ELaneColour::WallButter, T.X0, Y0, G, T.X0 + ScWall, T.Y0, F3);
    // The soffit of the ground verandah: pale ceiling and two warm lamps (no shadows).
    ScBox(B, TEXT("verandah.soffit"), ELaneColour::CeilingWhite, T.X0 + ScWall, Y0, ScFloorZ(1) - ScSlab - 1.5, T.X1, T.Y0, ScFloorZ(1) - ScSlab);
    for (const double X : {C.OfficeDoorX, C.ClassroomDoorX})
    {
        PWLane::FLaneRoomLight Lamp;
        Lamp.Position = FVector(X, Y0 + 170.0, ScFloorZ(1) - ScSlab - 25.0);
        Lamp.Radius = 650.0f;
        Lamp.Lumens = 700.0f;
        B.OutdoorLights.Add(Lamp);
    }
}

void ScBuildFacade(FScBuilt& B, const FScCampus& C)
{
    const double G = ScGroundZ;
    const FScRect& T = C.Teaching;
    const double Face = T.Y0; // the south face of the block (the verandah's back wall)
    const double Half = 0.5 * ScDoorWidth;
    // Ground floor: window pairs for the office and the classroom, door frames and name plates over the doorways.
    const double Centres[] = {C.OfficeDoorX - 260.0, C.OfficeDoorX + 260.0, C.ClassroomDoorX - 240.0, C.ClassroomDoorX + 240.0};
    int32 Index = 0;
    for (const double X : Centres)
    {
        ScWindowSouth(B, FString::Printf(TEXT("facade.ground_window%d"), Index++), X, Face, G + ScWindowSill);
    }
    const struct { const TCHAR* Id; double X; const TCHAR* Label; } Doors[] = {
        {TEXT("office"), C.OfficeDoorX, TEXT("OFFICE")}, {TEXT("classroom"), C.ClassroomDoorX, TEXT("CLASS XI")}};
    for (const auto& Door : Doors)
    {
        const FString Id = FString::Printf(TEXT("facade.%s_door"), Door.Id);
        ScBox(B, *(Id + TEXT("_jamb_west")), ELaneColour::TrimLight, Door.X - Half - 10.0, Face - 6.0, G, Door.X - Half, Face, G + ScDoorHeight + 8.0);
        ScBox(B, *(Id + TEXT("_jamb_east")), ELaneColour::TrimLight, Door.X + Half, Face - 6.0, G, Door.X + Half + 10.0, Face, G + ScDoorHeight + 8.0);
        ScBox(B, *(Id + TEXT("_head")), ELaneColour::TrimLight, Door.X - Half - 10.0, Face - 6.0, G + ScDoorHeight, Door.X + Half + 10.0, Face, G + ScDoorHeight + 8.0);
        ScBox(B, *FString::Printf(TEXT("facade.%s_plate"), Door.Id), ELaneColour::SignBlue, Door.X - 80.0, Face - 5.0, G + ScDoorHeight + 14.0, Door.X + 80.0, Face, G + ScDoorHeight + 56.0);
        ScText(B, *FString::Printf(TEXT("facade.%s_label"), Door.Id), Door.Label, FVector(Door.X, Face - 5.5, G + ScDoorHeight + 35.0), 14.0f);
    }
    // Storeys two and three: the same window rhythm, closed classroom doors over the two doorways, east-face windows.
    for (int32 Storey = 1; Storey <= 2; ++Storey)
    {
        const double Z = ScFloorZ(Storey);
        int32 N = 0;
        for (const double X : Centres)
        {
            ScWindowSouth(B, FString::Printf(TEXT("facade.floor%d_window%d"), Storey + 1, N++), X, Face, Z + ScWindowSill);
        }
        ScClosedDoor(B, FString::Printf(TEXT("facade.floor%d_door_office_side"), Storey + 1), C.OfficeDoorX, Face, Z);
        ScClosedDoor(B, FString::Printf(TEXT("facade.floor%d_door_class_side"), Storey + 1), C.ClassroomDoorX, Face, Z);
    }
    for (int32 Storey = 0; Storey <= 2; ++Storey)
    {
        const double Z = ScFloorZ(Storey);
        int32 N = 0;
        for (double Y = T.Y0 + 700.0; Y < T.Y1 - 400.0; Y += 600.0)
        {
            ScWindowEast(B, FString::Printf(TEXT("facade.east_floor%d_window%d"), Storey + 1, N++), Y, T.X1, Z + ScWindowSill);
        }
    }
}

/** The stair tower at the west end: a tall glazed strip with the flights showing through, and its closed grille door. */
void ScBuildStairTower(FScBuilt& B, const FScCampus& C)
{
    const double G = ScGroundZ;
    const FScRect& T = C.Teaching;
    const double Face = T.Y0;
    const double Cx = T.X0 + 250.0;
    const double Top = ScFloorZ(3);
    ScBox(B, TEXT("stairs.glass"), ELaneColour::GlassDusk, Cx - 90.0, Face - 6.0, G + 60.0, Cx + 90.0, Face - 4.0, Top - 70.0);
    ScBox(B, TEXT("stairs.frame_west"), ELaneColour::TrimLight, Cx - 100.0, Face - 8.0, G + 50.0, Cx - 90.0, Face - 2.0, Top - 60.0);
    ScBox(B, TEXT("stairs.frame_east"), ELaneColour::TrimLight, Cx + 90.0, Face - 8.0, G + 50.0, Cx + 100.0, Face - 2.0, Top - 60.0);
    for (int32 Storey = 0; Storey <= 2; ++Storey)
    {
        const double Z = ScFloorZ(Storey);
        ScBox(B, *FString::Printf(TEXT("stairs.landing%d"), Storey), ELaneColour::TrimLight, Cx - 100.0, Face - 8.0, Z + 170.0, Cx + 100.0, Face - 2.0, Z + 180.0);
        // Two flights per storey as dark diagonals behind the glass.
        B.Blocks.Tilted(ELaneColour::TrimDark, FVector(Cx - 30.0, Face - 7.0, Z + 70.0), FVector(140.0, 2.0, 8.0), FRotator(34.0, 0.0, 0.0));
        B.Blocks.Tilted(ELaneColour::TrimDark, FVector(Cx + 30.0, Face - 7.0, Z + 250.0), FVector(140.0, 2.0, 8.0), FRotator(-34.0, 0.0, 0.0));
    }
    ScBox(B, TEXT("stairs.grille"), ELaneColour::Rail, Cx - 60.0, Face - 6.0, G, Cx + 60.0, Face - 3.0, G + 200.0);
    ScBox(B, TEXT("stairs.sign_plate"), ELaneColour::SignGreen, Cx - 50.0, Face - 5.0, G + 205.0, Cx + 50.0, Face, G + 245.0);
    ScText(B, TEXT("stairs.sign_text"), TEXT("STAIRS"), FVector(Cx, Face - 5.5, G + 225.0), 13.0f);
}

void ScBuildRoof(FScBuilt& B, const FScCampus& C)
{
    const FScRect& T = C.Teaching;
    const double Z = ScFloorZ(3);
    constexpr double Height = 70.0;
    ScBox(B, TEXT("roof.parapet_south"), ELaneColour::WallCream, T.X0, T.Y0, Z, T.X1, T.Y0 + 25.0, Z + Height);
    ScBox(B, TEXT("roof.parapet_north"), ELaneColour::WallCream, T.X0, T.Y1 - 25.0, Z, T.X1, T.Y1, Z + Height);
    ScBox(B, TEXT("roof.parapet_west"), ELaneColour::WallCream, T.X0, T.Y0 + 25.0, Z, T.X0 + 25.0, T.Y1 - 25.0, Z + Height);
    ScBox(B, TEXT("roof.parapet_east"), ELaneColour::WallCream, T.X1 - 25.0, T.Y0 + 25.0, Z, T.X1, T.Y1 - 25.0, Z + Height);
    // Stair head-house over the tower, a water tank on legs and a pair of vents.
    ScBox(B, TEXT("roof.headhouse"), ELaneColour::WallButter, T.X0 + 30.0, T.Y0 + 120.0, Z, T.X0 + 450.0, T.Y0 + 520.0, Z + 240.0);
    ScBox(B, TEXT("roof.headhouse_cap"), ELaneColour::TrimLight, T.X0 + 20.0, T.Y0 + 110.0, Z + 240.0, T.X0 + 460.0, T.Y0 + 530.0, Z + 252.0);
    const double TankX = T.X1 - 420.0;
    const double TankY = T.Y1 - 700.0;
    for (const double Dx : {-70.0, 70.0})
    {
        for (const double Dy : {-70.0, 70.0})
        {
            ScColumn(B, *FString::Printf(TEXT("roof.tank_leg_%d_%d"), Dx < 0 ? 0 : 1, Dy < 0 ? 0 : 1), ELaneColour::TrimDark, TankX + Dx, TankY + Dy, 12.0, 12.0, Z + Height, Z + Height + 90.0);
        }
    }
    ScColumn(B, TEXT("roof.tank"), ELaneColour::TankBlack, TankX, TankY, 190.0, 190.0, Z + Height + 90.0, Z + Height + 230.0);
    ScColumn(B, TEXT("roof.tank_lid"), ELaneColour::TankLid, TankX, TankY, 170.0, 170.0, Z + Height + 230.0, Z + Height + 242.0);
    for (int32 I = 0; I < 2; ++I)
    {
        ScColumn(B, *FString::Printf(TEXT("roof.vent%d"), I), ELaneColour::AcUnit, T.X0 + 1000.0 + 700.0 * static_cast<double>(I), T.Y0 + 900.0, 70.0, 70.0, Z + Height, Z + Height + 110.0);
    }
}
} // namespace

void ScBuildBlock(FScBuilt& B, const FScCampus& C)
{
    ScBuildShell(B, C);
    ScBuildVerandahs(B, C);
    ScBuildFacade(B, C);
    ScBuildStairTower(B, C);
    ScBuildRoof(B, C);
}
} // namespace PWSchool
