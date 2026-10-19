#include "Doors.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace PerfectWorld;

namespace
{
int R1Assertions = 0;
void R1Expect(bool condition, const std::string& message)
{
    ++R1Assertions;
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

constexpr std::array<std::string_view, 23> R1FrozenIds{
    "door.nabapara.home_front", "door.nabapara.home_living", "door.nabapara.home_bedroom", "door.nabapara.home_kitchen",
    "door.nabapara.home_washroom", "door.nabapara.shanti_chai_shutter", "door.nabapara.care_front", "door.nabapara.care_dormitory",
    "door.nabapara.care_common", "door.nabapara.care_washroom", "door.nabapara.stationery_front", "door.nabapara.caretaker_flat_front",
    "door.nabapara.tall_flats_front", "door.nabapara.tall_flats_unit_a", "door.nabapara.tall_flats_unit_b",
    "door.nabapara.tea_row_house_front", "door.nabapara.repair_workshop_front", "door.nabapara.hardware_store_front",
    "door.nabapara.old_house_front", "door.nabapara.tall_flats_upper_a", "door.nabapara.market_stalls_a_front",
    "door.nabapara.market_stalls_b_front", "door.nabapara.end_shop_front"};

std::string R1Text(const DoorState& state, std::size_t count)
{
    std::string text = "pw.doors/2\ncount=" + std::to_string(count) + "\n";
    for (std::size_t i = 0; i < std::min(count, state.Doors.size()); ++i)
    {
        const auto& entry = state.Doors[i];
        text += "door=" + entry.Id + (entry.Open ? ",1," : ",0,") + std::to_string(entry.SwingSide) + "\n";
    }
    return text + "end\n";
}

void R1DefinitionsAndAccess()
{
    const auto doors = AuthoredDoors();
    R1Expect(doors.size() == 23 && ValidateDoorTable(), "twenty-three valid doors");
    for (std::size_t i = 0; i < doors.size(); ++i)
        R1Expect(doors[i].Id == R1FrozenIds[i], "the frozen twenty-door prefix and append order");
    const std::array<std::string_view, 3> locations{
        "location.nabapara.market_stalls_a", "location.nabapara.market_stalls_b", "location.nabapara.end_shop"};
    const std::int8_t R1InwardSides[]{-1, 1, -1}; // kit front leaf runs along +X; actor local +Y points toward +Y
    for (std::size_t i = 20; i < 23; ++i)
    {
        const auto& door = doors[i];
        R1Expect(door.LocationId == locations[i - 20] && door.Access == DoorAccess::BusinessHours &&
            !door.bInitiallyOpen && !door.bTwoWay && door.PreferredSwingSide == R1InwardSides[i - 20] && door.VisitingWindows.empty(),
            "public business browsing, initially closed and one-way inward");
        R1Expect(door.OpenMinute == 0 && door.CloseMinute == 0 && door.OpeningWindows.size() == 2 &&
            door.OpeningWindows[0].OpenMinute == 540 && door.OpeningWindows[0].CloseMinute == 780 &&
            door.OpeningWindows[1].OpenMinute == 900 && door.OpeningWindows[1].CloseMinute == 1170,
            "posted browsing hours 09:00-13:00, 15:00-19:30");
        for (int minute = 0; minute < DoorMinutesPerDay; ++minute)
        {
            const bool browsing = (minute >= 540 && minute < 780) || (minute >= 900 && minute < 1170);
            const auto permission = browsing ? DoorResult::Ok : DoorResult::Closed;
            R1Expect(EvaluateDoorPassage(door, minute, {}) == permission, "entry follows half-open browsing hours");
            R1Expect(EvaluateDoorPassage(door, minute, {DoorDirection::Exit}) == DoorResult::Ok, "egress at every minute");
            R1Expect(EvaluateDoorOpen(door, true, true, minute, {}) == (browsing ? DoorResult::NoChange : DoorResult::Closed),
                "an open physical leaf never grants closed-hours entry");
            R1Expect(EvaluateDoorOpen(door, true, false, minute, {}) == DoorResult::Ok, "closing always permitted");
        }
        auto state = MakeInitialDoorState();
        const auto before = state;
        R1Expect(TrySetDoorOpen(state, door.Id, true, 539, false) == DoorResult::Closed && state == before,
            "refused entry changes nothing");
        R1Expect(ApplyDoorSettlement(state, door.Id, true, -door.PreferredSwingSide) == DoorResult::InvalidState && state == before,
            "outward settlement refused atomically");
        R1Expect(TrySetDoorOpen(state, door.Id, true, 630, false) == DoorResult::Ok && state.Doors[i].SwingSide == door.PreferredSwingSide,
            "visitor opens inward during browsing");
    }
}

void R1Migration()
{
    const auto initial = MakeInitialDoorState();
    auto played = initial;
    // Exercise every old leaf, not only the original home. Both permitted sides and closed states must survive.
    for (std::size_t i = 0; i < 20; ++i)
    {
        const auto& def = AuthoredDoors()[i];
        const bool open = i % 3 != 1;
        const std::int8_t side = !open ? 0 : def.bTwoWay && i % 2 == 0 ? -1 : def.PreferredSwingSide;
        const auto result = ApplyDoorSettlement(played, def.Id, open, side);
        R1Expect(result == DoorResult::Ok || result == DoorResult::NoChange, "old physical fixture authored legally");
    }
    const auto twenty = R1Text(played, 20);
    const auto migrated = DeserialiseDoorState(twenty);
    R1Expect(migrated.Ok() && migrated.bMigrated && migrated.State == played, "20->23 preserves every old pair, adds only defaults");
    for (std::size_t i = 20; i < 23; ++i)
        R1Expect(migrated.State.Doors[i] == DoorEntry{std::string(R1FrozenIds[i]), false, 0}, "new leaves alone start closed");
    for (std::size_t count = 0; count <= 24; ++count)
    {
        const auto result = DeserialiseDoorState(R1Text(played, count));
        const bool realEdition = count == 10 || count == 11 || count == 16 || count == 20 || count == 23;
        R1Expect(result.Ok() == realEdition, "only real /2 roster editions accepted");
        if (!realEdition) R1Expect(result.State == DoorState{} && !result.bMigrated, "invalid roster publishes no state");
    }
    for (std::size_t length = 0; length < twenty.size(); ++length)
    {
        const auto result = DeserialiseDoorState(std::string_view(twenty).substr(0, length));
        R1Expect(!result.Ok() && result.State == DoorState{} && !result.bMigrated, "every truncated twenty-door migration refused whole");
    }
    for (const auto replacement : {R1FrozenIds[18], R1FrozenIds[20], std::string_view("door.nabapara.unknown")})
    {
        std::string damaged = twenty;
        damaged.replace(damaged.find(R1FrozenIds[19]), R1FrozenIds[19].size(), replacement);
        R1Expect(!DeserialiseDoorState(damaged).Ok(), "duplicate, gap and unknown door refused");
    }
    auto current = migrated.State;
    R1Expect(TrySetDoorOpen(current, R1FrozenIds[20], true, 630, false) == DoorResult::Ok &&
        TrySetDoorOpen(current, R1FrozenIds[22], true, 630, false) == DoorResult::Ok, "mixed new door states");
    const auto roundTrip = DeserialiseDoorState(SerialiseDoorState(current));
    R1Expect(roundTrip.Ok() && !roundTrip.bMigrated && roundTrip.State == current, "current edition round trips idempotently");
}
} // namespace

int main()
{
    R1DefinitionsAndAccess();
    R1Migration();
    std::cout << "DoorsRoster23Tests: " << R1Assertions << " assertions passed\n";
    return 0;
}
