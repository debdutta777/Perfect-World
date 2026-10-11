#include "Awakening.h"

// The awakening reader (ROADMAP M1-34): AwakeningResult -> the AwakenedProgression starting profile.
// Awaken() already refuses an affinity that is not whole percent summing to 100 and clamps every
// starting rating to the F band, so a result can never promote beyond F (AWAKENED_GRADES.md section 4);
// this reader additionally holds the result to its own documented bounds before handing it over.
namespace PerfectWorld
{
std::optional<AwakenedProfile> AwakenedStartingProfile(const AwakeningResult& result, std::int64_t day)
{
    if (!result.Ok() || day < 0)
        return std::nullopt;
    std::array<std::int32_t, PowerDimensionCount> ratings{};
    for (std::size_t d = 0; d < PowerDimensionCount; ++d)
    {
        if (result.Rating[d] < 0 || result.Rating[d] > AwakeningMaxRatingPoints)
            return std::nullopt;
        ratings[d] = result.Rating[d];
    }
    return Awaken(result.TattvaPercent, ratings, day);
}
} // namespace PerfectWorld
