#pragma once

#include "PerfectWorldLaneBuildings.h"
#include "PerfectWorldLaneNpcBindings.h"
#include "ShopsR7.h"

class UWorld;

/**
 * R7-04 Unreal half: the five staffed counters of the Kolkata neighbourhood (sweet shop owner and assistant, pharmacist, para-road
 * tea stall, jhalmuri pitch). The portable core (ShopsR7.h) owns identities, hours, counters and route lengths; the R7-01 plan owns
 * the world coordinates of the counters and entrances; the R7-02 shop lots and stalls build the furniture round them. This file
 * only joins them: it registers each staff counter, the planned entrance and the off-lane home with the lane NPC bindings and
 * spawns the people, and it adds one "Purchasing unavailable" browse notice per counter. Nothing here sells, orders or hands over
 * anything, and no entrance is a save door.
 *
 * Integration (the integrator's, PerfectWorldKolkataLane.cpp BuildKolkataLane, after R66SpawnTailor(World, Scene)):
 *     R74SpawnShopStaff(World, Scene);
 *     R74SpawnShopDisplays(World, Scene);
 * and R55IsWorkPilot must also accept IsShopStaffR7(person) before the first call (the spawn refuses otherwise).
 */
namespace PWLane
{
/** The four workplaces (the core's WorkplaceId), sweet shop first. */
inline constexpr const TCHAR* R74SweetShopId = TEXT("location.nabapara.sweet_shop");
inline constexpr const TCHAR* R74PharmacyId = TEXT("location.nabapara.pharmacy");
inline constexpr const TCHAR* R74TeaId = TEXT("pitch.nabapara.tea");
inline constexpr const TCHAR* R74VendorId = TEXT("pitch.nabapara.vendor");

/**
 * One provider per workplace: the entrance and every staff counter as Local anchors (facing +Y toward the para road), each staff
 * member's off-lane home with the entrance as its exit, and the counter-to-entrance route polylines. Returns an EMPTY provider
 * when the plan lacks a feature, a counter or entrance differs from the portable contract, or a derived length differs from the
 * schedule's frozen length by more than 1 cm.
 */
FNpBindingProvider R74ShopBindings(const PerfectWorld::NeighbourhoodPlan& Plan, FName WorkplaceId);

/** The route ids the schedule uses, e.g. route.r74.bimal_modak.counter_entrance and .entrance_home. */
FName R74CounterRouteId(const PerfectWorld::ShopStaffR7& Staff);
FName R74HomeRouteId(const PerfectWorld::ShopStaffR7& Staff);
/** Where the staff member stands, absolute centimetres, feet at the footpath datum. */
FVector R74StaffFeet(const PerfectWorld::ShopStaffR7& Staff);
/** The interaction tag of the workplace's browse notice, e.g. interaction.nabapara.sweet_shop.browse. */
FName R74BrowseTag(const PerfectWorld::ShopStaffR7& Staff);
/** The posted hours as "07:00-13:30 and 15:30-21:00", from the core windows. */
FString R74PostedHoursText(const PerfectWorld::ShopStaffR7& Staff);
/** What the notice says: the display, the posted hours and that purchasing is unavailable. */
FString R74BrowseNotice(const PerfectWorld::ShopStaffR7& Staff);
/** Where the browse interaction sits: on the customer's side of the counter, at counter-top height. */
FVector R74BrowsePoint(const PerfectWorld::ShopStaffR7& Staff);
/**
 * Where a customer stands to talk to the keeper: shops, in the 120 cm service flap of the counter 130 cm from the keeper (the
 * counter would otherwise keep a customer more than the 200 cm reach away; for the assistant, the flap's east half); pitches, in
 * front of the counter 120 cm from the keeper. Every point is within 190 cm of the keeper.
 */
FVector R74CustomerPoint(const PerfectWorld::ShopStaffR7& Staff);
/** True for the person who owns the workplace's planned counter (not the sweet-shop assistant). */
bool R74IsCounterOwner(const PerfectWorld::ShopStaffR7& Staff);

/** Registers the four providers and the five people. False (and nothing registered) on any refusal; no fallback spawn. */
bool R74SpawnShopStaff(UWorld& World, const FLaneScene& Scene);
/** One browse notice per workplace (the owner's counter). Returns how many were placed. */
int32 R74SpawnShopDisplays(UWorld& World, const FLaneScene& Scene);
} // namespace PWLane
