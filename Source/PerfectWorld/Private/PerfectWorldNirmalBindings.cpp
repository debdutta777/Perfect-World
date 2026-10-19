#include "PerfectWorldNirmalBindings.h"

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
FNpAnchorBinding NbLocal(const FVector& Feet, double Yaw)
{
    FNpAnchorBinding Result;
    Result.Feet = FTransform(FRotator(0, Yaw, 0), Feet);
    return Result;
}
FNpAnchorBinding NbOffLane(const TCHAR* Exit)
{
    FNpAnchorBinding Result;
    Result.Kind = ENpBindingKind::OffLane;
    Result.ExitAnchor = FName(Exit);
    return Result;
}
}

FNpBindingProvider NpNirmalBindings(const FLaneBuilding& TallFlats)
{
    FLaneKitFrontage Kit;
    if (!IsKitFrontage(TallFlats, Kit) || Kit.BuildingId != FName(SubrataTallFlatsId) || TallFlats.Side != 1) { return {}; }
    const LI::InteriorLayout Layout = MakeKitFrontageLayout(TallFlats, Kit);
    if (!Layout.Ok() || Layout.Kind != LI::Variant::SharedEntry) { return {}; }
    const LI::Opening* Front = KitFrontEntrance(Layout);
    const LI::Opening* Unit = nullptr;
    const LI::Room* Passage = nullptr;
    for (const auto& Opening : Layout.Openings)
        if (Opening.DoorId == "door.nabapara.tall_flats_unit_a") { Unit = &Opening; }
    for (const auto& Room : Layout.Rooms)
        if (Room.Kind == LI::RoomKind::Passage) { Passage = &Room; }
    if (Front == nullptr || Unit == nullptr || Passage == nullptr) { return {}; }
    const double CentreU = 0.5 * (Passage->Area.U0 + Passage->Area.U1);
    const double UnitV = 0.5 * (Unit->Plan.V0 + Unit->Plan.V1);
    if (CentreU - Passage->Area.U0 < 34 || Passage->Area.U1 - CentreU < 34 ||
        UnitV - Passage->Area.V0 < 34 || Passage->Area.V1 - UnitV < 34) { return {}; }
    const FVector Doorstep = KitPoint(TallFlats, Layout, CentreU, UnitV, 0);
    const FVector Threshold = KitPoint(TallFlats, Layout, CentreU, -45, 0);
    if (FVector::Dist(Doorstep, Threshold) > 500) { return {}; }
    FNpBindingProvider Result;
    Result.Anchors.Add(FName(NirmalDoorstepId), NbLocal(Doorstep, KitYaw(TallFlats, -1, 0)));
    Result.Anchors.Add(FName(NirmalFrontId), NbLocal(Threshold, KitYaw(TallFlats, 0, 1)));
    Result.Anchors.Add(FName(NirmalHomeId), NbOffLane(NirmalDoorstepId));
    Result.Anchors.Add(FName(NirmalWorkId), NbOffLane(NirmalFrontId));
    Result.Routes.Add(FName(NirmalUnitRoute), FNpRouteBinding{{Doorstep, Threshold}, false});
    // The 2800 cm work journey is simulated beyond the visible front threshold.
    Result.Routes.Add(FName(NirmalWorkRoute), FNpRouteBinding{{}, true});
    FNpDoorwayZone UnitZone;
    UnitZone.Id = FName(NirmalDoorId);
    UnitZone.Bounds += KitPoint(TallFlats, Layout, Unit->Plan.U0 - 130, Unit->Plan.V0 - 34, 0);
    UnitZone.Bounds += KitPoint(TallFlats, Layout, Unit->Plan.U1 + 65, Unit->Plan.V1 + 34, 212);
    UnitZone.WaitingFeet = {KitPoint(TallFlats, Layout, CentreU, UnitV + 170, 0)};
    Result.Doorways.Add(MoveTemp(UnitZone));
    return Result;
}

bool SpawnNirmal(UWorld& World, const FLaneScene& Scene)
{
    FLaneBuilding Tall;
    if (!HasKitDoor(Scene, NirmalDoorId) || !HasKitDoor(Scene, SubrataTallFlatsDoorId) ||
        !MakeSubrataBuilding(SubrataTallFlatsId, Tall)) { return false; }
    FNpBindingProvider Provider = NpNirmalBindings(Tall);
    UPWNpcPresenceSubsystem* Presence = World.GetSubsystem<UPWNpcPresenceSubsystem>();
    if (Provider.Anchors.IsEmpty() || Presence == nullptr ||
        !Presence->Bindings().RegisterProvider(FName(NirmalHomeId), MoveTemp(Provider))) { return false; }
    if (!Presence->RegisterScheduledPerson(FName(NirmalPersonId), NSLOCTEXT("PerfectWorldLane", "NirmalName", "Nirmal"),
        EPWNpcOutfit::HouseholdAdult)) { return false; }
    if (const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(&World)) { Presence->Reconcile(Life->GetNow()); }
    return true;
}
}
