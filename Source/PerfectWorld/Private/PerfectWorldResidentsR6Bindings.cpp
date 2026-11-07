#include "PerfectWorldResidentsR6Bindings.h"

#include "Engine/World.h"
#include "PerfectWorldLaneKitFrontages.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldSubrataBindings.h"

namespace PWLane
{
namespace
{
const FR64Home R64Madhabi{TEXT("person.madhabi_das"), TEXT("Madhabi Das"), TEXT("building.nabapara.alley_house"),
    TEXT("location.nabapara.alley_house"), TEXT("door.nabapara.alley_house_front"), TEXT("anchor.madhabi.alley_house_front"),
    TEXT("location.nabapara.madhabi_work"), TEXT("route.madhabi.home_to_work"), EPWNpcOutfit::HouseholdAdult, 156.0, 2100.0};
const FR64Home R64Arup{TEXT("person.arup_basu"), TEXT("Arup Basu"), TEXT("building.nabapara.stair_flats"),
    TEXT("location.nabapara.stair_flats"), TEXT("door.nabapara.stair_flats_front"), TEXT("anchor.arup.stair_flats_front"),
    TEXT("location.nabapara.arup_work"), TEXT("route.arup.home_to_work"), EPWNpcOutfit::ParentHousehold, 179.0, 2800.0};

FNpAnchorBinding R64Local(const FVector& Feet, double Yaw)
{
    FNpAnchorBinding Out;
    Out.Feet = FTransform(FRotator(0, Yaw, 0), Feet);
    return Out;
}

FNpAnchorBinding R64Away(const TCHAR* Exit)
{
    FNpAnchorBinding Out;
    Out.Kind = ENpBindingKind::OffLane;
    Out.ExitAnchor = FName(Exit);
    return Out;
}

const LI::Opening* R64Opening(const LI::InteriorLayout& Layout, const TCHAR* Door)
{
    const std::string Id(TCHAR_TO_UTF8(Door));
    for (const auto& Opening : Layout.Openings) if (Opening.DoorId == Id) return &Opening;
    return nullptr;
}

void R64Doorway(FNpBindingProvider& Out, const FLaneBuilding& Building, const LI::InteriorLayout& Layout, const LI::Opening& Opening,
                const FVector& Wait)
{
    const bool AlongU = Opening.Plan.Width() > Opening.Plan.Depth();
    FNpDoorwayZone Zone;
    Zone.Id = FName(UTF8_TO_TCHAR(Opening.DoorId.c_str()));
    Zone.Bounds += KitPoint(Building, Layout, Opening.Plan.U0 - (AlongU ? 34 : 135), Opening.Plan.V0 - (AlongU ? 135 : 34));
    Zone.Bounds += KitPoint(Building, Layout, Opening.Plan.U1 + (AlongU ? 34 : 135), Opening.Plan.V1 + (AlongU ? 135 : 34), 212);
    Zone.WaitingFeet = {Wait};
    Out.Doorways.Add(MoveTemp(Zone));
}
}

const FR64Home& R64MadhabiHome() { return R64Madhabi; }
const FR64Home& R64ArupHome() { return R64Arup; }

double ResidentsR6RouteLength(const FNpRouteBinding& Route)
{
    double Length = 0;
    for (int32 Index = 1; Index < Route.Points.Num(); ++Index) Length += FVector::Dist(Route.Points[Index - 1], Route.Points[Index]);
    return Length;
}

bool MakeResidentsR6Building(const FR64Home& Home, FLaneBuilding& Out)
{
    return MakeSubrataBuilding(Home.Building, Out);
}

FNpBindingProvider NpResidentsR6Bindings(const FLaneBuilding& Building, const FR64Home& Home)
{
    FLaneKitFrontage Kit;
    if (!IsKitFrontage(Building, Kit) || Kit.BuildingId != FName(Home.Building) || Kit.Kind != LI::Variant::CompactResidence) return {};
    const auto Layout = MakeKitFrontageLayout(Building, Kit);
    if (!Layout.Ok()) return {};
    const auto* Front = R64Opening(Layout, Home.Door);
    const auto* Bed = LI::FindAnchor(Layout, Layout.BuildingId + ".anchor.bed_side");
    const LI::Opening* Internal = nullptr;
    for (const auto& Opening : Layout.Openings)
        if (Opening.Kind == LI::OpeningKind::Doorway && Opening.RoomA != "outside") Internal = &Opening;
    if (!Front || !Internal || !Bed || Bed->Kind != LI::AnchorKind::Stand) return {};
    FNpBindingProvider Out;
    // bed_side is the kit's free 80 x 80 standing use zone, not the mattress or a solid chair. The logical home ID stays the
    // schedule zone (LaneLifeKnock compares it with DoorDef.LocationId); the kit anchors only supply a real standing body.
    Out.Anchors.Add(FName(Home.HomeLocation), R64Local(KitPoint(Building, Layout, Bed->U, Bed->V), KitYaw(Building, Bed->FaceU, Bed->FaceV)));
    TArray<FKitAnchor> KitAnchors;
    GetKitAnchors(Building, Layout, KitAnchors);
    for (const auto& Anchor : KitAnchors) Out.Anchors.Add(Anchor.Id, R64Local(Anchor.Location, Anchor.YawDegrees));
    const double DoorU = (Front->Plan.U0 + Front->Plan.U1) * 0.5;
    const double InternalU = (Internal->Plan.U0 + Internal->Plan.U1) * 0.5;
    const auto Point = [&](double U, double V) { return KitPoint(Building, Layout, U, V); };
    const FVector Threshold = Point(DoorU, -45);
    Out.Anchors.Add(FName(Home.FrontAlias), R64Local(Threshold, KitYaw(Building, 0, 1)));
    Out.Anchors.Add(FName(Home.AwayLocation), R64Away(Home.FrontAlias));
    // The one authored away leg includes off-lane distance; the visible interior and threshold stretch is reported, never padded.
    FNpRouteBinding Route;
    Route.Points = {Point(Bed->U, Bed->V), Point(InternalU, Bed->V), Point(InternalU, Internal->Plan.V0 - 65),
        Point(DoorU, Internal->Plan.V0 - 65), Threshold};
    const double Measured = ResidentsR6RouteLength(Route);
    if (Measured > Home.AuthoredRouteCm)
        UE_LOG(LogTemp, Error, TEXT("R64 %s route %.2f cm exceeds the authored %.0f cm: update the JSON route/travel times and the ApprovedRoute contract together; route retained."),
            Home.Person, Measured, Home.AuthoredRouteCm);
    Out.Routes.Add(FName(Home.Route), MoveTemp(Route));
    // Wait to the right of the doorway, clear of the bell and the inward leaf's footprint.
    R64Doorway(Out, Building, Layout, *Front, Point(DoorU + 110, -85));
    return Out;
}

bool SpawnResidentsR6(UWorld& World, const FLaneScene& Scene)
{
    auto* Presence = World.GetSubsystem<UPWNpcPresenceSubsystem>();
    if (!Presence) return false;
    bool bAll = true;
    for (const FR64Home* Home : {&R64Madhabi, &R64Arup})
    {
        FLaneBuilding Building;
        if (!HasKitDoor(Scene, Home->Door) || !MakeResidentsR6Building(*Home, Building))
        {
            UE_LOG(LogTemp, Error, TEXT("R64 %s: the home or its front door was not built; no fallback spawn."), Home->Person);
            bAll = false;
            continue;
        }
        auto Provider = NpResidentsR6Bindings(Building, *Home);
        if (Provider.Anchors.IsEmpty() || !Presence->Bindings().RegisterProvider(FName(Home->HomeLocation), MoveTemp(Provider)) ||
            !Presence->RegisterScheduledPerson(FName(Home->Person), FText::AsCultureInvariant(FString(Home->Name)), Home->Outfit))
        {
            UE_LOG(LogTemp, Error, TEXT("R64 %s: provider or scheduled registration failed."), Home->Person);
            bAll = false;
        }
    }
    if (const auto* Life = UPWLaneLifeSubsystem::Get(&World)) Presence->Reconcile(Life->GetNow());
    return bAll;
}
}
