#include "Doors.h"

#include <array>
#include <charconv>
#include <utility>

namespace PerfectWorld
{
namespace
{
constexpr std::string_view DoorsHeader = "pw.doors/2";
constexpr std::string_view DoorsEnd = "end";

constexpr std::string_view OpeningHome = "location.nabapara.opening_home";

/**
 * The opening home (LANE_LIFE_PLAN section C: entry/utility space, living/eating room, sleeping/study room, kitchen,
 * washroom). The front door and the bedroom door are private; the others are ordinary household doors. Each id is
 * named for the room it enters. Keep these five entries first for historical /1 saves.
 */
constexpr DoorOpeningWindow ChaiHours[]{{360, 690}, {930, 1140}};
constexpr DoorOpeningWindow DoorsStationeryHours[]{{480, 780}, {900, 1170}};
constexpr std::string_view CareHome = "location.nabapara.care_home";
/**
 * NL-01: the kit frontages (LANE_BUILDING_INVENTORY section 4) and the two dwellings inside tall_flats, whose ids follow
 * the kit's rooms unit_a and unit_b. Each dwelling is its own household (its own location id, so residency is per flat);
 * the stair passage behind tall_flats_front is common to both and public. Dwelling leaves are one-way and open inward.
 * Visiting hours: the caretaker (Subrata Dey, LANE_LIFE_PLAN) receives late morning and evening, around his midday
 * rest; the flats and the row house receive in the evening.
 */
constexpr DoorOpeningWindow CaretakerVisiting[]{{600, 750}, {1050, 1230}};
constexpr DoorOpeningWindow FlatsVisiting[]{{1020, 1230}};
constexpr DoorOpeningWindow RowHouseVisiting[]{{960, 1170}};
// NR-05 frontages (repairer's split day, closing 18:30; the hardware store's two sittings) and NR-06's first upper flat.
// The old house and the upper flat have nobody living there yet: a knock gets NoAnswer, so entry is by invitation only.
constexpr DoorOpeningWindow DoorsWorkshopHours[]{{480, 720}, {840, 1110}};
constexpr DoorOpeningWindow DoorsHardwareHours[]{{540, 780}, {900, 1200}};
constexpr DoorOpeningWindow DoorsOldHouseVisiting[]{{960, 1170}};
constexpr std::array<DoorDef, 20> DoorTable{{
    {"door.nabapara.home_front", OpeningHome, false, DoorAccess::Private, 0, 0},
    {"door.nabapara.home_living", OpeningHome, true, DoorAccess::Public, 0, 0},
    {"door.nabapara.home_bedroom", OpeningHome, false, DoorAccess::Private, 0, 0},
    {"door.nabapara.home_kitchen", OpeningHome, true, DoorAccess::Public, 0, 0},
    {"door.nabapara.home_washroom", OpeningHome, false, DoorAccess::Public, 0, 0},
    {"door.nabapara.shanti_chai_shutter", "location.nabapara.shanti_chai", false, DoorAccess::BusinessHours, 0, 0, ChaiHours},
    {"door.nabapara.care_front", CareHome, false, DoorAccess::Private, 0, 0, {}, false, 1},
    {"door.nabapara.care_dormitory", CareHome, false, DoorAccess::Private},
    {"door.nabapara.care_common", CareHome, true, DoorAccess::Public},
    {"door.nabapara.care_washroom", CareHome, false, DoorAccess::Public},
    {"door.nabapara.stationery_front", "location.nabapara.stationery", false, DoorAccess::BusinessHours, 0, 0, DoorsStationeryHours},
    {"door.nabapara.caretaker_flat_front", "location.nabapara.caretaker_flat", false, DoorAccess::Private, 0, 0, {}, false, 1, CaretakerVisiting},
    {"door.nabapara.tall_flats_front", "location.nabapara.tall_flats", false, DoorAccess::Public},
    {"door.nabapara.tall_flats_unit_a", "location.nabapara.tall_flats_unit_a", false, DoorAccess::Private, 0, 0, {}, false, 1, FlatsVisiting},
    {"door.nabapara.tall_flats_unit_b", "location.nabapara.tall_flats_unit_b", false, DoorAccess::Private, 0, 0, {}, false, 1, FlatsVisiting},
    {"door.nabapara.tea_row_house_front", "location.nabapara.tea_row_house", false, DoorAccess::Private, 0, 0, {}, false, 1, RowHouseVisiting},
    {"door.nabapara.repair_workshop_front", "location.nabapara.repair_workshop", false, DoorAccess::BusinessHours, 0, 0, DoorsWorkshopHours, false, 1},
    {"door.nabapara.hardware_store_front", "location.nabapara.hardware_store", false, DoorAccess::BusinessHours, 0, 0, DoorsHardwareHours, false, 1},
    {"door.nabapara.old_house_front", "location.nabapara.old_house", false, DoorAccess::Private, 0, 0, {}, false, 1, DoorsOldHouseVisiting},
    {"door.nabapara.tall_flats_upper_a", "location.nabapara.tall_flats_upper_a", false, DoorAccess::Private, 0, 0, {}, false, 1},
}};

// Every roster a save may hold, as a prefix length of DoorTable. /1 wrote five (2026-10-13); its reader also took the
// full ten- and eleven-door tables of their day. /2 wrote ten (D2), eleven (stationery, LL-02 2a), sixteen (NL-01) and
// twenty (NR-05/NR-06). Never add an arbitrary prefix: only a count some writer or reader really used.
constexpr std::array<DoorRosterEdition, 7> RosterEditions{{{"pw.doors/1", 5}, {"pw.doors/1", 10}, {"pw.doors/1", 11},
    {DoorsHeader, 10}, {DoorsHeader, 11}, {DoorsHeader, 16}, {DoorsHeader, DoorTable.size()}}};
static_assert(RosterEditions.back().Count == DoorTable.size(), "today's roster is the last edition");

constexpr std::array<std::string_view, DoorAccessCount> AccessIds{"public", "private", "locked", "business_hours"};

/** Half-open windows: sorted, disjoint, non-empty, within one day. */
bool AreWindowsValid(std::span<const DoorOpeningWindow> windows) noexcept
{
    std::int32_t previousClose = 0;
    for (const auto& window : windows)
    {
        if (window.OpenMinute < previousClose || window.OpenMinute >= window.CloseMinute ||
            window.CloseMinute > DoorMinutesPerDay)
            return false;
        previousClose = window.CloseMinute;
    }
    return true;
}

/** 0 inside a window, else the distance to the next opening (wrapping to tomorrow). Windows are valid and nonempty. */
std::int32_t MinutesUntilWindow(std::span<const DoorOpeningWindow> windows, std::int32_t minuteOfDay) noexcept
{
    for (const auto& window : windows)
    {
        if (minuteOfDay < window.OpenMinute)
            return window.OpenMinute - minuteOfDay;
        if (minuteOfDay < window.CloseMinute)
            return 0;
    }
    return DoorMinutesPerDay - minuteOfDay + windows.front().OpenMinute;
}

bool IsPhysicalPairValid(const DoorDef& door, bool open, std::int8_t side) noexcept
{
    return open ? ((side == -1 || side == 1) && (door.bTwoWay || side == door.PreferredSwingSide)) : side == 0;
}

bool NextLine(std::string_view text, std::size_t& position, std::string_view& line)
{
    const std::size_t newline = text.find('\n', position);
    if (newline == std::string_view::npos)
        return false; // no newline left: what a truncated text looks like
    line = text.substr(position, newline - position);
    position = newline + 1;
    return true;
}

bool KeyValue(std::string_view line, std::string_view key, std::string_view& value)
{
    if (line.size() <= key.size() || line.substr(0, key.size()) != key || line[key.size()] != '=')
        return false;
    value = line.substr(key.size() + 1);
    return true;
}

/** Canonical non-negative decimal only: no sign, no leading zeros, nothing trailing. */
bool ParseCount(std::string_view text, std::size_t& out)
{
    if (text.empty() || text.size() > 9 || (text.size() > 1 && text.front() == '0'))
        return false;
    for (const char c : text)
    {
        if (c < '0' || c > '9')
            return false;
    }
    std::size_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc() || end != text.data() + text.size())
        return false;
    out = value;
    return true;
}
} // namespace

std::span<const DoorDef> AuthoredDoors() noexcept
{
    return DoorTable;
}

const DoorDef* FindDoor(std::string_view id) noexcept
{
    for (const DoorDef& door : DoorTable)
    {
        if (door.Id == id)
            return &door;
    }
    return nullptr;
}

bool ValidateDoorTable() noexcept
{
    for (std::size_t i = 0; i < DoorTable.size(); ++i)
    {
        const DoorDef& door = DoorTable[i];
        if (!IsDoorDefinitionValid(door))
            return false;
        for (std::size_t j = 0; j < i; ++j)
        {
            if (DoorTable[j].Id == door.Id)
                return false;
        }
    }
    return true;
}

bool IsDoorDefinitionValid(const DoorDef& door) noexcept
{
    if (!door.Id.starts_with("door.") || door.Id.size() == 5 || door.LocationId.empty() ||
        door.Id.find_first_of(",\r\n=") != std::string_view::npos ||
        static_cast<std::size_t>(door.Access) >= DoorAccessCount ||
        (door.Access == DoorAccess::Locked && door.bInitiallyOpen) ||
        (door.PreferredSwingSide != -1 && door.PreferredSwingSide != 1))
        return false;
    // Visiting hours belong to private dwellings only.
    if (!door.VisitingWindows.empty() && (door.Access != DoorAccess::Private || !AreWindowsValid(door.VisitingWindows)))
        return false;
    if (door.Access != DoorAccess::BusinessHours)
        return door.OpenMinute == 0 && door.CloseMinute == 0 && door.OpeningWindows.empty();
    if (door.OpeningWindows.empty())
        return door.OpenMinute >= 0 && door.OpenMinute < door.CloseMinute && door.CloseMinute <= DoorMinutesPerDay;
    if (door.OpenMinute != 0 || door.CloseMinute != 0)
        return false;
    return AreWindowsValid(door.OpeningWindows);
}

std::string_view DoorAccessId(DoorAccess access) noexcept
{
    const std::size_t index = static_cast<std::size_t>(access);
    return index < DoorAccessCount ? AccessIds[index] : std::string_view{};
}

// Indexed by DoorResult, in enum order (Ok .. Knocked). Ok and Knocked have no reason key.
constexpr std::array<std::string_view, 12> DoorsResultCodes{"ok", "unknown_door", "locked", "private", "closed_hours", "no_change",
    "invalid_time", "invalid_definition", "invalid_context", "invalid_state", "not_available", "knocked"};
constexpr std::array<std::string_view, 12> DoorsReasonKeys{"", "door.reason.unknown", "door.reason.locked", "door.reason.private",
    "door.reason.closed_hours", "door.reason.no_change", "door.reason.invalid_time", "door.reason.invalid_definition",
    "door.reason.invalid_context", "door.reason.invalid_state", "door.reason.not_available", ""};
static_assert(static_cast<std::size_t>(DoorResult::Knocked) + 1 == DoorsResultCodes.size(), "one code per DoorResult");

std::string_view DoorResultCode(DoorResult result) noexcept
{
    const std::size_t index = static_cast<std::size_t>(result);
    return index < DoorsResultCodes.size() ? DoorsResultCodes[index] : std::string_view{};
}

std::string_view DoorResultReasonKey(DoorResult result) noexcept
{
    const std::size_t index = static_cast<std::size_t>(result);
    return index < DoorsReasonKeys.size() ? DoorsReasonKeys[index] : std::string_view{};
}

std::int32_t DoorMinuteOfDay(GameInstant instant) noexcept
{
    const CalendarTime calendar = GameClock::ToCalendar(instant);
    return calendar.Hour * 60 + calendar.Minute;
}

std::int32_t DoorOpensAtMinute(const DoorDef& door) noexcept
{
    if (!IsDoorDefinitionValid(door) || door.Access != DoorAccess::BusinessHours)
        return -1;
    return door.OpeningWindows.empty() ? door.OpenMinute : door.OpeningWindows.front().OpenMinute;
}

std::int32_t DoorMinutesUntilOpening(const DoorDef& door, std::int32_t minuteOfDay) noexcept
{
    if (minuteOfDay < 0 || minuteOfDay >= DoorMinutesPerDay || !IsDoorDefinitionValid(door) ||
        door.Access != DoorAccess::BusinessHours)
        return -1;
    const DoorOpeningWindow legacy{door.OpenMinute, door.CloseMinute};
    const auto windows = door.OpeningWindows.empty() ? std::span<const DoorOpeningWindow>(&legacy, 1) : door.OpeningWindows;
    return MinutesUntilWindow(windows, minuteOfDay);
}

std::int32_t DoorMinutesUntilVisiting(const DoorDef& door, std::int32_t minuteOfDay) noexcept
{
    if (minuteOfDay < 0 || minuteOfDay >= DoorMinutesPerDay || !IsDoorDefinitionValid(door) || door.VisitingWindows.empty())
        return -1;
    return MinutesUntilWindow(door.VisitingWindows, minuteOfDay);
}

DoorResult EvaluateDoorPassage(const DoorDef& door, std::int32_t minuteOfDay, const DoorUseContext& context) noexcept
{
    if (minuteOfDay < 0 || minuteOfDay >= DoorMinutesPerDay)
        return DoorResult::InvalidTime;
    if (!IsDoorDefinitionValid(door))
        return DoorResult::InvalidDefinition;
    if (context.Direction != DoorDirection::Entry && context.Direction != DoorDirection::Exit)
        return DoorResult::InvalidContext;
    if (context.Direction == DoorDirection::Exit)
        return DoorResult::Ok;
    switch (door.Access)
    {
    case DoorAccess::Public: return DoorResult::Ok;
    case DoorAccess::Private:
        // A knock admits only when it would be answered now: someone home, inside visiting hours.
        return context.IsResident || context.IsInvited ||
                       (context.HasKnocked && context.IsHostHome && DoorMinutesUntilVisiting(door, minuteOfDay) == 0)
                   ? DoorResult::Ok
                   : DoorResult::Private;
    case DoorAccess::Locked: return DoorResult::Locked;
    case DoorAccess::BusinessHours:
        return context.IsResident || DoorMinutesUntilOpening(door, minuteOfDay) == 0 ? DoorResult::Ok : DoorResult::Closed;
    }
    return DoorResult::InvalidDefinition;
}

DoorOffer EvaluateDoorOffer(const DoorDef& door, bool currentlyOpen, std::int32_t minuteOfDay,
                           const DoorUseContext& context) noexcept
{
    const DoorResult permission = EvaluateDoorPassage(door, minuteOfDay, context);
    if (permission == DoorResult::Private)
        return {DoorAction::Knock, permission, -1};
    if (permission != DoorResult::Ok)
        return {DoorAction::None, permission, permission == DoorResult::Closed ? DoorMinutesUntilOpening(door, minuteOfDay) : -1};
    return {currentlyOpen ? DoorAction::Close : DoorAction::Open, DoorResult::Ok, -1};
}

DoorKnockResult EvaluateDoorKnock(const DoorDef& door, std::int32_t minuteOfDay, const DoorUseContext& context) noexcept
{
    const DoorResult permission = EvaluateDoorPassage(door, minuteOfDay, context);
    if (permission != DoorResult::Private)
        return {permission == DoorResult::Ok ? DoorResult::NotAvailable : permission, DoorEvent::None};
    if (!context.IsHostHome)
        return {DoorResult::Knocked, DoorEvent::Knock, DoorKnockAnswer::NoAnswer, -1};
    const std::int32_t untilVisiting = DoorMinutesUntilVisiting(door, minuteOfDay);
    if (untilVisiting == 0)
        return {DoorResult::Knocked, DoorEvent::Knock, DoorKnockAnswer::Admitted, -1};
    return {DoorResult::Knocked, DoorEvent::Knock, DoorKnockAnswer::NotReceiving, untilVisiting};
}

DoorResult EvaluateDoorOpen(const DoorDef& door, bool currentlyOpen, bool open, std::int32_t minuteOfDay,
                           const DoorUseContext& context) noexcept
{
    const DoorResult permission = EvaluateDoorPassage(door, minuteOfDay, context);
    if (permission == DoorResult::InvalidTime || permission == DoorResult::InvalidDefinition || permission == DoorResult::InvalidContext)
        return permission;
    // Opening always evaluates permission, including when the physical leaf is already open.
    if (open && permission != DoorResult::Ok)
        return permission;
    if (currentlyOpen == open)
        return DoorResult::NoChange;
    return DoorResult::Ok;
}

DoorResult EvaluateDoorOpen(const DoorDef& door, bool currentlyOpen, bool open, std::int32_t minuteOfDay, bool bIsResident) noexcept
{
    return EvaluateDoorOpen(door, currentlyOpen, open, minuteOfDay, DoorUseContext{DoorDirection::Entry, bIsResident, false});
}

DoorState MakeInitialDoorState()
{
    DoorState state;
    state.Doors.reserve(DoorTable.size());
    for (const DoorDef& door : DoorTable)
        state.Doors.push_back(DoorEntry{std::string(door.Id), door.bInitiallyOpen,
                                       door.bInitiallyOpen ? door.PreferredSwingSide : std::int8_t{0}});
    return state;
}

bool IsDoorStateValid(const DoorState& state) noexcept
{
    if (state.Doors.size() != DoorTable.size())
        return false;
    for (std::size_t i = 0; i < DoorTable.size(); ++i)
    {
        if (state.Doors[i].Id != DoorTable[i].Id ||
            !IsPhysicalPairValid(DoorTable[i], state.Doors[i].Open, state.Doors[i].SwingSide))
            return false;
    }
    return true;
}

std::optional<bool> QueryDoorOpen(const DoorState& state, std::string_view doorId) noexcept
{
    for (const DoorEntry& entry : state.Doors)
    {
        if (entry.Id == doorId)
            return entry.Open;
    }
    return std::nullopt;
}

DoorResult TrySetDoorOpen(DoorState& state, std::string_view doorId, bool open, std::int32_t minuteOfDay, bool bIsResident)
{
    return TrySetDoorOpen(state, doorId, open, minuteOfDay, DoorUseContext{DoorDirection::Entry, bIsResident, false});
}

DoorResult TrySetDoorOpen(DoorState& state, std::string_view doorId, bool open, std::int32_t minuteOfDay,
                          const DoorUseContext& context)
{
    const DoorDef* door = FindDoor(doorId);
    if (door == nullptr)
        return DoorResult::UnknownDoor;
    if (!IsDoorStateValid(state))
        return DoorResult::InvalidState;
    for (DoorEntry& entry : state.Doors)
    {
        if (entry.Id != doorId)
            continue;
        const DoorResult result = EvaluateDoorOpen(*door, entry.Open, open, minuteOfDay, context);
        if (result == DoorResult::Ok)
        {
            entry.Open = open;
            entry.SwingSide = open ? door->PreferredSwingSide : std::int8_t{0};
        }
        return result;
    }
    return DoorResult::UnknownDoor; // authored, but this state does not hold it
}

DoorResult ApplyDoorSettlement(DoorState& state, std::string_view doorId, bool bIsOpenNow, std::int8_t swingSide)
{
    const DoorDef* door = FindDoor(doorId);
    if (door == nullptr)
        return DoorResult::UnknownDoor;
    if (!IsDoorStateValid(state) || !IsPhysicalPairValid(*door, bIsOpenNow, swingSide))
        return DoorResult::InvalidState;
    for (DoorEntry& entry : state.Doors)
    {
        if (entry.Id != doorId)
            continue;
        if (entry.Open == bIsOpenNow && entry.SwingSide == swingSide)
            return DoorResult::NoChange;
        entry.Open = bIsOpenNow;
        entry.SwingSide = swingSide;
        return DoorResult::Ok;
    }
    return DoorResult::UnknownDoor;
}

std::string SerialiseDoorState(const DoorState& state)
{
    if (!IsDoorStateValid(state))
        return {};
    std::string text(DoorsHeader);
    text += "\ncount=" + std::to_string(state.Doors.size());
    for (const DoorEntry& entry : state.Doors)
        text += "\ndoor=" + entry.Id + "," + (entry.Open ? "1" : "0") + "," + std::to_string(entry.SwingSide);
    text += "\n";
    text += DoorsEnd;
    text += "\n";
    return text;
}

DoorStateParse DeserialiseDoorState(std::string_view text)
{
    DoorStateParse result;
    const auto refuse = [&](const char* message) {
        result.Error = message;
        result.State = DoorState{};
        return result;
    };
    std::size_t position = 0;
    std::string_view line;
    std::string_view value;
    if (!NextLine(text, position, line) || (line != DoorsHeader && line != "pw.doors/1"))
        return refuse("The door text has no recognised header.");
    const bool legacy = line == "pw.doors/1";
    std::size_t count = 0;
    if (!NextLine(text, position, line) || !KeyValue(line, "count", value) || !ParseCount(value, count))
        return refuse("The door text has no valid count line.");
    // Explicit roster editions only (RosterEditions); never an arbitrary prefix.
    const std::string_view header = legacy ? std::string_view("pw.doors/1") : DoorsHeader;
    bool knownRoster = false;
    for (const DoorRosterEdition& edition : RosterEditions)
        knownRoster = knownRoster || (edition.Header == header && edition.Count == count);
    if (!knownRoster)
        return refuse("The door text does not hold exactly the authored doors.");
    const bool rosterMigration = count != DoorTable.size();
    DoorState state = MakeInitialDoorState();
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!NextLine(text, position, line) || !KeyValue(line, "door", value))
            return refuse("The door text is truncated or has a bad door line.");
        const std::size_t comma = value.find(',');
        if (comma == std::string_view::npos)
            return refuse("The door text has a door line without a flag.");
        const std::string_view id = value.substr(0, comma);
        const std::size_t sideComma = value.find(',', comma + 1);
        const std::string_view flag = value.substr(comma + 1, sideComma == std::string_view::npos ? sideComma : sideComma - comma - 1);
        if (FindDoor(id) == nullptr)
            return refuse("The door text names a door that does not exist.");
        if (id != DoorTable[i].Id)
            return refuse("The door text repeats a door or lists the doors out of order.");
        if (flag != "0" && flag != "1")
            return refuse("The door text has a flag that is not 0 or 1.");
        const bool open = flag == "1";
        std::int8_t side = open ? DoorTable[i].PreferredSwingSide : std::int8_t{0};
        if (legacy)
        {
            if (sideComma != std::string_view::npos)
                return refuse("A legacy door line has an extra field.");
        }
        else
        {
            if (sideComma == std::string_view::npos)
                return refuse("The door text has no swing side.");
            const auto sideText = value.substr(sideComma + 1);
            if (sideText != "-1" && sideText != "0" && sideText != "1")
                return refuse("The door text has a non-canonical swing side.");
            side = sideText == "-1" ? -1 : sideText == "1" ? 1 : 0;
        }
        if (!IsPhysicalPairValid(DoorTable[i], open, side))
            return refuse("The door text has an inconsistent physical pair.");
        state.Doors[i] = DoorEntry{std::string(id), open, side};
    }
    if (!NextLine(text, position, line) || line != DoorsEnd)
        return refuse("The door text is truncated or has no end line.");
    if (position != text.size())
        return refuse("The door text has data after the end line.");
    result.State = std::move(state);
    result.bMigrated = legacy || rosterMigration;
    return result;
}

std::span<const DoorRosterEdition> DoorRosterEditions() noexcept
{
    return RosterEditions;
}
} // namespace PerfectWorld
