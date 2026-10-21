#include "DoorBell.h"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace PerfectWorld;

namespace
{
int DbAssertions = 0;
void DbExpect(bool condition, const std::string& message)
{
    ++DbAssertions;
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

constexpr std::int32_t DbRowHouseHome = 18 * 60;   // inside 16:00-19:30
constexpr std::int32_t DbRowHouseLate = 20 * 60 + 30;
constexpr std::int32_t DbMorning = 9 * 60;

const DoorDef& DbDoor(std::string_view id)
{
    const DoorDef* door = FindDoor(id);
    if (door == nullptr)
    {
        std::cerr << "FAIL: authored door missing: " << id << '\n';
        std::exit(1);
    }
    return *door;
}

DoorUseContext DbStranger(bool hostHome, bool knocked = false)
{
    DoorUseContext context;
    context.Direction = DoorDirection::Entry;
    context.IsHostHome = hostHome;
    context.HasKnocked = knocked;
    return context;
}

void DbTableIsClassified()
{
    // Every Private door is either a bell entrance or a room door: a new Private door forces a decision here.
    for (const DoorDef& door : AuthoredDoors())
    {
        const bool entrance = IsDoorBellEntranceId(door.Id);
        const bool room = IsDoorPrivateRoomId(door.Id);
        DbExpect(!(entrance && room), std::string("not both entrance and room: ") + std::string(door.Id));
        if (door.Access == DoorAccess::Private)
            DbExpect(entrance || room, std::string("a Private door must be classified as entrance or room: ") + std::string(door.Id));
        else
            DbExpect(!entrance && !room, std::string("only Private doors are classified: ") + std::string(door.Id));
        DbExpect(DoorHasBell(door) == entrance, std::string("a bell hangs at exactly the entrances: ") + std::string(door.Id));
    }
    for (const std::string_view id : DoorBellEntranceIds)
        DbExpect(FindDoor(id) != nullptr, "every entrance id is an authored door");
    for (const std::string_view id : DoorPrivateRoomIds)
        DbExpect(FindDoor(id) != nullptr, "every room id is an authored door");
    DbExpect(DoorBellEntranceIds.size() == 8, "eight dwelling entrances: two home fronts, the caretaker, four flats and houses");
    // No roster change: this feature adds no door and no save field.
    DbExpect(AuthoredDoors().size() == 26 && ValidateDoorTable(), "the twenty-six-door roster is untouched");
    // Public, business-hours and locked doors never have a bell.
    DbExpect(!DoorHasBell(DbDoor("door.nabapara.tall_flats_front")), "the shared flats entry is public and has no bell");
    DbExpect(!DoorHasBell(DbDoor("door.nabapara.stationery_front")), "a shop has no bell");
    DbExpect(!DoorHasBell(DbDoor("door.nabapara.home_bedroom")), "a room door has no bell");
}

void DbRingMapsTheCoreKnock()
{
    const DoorDef& row = DbDoor("door.nabapara.tea_row_house_front");
    // A resident home in visiting hours answers.
    DoorRingResult ring = EvaluateDoorRing(row, DbRowHouseHome, DbStranger(true));
    DbExpect(ring.Outcome == DoorRingOutcome::Answered, "home in visiting hours: answered");
    DbExpect(ring.Knock == EvaluateDoorKnock(row, DbRowHouseHome, DbStranger(true)), "the ring carries the core's own knock");
    // Nobody home.
    ring = EvaluateDoorRing(row, DbRowHouseHome, DbStranger(false));
    DbExpect(ring.Outcome == DoorRingOutcome::NoAnswer && ring.MinutesUntilVisiting == -1, "nobody home: no answer");
    // Home, outside visiting hours: the resident's refusal, with the minutes until visiting.
    ring = EvaluateDoorRing(row, DbRowHouseLate, DbStranger(true));
    DbExpect(ring.Outcome == DoorRingOutcome::NotReceiving, "home but late: not receiving");
    DbExpect(ring.MinutesUntilVisiting == DoorMinutesUntilVisiting(row, DbRowHouseLate) && ring.MinutesUntilVisiting > 0,
             "the refusal names when visiting resumes, from the core");
    // A dwelling with no visiting hours never receives a stranger: a resident home only refuses.
    const DoorDef& home = DbDoor("door.nabapara.home_front");
    ring = EvaluateDoorRing(home, DbMorning, DbStranger(true));
    DbExpect(ring.Outcome == DoorRingOutcome::NotReceiving && ring.MinutesUntilVisiting == -1, "no visiting hours: not receiving, no time given");
    ring = EvaluateDoorRing(home, DbMorning, DbStranger(false));
    DbExpect(ring.Outcome == DoorRingOutcome::NoAnswer, "no visiting hours and nobody home: no answer");
    // A resident, an invited person and an already admitted visitor need no bell.
    DoorUseContext resident = DbStranger(false);
    resident.IsResident = true;
    DbExpect(EvaluateDoorRing(row, DbRowHouseLate, resident).Outcome == DoorRingOutcome::NoBellNeeded, "a resident needs no bell");
    DoorUseContext invited = DbStranger(false);
    invited.IsInvited = true;
    DbExpect(EvaluateDoorRing(row, DbRowHouseLate, invited).Outcome == DoorRingOutcome::NoBellNeeded, "an invited person needs no bell");
    DbExpect(EvaluateDoorRing(row, DbRowHouseHome, DbStranger(true, true)).Outcome == DoorRingOutcome::NoBellNeeded,
             "an already answered ring needs no second ring");
    // A ring from the inside side is still an Entry question: the direction is forced.
    DoorUseContext exiting = DbStranger(true);
    exiting.Direction = DoorDirection::Exit;
    DbExpect(EvaluateDoorRing(row, DbRowHouseHome, exiting).Outcome == DoorRingOutcome::Answered, "the bell is only ever an entry question");
    // No bell: nothing is rung. Invalid time is refused, not answered.
    DbExpect(EvaluateDoorRing(DbDoor("door.nabapara.tall_flats_front"), DbMorning, DbStranger(true)).Outcome == DoorRingOutcome::NoBell,
             "a public door has no bell to ring");
    DbExpect(EvaluateDoorRing(DbDoor("door.nabapara.home_bedroom"), DbMorning, DbStranger(true)).Outcome == DoorRingOutcome::NoBell,
             "a room door has no bell to ring");
    DbExpect(EvaluateDoorRing(row, 1440, DbStranger(true)).Outcome == DoorRingOutcome::Refused, "an invalid minute is refused");
    DbExpect(EvaluateDoorRing(row, -1, DbStranger(true)).Outcome == DoorRingOutcome::Refused, "a negative minute is refused");
}

void DbRingAgreesWithPassageForEveryBellDoor()
{
    // No parallel rule: for every bell door at every minute and both host facts, a ring is Answered exactly when the core would let a
    // visitor who knocked through, and a stranger who has not rung is never let through.
    for (const DoorDef& door : AuthoredDoors())
    {
        if (!DoorHasBell(door))
            continue;
        for (std::int32_t minute = 0; minute < DoorMinutesPerDay; ++minute)
        {
            for (const bool hostHome : {false, true})
            {
                const DoorRingResult ring = EvaluateDoorRing(door, minute, DbStranger(hostHome));
                const bool admittedByCore = EvaluateDoorPassage(door, minute, DbStranger(hostHome, true)) == DoorResult::Ok;
                DbExpect((ring.Outcome == DoorRingOutcome::Answered) == admittedByCore, std::string("ring agrees with the core's passage: ") + std::string(door.Id));
                DbExpect(EvaluateDoorPassage(door, minute, DbStranger(hostHome)) == DoorResult::Private,
                         "a stranger who has not rung is refused as Private");
                DbExpect(!EvaluateDoorThreshold(door, minute, DbStranger(hostHome)).bMayCross, "no ring, no crossing");
                if (ring.Outcome == DoorRingOutcome::Answered)
                    DbExpect(hostHome && DoorMinutesUntilVisiting(door, minute) == 0, "an answer needs a resident home inside a window");
            }
        }
    }
}

void DbThresholdFollowsThePassageRule()
{
    const DoorDef& row = DbDoor("door.nabapara.tea_row_house_front");
    // Exit is always allowed, for a stranger, with the leaf in any state (the leaf is not even an input), at any hour.
    DoorUseContext leaving = DbStranger(false);
    leaving.Direction = DoorDirection::Exit;
    for (std::int32_t minute : {0, DbMorning, DbRowHouseHome, DbRowHouseLate, 1439})
    {
        const DoorThresholdVerdict verdict = EvaluateDoorThreshold(row, minute, leaving);
        DbExpect(verdict.bMayCross && verdict.Why == DoorResult::Ok, "exit is never refused");
    }
    // An unadmitted stranger is stopped with the core's Private refusal, even though the leaf is open (nothing says it is open).
    DoorThresholdVerdict verdict = EvaluateDoorThreshold(row, DbRowHouseHome, DbStranger(true));
    DbExpect(!verdict.bMayCross && verdict.Why == DoorResult::Private, "an unadmitted visitor is stopped at the threshold");
    // An answered ring admits, while the window and the resident hold.
    DbExpect(EvaluateDoorThreshold(row, DbRowHouseHome, DbStranger(true, true)).bMayCross, "an answered ring crosses");
    DbExpect(!EvaluateDoorThreshold(row, DbRowHouseLate, DbStranger(true, true)).bMayCross, "an answered ring does not outlast the visiting window");
    DbExpect(!EvaluateDoorThreshold(row, DbRowHouseHome, DbStranger(false, true)).bMayCross, "an answered ring needs the resident still home");
    DoorUseContext resident = DbStranger(false);
    resident.IsResident = true;
    DbExpect(EvaluateDoorThreshold(row, DbRowHouseLate, resident).bMayCross, "a resident crosses at any hour");
    // Fail closed.
    DbExpect(!EvaluateDoorThreshold(row, 1440, DbStranger(true, true)).bMayCross, "an invalid minute never admits");
    DbExpect(EvaluateDoorThreshold(row, 1440, DbStranger(true, true)).Why == DoorResult::InvalidTime, "and says why");
    DoorUseContext broken = DbStranger(true, true);
    broken.Direction = static_cast<DoorDirection>(7);
    DbExpect(!EvaluateDoorThreshold(row, DbRowHouseHome, broken).bMayCross, "an invalid direction never admits");
    // Room doors of a home: a stranger is not admitted by any ring (no visiting hours), a resident is.
    const DoorDef& bedroom = DbDoor("door.nabapara.home_bedroom");
    DbExpect(!EvaluateDoorThreshold(bedroom, DbMorning, DbStranger(true, true)).bMayCross, "a stranger cannot reach the bedroom");
    DbExpect(EvaluateDoorThreshold(bedroom, DbMorning, resident).bMayCross, "the resident can");
    // Public and business doors are not this guard's business and are permissive or not exactly as the core says.
    DbExpect(EvaluateDoorThreshold(DbDoor("door.nabapara.tall_flats_front"), DbMorning, DbStranger(false)).bMayCross, "a public door crosses");
    DbExpect(!EvaluateDoorThreshold(DbDoor("door.nabapara.stationery_front"), 7 * 60, DbStranger(false)).bMayCross, "a shop is closed at 07:00");
}

void DbHostClosesBehindTheVisitor()
{
    const DoorDef& row = DbDoor("door.nabapara.tea_row_house_front");
    DbExpect(ShouldHostCloseDoor(row, true, false, false, false), "an open dwelling leaf, a stranger gone: closed by the household");
    DbExpect(!ShouldHostCloseDoor(row, false, false, false, false), "a shut leaf needs nothing");
    DbExpect(!ShouldHostCloseDoor(row, true, false, false, true), "not while the visitor is still at the door");
    DbExpect(!ShouldHostCloseDoor(row, true, true, false, false), "a resident's own door stays as they left it");
    DbExpect(!ShouldHostCloseDoor(row, true, false, true, false), "an invited visitor's leaf stays as they left it");
    DbExpect(!ShouldHostCloseDoor(DbDoor("door.nabapara.tall_flats_front"), true, false, false, false), "a public door is nobody's to close");
    DbExpect(!ShouldHostCloseDoor(DbDoor("door.nabapara.home_bedroom"), true, false, false, false), "a room door is not an entrance");
    DbExpect(!ShouldHostCloseDoor(DbDoor("door.nabapara.stationery_front"), true, false, false, false), "a shop door is not a dwelling");
}

void DbFamilyStartShell()
{
    DbExpect(IsFamilyStartShell(false, "location.nabapara.opening_home"), "a family origin lives in the opening shell it starts in");
    DbExpect(!IsFamilyStartShell(true, "location.nabapara.opening_home"), "a care origin does not");
    DbExpect(!IsFamilyStartShell(false, "location.nabapara.care_home"), "and a family origin is not resident of the care home");
    DbExpect(!IsFamilyStartShell(false, "location.nabapara.tall_flats_unit_a"), "nor of a neighbour's flat");
    DbExpect(!IsFamilyStartShell(false, ""), "an empty location grants nothing");
}

void DbKeysAreStable()
{
    DbExpect(DoorRingKey(DoorRingOutcome::Answered) == "door.bell.answered", "answered key");
    DbExpect(DoorRingKey(DoorRingOutcome::NoAnswer) == "door.bell.no_answer", "no answer key");
    DbExpect(DoorRingKey(DoorRingOutcome::NotReceiving) == "door.bell.not_receiving", "not receiving key");
    DbExpect(DoorRingKey(DoorRingOutcome::NoBell) == "door.bell.no_bell", "no bell key");
    DbExpect(DoorRingKey(DoorRingOutcome::NoBellNeeded) == "door.bell.not_needed", "not needed key");
    DbExpect(DoorRingKey(DoorRingOutcome::Refused) == "door.bell.refused", "refused key");
}
} // namespace

int main()
{
    DbTableIsClassified();
    DbRingMapsTheCoreKnock();
    DbRingAgreesWithPassageForEveryBellDoor();
    DbThresholdFollowsThePassageRule();
    DbHostClosesBehindTheVisitor();
    DbFamilyStartShell();
    DbKeysAreStable();
    std::cout << "DoorBellTests: " << DbAssertions << " assertions passed\n";
    return 0;
}
