#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * Awakened grades and levelling (Docs/AWAKENED_GRADES.md).
 *
 * Awakening grants a profile, never a class: a Tattva affinity, six power
 * dimensions trained separately, and a grade from F to S. The grade is a
 * certification. Practice raises the dimensions, practice has daily and
 * weekly ceilings so it cannot be ground, and only a passed assessment
 * promotes. A grade never multiplies any ordinary skill.
 *
 * Ratings are integers in hundredths of a point (a "centipoint"), so every
 * gain and decay is exact and identical on every machine. Days are whole
 * game days since the campaign epoch (GameClock).
 */
enum class AwakenedGrade : std::uint8_t
{
    F = 0,
    E,
    D,
    C,
    B,
    A,
    S
};
constexpr std::int32_t AwakenedGradeCount = 7;

enum class PowerDimension : std::uint8_t
{
    Output = 0,
    Precision,
    Range,
    Duration,
    Perception,
    Coordination
};
constexpr std::int32_t PowerDimensionCount = 6;

enum class Tattva : std::uint8_t
{
    Prithvi = 0,
    Jala,
    Agni,
    Vayu,
    Akasha
};
constexpr std::int32_t TattvaCount = 5;

/** Ratings run 0..1,000 points; stored in centipoints. */
constexpr std::int32_t CentipointsPerPoint = 100;
constexpr std::int32_t MaxRatingCentipoints = 1'000 * CentipointsPerPoint;

/** What promotion to a grade requires (the table in Docs/AWAKENED_GRADES.md §4). */
struct GradeRequirement
{
    AwakenedGrade Grade = AwakenedGrade::F;
    std::int32_t RatingPoints = 0;
    std::int32_t DimensionFloorPoints = 0;
    std::int32_t SupervisedPracticeMinutes = 0;
    /** Weekly gain ceiling per dimension while holding this grade; it shrinks as grades rise. */
    std::int32_t WeeklyCeilingPoints = 0;
};

PERFECTWORLDCORE_API std::string_view AwakenedGradeName(AwakenedGrade grade);
PERFECTWORLDCORE_API std::optional<AwakenedGrade> NextGrade(AwakenedGrade grade);
/** The requirement to be promoted *into* this grade. F has no requirement (everyone awakens at F). */
PERFECTWORLDCORE_API const GradeRequirement& RequirementFor(AwakenedGrade grade);

constexpr std::int32_t PracticeDaysInWindow = 7;
constexpr std::int32_t RetakeWaitDays = 30;
constexpr std::int32_t MaxSessionQualityPercent = 150;

struct PracticeDay
{
    std::int64_t Day = -1;
    std::int32_t Minutes = 0;
    std::int32_t GainCentipoints = 0;
};

struct AwakenedProfile
{
    std::array<std::int32_t, TattvaCount> AffinityPercent{};
    std::array<std::int32_t, PowerDimensionCount> Rating{};
    std::array<std::int32_t, PowerDimensionCount> Peak{};
    std::array<std::int64_t, PowerDimensionCount> LastPracticeDay{};
    std::array<std::int64_t, PowerDimensionCount> DecaySettledDay{};
    std::array<std::array<PracticeDay, PracticeDaysInWindow>, PowerDimensionCount> Window{};
    AwakenedGrade Grade = AwakenedGrade::F;
    std::int64_t SupervisedPracticeMinutes = 0;
    std::int64_t EarliestRetakeDay = 0;
};

/**
 * Creates the profile at awakening. The affinity must be five values from 0
 * to 100 summing to exactly 100. Starting ratings (points) are clamped to the
 * F band's ceiling, so the awakening never promotes beyond F.
 */
PERFECTWORLDCORE_API std::optional<AwakenedProfile> Awaken(
    const std::array<std::int32_t, TattvaCount>& affinityPercent,
    const std::array<std::int32_t, PowerDimensionCount>& startingRatingPoints, std::int64_t day);

/** The four strongest dimensions averaged, in centipoints. */
PERFECTWORLDCORE_API std::int32_t ProfileRatingCentipoints(const AwakenedProfile& profile);
/** Stage 1..10 inside the current grade: progress from its entry rating toward the next grade's. */
PERFECTWORLDCORE_API std::int32_t StageWithinGrade(const AwakenedProfile& profile);
/** The highest a dimension can reach before the next promotion (next grade's rating + 100 points). */
PERFECTWORLDCORE_API std::int32_t DimensionCeilingCentipoints(AwakenedGrade grade);

struct PracticeResult
{
    std::int32_t GainCentipoints = 0;
    bool HitDailyLimit = false;
    bool HitWeeklyCeiling = false;
    bool HitGradeCeiling = false;
};

/**
 * One practice session on one dimension. Refuses nothing: an unproductive
 * session simply gains nothing and says why. `qualityPercent` (facilities,
 * coaching) is capped at MaxSessionQualityPercent and never lifts a ceiling.
 */
PERFECTWORLDCORE_API PracticeResult Practise(AwakenedProfile& profile, PowerDimension dimension,
                                             std::int32_t minutes, std::int64_t day,
                                             std::int32_t qualityPercent = 100, bool supervised = false);

/** Applies rest decay up to `day`: after 90 untrained days, 1% per 30 days, floored at 80% of peak. */
PERFECTWORLDCORE_API void SettleDecay(AwakenedProfile& profile, std::int64_t day);

enum class AssessmentEligibility
{
    Eligible,
    AlreadyHighestGrade,
    RatingTooLow,
    DimensionBelowFloor,
    NeedsSupervisedPractice,
    FieldRequirementsNotMet,
    RetakeTooSoon
};

PERFECTWORLDCORE_API AssessmentEligibility CheckAssessmentEligibility(
    const AwakenedProfile& profile, std::int64_t day, bool fieldRequirementsMet);

/**
 * Sits the assessment for the next grade. `passed` is the outcome the
 * assessment scene produced. Promotes only when eligible and passed; a
 * failure sets the earliest retake and never lowers the grade.
 */
PERFECTWORLDCORE_API AssessmentEligibility SitAssessment(
    AwakenedProfile& profile, std::int64_t day, bool fieldRequirementsMet, bool passed);
} // namespace PerfectWorld
