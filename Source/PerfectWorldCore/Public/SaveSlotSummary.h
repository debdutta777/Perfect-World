#pragma once

#include "CreationIdentity.h"
#include "GameClock.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace PerfectWorld
{
struct WorldSaveData;

/**
 * Whole years at the instant's civil date; birthdays begin at midnight.
 * A 29 February birthday falls on 1 March in a non-leap year. Negative before
 * birth, as with AgeOnCampaignStart. Requires a valid civil birth date and an
 * instant in GameClock's supported calendar range; does not impose creation's age band.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::int64_t AgeOnInstant(const DateOfBirth& birth, GameInstant instant) noexcept;

/** Authored name for the lane or an opening-home kit. Empty for empty/unknown IDs; never derives a name from an ID. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view ZoneDisplayName(std::string_view zoneId) noexcept;

/** Read-only slot facts, independent of UI wording, file timestamps and disk recovery status. */
struct WorldSlotSummary
{
    /** Given and family names separated by one space when both exist; empty when neither exists. */
    std::string Name;
    std::int64_t CurrentAge = 0;
    /** Includes the weekday, from GameClock. A draft has no playable time or location; consumers must honour Draft. */
    CalendarTime Calendar;
    /** Empty when no authored zone name exists. HomeCity remains the saved home city, never an inferred current city. */
    std::string ZoneDisplayName;
    std::string HomeCity;
    bool Draft = false;
};

/** Projects a decoded slot without changing its creation, clock, placement or world state. */
[[nodiscard]] PERFECTWORLDCORE_API WorldSlotSummary MakeWorldSlotSummary(const WorldSaveData& data);
} // namespace PerfectWorld
