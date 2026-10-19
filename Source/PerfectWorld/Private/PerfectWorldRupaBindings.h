#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldLaneBuildings.h"
#include "PerfectWorldLaneInteriorKit.h"
#include "PerfectWorldLaneNpcBindings.h"

namespace PWLane
{
/**
 * Rupa Ghosh's spatial bindings (NR-01). Her schedule (npc_schedule.rupa_ghosh) keeps two places and one route: her home
 * location.nabapara.tea_row_house, her workplace location.nabapara.rupa_work, and route.rupa_work.home_to_work (1540 cm, 11
 * minutes at the presenter's 140 cm per game minute). This file turns the kit frontage building.nabapara.tea_row_house (the
 * +Y row house east of the tea shop, entrance mirrored) into the anchors and the polyline the NPC presenter consumes, from the
 * portable layout and the building frame, never from fixed world numbers.
 *
 * Her home is OFF-LANE behind the row house's own front door: the presenter has no pose for sleeping in a bed, cooking or
 * reading in a chair, and nothing in a house is lit for a visible resident. The footpath anchor
 * location.nabapara.tea_row_house_footpath is the doorstep (45 cm outside the door, level with the footpath), exactly where a
 * person leaving a house stands. Everything she does at home is therefore invisible, as for Anindita's sen_home and Subrata's flat;
 * what is visible is that she appears at her door for the visiting hours' comings and goings, and that the door's host-home truth
 * (UPWLaneLifeSubsystem::IsHostHome, which reads her presenter intent) follows her schedule.
 *
 * Her workplace is off-lane too, behind location.nabapara.rupa_work_footpath: a point on the +Y footpath's walking line
 * (world Y 340, the tea shop's own line) east of the row house. The presenter only lets an off-lane person appear at a threshold
 * that is the first point of a visible route and leave at the last (UaResolveArrival / UaResolveDeparture), so the one route is a
 * LOCAL polyline of exactly its authored length:
 *   home_to_work   doorstep -> straight out to the footpath line (Y 340) -> east along that line until the length is 1540 cm.
 * Reversed, it is the walk home. Nothing else is built for her: no stair, no furniture, no seat.
 *
 * Integration (the integrator's, PerfectWorldKolkataLane.cpp BuildKolkataLane, next to SpawnSubrata(World, Scene)):
 *     #include "PerfectWorldRupaBindings.h"
 *     SpawnRupa(World, Scene);
 * Her authored height (157 cm) belongs to PWNpc::StandingHeightCm / the roster (NR-04); until it lands she stands at the generic
 * adult height. Her outfit is the placeholder HouseholdAdult look until NR-04 gives her own.
 */
inline constexpr const TCHAR* RupaPersonId = TEXT("person.rupa_ghosh");
inline constexpr const TCHAR* RupaProviderId = TEXT("building.nabapara.tea_row_house");
inline constexpr const TCHAR* RupaBuildingId = TEXT("building.nabapara.tea_row_house");
inline constexpr const TCHAR* RupaDoorId = TEXT("door.nabapara.tea_row_house_front");

inline constexpr const TCHAR* RupaPlaceHome = TEXT("location.nabapara.tea_row_house");
inline constexpr const TCHAR* RupaPlaceWork = TEXT("location.nabapara.rupa_work");
inline constexpr const TCHAR* RupaHomeFootpath = TEXT("location.nabapara.tea_row_house_footpath");
inline constexpr const TCHAR* RupaWorkFootpath = TEXT("location.nabapara.rupa_work_footpath");
inline constexpr const TCHAR* RupaRouteHomeToWork = TEXT("route.rupa_work.home_to_work");

/** The schedule's frozen route length (NpcScheduleContent.cpp RupaRoutes): met exactly by the built polyline. */
inline constexpr double RupaHomeToWorkCm = 1540.0;

inline constexpr int32 RupaDoorstepCm = -45;      // V of the doorstep (the same threshold V as the stationery and the kit flats)
inline constexpr double RupaFootpathLineY = 340.0; // world Y of the +Y footpath walking line (the tea shop's own line)

/** The row house exactly as the lane generator makes it, from its catalogue row; false when the id is not a +Y kit row. */
bool MakeRupaBuilding(const TCHAR* BuildingId, FLaneBuilding& Out);

/** The provider for the row house: two off-lane places, their two footpath anchors, the one route and the front doorway zone. Empty when refused. */
FNpBindingProvider NpRupaBindings(const FLaneBuilding& RowHouse);

/**
 * Builds the row house from the catalogue, registers the provider and Rupa (outfit placeholder HouseholdAdult) and reconciles to the
 * world clock. Does nothing (and logs) unless the row house's kit door was built into the scene. Returns true when she was registered.
 */
bool SpawnRupa(UWorld& World, const FLaneScene& Scene);
} // namespace PWLane
