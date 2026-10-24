#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldLaneBuildings.h"
#include "PerfectWorldLaneInteriorKit.h"
#include "PerfectWorldLaneNpcBindings.h"

namespace PWLane
{
/**
 * Subrata Dey's spatial bindings (UPGRADE_PLAN UP-03b). His schedule (npc_schedule.subrata_dey) names the shared entry of the tall
 * flats (front door, noticeboard, stair), the off-lane places "lane_round" and "caretaker_flat", and four routes. This file turns
 * the two kit frontages (building.nabapara.tall_flats on +Y and building.nabapara.caretaker_flat on -Y) into the anchors and
 * polylines the NPC presenter consumes, from the portable layouts and the building frames, never from fixed world numbers.
 *
 * Tall flats (all on the centre line of the 130 cm stair passage, 65 cm from both partitions; the passage is a dead end):
 *   anchor.tall_flats.front_door    the doorstep, 45 cm outside the front face, facing into the passage.
 *   anchor.tall_flats.noticeboard   in the passage 305 cm behind the front wall's inner face, 20 cm toward the +U partition (45 cm from it).
 *   anchor.tall_flats.stair         75 cm short of the back wall: the foot of the stair, which this file does not build. The UP-06 stair
 *                                   fixture may move it; change SubrataStairFootBackCm and the two tall-flats route lengths follow.
 *
 * The caretaker's flat is a BUILT kit residence with a hung front door, but the presenter has no pose for sleeping in a bed or
 * reading in a chair and nothing in a flat is lit or furnished for a visible resident, so his home is OFF-LANE behind his own front
 * door: the footpath anchor location.nabapara.caretaker_flat_footpath is the flat's doorstep (45 cm outside the door), exactly where a
 * person leaving a flat would stand. Everything the schedule does at home is therefore invisible, as for Anindita's sen_home.
 *
 * The lane round is off-lane too, behind location.nabapara.lane_round_footpath: a point on the +Y footpath in front of the gap east
 * of the tea shop, 2200 cm of walking from the tall flats' doorstep. The two long routes are LOCAL polylines of exactly their
 * authored lengths (2400 and 2200 cm), because the presenter only lets an off-lane person appear at a threshold that is the first
 * point of a visible route (UaResolveArrival) and leave at the last:
 *   home_to_lane_round     doorstep -> along the -Y footpath 85 cm east -> across the road -> west along the road -> up to the +Y footpath.
 *   lane_round_to_entry    that footpath point -> west along the +Y footpath line (the tea shop's own walking line, Y 340) -> the tall flats' doorstep.
 * The two tall-flats routes are shorter than the schedule's 500 cm ceilings (the passage has only 745 cm from doorstep to stair).
 *
 * Integration (the integrator's, PerfectWorldKolkataLane.cpp BuildKolkataLane, next to PbSpawnAnindita(World, Scene)):
 *     #include "PerfectWorldSubrataBindings.h"
 *     SpawnSubrata(World, Scene);
 */
inline constexpr const TCHAR* SubrataPersonId = TEXT("person.subrata_dey");
inline constexpr const TCHAR* SubrataProviderId = TEXT("building.nabapara.tall_flats");
inline constexpr const TCHAR* SubrataTallFlatsId = TEXT("building.nabapara.tall_flats");
inline constexpr const TCHAR* SubrataCaretakerFlatId = TEXT("building.nabapara.caretaker_flat");
inline constexpr const TCHAR* SubrataTallFlatsDoorId = TEXT("door.nabapara.tall_flats_front");
inline constexpr const TCHAR* SubrataCaretakerDoorId = TEXT("door.nabapara.caretaker_flat_front");

inline constexpr const TCHAR* SubrataAnchorFrontDoor = TEXT("anchor.tall_flats.front_door");
inline constexpr const TCHAR* SubrataAnchorNoticeboard = TEXT("anchor.tall_flats.noticeboard");
inline constexpr const TCHAR* SubrataAnchorStair = TEXT("anchor.tall_flats.stair");
inline constexpr const TCHAR* SubrataPlaceHome = TEXT("location.nabapara.caretaker_flat");
inline constexpr const TCHAR* SubrataPlaceLaneRound = TEXT("location.nabapara.lane_round");
inline constexpr const TCHAR* SubrataHomeFootpath = TEXT("location.nabapara.caretaker_flat_footpath");
inline constexpr const TCHAR* SubrataLaneRoundFootpath = TEXT("location.nabapara.lane_round_footpath");
inline constexpr const TCHAR* SubrataRouteHomeToLaneRound = TEXT("route.caretaker.home_to_lane_round");
inline constexpr const TCHAR* SubrataRouteLaneRoundToEntry = TEXT("route.caretaker.lane_round_to_entry");
inline constexpr const TCHAR* SubrataRouteEntryToNoticeboard = TEXT("route.tall_flats.entry_to_noticeboard");
inline constexpr const TCHAR* SubrataRouteNoticeboardToStair = TEXT("route.tall_flats.noticeboard_to_stair");

/** The schedule's frozen route lengths (NpcScheduleContent.cpp SubrataRoutes). The two lane legs are met exactly; the two passage legs are ceilings. */
inline constexpr double SubrataHomeToLaneRoundCm = 2400.0;
inline constexpr double SubrataLaneRoundToEntryCm = 2200.0;
inline constexpr double SubrataEntryToNoticeboardCm = 500.0;
inline constexpr double SubrataNoticeboardToStairCm = 500.0;

inline constexpr int32 SubrataDoorstepCm = -45;        // V of both doorsteps (the stationery's threshold V)
inline constexpr int32 SubrataFootpathLineV = -120;    // V of the footpath walking line (+Y: world Y 340, the tea shop's line)
inline constexpr double SubrataRoadLineY = 80.0;       // world Y of the road crossing line: clear of the taxi (Y -222..-57) and both scooters
inline constexpr int32 SubrataNoticeboardFrontCm = 305; // passage V0 + this = the noticeboard stand V
inline constexpr int32 SubrataNoticeboardSideCm = 20;   // toward the +U partition
inline constexpr int32 SubrataStairFootBackCm = 75;     // passage V1 - this = the stair-foot stand V

/** The building exactly as the lane generator makes it, from its catalogue row; false when the id is not a kit row. */
bool MakeSubrataBuilding(const TCHAR* BuildingId, FLaneBuilding& Out);

/**
 * The provider for the tall flats and the caretaker's flat (one provider: the routes join them). Returns an empty provider when
 * either building is not the catalogue's, the layouts are refused, or the passage cannot hold the three anchors.
 */
FNpBindingProvider NpSubrataBindings(const FLaneBuilding& TallFlats, const FLaneBuilding& CaretakerFlat);

/**
 * Builds both buildings from the catalogue, registers the provider and Subrata (outfit placeholder HouseholdAdult, the sage
 * overshirt; his 181 cm height comes from PWNpc::StandingHeightCm for his person id) and reconciles to the world clock. Does nothing
 * (and logs) unless both kit doors were built into the scene. Returns true when he was registered.
 */
bool SpawnSubrata(UWorld& World, const FLaneScene& Scene);
} // namespace PWLane
