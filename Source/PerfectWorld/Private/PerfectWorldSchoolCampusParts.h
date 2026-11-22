#pragma once

#include "PerfectWorldSchoolCampus.h"

// Small helpers the campus builders share (R7-03). Every box goes into the lane's FLaneBlocks AND is recorded as a named part.

namespace PWSchool
{
using PWLane::ELaneColour;

/** A box between two corners, recorded under Id. Sizes under half a centimetre are dropped, as FLaneBlocks does. */
inline void ScBox(FScBuilt& B, const TCHAR* Id, ELaneColour Colour, double X0, double Y0, double Z0, double X1, double Y1, double Z1)
{
    const FVector A(FMath::Min(X0, X1), FMath::Min(Y0, Y1), FMath::Min(Z0, Z1));
    const FVector C(FMath::Max(X0, X1), FMath::Max(Y0, Y1), FMath::Max(Z0, Z1));
    if (C.X - A.X < 0.5 || C.Y - A.Y < 0.5 || C.Z - A.Z < 0.5)
    {
        return;
    }
    B.Blocks.Box(Colour, A, C);
    FScPart Part;
    Part.Id = FName(Id);
    Part.Colour = Colour;
    Part.Box = FBox(A, C);
    B.Parts.Add(Part);
}

/** Invisible collision hugging a solid prop (blocks the player, the camera and visibility). */
inline void ScProp(FScBuilt& B, const TCHAR* Id, double X0, double Y0, double Z0, double X1, double Y1, double Z1)
{
    ScBox(B, Id, ELaneColour::PropBlock, X0, Y0, Z0, X1, Y1, Z1);
}

/** A box that is Width x Depth centred on (Cx, Cy), from Z0 to Z1. */
inline void ScColumn(FScBuilt& B, const TCHAR* Id, ELaneColour Colour, double Cx, double Cy, double Width, double Depth, double Z0, double Z1)
{
    ScBox(B, Id, Colour, Cx - 0.5 * Width, Cy - 0.5 * Depth, Z0, Cx + 0.5 * Width, Cy + 0.5 * Depth, Z1);
}

/**
 * A wall along X (constant Y range) from XA to XB with door-like openings cut out: each opening is {X0, X1, Top} and leaves a
 * lintel from Top up to ZTop. Colour is the wall colour; IdPrefix names the segments.
 */
struct FScOpening
{
    double X0 = 0.0;
    double X1 = 0.0;
    double Top = 0.0; // absolute Z of the opening head
};

inline void ScWallAlongX(FScBuilt& B, const TCHAR* IdPrefix, ELaneColour Colour, double XA, double XB, double Y0, double Y1, double ZBase,
                         double ZTop, const TArray<FScOpening>& Openings)
{
    double Cursor = XA;
    int32 Index = 0;
    for (const FScOpening& Opening : Openings)
    {
        ScBox(B, *FString::Printf(TEXT("%s.seg%d"), IdPrefix, Index), Colour, Cursor, Y0, ZBase, Opening.X0, Y1, ZTop);
        ScBox(B, *FString::Printf(TEXT("%s.head%d"), IdPrefix, Index), Colour, Opening.X0, Y0, Opening.Top, Opening.X1, Y1, ZTop);
        Cursor = Opening.X1;
        ++Index;
    }
    ScBox(B, *FString::Printf(TEXT("%s.seg%d"), IdPrefix, Index), Colour, Cursor, Y0, ZBase, XB, Y1, ZTop);
}

/** A sign text; Location is the middle of the text at the sign's front. */
inline void ScText(FScBuilt& B, const TCHAR* Id, const FString& Text, const FVector& Location, float SizeCm, double Yaw = -90.0)
{
    FScTextSign Sign;
    Sign.Id = FName(Id);
    Sign.Text = Text;
    Sign.Location = Location;
    Sign.SizeCm = SizeCm;
    Sign.Yaw = Yaw;
    B.Texts.Add(Sign);
}

/** A trunk and three leaf layers; the trunk has a Prop box. */
void ScTree(FScBuilt& B, const TCHAR* Id, double Cx, double Cy, double Height);

// ---- The builders (one file each) ----

/** Earth, paving, assembly markings, flag post, trees, playground, goalposts, water tap. PerfectWorldSchoolCampusGrounds.cpp */
void ScBuildGrounds(FScBuilt& B, const FScCampus& C);
/** Boundary wall, gate pillars, signboard, guard booth, register stand. PerfectWorldSchoolCampusGate.cpp */
void ScBuildGate(FScBuilt& B, const FScCampus& C);
/** The block-letter Bengali line of the signboard (stylised placeholder). PerfectWorldSchoolCampusScript.cpp */
void ScBuildBengaliLine(FScBuilt& B, double CentreX, double Y, double ZBottom, double PixelCm);
/** Three-storey teaching block: shell, verandahs, facade, stair tower, roof. PerfectWorldSchoolCampusBlock.cpp */
void ScBuildBlock(FScBuilt& B, const FScCampus& C);
/** Ground-floor office. PerfectWorldSchoolCampusOffice.cpp */
void ScBuildOffice(FScBuilt& B, const FScCampus& C);
/** The enterable classroom. PerfectWorldSchoolCampusRoom.cpp */
void ScBuildClassroom(FScBuilt& B, const FScCampus& C);

/** Pale plaster over a green dado on the inner faces of a room (decoration), a tile floor skin and a white ceiling skin; the doorway is left open. */
void ScLineRoom(FScBuilt& B, const FString& Id, double X0, double Y0, double X1, double Y1, double DoorX0, double DoorX1, ELaneColour Plaster);
/** A ceiling fan hung from the ceiling of the ground storey at (X, Y): rod, hub and three blades (decoration, no collision). */
void ScFan(FScBuilt& B, const FString& Id, double X, double Y);
/** The solid floor of a ground-floor room and its doorway threshold (the opening's 30 cm of wall depth), level at -185. */
void ScRoomFloor(FScBuilt& B, const FString& Id, const FScCampus& C, double X0, double Y0, double X1, double Y1, double DoorX);

/** Room dimensions both rooms share, so the block, office and classroom agree on every wall. */
namespace ScRoom
{
constexpr double OfficeX0 = 6000.0;     // office west wall (outer face of the stair tower's east wall)
constexpr double PartyX = 7000.0;       // wall between the office and the classroom (30 thick: 7000..7030)
constexpr double OfficeNorth = 7500.0;  // office north wall 7500..7530
constexpr double ClassNorth = 7570.0;   // classroom north wall 7570..7600
constexpr double CeilingBelow = 30.0;   // slab thickness under storey 1
}
} // namespace PWSchool
