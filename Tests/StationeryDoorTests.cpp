#include "Doors.h"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace PerfectWorld;
namespace
{
void StationeryRequire(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

// Frozen ten-door /2 fixture: both open swing signs and a changed public leaf.
constexpr std::string_view StationeryOldDoors =
    "pw.doors/2\ncount=10\n"
    "door=door.nabapara.home_front,1,-1\n"
    "door=door.nabapara.home_living,0,0\n"
    "door=door.nabapara.home_bedroom,1,1\n"
    "door=door.nabapara.home_kitchen,1,1\n"
    "door=door.nabapara.home_washroom,0,0\n"
    "door=door.nabapara.shanti_chai_shutter,0,0\n"
    "door=door.nabapara.care_front,0,0\n"
    "door=door.nabapara.care_dormitory,0,0\n"
    "door=door.nabapara.care_common,1,1\n"
    "door=door.nabapara.care_washroom,0,0\nend\n";
}

int main()
{
    constexpr std::string_view id = "door.nabapara.stationery_front";
    const auto* shop = FindDoor(id);
    StationeryRequire(shop && shop->Access == DoorAccess::BusinessHours &&
                          shop->LocationId == "location.nabapara.stationery", "authored stationery business door");
    for (int minute = 0; minute < 1440; ++minute)
    {
        const bool open = (minute >= 480 && minute < 780) || (minute >= 900 && minute < 1170);
        const auto expected = open ? DoorResult::Ok : DoorResult::Closed;
        StationeryRequire(EvaluateDoorPassage(*shop, minute, {}) == expected, "public entry follows shop hours");
        StationeryRequire(EvaluateDoorPassage(*shop, minute, {DoorDirection::Entry, true}) == DoorResult::Ok,
                          "Anindita uses staff access during stock work and reconciliation");
        DoorState state = MakeInitialDoorState();
        const auto before = state;
        StationeryRequire(TrySetDoorOpen(state, id, true, minute, {}) == expected, "entry mutation follows hours");
        if (!open)
            StationeryRequire(state == before, "closed-hours entry refusal is atomic");
        StationeryRequire(TrySetDoorOpen(state, id, true, minute, {DoorDirection::Exit}) ==
                              (open ? DoorResult::NoChange : DoorResult::Ok), "a person inside can open the door to exit");
        StationeryRequire(EvaluateDoorOffer(*shop, true, minute, {DoorDirection::Exit}).Action == DoorAction::Close,
                          "exit context also permits closing an open leaf");
    }
    const auto migrated = DeserialiseDoorState(StationeryOldDoors);
    StationeryRequire(migrated.Ok() && migrated.bMigrated && IsDoorStateValid(migrated.State), "ten-door /2 migrates");
    StationeryRequire(migrated.State.Doors[0].SwingSide == -1 && migrated.State.Doors[2].SwingSide == 1 &&
                          !migrated.State.Doors[1].Open, "historical physical facts survive");
    StationeryRequire(QueryDoorOpen(migrated.State, id) == false && migrated.State.Doors.back().SwingSide == 0,
                      "stationery starts at its authored closed default");
    const auto current = DeserialiseDoorState(SerialiseDoorState(migrated.State));
    StationeryRequire(current.Ok() && !current.bMigrated && current.State == migrated.State, "resave migration is idempotent");
    for (std::size_t cut = 0; cut < StationeryOldDoors.size(); ++cut)
    {
        const auto refused = DeserialiseDoorState(StationeryOldDoors.substr(0, cut));
        StationeryRequire(!refused.Ok() && !refused.bMigrated && refused.State == DoorState{}, "truncated migration is atomic");
    }
    StationeryRequire(!DeserialiseDoorState(std::string(StationeryOldDoors) + "x").Ok(), "trailing migration data refused");
    std::cout << "PASS: stationery door hours, egress and historical migration\n";
}
