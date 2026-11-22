#include "PerfectWorldSahanaBindings.h"

#include "Engine/World.h"
#include "PerfectWorldLaneKitFrontages.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldSubrataBindings.h"

namespace PWLane
{
namespace
{
FNpAnchorBinding ShLocal(const FVector& Feet, double Yaw)
{
    FNpAnchorBinding Result;
    Result.Feet = FTransform(FRotator(0, Yaw, 0), Feet);
    return Result;
}
FNpAnchorBinding ShOffLane(const TCHAR* Exit)
{
    FNpAnchorBinding Result;
    Result.Kind = ENpBindingKind::OffLane;
    Result.ExitAnchor = FName(Exit);
    return Result;
}
}

FNpBindingProvider NpSahanaBindings(const FLaneBuilding& TallFlats)
{
    FLaneKitFrontage Kit;
    if (!IsKitFrontage(TallFlats, Kit) || Kit.BuildingId != FName(SubrataTallFlatsId) || TallFlats.Side != 1) { return {}; }
    const LI::InteriorLayout Layout = MakeKitFrontageLayout(TallFlats, Kit);
    if (!Layout.Ok() || Layout.Kind != LI::Variant::SharedEntry) { return {}; }
    const LI::Opening* Front = KitFrontEntrance(Layout);
    const LI::Opening* Unit = nullptr;
    const LI::Room* Passage = nullptr;
    for (const auto& Opening : Layout.Openings)
        if (Opening.DoorId == "door.nabapara.tall_flats_unit_b") { Unit = &Opening; }
    for (const auto& Room : Layout.Rooms)
        if (Room.Kind == LI::RoomKind::Passage) { Passage = &Room; }
    if (Front == nullptr || Unit == nullptr || Passage == nullptr) { return {}; }
    const double LineU = 0.5 * (Passage->Area.U0 + Passage->Area.U1) + SahanaPassageOffsetCm;
    const double UnitV = 0.5 * (Unit->Plan.V0 + Unit->Plan.V1);
    // The standing capsule (radius 34) must clear both passage walls and the doorway's own ends.
    if (LineU - Passage->Area.U0 < 34 || Passage->Area.U1 - LineU < 34 ||
        UnitV - Passage->Area.V0 < 34 || Passage->Area.V1 - UnitV < 34) { return {}; }
    const FVector Doorstep = KitPoint(TallFlats, Layout, LineU, UnitV, 0);
    const FVector Threshold = KitPoint(TallFlats, Layout, LineU, -45, 0);
    if (FVector::Dist(Doorstep, Threshold) > 500) { return {}; }
    FNpBindingProvider Result;
    // Unit B lies at higher U than the passage, so she stands facing +U at her door.
    Result.Anchors.Add(FName(SahanaDoorstepId), ShLocal(Doorstep, KitYaw(TallFlats, 1, 0)));
    Result.Anchors.Add(FName(SahanaFrontId), ShLocal(Threshold, KitYaw(TallFlats, 0, 1)));
    Result.Anchors.Add(FName(SahanaHomeId), ShOffLane(SahanaDoorstepId));
    Result.Anchors.Add(FName(SahanaErrandId), ShOffLane(SahanaFrontId));
    Result.Routes.Add(FName(SahanaUnitRoute), FNpRouteBinding{{Doorstep, Threshold}, false});
    // The 1680 cm errand journey is simulated beyond the visible front threshold.
    Result.Routes.Add(FName(SahanaErrandRoute), FNpRouteBinding{{}, true});
    FNpDoorwayZone UnitZone;
    UnitZone.Id = FName(SahanaDoorId);
    UnitZone.Bounds += KitPoint(TallFlats, Layout, Unit->Plan.U0 - 65, Unit->Plan.V0 - 34, 0);
    UnitZone.Bounds += KitPoint(TallFlats, Layout, Unit->Plan.U1 + 130, Unit->Plan.V1 + 34, 212);
    UnitZone.WaitingFeet = {KitPoint(TallFlats, Layout, 0.5 * (Passage->Area.U0 + Passage->Area.U1), UnitV + 170, 0)};
    Result.Doorways.Add(MoveTemp(UnitZone));
    return Result;
}

bool SpawnSahana(UWorld& World, const FLaneScene& Scene)
{
    FLaneBuilding Tall;
    if (!HasKitDoor(Scene, SahanaDoorId) || !HasKitDoor(Scene, SubrataTallFlatsDoorId) ||
        !MakeSubrataBuilding(SubrataTallFlatsId, Tall)) { return false; }
    FNpBindingProvider Provider = NpSahanaBindings(Tall);
    UPWNpcPresenceSubsystem* Presence = World.GetSubsystem<UPWNpcPresenceSubsystem>();
    if (Provider.Anchors.IsEmpty() || Presence == nullptr ||
        !Presence->Bindings().RegisterProvider(FName(SahanaHomeId), MoveTemp(Provider))) { return false; }
    if (!Presence->RegisterScheduledPerson(FName(SahanaPersonId), NSLOCTEXT("PerfectWorldLane", "SahanaName", "Sahana"),
        EPWNpcOutfit::HouseholdAdult)) { return false; }
    if (const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(&World)) { Presence->Reconcile(Life->GetNow()); }
    return true;
}
}
