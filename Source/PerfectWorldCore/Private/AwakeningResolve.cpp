#include "Awakening.h"

#include "AwakeningInternal.h"

#include <algorithm>

namespace PerfectWorld
{
namespace
{
using AwakeningDetail::SignalSet;
using AwakeningDetail::SignalWeights;

// ---- The documented mappings (Awakening.h points 2 and 3) ---------------------------------------
// Columns are the thirteen signals in AwakeningSignal order:
//   Study Craft Bodywork People Order Making Resolve Senses | Care Steadfastness Restraint Resourcefulness Attentiveness
// Rows are the Tattvas in Tattva order. Structure (Prithvi) holds and bears; adaptation (Jala) flows between
// people and means; transformation (Agni) works material; motion (Vayu) moves bodies, goods and plans;
// space and perception (Akasha) reads. GAME_PLAN.md section 2 names those five themes and nothing finer.
constexpr std::array<SignalWeights, TattvaCount> AwTattvaWeights = {{
    {0, 1, 2, 0, 2, 0, 3, 0, 1, 3, 1, 0, 0}, // Prithvi
    {0, 1, 0, 3, 1, 1, 0, 1, 3, 0, 1, 1, 0}, // Jala
    {1, 3, 0, 0, 0, 3, 0, 0, 0, 1, 0, 2, 1}, // Agni
    {0, 0, 3, 1, 3, 0, 0, 0, 0, 0, 0, 3, 1}, // Vayu
    {3, 0, 0, 0, 0, 1, 0, 3, 0, 0, 2, 0, 3}, // Akasha
}};

// Rows are the six power dimensions in PowerDimension order (Output, Precision, Range, Duration, Perception,
// Coordination). The latent columns give the latent term; the conduct columns decide where the fixed conduct
// budget goes. AWAKENED_GRADES.md names the six dimensions and says they train separately; PATHS_AND_PROGRESSION
// "The power problem" A describes them (force, millimetre control, reach, sustained output, sensing, joint work).
constexpr std::array<SignalWeights, PowerDimensionCount> AwCapabilityWeights = {{
    {0, 1, 3, 0, 0, 0, 1, 0, 0, 2, 0, 1, 0}, // Output
    {1, 2, 0, 0, 0, 1, 0, 1, 0, 0, 2, 0, 1}, // Precision
    {0, 0, 1, 0, 2, 0, 0, 1, 1, 0, 0, 2, 0}, // Range
    {0, 0, 1, 0, 0, 0, 3, 0, 0, 2, 1, 0, 0}, // Duration
    {1, 0, 0, 0, 0, 0, 0, 3, 0, 0, 1, 0, 2}, // Perception
    {0, 0, 0, 3, 1, 0, 1, 0, 2, 0, 0, 1, 0}, // Coordination
}};

std::int64_t AwScore(const SignalWeights& weights, const SignalSet& signals, std::size_t first, std::size_t last)
{
    std::int64_t score = 0;
    for (std::size_t s = first; s < last; ++s)
        score += static_cast<std::int64_t>(weights[s]) * signals.Reading[s];
    return score;
}

/** Five scores plus a prior become shares of 1000 (exactly). */
std::vector<std::int32_t> AwShares(const std::array<std::int64_t, TattvaCount>& scores, std::int32_t prior, std::uint64_t seed,
                                   std::string_view what)
{
    std::vector<std::int64_t> weights(scores.begin(), scores.end());
    for (std::int64_t& weight : weights)
        weight += prior;
    return AwakeningDetail::Apportion(weights, 1000, seed, what);
}

void AwResolveTattvas(const SignalSet& signals, bool conductObserved, std::uint64_t seed, AwakeningResult& result)
{
    std::array<std::int64_t, TattvaCount> latentScores{};
    std::array<std::int64_t, TattvaCount> conductScores{};
    for (std::size_t t = 0; t < TattvaCount; ++t)
    {
        latentScores[t] = AwScore(AwTattvaWeights[t], signals, 0, AwakeningLatentSignalCount);
        conductScores[t] = AwScore(AwTattvaWeights[t], signals, AwakeningLatentSignalCount, AwakeningSignalCount);
    }
    const std::vector<std::int32_t> latent = AwShares(latentScores, AwakeningLatentPrior, seed, "tattva.latent");
    std::vector<std::int32_t> conduct(TattvaCount, 0);
    if (conductObserved)
        conduct = AwShares(conductScores, AwakeningConductPrior, seed, "tattva.conduct");

    // With no conduct the sixteen years speak for the whole distribution: nothing is added or taken away.
    const std::int32_t latentPercent = conductObserved ? AwakeningLatentSharePercent : 100;
    const std::int32_t conductPercent = conductObserved ? AwakeningConductSharePercent : 0;
    std::vector<std::int64_t> mixed(TattvaCount, 0);
    for (std::size_t t = 0; t < TattvaCount; ++t)
        mixed[t] = static_cast<std::int64_t>(latent[t]) * latentPercent + static_cast<std::int64_t>(conduct[t]) * conductPercent;
    const std::int32_t floorTotal = AwakeningTattvaFloorMilli * static_cast<std::int32_t>(TattvaCount);
    const std::vector<std::int32_t> spread =
        AwakeningDetail::Apportion(mixed, AwakeningTattvaTotalMilli - floorTotal, seed, "tattva.final");
    std::vector<std::int64_t> milli(TattvaCount, 0);
    for (std::size_t t = 0; t < TattvaCount; ++t)
    {
        result.TattvaMilli[t] = AwakeningTattvaFloorMilli + spread[t];
        milli[t] = result.TattvaMilli[t];
        result.TattvaCauses[t] = AwakeningDetail::TopCauses(signals, AwTattvaWeights[t]);
    }
    const std::vector<std::int32_t> percent = AwakeningDetail::Apportion(milli, 100, seed, "tattva.percent");
    for (std::size_t t = 0; t < TattvaCount; ++t)
        result.TattvaPercent[t] = percent[t];
}

void AwResolveCapability(const SignalSet& signals, std::uint64_t seed, AwakeningResult& result)
{
    for (std::size_t d = 0; d < PowerDimensionCount; ++d)
    {
        const SignalWeights& weights = AwCapabilityWeights[d];
        std::int64_t weightSum = 0;
        for (std::size_t s = 0; s < AwakeningLatentSignalCount; ++s)
            weightSum += weights[s];
        const std::int64_t latentMilli = weightSum > 0 ? AwScore(weights, signals, 0, AwakeningLatentSignalCount) / weightSum : 0;
        result.LatentRating[d] = static_cast<std::int32_t>(latentMilli * AwakeningLatentCapabilityMaxPoints / AwakeningReadingMax);
        result.RatingCauses[d] = AwakeningDetail::TopCauses(signals, weights);
    }

    // The conduct terms: one fixed budget, the floor to each dimension and the remainder in proportion to where
    // the conduct pointed (an even split when it pointed nowhere, as for a student with no conduct recorded).
    std::vector<std::int64_t> pull(PowerDimensionCount, 0);
    for (std::size_t d = 0; d < PowerDimensionCount; ++d)
        pull[d] = AwScore(AwCapabilityWeights[d], signals, AwakeningLatentSignalCount, AwakeningSignalCount) + AwakeningCapabilityPrior;
    const std::vector<std::int32_t> spread = AwakeningDetail::Apportion(pull, AwakeningConductRemainderPoints, seed, "capability.conduct");
    for (std::size_t d = 0; d < PowerDimensionCount; ++d)
    {
        result.ConductRating[d] = AwakeningConductFloorPoints + spread[d];
        result.Rating[d] = result.LatentRating[d] + result.ConductRating[d];
    }
}

/** The mean reading of what a recipe requires (zero for a fallback), and whether every requirement is met. */
bool AwRecipeMet(const AbilityRecipe& recipe, const SignalSet& signals, std::int32_t& meanReading)
{
    std::int64_t sum = 0;
    for (const AbilityRequirement& need : recipe.Requires)
    {
        const std::int32_t reading = signals.Reading[static_cast<std::size_t>(need.Signal)];
        if (reading < need.MinReading)
            return false;
        sum += reading;
    }
    meanReading = recipe.Requires.empty() ? 0 : static_cast<std::int32_t>(sum / static_cast<std::int64_t>(recipe.Requires.size()));
    return true;
}

struct AwCandidate
{
    const AbilityRecipe* Recipe = nullptr;
    std::int32_t Score = 0;
};

void AwResolveAbility(const SignalSet& signals, const AbilityRecipeSet& recipes, std::uint64_t seed, AwakeningResult& result)
{
    // rank 0 is the leading Tattva; a tie shares the rank.
    std::array<std::int32_t, TattvaCount> rank{};
    for (std::size_t t = 0; t < TattvaCount; ++t)
    {
        for (std::size_t other = 0; other < TattvaCount; ++other)
            rank[t] += result.TattvaMilli[other] > result.TattvaMilli[t] ? 1 : 0;
    }

    std::vector<AwCandidate> candidates;
    for (const AbilityRecipe& recipe : recipes.Recipes)
    {
        const std::size_t tattva = static_cast<std::size_t>(recipe.Affinity);
        std::int32_t mean = 0;
        if (recipe.Fallback || tattva >= TattvaCount || rank[tattva] >= AwakeningAbilityTattvaRanks || !AwRecipeMet(recipe, signals, mean))
            continue;
        candidates.push_back({&recipe, result.TattvaMilli[tattva] / 100 + mean});
    }
    bool fallback = false;
    if (candidates.empty())
    {
        fallback = true;
        for (const AbilityRecipe& recipe : recipes.Recipes)
        {
            const std::size_t tattva = static_cast<std::size_t>(recipe.Affinity);
            if (recipe.Fallback && tattva < TattvaCount && rank[tattva] == 0)
                candidates.push_back({&recipe, result.TattvaMilli[tattva] / 100});
        }
    }
    if (candidates.empty())
    {
        result.Error = "the ability recipes have no fallback for the leading Tattva";
        return;
    }

    // Highest score; equal scores go to the higher keyed draw (never list order).
    const auto better = [&](const AwCandidate& a, const AwCandidate& b) {
        if (a.Score != b.Score)
            return a.Score > b.Score;
        const std::uint64_t drawA = AwakeningDetail::TieDraw(seed, "ability:" + a.Recipe->Id);
        const std::uint64_t drawB = AwakeningDetail::TieDraw(seed, "ability:" + b.Recipe->Id);
        if (drawA != drawB)
            return drawA > drawB;
        return a.Recipe->Id < b.Recipe->Id;
    };
    const AwCandidate& chosen = *std::min_element(candidates.begin(), candidates.end(), better);

    AwakenedAbilityGrant& grant = result.Ability;
    grant.RecipeId = chosen.Recipe->Id;
    grant.OriginTag = std::string(AwakeningOriginTag);
    grant.Rank = AwakeningAbilityRank;
    grant.Fallback = fallback;
    grant.Score = chosen.Score;
    if (fallback)
    {
        grant.Causes = result.TattvaCauses[static_cast<std::size_t>(chosen.Recipe->Affinity)];
        return;
    }
    SignalWeights needs{};
    for (const AbilityRequirement& need : chosen.Recipe->Requires)
        needs[static_cast<std::size_t>(need.Signal)] = 1;
    grant.Causes = AwakeningDetail::TopCauses(signals, needs);
}
} // namespace

AwakeningResult ResolveAwakening(const AwakeningInput& input, const AbilityRecipeSet& recipes)
{
    AwakeningResult result;
    const SignalSet signals = AwakeningDetail::ComputeSignals(input);
    result.Signals = signals.Reading;
    result.ConductObserved = input.Conduct.TestsRecorded > 0;
    AwResolveTattvas(signals, result.ConductObserved, input.WorldSeed, result);
    AwResolveCapability(signals, input.WorldSeed, result);
    AwResolveAbility(signals, recipes, input.WorldSeed, result);
    return result;
}
} // namespace PerfectWorld
