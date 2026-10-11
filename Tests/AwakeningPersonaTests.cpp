// ROADMAP M4-02: every Tattva is reachable by at least two non-combat personas.
#include "AwakeningPersonas.h"

using namespace PerfectWorld;

void TestAwakeningPersonas()
{
    std::array<int, TattvaCount> reached{};
    std::array<int, TattvaCount> evidenceAbility{};
    std::vector<std::string> names;
    for (const Persona& persona : NonCombatPersonas())
    {
        const AwakeningInput input = InputOf(persona.Evidence);
        const AptitudePathProjection* hunter = input.Projection.Find(AptitudePath::Hunter);
        Check(hunter != nullptr && hunter->Band < AptitudeBand::Plausible, "a non-combat persona's life does not point at the Hunter path");
        Check(std::find(names.begin(), names.end(), persona.Name) == names.end(), "persona names are distinct");
        names.push_back(persona.Name);

        const AwakeningResult result = ResolveAwakening(input, TestWorld->Recipes);
        if (LeadingTattva(result) != persona.Target)
        {
            std::cerr << persona.Name << " leads with " << AwakeningTattvaId(LeadingTattva(result)) << ", expected "
                      << AwakeningTattvaId(persona.Target) << '\n';
            for (std::size_t t = 0; t < TattvaCount; ++t)
                std::cerr << "  " << AwakeningTattvaId(static_cast<Tattva>(t)) << " " << result.TattvaMilli[t] << '\n';
        }
        Check(result.Ok() && LeadingTattva(result) == persona.Target, "the persona's life leads with its Tattva");
        // The persona never ties for the lead (so the seed is not what reached it).
        const std::size_t lead = static_cast<std::size_t>(persona.Target);
        for (std::size_t t = 0; t < TattvaCount; ++t)
            Check(t == lead || result.TattvaMilli[t] < result.TattvaMilli[lead], "the lead is not a tie");
        Check(result.TattvaCauses[lead].size() >= 2, "the lead names its evidence");
        // The same persona under other seeds reaches the same Tattva: reachability does not depend on luck.
        for (std::uint64_t seed = 11; seed < 31; ++seed)
            Check(LeadingTattva(AwakenLife(persona.Evidence, seed)) == persona.Target, "the seed does not decide reachability");
        ++reached[lead];
        const AbilityRecipe* granted = TestWorld->Recipes.Find(result.Ability.RecipeId);
        if (granted != nullptr && !result.Ability.Fallback && granted->Affinity == persona.Target)
            ++evidenceAbility[lead];
        // And no persona needed to fight: nothing in their conduct is a combat verb beyond evasion and improvisation.
        for (const auto& [action, stakes] : persona.Evidence.Conduct)
            Check(stakes >= ConductMinStakes && stakes <= ConductMaxStakes, "the persona's stakes are the dungeon's");
    }
    for (std::size_t t = 0; t < TattvaCount; ++t)
        Check(reached[t] >= 2, "every Tattva is reached by at least two non-combat personas");
    for (std::size_t t = 0; t < TattvaCount; ++t)
        Check(evidenceAbility[t] >= 1, "and at least one of them earns an evidence ability under it");
}
