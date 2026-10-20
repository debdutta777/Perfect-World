#include "PerfectWorldResidentsR5Bindings.h"

#include "Engine/World.h"
#include "LaneTallFlatsLayout.h"
#include "PerfectWorldLaneKitFrontages.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldRupaBindings.h"

namespace PWLane
{
namespace
{
FNpAnchorBinding R52Local(const FVector& Feet, double Yaw)
{
    FNpAnchorBinding Out;
    Out.Feet = FTransform(FRotator(0, Yaw, 0), Feet);
    return Out;
}

FNpAnchorBinding R52Away(const TCHAR* Exit)
{
    FNpAnchorBinding Out;
    Out.Kind = ENpBindingKind::OffLane;
    Out.ExitAnchor = FName(Exit);
    return Out;
}

const LI::Opening* R52Opening(const LI::InteriorLayout& Layout, const TCHAR* Door)
{
    const std::string Id(TCHAR_TO_UTF8(Door));
    for (const auto& Opening : Layout.Openings) if (Opening.DoorId == Id) return &Opening;
    return nullptr;
}

bool R52Home(FNpBindingProvider& Out, const FLaneBuilding& Building, const LI::InteriorLayout& Layout,
             const TCHAR* Home)
{
    const auto* Bed = LI::FindAnchor(Layout, Layout.BuildingId + ".anchor.bed_side");
    if (!Bed || Bed->Kind != LI::AnchorKind::Stand) return false;
    // bed_side is the kit's free 80 x 80 standing use zone, not the mattress or a solid chair.
    Out.Anchors.Add(FName(Home), R52Local(KitPoint(Building, Layout, Bed->U, Bed->V),
        KitYaw(Building, Bed->FaceU, Bed->FaceV)));
    TArray<FKitAnchor> Anchors;
    GetKitAnchors(Building, Layout, Anchors);
    for (const auto& Anchor : Anchors)
        Out.Anchors.Add(Anchor.Id, R52Local(Anchor.Location, Anchor.YawDegrees));
    return true;
}

void R52Doorway(FNpBindingProvider& Out, const FLaneBuilding& Building, const LI::InteriorLayout& Layout,
                const LI::Opening& Opening, const FVector& Wait)
{
    const bool AlongU = Opening.Plan.Width() > Opening.Plan.Depth();
    FNpDoorwayZone Zone;
    Zone.Id = FName(UTF8_TO_TCHAR(Opening.DoorId.c_str()));
    Zone.Bounds += KitPoint(Building, Layout, Opening.Plan.U0 - (AlongU ? 34 : 135),
        Opening.Plan.V0 - (AlongU ? 135 : 34));
    Zone.Bounds += KitPoint(Building, Layout, Opening.Plan.U1 + (AlongU ? 34 : 135),
        Opening.Plan.V1 + (AlongU ? 135 : 34), 212);
    Zone.WaitingFeet = {Wait};
    Out.Doorways.Add(MoveTemp(Zone));
}
}

double ResidentsR5RouteLength(const FNpRouteBinding& Route)
{
    double Length = 0;
    for (int32 Index = 1; Index < Route.Points.Num(); ++Index)
        Length += FVector::Dist(Route.Points[Index - 1], Route.Points[Index]);
    return Length;
}

FNpBindingProvider NpPrabirR5Bindings(const FLaneBuilding& OldHouse)
{
    FLaneKitFrontage Kit;
    if (!IsKitFrontage(OldHouse, Kit) || Kit.BuildingId != FName(R52OldHouse)) return {};
    const auto Layout = MakeKitFrontageLayout(OldHouse, Kit);
    const auto* Front = Layout.Ok() ? R52Opening(Layout, R52OldDoor) : nullptr;
    const auto* Bed = LI::FindAnchor(Layout, Layout.BuildingId + ".anchor.bed_side");
    const LI::Opening* Internal = nullptr;
    for (const auto& Opening : Layout.Openings)
        if (Opening.Kind == LI::OpeningKind::Doorway && Opening.RoomA != "outside") Internal = &Opening;
    FNpBindingProvider Out;
    if (!Front || !Internal || !Bed || !R52Home(Out, OldHouse, Layout, R52PrabirHome)) return {};
    const double DoorU = (Front->Plan.U0 + Front->Plan.U1) * 0.5;
    const double InternalU = (Internal->Plan.U0 + Internal->Plan.U1) * 0.5;
    const auto Point = [&](double U, double V) { return KitPoint(OldHouse, Layout, U, V); };
    const FVector Threshold = Point(DoorU, -45);
    Out.Anchors.Add(FName(R52PrabirFront), R52Local(Threshold, KitYaw(OldHouse, 0, 1)));
    Out.Anchors.Add(FName(R52PrabirAway), R52Away(R52PrabirFront));
    // The sole authored errand leg includes off-lane distance. Keep the entire physical ingress/egress
    // through the actual doors; its shorter measured geometry is reported, never padded with a fake detour.
    Out.Routes.Add(FName(R52PrabirRoute), FNpRouteBinding{{Point(Bed->U, Bed->V),
        Point(InternalU, Bed->V), Point(InternalU, Internal->Plan.V0 - 65),
        Point(DoorU, Internal->Plan.V0 - 65), Threshold}, false});
    R52Doorway(Out, OldHouse, Layout, *Front, Point(DoorU + 160, -85));
    return Out;
}

FNpBindingProvider NpIshitaR5Bindings(const FLaneBuilding& TallFlats)
{
    FLaneKitFrontage Kit;
    if (!IsKitFrontage(TallFlats, Kit) || Kit.BuildingId != FName(R52TallFlats)) return {};
    const auto Layout = PerfectWorld::LaneTallFlats::GenerateLayout();
    if (!Layout.Ok()) return {};
    const auto* Upper = R52Opening(Layout.Upper, R52UpperDoor);
    const auto* Front = R52Opening(Layout.Ground, R52SharedDoor);
    const auto* Bed = LI::FindAnchor(Layout.Upper, Layout.Upper.BuildingId + ".anchor.bed_side");
    FNpBindingProvider Out;
    if (!Upper || !Front || !Bed || !R52Home(Out, TallFlats, Layout.Upper, R52IshitaHome)) return {};
    const auto Point = [&](double U, double V, double Z) { return TallFlats.Frame.Pt(U, V, Z); };
    const auto& Stair = Layout.Stair;
    const double LineU = (Stair.Footprint.U0 + Stair.Footprint.U1) * 0.5;
    const double DoorV = (Upper->Plan.V0 + Upper->Plan.V1) * 0.5;
    const double ColumnU = Upper->Plan.U1 + 75.0; // centre of the existing 120 cm sleeping column
    const double CrossV = 500; // centre of the existing sleeping cross, V442..562
    const double FrontU = (Front->Plan.U0 + Front->Plan.U1) * 0.5;
    const FVector Threshold = Point(FrontU, -45, Stair.LowerFloorCm);
    Out.Anchors.Add(FName(R52IshitaFront), R52Local(Threshold, KitYaw(TallFlats, 0, 1)));
    Out.Anchors.Add(FName(R52IshitaAway), R52Away(R52IshitaFront));
    FNpRouteBinding Route;
    Route.Points = {Point(Bed->U, Bed->V, Stair.UpperFloorCm), Point(Bed->U, CrossV, Stair.UpperFloorCm),
        Point(ColumnU, CrossV, Stair.UpperFloorCm), Point(ColumnU, DoorV, Stair.UpperFloorCm),
        Point(LineU, DoorV, Stair.UpperFloorCm)};
    // Real nosings, in reverse climbing order. Include every riser, with the same mm heights as the presenter.
    Route.Points.Add(Point(LineU, Stair.Steps.back().Plan.V1, Stair.UpperFloorCm));
    for (size_t Index = Stair.Steps.size(); Index > 0; --Index)
    {
        const auto& Step = Stair.Steps[Index - 1];
        Route.Points.Add(Point(LineU, Step.Plan.V0, Stair.LowerFloorCm + Step.TopMm * 0.1));
    }
    for (const auto& Landing : Stair.Landings)
        if (Landing.Kind == PerfectWorld::LaneStair::LandingKind::Bottom)
            Route.Points.Add(Point(LineU, (Landing.Plan.V0 + Landing.Plan.V1) * 0.5, Stair.LowerFloorCm));
    Route.Points.Add(Threshold);
    const double Measured = ResidentsR5RouteLength(Route);
    if (Measured > 1680.0)
        UE_LOG(LogTemp, Error, TEXT("R52 upper_to_front %.2f cm exceeds 1680: update schedule length/travel times and ApprovedRoute contract before acceptance; route retained."), Measured);
    Out.Routes.Add(FName(R52UpperRoute), MoveTemp(Route));
    Out.Routes.Add(FName(R52WorkRoute), FNpRouteBinding{{}, true}); // 2520 cm beyond the shared front
    R52Doorway(Out, TallFlats, Layout.Upper, *Upper, Point(ColumnU, CrossV, Stair.UpperFloorCm));
    // The shared front's zone is already owned by Subrata. Do not duplicate its provider ownership.
    return Out;
}

bool SpawnResidentsR5(UWorld& World, const FLaneScene& Scene)
{
    FLaneBuilding Old, Tall;
    if (!HasKitDoor(Scene, R52OldDoor) || !HasKitDoor(Scene, R52UpperDoor) || !HasKitDoor(Scene, R52SharedDoor) ||
        !MakeRupaBuilding(R52OldHouse, Old) || !MakeRupaBuilding(R52TallFlats, Tall)) return false;
    auto Prabir = NpPrabirR5Bindings(Old);
    auto Ishita = NpIshitaR5Bindings(Tall);
    auto* Presence = World.GetSubsystem<UPWNpcPresenceSubsystem>();
    if (!Presence || Prabir.Anchors.IsEmpty() || Ishita.Anchors.IsEmpty()) return false;
    if (!Presence->Bindings().RegisterProvider(FName(R52PrabirHome), MoveTemp(Prabir)) ||
        !Presence->Bindings().RegisterProvider(FName(R52IshitaHome), MoveTemp(Ishita))) return false;
    if (!Presence->RegisterScheduledPerson(FName(R52Prabir),
        NSLOCTEXT("PerfectWorldLane", "PrabirName", "Prabir Chatterjee"), EPWNpcOutfit::HouseholdAdult)) return false;
    if (!Presence->RegisterScheduledPerson(FName(R52Ishita),
        NSLOCTEXT("PerfectWorldLane", "IshitaName", "Ishita Banerjee"), EPWNpcOutfit::ParentHousehold)) return false;
    if (const auto* Life = UPWLaneLifeSubsystem::Get(&World)) Presence->Reconcile(Life->GetNow());
    return true;
}
}
