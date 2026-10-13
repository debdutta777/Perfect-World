#pragma once

#include "GameClock.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * Doors (LANE_LIFE_IMPLEMENTATION rows 3.1 and 3.3, minimal slice; LANE_LIFE_PLAN section C "Construction and door feel").
 *
 * The portable core decides whether a door may open and records open or closed; Unreal only presents the leaf, the
 * sweep and the prompt. A door is authored once (id, the location it belongs to, whether it starts open, an access
 * rule); the live part is one open/closed flag per door in a DoorState.
 *
 * Access rules, as TrySetDoorOpen applies them when a door is asked to OPEN (closing is always allowed):
 *   Public         anyone, at any time.
 *   Private        residents only; a visitor is refused with DoorResult::Private ("Knock").
 *   Locked         nobody, resident or not (no keys exist in this slice); DoorResult::Locked. A locked door starts closed.
 *   BusinessHours  anyone between OpenMinute (inclusive) and CloseMinute (exclusive) of the day; outside those hours a
 *                  visitor is refused with DoorResult::Closed ("Closed until ..."). A resident (staff) may always open.
 * "Resident" is the caller's fact: the player living in, or working at, the place the door belongs to.
 *
 * Adjacency of rooms is the interior layout's business: the opening-home ids are named for the room each door enters,
 * not for a fixed floor plan, so the layout may place them as it needs.
 *
 * DoorState keeps one entry for every authored door, in table order, and its text form requires exactly that set. Adding a
 * door to the table is therefore a save-format change for whoever embeds the state in a world save (a migration or a new
 * version, the integrator's decision). This module is standalone: it is not in WorldState, the reducer or the hash yet.
 *
 * Integer arithmetic only. Nothing here throws.
 */
constexpr std::int32_t DoorMinutesPerDay = 1440;

enum class DoorAccess : std::uint8_t
{
    Public, Private, Locked, BusinessHours
};
constexpr std::size_t DoorAccessCount = 4;

struct DoorDef
{
    std::string_view Id;
    std::string_view LocationId;
    bool bInitiallyOpen = false;
    DoorAccess Access = DoorAccess::Public;
    /** BusinessHours only: minutes since midnight, 0 <= OpenMinute < CloseMinute <= 1440. Zero for the other kinds. */
    std::int32_t OpenMinute = 0;
    std::int32_t CloseMinute = 0;
};

enum class DoorResult : std::uint8_t
{
    Ok, UnknownDoor, Locked, Private, Closed, NoChange, InvalidTime
};

struct DoorEntry
{
    std::string Id;
    bool Open = false;
    friend bool operator==(const DoorEntry&, const DoorEntry&) = default;
};

/** One entry per authored door, in table order. Build with MakeInitialDoorState. */
struct DoorState
{
    std::vector<DoorEntry> Doors;
    friend bool operator==(const DoorState&, const DoorState&) = default;
};

struct DoorStateParse
{
    DoorState State;
    /** Empty on success; otherwise a short sentence and State is left default. */
    std::string Error;
    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/** The authored table, in a fixed order. */
[[nodiscard]] PERFECTWORLDCORE_API std::span<const DoorDef> AuthoredDoors() noexcept;
[[nodiscard]] PERFECTWORLDCORE_API const DoorDef* FindDoor(std::string_view id) noexcept;
/** True when the table is well formed (unique "door." ids, locked doors closed, sane business hours). Tests call it. */
[[nodiscard]] PERFECTWORLDCORE_API bool ValidateDoorTable() noexcept;

[[nodiscard]] PERFECTWORLDCORE_API std::string_view DoorAccessId(DoorAccess access) noexcept;
/** "ok", "unknown_door", "locked", "private", "closed_hours", "no_change", "invalid_time". Stable; in logs and tests. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view DoorResultCode(DoorResult result) noexcept;
/** Player-facing text key: "door.reason.locked" and so on; empty for Ok. The wording lives in the presenter's text table. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view DoorResultReasonKey(DoorResult result) noexcept;

/** Minutes since midnight for a game instant (0..1439). */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t DoorMinuteOfDay(GameInstant instant) noexcept;
/** The minute of day a BusinessHours door opens, for "Closed until ..."; -1 for every other kind. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t DoorOpensAtMinute(const DoorDef& door) noexcept;

/**
 * The rule, pure: may `door`, currently open or not, be put in the state `open` at `minuteOfDay`?
 * Order: invalid time, same state (NoChange), closing (Ok), then the access rule for opening.
 */
[[nodiscard]] PERFECTWORLDCORE_API DoorResult EvaluateDoorOpen(const DoorDef& door, bool currentlyOpen, bool open,
                                                                std::int32_t minuteOfDay, bool bIsResident) noexcept;

[[nodiscard]] PERFECTWORLDCORE_API DoorState MakeInitialDoorState();
/** True when the state holds exactly the authored doors, in table order. */
[[nodiscard]] PERFECTWORLDCORE_API bool IsDoorStateValid(const DoorState& state) noexcept;
/** The door's current flag; empty for an id the state does not hold. */
[[nodiscard]] PERFECTWORLDCORE_API std::optional<bool> QueryDoorOpen(const DoorState& state, std::string_view doorId) noexcept;

/**
 * Opens or closes a door. The state changes only when the result is Ok; every refusal and NoChange leave it untouched.
 * `minuteOfDay` is 0..1439 (see DoorMinuteOfDay); anything else is InvalidTime.
 */
[[nodiscard]] PERFECTWORLDCORE_API DoorResult TrySetDoorOpen(DoorState& state, std::string_view doorId, bool open,
                                                              std::int32_t minuteOfDay, bool bIsResident);

/** Strict text form ("pw.doors/1"). An invalid state serialises to the empty string. */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseDoorState(const DoorState& state);
/** Refuses a missing or wrong header, a truncated text, an unknown, repeated or out-of-order door, a bad flag, and trailing data. */
[[nodiscard]] PERFECTWORLDCORE_API DoorStateParse DeserialiseDoorState(std::string_view text);
} // namespace PerfectWorld
