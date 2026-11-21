#pragma once

#include "PerfectWorldLaneBuildings.h"
#include "PerfectWorldLaneNpcBindings.h"

class UWorld;

namespace PWLane
{
inline constexpr const TCHAR* R52Prabir = TEXT("person.prabir_chatterjee");
inline constexpr const TCHAR* R52Ishita = TEXT("person.ishita_banerjee");
inline constexpr const TCHAR* R52OldHouse = TEXT("building.nabapara.old_house");
inline constexpr const TCHAR* R52TallFlats = TEXT("building.nabapara.tall_flats");
inline constexpr const TCHAR* R52PrabirHome = TEXT("location.nabapara.old_house");
inline constexpr const TCHAR* R52IshitaHome = TEXT("location.nabapara.tall_flats_upper_a");
inline constexpr const TCHAR* R52OldDoor = TEXT("door.nabapara.old_house_front");
inline constexpr const TCHAR* R52UpperDoor = TEXT("door.nabapara.tall_flats_upper_a");
inline constexpr const TCHAR* R52SharedDoor = TEXT("door.nabapara.tall_flats_front");
inline constexpr const TCHAR* R52PrabirFront = TEXT("anchor.prabir.old_house_front");
inline constexpr const TCHAR* R52IshitaFront = TEXT("anchor.ishita.tall_flats_front");
inline constexpr const TCHAR* R52PrabirAway = TEXT("location.nabapara.prabir_errands");
inline constexpr const TCHAR* R52IshitaAway = TEXT("location.nabapara.ishita_work");
inline constexpr const TCHAR* R52PrabirRoute = TEXT("route.prabir.home_to_errands");
inline constexpr const TCHAR* R52UpperRoute = TEXT("route.ishita.upper_to_front");
inline constexpr const TCHAR* R52WorkRoute = TEXT("route.ishita.front_to_work");

// Home IDs remain schedule zones. Furniture anchors supply real standing positions, never replacement zone IDs.
FNpBindingProvider NpPrabirR5Bindings(const FLaneBuilding& OldHouse);
FNpBindingProvider NpIshitaR5Bindings(const FLaneBuilding& TallFlats);
double ResidentsR5RouteLength(const FNpRouteBinding& Route);
bool SpawnResidentsR5(UWorld& World, const FLaneScene& Scene);
}
