// The twenty-door roster (NR-05 frontages and NR-06's first upper flat) and the pw.doors/2 16 -> 20 migration.
#include "Doors.h"
#include "LaneTallFlatsLayout.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

using namespace PerfectWorld;

namespace
{
int R20Assertions = 0;

void R20Expect(bool condition, const std::string& message)
{
    ++R20Assertions;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

constexpr std::string_view Workshop = "door.nabapara.repair_workshop_front";
constexpr std::string_view Hardware = "door.nabapara.hardware_store_front";
constexpr std::string_view OldHouse = "door.nabapara.old_house_front";
constexpr std::string_view Upper = "door.nabapara.tall_flats_upper_a";

// The sixteen-door roster exactly as ec27cc7 wrote it; the new doors only ever append after it.
constexpr std::array<std::string_view, 16> Frozen16{
    "door.nabapara.home_front", "door.nabapara.home_living", "door.nabapara.home_bedroom", "door.nabapara.home_kitchen",
    "door.nabapara.home_washroom", "door.nabapara.shanti_chai_shutter", "door.nabapara.care_front", "door.nabapara.care_dormitory",
    "door.nabapara.care_common", "door.nabapara.care_washroom", "door.nabapara.stationery_front", "door.nabapara.caretaker_flat_front",
    "door.nabapara.tall_flats_front", "door.nabapara.tall_flats_unit_a", "door.nabapara.tall_flats_unit_b",
    "door.nabapara.tea_row_house_front"};

bool SameWindows(std::span<const DoorOpeningWindow> a, std::span<const DoorOpeningWindow> b)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (a[i].OpenMinute != b[i].OpenMinute || a[i].CloseMinute != b[i].CloseMinute)
            return false;
    }
    return true;
}

bool SameDef(const DoorDef& a, const DoorDef& b)
{
    return a.Id == b.Id && a.LocationId == b.LocationId && a.bInitiallyOpen == b.bInitiallyOpen && a.Access == b.Access &&
           a.OpenMinute == b.OpenMinute && a.CloseMinute == b.CloseMinute && SameWindows(a.OpeningWindows, b.OpeningWindows) &&
           a.bTwoWay == b.bTwoWay && a.PreferredSwingSide == b.PreferredSwingSide && SameWindows(a.VisitingWindows, b.VisitingWindows);
}

bool InWindows(std::span<const DoorOpeningWindow> windows, int minute)
{
    for (const auto& w : windows)
    {
        if (minute >= w.OpenMinute && minute < w.CloseMinute)
            return true;
    }
    return false;
}

void R20TestTable()
{
    const auto doors = AuthoredDoors();
    R20Expect(ValidateDoorTable() && doors.size() == 29, "twenty-door prefix in today's well-formed roster");
    for (std::size_t i = 0; i < Frozen16.size(); ++i)
        R20Expect(doors[i].Id == Frozen16[i], "the sixteen-door prefix is unchanged: " + std::string(Frozen16[i]));
    // The exact DoorDefs recorded by NR-05 and NR-06, appended in this order.
    const DoorOpeningWindow workshop[]{{480, 720}, {840, 1110}};
    const DoorOpeningWindow hardware[]{{540, 780}, {900, 1200}};
    const DoorOpeningWindow visiting[]{{960, 1170}};
    const DoorDef expected[]{
        {Workshop, "location.nabapara.repair_workshop", false, DoorAccess::BusinessHours, 0, 0, workshop, false, 1, {}},
        {Hardware, "location.nabapara.hardware_store", false, DoorAccess::BusinessHours, 0, 0, hardware, false, 1, {}},
        {OldHouse, "location.nabapara.old_house", false, DoorAccess::Private, 0, 0, {}, false, 1, visiting},
        LaneTallFlats::PlannedUpperDoor()};
    R20Expect(LaneTallFlats::PlannedUpperDoor().Id == Upper && LaneTallFlats::PlannedUpperDoor().LocationId == LaneTallFlats::UpperLocationId,
              "NR-06's planned door is the upper flat");
    for (std::size_t i = 0; i < 4; ++i)
    {
        const DoorDef& door = doors[16 + i];
        const std::string id(door.Id);
        R20Expect(SameDef(door, expected[i]), "authored exactly as planned: " + id);
        R20Expect(FindDoor(door.Id) == &door, "found by id: " + id);
        R20Expect(!door.bInitiallyOpen && !door.bTwoWay && door.PreferredSwingSide == 1, "starts closed, one-way, inward: " + id);
        R20Expect(door.LocationId == "location.nabapara." + id.substr(14, id.size() - 14 - (id.ends_with("_front") ? 6 : 0)),
                  "location follows the row: " + id);
        R20Expect(MakeInitialDoorState().Doors[16 + i] == DoorEntry{id, false, 0}, "initial entry closed: " + id);
    }
}

void R20TestAccess()
{
    const DoorDef& workshop = *FindDoor(Workshop);
    const DoorDef& hardware = *FindDoor(Hardware);
    for (const DoorDef* shop : {&workshop, &hardware})
    {
        for (int minute = 0; minute < DoorMinutesPerDay; ++minute)
        {
            const bool open = InWindows(shop->OpeningWindows, minute);
            R20Expect(EvaluateDoorPassage(*shop, minute, {}) == (open ? DoorResult::Ok : DoorResult::Closed), "visitors only in hours");
            R20Expect(EvaluateDoorPassage(*shop, minute, {DoorDirection::Entry, true}) == DoorResult::Ok, "staff always enter");
            R20Expect(EvaluateDoorPassage(*shop, minute, {DoorDirection::Entry, false, true}) == (open ? DoorResult::Ok : DoorResult::Closed),
                      "an invitation does not override business hours");
            R20Expect(EvaluateDoorPassage(*shop, minute, {DoorDirection::Exit}) == DoorResult::Ok, "exit is unrestricted");
        }
    }
    R20Expect(EvaluateDoorOffer(workshop, false, 720, {}).MinutesUntilOpening == 120 && DoorMinutesUntilOpening(workshop, 1110) == 810,
              "the workshop reopens at 14:00 and next morning at 08:00");
    R20Expect(DoorMinutesUntilOpening(hardware, 780) == 120 && DoorMinutesUntilOpening(hardware, 1200) == 780 && DoorOpensAtMinute(hardware) == 540,
              "the hardware store reopens at 15:00 and next morning at 09:00");

    const DoorDef& oldHouse = *FindDoor(OldHouse);
    const DoorDef& upper = *FindDoor(Upper);
    for (int minute = 0; minute < DoorMinutesPerDay; ++minute)
    {
        for (const DoorDef* home : {&oldHouse, &upper})
        {
            // Nobody lives here yet, so nobody is ever home: the knock is never answered and admits nobody.
            R20Expect(EvaluateDoorKnock(*home, minute, {}).Answer == DoorKnockAnswer::NoAnswer, "an empty home gives no answer");
            R20Expect(EvaluateDoorPassage(*home, minute, {DoorDirection::Entry, false, false, false, true}) == DoorResult::Private,
                      "an unanswered knock admits nobody");
            R20Expect(EvaluateDoorPassage(*home, minute, {DoorDirection::Entry, false, true}) == DoorResult::Ok, "an invitation admits");
            R20Expect(EvaluateDoorPassage(*home, minute, {DoorDirection::Exit}) == DoorResult::Ok, "exit is unrestricted");
            R20Expect(EvaluateDoorOffer(*home, false, minute, {}).Action == DoorAction::Knock, "a visitor is offered Knock");
        }
        // Were somebody home: both the old house and (since R5-01 gave it a resident) the upper flat receive 16:00-19:30.
        const bool visiting = minute >= 960 && minute < 1170;
        R20Expect(EvaluateDoorKnock(oldHouse, minute, {DoorDirection::Entry, false, false, true}).Answer ==
                      (visiting ? DoorKnockAnswer::Admitted : DoorKnockAnswer::NotReceiving), "old-house visiting hours");
        R20Expect(EvaluateDoorKnock(upper, minute, {DoorDirection::Entry, false, false, true}).Answer ==
                      (visiting ? DoorKnockAnswer::Admitted : DoorKnockAnswer::NotReceiving), "upper-flat visiting hours");
    }

    DoorState state = MakeInitialDoorState();
    for (std::size_t i = 16; i < 20; ++i)
    {
        const std::string_view id = AuthoredDoors()[i].Id;
        R20Expect(ApplyDoorSettlement(state, id, true, -1) == DoorResult::InvalidState, "a one-way leaf never settles outward");
        R20Expect(TrySetDoorOpen(state, id, true, 600, DoorUseContext{DoorDirection::Entry, true}) == DoorResult::Ok &&
                      state.Doors[i].SwingSide == 1, "a resident opens the leaf inward");
        R20Expect(TrySetDoorOpen(state, id, false, 610, DoorUseContext{}) == DoorResult::Ok, "closing is unrestricted");
    }
}

std::string RosterText(std::string_view header, const DoorState& state, std::size_t lines, std::size_t count, bool sides)
{
    std::string text = std::string(header) + "\ncount=" + std::to_string(count) + "\n";
    for (std::size_t i = 0; i < lines; ++i)
    {
        const DoorEntry& e = state.Doors[i];
        text += "door=" + e.Id + "," + (e.Open ? "1" : "0") + (sides ? "," + std::to_string(e.SwingSide) : std::string{}) + "\n";
    }
    return text + "end\n";
}

std::string Swap(std::string text, const std::string& from, const std::string& to)
{
    const std::size_t at = text.find(from);
    R20Expect(at != std::string::npos, "test text holds: " + from);
    return text.replace(at, from.size(), to);
}

void R20TestMigration()
{
    const auto editions = DoorRosterEditions();
    const DoorRosterEdition want[]{{"pw.doors/1", 5}, {"pw.doors/1", 10}, {"pw.doors/1", 11}, {"pw.doors/2", 10},
                                   {"pw.doors/2", 11}, {"pw.doors/2", 16}, {"pw.doors/2", 20}, {"pw.doors/2", 23}, {"pw.doors/2", 26}, {"pw.doors/2", 29}};
    R20Expect(editions.size() == 10, "ten roster editions");
    for (std::size_t i = 0; i < editions.size() && i < 10; ++i)
        R20Expect(editions[i].Header == want[i].Header && editions[i].Count == want[i].Count, "historical editions retained before twenty-nine");

    DoorState played = MakeInitialDoorState();
    R20Expect(ApplyDoorSettlement(played, "door.nabapara.home_front", true, -1) == DoorResult::Ok &&
                  ApplyDoorSettlement(played, "door.nabapara.tall_flats_front", true, -1) == DoorResult::Ok &&
                  ApplyDoorSettlement(played, "door.nabapara.tea_row_house_front", true, 1) == DoorResult::Ok,
              "a played sixteen-door prefix");
    const DoorState initial = MakeInitialDoorState();
    const std::string sixteen = RosterText("pw.doors/2", played, 16, 16, true);
    const DoorStateParse parsed = DeserialiseDoorState(sixteen);
    R20Expect(parsed.Ok() && parsed.bMigrated && IsDoorStateValid(parsed.State), "a sixteen-door /2 roster migrates");
    for (std::size_t i = 0; i < 20; ++i)
        R20Expect(parsed.State.Doors[i] == (i < 16 ? played.Doors[i] : initial.Doors[i]), "stored facts kept; the four new doors closed");
    const std::string resaved = SerialiseDoorState(parsed.State);
    R20Expect(resaved.starts_with("pw.doors/2\ncount=29\n"), "the migrated roster resaves at twenty-nine");
    const DoorStateParse again = DeserialiseDoorState(resaved);
    R20Expect(again.Ok() && !again.bMigrated && again.State == parsed.State, "and round trips without migrating again");

    DoorState twentyPlayed = played;
    R20Expect(ApplyDoorSettlement(twentyPlayed, Workshop, true, 1) == DoorResult::Ok && ApplyDoorSettlement(twentyPlayed, Upper, true, 1) == DoorResult::Ok,
              "new leaves can stand open");
    const std::string twenty = RosterText("pw.doors/2", twentyPlayed, 20, 20, true);
    R20Expect(DeserialiseDoorState(twenty).State == twentyPlayed && DeserialiseDoorState(twenty).bMigrated,
              "the historical twenty-door roster retains its facts and appends three defaults");

    for (std::size_t count = 0; count <= 22; ++count)
    {
        for (const bool legacy : {false, true})
        {
            const std::size_t lines = count < 20 ? count : 20;
            const auto result = DeserialiseDoorState(RosterText(legacy ? "pw.doors/1" : "pw.doors/2", played, lines, count, !legacy));
            const bool known = legacy ? (count == 5 || count == 10 || count == 11) : (count == 10 || count == 11 || count == 16 || count == 20);
            R20Expect(result.Ok() == known, "only real editions load (count " + std::to_string(count) + (legacy ? ", /1)" : ", /2)"));
            if (!known)
                R20Expect(result.State == DoorState{} && !result.bMigrated, "a refused roster publishes nothing");
        }
    }

    // Tampered, unknown, duplicate and partial sixteen-door rosters are refused, publishing nothing.
    const std::string refused[]{
        Swap(sixteen, "tea_row_house_front,1,1", "tea_row_house_front,1,-1"),
        Swap(sixteen, "tea_row_house_front", "repair_workshop_front"),
        Swap(sixteen, "tea_row_house_front", "tall_flats_unit_b"),
        Swap(sixteen, "tea_row_house_front", "tea_row_house_back"),
        RosterText("pw.doors/2", played, 15, 16, true),
        RosterText("pw.doors/2", played, 17, 16, true),
        RosterText("pw.doors/2", played, 16, 17, true),
        RosterText("pw.doors/2", played, 19, 19, true),
        Swap(twenty, "door=door.nabapara.repair_workshop_front,1,1\ndoor=door.nabapara.hardware_store_front,0,0",
             "door=door.nabapara.hardware_store_front,0,0\ndoor=door.nabapara.repair_workshop_front,1,1"),
        Swap(twenty, "tall_flats_upper_a,1,1", "tall_flats_upper_a,1,-1"),
        Swap(twenty, "tall_flats_upper_a", "old_house_front"),
        Swap(sixteen, "pw.doors/2", "pw.doors/1")};
    for (const std::string& text : refused)
    {
        const DoorStateParse bad = DeserialiseDoorState(text);
        R20Expect(!bad.Ok() && !bad.bMigrated && bad.State == DoorState{}, "a damaged roster is refused whole");
    }
    for (std::size_t length = 0; length < sixteen.size(); ++length)
    {
        const DoorStateParse cut = DeserialiseDoorState(std::string_view(sixteen).substr(0, length));
        R20Expect(!cut.Ok() && !cut.bMigrated && cut.State == DoorState{}, "every truncated migration fails without partial state");
    }
}
} // namespace

int main()
{
    R20TestTable();
    R20TestAccess();
    R20TestMigration();
    std::cout << "PerfectWorld doors roster 20: " << R20Assertions << " assertions passed\n";
    return 0;
}
