#include "PerfectWorldSchoolCampusParts.h"

// The open ground of Nabapara Vidyalaya: earth, paving, the assembly area and flag post, trees, the playground with its goalposts
// and the water tap. All extents come from the plan's campus (FScCampus).

namespace PWSchool
{
namespace
{
constexpr double ScLine = 1.2; // painted lines lie this high on the paving; below the 15 cm kerb and the 45 cm step

/** A painted line (no collision), axis aligned. */
void ScPaint(FScBuilt& B, const TCHAR* Id, double X0, double Y0, double X1, double Y1, double Z)
{
    ScBox(B, Id, ELaneColour::TrimLight, X0, Y0, Z, X1, Y1, Z + ScLine);
}

/** A hollow rectangle of painted lines, Width wide. */
void ScPaintRect(FScBuilt& B, const FString& Id, double X0, double Y0, double X1, double Y1, double Width, double Z)
{
    ScPaint(B, *(Id + TEXT(".south")), X0, Y0, X1, Y0 + Width, Z);
    ScPaint(B, *(Id + TEXT(".north")), X0, Y1 - Width, X1, Y1, Z);
    ScPaint(B, *(Id + TEXT(".west")), X0, Y0, X0 + Width, Y1, Z);
    ScPaint(B, *(Id + TEXT(".east")), X1 - Width, Y0, X1, Y1, Z);
}

void ScBuildPaving(FScBuilt& B, const FScCampus& C)
{
    const double G = ScGroundZ;
    const FScRect& S = C.Site;
    const FScRect& F = C.Forecourt;
    const FScRect& T = C.Teaching;
    const FScRect& P = C.Playground;
    // Packed earth over the whole site; its top is 2 cm under the paving so the two never fight.
    ScBox(B, TEXT("ground.earth"), ELaneColour::Ground, S.X0, S.Y0, G - 22.0, S.X1, S.Y1, G - 2.0);
    // The forecourt, including the gate's sill, and the covered way to the verandah, level with the road side footpath (-185).
    ScBox(B, TEXT("paving.forecourt"), ELaneColour::Footpath, F.X0, F.Y0, G - 15.0, F.X1, F.Y1, G);
    ScBox(B, TEXT("paving.to_block"), ELaneColour::Footpath, T.X0, F.Y1, G - 15.0, T.X1, T.Y0, G);
    // The path between the block's east end and the playground (the plan's playground link node is at the foot of it).
    ScBox(B, TEXT("paving.to_playground"), ELaneColour::Footpath, T.X1, F.Y1, G - 15.0, P.X0, T.Y0, G);
    // A paved apron along the block's east face, so the way to the water tap is dry underfoot.
    ScBox(B, TEXT("paving.east_apron"), ELaneColour::Footpath, T.X1, T.Y0, G - 15.0, P.X0, T.Y0 + 1500.0, G);
}

void ScBuildAssembly(FScBuilt& B, const FScCampus& C)
{
    const double G = ScGroundZ;
    const FScRect& F = C.Forecourt;
    const double X0 = F.X0 + 1900.0;
    const double X1 = F.X1 - 500.0;
    const double Y0 = F.Y0 + 150.0;
    const double Y1 = F.Y1 - 200.0;
    ScPaintRect(B, TEXT("assembly.outline"), X0, Y0, X1, Y1, 8.0, G);
    // Six classes' columns, each a standing line for the morning assembly.
    for (int32 I = 1; I < 6; ++I)
    {
        const double X = X0 + (X1 - X0) * static_cast<double>(I) / 6.0;
        ScPaint(B, *FString::Printf(TEXT("assembly.column%d"), I), X - 3.0, Y0, X + 3.0, Y1, G);
    }
    // The dais at the north edge, where the headmaster and the prefects stand: 30 cm up, within the capsule's step.
    ScBox(B, TEXT("assembly.dais"), ELaneColour::WallCream, X0 + 1500.0, Y1 - 120.0, G, X0 + 2300.0, F.Y1 - 10.0, G + 30.0);
    ScBox(B, TEXT("assembly.dais_trim"), ELaneColour::TrimDark, X0 + 1500.0, Y1 - 120.0, G + 30.0, X0 + 2300.0, Y1 - 110.0, G + 31.0);
    ScColumn(B, TEXT("assembly.lectern"), ELaneColour::WoodLight, X0 + 1900.0, F.Y1 - 80.0, 60.0, 40.0, G + 30.0, G + 120.0);
    ScProp(B, TEXT("assembly.lectern_collision"), X0 + 1870.0, F.Y1 - 100.0, G + 30.0, X0 + 1930.0, F.Y1 - 60.0, G + 120.0);
}

void ScBuildFlagPost(FScBuilt& B, const FScCampus& C)
{
    const double G = ScGroundZ;
    const double Cx = C.Forecourt.X0 + 1600.0;
    const double Cy = C.Forecourt.Y0 + 850.0;
    ScBox(B, TEXT("flag.plinth"), ELaneColour::WallCream, Cx - 100.0, Cy - 100.0, G, Cx + 100.0, Cy + 100.0, G + 20.0);
    ScBox(B, TEXT("flag.plinth_step"), ELaneColour::WallCream, Cx - 60.0, Cy - 60.0, G + 20.0, Cx + 60.0, Cy + 60.0, G + 40.0);
    ScColumn(B, TEXT("flag.pole"), ELaneColour::Chrome, Cx, Cy, 12.0, 12.0, G + 40.0, G + 740.0);
    ScProp(B, TEXT("flag.pole_collision"), Cx - 8.0, Cy - 8.0, G + 20.0, Cx + 8.0, Cy + 8.0, G + 740.0);
    ScProp(B, TEXT("flag.plinth_collision"), Cx - 100.0, Cy - 100.0, G, Cx + 100.0, Cy + 100.0, G + 40.0);
    // The tricolour hangs from the top of the pole, along +X; thin (2 cm) so it shows from both sides.
    const double FlagX0 = Cx + 6.0;
    const double FlagX1 = Cx + 156.0;
    ScBox(B, TEXT("flag.saffron"), ELaneColour::GoodsOrange, FlagX0, Cy - 1.0, G + 690.0, FlagX1, Cy + 1.0, G + 730.0);
    ScBox(B, TEXT("flag.white"), ELaneColour::TrimLight, FlagX0, Cy - 1.0, G + 650.0, FlagX1, Cy + 1.0, G + 690.0);
    ScBox(B, TEXT("flag.green"), ELaneColour::GoodsGreen, FlagX0, Cy - 1.0, G + 610.0, FlagX1, Cy + 1.0, G + 650.0);
    ScBox(B, TEXT("flag.wheel"), ELaneColour::SignBlue, 0.5 * (FlagX0 + FlagX1) - 9.0, Cy - 1.6, G + 661.0, 0.5 * (FlagX0 + FlagX1) + 9.0, Cy + 1.6, G + 679.0);
    B.Points.FlagPost = FVector(Cx, Cy, G);
}

void ScBuildTrees(FScBuilt& B, const FScCampus& C)
{
    const FScRect& S = C.Site;
    const FScRect& F = C.Forecourt;
    const FScRect& T = C.Teaching;
    ScTree(B, TEXT("tree.neem_gate"), F.X0 + 400.0, F.Y0 + 560.0, 330.0);
    ScTree(B, TEXT("tree.neem_forecourt_east"), F.X1 - 250.0, F.Y0 + 560.0, 340.0);
    ScTree(B, TEXT("tree.west_a"), S.X0 + 250.0, T.Y0 + 1500.0, 320.0);
    ScTree(B, TEXT("tree.west_b"), S.X0 + 250.0, T.Y0 + 3500.0, 360.0);
    ScTree(B, TEXT("tree.field_east_a"), C.Playground.X1 - 200.0, C.Playground.Y0 + 500.0, 340.0);
    ScTree(B, TEXT("tree.field_east_b"), C.Playground.X1 - 200.0, C.Playground.Y1 - 500.0, 350.0);
}

/** A twelve-sided centre circle of painted segments (tilted boxes carry no part record). */
void ScPaintCircle(FScBuilt& B, double Cx, double Cy, double Radius, double Z)
{
    constexpr int32 Sides = 12;
    const double Half = Radius * FMath::Tan(PI / Sides);
    for (int32 I = 0; I < Sides; ++I)
    {
        const double Angle = 2.0 * PI * static_cast<double>(I) / Sides;
        const FVector Centre(Cx + Radius * FMath::Cos(Angle), Cy + Radius * FMath::Sin(Angle), Z + 0.5 * ScLine);
        B.Blocks.Tilted(ELaneColour::TrimLight, Centre, FVector(8.0, 2.0 * Half, ScLine), FRotator(0.0, FMath::RadiansToDegrees(Angle), 0.0));
    }
}

void ScBuildGoal(FScBuilt& B, const TCHAR* Id, double Cx, double Y, double Facing)
{
    const double G = ScGroundZ;
    constexpr double Mouth = 500.0;
    constexpr double Height = 244.0;
    const double XLeft = Cx - 0.5 * Mouth;
    const double XRight = Cx + 0.5 * Mouth;
    for (const double X : {XLeft, XRight})
    {
        const FString Post = FString::Printf(TEXT("%s.post_%d"), Id, X < Cx ? 0 : 1);
        ScColumn(B, *Post, ELaneColour::TrimLight, X, Y, 12.0, 12.0, G, G + Height);
        ScProp(B, *(Post + TEXT("_collision")), X - 7.0, Y - 7.0, G, X + 7.0, Y + 7.0, G + Height);
        // A back stay leaning away from the pitch, tied to a short peg.
        ScBox(B, *(Post + TEXT("_stay")), ELaneColour::TrimLight, X - 3.0, Y + Facing * 120.0 - 3.0, G + 2.0, X + 3.0, Y + Facing * 120.0 + 3.0, G + 8.0);
        B.Blocks.Wire(ELaneColour::TrimLight, FVector(X, Y, G + Height - 4.0), FVector(X, Y + Facing * 120.0, G + 5.0), 4.0);
    }
    ScBox(B, *FString::Printf(TEXT("%s.crossbar"), Id), ELaneColour::TrimLight, XLeft - 6.0, Y - 6.0, G + Height - 12.0, XRight + 6.0, Y + 6.0, G + Height);
    // The net: a thin pale panel across the back, no collision.
    ScBox(B, *FString::Printf(TEXT("%s.net_back"), Id), ELaneColour::Chrome, XLeft, Y + Facing * 120.0 - 0.6, G + 2.0, XRight, Y + Facing * 120.0 + 0.6, G + Height - 4.0);
}

void ScBuildPlayground(FScBuilt& B, const FScCampus& C)
{
    const double G = ScGroundZ;
    const FScRect& P = C.Playground;
    const double X0 = P.X0 + 350.0;
    const double X1 = P.X1 - 350.0;
    const double Y0 = P.Y0 + 350.0;
    const double Y1 = P.Y1 - 350.0;
    const double Cx = P.CentreX();
    const double Cy = P.CentreY();
    const double Z = G - 2.0; // the earth's top
    ScPaintRect(B, TEXT("field.outline"), X0, Y0, X1, Y1, 10.0, Z);
    ScPaint(B, TEXT("field.halfway"), X0, Cy - 5.0, X1, Cy + 5.0, Z);
    ScPaintCircle(B, Cx, Cy, 250.0, Z);
    ScPaintRect(B, TEXT("field.box_south"), Cx - 450.0, Y0, Cx + 450.0, Y0 + 300.0, 8.0, Z);
    ScPaintRect(B, TEXT("field.box_north"), Cx - 450.0, Y1 - 300.0, Cx + 450.0, Y1, 8.0, Z);
    ScBuildGoal(B, TEXT("goal.south"), Cx, Y0, -1.0);
    ScBuildGoal(B, TEXT("goal.north"), Cx, Y1, 1.0);
    // A low earth bank around the field and two benches in the shade, so it reads as a playground and not an empty lot.
    for (const double Side : {-1.0, 1.0})
    {
        const double X = Cx + Side * 0.5 * (X1 - X0 + 260.0);
        ScBox(B, *FString::Printf(TEXT("playground.kerb_%d"), Side < 0 ? 0 : 1), ELaneColour::Plinth, X - 5.0, Y0 - 100.0, G - 2.0, X + 5.0, Y1 + 100.0, G + 8.0);
    }
    for (int32 I = 0; I < 2; ++I)
    {
        const double Y = P.Y0 + 1800.0 + static_cast<double>(I) * 1800.0;
        const double X = P.X1 - 220.0;
        ScBox(B, *FString::Printf(TEXT("bench.field%d.seat"), I), ELaneColour::WoodLight, X - 25.0, Y - 90.0, G + 38.0, X + 25.0, Y + 90.0, G + 44.0);
        ScBox(B, *FString::Printf(TEXT("bench.field%d.back"), I), ELaneColour::WoodLight, X + 21.0, Y - 90.0, G + 44.0, X + 25.0, Y + 90.0, G + 85.0);
        ScBox(B, *FString::Printf(TEXT("bench.field%d.leg_a"), I), ELaneColour::TrimDark, X - 22.0, Y - 80.0, G, X + 22.0, Y - 72.0, G + 38.0);
        ScBox(B, *FString::Printf(TEXT("bench.field%d.leg_b"), I), ELaneColour::TrimDark, X - 22.0, Y + 72.0, G, X + 22.0, Y + 80.0, G + 38.0);
        ScProp(B, *FString::Printf(TEXT("bench.field%d.collision"), I), X - 26.0, Y - 91.0, G, X + 26.0, Y + 91.0, G + 85.0);
    }
    B.Points.Playground = FVector(Cx, Cy, G);
}

void ScBuildWaterTap(FScBuilt& B, const FScCampus& C)
{
    const double G = ScGroundZ;
    const double Cx = C.Teaching.X1 + 200.0; // beside the block's east face, on the paved apron
    const double Y = C.Teaching.Y0 + 900.0;
    ScBox(B, TEXT("tap.trough"), ELaneColour::Concrete, Cx - 100.0, Y - 25.0, G, Cx + 100.0, Y + 25.0, G + 40.0);
    ScBox(B, TEXT("tap.basin"), ELaneColour::Drain, Cx - 92.0, Y - 17.0, G + 38.0, Cx + 92.0, Y + 17.0, G + 41.0);
    ScProp(B, TEXT("tap.trough_collision"), Cx - 102.0, Y - 27.0, G, Cx + 102.0, Y + 27.0, G + 42.0);
    // The standpipe: a pipe on two posts behind the trough, with three taps over it.
    const double WallY = Y + 30.0;
    for (const double PostX : {Cx - 100.0, Cx + 100.0})
    {
        const FString Post = FString::Printf(TEXT("tap.post_%d"), PostX < Cx ? 0 : 1);
        ScColumn(B, *Post, ELaneColour::WallCream, PostX, WallY, 12.0, 12.0, G, G + 105.0);
        ScProp(B, *(Post + TEXT("_collision")), PostX - 7.0, WallY - 7.0, G, PostX + 7.0, WallY + 7.0, G + 105.0);
    }
    ScBox(B, TEXT("tap.pipe"), ELaneColour::Chrome, Cx - 100.0, WallY - 3.0, G + 95.0, Cx + 100.0, WallY + 3.0, G + 101.0);
    for (int32 I = 0; I < 3; ++I)
    {
        const double X = Cx - 70.0 + 70.0 * static_cast<double>(I);
        ScBox(B, *FString::Printf(TEXT("tap.spout%d"), I), ELaneColour::Chrome, X - 2.5, WallY - 12.0, G + 88.0, X + 2.5, WallY - 4.0, G + 98.0);
        ScBox(B, *FString::Printf(TEXT("tap.handle%d"), I), ELaneColour::GoodsRed, X - 5.0, WallY - 1.5, G + 101.0, X + 5.0, WallY + 1.5, G + 105.0);
    }
    ScBox(B, TEXT("tap.drain"), ELaneColour::Drain, Cx - 90.0, Y - 60.0, G + 0.2, Cx + 90.0, Y - 30.0, G + 1.0);
    B.Points.WaterTap = FVector(Cx, Y - 80.0, G);
}
} // namespace

void ScBuildGrounds(FScBuilt& B, const FScCampus& C)
{
    ScBuildPaving(B, C);
    ScBuildAssembly(B, C);
    ScBuildFlagPost(B, C);
    ScBuildTrees(B, C);
    ScBuildPlayground(B, C);
    ScBuildWaterTap(B, C);
}
} // namespace PWSchool
