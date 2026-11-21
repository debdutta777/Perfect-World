#include "PerfectWorldLaneEndInteriors.h"

#include "Engine/World.h"
#include "PerfectWorldShopDisplayActor.h"

namespace PWLane
{
namespace
{
// File-local names start with Lei: the module builds in unity blobs.
constexpr double LeiPlinthTop = 75.0;
constexpr double LeiTrim = 5.0;          // the opening's trim strip (IkDressOpening)
constexpr double LeiBoxOverhang = 14.0;  // the shutter box is this much wider than the archway each side
constexpr double LeiBoxGap = 2.0;        // above the opening's head trim
constexpr double LeiBoxHeight = 28.0;
constexpr double LeiBoxProjection = 22.0;
constexpr double LeiTextSizeCm = 7.0;
constexpr double LeiLampBelowTop = 62.0; // lamps hang beside the arch, this far under its head

const LI::Opening* LeiEntrance(const LI::InteriorLayout& Layout)
{
    return Layout.Ok() ? LE::FindEndEntrance(Layout) : nullptr;
}

/** Absolute Z (above GroundZ) of the top of the archway's opening. */
double LeiArchTop(const LE::HallInfo& Info)
{
    return GroundFloorZ + static_cast<double>(Info.EntranceHeightCm);
}

/** The street face: plinths either side of the archway, the roller-shutter box above it (shutters raised) and a lamp each side. */
void LeiBuildFront(FLaneScene& S, const FLaneBuilding& B, const LE::HallInfo& Info, const LI::Opening& Entrance)
{
    FLaneBlocks& K = S.Blocks;
    const FLaneFrame& F = B.Frame;
    const double U0 = Entrance.Plan.U0;
    const double U1 = Entrance.Plan.U1;
    F.Add(K, ELaneColour::Plinth, 0.0, U0 - LeiTrim, -2.0, 0.0, FootpathHeight, LeiPlinthTop);
    F.Add(K, ELaneColour::Plinth, U1 + LeiTrim, B.Width, -2.0, 0.0, FootpathHeight, LeiPlinthTop);

    const double Top = LeiArchTop(Info) + LeiTrim + LeiBoxGap; // above the head trim
    F.Add(K, ELaneColour::ShutterBlue, U0 - LeiBoxOverhang, U1 + LeiBoxOverhang, -LeiBoxProjection, 0.0, Top, Top + LeiBoxHeight);
    F.Add(K, ELaneColour::TrimLight, U0 - LeiBoxOverhang - 2.0, U1 + LeiBoxOverhang + 2.0, -LeiBoxProjection - 2.0, 0.0, Top + LeiBoxHeight,
          Top + LeiBoxHeight + 3.0);
    for (const double LampU : {U0 - 28.0, U1 + 28.0})
    {
        F.Add(K, ELaneColour::GlassLit, LampU - 6.0, LampU + 6.0, -10.0, -2.0, LeiArchTop(Info) - LeiLampBelowTop,
              LeiArchTop(Info) - LeiLampBelowTop + 20.0);
    }
}
} // namespace

FLaneBuilding MakeLaneEndBuilding(const LE::EndHall Hall)
{
    const LE::HallInfo& Info = LE::GetHallInfo(Hall);
    FLaneBuilding B;
    B.Width = LE::HallWidthCm;
    B.Depth = LE::HallDepthCm;
    B.Storeys = Info.Storeys;
    B.Index = Info.East ? 1 : 0; // as BuildClosingRows: the far (east) end is index 1
    B.Seed = LaneHash(900 + (Info.East ? 0 : 1), 3, 3);
    B.Wall = Info.East ? ELaneColour::WallTerracotta : ELaneColour::WallCream;
    B.Detail = ELaneDetail::Full;
    B.Frame = MakeLaneFrame(static_cast<double>(Info.LaneFaceXCm), static_cast<double>(LE::OriginYCm), FVector(0.0, 1.0, 0.0),
                            FVector(static_cast<double>(Info.FaceXSign), 0.0, 0.0));
    return B;
}

bool IsLaneEndHall(const FLaneBuilding& B, LE::EndHall& Out)
{
    if (B.Detail != ELaneDetail::Full || B.Side != 0)
    {
        return false;
    }
    for (const LE::EndHall Hall : {LE::EndHall::CommunityHall, LE::EndHall::MarketHall})
    {
        const LE::HallInfo& Info = LE::GetHallInfo(Hall);
        if (B.Storeys == Info.Storeys && FMath::IsNearlyEqual(B.Width, static_cast<double>(LE::HallWidthCm), 0.01) &&
            FMath::IsNearlyEqual(B.Depth, static_cast<double>(LE::HallDepthCm), 0.01) && FMath::IsNearlyEqual(B.Setback, 0.0, 0.01) &&
            FMath::IsNearlyEqual(B.StoreyHeightCm, StoreyHeight, 0.5) &&
            B.Frame.Origin.Equals(FVector(static_cast<double>(Info.LaneFaceXCm), static_cast<double>(LE::OriginYCm), GroundZ), 0.01) &&
            B.Frame.U.Equals(FVector(0.0, 1.0, 0.0), 0.001) && B.Frame.V.Equals(FVector(static_cast<double>(Info.FaceXSign), 0.0, 0.0), 0.001))
        {
            Out = Hall;
            return true;
        }
    }
    return false;
}

void BuildLaneEndHall(FLaneScene& S, const FLaneBuilding& B, const LE::EndHall Hall)
{
    const LE::HallInfo& Info = LE::GetHallInfo(Hall);
    const LI::InteriorLayout Layout = LE::GenerateEndLayout(Hall);
    const LI::Opening* Entrance = LeiEntrance(Layout);
    if (Entrance == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("Lane end hall %hs refused (%hs); the mass stays a solid block."), std::string(Info.BuildingId).c_str(),
               std::string(LI::RefusalCode(Layout.Status)).c_str());
        B.Frame.Add(S.Blocks, B.Wall, 0.0, B.Width, 0.0, B.Depth, 0.0, B.RoofZ());
        return;
    }
    BuildInteriorKit(S, B, Layout);
    LeiBuildFront(S, B, Info, *Entrance);
}

FString LaneEndSignText(const LE::EndHall Hall)
{
    const LE::HallInfo& Info = LE::GetHallInfo(Hall);
    return FString(UTF8_TO_TCHAR(std::string(Info.Label).c_str())) + TEXT(" hours\n") + FString(UTF8_TO_TCHAR(LE::PostedHoursText(Hall).c_str()));
}

FVector LaneEndSignLocation(const LE::EndHall Hall)
{
    const LE::HallInfo& Info = LE::GetHallInfo(Hall);
    const double Top = LeiArchTop(Info) + LeiTrim + LeiBoxGap;
    return MakeLaneEndBuilding(Hall).Frame.Pt(Info.EntranceCentreU, -LeiBoxProjection - 1.0, Top + 0.5 * LeiBoxHeight);
}

double LaneEndSignYaw(const LE::EndHall Hall)
{
    const FVector Out = -MakeLaneEndBuilding(Hall).Frame.V; // the text's front is +X; it faces the lane, toward -V
    return FMath::RadiansToDegrees(FMath::Atan2(Out.Y, Out.X));
}

int32 SpawnLaneEndSigns(UWorld& World)
{
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    int32 Spawned = 0;
    for (const LE::EndHall Hall : {LE::EndHall::CommunityHall, LE::EndHall::MarketHall})
    {
        if (!LE::GenerateEndLayout(Hall).Ok())
        {
            continue;
        }
        const FVector Location = LaneEndSignLocation(Hall);
        APWShopHoursSignActor* Sign = World.SpawnActor<APWShopHoursSignActor>(Location, FRotator::ZeroRotator, Params);
        if (Sign != nullptr)
        {
            Sign->Configure(FName(UTF8_TO_TCHAR(std::string(LE::GetHallInfo(Hall).BuildingId).c_str())), LaneEndSignText(Hall), Location, LaneEndSignYaw(Hall),
                            static_cast<float>(LeiTextSizeCm));
            ++Spawned;
        }
    }
    return Spawned;
}
} // namespace PWLane
