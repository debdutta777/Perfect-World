#include "Doors.h"

#include <array>
#include <charconv>
#include <utility>

namespace PerfectWorld
{
namespace
{
constexpr std::string_view DoorsHeader = "pw.doors/1";
constexpr std::string_view DoorsEnd = "end";

constexpr std::string_view OpeningHome = "location.nabapara.opening_home";

/**
 * The opening home (LANE_LIFE_PLAN section C: entry/utility space, living/eating room, sleeping/study room, kitchen,
 * washroom). The front door and the bedroom door are private; the others are ordinary household doors. Each id is
 * named for the room it enters. TODO(owner): the real second and later homes, shops and their hours are not authored yet.
 */
constexpr std::array<DoorDef, 5> DoorTable{{
    {"door.nabapara.home_front", OpeningHome, false, DoorAccess::Private, 0, 0},
    {"door.nabapara.home_living", OpeningHome, true, DoorAccess::Public, 0, 0},
    {"door.nabapara.home_bedroom", OpeningHome, false, DoorAccess::Private, 0, 0},
    {"door.nabapara.home_kitchen", OpeningHome, true, DoorAccess::Public, 0, 0},
    {"door.nabapara.home_washroom", OpeningHome, false, DoorAccess::Public, 0, 0},
}};

constexpr std::array<std::string_view, DoorAccessCount> AccessIds{"public", "private", "locked", "business_hours"};

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
        (door.Access == DoorAccess::Locked && door.bInitiallyOpen))
        return false;
    if (door.Access != DoorAccess::BusinessHours)
        return door.OpenMinute == 0 && door.CloseMinute == 0 && door.OpeningWindows.empty();
    if (door.OpeningWindows.empty())
        return door.OpenMinute >= 0 && door.OpenMinute < door.CloseMinute && door.CloseMinute <= DoorMinutesPerDay;
    if (door.OpenMinute != 0 || door.CloseMinute != 0)
        return false;
    std::int32_t previousClose = 0;
    for (const auto& window : door.OpeningWindows)
    {
        if (window.OpenMinute < previousClose || window.OpenMinute >= window.CloseMinute ||
            window.CloseMinute > DoorMinutesPerDay)
            return false;
        previousClose = window.CloseMinute;
    }
    return true;
}

std::string_view DoorAccessId(DoorAccess access) noexcept
{
    const std::size_t index = static_cast<std::size_t>(access);
    return index < DoorAccessCount ? AccessIds[index] : std::string_view{};
}

std::string_view DoorResultCode(DoorResult result) noexcept
{
    switch (result)
    {
    case DoorResult::Ok: return "ok";
    case DoorResult::UnknownDoor: return "unknown_door";
    case DoorResult::Locked: return "locked";
    case DoorResult::Private: return "private";
    case DoorResult::Closed: return "closed_hours";
    case DoorResult::NoChange: return "no_change";
    case DoorResult::InvalidTime: return "invalid_time";
    case DoorResult::InvalidDefinition: return "invalid_definition";
    case DoorResult::InvalidContext: return "invalid_context";
    case DoorResult::InvalidState: return "invalid_state";
    case DoorResult::NotAvailable: return "not_available";
    case DoorResult::Knocked: return "knocked";
    }
    return {};
}

std::string_view DoorResultReasonKey(DoorResult result) noexcept
{
    switch (result)
    {
    case DoorResult::Ok: return {};
    case DoorResult::UnknownDoor: return "door.reason.unknown";
    case DoorResult::Locked: return "door.reason.locked";
    case DoorResult::Private: return "door.reason.private";
    case DoorResult::Closed: return "door.reason.closed_hours";
    case DoorResult::NoChange: return "door.reason.no_change";
    case DoorResult::InvalidTime: return "door.reason.invalid_time";
    case DoorResult::InvalidDefinition: return "door.reason.invalid_definition";
    case DoorResult::InvalidContext: return "door.reason.invalid_context";
    case DoorResult::InvalidState: return "door.reason.invalid_state";
    case DoorResult::NotAvailable: return "door.reason.not_available";
    case DoorResult::Knocked: return {};
    }
    return {};
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
    for (const auto& window : windows)
    {
        if (minuteOfDay < window.OpenMinute)
            return window.OpenMinute - minuteOfDay;
        if (minuteOfDay < window.CloseMinute)
            return 0;
    }
    return DoorMinutesPerDay - minuteOfDay + windows.front().OpenMinute;
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
    case DoorAccess::Private: return context.IsResident || context.IsInvited ? DoorResult::Ok : DoorResult::Private;
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
    if (permission == DoorResult::Private)
        return {DoorResult::Knocked, DoorEvent::Knock};
    return {permission == DoorResult::Ok ? DoorResult::NotAvailable : permission, DoorEvent::None};
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
        state.Doors.push_back(DoorEntry{std::string(door.Id), door.bInitiallyOpen});
    return state;
}

bool IsDoorStateValid(const DoorState& state) noexcept
{
    if (state.Doors.size() != DoorTable.size())
        return false;
    for (std::size_t i = 0; i < DoorTable.size(); ++i)
    {
        if (state.Doors[i].Id != DoorTable[i].Id)
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
            entry.Open = open;
        return result;
    }
    return DoorResult::UnknownDoor; // authored, but this state does not hold it
}

std::string SerialiseDoorState(const DoorState& state)
{
    if (!IsDoorStateValid(state))
        return {};
    std::string text(DoorsHeader);
    text += "\ncount=" + std::to_string(state.Doors.size());
    for (const DoorEntry& entry : state.Doors)
        text += "\ndoor=" + entry.Id + "," + (entry.Open ? "1" : "0");
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
    if (!NextLine(text, position, line) || line != DoorsHeader)
        return refuse("The door text has no recognised header.");
    std::size_t count = 0;
    if (!NextLine(text, position, line) || !KeyValue(line, "count", value) || !ParseCount(value, count))
        return refuse("The door text has no valid count line.");
    if (count != DoorTable.size())
        return refuse("The door text does not hold exactly the authored doors.");
    DoorState state;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!NextLine(text, position, line) || !KeyValue(line, "door", value))
            return refuse("The door text is truncated or has a bad door line.");
        const std::size_t comma = value.rfind(',');
        if (comma == std::string_view::npos)
            return refuse("The door text has a door line without a flag.");
        const std::string_view id = value.substr(0, comma);
        const std::string_view flag = value.substr(comma + 1);
        if (FindDoor(id) == nullptr)
            return refuse("The door text names a door that does not exist.");
        if (id != DoorTable[i].Id)
            return refuse("The door text repeats a door or lists the doors out of order.");
        if (flag != "0" && flag != "1")
            return refuse("The door text has a flag that is not 0 or 1.");
        state.Doors.push_back(DoorEntry{std::string(id), flag == "1"});
    }
    if (!NextLine(text, position, line) || line != DoorsEnd)
        return refuse("The door text is truncated or has no end line.");
    if (position != text.size())
        return refuse("The door text has data after the end line.");
    result.State = std::move(state);
    return result;
}
} // namespace PerfectWorld
