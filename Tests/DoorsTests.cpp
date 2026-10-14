// LANE_LIFE_IMPLEMENTATION rows 3.1 and 3.3 (minimal): door definitions, access rules, door state and its text form.
#include "Doors.h"

#include <cstdlib>
#include <iostream>
#include <set>
#include <string>

using namespace PerfectWorld;

namespace
{
int Assertions = 0;

void Expect(bool condition, const char* message)
{
    ++Assertions;
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
    Expect(ValidateDoorTable(), "the authored door table is well formed");
    Expect(AuthoredDoors().size() == 5, "the opening home has a front door and four room doors");
    for (const std::string_view id : {Front, Living, Bedroom, Kitchen, Washroom})
    {
        const DoorDef* door = FindDoor(id);
        Expect(door != nullptr, "every named home door is authored");
        Expect(door != nullptr && door->LocationId == "location.nabapara.opening_home", "the home doors share the opening home location");
    }
    Expect(FindDoor("door.nabapara.nope") == nullptr, "an unknown id finds nothing");
    Expect(FindDoor(Front)->Access == DoorAccess::Private && !FindDoor(Front)->bInitiallyOpen, "the front door is private and starts closed");
    Expect(FindDoor(Living)->Access == DoorAccess::Public && FindDoor(Living)->bInitiallyOpen, "the living room door is public and starts open");
    Expect(DoorAccessId(DoorAccess::BusinessHours) == "business_hours" && DoorAccessId(DoorAccess::Locked) == "locked", "access ids are stable");
}

void TestCodes()
{
    std::set<std::string_view> codes;
    std::set<std::string_view> keys;
    for (const DoorResult result : {DoorResult::Ok, DoorResult::UnknownDoor, DoorResult::Locked, DoorResult::Private, DoorResult::Closed,
                                    DoorResult::NoChange, DoorResult::InvalidTime, DoorResult::InvalidDefinition, DoorResult::InvalidContext,
                                    DoorResult::InvalidState, DoorResult::NotAvailable, DoorResult::Knocked})
    {
        Expect(!DoorResultCode(result).empty(), "every result has a code");
        codes.insert(DoorResultCode(result));
        if (result != DoorResult::Ok && result != DoorResult::Knocked)
        {
            Expect(DoorResultReasonKey(result).substr(0, 12) == "door.reason.", "a refusal has a player-facing reason key");
            keys.insert(DoorResultReasonKey(result));
        }
    }
    Expect(DoorResultReasonKey(DoorResult::Ok).empty(), "success has no reason key");
    Expect(codes.size() == 12 && keys.size() == 10, "codes and reason keys are distinct");
    Expect(DoorResultReasonKey(DoorResult::Knocked).empty(), "a delivered Knock is not a refusal");
    Expect(DoorResultCode(static_cast<DoorResult>(255)).empty(), "invalid result has no code");
    Expect(DoorAccessId(static_cast<DoorAccess>(255)).empty(), "invalid access has no id");
    Expect(DoorResultCode(DoorResult::Closed) == "closed_hours" && DoorResultCode(DoorResult::Private) == "private", "codes are stable");
}

void TestInitialState()
{
    const DoorState state = MakeInitialDoorState();
    Expect(IsDoorStateValid(state), "the initial state is valid");
    Expect(state.Doors.size() == AuthoredDoors().size(), "one entry per authored door");
    for (std::size_t i = 0; i < state.Doors.size(); ++i)
    {
        Expect(state.Doors[i].Id == AuthoredDoors()[i].Id, "entries follow table order");
        Expect(state.Doors[i].Open == AuthoredDoors()[i].bInitiallyOpen, "entries start as authored");
    }
    Expect(QueryDoorOpen(state, Living) == std::optional<bool>(true), "the living door reads open");
    Expect(QueryDoorOpen(state, Front) == std::optional<bool>(false), "the front door reads closed");
    Expect(!QueryDoorOpen(state, "door.nope").has_value(), "an unknown door has no state");
    DoorState wrong = state;
    wrong.Doors.pop_back();
    Expect(!IsDoorStateValid(wrong), "a state missing a door is invalid");
}

void TestResidentAndVisitor()
{
    DoorState state = MakeInitialDoorState();
    const DoorState before = state;
    Expect(TrySetDoorOpen(state, Front, true, 600, false) == DoorResult::Private, "a visitor cannot open the private front door");
    Expect(state == before, "a refusal leaves the state untouched");
    Expect(TrySetDoorOpen(state, Bedroom, true, 600, false) == DoorResult::Private, "a visitor cannot open the bedroom door");
    Expect(TrySetDoorOpen(state, Front, true, 600, true) == DoorResult::Ok, "a resident opens the front door");
    Expect(QueryDoorOpen(state, Front) == std::optional<bool>(true), "the front door is now open");
    Expect(TrySetDoorOpen(state, Front, true, 600, true) == DoorResult::NoChange, "opening an open door changes nothing");
    Expect(TrySetDoorOpen(state, Front, false, 600, false) == DoorResult::Ok, "a visitor may close a door");
    Expect(QueryDoorOpen(state, Front) == std::optional<bool>(false), "the front door is closed again");
    Expect(TrySetDoorOpen(state, Front, false, 600, true) == DoorResult::NoChange, "closing a closed door changes nothing");
    Expect(TrySetDoorOpen(state, Washroom, true, 1, false) == DoorResult::Ok, "a visitor opens a public door at any hour");
    Expect(TrySetDoorOpen(state, Kitchen, false, 1439, false) == DoorResult::Ok, "a visitor closes a public door at any hour");
}

void TestRefusals()
{
    DoorState state = MakeInitialDoorState();
    const DoorState before = state;
    Expect(TrySetDoorOpen(state, "door.nabapara.nope", true, 600, true) == DoorResult::UnknownDoor, "an unknown door is refused");
    Expect(TrySetDoorOpen(state, "", true, 600, true) == DoorResult::UnknownDoor, "an empty id is refused");
    Expect(TrySetDoorOpen(state, Front, true, -1, true) == DoorResult::InvalidTime, "a negative minute is refused");
    Expect(TrySetDoorOpen(state, Front, true, 1440, true) == DoorResult::InvalidTime, "minute 1440 is refused");
    Expect(TrySetDoorOpen(state, Living, true, 1440, true) == DoorResult::InvalidTime, "the time is checked before no-change");
    Expect(state == before, "no refusal changed the state");
    DoorState partial = before;
    partial.Doors.pop_back();
    Expect(TrySetDoorOpen(partial, Washroom, true, 600, true) == DoorResult::InvalidState, "an incomplete state is refused");
}

void TestRules()
{
    const DoorDef locked{"door.test.locked", "location.test", false, DoorAccess::Locked, 0, 0};
    Expect(EvaluateDoorOpen(locked, false, true, 600, false) == DoorResult::Locked, "a visitor cannot open a locked door");
    Expect(EvaluateDoorOpen(locked, false, true, 600, true) == DoorResult::Locked, "nor can a resident (no keys in this slice)");
    Expect(EvaluateDoorOpen(locked, false, false, 600, true) == DoorResult::NoChange, "a closed locked door is already closed");

    const DoorDef shop{"door.test.shop", "location.test", false, DoorAccess::BusinessHours, 7 * 60, 21 * 60};
    Expect(EvaluateDoorOpen(shop, false, true, 7 * 60 - 1, false) == DoorResult::Closed, "a shop is closed the minute before opening");
    Expect(EvaluateDoorOpen(shop, false, true, 7 * 60, false) == DoorResult::Ok, "a shop opens at its opening minute");
    Expect(EvaluateDoorOpen(shop, false, true, 21 * 60 - 1, false) == DoorResult::Ok, "a shop is open the minute before closing");
    Expect(EvaluateDoorOpen(shop, false, true, 21 * 60, false) == DoorResult::Closed, "a shop is closed at its closing minute");
    Expect(EvaluateDoorOpen(shop, false, true, 2 * 60, true) == DoorResult::Ok, "staff may open outside the hours");
    Expect(EvaluateDoorOpen(shop, true, false, 2 * 60, false) == DoorResult::Ok, "anyone may close it outside the hours");
    Expect(DoorOpensAtMinute(shop) == 7 * 60, "a shop reports when it opens");
    Expect(DoorOpensAtMinute(locked) == -1 && DoorOpensAtMinute(*FindDoor(Front)) == -1, "other kinds report no opening time");

    const DoorDef open{"door.test.open", "location.test", true, DoorAccess::Public, 0, 0};
    Expect(EvaluateDoorOpen(open, true, true, 0, false) == DoorResult::NoChange, "an open public door stays open");
    Expect(EvaluateDoorOpen(open, false, true, 0, false) == DoorResult::Ok, "a closed public door opens at midnight");
}

void TestMinuteOfDay()
{
    Expect(DoorMinuteOfDay(GameClock::FromCalendar(2042, 6, 2, 6, 0)) == 360, "06:00 is minute 360");
    Expect(DoorMinuteOfDay(GameClock::FromCalendar(2042, 6, 2, 23, 59)) == 1439, "23:59 is minute 1439");
    Expect(DoorMinuteOfDay(GameClock::FromCalendar(2042, 6, 3, 0, 0)) == 0, "midnight is minute 0");
}

void TestOffersAndPassage()
{
    const DoorDef& front = *FindDoor(Front);
    const DoorUseContext visitor{};
    const DoorUseContext invited{DoorDirection::Entry, false, true};
    const DoorUseContext exiting{DoorDirection::Exit, false, false};
    Expect(EvaluateDoorOffer(front, false, 600, visitor) == DoorOffer{DoorAction::Knock, DoorResult::Private, -1},
           "private visitors are offered Knock");
    Expect(EvaluateDoorOffer(front, true, 600, visitor).Action == DoorAction::Knock, "an open leaf does not replace invitation");
    Expect(EvaluateDoorPassage(front, 600, visitor) == DoorResult::Private, "private passage requires permission");
    Expect(EvaluateDoorOpen(front, true, true, 600, visitor) == DoorResult::Private, "an open leaf cannot bypass access via NoChange");
    Expect(EvaluateDoorPassage(front, 600, invited) == DoorResult::Ok, "an invited visitor may enter");
    Expect(EvaluateDoorOffer(front, false, 600, invited).Action == DoorAction::Open, "invitation offers Open");
    Expect(EvaluateDoorOffer(front, true, 600, invited).Action == DoorAction::Close, "authorized open leaf offers Close");
    Expect(EvaluateDoorOffer(front, false, 600, exiting).Action == DoorAction::Open, "private egress requires no invitation");
    Expect(EvaluateDoorKnock(front, 600, visitor) == DoorKnockResult{DoorResult::Knocked, DoorEvent::Knock}, "Knock has an explicit event/result");
    Expect(EvaluateDoorKnock(front, 600, invited).Event == DoorEvent::None, "invited visitor has no Knock offer");
    Expect(EvaluateDoorKnock(front, 600, exiting).Result == DoorResult::NotAvailable, "egress does not knock");
    DoorState state = MakeInitialDoorState();
    const DoorState before = state;
    (void)EvaluateDoorKnock(front, 600, visitor);
    Expect(state == before && !visitor.IsInvited, "Knock alters neither state nor invitation");
    Expect(TrySetDoorOpen(state, Front, true, 600, visitor) == DoorResult::Private && state == before, "refusal preserves state");
    Expect(TrySetDoorOpen(state, Front, true, 600, invited) == DoorResult::Ok, "invited leaf mutation succeeds");
    const DoorState opened = state;
    Expect(TrySetDoorOpen(state, Front, true, 600, visitor) == DoorResult::Private && state == opened,
           "unauthorized open-leaf mutation remains a refusal");
    const DoorDef locked{"door.test.locked", "location.test", false, DoorAccess::Locked};
    Expect(EvaluateDoorPassage(locked, 600, exiting) == DoorResult::Ok, "locked egress prevents trapping");
    Expect(EvaluateDoorPassage(locked, 600, invited) == DoorResult::Locked, "invitation cannot unlock a door");
    Expect(EvaluateDoorKnock(*FindDoor(Living), 600, visitor).Result == DoorResult::NotAvailable, "public door does not offer Knock");
}

void TestSplitWindows()
{
    const DoorOpeningWindow windows[]{{360, 690}, {930, 1140}};
    const DoorDef shop{"door.test.split", "location.test", false, DoorAccess::BusinessHours, 0, 0, windows};
    const DoorUseContext visitor{};
    const DoorUseContext exiting{DoorDirection::Exit};
    Expect(IsDoorDefinitionValid(shop), "split windows are valid");
    for (const int minute : {360, 689, 930, 1139})
    {
        Expect(EvaluateDoorPassage(shop, minute, visitor) == DoorResult::Ok, "split windows include start and minute before end");
        Expect(DoorMinutesUntilOpening(shop, minute) == 0, "inside a window the delay is zero");
    }
    for (const int minute : {0, 359, 690, 929, 1140, 1439})
    {
        Expect(EvaluateDoorPassage(shop, minute, visitor) == DoorResult::Closed, "split windows exclude their ends and the gap");
        Expect(EvaluateDoorPassage(shop, minute, exiting) == DoorResult::Ok, "closed-hours egress is always permitted");
        Expect(EvaluateDoorOpen(shop, false, true, minute, exiting) == DoorResult::Ok, "closed-hours egress can open the leaf");
        Expect(EvaluateDoorOpen(shop, true, true, minute, visitor) == DoorResult::Closed, "open shop leaf confers no closed-hours entry");
    }
    Expect(DoorMinutesUntilOpening(shop, 359) == 1, "next opening before first window");
    Expect(DoorMinutesUntilOpening(shop, 690) == 240, "next opening during split closure");
    Expect(DoorMinutesUntilOpening(shop, 1140) == 660, "next opening wraps to tomorrow");
    Expect(DoorMinutesUntilOpening(shop, 1439) == 361, "next opening across midnight");
    Expect(EvaluateDoorOffer(shop, false, 690, visitor) == DoorOffer{DoorAction::None, DoorResult::Closed, 240}, "closed offer carries next-opening delay");
    Expect(EvaluateDoorOffer(shop, false, 690, exiting).Action == DoorAction::Open, "closed shop offers egress Open");
    Expect(EvaluateDoorPassage(shop, 690, {DoorDirection::Entry, true, false}) == DoorResult::Ok, "staff may enter during closure");
    Expect(EvaluateDoorPassage(shop, 690, {DoorDirection::Entry, false, true}) == DoorResult::Closed, "private invitation does not override shop hours");
    Expect(DoorOpensAtMinute(shop) == 360, "first opening remains available to legacy callers");
    const DoorDef allDay{"door.test.allday", "location.test", false, DoorAccess::BusinessHours, 0, 1440};
    Expect(DoorMinutesUntilOpening(allDay, 0) == 0 && DoorMinutesUntilOpening(allDay, 1439) == 0, "full-day opening includes midnight and last minute");
    Expect(DoorMinutesUntilOpening(shop, -1) == -1 && DoorMinutesUntilOpening(shop, 1440) == -1, "next opening refuses invalid time");
}

void TestInvalidDefinitionsAndContexts()
{
    const DoorOpeningWindow overlap[]{{360, 690}, {689, 900}};
    const DoorOpeningWindow reversed[]{{930, 1140}, {360, 690}};
    const DoorOpeningWindow midnight[]{{1380, 60}};
    for (const auto windows : {std::span<const DoorOpeningWindow>(overlap), std::span<const DoorOpeningWindow>(reversed),
                               std::span<const DoorOpeningWindow>(midnight)})
    {
        const DoorDef invalid{"door.test.invalid", "location.test", false, DoorAccess::BusinessHours, 0, 0, windows};
        Expect(!IsDoorDefinitionValid(invalid), "overlapping, unsorted and overnight windows are rejected");
        Expect(EvaluateDoorPassage(invalid, 600, {DoorDirection::Exit}) == DoorResult::InvalidDefinition, "even egress validates inputs");
        Expect(DoorMinutesUntilOpening(invalid, 600) == -1, "bad windows have no next opening");
    }
    DoorDef invalid = *FindDoor(Front);
    invalid.Access = static_cast<DoorAccess>(255);
    Expect(EvaluateDoorOpen(invalid, false, false, 600, false) == DoorResult::InvalidDefinition, "bad access precedes NoChange");
    invalid = *FindDoor(Front);
    invalid.OpenMinute = 1;
    Expect(!IsDoorDefinitionValid(invalid), "private doors carry no business hours");
    const DoorUseContext invalidContext{static_cast<DoorDirection>(255), true, true};
    Expect(EvaluateDoorOffer(*FindDoor(Front), false, 600, invalidContext).Refusal == DoorResult::InvalidContext, "invalid direction refused");
    Expect(EvaluateDoorKnock(*FindDoor(Front), -1, {}).Result == DoorResult::InvalidTime, "Knock validates time");
    Expect(DoorMinutesUntilOpening(*FindDoor(Front), 600) == -1, "private doors have no daily opening");
    DoorState state = MakeInitialDoorState();
    const DoorState before = state;
    Expect(TrySetDoorOpen(state, Front, true, 600, invalidContext) == DoorResult::InvalidContext && state == before,
           "invalid context preserves state");
    state.Doors.back().Id = "door.unknown";
    const DoorState invalidState = state;
    Expect(TrySetDoorOpen(state, Front, true, 600, true) == DoorResult::InvalidState && state == invalidState,
           "invalid unrelated entry prevents partial mutation");
}

void TestDailyPermissionMatrix()
{
    const DoorOpeningWindow windows[]{{360, 690}, {930, 1140}};
    const DoorDef shop{"door.test.split", "location.test", false, DoorAccess::BusinessHours, 0, 0, windows};
    for (int minute = 0; minute < DoorMinutesPerDay; ++minute)
    {
        const bool serving = (minute >= 360 && minute < 690) || (minute >= 930 && minute < 1140);
        const DoorResult expected = serving ? DoorResult::Ok : DoorResult::Closed;
        for (const bool leafOpen : {false, true})
        {
            Expect(EvaluateDoorPassage(shop, minute, {}) == expected, "daily passage follows the two authored windows");
            const DoorOffer offer = EvaluateDoorOffer(shop, leafOpen, minute, {});
            Expect(offer.Refusal == expected, "daily offer permission is independent of the leaf");
            Expect(offer.Action == (serving ? (leafOpen ? DoorAction::Close : DoorAction::Open) : DoorAction::None),
                   "daily offer action follows permission and leaf position");
            Expect(EvaluateDoorOffer(shop, leafOpen, minute, {DoorDirection::Exit}).Refusal == DoorResult::Ok,
                   "every minute permits egress");
        }
        const int delay = DoorMinutesUntilOpening(shop, minute);
        const int next = (minute + delay) % DoorMinutesPerDay;
        Expect(delay >= 0 && delay < DoorMinutesPerDay && EvaluateDoorPassage(shop, next, {}) == DoorResult::Ok,
               "next opening always lands inside a serving window");
    }
    const DoorOpeningWindow adjacent[]{{0, 720}, {720, 1440}};
    const DoorDef allDay{"door.test.adjacent", "location.test", false, DoorAccess::BusinessHours, 0, 0, adjacent};
    Expect(IsDoorDefinitionValid(allDay) && DoorMinutesUntilOpening(allDay, 720) == 0,
           "adjacent half-open windows have no artificial closure");
    const DoorOpeningWindow negative[]{{-1, 100}};
    const DoorOpeningWindow tooLate[]{{100, 1441}};
    const DoorOpeningWindow empty[]{{100, 100}};
    for (const auto span : {std::span<const DoorOpeningWindow>(negative), std::span<const DoorOpeningWindow>(tooLate),
                            std::span<const DoorOpeningWindow>(empty)})
    {
        const DoorDef invalid{"door.test.invalid", "location.test", false, DoorAccess::BusinessHours, 0, 0, span};
        Expect(!IsDoorDefinitionValid(invalid), "out-of-range and zero-length windows are refused");
        Expect(EvaluateDoorKnock(invalid, 600, {}).Result == DoorResult::InvalidDefinition,
               "Knock refuses malformed definitions");
    }
    DoorDef mixed = shop;
    mixed.OpenMinute = 360;
    mixed.CloseMinute = 690;
    Expect(!IsDoorDefinitionValid(mixed), "legacy and split windows cannot both supply hours");
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
    Expect(a == b, "the same commands give the same state");
    Expect(SerialiseDoorState(a) == SerialiseDoorState(b), "and the same text");
    Expect(QueryDoorOpen(a, Bedroom) == std::optional<bool>(true) && QueryDoorOpen(a, Living) == std::optional<bool>(false),
          "the mix of accepted and refused commands landed as expected");
    Expect(SerialiseDoorState(MakeInitialDoorState()) == SerialiseDoorState(MakeInitialDoorState()), "the initial text is stable");
}

void TestRoundTrip()
{
    constexpr std::string_view legacy =
        "pw.doors/1\ncount=5\n"
        "door=door.nabapara.home_front,0\n"
        "door=door.nabapara.home_living,1\n"
        "door=door.nabapara.home_bedroom,0\n"
        "door=door.nabapara.home_kitchen,1\n"
        "door=door.nabapara.home_washroom,0\nend\n";
    const DoorStateParse historical = DeserialiseDoorState(legacy);
    Expect(historical.Ok() && historical.State == MakeInitialDoorState(), "the historical five-door fixture loads");
    Expect(SerialiseDoorState(historical.State) == legacy, "the historical fixture round trips exactly");
    DoorState state = MakeInitialDoorState();
    Play(state);
    const std::string text = SerialiseDoorState(state);
    Expect(!text.empty() && text.substr(0, 11) == "pw.doors/1\n", "the text opens with its header");
    const DoorStateParse parsed = DeserialiseDoorState(text);
    Expect(parsed.Ok() && parsed.State == state, "the state round trips");
    Expect(SerialiseDoorState(parsed.State) == text, "and re-serialises identically");
    const DoorStateParse initial = DeserialiseDoorState(SerialiseDoorState(MakeInitialDoorState()));
    Expect(initial.Ok() && initial.State == MakeInitialDoorState(), "the initial state round trips");

    DoorState invalid = state;
    invalid.Doors.pop_back();
    Expect(SerialiseDoorState(invalid).empty(), "an invalid state serialises to nothing");
}

std::string Replace(std::string text, const std::string& from, const std::string& to)
{
    const std::size_t at = text.find(from);
    Expect(at != std::string::npos, "test text contains the part to replace");
    text.replace(at, from.size(), to);
    return text;
}

void TestRefusedText()
{
    const std::string text = SerialiseDoorState(MakeInitialDoorState());
    for (std::size_t length = 0; length < text.size(); ++length)
        Expect(!DeserialiseDoorState(std::string_view(text).substr(0, length)).Ok(), "every truncation is refused");
    Expect(!DeserialiseDoorState(text + "x").Ok(), "trailing data is refused");
    Expect(!DeserialiseDoorState(text + "\n").Ok(), "a trailing blank line is refused");
    Expect(!DeserialiseDoorState(Replace(text, "pw.doors/1", "pw.doors/2")).Ok(), "a wrong header is refused");
    Expect(!DeserialiseDoorState(Replace(text, "count=5", "count=4")).Ok(), "a wrong count is refused");
    Expect(!DeserialiseDoorState(Replace(text, "count=5", "count=05")).Ok(), "a non-canonical count is refused");
    Expect(!DeserialiseDoorState(Replace(text, "home_front,0", "home_front,2")).Ok(), "a bad flag is refused");
    Expect(!DeserialiseDoorState(Replace(text, "home_front,0", "home_front")).Ok(), "a missing flag is refused");
    Expect(!DeserialiseDoorState(Replace(text, "home_front,0", "home_nowhere,0")).Ok(), "an unknown door is refused");
    Expect(!DeserialiseDoorState(Replace(text, "door.nabapara.home_living,1", "door.nabapara.home_front,0")).Ok(), "a repeated door is refused");
    const std::string swapped = Replace(Replace(Replace(text, "home_living,1", "home_TMP,1"), "home_bedroom,0", "home_living,1"), "home_TMP,1", "home_bedroom,0");
    Expect(!DeserialiseDoorState(swapped).Ok(), "doors out of table order are refused");
    const DoorStateParse bad = DeserialiseDoorState("garbage\n");
    Expect(!bad.Ok() && bad.State == DoorState{}, "a refusal returns a default state");
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
    TestOffersAndPassage();
    TestSplitWindows();
    TestInvalidDefinitionsAndContexts();
    TestDailyPermissionMatrix();
    TestDeterminism();
    TestRoundTrip();
    TestRefusedText();
    std::cout << "PerfectWorld doors: " << Assertions << " assertions passed\n";
    return 0;
}
