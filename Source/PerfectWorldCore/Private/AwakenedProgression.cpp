#include "AwakenedProgression.h"

#include <algorithm>

namespace PerfectWorld
{
namespace
{
// Docs/AWAKENED_GRADES.md §4. WeeklyCeilingPoints applies while *holding*
// the grade; it shrinks as grades rise so that even maximum-effort practice
// takes years, not weeks, to reach S.
constexpr std::array<GradeRequirement, AwakenedGradeCount> Requirements = {{
    {AwakenedGrade::F, 0, 0, 0, 12},
    {AwakenedGrade::E, 150, 60, 20 * 60, 10},
    {AwakenedGrade::D, 300, 120, 0, 8},
    {AwakenedGrade::C, 450, 180, 0, 6},
    {AwakenedGrade::B, 600, 250, 0, 4},
    {AwakenedGrade::A, 750, 330, 0, 3},
    {AwakenedGrade::S, 900, 420, 0, 2},
}};

// Daily diminishing returns: percentage value of each minute by how long the
// dimension has already been practised that day.
struct Band
{
    std::int32_t FromMinute;
    std::int32_t ToMinute;
    std::int32_t Percent;
};
constexpr std::array<Band, 3> DailyBands = {{{0, 60, 100}, {60, 120, 60}, {120, 240, 30}}};
constexpr std::int32_t DailyLimitMinutes = 240;
/** 0.2 points per full-value minute. */
constexpr std::int64_t CentipointsPerFullMinute = 20;

constexpr std::int32_t IdleDaysBeforeDecay = 90;
constexpr std::int32_t DecayPeriodDays = 30;
constexpr std::int32_t DecayPercentPerPeriod = 1;
constexpr std::int32_t DecayFloorPercentOfPeak = 80;
/** Fixed awakening ceiling: just below E's rating, so awakening never reaches E. */
constexpr std::int32_t AwakeningCeilingCentipoints = 150 * CentipointsPerPoint - 1;

std::int32_t Index(AwakenedGrade grade)
{
    return static_cast<std::int32_t>(grade);
}

std::int32_t WindowGain(const AwakenedProfile& profile, std::int32_t dimension, std::int64_t day)
{
    std::int32_t sum = 0;
    for (const PracticeDay& entry : profile.Window[dimension])
    {
        if (entry.Day > day - PracticeDaysInWindow && entry.Day <= day)
        {
            sum += entry.GainCentipoints;
        }
    }
    return sum;
}
} // namespace

std::string_view AwakenedGradeName(AwakenedGrade grade)
{
    constexpr std::array<std::string_view, AwakenedGradeCount> Names = {"F", "E", "D", "C", "B", "A", "S"};
    return Names[Index(grade)];
}

std::optional<AwakenedGrade> NextGrade(AwakenedGrade grade)
{
    if (grade == AwakenedGrade::S)
    {
        return std::nullopt;
    }
    return static_cast<AwakenedGrade>(Index(grade) + 1);
}

const GradeRequirement& RequirementFor(AwakenedGrade grade)
{
    return Requirements[Index(grade)];
}

std::int32_t DimensionCeilingCentipoints(AwakenedGrade grade)
{
    const std::optional<AwakenedGrade> next = NextGrade(grade);
    if (!next)
    {
        return MaxRatingCentipoints;
    }
    return std::min(MaxRatingCentipoints, (RequirementFor(*next).RatingPoints + 100) * CentipointsPerPoint);
}

std::optional<AwakenedProfile> Awaken(const std::array<std::int32_t, TattvaCount>& affinityPercent,
                                      const std::array<std::int32_t, PowerDimensionCount>& startingRatingPoints,
                                      std::int64_t day)
{
    std::int32_t total = 0;
    for (const std::int32_t percent : affinityPercent)
    {
        if (percent < 0 || percent > 100)
        {
            return std::nullopt;
        }
        total += percent;
    }
    if (total != 100 || day < 0)
    {
        return std::nullopt;
    }
    AwakenedProfile profile;
    profile.AffinityPercent = affinityPercent;
    for (std::int32_t d = 0; d < PowerDimensionCount; ++d)
    {
        const std::int64_t wanted = static_cast<std::int64_t>(startingRatingPoints[d]) * CentipointsPerPoint;
        const std::int32_t rating = static_cast<std::int32_t>(std::clamp<std::int64_t>(wanted, 0, AwakeningCeilingCentipoints));
        profile.Rating[d] = rating;
        profile.Peak[d] = rating;
        profile.LastPracticeDay[d] = day;
        profile.DecaySettledDay[d] = day;
    }
    profile.EarliestRetakeDay = day;
    return profile;
}

std::int32_t ProfileRatingCentipoints(const AwakenedProfile& profile)
{
    std::array<std::int32_t, PowerDimensionCount> sorted = profile.Rating;
    std::sort(sorted.begin(), sorted.end(), std::greater<>());
    return (sorted[0] + sorted[1] + sorted[2] + sorted[3]) / 4;
}

std::int32_t StageWithinGrade(const AwakenedProfile& profile)
{
    const std::int32_t entry = RequirementFor(profile.Grade).RatingPoints * CentipointsPerPoint;
    const std::optional<AwakenedGrade> next = NextGrade(profile.Grade);
    const std::int32_t target = next ? RequirementFor(*next).RatingPoints * CentipointsPerPoint : MaxRatingCentipoints;
    const std::int64_t progress = ProfileRatingCentipoints(profile) - entry;
    if (progress <= 0)
    {
        return 1;
    }
    const std::int64_t stage = 1 + progress * 10 / (target - entry);
    return static_cast<std::int32_t>(std::min<std::int64_t>(stage, 10));
}

void SettleDecay(AwakenedProfile& profile, std::int64_t day)
{
    for (std::int32_t d = 0; d < PowerDimensionCount; ++d)
    {
        const std::int64_t decayStart = profile.LastPracticeDay[d] + IdleDaysBeforeDecay;
        const std::int64_t from = std::max(profile.DecaySettledDay[d], decayStart);
        if (day <= from)
        {
            continue;
        }
        const std::int64_t periods = (day - from) / DecayPeriodDays;
        if (periods == 0)
        {
            continue;
        }
        const std::int64_t floor = static_cast<std::int64_t>(profile.Peak[d]) * DecayFloorPercentOfPeak / 100;
        const std::int64_t loss = periods * profile.Peak[d] * DecayPercentPerPeriod / 100;
        const std::int64_t rating = std::max<std::int64_t>(profile.Rating[d] - loss, std::min<std::int64_t>(floor, profile.Rating[d]));
        profile.Rating[d] = static_cast<std::int32_t>(rating);
        profile.DecaySettledDay[d] = from + periods * DecayPeriodDays;
    }
}

PracticeResult Practise(AwakenedProfile& profile, PowerDimension dimension, std::int32_t minutes, std::int64_t day,
                        std::int32_t qualityPercent, bool supervised)
{
    PracticeResult result;
    if (minutes <= 0 || day < 0)
    {
        return result;
    }
    SettleDecay(profile, day);
    const std::int32_t d = static_cast<std::int32_t>(dimension);
    PracticeDay& slot = profile.Window[d][static_cast<std::size_t>(day % PracticeDaysInWindow)];
    if (slot.Day != day)
    {
        slot = PracticeDay{day, 0, 0};
    }

    // Daily diminishing returns.
    const std::int64_t start = slot.Minutes;
    const std::int64_t end = start + minutes;
    std::int64_t weightedMinutes = 0;
    for (const Band& band : DailyBands)
    {
        const std::int64_t overlap = std::min<std::int64_t>(end, band.ToMinute) - std::max<std::int64_t>(start, band.FromMinute);
        if (overlap > 0)
        {
            weightedMinutes += overlap * band.Percent;
        }
    }
    result.HitDailyLimit = end > DailyLimitMinutes;
    std::int64_t gain = weightedMinutes * CentipointsPerFullMinute / 100;
    gain = gain * std::clamp(qualityPercent, 0, MaxSessionQualityPercent) / 100;

    // Growth slows over the last 100 points below the grade ceiling.
    const std::int32_t ceiling = DimensionCeilingCentipoints(profile.Grade);
    const std::int64_t headroom = std::max<std::int64_t>(0, ceiling - profile.Rating[d]);
    const std::int64_t headroomPercent = std::clamp<std::int64_t>(headroom * 100 / (100 * CentipointsPerPoint), 25, 100);
    gain = gain * headroomPercent / 100;
    // Weekly ceiling for the grade held.
    const std::int64_t weeklyCap = static_cast<std::int64_t>(RequirementFor(profile.Grade).WeeklyCeilingPoints) * CentipointsPerPoint;
    const std::int64_t remaining = std::max<std::int64_t>(0, weeklyCap - WindowGain(profile, d, day));
    if (gain > remaining)
    {
        gain = remaining;
        result.HitWeeklyCeiling = true;
    }
    // Recovering decayed rating earns the session's gain a second time, up to
    // the old peak: double the normal rate, with the bonus outside the window.
    const std::int64_t recovery = std::min<std::int64_t>(gain, std::max<std::int64_t>(0, profile.Peak[d] - profile.Rating[d]));
    std::int64_t total = gain + recovery;
    if (profile.Rating[d] + total > ceiling)
    {
        total = headroom;
        result.HitGradeCeiling = true;
    }

    profile.Rating[d] += static_cast<std::int32_t>(total);
    profile.Peak[d] = std::max(profile.Peak[d], profile.Rating[d]);
    slot.Minutes += minutes;
    slot.GainCentipoints += static_cast<std::int32_t>(std::min(gain, total));
    profile.LastPracticeDay[d] = day;
    profile.DecaySettledDay[d] = day;
    if (supervised)
    {
        profile.SupervisedPracticeMinutes += minutes;
    }
    result.GainCentipoints = static_cast<std::int32_t>(total);
    return result;
}

AssessmentEligibility CheckAssessmentEligibility(const AwakenedProfile& profile, std::int64_t day, bool fieldRequirementsMet)
{
    const std::optional<AwakenedGrade> next = NextGrade(profile.Grade);
    if (!next)
    {
        return AssessmentEligibility::AlreadyHighestGrade;
    }
    if (day < profile.EarliestRetakeDay)
    {
        return AssessmentEligibility::RetakeTooSoon;
    }
    const GradeRequirement& need = RequirementFor(*next);
    if (ProfileRatingCentipoints(profile) < need.RatingPoints * CentipointsPerPoint)
    {
        return AssessmentEligibility::RatingTooLow;
    }
    for (const std::int32_t rating : profile.Rating)
    {
        if (rating < need.DimensionFloorPoints * CentipointsPerPoint)
        {
            return AssessmentEligibility::DimensionBelowFloor;
        }
    }
    if (profile.SupervisedPracticeMinutes < need.SupervisedPracticeMinutes)
    {
        return AssessmentEligibility::NeedsSupervisedPractice;
    }
    // E needs only supervised practice; every higher grade needs field assessments.
    if (*next != AwakenedGrade::E && !fieldRequirementsMet)
    {
        return AssessmentEligibility::FieldRequirementsNotMet;
    }
    return AssessmentEligibility::Eligible;
}

AssessmentEligibility SitAssessment(AwakenedProfile& profile, std::int64_t day, bool fieldRequirementsMet, bool passed)
{
    const AssessmentEligibility eligibility = CheckAssessmentEligibility(profile, day, fieldRequirementsMet);
    if (eligibility != AssessmentEligibility::Eligible)
    {
        return eligibility;
    }
    if (passed)
    {
        profile.Grade = *NextGrade(profile.Grade);
    }
    else
    {
        profile.EarliestRetakeDay = day + RetakeWaitDays;
    }
    return AssessmentEligibility::Eligible;
}
} // namespace PerfectWorld
