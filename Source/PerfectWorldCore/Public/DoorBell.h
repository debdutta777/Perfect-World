#pragma once

#include "Doors.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace PerfectWorld
{
/**
 * The calling bell (order F5). Owner feedback after the packaged build: the player must not walk into other people's homes;
 * a visitor first rings the bell. This header adds NO rule of its own. Every answer below is the core's own Knock
 * (Doors.h EvaluateDoorKnock) and every threshold decision is its own EvaluateDoorPassage; the functions only name which doors
 * have a bell, wrap the knock as a "ring", and give the engine two further questions with portable answers (is a visitor
 * allowed over the threshold right now, should the household close an open leaf behind a visitor who left).
 *
 * Nothing here is saved. An answered ring is the caller's HasKnocked fact for the current visit only (Doors.h), exactly like
 * a knock; the door roster and the pw.doors/2 format are untouched. Header only: no new core translation unit.
 *
 * A bell hangs beside every private DWELLING ENTRANCE: the home front doors and the flat doors, i.e. the Private doors a
 * stranger meets from outside the household. The Private doors between rooms of one household (the bedroom, the dormitory) have no
 * bell; DoorPrivateRoomIds names them, and the tests refuse a new Private door that is in neither list, so a future door
 * forces a decision.
 */
inline constexpr std::array<std::string_view, 8> DoorBellEntranceIds{{
    "door.nabapara.home_front", "door.nabapara.care_front", "door.nabapara.caretaker_flat_front",
    "door.nabapara.tall_flats_unit_a", "door.nabapara.tall_flats_unit_b", "door.nabapara.tea_row_house_front",
    "door.nabapara.old_house_front", "door.nabapara.tall_flats_upper_a"}};

inline constexpr std::array<std::string_view, 2> DoorPrivateRoomIds{{"door.nabapara.home_bedroom", "door.nabapara.care_dormitory"}};

/** The one built home shell every family origin starts in (BlockTestGameMode PlacePlayerAtOpeningHome). */
inline constexpr std::string_view DoorBellOpeningHomeLocationId = "location.nabapara.opening_home";

[[nodiscard]] constexpr bool IsDoorBellEntranceId(std::string_view doorId) noexcept
{
    for (const std::string_view id : DoorBellEntranceIds)
    {
        if (id == doorId)
            return true;
    }
    return false;
}

[[nodiscard]] constexpr bool IsDoorPrivateRoomId(std::string_view doorId) noexcept
{
    for (const std::string_view id : DoorPrivateRoomIds)
    {
        if (id == doorId)
            return true;
    }
    return false;
}

/** True when this door has a bell beside it: a Private dwelling entrance. */
[[nodiscard]] inline bool DoorHasBell(const DoorDef& door) noexcept
{
    return door.Access == DoorAccess::Private && IsDoorBellEntranceId(door.Id);
}

enum class DoorRingOutcome : std::uint8_t
{
    /** The door has no bell (a public door, a business door, a room door). Nothing was rung. */
    NoBell = 0,
    /** The visitor needs no answer: a resident or invited person, or one already admitted on this approach. */
    NoBellNeeded = 1,
    /** A resident is home in visiting hours: the visitor may come in on this approach (the caller sets HasKnocked). */
    Answered = 2,
    /** Nobody is home to answer. */
    NoAnswer = 3,
    /** A resident is home but not receiving visitors now (outside visiting hours, or the home has none). */
    NotReceiving = 4,
    /** The core would not deliver a knock for another reason (Locked, invalid time or definition). */
    Refused = 5
};

struct DoorRingResult
{
    DoorRingOutcome Outcome = DoorRingOutcome::NoBell;
    /** The core's own knock answer this ring was built from; default when no knock was delivered. */
    DoorKnockResult Knock;
    /** NotReceiving at a door with visiting hours: minutes until the next window; otherwise -1. */
    std::int32_t MinutesUntilVisiting = -1;
    friend bool operator==(const DoorRingResult&, const DoorRingResult&) = default;
};

/**
 * Ringing the bell is the core's Knock on a door that has one: no second rule. The direction is forced to Entry (a bell is only
 * pressed from outside). Answered exactly when EvaluateDoorKnock gives DoorKnockAnswer::Admitted.
 */
[[nodiscard]] inline DoorRingResult EvaluateDoorRing(const DoorDef& door, std::int32_t minuteOfDay, DoorUseContext context) noexcept
{
    DoorRingResult result;
    if (!DoorHasBell(door))
        return result;
    context.Direction = DoorDirection::Entry;
    result.Knock = EvaluateDoorKnock(door, minuteOfDay, context);
    result.MinutesUntilVisiting = result.Knock.MinutesUntilVisiting;
    if (result.Knock.Event != DoorEvent::Knock)
    {
        // The core did not knock: either passage is already permitted (resident, invited, admitted), or it is refused outright.
        result.Outcome = result.Knock.Result == DoorResult::NotAvailable ? DoorRingOutcome::NoBellNeeded : DoorRingOutcome::Refused;
        return result;
    }
    switch (result.Knock.Answer)
    {
    case DoorKnockAnswer::Admitted: result.Outcome = DoorRingOutcome::Answered; break;
    case DoorKnockAnswer::NoAnswer: result.Outcome = DoorRingOutcome::NoAnswer; break;
    case DoorKnockAnswer::NotReceiving: result.Outcome = DoorRingOutcome::NotReceiving; break;
    case DoorKnockAnswer::None: result.Outcome = DoorRingOutcome::Refused; break;
    }
    return result;
}

/** Stable identifiers, for logs and tests; the words belong to the presenter. */
[[nodiscard]] constexpr std::string_view DoorRingKey(DoorRingOutcome outcome) noexcept
{
    switch (outcome)
    {
    case DoorRingOutcome::NoBell: return "door.bell.no_bell";
    case DoorRingOutcome::NoBellNeeded: return "door.bell.not_needed";
    case DoorRingOutcome::Answered: return "door.bell.answered";
    case DoorRingOutcome::NoAnswer: return "door.bell.no_answer";
    case DoorRingOutcome::NotReceiving: return "door.bell.not_receiving";
    case DoorRingOutcome::Refused: return "door.bell.refused";
    }
    return "door.bell.refused";
}

struct DoorThresholdVerdict
{
    bool bMayCross = false;
    /** Ok when allowed; otherwise the core's refusal (Private, Locked, Closed, InvalidTime ...). */
    DoorResult Why = DoorResult::InvalidContext;
    friend bool operator==(const DoorThresholdVerdict&, const DoorThresholdVerdict&) = default;
};

/**
 * May a person cross the threshold of this door right now, whatever the leaf is doing? Exit is always allowed (the core's Exit
 * is always Ok); Entry is the core's EvaluateDoorPassage. An OPEN leaf grants nothing: the leaf's position is not an input.
 * An invalid minute or definition is refused (fail closed).
 */
[[nodiscard]] inline DoorThresholdVerdict EvaluateDoorThreshold(const DoorDef& door, std::int32_t minuteOfDay,
                                                                const DoorUseContext& context) noexcept
{
    const DoorResult result = EvaluateDoorPassage(door, minuteOfDay, context);
    return {result == DoorResult::Ok, result};
}

/**
 * The household closes an open dwelling leaf behind a visitor who has gone: a bell door that stands open, whose household is not
 * the one near it, and nobody it was opened for is still close by. A resident (or invited person) keeps their own door as they
 * left it. Pure; the caller supplies the facts.
 */
[[nodiscard]] inline bool ShouldHostCloseDoor(const DoorDef& door, bool bLeafOpen, bool bIsResident, bool bIsInvited,
                                              bool bVisitorNearby) noexcept
{
    return DoorHasBell(door) && bLeafOpen && !bIsResident && !bIsInvited && !bVisitorNearby;
}

/**
 * Every family origin is placed in the ONE built opening-home shell (BlockTestGameMode PlacePlayerAtOpeningHome: "All family
 * origins use the opening shell"), but LaneOrigin.cpp lists residency only for each origin's own, unbuilt, home location. Without
 * this, an extended-family, grandparent, sibling or guardian player would start inside a home that treats them as a stranger
 * (locked out on the way back in). A care-origin player is not a resident of the opening home.
 */
[[nodiscard]] constexpr bool IsFamilyStartShell(bool bGrewUpInCare, std::string_view locationId) noexcept
{
    return !bGrewUpInCare && locationId == DoorBellOpeningHomeLocationId;
}
} // namespace PerfectWorld
