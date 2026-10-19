#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldLaneBuildings.h"
#include "PerfectWorldLaneInteriorKit.h"
#include "PerfectWorldLaneNpcBindings.h"

class UWorld;

namespace PWLane
{
/**
 * Spatial bindings of the two working proprietors (R4-02): Debashis Pal (171 cm) in the repair workshop and Mahua Saha (162 cm) in
 * the hardware store. Their schedules (npc_schedule.debashis_pal / npc_schedule.mahua_saha) name the kit's own work anchors, so every
 * anchor and route here is derived from the portable interior layout and the building frame (never from fixed world numbers):
 *
 *   Workshop (building.nabapara.repair_workshop, -Y row 3, Workshop set): staff_counter, tool_rack, workbench, finishing_table,
 *     front_door, customer. Kit routes from the counter: counter_to_tool_rack 953, counter_to_workbench 947,
 *     counter_to_finishing_table 1537, counter_to_door 1582. Three more routes are sub-paths of those same polylines (so no
 *     unverified geometry): workbench_to_tool_rack 306, workbench_to_finishing_table 590, door_to_workbench 1399.
 *   Hardware store (building.nabapara.hardware_store, +Y row 8, GeneralGoods set): staff_counter, stock_room, weigh_station,
 *     front_door, customer, browse_shelf. Kit routes: counter_to_stock_room 1135, counter_to_weigh_station 1197, counter_to_door
 *     2100; sub-paths: weigh_station_to_stock_room 158, door_to_stock_room 1505.
 *
 * Every visible polyline must equal its authored length (within 1 cm) or the provider is refused: the presenter's pace of 140 cm per
 * game minute then matches the authored travel time. Homes (location.nabapara.pal_home, location.nabapara.saha_home) are OFF-LANE:
 * each is reached from the shop's own front threshold, so the footpath anchor (the front_door anchor's transform, 45 cm outside the
 * door on the footpath) is where each proprietor appears and leaves, and the home walk (1680 / 1820 cm) is an authored off-lane
 * segment. Both shops' doorways (front and the partition doorway) get yield zones.
 *
 * Integration (the integrator's, PerfectWorldKolkataLane.cpp BuildKolkataLane, after SpawnSahana(World, Scene)):
 *     #include "PerfectWorldProprietorBindings.h"
 *     SpawnDebashis(World, Scene);
 *     SpawnMahua(World, Scene);
 * Roster rows (PerfectWorldNpcRosterTests.cpp RosterPeople, placed by rising probe minute; Monday day 0: Debashis is at the
 * workbench at 10:00, Mahua at the weigh station at 10:50):
 *     {TEXT("person.debashis_pal"), TEXT("Debashis"), 171.0, EPWNpcOutfit::ShopProprietor, true, true, true, TEXT(""), 600},
 *     {TEXT("person.mahua_saha"), TEXT("Mahua"), 162.0, EPWNpcOutfit::ShopProprietor, true, true, true, TEXT(""), 650},
 * Their authored heights and own looks belong to the NpcLooks order (R4-05); until then they use the placeholder ShopProprietor outfit.
 */
inline constexpr const TCHAR* DebashisPersonId = TEXT("person.debashis_pal");
inline constexpr const TCHAR* DebashisBuildingId = TEXT("building.nabapara.repair_workshop");
inline constexpr const TCHAR* DebashisDoorId = TEXT("door.nabapara.repair_workshop_front");
inline constexpr const TCHAR* DebashisHomeId = TEXT("location.nabapara.pal_home");
inline constexpr const TCHAR* DebashisFootpathId = TEXT("location.nabapara.repair_workshop_footpath");
inline constexpr const TCHAR* DebashisAnchorCounter = TEXT("anchor.repair_workshop.staff_counter");
inline constexpr const TCHAR* DebashisAnchorToolRack = TEXT("anchor.repair_workshop.tool_rack");
inline constexpr const TCHAR* DebashisAnchorWorkbench = TEXT("anchor.repair_workshop.workbench");
inline constexpr const TCHAR* DebashisAnchorFinishing = TEXT("anchor.repair_workshop.finishing_table");
inline constexpr const TCHAR* DebashisAnchorFrontDoor = TEXT("anchor.repair_workshop.front_door");
inline constexpr const TCHAR* DebashisAnchorCustomer = TEXT("anchor.repair_workshop.customer");
inline constexpr float DebashisStandingHeightCm = 171.0f;

inline constexpr const TCHAR* MahuaPersonId = TEXT("person.mahua_saha");
inline constexpr const TCHAR* MahuaBuildingId = TEXT("building.nabapara.hardware_store");
inline constexpr const TCHAR* MahuaDoorId = TEXT("door.nabapara.hardware_store_front");
inline constexpr const TCHAR* MahuaHomeId = TEXT("location.nabapara.saha_home");
inline constexpr const TCHAR* MahuaFootpathId = TEXT("location.nabapara.hardware_store_footpath");
inline constexpr const TCHAR* MahuaAnchorCounter = TEXT("anchor.hardware_store.staff_counter");
inline constexpr const TCHAR* MahuaAnchorStockRoom = TEXT("anchor.hardware_store.stock_room");
inline constexpr const TCHAR* MahuaAnchorWeigh = TEXT("anchor.hardware_store.weigh_station");
inline constexpr const TCHAR* MahuaAnchorFrontDoor = TEXT("anchor.hardware_store.front_door");
inline constexpr const TCHAR* MahuaAnchorCustomer = TEXT("anchor.hardware_store.customer");
inline constexpr const TCHAR* MahuaAnchorBrowse = TEXT("anchor.hardware_store.browse_shelf");
inline constexpr float MahuaStandingHeightCm = 162.0f;

/** The provider for the workshop / store frontage as the lane builds it; empty when the building is not that kit row or any length differs from the schedule's. */
FNpBindingProvider NpDebashisBindings(const FLaneBuilding& Workshop);
FNpBindingProvider NpMahuaBindings(const FLaneBuilding& Store);

/** The catalogue building as the lane generator makes it (either side); false when the id is not a kit row. */
bool MakeProprietorBuilding(const TCHAR* BuildingId, FLaneBuilding& Out);

/**
 * Builds the shop from the catalogue, registers the provider and the proprietor (placeholder ShopProprietor outfit) and reconciles to
 * the world clock. Does nothing (and logs) unless the shop's kit door was built into the scene. Returns true when registered.
 */
bool SpawnDebashis(UWorld& World, const FLaneScene& Scene);
bool SpawnMahua(UWorld& World, const FLaneScene& Scene);
} // namespace PWLane
