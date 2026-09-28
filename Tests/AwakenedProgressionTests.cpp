#include "AwakenedProgression.h"

#include <cstdlib>
#include <iostream>

using namespace PerfectWorld;

int Checks = 0;
void Check(bool condition, const char* message)
{
    ++Checks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

namespace
{
constexpr std::array<std::int32_t, TattvaCount> Balanced = {20, 20, 20, 20, 20};

AwakenedProfile Fresh(std::int32_t startingPoints = 40, std::int64_t day = 0)
{
    std::array<std::int32_t, PowerDimensionCount> start{};
    start.fill(startingPoints);
    const std::optional<AwakenedProfile> profile = Awaken(Balanced, start, day);
    Check(profile.has_value(), "a valid awakening succeeds");
    return *profile;
}

std::int32_t Points(std::int32_t centipoints)
{
    return centipoints / CentipointsPerPoint;
}
} // namespace

int main()
{
    // The requirement table climbs strictly and matches Docs/AWAKENED_GRADES.md §4.
    Check(RequirementFor(AwakenedGrade::E).RatingPoints == 150 && RequirementFor(AwakenedGrade::S).RatingPoints == 900,
          "E needs 150 and S needs 900");
    for (std::int32_t g = 1; g < AwakenedGradeCount; ++g)
    {
        const GradeRequirement& lower = RequirementFor(static_cast<AwakenedGrade>(g - 1));
        const GradeRequirement& higher = RequirementFor(static_cast<AwakenedGrade>(g));
        Check(higher.RatingPoints > lower.RatingPoints, "each grade needs a higher rating");
        Check(higher.DimensionFloorPoints > lower.DimensionFloorPoints, "each grade needs a higher floor");
        Check(higher.WeeklyCeilingPoints < lower.WeeklyCeilingPoints, "weekly ceilings shrink as grades rise");
    }
    Check(!NextGrade(AwakenedGrade::S).has_value() && *NextGrade(AwakenedGrade::F) == AwakenedGrade::E, "grade order");
    Check(AwakenedGradeName(AwakenedGrade::C) == "C", "grade names");

    // Awakening: a valid affinity is required, and nobody awakens above F.
    {
        std::array<std::int32_t, PowerDimensionCount> start{};
        Check(!Awaken({50, 50, 1, 0, 0}, start, 0).has_value(), "affinity must sum to exactly 100");
        Check(!Awaken({120, -20, 0, 0, 0}, start, 0).has_value(), "affinity values must be 0..100");
        start.fill(900);
        const AwakenedProfile gifted = *Awaken({100, 0, 0, 0, 0}, start, 0);
        Check(gifted.Grade == AwakenedGrade::F, "even a gifted awakening is F");
        Check(ProfileRatingCentipoints(gifted) < RequirementFor(AwakenedGrade::E).RatingPoints * CentipointsPerPoint,
              "awakening never reaches E's rating");
        Check(StageWithinGrade(gifted) == 10, "a gifted awakening is F-10");
        Check(StageWithinGrade(Fresh(0)) == 1, "a weak awakening is F-1");
    }

    // The rating is the four strongest dimensions averaged: specialists advance.
    {
        AwakenedProfile p = Fresh(0);
        p.Rating = {400 * 100, 400 * 100, 400 * 100, 400 * 100, 0, 0};
        Check(ProfileRatingCentipoints(p) == 400 * 100, "two weak dimensions do not drag a specialist down");
        p.Rating = {400 * 100, 0, 0, 0, 0, 0};
        Check(ProfileRatingCentipoints(p) == 100 * 100, "one strong dimension is not a rating");
    }

    // Daily diminishing returns: the first hour is worth most, and after four
    // hours the day is spent.
    {
        AwakenedProfile p = Fresh();
        const std::int32_t first = Practise(p, PowerDimension::Output, 60, 10).GainCentipoints;
        AwakenedProfile q = Fresh();
        Practise(q, PowerDimension::Output, 60, 10);
        const std::int32_t second = Practise(q, PowerDimension::Output, 60, 10).GainCentipoints;
        Check(first == 12 * 100, "a full-value hour is 12 points");
        Check(second < first, "the second hour is worth less than the first");
        AwakenedProfile r = Fresh();
        r.Grade = AwakenedGrade::F;
        Practise(r, PowerDimension::Precision, 240, 11);
        const PracticeResult spent = Practise(r, PowerDimension::Precision, 60, 11);
        Check(spent.GainCentipoints == 0 && spent.HitDailyLimit, "after four hours further practice that day earns nothing");
        Check(Practise(r, PowerDimension::Range, 30, 11).GainCentipoints > 0, "other dimensions keep their own day");
    }

    // The weekly ceiling: cramming cannot beat steady practice.
    {
        AwakenedProfile crammer = Fresh();
        std::int32_t crammed = 0;
        for (std::int64_t day = 100; day < 106; ++day)
        {
            crammed += Practise(crammer, PowerDimension::Duration, 240, day, 150).GainCentipoints;
        }
        Check(crammed == RequirementFor(AwakenedGrade::F).WeeklyCeilingPoints * CentipointsPerPoint,
              "six days of maximum practice earn exactly the weekly ceiling");
        const PracticeResult blocked = Practise(crammer, PowerDimension::Duration, 60, 106);
        Check(blocked.GainCentipoints == 0 && blocked.HitWeeklyCeiling, "the ceiling holds within the week");
        Check(Practise(crammer, PowerDimension::Duration, 60, 107).GainCentipoints > 0, "the window slides and practice counts again");
        AwakenedProfile rich = Fresh();
        const std::int32_t luxury = Practise(rich, PowerDimension::Perception, 30, 20, 500).GainCentipoints;
        Check(luxury == 6 * 100 * MaxSessionQualityPercent / 100, "facility quality is capped at 150%");
    }

    // Stage 10 does not promote; only an assessment does.
    {
        AwakenedProfile p = Fresh(0);
        std::int64_t day = 1;
        while (ProfileRatingCentipoints(p) < DimensionCeilingCentipoints(AwakenedGrade::F) && day < 5'000)
        {
            for (std::int32_t d = 0; d < PowerDimensionCount; ++d)
            {
                Practise(p, static_cast<PowerDimension>(d), 60, day, 100, true);
            }
            ++day;
        }
        Check(ProfileRatingCentipoints(p) == DimensionCeilingCentipoints(AwakenedGrade::F), "practice stops at the grade ceiling");
        Check(p.Grade == AwakenedGrade::F && StageWithinGrade(p) == 10, "still F-10 without an assessment");
        const PracticeResult capped = Practise(p, PowerDimension::Output, 60, day + 7);
        Check(capped.GainCentipoints == 0 && capped.HitGradeCeiling, "no gain past the ceiling");

        Check(CheckAssessmentEligibility(p, day, false) == AssessmentEligibility::Eligible, "E needs no field assessment");
        Check(SitAssessment(p, day, false, false) == AssessmentEligibility::Eligible && p.Grade == AwakenedGrade::F,
              "a failed assessment does not promote");
        Check(CheckAssessmentEligibility(p, day + 29, false) == AssessmentEligibility::RetakeTooSoon, "retake waits 30 days");
        SitAssessment(p, day + 30, false, true);
        Check(p.Grade == AwakenedGrade::E, "a passed assessment promotes");
        Check(CheckAssessmentEligibility(p, day + 30, true) == AssessmentEligibility::RatingTooLow, "D needs a higher rating");
    }

    // Each blocking reason is reported.
    {
        AwakenedProfile p = Fresh(0);
        p.Rating = {200 * 100, 200 * 100, 200 * 100, 200 * 100, 10 * 100, 200 * 100};
        Check(CheckAssessmentEligibility(p, 0, true) == AssessmentEligibility::DimensionBelowFloor, "a neglected dimension blocks");
        p.Rating[4] = 100 * 100;
        Check(CheckAssessmentEligibility(p, 0, true) == AssessmentEligibility::NeedsSupervisedPractice, "E needs 20 supervised hours");
        p.SupervisedPracticeMinutes = 20 * 60;
        Check(CheckAssessmentEligibility(p, 0, true) == AssessmentEligibility::Eligible, "then E is open");
        p.Grade = AwakenedGrade::E;
        p.Rating.fill(350 * 100);
        Check(CheckAssessmentEligibility(p, 0, false) == AssessmentEligibility::FieldRequirementsNotMet, "D needs a field assessment");
        p.Grade = AwakenedGrade::S;
        Check(CheckAssessmentEligibility(p, 0, true) == AssessmentEligibility::AlreadyHighestGrade, "S is the top");
    }

    // Rest: grades never decay; ratings decay slowly and recover fast.
    {
        AwakenedProfile p = Fresh(0);
        p.Grade = AwakenedGrade::C;
        p.Rating.fill(500 * 100);
        p.Peak.fill(500 * 100);
        p.LastPracticeDay.fill(0);
        p.DecaySettledDay.fill(0);
        SettleDecay(p, 90);
        Check(p.Rating[0] == 500 * 100, "ninety idle days cost nothing");
        SettleDecay(p, 150);
        Check(p.Rating[0] == 490 * 100, "then 1% of peak per 30 days");
        SettleDecay(p, 10'000);
        Check(p.Rating[0] == 400 * 100, "never below 80% of peak");
        Check(p.Grade == AwakenedGrade::C, "the grade is kept");
        AwakenedProfile unrested = p;
        unrested.Peak[0] = unrested.Rating[0];
        const std::int32_t back = Practise(p, PowerDimension::Output, 60, 10'001).GainCentipoints;
        const std::int32_t normal = Practise(unrested, PowerDimension::Output, 60, 10'001).GainCentipoints;
        Check(normal > 0, "a normal session gains something");
        Check(back == 2 * normal, "lost rating recovers at double rate");
    }

    // A whole career at maximum effort: S takes years, not weeks. Practice
    // four hours on every dimension every day and pass every assessment the
    // moment it opens.
    {
        AwakenedProfile p = Fresh(40);
        std::int64_t day = 0;
        std::array<std::int64_t, AwakenedGradeCount> reached{};
        while (p.Grade != AwakenedGrade::S && day < 40 * 365)
        {
            ++day;
            for (std::int32_t d = 0; d < PowerDimensionCount; ++d)
            {
                Practise(p, static_cast<PowerDimension>(d), 240, day, 150, true);
            }
            if (CheckAssessmentEligibility(p, day, true) == AssessmentEligibility::Eligible)
            {
                SitAssessment(p, day, true, true);
                reached[static_cast<std::size_t>(p.Grade)] = day;
            }
        }
        Check(p.Grade == AwakenedGrade::S, "S is reachable within a working life");
        Check(reached[static_cast<std::size_t>(AwakenedGrade::E)] >= 7 * 5, "E takes more than a month");
        Check(day >= static_cast<std::int64_t>(2.5 * 365), "even maximum effort needs over two and a half years for S");
        Check(day <= 15 * 365, "and a dedicated specialist gets there within fifteen");
        std::cout << "Maximum-effort S after " << day << " days (" << Points(ProfileRatingCentipoints(p)) << " rating)\n";
    }

    std::cout << "PASS: " << Checks << " awakened progression checks\n";
    return 0;
}
