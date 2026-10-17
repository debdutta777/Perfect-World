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
 * rule); the live part records the physical open/closed flag and swing side per door.
 *
 * Entry rules, independent of leaf position (valid egress and closing are always allowed):
 *   Public         anyone, at any time.
 *   Private        residents or invited visitors; an unauthorized visitor is offered Knock.
 *   Locked         nobody, resident or not (no keys exist in this slice); DoorResult::Locked. A locked door starts closed.
 *   BusinessHours  anyone in the daily half-open window(s); outside those hours a visitor is refused with DoorResult::Closed.
 *                  A resident (staff) may always enter. An invitation does not override business hours.
 * "Resident" is the caller's fact: someone living here, business staff, or on-duty care-home staff.
 *
 * Adjacency of rooms is the interior layout's business: the opening-home ids are named for the room each door enters,
 * not for a fixed floor plan, so the layout may place them as it needs.
 *
 * DoorState keeps one entry for every authored door, in table order. Historical ten-door /2 saves add the stationery
 * default; /1 saves also migrate their sides. Future table additions require an explicit migration.
 *
 * Integer arithmetic only. Nothing here throws.
 */
constexpr std::int32_t DoorMinutesPerDay = 1440;

enum class DoorAccess : std::uint8_t
{
    Public, Private, Locked, BusinessHours
};
constexpr std::size_t DoorAccessCount = 4;

/** Daily intervals are half-open, sorted, disjoint and never cross midnight; adjacent intervals are allowed. */
struct DoorOpeningWindow
{
    std::int32_t OpenMinute = 0;
    std::int32_t CloseMinute = 0;
};

struct DoorDef
{
    std::string_view Id;
    std::string_view LocationId;
    bool bInitiallyOpen = false;
    DoorAccess Access = DoorAccess::Public;
    /** BusinessHours only: minutes since midnight, 0 <= OpenMinute < CloseMinute <= 1440. Zero for the other kinds. */
    std::int32_t OpenMinute = 0;
    std::int32_t CloseMinute = 0;
    /** When nonempty, replaces the legacy single window above (which must then be zero). */
    std::span<const DoorOpeningWindow> OpeningWindows{};
    bool bTwoWay = true;
    /** +1 is inward, -1 outward; also the only accepted open side for a one-way leaf. */
    std::int8_t PreferredSwingSide = 1;
};

enum class DoorResult : std::uint8_t
{
    Ok, UnknownDoor, Locked, Private, Closed, NoChange, InvalidTime,
    InvalidDefinition, InvalidContext, InvalidState, NotAvailable, Knocked
};

enum class DoorDirection : std::uint8_t { Entry = 0, Exit = 1 };
struct DoorUseContext
{
    DoorDirection Direction = DoorDirection::Entry;
    bool IsResident = false;
    /** Caller-owned permission for this particular door; Knock never changes it. */
    bool IsInvited = false;
};

enum class DoorAction : std::uint8_t { None = 0, Open = 1, Close = 2, Knock = 3 };
struct DoorOffer
{
    DoorAction Action = DoorAction::None;
    DoorResult Refusal = DoorResult::Ok;
    /** Minutes from now to the next opening; -1 unless refused for closed hours. */
    std::int32_t MinutesUntilOpening = -1;
    friend bool operator==(const DoorOffer&, const DoorOffer&) = default;
};

enum class DoorEvent : std::uint8_t { None = 0, Knock = 1 };
struct DoorKnockResult
{
    DoorResult Result = DoorResult::NotAvailable;
    DoorEvent Event = DoorEvent::None;
    friend bool operator==(const DoorKnockResult&, const DoorKnockResult&) = default;
};

struct DoorEntry
{
    std::string Id;
    bool Open = false;
    std::int8_t SwingSide = 0;
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
    bool bMigrated = false;
    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/** The authored table, in a fixed order. */
[[nodiscard]] PERFECTWORLDCORE_API std::span<const DoorDef> AuthoredDoors() noexcept;
[[nodiscard]] PERFECTWORLDCORE_API const DoorDef* FindDoor(std::string_view id) noexcept;
/** True when the table is well formed (unique "door." ids, locked doors closed, sane business hours). Tests call it. */
[[nodiscard]] PERFECTWORLDCORE_API bool ValidateDoorTable() noexcept;
[[nodiscard]] PERFECTWORLDCORE_API bool IsDoorDefinitionValid(const DoorDef& door) noexcept;

[[nodiscard]] PERFECTWORLDCORE_API std::string_view DoorAccessId(DoorAccess access) noexcept;
/** Stable result identifiers, including "knocked" for a delivered transient Knock event. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view DoorResultCode(DoorResult result) noexcept;
/** Player-facing text key: "door.reason.locked" and so on; empty for Ok/Knocked. Wording belongs to the presenter. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view DoorResultReasonKey(DoorResult result) noexcept;

/** Minutes since midnight for a game instant (0..1439). */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t DoorMinuteOfDay(GameInstant instant) noexcept;
/** The minute of day a BusinessHours door opens, for "Closed until ..."; -1 for every other kind. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t DoorOpensAtMinute(const DoorDef& door) noexcept;
/** 0 during an opening window; otherwise distance to the next daily opening, or -1 for invalid/non-business doors. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t DoorMinutesUntilOpening(const DoorDef& door, std::int32_t minuteOfDay) noexcept;

/** Permission only, independent of leaf position. Exit is allowed even after closing or through a locked door. */
[[nodiscard]] PERFECTWORLDCORE_API DoorResult EvaluateDoorPassage(const DoorDef& door, std::int32_t minuteOfDay,
                                                                const DoorUseContext& context) noexcept;
/** Unauthorized private entry offers Knock even with an open leaf; an open leaf never grants permission. */
[[nodiscard]] PERFECTWORLDCORE_API DoorOffer EvaluateDoorOffer(const DoorDef& door, bool currentlyOpen,
                                                              std::int32_t minuteOfDay, const DoorUseContext& context) noexcept;
/** Explicit transient event. No state or invitation is stored or granted. */
[[nodiscard]] PERFECTWORLDCORE_API DoorKnockResult EvaluateDoorKnock(const DoorDef& door, std::int32_t minuteOfDay,
                                                                   const DoorUseContext& context) noexcept;

/**
 * The rule, pure: may `door`, currently open or not, be put in the state `open` at `minuteOfDay`?
 * Invalid inputs are refused first. Opening requires passage permission even for an already-open leaf;
 * closing requires no entry permission. The bool overload is a legacy Entry context with no invitation.
 */
[[nodiscard]] PERFECTWORLDCORE_API DoorResult EvaluateDoorOpen(const DoorDef& door, bool currentlyOpen, bool open,
                                                                std::int32_t minuteOfDay, bool bIsResident) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API DoorResult EvaluateDoorOpen(const DoorDef& door, bool currentlyOpen, bool open,
                                                              std::int32_t minuteOfDay, const DoorUseContext& context) noexcept;

[[nodiscard]] PERFECTWORLDCORE_API DoorState MakeInitialDoorState();
/** True when the state holds exactly the authored doors, in table order, with consistent physical pairs. */
[[nodiscard]] PERFECTWORLDCORE_API bool IsDoorStateValid(const DoorState& state) noexcept;
/** The door's current flag; empty for an id the state does not hold. */
[[nodiscard]] PERFECTWORLDCORE_API std::optional<bool> QueryDoorOpen(const DoorState& state, std::string_view doorId) noexcept;

/**
 * Opens or closes a door through access rules, using the preferred side when opening and zero when closing.
 * The state changes only when the result is Ok; every refusal and NoChange leave it untouched.
 * `minuteOfDay` is 0..1439 (see DoorMinuteOfDay); anything else is InvalidTime.
 */
[[nodiscard]] PERFECTWORLDCORE_API DoorResult TrySetDoorOpen(DoorState& state, std::string_view doorId, bool open,
                                                              std::int32_t minuteOfDay, bool bIsResident);
[[nodiscard]] PERFECTWORLDCORE_API DoorResult TrySetDoorOpen(DoorState& state, std::string_view doorId, bool open,
                                                            std::int32_t minuteOfDay, const DoorUseContext& context);

/** Physical fact only, including a partly stopped open leaf. Invalid pairs return InvalidState without mutation.
 * No access rules apply. Identical facts return NoChange; changed facts return Ok.
 */
[[nodiscard]] PERFECTWORLDCORE_API DoorResult ApplyDoorSettlement(DoorState& state, std::string_view doorId,
                                                                 bool bIsOpenNow, std::int8_t swingSide);

/** Strict text form ("pw.doors/2"): door=id,open,side. An invalid state serialises to the empty string. */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseDoorState(const DoorState& state);
/** Also reads the historical ten-door /2 set, and /1 (five home doors, ten doors or today's full table).
 * Adds authored defaults and, for /1, preferred open sides. Successful migrations set bMigrated.
 * Refuses malformed, inconsistent, out-of-order or trailing data.
 */
[[nodiscard]] PERFECTWORLDCORE_API DoorStateParse DeserialiseDoorState(std::string_view text);
} // namespace PerfectWorld
