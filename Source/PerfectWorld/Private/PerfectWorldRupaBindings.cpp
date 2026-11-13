#include "PerfectWorldRupaBindings.h"

#include "Engine/World.h"
#include "PerfectWorldLaneKitFrontages.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldNpcPresenceSubsystem.h"

namespace PWLane
{
namespace
{
// File-local names start with Rg (Rupa Ghosh): the module builds in unity blobs.

FNpAnchorBinding RgLocalAnchor(const FVector& Feet, const double YawDegrees)
{
    FNpAnchorBinding Anchor;
    Anchor.Feet = FTransform(FRotator(0.0, YawDegrees, 0.0), Feet);
    return Anchor;
}

FNpAnchorBinding RgOffLanePlace(const TCHAR* ExitAnchor)
{
    FNpAnchorBinding Place;
    Place.Kind = ENpBindingKind::OffLane;
    Place.ExitAnchor = FName(ExitAnchor);
    return Place;
}

double RgYawOf(const FVector& From, const FVector& To)
{
    return (To - From).GetSafeNormal2D().Rotation().Yaw;
}

/** The front doorway's yield zone: the closed leaf plus its sweep and approach on both sides, with two waiting spots on the footpath. */
FNpDoorwayZone RgFrontZone(const FLaneBuilding& B, const LI::InteriorLayout& L, const LI::Opening& O, const double DoorU)
{
    FNpDoorwayZone Zone;
    Zone.Id = FName(UTF8_TO_TCHAR((O.DoorId.empty() ? O.Id : O.DoorId).c_str()));
    const double Approach = O.DoorId.empty() ? 65.0 : LI::DoorClearWidthCm + 40.0;
    Zone.Bounds += KitPoint(B, L, O.Plan.U0 - 34.0, O.Plan.V0 - Approach, 0.0);
    Zone.Bounds += KitPoint(B, L, O.Plan.U1 + 34.0, O.Plan.V1 + Approach, 212.0);
    Zone.WaitingFeet = {KitPoint(B, L, DoorU - 150.0, -85.0, 0.0), KitPoint(B, L, DoorU + 150.0, -85.0, 0.0)};
    return Zone;
}
} // namespace

bool MakeRupaBuilding(const TCHAR* BuildingId, FLaneBuilding& Out)
{
    const LI::FrontageEntry* Row = LI::FindFrontageById(std::string(TCHAR_TO_UTF8(BuildingId)));
    if (Row == nullptr || Row->Side != 1) { return false; }
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
    Building.Frame = MakeLaneFrame(Row->OriginXCm, static_cast<double>(Row->FrontAbsYCm), FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0));
    FLaneKitFrontage Kit;
    if (!IsKitFrontage(Building, Kit) || Kit.BuildingId != FName(BuildingId)) { return false; }
    Out = Building;
    return true;
}

FNpBindingProvider NpRupaBindings(const FLaneBuilding& RowHouse)
{
    FLaneKitFrontage Kit;
    if (!IsKitFrontage(RowHouse, Kit) || Kit.BuildingId != FName(RupaBuildingId) || RowHouse.Side != 1)
    {
        return FNpBindingProvider();
    }
    const LI::InteriorLayout Layout = MakeKitFrontageLayout(RowHouse, Kit);
    const LI::Opening* Entrance = Layout.Ok() ? KitFrontEntrance(Layout) : nullptr;
    if (Entrance == nullptr)
    {
        return FNpBindingProvider();
    }
    const double DoorU = 0.5 * (Entrance->Plan.U0 + Entrance->Plan.U1);
    const FVector Doorstep = KitPoint(RowHouse, Layout, DoorU, RupaDoorstepCm, 0.0);
    const double FootZ = Doorstep.Z;
    // Straight out to the walking line, then east along it until the leg is exactly its authored length.
    const double OutCm = Doorstep.Y - RupaFootpathLineY;
    const double EastCm = RupaHomeToWorkCm - OutCm;
    const double WorkX = Doorstep.X + EastCm;
    if (OutCm <= 0.0 || EastCm <= 0.0 || WorkX > LaneX1 - 400.0) { return FNpBindingProvider(); }
    const FVector OnLine(Doorstep.X, RupaFootpathLineY, FootZ);
    const FVector Work(WorkX, RupaFootpathLineY, FootZ);

    FNpBindingProvider Provider;
    Provider.Anchors.Add(FName(RupaHomeFootpath), RgLocalAnchor(Doorstep, RgYawOf(Doorstep, OnLine)));
    Provider.Anchors.Add(FName(RupaWorkFootpath), RgLocalAnchor(Work, RgYawOf(OnLine, Work)));
    // Both places are off-lane; a person appears at, and leaves by, the footpath anchor named here, the first or last point of the route.
    Provider.Anchors.Add(FName(RupaPlaceHome), RgOffLanePlace(RupaHomeFootpath));
    Provider.Anchors.Add(FName(RupaPlaceWork), RgOffLanePlace(RupaWorkFootpath));
    Provider.Routes.Add(FName(RupaRouteHomeToWork), FNpRouteBinding{TArray<FVector>{Doorstep, OnLine, Work}, false});
    Provider.Doorways.Add(RgFrontZone(RowHouse, Layout, *Entrance, DoorU));
    return Provider;
}

bool SpawnRupa(UWorld& World, const FLaneScene& Scene)
{
    if (!HasKitDoor(Scene, RupaDoorId))
    {
        UE_LOG(LogTemp, Warning, TEXT("Rupa: the tea row house was not built; she is not placed."));
        return false;
    }
    FLaneBuilding RowHouse;
    if (!MakeRupaBuilding(RupaBuildingId, RowHouse))
    {
        UE_LOG(LogTemp, Error, TEXT("Rupa: the catalogue row of the row house did not resolve; no fallback spawn."));
        return false;
    }
    FNpBindingProvider Provider = NpRupaBindings(RowHouse);
    if (Provider.Anchors.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Rupa: her bindings could not be derived from the built layout; no fallback spawn."));
        return false;
    }
    UPWNpcPresenceSubsystem* Presence = World.GetSubsystem<UPWNpcPresenceSubsystem>();
    if (Presence == nullptr || !Presence->Bindings().RegisterProvider(FName(RupaProviderId), MoveTemp(Provider)))
    {
        UE_LOG(LogTemp, Error, TEXT("Rupa: NPC provider registration failed; no fallback spawn."));
        return false;
    }
    // TODO(outfit): her own look replaces the placeholder outfit (NR-04); her 157 cm joins the authored heights there.
    if (!Presence->RegisterScheduledPerson(FName(RupaPersonId), NSLOCTEXT("PerfectWorldLane", "RupaName", "Rupa"), EPWNpcOutfit::HouseholdAdult))
    {
        UE_LOG(LogTemp, Error, TEXT("Rupa's schedule registration failed; no fallback spawn."));
        return false;
    }
    if (const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(&World)) { Presence->Reconcile(Life->GetNow()); }
    return true;
}
} // namespace PWLane
