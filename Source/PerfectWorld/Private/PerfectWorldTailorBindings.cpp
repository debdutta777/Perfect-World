#include "PerfectWorldTailorBindings.h"

#include "Engine/World.h"
#include "PerfectWorldLaneKitFrontages.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNpcPilotBridge.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldProprietorBindings.h"

namespace PWLane
{
namespace
{
struct FR66Route { const char* Suffix; double Cm; };
constexpr FR66Route R66Routes[] = {{"counter_to_door", 1182}, {"counter_to_workbench", 747},
    {"counter_to_finishing_table", 1137}};
FName R66AnchorId(const char* Suffix) { return FName(UTF8_TO_TCHAR((std::string("anchor.tailor_shop.") + Suffix).c_str())); }
FName R66RouteId(const char* Suffix) { return FName(UTF8_TO_TCHAR((std::string("route.tailor_shop.") + Suffix).c_str())); }
double R66Length(const TArray<FVector>& Points)
{
    double Cm = 0;
    for (int32 I = 1; I < Points.Num(); ++I) Cm += FVector::Dist2D(Points[I - 1], Points[I]);
    return Cm;
}
}

FNpBindingProvider R66TailorBindings(const FLaneBuilding& Building)
{
    FLaneKitFrontage Kit;
    if (!IsKitFrontage(Building, Kit) || Kit.BuildingId != FName(R66TailorBuilding)) return {};
    const auto Layout = MakeKitFrontageLayout(Building, Kit);
    if (!Layout.Ok()) return {};
    FNpBindingProvider Provider;
    for (const char* Suffix : {"staff_counter", "workbench", "finishing_table", "front_door", "customer_counter"})
    {
        const auto* Anchor = LI::FindAnchor(Layout, Layout.BuildingId + ".anchor." + Suffix);
        if (Anchor == nullptr) return {};
        FNpAnchorBinding Binding;
        Binding.Feet = FTransform(FRotator(0, KitYaw(Building, Anchor->FaceU, Anchor->FaceV), 0),
            KitPoint(Building, Layout, Anchor->U, Anchor->V));
        Provider.Anchors.Add(R66AnchorId(Suffix), Binding);
    }
    FNpAnchorBinding Home;
    Home.Kind = ENpBindingKind::OffLane;
    Home.ExitAnchor = R66AnchorId("front_door");
    Provider.Anchors.Add(FName(TEXT("location.nabapara.dutta_home")), Home);
    for (const auto& Row : R66Routes)
    {
        const auto* Route = LI::FindRoute(Layout, Layout.BuildingId + ".route." + Row.Suffix);
        if (Route == nullptr) return {};
        FNpRouteBinding Binding;
        for (const auto& Point : Route->Points) Binding.Points.Add(KitPoint(Building, Layout, Point.U, Point.V));
        if (FMath::Abs(R66Length(Binding.Points) - Row.Cm) > 1.0) return {};
        Provider.Routes.Add(R66RouteId(Row.Suffix), MoveTemp(Binding));
    }
    const auto* Bench = Provider.Routes.Find(R66RouteId("counter_to_workbench"));
    const auto* Finish = Provider.Routes.Find(R66RouteId("counter_to_finishing_table"));
    if (Bench == nullptr || Finish == nullptr || Bench->Points.Num() != 4 || Finish->Points.Num() != 5) return {};
    FNpRouteBinding Short;
    Short.Points = {Bench->Points[3], Finish->Points[3], Finish->Points[4]};
    if (FMath::Abs(R66Length(Short.Points) - 390.0) > 1.0) return {};
    Provider.Routes.Add(R66RouteId("workbench_to_finishing_table"), MoveTemp(Short));
    Provider.Routes.Add(FName(TEXT("route.dutta_home.footpath_to_home")), FNpRouteBinding{{}, true});
    for (const auto& Opening : Layout.Openings)
    {
        if (Opening.Kind == LI::OpeningKind::Window) continue;
        FNpDoorwayZone Zone;
        Zone.Id = FName(UTF8_TO_TCHAR((Opening.DoorId.empty() ? Opening.Id : Opening.DoorId).c_str()));
        const double Approach = Opening.DoorId.empty() ? 65.0 : LI::DoorClearWidthCm + 40.0;
        Zone.Bounds += KitPoint(Building, Layout, Opening.Plan.U0 - 34, Opening.Plan.V0 - Approach);
        Zone.Bounds += KitPoint(Building, Layout, Opening.Plan.U1 + 34, Opening.Plan.V1 + Approach, 212);
        if (Opening.RoomA == "outside")
        {
            const double U = 0.5 * (Opening.Plan.U0 + Opening.Plan.U1);
            Zone.WaitingFeet = {KitPoint(Building, Layout, U - 150, -85), KitPoint(Building, Layout, U + 150, -85)};
        }
        else
        {
            for (const char* Suffix : {"staff_counter", "workbench", "finishing_table"})
                Zone.WaitingFeet.Add(Provider.Anchors.Find(R66AnchorId(Suffix))->Feet.GetLocation());
        }
        Provider.Doorways.Add(MoveTemp(Zone));
    }
    return Provider;
}

bool R66SpawnTailor(UWorld& World, const FLaneScene& Scene)
{
    FLaneBuilding Building;
    if (!HasKitDoor(Scene, R66TailorDoor) || !MakeProprietorBuilding(R66TailorBuilding, Building) ||
        !R55IsWorkPilot(FName(R66TailorPerson))) return false;
    auto Provider = R66TailorBindings(Building);
    auto* Presence = World.GetSubsystem<UPWNpcPresenceSubsystem>();
    if (Presence == nullptr || Provider.Anchors.Num() != 6 || Provider.Routes.Num() != 5 ||
        !Presence->Bindings().RegisterProvider(FName(R66TailorBuilding), MoveTemp(Provider))) return false;
    if (!Presence->RegisterScheduledPerson(FName(R66TailorPerson),
        NSLOCTEXT("PerfectWorldLane", "R66TailorName", "Satyajit Dutta"), EPWNpcOutfit::ShopProprietor)) return false;
    if (const auto* Life = UPWLaneLifeSubsystem::Get(&World)) Presence->Reconcile(Life->GetNow());
    return true;
}
}
