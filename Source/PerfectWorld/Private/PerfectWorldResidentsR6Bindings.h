#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldLaneBuildings.h"
#include "PerfectWorldLaneNpcBindings.h"
#include "PerfectWorldNpcCharacter.h"

class UWorld;

/**
 * R6-04: the two remaining private homes get a resident each, exactly as R5-01/R5-02 did for the old house. Madhabi Das (58,
 * 156 cm, municipal records clerk) lives in building.nabapara.alley_house; Arup Basu (34, 179 cm, parcel dispatch supervisor)
 * in building.nabapara.stair_flats. Both are ground-floor CompactResidence kit frontages on the -Y side, so the interior is the
 * old house's: front door, a living room, an internal doorway on the same axis and a sleeping room with the free standing
 * bed_side anchor. Each schedule has ONE away zone behind the home's own front door; the visible stretch (bed_side, across the
 * sleeping room, down the doorway axis, out to a threshold 45 cm outside the door) is far shorter than the authored off-lane
 * allowance and is never padded.
 *
 * Integration (PerfectWorldKolkataLane.cpp BuildKolkataLane, after SpawnResidentsR5): `SpawnResidentsR6(World, Scene);`
 */
namespace PWLane
{
struct FR64Home
{
    const TCHAR* Person;
    const TCHAR* Name;
    const TCHAR* Building;
    const TCHAR* HomeLocation;
    const TCHAR* Door;
    const TCHAR* FrontAlias;
    const TCHAR* AwayLocation;
    const TCHAR* Route;
    EPWNpcOutfit Outfit;
    double HeightCm;
    double AuthoredRouteCm;
};

const FR64Home& R64MadhabiHome();
const FR64Home& R64ArupHome();

/**
 * The home's bindings: bed_side as the home zone, every kit anchor, the threshold alias, the away zone off-lane behind that
 * threshold, the one visible route and the front doorway yield zone. Empty when Building is not that home's catalogue frame.
 */
FNpBindingProvider NpResidentsR6Bindings(const FLaneBuilding& Building, const FR64Home& Home);
double ResidentsR6RouteLength(const FNpRouteBinding& Route);
/** Builds the home's FLaneBuilding from its catalogue row (either lane side). */
bool MakeResidentsR6Building(const FR64Home& Home, FLaneBuilding& Out);
bool SpawnResidentsR6(UWorld& World, const FLaneScene& Scene);
}
