#include "Doors.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace PerfectWorld;

namespace
{
int R53Assertions = 0;
void R53Expect(bool condition, const std::string& message)
{
    ++R53Assertions;
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

constexpr std::array<std::string_view, 26> R53FrozenIds{
    "door.nabapara.home_front", "door.nabapara.home_living", "door.nabapara.home_bedroom", "door.nabapara.home_kitchen",
    "door.nabapara.home_washroom", "door.nabapara.shanti_chai_shutter", "door.nabapara.care_front", "door.nabapara.care_dormitory",
    "door.nabapara.care_common", "door.nabapara.care_washroom", "door.nabapara.stationery_front", "door.nabapara.caretaker_flat_front",
    "door.nabapara.tall_flats_front", "door.nabapara.tall_flats_unit_a", "door.nabapara.tall_flats_unit_b",
    "door.nabapara.tea_row_house_front", "door.nabapara.repair_workshop_front", "door.nabapara.hardware_store_front",
    "door.nabapara.old_house_front", "door.nabapara.tall_flats_upper_a", "door.nabapara.market_stalls_a_front",
    "door.nabapara.market_stalls_b_front", "door.nabapara.end_shop_front",
    "door.nabapara.cloth_merchant_front", "door.nabapara.narrow_shop_front", "door.nabapara.tailor_shop_front"};

std::string R53Text(const DoorState& state, std::size_t count)
{
    std::string text = "pw.doors/2\ncount=" + std::to_string(count) + "\n";
    for (std::size_t i = 0; i < std::min(count, state.Doors.size()); ++i)
    {
        const auto& entry = state.Doors[i];
        text += "door=" + entry.Id + (entry.Open ? ",1," : ",0,") + std::to_string(entry.SwingSide) + "\n";
    }
    return text + "end\n";
}

void R53DefinitionsAndAccess()
{
    const auto doors = AuthoredDoors();
    R53Expect(doors.size() == 29 && ValidateDoorTable(), "twenty-six-door prefix in the current valid roster");
    for (std::size_t i = 0; i < R53FrozenIds.size(); ++i)
        R53Expect(doors[i].Id == R53FrozenIds[i], "the frozen twenty-three-door prefix and append order");
    const std::array<std::string_view, 3> locations{
        "location.nabapara.cloth_merchant", "location.nabapara.narrow_shop", "location.nabapara.tailor_shop"};
    const std::int8_t R53InwardSides[]{-1, -1, 1}; // kit front leaf runs along +X; actor local +Y points toward +Y
    for (std::size_t i = 23; i < 26; ++i)
    {
        const auto& door = doors[i];
        R53Expect(door.LocationId == locations[i - 23] && door.Access == DoorAccess::BusinessHours &&
            !door.bInitiallyOpen && !door.bTwoWay && door.PreferredSwingSide == R53InwardSides[i - 23] && door.VisitingWindows.empty(),
            "public business browsing, initially closed and one-way inward");
        R53Expect(door.OpenMinute == 0 && door.CloseMinute == 0 && door.OpeningWindows.size() == 2 &&
            door.OpeningWindows[0].OpenMinute == (i == 24 ? 540 : 600) && door.OpeningWindows[0].CloseMinute == 780 &&
            door.OpeningWindows[1].OpenMinute == 900 && door.OpeningWindows[1].CloseMinute == (i == 23 ? 1200 : 1170),
            "trade-specific split-day browsing hours");
        for (int minute = 0; minute < DoorMinutesPerDay; ++minute)
        {
            const bool browsing = (minute >= (i == 24 ? 540 : 600) && minute < 780) || (minute >= 900 && minute < (i == 23 ? 1200 : 1170));
            const auto permission = browsing ? DoorResult::Ok : DoorResult::Closed;
            R53Expect(EvaluateDoorPassage(door, minute, {}) == permission, "entry follows half-open browsing hours");
            R53Expect(EvaluateDoorPassage(door, minute, {DoorDirection::Exit}) == DoorResult::Ok, "egress at every minute");
            R53Expect(EvaluateDoorOpen(door, true, true, minute, {}) == (browsing ? DoorResult::NoChange : DoorResult::Closed),
                "an open physical leaf never grants closed-hours entry");
            R53Expect(EvaluateDoorOpen(door, true, false, minute, {}) == DoorResult::Ok, "closing always permitted");
        }
        auto state = MakeInitialDoorState();
        const auto before = state;
        R53Expect(TrySetDoorOpen(state, door.Id, true, 539, false) == DoorResult::Closed && state == before,
            "refused entry changes nothing");
        R53Expect(ApplyDoorSettlement(state, door.Id, true, -door.PreferredSwingSide) == DoorResult::InvalidState && state == before,
            "outward settlement refused atomically");
        R53Expect(TrySetDoorOpen(state, door.Id, true, 630, false) == DoorResult::Ok && state.Doors[i].SwingSide == door.PreferredSwingSide,
            "visitor opens inward during browsing");
    }
}

void R53Migration()
{
    const auto initial = MakeInitialDoorState();
    auto played = initial;
    // Exercise every old leaf, not only the original home. Both permitted sides and closed states must survive.
    for (std::size_t i = 0; i < 23; ++i)
    {
        const auto& def = AuthoredDoors()[i];
        const bool open = i % 3 != 1;
        const std::int8_t side = !open ? 0 : def.bTwoWay && i % 2 == 0 ? -1 : def.PreferredSwingSide;
        const auto result = ApplyDoorSettlement(played, def.Id, open, side);
        R53Expect(result == DoorResult::Ok || result == DoorResult::NoChange, "old physical fixture authored legally");
    }
    const auto twentyThree = R53Text(played, 23);
    const auto migrated = DeserialiseDoorState(twentyThree);
    R53Expect(migrated.Ok() && migrated.bMigrated && migrated.State == played, "23->26 preserves every old pair, adds only defaults");
    for (std::size_t i = 23; i < 26; ++i)
        R53Expect(migrated.State.Doors[i] == DoorEntry{std::string(R53FrozenIds[i]), false, 0}, "new leaves alone start closed");
    for (std::size_t count = 0; count <= 27; ++count)
    {
        const auto result = DeserialiseDoorState(R53Text(played, count));
        const bool realEdition = count == 10 || count == 11 || count == 16 || count == 20 || count == 23 || count == 26 || count == 29;
        R53Expect(result.Ok() == realEdition, "only real /2 roster editions accepted");
        if (!realEdition) R53Expect(result.State == DoorState{} && !result.bMigrated, "invalid roster publishes no state");
    }
    for (std::size_t length = 0; length < twentyThree.size(); ++length)
    {
        const auto result = DeserialiseDoorState(std::string_view(twentyThree).substr(0, length));
        R53Expect(!result.Ok() && result.State == DoorState{} && !result.bMigrated, "every truncated twenty-three-door migration refused whole");
    }
    for (const auto replacement : {R53FrozenIds[21], R53FrozenIds[23], std::string_view("door.nabapara.unknown")})
    {
        std::string damaged = twentyThree;
        damaged.replace(damaged.find(R53FrozenIds[22]), R53FrozenIds[22].size(), replacement);
        R53Expect(!DeserialiseDoorState(damaged).Ok(), "duplicate, gap and unknown door refused");
    }
    auto current = migrated.State;
    R53Expect(TrySetDoorOpen(current, R53FrozenIds[23], true, 630, false) == DoorResult::Ok &&
        TrySetDoorOpen(current, R53FrozenIds[25], true, 630, false) == DoorResult::Ok, "mixed new door states");
    const auto roundTrip = DeserialiseDoorState(SerialiseDoorState(current));
    R53Expect(roundTrip.Ok() && !roundTrip.bMigrated && roundTrip.State == current, "current edition round trips idempotently");
}
} // namespace

int main()
{
    R53DefinitionsAndAccess();
    R53Migration();
    std::cout << "DoorsRoster26Tests: " << R53Assertions << " assertions passed\n";
    return 0;
}
