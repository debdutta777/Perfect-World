#pragma once

#include "WeekCompression.h"

#include <cstdint>
#include <vector>

// Shared by WeekCompressionPlan.cpp (validation and the time-contract view), WeekCompressionRun.cpp
// (the two resolvers) and WeekCompressionCompare.cpp (equivalence and the receipt).
namespace PerfectWorld
{
/** The pieces a week is resolved in. Every piece is one declared interaction of the time contract. */
enum class WeekUnitKind : std::uint8_t
{
    /** Time awake between commitments (at most a day each). */
    Gap = 0,
    Spend,
    Activity,
    School,
    Sleep
};

struct WeekUnit
{
    WeekUnitKind Kind = WeekUnitKind::Gap;
    std::int32_t Day = 0;
    /** The entry's index within its day, and across the week (days in order). */
    std::int32_t Entry = 0;
    std::int32_t Flat = 0;
    /**
     * The first piece of an entry at which the morning plan must be made: the first entry of the
     * week, of a calendar day, or after waking. The plan lists the day's activities from this entry on.
     */
    bool OpensDay = false;
    /** The instant the piece begins, and the minutes it takes (0 for arriving at school). */
    GameInstant Start = 0;
    std::int64_t Minutes = 0;
    /** The entry it belongs to (a gap belongs to the entry it leads to). */
    const WeekEntry* Source = nullptr;
    const ActivityDef* Def = nullptr;
};

struct WeekUnits
{
    WeekPlanIssue Issue;
    std::vector<WeekUnit> Units;
    /** Entries in the whole plan. */
    std::int32_t EntryCount = 0;
    GameInstant StartInstant = 0;
    GameInstant EndInstant = 0;
};

/** Validates the whole plan against the world and expands it into units. The time contract is checked by the caller. */
[[nodiscard]] WeekUnits WeekBuildUnits(const WorldState& world, const WeekPlan& plan, const DayLoopCatalogues& catalogues);

/** The declaration a unit makes to the time contract. */
[[nodiscard]] InteractionDeclaration WeekUnitDeclaration(const WeekUnit& unit);
/** The clock mode a unit is charged under. */
[[nodiscard]] ClockMode WeekUnitMode(const WeekUnit& unit);
/** What the entry is called in a receipt: the activity id, or "school", "sleep", "spend". */
[[nodiscard]] std::string WeekEntryWhat(const WeekEntry& entry);

[[nodiscard]] WeekPlanIssue WeekMakeIssue(WeekError error, std::int32_t day, std::int32_t entry, std::string detail = {});
} // namespace PerfectWorld
