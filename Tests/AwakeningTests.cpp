// ROADMAP M4-02 and M1-34: the awakening and its reader.
// Usage: PerfectWorldAwakeningTests <path to Data/Content>
#include "AwakeningTestSupport.h"

using namespace PerfectWorld;

namespace
{
using A = ConductAction;
using F = AptitudeFacet;
using S = AptitudeFacetState;

/** A student who read, repaired and ran a little: some of everything, so conduct has latent company. */
Life SteadyLife(std::vector<std::pair<ConductAction, std::int32_t>> conduct)
{
    Life life;
    life.Growth = {4'000, 3'000, 9'000, 5'000, 6'000, 4'000};
    life.Skills = {{"skill.academics", 40'000}, {"skill.practical_repair", 20'000}, {"skill.fitness", 20'000},
                   {"skill.communication", 20'000}};
    life.Facets = {{F::Analysis, S::Shown}, {F::Teamwork, S::Shown}};
    life.Conduct = std::move(conduct);
    return life;
}

std::int32_t Sum(const std::array<std::int32_t, PowerDimensionCount>& values)
{
    std::int32_t total = 0;
    for (const std::int32_t value : values)
        total += value;
    return total;
}

std::int32_t SumMilli(const AwakeningResult& result)
{
    std::int32_t total = 0;
    for (const std::int32_t value : result.TattvaMilli)
        total += value;
    return total;
}

/** The invariants every result keeps, whatever the life. */
void CheckInvariants(const AwakeningResult& result, const char* what)
{
    if (!result.Ok())
        std::cerr << "invariants failed for " << what << ": " << result.Error << '\n';
    Check(result.Ok(), "the awakening resolves");
    Check(SumMilli(result) == AwakeningTattvaTotalMilli, "the distribution sums to 100,000 milli");
    std::int32_t percent = 0;
    for (std::size_t t = 0; t < TattvaCount; ++t)
    {
        Check(result.TattvaMilli[t] >= AwakeningTattvaFloorMilli, "no Tattva is ever shut out");
        percent += result.TattvaPercent[t];
        Check(result.TattvaCauses[t].size() <= AwakeningCauseCount, "at most three causes per Tattva");
    }
    Check(percent == 100, "the whole percent sums to exactly 100");
    for (std::size_t d = 0; d < PowerDimensionCount; ++d)
    {
        Check(result.Rating[d] == result.LatentRating[d] + result.ConductRating[d], "a rating is its two terms");
        Check(result.ConductRating[d] >= AwakeningConductFloorPoints && result.ConductRating[d] <= AwakeningConductMaxPoints,
              "a conduct term stays between its floor and its maximum");
        Check(result.LatentRating[d] >= 0 && result.LatentRating[d] <= AwakeningLatentCapabilityMaxPoints,
              "a latent term stays inside its range");
        Check(result.Rating[d] < 150, "awakening never reaches E (F's ceiling is 149 points)");
        Check(result.RatingCauses[d].size() <= AwakeningCauseCount, "at most three causes per rating");
    }
    Check(Sum(result.ConductRating) == AwakeningConductBudgetPoints, "conduct always supplies exactly the fixed budget");
    Check(!result.Ability.RecipeId.empty() && result.Ability.OriginTag == AwakeningOriginTag &&
              result.Ability.Rank == AwakeningAbilityRank,
          "exactly one ability is granted");
    Check(result.Ability.Causes.size() <= AwakeningCauseCount, "at most three causes for the ability");
    Check(TestWorld->Recipes.Find(result.Ability.RecipeId) != nullptr, "the ability is an authored recipe");
    for (const std::vector<AwakeningCause>& causes : result.TattvaCauses)
    {
        for (std::size_t i = 1; i < causes.size(); ++i)
            Check(causes[i - 1].Contribution >= causes[i].Contribution, "causes are strongest first");
    }
}

void TestRecipeSet()
{
    const AbilityRecipeSet& set = TestWorld->Recipes;
    Check(set.Recipes.size() >= 2 * TattvaCount, "there are recipes beyond the fallbacks");
    for (std::size_t i = 1; i < set.Recipes.size(); ++i)
        Check(set.Recipes[i - 1].Id < set.Recipes[i].Id, "recipes are ascending by id");
    for (std::size_t t = 0; t < TattvaCount; ++t)
    {
        const Tattva tattva = static_cast<Tattva>(t);
        Check(set.FallbackFor(tattva) != nullptr && set.FallbackFor(tattva)->Requires.empty(), "every Tattva has a fallback that asks for nothing");
        int evidence = 0;
        int latentOnly = 0;
        for (const AbilityRecipe& recipe : set.Recipes)
        {
            if (recipe.Affinity != tattva || recipe.Fallback)
                continue;
            ++evidence;
            bool conduct = false;
            for (const AbilityRequirement& need : recipe.Requires)
                conduct = conduct || AwakeningSignalIsConduct(need.Signal);
            latentOnly += conduct ? 0 : 1;
            Check(!recipe.Requires.empty() && recipe.Requires.size() <= AwakeningRecipeMaxRequirements, "an evidence recipe names 1 to 3 requirements");
        }
        Check(evidence >= 1, "every Tattva has an evidence recipe");
        Check(latentOnly >= 1, "every Tattva has an evidence recipe a student who never entered the dungeon can earn");
    }
    Check(set.Find("ability_recipe.no_such") == nullptr, "an unknown recipe is not found");
    Check(AwakeningSignalName(AwakeningSignal::Care) == "care" && ParseAwakeningSignal("care") == AwakeningSignal::Care, "signal names round-trip");
    Check(ParseAwakeningSignal("kindness") == std::nullopt, "an unknown signal is refused");
    Check(ParseAwakeningTattva("agni") == Tattva::Agni && AwakeningTattvaId(Tattva::Akasha) == "akasha", "Tattva ids round-trip");
    Check(AwakeningCapabilityName(PowerDimension::Coordination) == "coordination", "capability names are stable");
}

void TestDeterminismAndConduct()
{
    const Life steady = SteadyLife({{A::Assist, 2}, {A::Observe, 1}, {A::Improvise, 2}});
    const AwakeningResult first = AwakenLife(steady);
    CheckInvariants(first, "steady");
    Check(first == AwakenLife(steady), "the same life gives the same result");
    Check(first.ConductObserved, "a student with recorded tests has conduct observed");

    // Changing only the conduct changes the result; the sixteen years are untouched.
    const AwakeningResult carer = AwakenLife(SteadyLife({{A::Assist, 3}, {A::Assist, 3}, {A::Assist, 3}}));
    const AwakeningResult watcher = AwakenLife(SteadyLife({{A::Observe, 3}, {A::Observe, 3}, {A::Observe, 3}}));
    const AwakeningResult runner = AwakenLife(SteadyLife({{A::Evade, 3}, {A::Evade, 3}, {A::Evade, 3}}));
    CheckInvariants(carer, "carer");
    CheckInvariants(watcher, "watcher");
    CheckInvariants(runner, "runner");
    for (std::size_t s = 0; s < AwakeningLatentSignalCount; ++s)
        Check(carer.Signals[s] == watcher.Signals[s] && carer.Signals[s] == runner.Signals[s], "the latent readings do not move with conduct");
    Check(carer.Signals != watcher.Signals && carer.Signals != runner.Signals, "the conduct readings move with conduct");
    Check(carer.TattvaMilli != watcher.TattvaMilli, "conduct alone changes the distribution");
    Check(carer.Rating != watcher.Rating && watcher.Rating != runner.Rating, "conduct alone changes the capability profile");
    Check(carer.Ability.RecipeId != watcher.Ability.RecipeId || carer.TattvaMilli != runner.TattvaMilli,
          "conduct alone changes the result as a whole");

    // The seed breaks ties only: a life with a clear result is the same under any seed.
    for (std::uint64_t seed = 1; seed < 40; ++seed)
    {
        const AwakeningResult other = AwakenLife(steady, seed * 7919);
        CheckInvariants(other, "seeded");
        Check(LeadingTattva(other) == LeadingTattva(first), "the seed does not move the leading Tattva of a clear life");
        Check(other.Signals == first.Signals, "the seed does not move a reading");
    }
}

void TestAbsentAndWithdrawn()
{
    const AwakeningResult absent = AwakenLife(SteadyLife({}));
    CheckInvariants(absent, "absent");
    Check(!absent.ConductObserved, "a student with no tests recorded has no conduct observed");
    for (std::size_t c = AwakeningLatentSignalCount; c < AwakeningSignalCount; ++c)
        Check(absent.Signals[c] == 0, "no conduct is read for an absent student");
    for (std::size_t d = 0; d < PowerDimensionCount; ++d)
        Check(absent.ConductRating[d] == AwakeningConductBudgetPoints / PowerDimensionCount, "absence is the even split of the full budget");

    // A summary that rests on no test is the unpenalised case whatever it holds.
    AwakeningInput bogus = InputOf(SteadyLife({}));
    bogus.Conduct.Dimensions[0].Milli = 900;
    Check(ResolveAwakening(bogus, TestWorld->Recipes) == absent, "a summary resting on no test is read as absence");

    // Withdrawing, rescuing and being absent supply exactly the same total, and none is capped lower.
    const AwakeningResult withdrew = AwakenLife(SteadyLife({{A::Withdraw, 3}, {A::Withdraw, 3}, {A::Withdraw, 3}}));
    const AwakeningResult rescued = AwakenLife(SteadyLife({{A::Assist, 3}, {A::Assist, 3}, {A::Assist, 3}}));
    CheckInvariants(withdrew, "withdrew");
    CheckInvariants(rescued, "rescued");
    Check(Sum(withdrew.ConductRating) == Sum(rescued.ConductRating) && Sum(rescued.ConductRating) == Sum(absent.ConductRating),
          "withdrawal, rescue and absence receive the same conduct total");
    Check(Sum(withdrew.Rating) == Sum(rescued.Rating) && Sum(rescued.Rating) == Sum(absent.Rating),
          "so the three profiles have the same total rating");
    Check(withdrew.Rating != rescued.Rating, "while where the points sit still differs");

    // A latent-only recipe is earnable by an absent student: the scholar still gets an evidence ability.
    Life scholar;
    scholar.Growth = {0, 0, 28'000, 20'000, 0, 0};
    scholar.Skills = {{"skill.academics", 90'000}, {"skill.computing", 60'000}};
    scholar.Facets = {{F::Analysis, S::Established}, {F::Interpretation, S::Established}, {F::FaultFinding, S::Established}};
    const AwakeningResult scholarAbsent = AwakenLife(scholar);
    CheckInvariants(scholarAbsent, "scholar absent");
    Check(!scholarAbsent.Ability.Fallback, "an absent student can earn an evidence ability from the sixteen years alone");
    Check(!scholarAbsent.Ability.Causes.empty(), "and the ability names its evidence");
}

void TestNothingToReadAndTies()
{
    const Life blank;
    const AwakeningResult none = AwakenLife(blank);
    CheckInvariants(none, "blank");
    Check(none.Ability.Fallback, "a life with no evidence is granted a fallback");
    Check(none.Ability.Causes.empty(), "and no cause is invented");
    for (std::size_t t = 0; t < TattvaCount; ++t)
    {
        Check(none.TattvaMilli[t] == AwakeningTattvaTotalMilli / static_cast<std::int32_t>(TattvaCount), "no evidence is an even distribution");
        Check(none.TattvaCauses[t].empty(), "with no causes");
    }
    Check(none == AwakenLife(blank), "the same blank life gives the same result");

    // Tied scores are broken by the seed, not the list: any order of recipes gives the same answer.
    AbilityRecipeSet reversed = TestWorld->Recipes;
    std::reverse(reversed.Recipes.begin(), reversed.Recipes.end());
    std::vector<std::string> seen;
    for (std::uint64_t seed = 1; seed <= 60; ++seed)
    {
        const AwakeningResult a = ResolveAwakening(InputOf(blank, seed), TestWorld->Recipes);
        const AwakeningResult b = ResolveAwakening(InputOf(blank, seed), reversed);
        Check(a.Ability.RecipeId == b.Ability.RecipeId && a.TattvaPercent == b.TattvaPercent, "recipe order never decides a tie");
        Check(a == ResolveAwakening(InputOf(blank, seed), TestWorld->Recipes), "a tie resolves the same way twice");
        if (std::find(seen.begin(), seen.end(), a.Ability.RecipeId) == seen.end())
            seen.push_back(a.Ability.RecipeId);
    }
    Check(seen.size() >= 3, "the keyed draw spreads a tie across the Tattvas' fallbacks");

    AbilityRecipeSet noFallback = TestWorld->Recipes;
    std::erase_if(noFallback.Recipes, [](const AbilityRecipe& recipe) { return recipe.Fallback; });
    const AwakeningResult refused = ResolveAwakening(InputOf(blank), noFallback);
    Check(!refused.Ok() && refused.Ability.RecipeId.empty(), "a recipe set without a fallback is refused, never invented");
    Check(!ResolveAwakening(InputOf(blank), AbilityRecipeSet{}).Ok(), "an empty recipe set is refused");
}

/** Stream, household, appearance, name, pronouns and languages: none can reach the stats, so none can reach the result. */
void TestFairness()
{
    const CreationPreset* preset = CharacterCreator::FindPreset(DefaultPresetId);
    CharacterCreator plain;
    Check(plain.ApplyPreset(preset->Id).Succeeded(), "the plain creation applies its preset");
    CharacterCreator varied;
    Check(varied.ApplyPreset(preset->Id).Succeeded(), "the varied creation applies the same preset");
    Check(varied.SetHouseholdTemplate("grandparent_household").Succeeded(), "a different household template");
    Check(varied.SetSchoolStream(SchoolStreams()[2].Id, "I like the subjects").Succeeded(), "a different school stream");
    AppearanceRecord look = AppearanceRecord::Default();
    look.FaceShapeId = std::string(FaceShapes()[1].Id);
    look.HairStyleId = std::string(HairStyles()[2].Id);
    look.BuildId = std::string(Builds()[2].Id);
    look.SkinTone = {40, 30, 20};
    look.HairColor = {250, 250, 250};
    look.EyeColor = {10, 200, 90};
    Check(varied.SetAppearance(look).Succeeded(), "a different appearance");
    IdentityProfile identity = varied.Identity();
    identity.GivenName = "Zainab";
    identity.FamilyName = "Quraishi";
    identity.Pronouns = "she/her";
    identity.PrimaryLanguage = "Urdu";
    identity.AdditionalLanguages = {"Bengali", "English"};
    identity.Birth = EarliestDateOfBirth;
    Check(varied.SetIdentity(identity).Succeeded(), "a different name, pronouns, languages and birthday");

    const LatentStatsResult statsA = ComputeStartingStats(TestWorld->Content.Db, plain.CaptureSave());
    const LatentStatsResult statsB = ComputeStartingStats(TestWorld->Content.Db, varied.CaptureSave());
    Check(statsA.Ok() && statsB.Ok(), "both creations have starting stats");
    Check(statsA.Stats == statsB.Stats, "none of those can change the latent stats");

    // Run the same dungeon and the same projection through both: the awakenings are identical.
    const Life dungeon = SteadyLife({{A::Assist, 2}, {A::Hold, 1}, {A::Improvise, 2}});
    AwakeningInput a = InputOf(dungeon);
    AwakeningInput b = InputOf(dungeon);
    a.Stats = statsA.Stats;
    b.Stats = statsB.Stats;
    const AwakeningResult resultA = ResolveAwakening(a, TestWorld->Recipes);
    CheckInvariants(resultA, "plain creation");
    Check(resultA == ResolveAwakening(b, TestWorld->Recipes), "stream, household, appearance, pronouns, languages and names change nothing");
}
} // namespace

void TestAwakeningRules()
{
    TestRecipeSet();
    TestDeterminismAndConduct();
    TestAbsentAndWithdrawn();
    TestNothingToReadAndTies();
    TestFairness();
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: PerfectWorldAwakeningTests <path to Data/Content>\n";
        return 2;
    }
    static AwakeningWorld world;
    world.Files = ReadContentDirectory(argv[1]);
    world.Content = LoadContent(world.Files);
    Check(world.Content.Ok(), "the real Data/Content validates");
    const AbilityRecipeLoad recipes = LoadAbilityRecipes(world.Content.Db);
    Check(recipes.Ok(), "the recipes load");
    world.Recipes = recipes.Set;
    const CreationPreset* preset = CharacterCreator::FindPreset(DefaultPresetId);
    Check(preset != nullptr, "the default preset exists");
    const LatentStatsResult stats = ComputeStartingStats(world.Content.Db, preset->Ranks);
    Check(stats.Ok(), "the starting stats compute");
    world.Base = stats.Stats;
    TestWorld = &world;

    TestAwakeningRules();
    TestAwakeningPersonas();
    TestAwakeningProfile();
    std::cout << "PASS: " << AwakeningTestChecks << " checks\n";
    return 0;
}
