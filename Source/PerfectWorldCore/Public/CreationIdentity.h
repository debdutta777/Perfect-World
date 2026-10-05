#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Unreal supplies this export macro; standalone CMake builds do not.
#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * Who the character is on paper: date of birth, pronouns, languages, and the
 * school stream with the player's reason for it (ROADMAP M0-18; see
 * Docs/CHARACTER_CREATION.md "Creation flow" steps 1 and 4).
 *
 * All of it is free. None of it costs creation points, and none of it may carry
 * a stat or price effect: religion, language, family background and stream
 * never carry innate stat multipliers (SCHOOL_AND_PROGRESSION.md). The stream
 * sets a timetable, not a destiny.
 */

/**
 * A calendar date of birth. The campaign opens on 2 June 2042 and the player is
 * exactly 16 on that day, so only dates from 3 June 2025 to 2 June 2026
 * inclusive are accepted (see AgeOnCampaignStart).
 */
struct DateOfBirth
{
    std::int32_t Year = 2026;
    std::int32_t Month = 1;
    std::int32_t Day = 1;

    friend bool operator==(const DateOfBirth&, const DateOfBirth&) = default;
};

/** Earliest and latest dates of birth that make the player 16 on 2 June 2042. */
constexpr DateOfBirth EarliestDateOfBirth{2025, 6, 3};
constexpr DateOfBirth LatestDateOfBirth{2026, 6, 2};
/** Decision R2: what a record written before schema v5 migrates to. A fresh draft starts here too. */
constexpr DateOfBirth DefaultDateOfBirth{2026, 1, 1};

/** Three authored school streams. The ID is the contract; the names are display text. */
constexpr std::size_t SchoolStreamCount = 3;
/** Decision R2: the stream a record written before schema v5 migrates to. A fresh draft starts here too. */
constexpr std::string_view DefaultSchoolStreamId = "science";

/** Byte limits, not character counts: free text is UTF-8 and a limit in bytes is the same on every platform. */
constexpr std::size_t MaxPronounsBytes = 64;
constexpr std::size_t MaxLanguageBytes = 64;
constexpr std::size_t MaxAdditionalLanguages = 6;
constexpr std::size_t MaxSchoolReasonBytes = 240;

struct SchoolStreamDefinition
{
    std::string_view Id;
    std::string_view DisplayName;
    std::string_view TimetableSummary;
};

/** Everything the identity screen edits. Names are free text; pronouns and languages are bounded free text. */
struct IdentityProfile
{
    std::string GivenName;
    std::string FamilyName;
    std::string Pronouns;
    std::string HomeCity;
    std::string PrimaryLanguage;
    /** Other languages the character speaks, in the order the player listed them. */
    std::vector<std::string> AdditionalLanguages;
    DateOfBirth Birth;
};

/** The school stream and the player's own words about why. The reason may be empty. */
struct SchoolChoice
{
    std::string StreamId{DefaultSchoolStreamId};
    std::string Reason;
};

[[nodiscard]] PERFECTWORLDCORE_API const std::array<SchoolStreamDefinition, SchoolStreamCount>& SchoolStreams() noexcept;
[[nodiscard]] PERFECTWORLDCORE_API const SchoolStreamDefinition* FindSchoolStream(std::string_view streamId) noexcept;

/**
 * Whole years the person has lived on 2 June 2042, counted by birthdays. A
 * date that does not exist on the calendar has no age; callers check it with
 * DescribeDateOfBirthProblem first. Negative for a birth after the campaign opens.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::int64_t AgeOnCampaignStart(const DateOfBirth& birth) noexcept;

/**
 * Each validator returns why the value is unusable, or an empty string when it is
 * valid, in text suitable for showing the player. Empty free text is valid:
 * a draft that has not reached the screen yet is not an error.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string DescribeDateOfBirthProblem(const DateOfBirth& birth);
[[nodiscard]] PERFECTWORLDCORE_API std::string DescribePronounsProblem(std::string_view pronouns);
[[nodiscard]] PERFECTWORLDCORE_API std::string DescribeLanguagesProblem(std::string_view primary,
                                                                        const std::vector<std::string>& additional);
[[nodiscard]] PERFECTWORLDCORE_API std::string DescribeSchoolReasonProblem(std::string_view reason);
/** The date, pronouns and languages of a profile together. Names are not bounded here. */
[[nodiscard]] PERFECTWORLDCORE_API std::string DescribeIdentityProblem(const IdentityProfile& identity);
} // namespace PerfectWorld
