#include "Awakening.h"
#include "ContentSchemaBuilders.h"
#include "ContentStructureChecks.h"

#include <set>

// The ability recipe record (ROADMAP M4-02; SYSTEMS_CATALOGUE "Awakening profile and abilities": the
// ability comes from authored recipes). One file per recipe under Data/Content/ability_recipe. A recipe
// names a Tattva, says whether it is that Tattva's fallback, and lists the evidence it needs: signals
// (Awakening.h) each at or above a minimum reading. A recipe carries no number of power: affinity changes
// method, never ceiling (PATHS_AND_PROGRESSION), and what an ability does is words in `effect`.
namespace PerfectWorld::ContentSchema
{
using namespace Build;
using namespace StructureChecks;

namespace
{
std::vector<std::string> AbilitySchemaSignals()
{
    std::vector<std::string> names;
    for (std::size_t i = 0; i < AwakeningSignalCount; ++i)
        names.emplace_back(AwakeningSignalName(static_cast<AwakeningSignal>(i)));
    return names;
}

std::vector<std::string> AbilitySchemaTattvas()
{
    std::vector<std::string> names;
    for (std::size_t i = 0; i < TattvaCount; ++i)
        names.emplace_back(AwakeningTattvaId(static_cast<Tattva>(i)));
    return names;
}

const TypeSchema& AbilitySchemaRequirement()
{
    static const TypeSchema s{"ability_requirement", "", {
        OneOf("signal", true, AbilitySchemaSignals()), Int("min", true, 1, AwakeningReadingMax)}};
    return s;
}

/** True when a recipe asks for conduct: a student absent from the dungeon could never earn it. */
bool AbilitySchemaNeedsConduct(const ContentRecord& recipe)
{
    for (const JsonValue& need : ItemsOf(&recipe.Data, "requires"))
    {
        const std::optional<AwakeningSignal> signal = ParseAwakeningSignal(StrOf(&need, "signal"));
        if (signal && AwakeningSignalIsConduct(*signal))
            return true;
    }
    return false;
}

void AbilitySchemaCheckRecipe(const ContentRecord& recipe, std::vector<ContentIssue>& issues)
{
    const std::vector<JsonValue>& needs = ItemsOf(&recipe.Data, "requires");
    if (FlagOf(&recipe.Data, "fallback"))
    {
        if (!needs.empty())
            Report(issues, recipe, "a fallback recipe asks for no evidence");
        return;
    }
    if (needs.empty() || needs.size() > AwakeningRecipeMaxRequirements)
        Report(issues, recipe, "an evidence recipe needs 1 to " + std::to_string(AwakeningRecipeMaxRequirements) +
                                   " requirements, found " + std::to_string(needs.size()));
    std::set<std::string> seen;
    for (const JsonValue& need : needs)
    {
        const std::string signal = StrOf(&need, "signal");
        if (!seen.insert(signal).second)
            Report(issues, recipe, "requires the signal " + signal + " twice");
    }
}

void AbilitySchemaCheckTattva(const std::vector<const ContentRecord*>& recipes, Tattva tattva, std::vector<ContentIssue>& issues)
{
    const std::string name(AwakeningTattvaId(tattva));
    int fallbacks = 0;
    int evidence = 0;
    int unconditioned = 0;
    for (const ContentRecord* recipe : recipes)
    {
        if (StrOf(&recipe->Data, "tattva") != name)
            continue;
        if (FlagOf(&recipe->Data, "fallback"))
            ++fallbacks;
        else
        {
            ++evidence;
            unconditioned += AbilitySchemaNeedsConduct(*recipe) ? 0 : 1;
        }
    }
    const ContentRecord& anchor = *recipes.front();
    if (fallbacks != 1)
        Report(issues, anchor, "the tattva " + name + " has " + std::to_string(fallbacks) + " fallback recipes; it needs exactly 1");
    if (evidence == 0)
        Report(issues, anchor, "the tattva " + name + " has no evidence recipe");
    else if (unconditioned == 0)
        Report(issues, anchor, "the tattva " + name + " has no evidence recipe a student who never entered the dungeon can earn");
}
} // namespace

const TypeSchema& AbilityRecipeSchema()
{
    static const TypeSchema s{"ability_recipe", "ability_recipe.", Base({
        Str("name", true), OneOf("tattva", true, AbilitySchemaTattvas()), Flag("fallback", true),
        Obj("requires", true, &AbilitySchemaRequirement(), true), Str("effect", true)})};
    return s;
}

void CheckAbilitySemantics(const ContentDb& db, std::vector<ContentIssue>& issues)
{
    const std::vector<const ContentRecord*> recipes = db.OfType("ability_recipe");
    if (recipes.empty())
        return;
    for (const ContentRecord* recipe : recipes)
        AbilitySchemaCheckRecipe(*recipe, issues);
    // Every Tattva must be reachable: one fallback, and an evidence recipe that needs nothing from the dungeon.
    for (std::size_t t = 0; t < TattvaCount; ++t)
        AbilitySchemaCheckTattva(recipes, static_cast<Tattva>(t), issues);
}
} // namespace PerfectWorld::ContentSchema
