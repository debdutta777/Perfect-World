// ROADMAP M1-34: the awakening reader. The same life gives the same profile; conduct moves the profile
// within documented bounds; the awakening never exceeds F.
#include "AwakeningPersonas.h"

using namespace PerfectWorld;

namespace
{
using A = ConductAction;
using F = AptitudeFacet;
using S = AptitudeFacetState;

constexpr std::int64_t AwDay = 6'000;

std::int32_t Sum(const std::array<std::int32_t, PowerDimensionCount>& values)
{
    std::int32_t total = 0;
    for (const std::int32_t value : values)
        total += value;
    return total;
}

Life StrongestLife(std::vector<std::pair<ConductAction, std::int32_t>> conduct)
{
    Life life;
    life.Growth = {30'000, 30'000, 30'000, 30'000, 30'000, 30'000};
    for (const char* skill : {"skill.academics", "skill.computing", "skill.practical_repair", "skill.household_tasks", "skill.fitness",
                              "skill.communication", "skill.logistics", "skill.commerce", "skill.self_management", "skill.creative"})
        life.Skills.push_back({skill, 100'000});
    for (std::size_t f = 0; f < AptitudeFacetCount; ++f)
        life.Facets.push_back({static_cast<AptitudeFacet>(f), S::Established});
    life.Conduct = std::move(conduct);
    return life;
}

Life ModestLife(std::vector<std::pair<ConductAction, std::int32_t>> conduct)
{
    Life life;
    life.Growth = {3'000, 2'000, 6'000, 4'000, 5'000, 3'000};
    life.Skills = {{"skill.academics", 30'000}, {"skill.fitness", 20'000}, {"skill.communication", 20'000}};
    life.Facets = {{F::Analysis, S::Shown}, {F::Physical, S::Shown}};
    life.Conduct = std::move(conduct);
    return life;
}

AwakenedProfile ProfileOf(const AwakeningResult& result)
{
    const std::optional<AwakenedProfile> profile = AwakenedStartingProfile(result, AwDay);
    Check(profile.has_value(), "an awakening result becomes a starting profile");
    return *profile;
}

void CheckProfileMatches(const AwakeningResult& result, const AwakenedProfile& profile)
{
    for (std::size_t t = 0; t < TattvaCount; ++t)
        Check(profile.AffinityPercent[t] == result.TattvaPercent[t], "the affinity is the result's whole percent");
    for (std::size_t d = 0; d < PowerDimensionCount; ++d)
    {
        Check(profile.Rating[d] == result.Rating[d] * CentipointsPerPoint, "each rating is the result's, in centipoints");
        Check(profile.Peak[d] == profile.Rating[d], "the starting peak is the starting rating");
        Check(profile.LastPracticeDay[d] == AwDay && profile.DecaySettledDay[d] == AwDay, "the clocks start at awakening");
    }
}

/** Every combination of three tests among the six actions at stakes 1 to 3: the grid of what the dungeon can record. */
std::vector<Life> ConductGrid()
{
    std::vector<Life> lives;
    for (int a = 0; a < static_cast<int>(ConductActionCount); ++a)
    {
        for (int b = 0; b < static_cast<int>(ConductActionCount); ++b)
        {
            for (int c = 0; c < static_cast<int>(ConductActionCount); c += 2)
            {
                const std::int32_t stakes = 1 + (a + b + c) % 3;
                lives.push_back(ModestLife({{static_cast<ConductAction>(a), stakes},
                                            {static_cast<ConductAction>(b), 1 + (stakes % 3)},
                                            {static_cast<ConductAction>(c), 3}}));
            }
        }
    }
    return lives;
}

void TestSameLifeSameProfile()
{
    for (const Persona& persona : NonCombatPersonas())
    {
        const AwakeningResult result = AwakenLife(persona.Evidence);
        const AwakenedProfile first = ProfileOf(result);
        const AwakenedProfile again = ProfileOf(AwakenLife(persona.Evidence));
        CheckProfileMatches(result, first);
        Check(first.AffinityPercent == again.AffinityPercent && first.Rating == again.Rating && first.Grade == again.Grade,
              "the same life gives the same profile");
        Check(first.Grade == AwakenedGrade::F, "the awakening grants F");
        Check(first.SupervisedPracticeMinutes == 0 && first.EarliestRetakeDay == AwDay, "no practice and no retake wait at awakening");
        Check(StageWithinGrade(first) >= 1 && StageWithinGrade(first) <= 10, "the stage is F-1 to F-10");
        Check(ProfileRatingCentipoints(first) < RequirementFor(AwakenedGrade::E).RatingPoints * CentipointsPerPoint,
              "the profile's rating is below E's");
        // It is a real profile: the practice rules accept it.
        AwakenedProfile practising = first;
        Check(Practise(practising, PowerDimension::Output, 60, AwDay + 1).GainCentipoints > 0, "the profile can be practised from");
    }
    Check(!AwakenedStartingProfile(AwakenLife(Life{}), -1).has_value(), "a negative day is refused");
    AwakeningResult broken = AwakenLife(Life{});
    broken.Error = "broken";
    Check(!AwakenedStartingProfile(broken, AwDay).has_value(), "a result that did not resolve is refused");
    AwakeningResult wild = AwakenLife(Life{});
    wild.Rating[0] = AwakeningMaxRatingPoints + 1;
    Check(!AwakenedStartingProfile(wild, AwDay).has_value(), "a rating above the documented maximum is refused");
}

void TestNeverExceedsF()
{
    for (const Life& life : {StrongestLife({{A::Assist, 3}, {A::Improvise, 3}, {A::Observe, 3}}),
                             StrongestLife({{A::Hold, 3}, {A::Hold, 3}, {A::Hold, 3}}), StrongestLife({}), Life{}})
    {
        const AwakeningResult result = AwakenLife(life);
        const AwakenedProfile profile = ProfileOf(result);
        CheckProfileMatches(result, profile);
        for (std::size_t d = 0; d < PowerDimensionCount; ++d)
        {
            Check(result.Rating[d] <= AwakeningMaxRatingPoints, "no rating exceeds the documented maximum");
            Check(profile.Rating[d] < RequirementFor(AwakenedGrade::E).RatingPoints * CentipointsPerPoint, "so no dimension reaches E's rating");
        }
        Check(profile.Grade == AwakenedGrade::F, "the awakening cannot promote beyond F");
        Check(CheckAssessmentEligibility(profile, AwDay, true) != AssessmentEligibility::Eligible, "a fresh profile is not eligible for E");
    }
    // The strongest possible life reaches the top of the band, and not past it.
    const AwakeningResult top = AwakenLife(StrongestLife({{A::Assist, 3}, {A::Assist, 3}, {A::Assist, 3}}));
    std::int32_t best = 0;
    for (const std::int32_t rating : top.Rating)
        best = std::max(best, rating);
    Check(best > AwakeningLatentCapabilityMaxPoints, "the strongest life earns more than the sixteen years' maximum alone");
    Check(best <= AwakeningMaxRatingPoints, "and stays inside the maximum");
}

void TestConductMovesWithinBounds()
{
    const AwakeningResult absent = AwakenLife(ModestLife({}));
    const AwakenedProfile absentProfile = ProfileOf(absent);
    const std::vector<Life> grid = ConductGrid();
    Check(grid.size() == 6 * 6 * 3, "the grid of conduct is the one expected");
    std::int32_t widest = 0;
    for (const Life& life : grid)
    {
        const AwakeningResult result = AwakenLife(life);
        Check(result.Ok(), "every conduct resolves");
        const AwakenedProfile profile = ProfileOf(result);
        for (std::size_t d = 0; d < PowerDimensionCount; ++d)
        {
            Check(result.LatentRating[d] == absent.LatentRating[d], "conduct never moves the sixteen years' term");
            const std::int32_t move = result.Rating[d] - absent.Rating[d];
            // Against absence a dimension moves by at most the remainder up, and down to the floor from the even split.
            Check(move <= AwakeningConductMaxPoints - AwakeningConductBudgetPoints / PowerDimensionCount, "conduct moves a rating up by a bounded amount");
            Check(move >= AwakeningConductFloorPoints - AwakeningConductBudgetPoints / PowerDimensionCount, "and down by a bounded amount");
            Check(profile.Rating[d] - absentProfile.Rating[d] == move * CentipointsPerPoint, "the profile moves as the result does");
            widest = std::max(widest, move);
        }
        Check(Sum(result.ConductRating) == Sum(absent.ConductRating), "conduct redistributes, never adds or removes");
        for (std::size_t t = 0; t < TattvaCount; ++t)
            Check(profile.AffinityPercent[t] >= AwakeningTattvaFloorMilli / 1000, "whatever the conduct, no Tattva falls below its floor");
    }
    Check(widest > 0, "conduct does move the ratings");
    Check(widest <= AwakeningConductMaxPoints - AwakeningConductBudgetPoints / PowerDimensionCount, "by no more than the documented amount");
}

void TestConductShiftsTowardTheChoice()
{
    // A life with no preference: ratings move toward where the dungeon pointed.
    const AwakeningResult carer = AwakenLife(ModestLife({{A::Assist, 3}, {A::Assist, 3}, {A::Assist, 3}}));
    const AwakeningResult watcher = AwakenLife(ModestLife({{A::Observe, 3}, {A::Observe, 3}, {A::Observe, 3}}));
    const std::size_t coordination = static_cast<std::size_t>(PowerDimension::Coordination);
    const std::size_t perception = static_cast<std::size_t>(PowerDimension::Perception);
    Check(carer.ConductRating[coordination] > watcher.ConductRating[coordination], "care moves points toward coordination");
    Check(watcher.ConductRating[perception] > carer.ConductRating[perception], "attentiveness moves points toward perception");
    Check(!carer.RatingCauses[coordination].empty(), "the rating names its evidence");
    bool conductNamed = false;
    for (const AwakeningCause& cause : carer.RatingCauses[coordination])
        conductNamed = conductNamed || (AwakeningSignalIsConduct(cause.Signal) && !cause.TestId.empty() && !cause.ChoiceId.empty());
    Check(conductNamed, "and a conduct cause names the test and the choice behind it");
}
} // namespace

void TestAwakeningProfile()
{
    TestSameLifeSameProfile();
    TestNeverExceedsF();
    TestConductMovesWithinBounds();
    TestConductShiftsTowardTheChoice();
}
