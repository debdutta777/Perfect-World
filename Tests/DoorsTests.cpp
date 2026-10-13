// LANE_LIFE_IMPLEMENTATION rows 3.1 and 3.3 (minimal): door definitions, access rules, door state and its text form.
#include "Doors.h"

#include <cstdlib>
#include <iostream>
#include <set>
#include <string>

using namespace PerfectWorld;

namespace
{
int Checks = 0;

void Check(bool condition, const char* message)
{
    ++Checks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

constexpr std::string_view Front = "door.nabapara.home_front";
constexpr std::string_view Living = "door.nabapara.home_living";
constexpr std::string_view Bedroom = "door.nabapara.home_bedroom";
constexpr std::string_view Kitchen = "door.nabapara.home_kitchen";
constexpr std::string_view Washroom = "door.nabapara.home_washroom";

void TestTable()
{
    Check(ValidateDoorTable(), "the authored door table is well formed");
    Check(AuthoredDoors().size() == 5, "the opening home has a front door and four room doors");
    for (const std::string_view id : {Front, Living, Bedroom, Kitchen, Washroom})
    {
        const DoorDef* door = FindDoor(id);
        Check(door != nullptr, "every named home door is authored");
        Check(door != nullptr && door->LocationId == "location.nabapara.opening_home", "the home doors share the opening home location");
    }
    Check(FindDoor("door.nabapara.nope") == nullptr, "an unknown id finds nothing");
    Check(FindDoor(Front)->Access == DoorAccess::Private && !FindDoor(Front)->bInitiallyOpen, "the front door is private and starts closed");
    Check(FindDoor(Living)->Access == DoorAccess::Public && FindDoor(Living)->bInitiallyOpen, "the living room door is public and starts open");
    Check(DoorAccessId(DoorAccess::BusinessHours) == "business_hours" && DoorAccessId(DoorAccess::Locked) == "locked", "access ids are stable");
}

void TestCodes()
{
    std::set<std::string_view> codes;
    std::set<std::string_view> keys;
    for (const DoorResult result : {DoorResult::Ok, DoorResult::UnknownDoor, DoorResult::Locked, DoorResult::Private, DoorResult::Closed,
                                    DoorResult::NoChange, DoorResult::InvalidTime})
    {
        Check(!DoorResultCode(result).empty(), "every result has a code");
        codes.insert(DoorResultCode(result));
        if (result != DoorResult::Ok)
        {
            Check(DoorResultReasonKey(result).substr(0, 12) == "door.reason.", "a refusal has a player-facing reason key");
            keys.insert(DoorResultReasonKey(result));
        }
    }
    Check(DoorResultReasonKey(DoorResult::Ok).empty(), "success has no reason key");
    Check(codes.size() == 7 && keys.size() == 6, "codes and reason keys are distinct");
    Check(DoorResultCode(DoorResult::Closed) == "closed_hours" && DoorResultCode(DoorResult::Private) == "private", "codes are stable");
}

void TestInitialState()
{
    const DoorState state = MakeInitialDoorState();
    Check(IsDoorStateValid(state), "the initial state is valid");
    Check(state.Doors.size() == AuthoredDoors().size(), "one entry per authored door");
    for (std::size_t i = 0; i < state.Doors.size(); ++i)
    {
        Check(state.Doors[i].Id == AuthoredDoors()[i].Id, "entries follow table order");
        Check(state.Doors[i].Open == AuthoredDoors()[i].bInitiallyOpen, "entries start as authored");
    }
    Check(QueryDoorOpen(state, Living) == std::optional<bool>(true), "the living door reads open");
    Check(QueryDoorOpen(state, Front) == std::optional<bool>(false), "the front door reads closed");
    Check(!QueryDoorOpen(state, "door.nope").has_value(), "an unknown door has no state");
    DoorState wrong = state;
    wrong.Doors.pop_back();
    Check(!IsDoorStateValid(wrong), "a state missing a door is invalid");
}

void TestResidentAndVisitor()
{
    DoorState state = MakeInitialDoorState();
    const DoorState before = state;
    Check(TrySetDoorOpen(state, Front, true, 600, false) == DoorResult::Private, "a visitor cannot open the private front door");
    Check(state == before, "a refusal leaves the state untouched");
    Check(TrySetDoorOpen(state, Bedroom, true, 600, false) == DoorResult::Private, "a visitor cannot open the bedroom door");
    Check(TrySetDoorOpen(state, Front, true, 600, true) == DoorResult::Ok, "a resident opens the front door");
    Check(QueryDoorOpen(state, Front) == std::optional<bool>(true), "the front door is now open");
    Check(TrySetDoorOpen(state, Front, true, 600, true) == DoorResult::NoChange, "opening an open door changes nothing");
    Check(TrySetDoorOpen(state, Front, false, 600, false) == DoorResult::Ok, "a visitor may close a door");
    Check(QueryDoorOpen(state, Front) == std::optional<bool>(false), "the front door is closed again");
    Check(TrySetDoorOpen(state, Front, false, 600, true) == DoorResult::NoChange, "closing a closed door changes nothing");
    Check(TrySetDoorOpen(state, Washroom, true, 1, false) == DoorResult::Ok, "a visitor opens a public door at any hour");
    Check(TrySetDoorOpen(state, Kitchen, false, 1439, false) == DoorResult::Ok, "a visitor closes a public door at any hour");
}

void TestRefusals()
{
    DoorState state = MakeInitialDoorState();
    const DoorState before = state;
    Check(TrySetDoorOpen(state, "door.nabapara.nope", true, 600, true) == DoorResult::UnknownDoor, "an unknown door is refused");
    Check(TrySetDoorOpen(state, "", true, 600, true) == DoorResult::UnknownDoor, "an empty id is refused");
    Check(TrySetDoorOpen(state, Front, true, -1, true) == DoorResult::InvalidTime, "a negative minute is refused");
    Check(TrySetDoorOpen(state, Front, true, 1440, true) == DoorResult::InvalidTime, "minute 1440 is refused");
    Check(TrySetDoorOpen(state, Living, true, 1440, true) == DoorResult::InvalidTime, "the time is checked before no-change");
    Check(state == before, "no refusal changed the state");
    DoorState partial = before;
    partial.Doors.pop_back();
    Check(TrySetDoorOpen(partial, Washroom, true, 600, true) == DoorResult::UnknownDoor, "an authored door the state lacks is refused");
}

void TestRules()
{
    const DoorDef locked{"door.test.locked", "location.test", false, DoorAccess::Locked, 0, 0};
    Check(EvaluateDoorOpen(locked, false, true, 600, false) == DoorResult::Locked, "a visitor cannot open a locked door");
    Check(EvaluateDoorOpen(locked, false, true, 600, true) == DoorResult::Locked, "nor can a resident (no keys in this slice)");
    Check(EvaluateDoorOpen(locked, false, false, 600, true) == DoorResult::NoChange, "a closed locked door is already closed");

    const DoorDef shop{"door.test.shop", "location.test", false, DoorAccess::BusinessHours, 7 * 60, 21 * 60};
    Check(EvaluateDoorOpen(shop, false, true, 7 * 60 - 1, false) == DoorResult::Closed, "a shop is closed the minute before opening");
    Check(EvaluateDoorOpen(shop, false, true, 7 * 60, false) == DoorResult::Ok, "a shop opens at its opening minute");
    Check(EvaluateDoorOpen(shop, false, true, 21 * 60 - 1, false) == DoorResult::Ok, "a shop is open the minute before closing");
    Check(EvaluateDoorOpen(shop, false, true, 21 * 60, false) == DoorResult::Closed, "a shop is closed at its closing minute");
    Check(EvaluateDoorOpen(shop, false, true, 2 * 60, true) == DoorResult::Ok, "staff may open outside the hours");
    Check(EvaluateDoorOpen(shop, true, false, 2 * 60, false) == DoorResult::Ok, "anyone may close it outside the hours");
    Check(DoorOpensAtMinute(shop) == 7 * 60, "a shop reports when it opens");
    Check(DoorOpensAtMinute(locked) == -1 && DoorOpensAtMinute(*FindDoor(Front)) == -1, "other kinds report no opening time");

    const DoorDef open{"door.test.open", "location.test", true, DoorAccess::Public, 0, 0};
    Check(EvaluateDoorOpen(open, true, true, 0, false) == DoorResult::NoChange, "an open public door stays open");
    Check(EvaluateDoorOpen(open, false, true, 0, false) == DoorResult::Ok, "a closed public door opens at midnight");
}

void TestMinuteOfDay()
{
    Check(DoorMinuteOfDay(GameClock::FromCalendar(2042, 6, 2, 6, 0)) == 360, "06:00 is minute 360");
    Check(DoorMinuteOfDay(GameClock::FromCalendar(2042, 6, 2, 23, 59)) == 1439, "23:59 is minute 1439");
    Check(DoorMinuteOfDay(GameClock::FromCalendar(2042, 6, 3, 0, 0)) == 0, "midnight is minute 0");
}

void Play(DoorState& state)
{
    (void)TrySetDoorOpen(state, Front, true, 400, true);
    (void)TrySetDoorOpen(state, Bedroom, true, 401, false);
    (void)TrySetDoorOpen(state, Living, false, 402, false);
    (void)TrySetDoorOpen(state, Washroom, true, 403, false);
    (void)TrySetDoorOpen(state, Kitchen, true, 404, true);
    (void)TrySetDoorOpen(state, Bedroom, true, 405, true);
}

void TestDeterminism()
{
    DoorState a = MakeInitialDoorState();
    DoorState b = MakeInitialDoorState();
    Play(a);
    Play(b);
    Check(a == b, "the same commands give the same state");
    Check(SerialiseDoorState(a) == SerialiseDoorState(b), "and the same text");
    Check(QueryDoorOpen(a, Bedroom) == std::optional<bool>(true) && QueryDoorOpen(a, Living) == std::optional<bool>(false),
          "the mix of accepted and refused commands landed as expected");
    Check(SerialiseDoorState(MakeInitialDoorState()) == SerialiseDoorState(MakeInitialDoorState()), "the initial text is stable");
}

void TestRoundTrip()
{
    DoorState state = MakeInitialDoorState();
    Play(state);
    const std::string text = SerialiseDoorState(state);
    Check(!text.empty() && text.substr(0, 11) == "pw.doors/1\n", "the text opens with its header");
    const DoorStateParse parsed = DeserialiseDoorState(text);
    Check(parsed.Ok() && parsed.State == state, "the state round trips");
    Check(SerialiseDoorState(parsed.State) == text, "and re-serialises identically");
    const DoorStateParse initial = DeserialiseDoorState(SerialiseDoorState(MakeInitialDoorState()));
    Check(initial.Ok() && initial.State == MakeInitialDoorState(), "the initial state round trips");

    DoorState invalid = state;
    invalid.Doors.pop_back();
    Check(SerialiseDoorState(invalid).empty(), "an invalid state serialises to nothing");
}

std::string Replace(std::string text, const std::string& from, const std::string& to)
{
    const std::size_t at = text.find(from);
    Check(at != std::string::npos, "test text contains the part to replace");
    text.replace(at, from.size(), to);
    return text;
}

void TestRefusedText()
{
    const std::string text = SerialiseDoorState(MakeInitialDoorState());
    for (std::size_t length = 0; length < text.size(); ++length)
        Check(!DeserialiseDoorState(std::string_view(text).substr(0, length)).Ok(), "every truncation is refused");
    Check(!DeserialiseDoorState(text + "x").Ok(), "trailing data is refused");
    Check(!DeserialiseDoorState(text + "\n").Ok(), "a trailing blank line is refused");
    Check(!DeserialiseDoorState(Replace(text, "pw.doors/1", "pw.doors/2")).Ok(), "a wrong header is refused");
    Check(!DeserialiseDoorState(Replace(text, "count=5", "count=4")).Ok(), "a wrong count is refused");
    Check(!DeserialiseDoorState(Replace(text, "count=5", "count=05")).Ok(), "a non-canonical count is refused");
    Check(!DeserialiseDoorState(Replace(text, "home_front,0", "home_front,2")).Ok(), "a bad flag is refused");
    Check(!DeserialiseDoorState(Replace(text, "home_front,0", "home_front")).Ok(), "a missing flag is refused");
    Check(!DeserialiseDoorState(Replace(text, "home_front,0", "home_nowhere,0")).Ok(), "an unknown door is refused");
    Check(!DeserialiseDoorState(Replace(text, "door.nabapara.home_living,1", "door.nabapara.home_front,0")).Ok(), "a repeated door is refused");
    const std::string swapped = Replace(Replace(Replace(text, "home_living,1", "home_TMP,1"), "home_bedroom,0", "home_living,1"), "home_TMP,1", "home_bedroom,0");
    Check(!DeserialiseDoorState(swapped).Ok(), "doors out of table order are refused");
    const DoorStateParse bad = DeserialiseDoorState("garbage\n");
    Check(!bad.Ok() && bad.State == DoorState{}, "a refusal returns a default state");
}
} // namespace

int main()
{
    TestTable();
    TestCodes();
    TestInitialState();
    TestResidentAndVisitor();
    TestRefusals();
    TestRules();
    TestMinuteOfDay();
    TestDeterminism();
    TestRoundTrip();
    TestRefusedText();
    std::cout << "PerfectWorld doors: " << Checks << " checks passed\n";
    return 0;
}
