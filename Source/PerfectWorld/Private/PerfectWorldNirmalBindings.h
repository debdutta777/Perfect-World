#pragma once

#include "PerfectWorldLaneBuildings.h"
#include "PerfectWorldLaneNpcBindings.h"

class UWorld;

namespace PWLane
{
inline constexpr const TCHAR* NirmalPersonId = TEXT("person.nirmal_bose");
inline constexpr const TCHAR* NirmalHomeId = TEXT("location.nabapara.tall_flats_unit_a");
inline constexpr const TCHAR* NirmalDoorId = TEXT("door.nabapara.tall_flats_unit_a");
inline constexpr const TCHAR* NirmalFrontId = TEXT("anchor.nirmal.tall_flats_front");
inline constexpr const TCHAR* NirmalDoorstepId = TEXT("anchor.nirmal.unit_a_doorstep");
inline constexpr const TCHAR* NirmalWorkId = TEXT("location.nabapara.nirmal_work");
inline constexpr const TCHAR* NirmalUnitRoute = TEXT("route.nirmal.unit_to_front");
inline constexpr const TCHAR* NirmalWorkRoute = TEXT("route.nirmal.front_to_work");
inline constexpr float NirmalStandingHeightCm = 174.0f;

// Home is off-lane behind unit A's hung door. Its exit anchor is INSIDE the passage, opposite that door.
// The local home->front route follows the passage centre to the public front threshold.
FNpBindingProvider NpNirmalBindings(const FLaneBuilding& TallFlats);
bool SpawnNirmal(UWorld& World, const FLaneScene& Scene);
}
