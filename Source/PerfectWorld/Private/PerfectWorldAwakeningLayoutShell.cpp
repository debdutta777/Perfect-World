#include "PerfectWorldAwakeningLayout.h"

// The sealed room, its glowing grid, the teacher's safe area, the arrival and return pads, the lights and the campus terminal.

namespace PWAwakening
{
void AwAddPadVisual(FAwFloorLayout& L, const FAwPadSpec& Pad, const EAwColour Plate, const EAwColour Shape)
{
    const double H = 0.5 * AwNumbers::PadSizeCm;
    const FString PlateId = Pad.Id.ToString() + TEXT(".plate");
    AwAddWorldPart(L, *PlateId, Pad.Kind == EAwPadKind::Reset ? AwAssetIds::ResetPad : AwAssetIds::ChoicePad, Plate, EAwRole::Visual,
                   Pad.Centre + FVector(-H, -H, 0.0), Pad.Centre + FVector(H, H, 4.0));
    TArray<TPair<FVector, FVector>> Boxes;
    AwShapeBoxes(Pad.Shape, Boxes);
    int32 K = 0;
    for (const TPair<FVector, FVector>& Box : Boxes)
    {
        const FString Id = FString::Printf(TEXT("%s.shape%d"), *Pad.Id.ToString(), K++);
        AwAddWorldPart(L, *Id, AwAssetIds::ChoicePad, Shape, EAwRole::Visual, Pad.Centre + Box.Key - 0.5 * Box.Value,
                       Pad.Centre + Box.Key + 0.5 * Box.Value);
    }
}

namespace
{
void AwFlBuildRoom(FAwFloorLayout& L)
{
    constexpr double Ceil = AwNumbers::CeilingZ;
    AwAddPart(L, TEXT("floor.panel"), AwAssetIds::FloorGrid, EAwColour::FloorPanel, EAwRole::Solid, -1400, 0, -40, 1400, 3800, 0);
    AwAddPart(L, TEXT("wall.west"), AwAssetIds::WallPanel, EAwColour::WallPanel, EAwRole::Solid, -1430, -30, -40, -1400, 3830, Ceil);
    AwAddPart(L, TEXT("wall.east"), AwAssetIds::WallPanel, EAwColour::WallPanel, EAwRole::Solid, 1400, -30, -40, 1430, 3830, Ceil);
    AwAddPart(L, TEXT("wall.south"), AwAssetIds::WallPanel, EAwColour::WallPanel, EAwRole::Solid, -1430, -30, -40, 1430, 0, Ceil);
    AwAddPart(L, TEXT("wall.north"), AwAssetIds::WallPanel, EAwColour::WallPanel, EAwRole::Solid, -1430, 3800, -40, 1430, 3830, Ceil);
    AwAddPart(L, TEXT("ceiling.panel"), AwAssetIds::WallPanel, EAwColour::Ceiling, EAwRole::Solid, -1430, -30, Ceil, 1430, 3830, Ceil + 30);
    // Teal accent strips on the inner faces.
    AwAddPart(L, TEXT("accent.west"), AwAssetIds::WallPanel, EAwColour::GateTeal, EAwRole::Visual, -1400, 0, 200, -1398, 3800, 212);
    AwAddPart(L, TEXT("accent.east"), AwAssetIds::WallPanel, EAwColour::GateTeal, EAwRole::Visual, 1398, 0, 200, 1400, 3800, 212);
    AwAddPart(L, TEXT("accent.south"), AwAssetIds::WallPanel, EAwColour::GateTeal, EAwRole::Visual, -1400, 0, 200, 1400, 2, 212);
    AwAddPart(L, TEXT("accent.north"), AwAssetIds::WallPanel, EAwColour::GateTeal, EAwRole::Visual, -1400, 3798, 200, 1400, 3800, 212);
    // The glowing grid: a line every two metres.
    for (int32 I = -6; I <= 6; ++I)
    {
        const double X = 200.0 * I;
        AwAddPart(L, *FString::Printf(TEXT("grid.x%d"), I + 6), AwAssetIds::FloorGrid, EAwColour::GridLine, EAwRole::Visual, X - 3, 0, 0, X + 3, 3800, 1.2);
    }
    for (int32 I = 1; I <= 18; ++I)
    {
        const double Y = 200.0 * I;
        AwAddPart(L, *FString::Printf(TEXT("grid.y%d"), I), AwAssetIds::FloorGrid, EAwColour::GridLine, EAwRole::Visual, -1400, Y - 3, 0, 1400, Y + 3, 1.2);
    }
}

void AwFlBuildSafeArea(FAwFloorLayout& L)
{
    L.SafeArea = FBox(AwWorld(L, -1400.0, 150.0, 0.0), AwWorld(L, -700.0, 1350.0, 200.0));
    L.SafeStand = AwWorld(L, -950.0, 400.0, 0.0);
    L.SafeYaw = 90.0;
    AwAddPart(L, TEXT("safe.floor"), AwAssetIds::SafeArea, EAwColour::SafeGreen, EAwRole::Visual, -1400, 150, 0, -700, 1350, 2.2);
    // Soft light barriers round the area: posts and a rail, no collision. The east edge has a 3 m opening.
    int32 N = 0;
    for (double Y = 150.0; Y <= 1350.0; Y += 100.0)
    {
        if (Y >= 600.0 && Y <= 900.0)
        {
            continue;
        }
        AwAddPart(L, *FString::Printf(TEXT("safe.barrier.east.post%d"), N++), AwAssetIds::LightBarrier, EAwColour::GateTeal, EAwRole::Visual, -703, Y - 3, 0, -697, Y + 3, 90);
    }
    AwAddPart(L, TEXT("safe.barrier.east.rail_a"), AwAssetIds::LightBarrier, EAwColour::GateTeal, EAwRole::Visual, -702, 150, 84, -698, 600, 90);
    AwAddPart(L, TEXT("safe.barrier.east.rail_b"), AwAssetIds::LightBarrier, EAwColour::GateTeal, EAwRole::Visual, -702, 900, 84, -698, 1350, 90);
    N = 0;
    for (double X = -1400.0; X <= -700.0; X += 100.0)
    {
        AwAddPart(L, *FString::Printf(TEXT("safe.barrier.south.post%d"), N), AwAssetIds::LightBarrier, EAwColour::GateTeal, EAwRole::Visual, X - 3, 147, 0, X + 3, 153, 90);
        AwAddPart(L, *FString::Printf(TEXT("safe.barrier.north.post%d"), N), AwAssetIds::LightBarrier, EAwColour::GateTeal, EAwRole::Visual, X - 3, 1347, 0, X + 3, 1353, 90);
        ++N;
    }
    AwAddPart(L, TEXT("safe.barrier.south.rail"), AwAssetIds::LightBarrier, EAwColour::GateTeal, EAwRole::Visual, -1400, 148, 84, -700, 152, 90);
    AwAddPart(L, TEXT("safe.barrier.north.rail"), AwAssetIds::LightBarrier, EAwColour::GateTeal, EAwRole::Visual, -1400, 1348, 84, -700, 1352, 90);
    // The teacher's table.
    AwAddPart(L, TEXT("safe.table.body"), AwAssetIds::StationCuboid, EAwColour::Ink, EAwRole::Solid, -1160, 1060, 0, -960, 1120, 70);
    AwAddPart(L, TEXT("safe.table.top"), AwAssetIds::StationCuboid, EAwColour::Paper, EAwRole::Visual, -1164, 1056, 70, -956, 1124, 76);
    AwAddText(L, TEXT("sign.safe_area"),
              TEXT("SAFE AREA\nMs Basu is in charge here.\nYou may rest here at any time.\nWithdrawing is a complete choice."), -1050, 1335, 150, 8.0f);
}

void AwFlBuildArrival(FAwFloorLayout& L)
{
    L.Arrival = AwWorld(L, 0.0, 260.0, 0.0);
    L.ArrivalYaw = 90.0;
    AwAddPart(L, TEXT("arrival.plate"), AwAssetIds::ResetPad, EAwColour::GateTeal, EAwRole::Visual, -80, 180, 0, 80, 340, 2.5);
    AwAddText(L, TEXT("sign.arrival"), TEXT("VIRTUAL FLOOR\nSupervised practice. Nothing here can hurt you.\nMs Basu is with the class."), 0, 560, 175, 9.0f);

    FAwPadSpec& Pad = L.LeavePad;
    Pad.Id = FName(TEXT("pad.return"));
    Pad.Kind = EAwPadKind::Leave;
    Pad.Stance = EAwStance::Withdraw;
    Pad.Shape = EAwShape::Ring;
    Pad.Label = TEXT("RETURN");
    Pad.Centre = AwWorld(L, 350.0, 260.0, 0.0);
    Pad.Point = Pad.Centre + FVector(0.0, 0.0, AwNumbers::PadPointZ);
    AwAddPadVisual(L, Pad, EAwColour::PadViolet, EAwColour::Paper);
    AwAddText(L, TEXT("label.return"), Pad.Label, 350, 260, 70, 6.0f);
}

void AwFlBuildLights(FAwFloorLayout& L)
{
    const double Spots[][2] = {{300, 400}, {300, 1300}, {300, 2300}, {300, 3200}, {1000, 1100}, {1000, 2100}, {1000, 3050}};
    for (const double (&Spot)[2] : Spots)
    {
        FAwLight Light;
        Light.Position = AwWorld(L, Spot[0], Spot[1], 440.0);
        L.Lights.Add(Light);
    }
    for (const double Y : {700.0, 1300.0})
    {
        FAwLight Safe;
        Safe.Position = AwWorld(L, -1000.0, Y, 400.0);
        Safe.Radius = 700.0f;
        Safe.Lumens = 2000.0f;
        Safe.Colour = FLinearColor(1.0f, 0.90f, 0.72f);
        L.Lights.Add(Safe);
    }
}
} // namespace

void AwBuildShell(FAwFloorLayout& L)
{
    AwFlBuildRoom(L);
    AwFlBuildSafeArea(L);
    AwFlBuildArrival(L);
    AwFlBuildLights(L);
}

void AwBuildTerminal(FAwFloorLayout& L)
{
    const FVector Stand = L.Site.EntryStand;
    const FVector Kiosk = L.Site.EntryKiosk; // the front face, at floor level
    const double X = Kiosk.X;
    const double Y = Kiosk.Y;
    const double G = Kiosk.Z;
    AwAddWorldPart(L, TEXT("terminal.body"), AwAssetIds::EntryKiosk, EAwColour::Ink, EAwRole::Visual, FVector(X - 70, Y, G), FVector(X + 70, Y + 50, G + 110));
    AwAddWorldPart(L, TEXT("terminal.collision"), AwAssetIds::EntryKiosk, EAwColour::Ink, EAwRole::Prop, FVector(X - 72, Y - 2, G), FVector(X + 72, Y + 52, G + 114));
    AwAddWorldPart(L, TEXT("terminal.frame.top"), AwAssetIds::EntryKiosk, EAwColour::GateTeal, EAwRole::Visual, FVector(X - 78, Y - 2, G + 110), FVector(X + 78, Y + 54, G + 122));
    AwAddWorldPart(L, TEXT("terminal.frame.west"), AwAssetIds::EntryKiosk, EAwColour::GateTeal, EAwRole::Visual, FVector(X - 78, Y - 2, G), FVector(X - 70, Y + 54, G + 110));
    AwAddWorldPart(L, TEXT("terminal.frame.east"), AwAssetIds::EntryKiosk, EAwColour::GateTeal, EAwRole::Visual, FVector(X + 70, Y - 2, G), FVector(X + 78, Y + 54, G + 110));
    AwAddWorldPart(L, TEXT("terminal.screen"), AwAssetIds::EntryKiosk, EAwColour::GateViolet, EAwRole::Visual, FVector(X - 55, Y - 2, G + 50), FVector(X + 55, Y, G + 102));
    AwAddWorldPart(L, TEXT("terminal.plate"), AwAssetIds::EntryKiosk, EAwColour::GateTeal, EAwRole::Visual, FVector(X - 50, Stand.Y - 50, G), FVector(X + 50, Stand.Y + 50, G + 1.5));
    FAwText Sign;
    Sign.Id = FName(TEXT("sign.terminal"));
    Sign.Text = TEXT("VIRTUAL FLOOR\nsupervised practice");
    Sign.Location = FVector(X, Y - 3.5, G + 76);
    Sign.SizeCm = 7.0f;
    L.Texts.Add(Sign);

    FAwPadSpec& Pad = L.EntryPad;
    Pad.Id = FName(TEXT("pad.entry"));
    Pad.Kind = EAwPadKind::Enter;
    Pad.Stance = EAwStance::Help;
    Pad.Shape = EAwShape::Arrow;
    Pad.Label = TEXT("ENTER");
    Pad.Centre = Kiosk;
    Pad.Point = FVector(X, Y - 2.0, G + 85.0);
}
} // namespace PWAwakening
