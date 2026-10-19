#pragma once

#include "PerfectWorldLaneBuildings.h"
#include "PerfectWorldLaneNpcBindings.h"

class UWorld;

namespace PWLane
{
/**
 * Sahana Mitra's spatial bindings (NR-03). Her schedule (npc_schedule.sahana_mitra) keeps three places and two routes: home
 * location.nabapara.tall_flats_unit_b, the public front threshold anchor.sahana.tall_flats_front, and her off-lane errand
 * location.nabapara.sahana_errand. She lives exclusively in unit B, mirroring Nirmal's unit A (NR-02): home is off-lane behind
 * unit B's hung door, its exit anchor is INSIDE the shared passage opposite that door (16 cm toward unit B of the passage
 * centre line, so her doorstep never coincides with Nirmal's), and the visible local route follows the passage to the public
 * front threshold (500 cm ceiling). The 1680 cm errand route lies beyond the threshold and is simulated off-lane.
 *
 * Integration (the integrator's, PerfectWorldKolkataLane.cpp BuildKolkataLane, after SpawnNirmal(World, Scene)):
 *     #include "PerfectWorldSahanaBindings.h"
 *     SpawnSahana(World, Scene);
 * Roster row (PerfectWorldNpcRosterTests.cpp RosterPeople), her authored height and looks already exist in NpcLooks:
 *     {TEXT("person.sahana_mitra"), TEXT("Sahana"), 164.0, EPWNpcOutfit::HouseholdAdult, false, true, true, TEXT(""), 0},
 */
inline constexpr const TCHAR* SahanaPersonId = TEXT("person.sahana_mitra");
inline constexpr const TCHAR* SahanaHomeId = TEXT("location.nabapara.tall_flats_unit_b");
inline constexpr const TCHAR* SahanaDoorId = TEXT("door.nabapara.tall_flats_unit_b");
inline constexpr const TCHAR* SahanaFrontId = TEXT("anchor.sahana.tall_flats_front");
inline constexpr const TCHAR* SahanaDoorstepId = TEXT("anchor.sahana.unit_b_doorstep");
inline constexpr const TCHAR* SahanaErrandId = TEXT("location.nabapara.sahana_errand");
inline constexpr const TCHAR* SahanaUnitRoute = TEXT("route.sahana.unit_to_front");
inline constexpr const TCHAR* SahanaErrandRoute = TEXT("route.sahana.front_to_errand");
inline constexpr float SahanaStandingHeightCm = 164.0f;
/** Her passage line sits this far from the centre line toward unit B (Nirmal's is on the centre line). */
inline constexpr double SahanaPassageOffsetCm = 16.0;

/** The provider for the tall-flats frontage; empty when the frontage is not the +Y kit tall flats with a shared-entry layout. */
FNpBindingProvider NpSahanaBindings(const FLaneBuilding& TallFlats);
bool SpawnSahana(UWorld& World, const FLaneScene& Scene);
}
