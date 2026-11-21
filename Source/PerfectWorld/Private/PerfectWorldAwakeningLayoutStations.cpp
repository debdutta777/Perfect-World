#include "PerfectWorldAwakeningLayout.h"

// The three stations: console, six pads on an arc round the stand point, the reset point, the hazard marking and the moving parts.

namespace PWAwakening
{
namespace
{
FAwPose AwFlPose(const FAwFloorLayout& L, const double X, const double Y, const double Z, const EAwColour Colour)
{
    FAwPose Pose;
    Pose.Centre = AwWorld(L, X, Y, Z);
    Pose.Colour = Colour;
    return Pose;
}

FString AwFlLastSegment(const FName Id)
{
    FString Text = Id.ToString();
    int32 Dot = INDEX_NONE;
    if (Text.FindLastChar(TEXT('.'), Dot))
    {
        Text = Text.Mid(Dot + 1);
    }
    return Text;
}

/** Stripes across a rectangle (yellow bars on a dark surface): a hazard is shape and words, not colour alone. */
void AwFlHazardSurface(FAwFloorLayout& L, const FString& Slug, const double X0, const double Y0, const double X1, const double Y1)
{
    AwAddPart(L, *(Slug + TEXT(".hazard.surface")), AwAssetIds::HazardStripe, EAwColour::HazardDark, EAwRole::Visual, X0, Y0, 0, X1, Y1, 2.0);
    int32 K = 0;
    for (double Y = Y0 + 10.0; Y + 20.0 <= Y1; Y += 80.0)
    {
        AwAddPart(L, *FString::Printf(TEXT("%s.hazard.stripe%d"), *Slug, K++), AwAssetIds::HazardStripe, EAwColour::HazardYellow, EAwRole::Visual, X0, Y, 2.0, X1,
                  Y + 20.0, 3.0);
    }
}

/** A row of posts and a rail along X = Edge from Y0 to Y1 (soft light barrier: no collision). */
void AwFlBarrierAlongY(FAwFloorLayout& L, const FString& Id, const double Edge, const double Y0, const double Y1)
{
    int32 K = 0;
    for (double Y = Y0; Y <= Y1; Y += 100.0)
    {
        AwAddPart(L, *FString::Printf(TEXT("%s.post%d"), *Id, K++), AwAssetIds::LightBarrier, EAwColour::GateViolet, EAwRole::Visual, Edge - 3, Y - 3, 0,
                  Edge + 3, Y + 3, 90);
    }
    AwAddPart(L, *(Id + TEXT(".rail")), AwAssetIds::LightBarrier, EAwColour::GateViolet, EAwRole::Visual, Edge - 2, Y0, 84, Edge + 2, Y1, 90);
}

void AwFlBuildPads(FAwFloorLayout& L, FAwStationSpec& S, const FAwSituationDef& Def, const double Yb)
{
    const FVector Stand = AwWorld(L, AwNumbers::StandX, Yb + AwNumbers::StandBack, 0.0);
    for (int32 K = 0; K < AwNumbers::PadsPerStation; ++K)
    {
        const double Angle = FMath::DegreesToRadians(AwNumbers::PadAngles[K]);
        FAwPadSpec Pad;
        Pad.StationIndex = S.Index;
        Pad.Situation = S.Situation;
        FString Key;
        EAwColour Plate = EAwColour::PadTeal;
        EAwColour Shape = EAwColour::Paper;
        if (K < 3)
        {
            const FAwChoiceDef& Choice = Def.Choices[K];
            Pad.Kind = EAwPadKind::Choice;
            Pad.Choice = Choice.Id;
            Pad.Stance = Choice.Stance;
            Pad.Shape = AwStanceShape(Choice.Stance);
            Pad.Label = Choice.Label;
            Key = AwFlLastSegment(Choice.Id);
        }
        else if (K == 3)
        {
            const FAwChoiceDef& Withdraw = AwWithdrawDef();
            Pad.Kind = EAwPadKind::Withdraw;
            Pad.Choice = Withdraw.Id;
            Pad.Stance = EAwStance::Withdraw;
            Pad.Shape = EAwShape::Back;
            Pad.Label = Withdraw.Label;
            Key = TEXT("withdraw");
            Plate = EAwColour::SafeGreen;
        }
        else if (K == 4)
        {
            Pad.Kind = EAwPadKind::Advance;
            Pad.Stance = EAwStance::Observe;
            Pad.Shape = EAwShape::Dots;
            Pad.Label = TEXT("ADVANCE");
            Key = TEXT("advance");
            Plate = EAwColour::PadViolet;
        }
        else
        {
            Pad.Kind = EAwPadKind::Reset;
            Pad.Stance = EAwStance::Wait;
            Pad.Shape = EAwShape::Ring;
            Pad.Label = TEXT("RESET");
            Key = TEXT("reset");
            Plate = EAwColour::Ink;
            Shape = EAwColour::GateTeal;
        }
        Pad.Id = FName(*FString::Printf(TEXT("pad.s%d.%s"), S.Index + 1, *Key));
        Pad.Centre = Stand + FVector(AwNumbers::PadRadiusCm * FMath::Sin(Angle), AwNumbers::PadRadiusCm * FMath::Cos(Angle), 0.0);
        Pad.Point = Pad.Centre + FVector(0.0, 0.0, AwNumbers::PadPointZ);
        AwAddPadVisual(L, Pad, Plate, Shape);
        FAwText Label;
        Label.Id = FName(*FString::Printf(TEXT("label.s%d.%s"), S.Index + 1, *Key));
        Label.Text = Pad.Label;
        Label.Location = Pad.Centre + FVector(0.0, 0.0, 70.0);
        Label.SizeCm = 6.0f;
        L.Texts.Add(Label);
        S.Pads.Add(Pad);
    }
}

void AwFlBuildConsole(FAwFloorLayout& L, FAwStationSpec& S, const FAwSituationDef& Def, const double Yb)
{
    const FString Slug = FString::Printf(TEXT("s%d"), S.Index + 1);
    AwAddPart(L, *(Slug + TEXT(".console.body")), AwAssetIds::StationCuboid, EAwColour::PadViolet, EAwRole::Visual, 1150, Yb + 700, 0, 1390, Yb + 760, 120);
    AwAddPart(L, *(Slug + TEXT(".console.collision")), AwAssetIds::StationCuboid, EAwColour::PadViolet, EAwRole::Prop, 1148, Yb + 698, 0, 1392, Yb + 762, 122);
    AwAddPart(L, *(Slug + TEXT(".console.top")), AwAssetIds::StationCuboid, EAwColour::GateTeal, EAwRole::Visual, 1146, Yb + 696, 120, 1394, Yb + 764, 126);
    S.Console = AwWorld(L, 1270.0, Yb + 696.0, 120.0);
    AwAddText(L, *(Slug + TEXT(".title")), Def.Title, 1270, Yb + 692.5, 100, 8.0f);
    AwAddText(L, *(Slug + TEXT(".summary")), Def.Summary, 1270, Yb + 692.5, 62, 4.6f);
    S.StatusAt = AwWorld(L, 1270.0, Yb + 692.5, 26.0);
}

void AwFlBuildResetPoint(FAwFloorLayout& L, FAwStationSpec& S, const double Yb)
{
    const FString Slug = FString::Printf(TEXT("s%d"), S.Index + 1);
    S.Stand = AwWorld(L, AwNumbers::StandX, Yb + AwNumbers::StandBack, 0.0);
    S.ResetPad = S.Stand;
    AwAddPart(L, *(Slug + TEXT(".reset_plate")), AwAssetIds::ResetPad, EAwColour::PadTeal, EAwRole::Visual, AwNumbers::StandX - 50, Yb + AwNumbers::StandBack - 50, 0,
              AwNumbers::StandX + 50, Yb + AwNumbers::StandBack + 50, 2.5);
    AwAddPart(L, *(Slug + TEXT(".bay_edge")), AwAssetIds::FloorGrid, S.Index == 1 ? EAwColour::GateViolet : EAwColour::GateTeal, EAwRole::Visual, -300, Yb - 10, 0,
              1400, Yb, 1.8);
    AwAddText(L, *(Slug + TEXT(".reset_label")), TEXT("RESET POINT"), AwNumbers::StandX, Yb + 30, 12, 5.0f);
}

void AwFlBuildCrossing(FAwFloorLayout& L, FAwStationSpec& S, const double Yb)
{
    AwFlHazardSurface(L, TEXT("s1"), 700, Yb + 60, 940, Yb + 700);
    AwFlBarrierAlongY(L, TEXT("s1.barrier.west"), 695, Yb + 60, Yb + 700);
    AwFlBarrierAlongY(L, TEXT("s1.barrier.east"), 945, Yb + 60, Yb + 700);
    AwAddText(L, TEXT("s1.sign.gap"), TEXT("GAP\nCross by the platform only"), 820, Yb + 30, 130, 8.0f);
    S.Hazard = FBox(AwWorld(L, 700.0, Yb + 60.0, -10.0), AwWorld(L, 940.0, Yb + 700.0, 8.0));
    S.SafeWhenAligned = FBox(AwWorld(L, 700.0, Yb + 285.0, -10.0), AwWorld(L, 940.0, Yb + 435.0, 40.0));

    S.Primary.Id = FName(TEXT("mover.s1.primary"));
    S.Primary.AssetId = FName(AwAssetIds::PlatformCycling);
    S.Primary.Size = FVector(240.0, 150.0, 12.0);
    S.Primary.Poses = {AwFlPose(L, 820, Yb + 360, 6, EAwColour::GateTeal), AwFlPose(L, 820, Yb + 620, 6, EAwColour::GateTeal)};
    S.Primary.StartPose = 1; // stalled away from the crossing
    S.Primary.bSolid = true;

    S.Secondary.Id = FName(TEXT("mover.s1.secondary"));
    S.Secondary.AssetId = FName(AwAssetIds::LightBarrier);
    S.Secondary.Size = FVector(20.0, 20.0, 110.0);
    S.Secondary.Poses = {AwFlPose(L, 620, Yb + 360, -60, EAwColour::GateViolet), AwFlPose(L, 620, Yb + 360, 55, EAwColour::GateViolet)};
    S.Secondary.bSolid = false; // route cue lights: they rise out of the floor

    S.FigureFeet = AwWorld(L, 1060.0, Yb + 360.0, 0.0);
}

void AwFlBuildCorridor(FAwFloorLayout& L, FAwStationSpec& S, const double Yb)
{
    AwAddPart(L, TEXT("s2.wall.north"), AwAssetIds::WallPanel, EAwColour::PadViolet, EAwRole::Solid, 650, Yb + 520, 0, 1250, Yb + 550, 220);
    AwAddPart(L, TEXT("s2.wall.south"), AwAssetIds::WallPanel, EAwColour::PadViolet, EAwRole::Solid, 650, Yb + 250, 0, 1250, Yb + 280, 220);
    AwAddPart(L, TEXT("s2.corridor.floor"), AwAssetIds::FloorGrid, EAwColour::PadViolet, EAwRole::Visual, 650, Yb + 280, 0, 1250, Yb + 520, 1.8);
    AwFlHazardSurface(L, TEXT("s2"), 1205, Yb + 10, 1265, Yb + 250);
    AwAddText(L, TEXT("s2.sign.wall"), TEXT("MOVING WALL\nIt shifts when announced."), 1235, Yb + 6, 130, 7.0f);
    S.Hazard = FBox(AwWorld(L, 1205.0, Yb + 10.0, -10.0), AwWorld(L, 1265.0, Yb + 250.0, 60.0));

    S.Primary.Id = FName(TEXT("mover.s2.primary"));
    S.Primary.AssetId = FName(AwAssetIds::WallShifting);
    S.Primary.Size = FVector(30.0, 240.0, 220.0);
    S.Primary.Poses = {AwFlPose(L, 1235, Yb + 400, 110, EAwColour::GateViolet), AwFlPose(L, 1235, Yb + 130, 110, EAwColour::GateViolet)};
    S.Primary.StartPose = 0;
    S.Primary.bAutoCycle = true;
    S.Primary.bSolid = true;

    S.Secondary.Id = FName(TEXT("mover.s2.secondary"));
    S.Secondary.AssetId = FName(AwAssetIds::CorridorBarrier);
    S.Secondary.Size = FVector(20.0, 140.0, 140.0);
    S.Secondary.Poses = {AwFlPose(L, 800, Yb + 650, 70, EAwColour::GateTeal), AwFlPose(L, 900, Yb + 400, 70, EAwColour::GateTeal),
                         AwFlPose(L, 1205, Yb + 400, 70, EAwColour::GateTeal)};
    S.Secondary.bSolid = true;

    S.FigureFeet = AwWorld(L, 1000.0, Yb + 400.0, 0.0);
}

void AwFlBuildDelivery(FAwFloorLayout& L, FAwStationSpec& S, const double Yb)
{
    AwAddPart(L, TEXT("s3.desk.body"), AwAssetIds::StationCuboid, EAwColour::Ink, EAwRole::Solid, 1090, Yb + 380, 0, 1210, Yb + 460, 80);
    AwAddPart(L, TEXT("s3.desk.top"), AwAssetIds::StationCuboid, EAwColour::Paper, EAwRole::Visual, 1086, Yb + 376, 80, 1214, Yb + 464, 86);
    AwAddText(L, TEXT("s3.sign.desk"), TEXT("HANDOVER DESK"), 1150, Yb + 372, 125, 7.0f);
    AwFlHazardSurface(L, TEXT("s3"), 860, Yb + 560, 940, Yb + 680);
    AwAddText(L, TEXT("s3.sign.marker"), TEXT("FAULTY MARKER\nIts light flickers. It is safe."), 900, Yb + 552, 150, 6.0f);
    // No hazard zone: the marker only flickers.

    S.Primary.Id = FName(TEXT("mover.s3.primary"));
    S.Primary.AssetId = FName(AwAssetIds::DeliveryCrates);
    S.Primary.Size = FVector(200.0, 80.0, 70.0);
    S.Primary.Poses = {AwFlPose(L, 800, Yb + 250, 35, EAwColour::HazardYellow), AwFlPose(L, 985, Yb + 420, 35, EAwColour::HazardYellow)};
    S.Primary.StartPose = 0;
    S.Primary.bSolid = true;

    S.Secondary.Id = FName(TEXT("mover.s3.secondary"));
    S.Secondary.AssetId = FName(AwAssetIds::DeliveryMarker);
    S.Secondary.Size = FVector(24.0, 24.0, 140.0);
    S.Secondary.Poses = {AwFlPose(L, 900, Yb + 620, 70, EAwColour::HazardYellow), AwFlPose(L, 900, Yb + 620, 70, EAwColour::GateTeal),
                         AwFlPose(L, 900, Yb + 620, 70, EAwColour::GateViolet)};
    S.Secondary.bFlicker = true;
    S.Secondary.bSolid = false;

    S.FigureFeet = AwWorld(L, 720.0, Yb + 420.0, 0.0);
}
} // namespace

void AwBuildStations(FAwFloorLayout& L)
{
    const TArray<FAwSituationDef>& Catalogue = AwCatalogue();
    for (int32 I = 0; I < Catalogue.Num(); ++I)
    {
        const FAwSituationDef& Def = Catalogue[I];
        const double Yb = AwNumbers::BayBaseY + AwNumbers::BayStride * I;
        FAwStationSpec S;
        S.Index = I;
        S.Situation = Def.Id;
        S.FigurePerson = Def.FigurePerson;
        S.FigureYaw = I == 2 ? 0.0 : 180.0;
        AwFlBuildResetPoint(L, S, Yb);
        AwFlBuildPads(L, S, Def, Yb);
        AwFlBuildConsole(L, S, Def, Yb);
        if (I == 0)
        {
            AwFlBuildCrossing(L, S, Yb);
        }
        else if (I == 1)
        {
            AwFlBuildCorridor(L, S, Yb);
        }
        else
        {
            AwFlBuildDelivery(L, S, Yb);
        }
        L.Stations.Add(MoveTemp(S));
    }
}
} // namespace PWAwakening
