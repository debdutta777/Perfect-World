#include "PerfectWorldLaneTallFlats.h"

namespace PWLane
{
namespace
{
using T6Rect = LI::Rect;

void T6Box(FLaneScene& S, const FLaneBuilding& B, ELaneColour Colour, const T6Rect& R, double Z0, double Z1, bool Solid = true)
{
    B.Frame.Add(S.Blocks, Colour, R.U0, R.U1, R.V0, R.V1, Z0, Z1);
    if (Solid) S.Blocks.PropCollision(B.Frame.Pt(R.U0, R.V0, Z0), B.Frame.Pt(R.U1, R.V1, Z1));
}

// Emit floor-relative kit parts without BuildInteriorKit's solid infill above the ground ceiling.
void T6Interior(FLaneScene& S, const FLaneBuilding& B, const LI::InteriorLayout& L, bool Ground = false)
{
    const double Floor = L.FinishedFloorCm;
    for (const auto& W : L.Walls)
        T6Box(S, B, W.WallId.find("partition") != std::string::npos ? ELaneColour::PlasterPale : B.Wall,
              W.Plan, Floor + W.W0, Floor + W.W1);
    for (const auto& P : L.Pieces)
        T6Box(S, B, KitColour(P.Palette), P.Footprint, Floor + P.H0, Floor + P.H1, P.Solid);
    for (const auto& O : L.Openings)
    {
        const bool AlongU = O.Plan.Width() > O.Plan.Depth();
        const int A0 = AlongU ? O.Plan.U0 : O.Plan.V0;
        const int A1 = AlongU ? O.Plan.U1 : O.Plan.V1;
        const int Cross0 = AlongU ? O.Plan.V0 : O.Plan.U0;
        const int Cross1 = AlongU ? O.Plan.V1 : O.Plan.U1;
        const auto Put = [&](int Lo, int Hi, double Z0, double Z1)
        {
            const T6Rect R = AlongU ? T6Rect{Lo, Hi, Cross0, Cross1} : T6Rect{Cross0, Cross1, Lo, Hi};
            T6Box(S, B, ELaneColour::TrimDark, R, Z0, Z1, false);
        };
        Put(A0 - 4, A0, Floor + O.W0, Floor + O.W1);
        Put(A1, A1 + 4, Floor + O.W0, Floor + O.W1);
        Put(A0 - 4, A1 + 4, Floor + O.W1, Floor + O.W1 + 4);
        if (O.Kind == LI::OpeningKind::Window)
            T6Box(S, B, ELaneColour::GlassDusk, O.Plan, Floor + O.W0, Floor + O.W1);
        if (!O.DoorId.empty()) S.Doors.Add(KitDoorSpec(B, L, O));
    }
    for (const auto& Light : L.Lights)
    {
        // The passage ceiling is now open to the shaft. Its old suspended fitting would enter headroom;
        // the shaft receives a light at the upper ceiling instead. Ground unit fittings stay intact.
        if (Ground && Light.U >= 485 && Light.U <= 615) continue;
        const double Z = Floor + L.ClearHeightCm;
        T6Box(S, B, ELaneColour::TrimLight, {Light.U - 11, Light.U + 11, Light.V - 11, Light.V + 11}, Z - 4, Z, false);
        T6Box(S, B, ELaneColour::GlassLit, {Light.U - 8, Light.U + 8, Light.V - 8, Light.V + 8}, Z - 8, Z - 4, false);
        FLaneRoomLight Spec;
        Spec.Position = KitPoint(B, L, Light.U, Light.V, Light.HeightCm);
        Spec.Radius = static_cast<float>(Light.RadiusCm);
        Spec.Lumens = static_cast<float>(Light.Lumens);
        S.RoomLights.Add(Spec);
    }
}
} // namespace

bool IsTallFlatsUpstairs(const FLaneBuilding& B)
{
    FLaneKitFrontage Frontage;
    return IsKitFrontage(B, Frontage) && Frontage.Entry != nullptr && Frontage.Entry->BuildingId == "building.nabapara.tall_flats";
}

void BuildTallFlatsUpstairs(FLaneScene& S, const FLaneBuilding& B)
{
    if (!IsTallFlatsUpstairs(B)) return;
    const auto L = PerfectWorld::LaneTallFlats::GenerateLayout();
    if (!L.Ok())
    {
        UE_LOG(LogTemp, Error, TEXT("Tall flats upstairs refused: ground=%hs upper=%hs stair=%hs"),
            std::string(LI::RefusalCode(L.Ground.Status)).c_str(), std::string(LI::RefusalCode(L.Upper.Status)).c_str(),
            std::string(PerfectWorld::LaneStair::RefusalCode(L.Stair.Status)).c_str());
        B.Frame.Add(S.Blocks, B.Wall, 0, B.Width, 0, B.Depth, 0, B.RoofZ());
        return;
    }
    T6Box(S, B, ELaneColour::FloorOxide, {0, 1100, 0, 800}, 0, 15);
    for (const auto& Room : L.Ground.Rooms)
        T6Box(S, B, KitColour(Room.FloorPalette), Room.Area, 14, 15, false);
    T6Interior(S, B, L.Ground, true);
    const double Base = L.Stair.LowerFloorCm;
    const double Upper = L.Stair.UpperFloorCm;
    for (const auto& R : L.Slab)
        T6Box(S, B, ELaneColour::FloorOxide, R, Base + L.Stair.SlabUndersideMm * 0.1, Upper);
    for (const auto& Step : L.Stair.Steps) T6Box(S, B, ELaneColour::Concrete, Step.Plan, Base, Base + Step.TopMm * 0.1);
    for (const auto& Landing : L.Stair.Landings)
        if (Landing.Solid) T6Box(S, B, ELaneColour::Concrete, Landing.Plan, Base, Base + Landing.TopMm * 0.1);
    for (const auto& Guard : L.Stair.Guards)
        T6Box(S, B, ELaneColour::TrimDark, Guard.Plan, Base + Guard.BaseMm * 0.1, Base + Guard.TopMm * 0.1);
    T6Interior(S, B, L.Upper);
    // Shaft walls are real enclosures, continuing the existing passage partitions above the slab.
    T6Box(S, B, B.Wall, {0, 485, 0, 800}, 315, 589); // sealed, unused west half
    T6Box(S, B, B.Wall, {485, 615, 0, 25}, 315, 589);
    T6Box(S, B, B.Wall, {485, 615, 775, 800}, 315, 589);
    T6Box(S, B, ELaneColour::CeilingWhite, {0, 1100, 0, 800}, 589, 590);
    T6Box(S, B, B.Wall, {0, 1100, 0, 800}, 590, B.RoofZ());
    FLaneRoomLight StairLight;
    StairLight.Position = B.Frame.Pt(550, 715, 559);
    StairLight.Radius = 450;
    StairLight.Lumens = 900;
    S.RoomLights.Add(StairLight);
    // Keep the street threshold and shallow canopy clear of the shared front door.
    B.Frame.Add(S.Blocks, ELaneColour::Concrete, 488, 612, -42, 0, 0, 15.5);
    B.Frame.Add(S.Blocks, ELaneColour::TrimLight, 476, 624, -38, 0, 236, 242);
}
} // namespace PWLane
