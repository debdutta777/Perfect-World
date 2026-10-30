#include "Doors.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace PerfectWorld;

namespace
{
int R61Assertions = 0;
void R61Expect(bool condition, const std::string& message)
{
    ++R61Assertions;
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

constexpr std::array<std::string_view, 29> R61FrozenIds{
    "door.nabapara.home_front", "door.nabapara.home_living", "door.nabapara.home_bedroom", "door.nabapara.home_kitchen",
    "door.nabapara.home_washroom", "door.nabapara.shanti_chai_shutter", "door.nabapara.care_front", "door.nabapara.care_dormitory",
    "door.nabapara.care_common", "door.nabapara.care_washroom", "door.nabapara.stationery_front", "door.nabapara.caretaker_flat_front",
    "door.nabapara.tall_flats_front", "door.nabapara.tall_flats_unit_a", "door.nabapara.tall_flats_unit_b",
    "door.nabapara.tea_row_house_front", "door.nabapara.repair_workshop_front", "door.nabapara.hardware_store_front",
    "door.nabapara.old_house_front", "door.nabapara.tall_flats_upper_a", "door.nabapara.market_stalls_a_front",
    "door.nabapara.market_stalls_b_front", "door.nabapara.end_shop_front",
    "door.nabapara.cloth_merchant_front", "door.nabapara.narrow_shop_front", "door.nabapara.tailor_shop_front",
    "door.nabapara.alley_house_front", "door.nabapara.stair_flats_front", "door.nabapara.corner_stalls_front"};

std::string R61Text(const DoorState& state, std::size_t count)
{
    std::string text = "pw.doors/2\ncount=" + std::to_string(count) + "\n";
    for (std::size_t i = 0; i < std::min(count, state.Doors.size()); ++i)
    {
        const auto& entry = state.Doors[i];
        text += "door=" + entry.Id + (entry.Open ? ",1," : ",0,") + std::to_string(entry.SwingSide) + "\n";
    }
    return text + "end\n";
}

void R61DefinitionsAndAccess()
{
    const auto doors = AuthoredDoors();
    R61Expect(doors.size() == 29 && ValidateDoorTable(), "twenty-nine valid doors");
    for (std::size_t i = 0; i < R61FrozenIds.size(); ++i)
        R61Expect(doors[i].Id == R61FrozenIds[i], "frozen twenty-six-door prefix and append order");
    const std::array<std::string_view, 3> locations{
        "location.nabapara.alley_house", "location.nabapara.stair_flats", "location.nabapara.corner_stalls"};
    for (std::size_t i = 26; i < 29; ++i)
    {
        const auto& door = doors[i];
        const bool shop = i == 28;
        R61Expect(door.LocationId == locations[i - 26] && !door.bInitiallyOpen && !door.bTwoWay &&
            door.PreferredSwingSide == (shop ? 1 : -1) && door.VisitingWindows.empty() &&
            door.OpenMinute == 0 && door.CloseMinute == 0 &&
            door.Access == (shop ? DoorAccess::BusinessHours : DoorAccess::Private), "R61 identity, access and inward leaf policy");
        R61Expect(shop ? door.OpeningWindows.size() == 2 && door.OpeningWindows[0].OpenMinute == 540 &&
            door.OpeningWindows[0].CloseMinute == 780 && door.OpeningWindows[1].OpenMinute == 900 &&
            door.OpeningWindows[1].CloseMinute == 1170 : door.OpeningWindows.empty(), "browsing windows only on the corner shop");
        for (int minute = 0; minute < DoorMinutesPerDay; ++minute)
        {
            const bool browsing = (minute >= 540 && minute < 780) || (minute >= 900 && minute < 1170);
            const auto permission = shop ? (browsing ? DoorResult::Ok : DoorResult::Closed) : DoorResult::Private;
            R61Expect(EvaluateDoorPassage(door, minute, {}) == permission, "visitor entry follows the access rule at every minute");
            R61Expect(EvaluateDoorPassage(door, minute, {DoorDirection::Exit}) == DoorResult::Ok, "unrestricted egress");
            R61Expect(EvaluateDoorOpen(door, true, true, minute, {}) ==
                (permission == DoorResult::Ok ? DoorResult::NoChange : permission), "an open leaf grants no entry permission");
            R61Expect(EvaluateDoorOpen(door, true, false, minute, {}) == DoorResult::Ok, "closing is unrestricted");
            if (!shop)
            {
                R61Expect(EvaluateDoorOffer(door, false, minute, {}).Action == DoorAction::Knock &&
                    EvaluateDoorKnock(door, minute, {}).Answer == DoorKnockAnswer::NoAnswer, "unoccupied home offers an unanswered knock");
                R61Expect(EvaluateDoorPassage(door, minute, {DoorDirection::Entry, false, false, false, true}) == DoorResult::Private,
                    "an unanswered knock grants nothing");
                R61Expect(EvaluateDoorKnock(door, minute, {DoorDirection::Entry, false, false, true}).Answer == DoorKnockAnswer::NotReceiving &&
                    DoorMinutesUntilVisiting(door, minute) == -1, "no visiting window is invented even if a future host is home");
            }
        }
        auto state = MakeInitialDoorState();
        const auto before = state;
        R61Expect(TrySetDoorOpen(state, door.Id, true, 539, false) == (shop ? DoorResult::Closed : DoorResult::Private) && state == before,
            "refused entry is atomic");
        R61Expect(ApplyDoorSettlement(state, door.Id, true, -door.PreferredSwingSide) == DoorResult::InvalidState && state == before,
            "outward settlement is refused atomically");
        R61Expect(TrySetDoorOpen(state, door.Id, true, 630, shop ? DoorUseContext{} : DoorUseContext{DoorDirection::Entry, false, true}) == DoorResult::Ok &&
            state.Doors[i].SwingSide == door.PreferredSwingSide, "legitimate access opens inward");
    }
}

void R61Migration()
{
    auto played = MakeInitialDoorState();
    for (std::size_t i = 0; i < 26; ++i)
    {
        const auto& def = AuthoredDoors()[i];
        const bool open = i % 3 != 1;
        const std::int8_t side = !open ? 0 : def.bTwoWay && i % 2 == 0 ? -1 : def.PreferredSwingSide;
        const auto result = ApplyDoorSettlement(played, def.Id, open, side);
        R61Expect(result == DoorResult::Ok || result == DoorResult::NoChange, "every old physical pair is legal");
    }
    const auto frozen = R61Text(played, 26);
    const auto migrated = DeserialiseDoorState(frozen);
    R61Expect(migrated.Ok() && migrated.bMigrated && migrated.State == played, "26->29 preserves every old pair and appends only defaults");
    for (std::size_t i = 26; i < 29; ++i)
        R61Expect(migrated.State.Doors[i] == DoorEntry{std::string(R61FrozenIds[i]), false, 0}, "only the three appended leaves default closed");
    const auto editions = DoorRosterEditions();
    R61Expect(editions.size() == 10 && editions[8].Header == "pw.doors/2" && editions[8].Count == 26 &&
        editions[9].Header == "pw.doors/2" && editions[9].Count == 29, "the historical 26 edition precedes today's 29 edition");
    for (std::size_t count = 0; count <= 30; ++count)
    {
        const auto result = DeserialiseDoorState(R61Text(played, count));
        const bool realEdition = count == 10 || count == 11 || count == 16 || count == 20 || count == 23 || count == 26 || count == 29;
        R61Expect(result.Ok() == realEdition, "only real /2 editions load, including refusal of 27/28");
        if (!realEdition) R61Expect(result.State == DoorState{} && !result.bMigrated, "refusal publishes no partial state");
    }
    for (std::size_t length = 0; length < frozen.size(); ++length)
    {
        const auto result = DeserialiseDoorState(std::string_view(frozen).substr(0, length));
        R61Expect(!result.Ok() && result.State == DoorState{} && !result.bMigrated, "every truncated 26-door record is refused whole");
    }
    for (const auto replacement : {R61FrozenIds[24], R61FrozenIds[26], std::string_view("door.nabapara.unknown")})
    {
        auto damaged = frozen;
        damaged.replace(damaged.find(R61FrozenIds[25]), R61FrozenIds[25].size(), replacement);
        R61Expect(!DeserialiseDoorState(damaged).Ok(), "duplicate, gap and unknown ID refused");
    }
    for (const char* damage : {",0,1", ",1,0", ",1,1", ",1,-01"})
    {
        auto damaged = frozen;
        const auto at = damaged.find("door.nabapara.cloth_merchant_front"); // index 23 is open, side -1
        const auto comma = damaged.find(',', at);
        damaged.replace(comma, damaged.find('\n', comma) - comma, damage);
        const auto result = DeserialiseDoorState(damaged);
        R61Expect(!result.Ok() && result.State == DoorState{} && !result.bMigrated, "bad physical pair or noncanonical side refused atomically");
    }
    R61Expect(!DeserialiseDoorState(frozen + "x").Ok(), "trailing data refused");
    auto current = migrated.State;
    R61Expect(TrySetDoorOpen(current, R61FrozenIds[28], true, 630, false) == DoorResult::Ok, "corner browse leaf opens");
    const auto roundTrip = DeserialiseDoorState(SerialiseDoorState(current));
    R61Expect(roundTrip.Ok() && !roundTrip.bMigrated && roundTrip.State == current &&
        SerialiseDoorState(roundTrip.State) == SerialiseDoorState(current), "29-door resave is idempotent with both homes closed");
}
} // namespace

int main()
{
    R61DefinitionsAndAccess();
    R61Migration();
    std::cout << "DoorsRoster29Tests: " << R61Assertions << " assertions passed\n";
    return 0;
}
