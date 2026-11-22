#pragma once

#include "PerfectWorldLaneNpcBindings.h"
#include "PerfectWorldLaneBuildings.h"

class UWorld;
namespace PWLane
{
inline constexpr const TCHAR* R66TailorPerson = TEXT("person.satyajit_dutta");
inline constexpr const TCHAR* R66TailorBuilding = TEXT("building.nabapara.tailor_shop");
inline constexpr const TCHAR* R66TailorDoor = TEXT("door.nabapara.tailor_shop_front");
FNpBindingProvider R66TailorBindings(const FLaneBuilding& Building);
bool R66SpawnTailor(UWorld& World, const FLaneScene& Scene);
}
