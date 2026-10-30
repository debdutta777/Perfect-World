#include "PerfectWorldProprietorBindings.h"

#include "Engine/World.h"
#include "PerfectWorldLaneKitFrontages.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldNpcPilotBridge.h"

namespace PWLane
{
namespace
{
// File-local names start with Pr (proprietor): the module builds in unity blobs.

/** One kit anchor under its public id. */
struct FPrAnchorRow
{
    const char* Role;
    const TCHAR* PublicId;
};
/** One kit route from the counter under its public schedule id; the polyline must equal AuthoredCm. */
struct FPrRouteRow
{
    const char* LayoutName;
    const TCHAR* PublicId;
    int32 AuthoredCm;
};
/** Points From..To (inclusive, either direction) of one kit route. */
struct FPrPiece
{
    const char* LayoutName;
    int32 From;
    int32 To;
};
/** A route made of pieces of the kit's own polylines. */
struct FPrDerivedRow
{
    const TCHAR* PublicId;
    int32 AuthoredCm;
    int32 PieceCount;
    FPrPiece Pieces[3];
};

struct FPrShop
{
    const TCHAR* BuildingId;
    const TCHAR* FootpathId;
    const TCHAR* HomeId;
    const TCHAR* HomeRouteId;
    const FPrAnchorRow* Anchors;
    int32 AnchorCount;
    const FPrRouteRow* Routes;
    int32 RouteCount;
    const FPrDerivedRow* Derived;
    int32 DerivedCount;
    const TCHAR* const* WorkAnchors; // public ids where staff wait beside a doorway
    int32 WorkAnchorCount;
};

constexpr FPrAnchorRow PrWorkshopAnchors[] = {
    {"staff_counter", TEXT("anchor.repair_workshop.staff_counter")},
    {"tool_rack", TEXT("anchor.repair_workshop.tool_rack")},
    {"workbench", TEXT("anchor.repair_workshop.workbench")},
    {"finishing_table", TEXT("anchor.repair_workshop.finishing_table")},
    {"front_door", TEXT("anchor.repair_workshop.front_door")},
    {"customer_counter", TEXT("anchor.repair_workshop.customer")},
};
constexpr FPrRouteRow PrWorkshopRoutes[] = {
    {"counter_to_door", TEXT("route.repair_workshop.counter_to_door"), 1582},
    {"counter_to_tool_rack", TEXT("route.repair_workshop.counter_to_tool_rack"), 953},
    {"counter_to_workbench", TEXT("route.repair_workshop.counter_to_workbench"), 947},
    {"counter_to_finishing_table", TEXT("route.repair_workshop.counter_to_finishing_table"), 1537},
};
// Workbench (815,639)-line: bench -> gap column -> down to the rack; bench -> along the finishing-table line; door -> the staff gap -> bench.
constexpr FPrDerivedRow PrWorkshopDerived[] = {
    {TEXT("route.repair_workshop.workbench_to_tool_rack"), 306, 2, {{"counter_to_workbench", 3, 2}, {"counter_to_tool_rack", 2, 2}, {}}},
    {TEXT("route.repair_workshop.workbench_to_finishing_table"), 590, 2, {{"counter_to_workbench", 3, 3}, {"counter_to_finishing_table", 3, 4}, {}}},
    {TEXT("route.repair_workshop.door_to_workbench"), 1399, 2, {{"counter_to_door", 4, 1}, {"counter_to_workbench", 2, 3}, {}}},
};
const TCHAR* const PrWorkshopWork[] = {TEXT("anchor.repair_workshop.staff_counter"), TEXT("anchor.repair_workshop.workbench"),
                                       TEXT("anchor.repair_workshop.tool_rack"), TEXT("anchor.repair_workshop.finishing_table")};

constexpr FPrAnchorRow PrStoreAnchors[] = {
    {"staff_counter", TEXT("anchor.hardware_store.staff_counter")},
    {"stock_room", TEXT("anchor.hardware_store.stock_room")},
    {"weigh_station", TEXT("anchor.hardware_store.weigh_station")},
    {"front_door", TEXT("anchor.hardware_store.front_door")},
    {"customer_counter", TEXT("anchor.hardware_store.customer")},
    {"browse_shelf", TEXT("anchor.hardware_store.browse_shelf")},
};
constexpr FPrRouteRow PrStoreRoutes[] = {
    {"counter_to_door", TEXT("route.hardware_store.counter_to_door"), 2100},
    {"counter_to_stock_room", TEXT("route.hardware_store.counter_to_stock_room"), 1135},
    {"counter_to_weigh_station", TEXT("route.hardware_store.counter_to_weigh_station"), 1197},
};
constexpr FPrDerivedRow PrStoreDerived[] = {
    {TEXT("route.hardware_store.weigh_station_to_stock_room"), 158, 2, {{"counter_to_weigh_station", 3, 2}, {"counter_to_stock_room", 2, 2}, {}}},
    {TEXT("route.hardware_store.door_to_stock_room"), 1505, 2, {{"counter_to_door", 4, 1}, {"counter_to_stock_room", 2, 2}, {}}},
};
const TCHAR* const PrStoreWork[] = {TEXT("anchor.hardware_store.staff_counter"), TEXT("anchor.hardware_store.stock_room"),
                                    TEXT("anchor.hardware_store.weigh_station")};

FNpAnchorBinding PrOffLanePlace(const TCHAR* ExitAnchor)
{
    FNpAnchorBinding Place;
    Place.Kind = ENpBindingKind::OffLane;
    Place.ExitAnchor = FName(ExitAnchor);
    return Place;
}

double PrLength(const TArray<FVector>& Points)
{
    double Total = 0.0;
    for (int32 I = 0; I + 1 < Points.Num(); ++I) { Total += FVector::Dist2D(Points[I], Points[I + 1]); }
    return Total;
}

void PrAddDistinct(TArray<FVector>& Points, const FVector& Point)
{
    if (Points.IsEmpty() || !Points.Last().Equals(Point, 0.5)) { Points.Add(Point); }
}

bool PrAppendPiece(const FLaneBuilding& B, const LI::InteriorLayout& L, const FPrPiece& Piece, TArray<FVector>& Out)
{
    const LI::Route* Route = LI::FindRoute(L, L.BuildingId + ".route." + Piece.LayoutName);
    if (Route == nullptr) { return false; }
    const int32 Count = static_cast<int32>(Route->Points.size());
    if (Piece.From < 0 || Piece.To < 0 || Piece.From >= Count || Piece.To >= Count) { return false; }
    const int32 Step = Piece.To >= Piece.From ? 1 : -1;
    for (int32 I = Piece.From;; I += Step)
    {
        const LI::RoutePoint& P = Route->Points[static_cast<size_t>(I)];
        PrAddDistinct(Out, KitPoint(B, L, P.U, P.V, 0.0));
        if (I == Piece.To) { break; }
    }
    return true;
}

/** Every opening of the layout but the windows: the leaf, its sweep and approach, with waiting spots (footpath outside, work anchors inside). */
FNpDoorwayZone PrDoorway(const FLaneBuilding& B, const LI::InteriorLayout& L, const LI::Opening& O, const FNpBindingProvider& Provider,
                         const TCHAR* const* WorkAnchors, int32 WorkCount)
{
    FNpDoorwayZone Zone;
    Zone.Id = FName(UTF8_TO_TCHAR((O.DoorId.empty() ? O.Id : O.DoorId).c_str()));
    const double Approach = O.DoorId.empty() ? 65.0 : LI::DoorClearWidthCm + 40.0;
    Zone.Bounds += KitPoint(B, L, O.Plan.U0 - 34.0, O.Plan.V0 - Approach, 0.0);
    Zone.Bounds += KitPoint(B, L, O.Plan.U1 + 34.0, O.Plan.V1 + Approach, 212.0);
    if (O.RoomA == "outside")
    {
        const double Centre = 0.5 * (O.Plan.U0 + O.Plan.U1);
        Zone.WaitingFeet = {KitPoint(B, L, Centre - 150.0, -85.0, 0.0), KitPoint(B, L, Centre + 150.0, -85.0, 0.0)};
    }
    else
    {
        for (int32 I = 0; I < WorkCount; ++I)
        {
            if (const FNpAnchorBinding* Wait = Provider.Anchors.Find(FName(WorkAnchors[I]))) { Zone.WaitingFeet.Add(Wait->Feet.GetLocation()); }
        }
    }
    return Zone;
}

FNpBindingProvider PrBuild(const FLaneBuilding& Building, const FPrShop& Shop)
{
    FLaneKitFrontage Kit;
    if (!IsKitFrontage(Building, Kit) || Kit.BuildingId != FName(Shop.BuildingId)) { return FNpBindingProvider(); }
    const LI::InteriorLayout Layout = MakeKitFrontageLayout(Building, Kit);
    if (!Layout.Ok()) { return FNpBindingProvider(); }
    FNpBindingProvider Provider;
    for (int32 I = 0; I < Shop.AnchorCount; ++I)
    {
        const LI::Anchor* A = LI::FindAnchor(Layout, Layout.BuildingId + ".anchor." + Shop.Anchors[I].Role);
        if (A == nullptr) { return FNpBindingProvider(); }
        FNpAnchorBinding Binding;
        Binding.Feet = FTransform(FRotator(0.0, KitYaw(Building, A->FaceU, A->FaceV), 0.0), KitPoint(Building, Layout, A->U, A->V, 0.0));
        Provider.Anchors.Add(FName(Shop.Anchors[I].PublicId), Binding);
    }
    // The threshold anchor is already on the footpath (V -45); the footpath anchor is the same transform, so nobody appears or vanishes elsewhere.
    const FNpAnchorBinding* Front = nullptr;
    for (int32 I = 0; I < Shop.AnchorCount; ++I)
    {
        if (FCStringAnsi::Strcmp(Shop.Anchors[I].Role, "front_door") == 0) { Front = Provider.Anchors.Find(FName(Shop.Anchors[I].PublicId)); }
    }
    if (Front == nullptr) { return FNpBindingProvider(); }
    Provider.Anchors.Add(FName(Shop.FootpathId), *Front);
    Provider.Anchors.Add(FName(Shop.HomeId), PrOffLanePlace(Shop.FootpathId));
    for (int32 I = 0; I < Shop.RouteCount; ++I)
    {
        const LI::Route* Route = LI::FindRoute(Layout, Layout.BuildingId + ".route." + Shop.Routes[I].LayoutName);
        if (Route == nullptr) { return FNpBindingProvider(); }
        FNpRouteBinding Line;
        for (const LI::RoutePoint& P : Route->Points) { PrAddDistinct(Line.Points, KitPoint(Building, Layout, P.U, P.V, 0.0)); }
        if (FMath::Abs(PrLength(Line.Points) - Shop.Routes[I].AuthoredCm) > 1.0) { return FNpBindingProvider(); }
        Provider.Routes.Add(FName(Shop.Routes[I].PublicId), MoveTemp(Line));
    }
    for (int32 I = 0; I < Shop.DerivedCount; ++I)
    {
        const FPrDerivedRow& Row = Shop.Derived[I];
        FNpRouteBinding Line;
        for (int32 P = 0; P < Row.PieceCount; ++P)
        {
            if (!PrAppendPiece(Building, Layout, Row.Pieces[P], Line.Points)) { return FNpBindingProvider(); }
        }
        if (FMath::Abs(PrLength(Line.Points) - Row.AuthoredCm) > 1.0) { return FNpBindingProvider(); }
        Provider.Routes.Add(FName(Row.PublicId), MoveTemp(Line));
    }
    // The home walk is an authored off-lane segment: no points, so the person is absent from the lane meanwhile.
    Provider.Routes.Add(FName(Shop.HomeRouteId), FNpRouteBinding{{}, true});
    for (const LI::Opening& Opening : Layout.Openings)
    {
        if (Opening.Kind == LI::OpeningKind::Window) { continue; }
        Provider.Doorways.Add(PrDoorway(Building, Layout, Opening, Provider, Shop.WorkAnchors, Shop.WorkAnchorCount));
    }
    return Provider;
}

const FPrShop PrWorkshop{TEXT("building.nabapara.repair_workshop"), TEXT("location.nabapara.repair_workshop_footpath"),
                         TEXT("location.nabapara.pal_home"), TEXT("route.pal_home.footpath_to_home"),
                         PrWorkshopAnchors, UE_ARRAY_COUNT(PrWorkshopAnchors), PrWorkshopRoutes, UE_ARRAY_COUNT(PrWorkshopRoutes),
                         PrWorkshopDerived, UE_ARRAY_COUNT(PrWorkshopDerived), PrWorkshopWork, UE_ARRAY_COUNT(PrWorkshopWork)};
const FPrShop PrStore{TEXT("building.nabapara.hardware_store"), TEXT("location.nabapara.hardware_store_footpath"),
                      TEXT("location.nabapara.saha_home"), TEXT("route.saha_home.footpath_to_home"),
                      PrStoreAnchors, UE_ARRAY_COUNT(PrStoreAnchors), PrStoreRoutes, UE_ARRAY_COUNT(PrStoreRoutes),
                      PrStoreDerived, UE_ARRAY_COUNT(PrStoreDerived), PrStoreWork, UE_ARRAY_COUNT(PrStoreWork)};

bool PrSpawn(UWorld& World, const FLaneScene& Scene, const FPrShop& Shop, const TCHAR* DoorId, const TCHAR* PersonId, const FText& Name,
             FNpBindingProvider (*Bind)(const FLaneBuilding&), const TCHAR* Label)
{
    // Registration opts exactly these identities into the presence-owned R5-04 decision bridge.
    // Presence installs their route block/clear subscription when the physical actor appears.
    if (!R55IsWorkPilot(FName(PersonId))) { return false; }
    if (!HasKitDoor(Scene, DoorId))
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: the shop's door was not built; the proprietor is not placed."), Label);
        return false;
    }
    FLaneBuilding Building;
    if (!MakeProprietorBuilding(Shop.BuildingId, Building))
    {
        UE_LOG(LogTemp, Error, TEXT("%s: the catalogue row of the shop did not resolve; no fallback spawn."), Label);
        return false;
    }
    FNpBindingProvider Provider = Bind(Building);
    if (Provider.Anchors.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("%s: the bindings could not be derived from the built layout; no fallback spawn."), Label);
        return false;
    }
    UPWNpcPresenceSubsystem* Presence = World.GetSubsystem<UPWNpcPresenceSubsystem>();
    if (Presence == nullptr || !Presence->Bindings().RegisterProvider(FName(Shop.BuildingId), MoveTemp(Provider)))
    {
        UE_LOG(LogTemp, Error, TEXT("%s: NPC provider registration failed; no fallback spawn."), Label);
        return false;
    }
    // TODO(outfit): their own looks and authored heights (171 and 162 cm) join NpcLooks (R4-05); placeholder outfit until then.
    if (!Presence->RegisterScheduledPerson(FName(PersonId), Name, EPWNpcOutfit::ShopProprietor))
    {
        UE_LOG(LogTemp, Error, TEXT("%s: schedule registration failed; no fallback spawn."), Label);
        return false;
    }
    if (const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(&World)) { Presence->Reconcile(Life->GetNow()); }
    return true;
}
} // namespace

bool MakeProprietorBuilding(const TCHAR* BuildingId, FLaneBuilding& Out)
{
    const LI::FrontageEntry* Row = LI::FindFrontageById(std::string(TCHAR_TO_UTF8(BuildingId)));
    if (Row == nullptr || (Row->Side != -1 && Row->Side != 1)) { return false; }
    const double Side = static_cast<double>(Row->Side);
    FLaneBuilding Building;
    Building.Width = Row->WidthCm;
    Building.Depth = Row->DepthCm;
    Building.Setback = Row->SetbackCm;
    Building.Storeys = Row->Storeys;
    Building.Side = Row->Side;
    Building.Index = Row->RowIndex;
    Building.Seed = Row->Seed;
    Building.Detail = ELaneDetail::Full;
    Building.Wall = ELaneColour::WallCream;
    Building.Frame = MakeLaneFrame(Row->OriginXCm, Side * Row->FrontAbsYCm, FVector(1.0, 0.0, 0.0), FVector(0.0, Side, 0.0));
    FLaneKitFrontage Kit;
    if (!IsKitFrontage(Building, Kit) || Kit.BuildingId != FName(BuildingId)) { return false; }
    Out = Building;
    return true;
}

FNpBindingProvider NpDebashisBindings(const FLaneBuilding& Workshop) { return PrBuild(Workshop, PrWorkshop); }
FNpBindingProvider NpMahuaBindings(const FLaneBuilding& Store) { return PrBuild(Store, PrStore); }

bool SpawnDebashis(UWorld& World, const FLaneScene& Scene)
{
    return PrSpawn(World, Scene, PrWorkshop, DebashisDoorId, DebashisPersonId, NSLOCTEXT("PerfectWorldLane", "DebashisName", "Debashis"),
                   &NpDebashisBindings, TEXT("Debashis"));
}

bool SpawnMahua(UWorld& World, const FLaneScene& Scene)
{
    return PrSpawn(World, Scene, PrStore, MahuaDoorId, MahuaPersonId, NSLOCTEXT("PerfectWorldLane", "MahuaName", "Mahua"), &NpMahuaBindings,
                   TEXT("Mahua"));
}
} // namespace PWLane
