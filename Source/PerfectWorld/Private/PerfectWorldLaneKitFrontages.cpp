#include "PerfectWorldLaneKitFrontages.h"

namespace PWLane
{
namespace
{
// File-local names start with Kf (kit frontages): the module builds in unity blobs.
constexpr double KfPlinthTop = 75.0;       // the decorative plinth fascia, like the generic wall volume
constexpr double KfCanopyBottomZ = 236.0;  // 221 above the footpath: well over the 210 door clear height
constexpr double KfCanopyThickness = 6.0;
constexpr double KfCanopyReach = 38.0;
constexpr double KfPositionTolerance = 0.25; // whole centimetres only: the generator's cursor is a whole number

bool KfNearlyWhole(double Value, double Tolerance)
{
    return FMath::Abs(Value - FMath::RoundToDouble(Value)) <= Tolerance;
}

/** The street face: a plinth either side of the doorway, a threshold plate, a canopy over the door and a lamp. All of it stands outside the wall line. */
void KfBuildFront(FLaneScene& S, const FLaneBuilding& B, const LI::Opening& Entrance)
{
    FLaneBlocks& K = S.Blocks;
    const FLaneFrame& F = B.Frame;
    const double DoorU = 0.5 * (Entrance.Plan.U0 + Entrance.Plan.U1);
    const double Cut = LI::DoorOpeningWidthCm * 0.5 + 5.0; // the door trim stands 5 cm beyond the opening
    F.Add(K, ELaneColour::Plinth, 0.0, DoorU - Cut, -2.0, 0.0, FootpathHeight, KfPlinthTop);
    F.Add(K, ELaneColour::Plinth, DoorU + Cut, B.Width, -2.0, 0.0, FootpathHeight, KfPlinthTop);

    // The threshold plate is flush with the footpath (a 5 mm lip), as at the generic house door.
    F.Add(K, ELaneColour::Concrete, DoorU - 62.0, DoorU + 62.0, -42.0, 0.0, 0.0, FootpathHeight + 0.5);

    // A shallow canopy (chhajja) and a lamp over the door; the canopy's underside is well above the 210 cm clear height.
    const double CanopyHalf = Cut + 22.0;
    F.Add(K, ELaneColour::TrimLight, DoorU - CanopyHalf, DoorU + CanopyHalf, -KfCanopyReach, 0.0, KfCanopyBottomZ, KfCanopyBottomZ + KfCanopyThickness);
    F.Add(K, ELaneColour::GlassLit, DoorU - 8.0, DoorU + 8.0, -12.0, -2.0, KfCanopyBottomZ - 10.0, KfCanopyBottomZ); // hangs under the canopy
}
} // namespace

const LI::Opening* KitFrontEntrance(const LI::InteriorLayout& L)
{
    for (const LI::Opening& O : L.Openings)
    {
        if (O.RoomA == "outside" && O.Kind != LI::OpeningKind::Window) // front windows face outside too
        {
            return &O;
        }
    }
    return nullptr;
}

bool IsKitFrontage(const FLaneBuilding& B, FLaneKitFrontage& Out)
{
    if (B.Detail != ELaneDetail::Full || (B.Side != -1 && B.Side != 1) || B.Storeys < 2)
    {
        return false;
    }
    // The frame must be the generator's: U along +X, V along the side's Y, origin on the side's frontage line.
    const double Side = static_cast<double>(B.Side);
    if (!B.Frame.U.Equals(FVector(1.0, 0.0, 0.0), 0.001) || !B.Frame.V.Equals(FVector(0.0, Side, 0.0), 0.001) ||
        B.Frame.Origin.Y * Side <= 0.0 || !FMath::IsNearlyEqual(B.StoreyHeightCm, StoreyHeight, 0.5))
    {
        return false;
    }
    if (!KfNearlyWhole(B.Width, 0.01) || !KfNearlyWhole(B.Depth, 0.01) || !KfNearlyWhole(B.Setback, 0.01) ||
        !KfNearlyWhole(B.Frame.Origin.X, KfPositionTolerance) || !KfNearlyWhole(B.Frame.Origin.Y, KfPositionTolerance))
    {
        return false;
    }
    const LI::FrontageEntry* Row = LI::FindFrontageByFootprint(
        B.Side, FMath::RoundToInt(B.Frame.Origin.X), FMath::RoundToInt(FMath::Abs(B.Frame.Origin.Y)), FMath::RoundToInt(B.Width),
        FMath::RoundToInt(B.Depth), FMath::RoundToInt(B.Setback), B.Storeys, B.Seed);
    if (Row == nullptr)
    {
        return false;
    }
    Out.Entry = Row;
    Out.BuildingId = FName(UTF8_TO_TCHAR(std::string(Row->BuildingId).c_str()));
    Out.PlannedDoorId = Row->PlannedDoorId.empty() ? FName() : FName(UTF8_TO_TCHAR(std::string(Row->PlannedDoorId).c_str()));
    Out.DoorId = Row->EntranceDoorId.empty() ? FName() : FName(UTF8_TO_TCHAR(std::string(Row->EntranceDoorId).c_str()));
    Out.Kind = Row->Kind;
    Out.Furniture = Row->Furniture;
    Out.EntranceCentreU = Row->EntranceCentreU;
    Out.bMirrorU = Row->MirrorU;
    return true;
}

LI::InteriorLayout MakeKitFrontageLayout(const FLaneBuilding& B, const FLaneKitFrontage& Frontage)
{
    if (Frontage.Entry == nullptr)
    {
        LI::InteriorLayout NoRow;
        NoRow.Status = LI::Refusal::InvalidBuildingId; // no catalogue row: refused, never an empty "ok" layout
        return NoRow;
    }
    return LI::GenerateInteriorLayout(LI::MakeFrontageInput(*Frontage.Entry, FMath::RoundToInt(GroundFloorZ), FMath::RoundToInt(B.StoreyHeightCm)));
}

void BuildKitFrontage(FLaneScene& S, const FLaneBuilding& B, const FLaneKitFrontage& Frontage)
{
    const LI::InteriorLayout Layout = MakeKitFrontageLayout(B, Frontage);
    const LI::Opening* Entrance = Layout.Ok() ? KitFrontEntrance(Layout) : nullptr;
    if (Entrance == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("Kit frontage %s refused (%hs); the building stays a solid block."), *Frontage.BuildingId.ToString(),
               std::string(LI::RefusalCode(Layout.Status)).c_str());
        B.Frame.Add(S.Blocks, B.Wall, 0.0, B.Width, 0.0, B.Depth, 0.0, B.RoofZ());
        return;
    }
    BuildInteriorKit(S, B, Layout);
    KfBuildFront(S, B, *Entrance);
}
} // namespace PWLane
