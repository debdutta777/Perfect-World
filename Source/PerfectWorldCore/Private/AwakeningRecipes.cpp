#include "Awakening.h"

#include <algorithm>

namespace PerfectWorld
{
namespace
{
std::string AwRecipeString(const JsonValue& object, std::string_view name)
{
    const JsonValue* value = object.Find(name);
    return value != nullptr && value->IsString() ? value->String : std::string();
}

/** Reads one validated record; returns the reason it cannot be read, or empty. */
std::string AwReadRecipe(const ContentRecord& record, AbilityRecipe& recipe)
{
    recipe.Id = record.Id;
    recipe.Name = AwRecipeString(record.Data, "name");
    recipe.Effect = AwRecipeString(record.Data, "effect");
    const std::optional<Tattva> tattva = ParseAwakeningTattva(AwRecipeString(record.Data, "tattva"));
    if (!tattva)
        return record.Id + " names no Tattva";
    recipe.Affinity = *tattva;
    const JsonValue* fallback = record.Data.Find("fallback");
    recipe.Fallback = fallback != nullptr && fallback->Type == JsonValue::Kind::Bool && fallback->Bool;
    const JsonValue* needs = record.Data.Find("requires");
    if (needs == nullptr || !needs->IsArray())
        return record.Id + " has no list of requirements";
    for (const JsonValue& entry : needs->Array)
    {
        const std::optional<AwakeningSignal> signal = ParseAwakeningSignal(AwRecipeString(entry, "signal"));
        const JsonValue* min = entry.Find("min");
        if (!signal || min == nullptr || !min->Integer || *min->Integer < 1 || *min->Integer > AwakeningReadingMax)
            return record.Id + " has a requirement that is not a signal with a minimum of 1 to 1000";
        recipe.Requires.push_back({*signal, static_cast<std::int32_t>(*min->Integer)});
    }
    if (recipe.Fallback != recipe.Requires.empty())
        return record.Id + (recipe.Fallback ? " is a fallback but asks for evidence" : " is an evidence recipe that asks for none");
    return {};
}
} // namespace

const AbilityRecipe* AbilityRecipeSet::Find(std::string_view id) const
{
    const auto found = std::lower_bound(Recipes.begin(), Recipes.end(), id,
                                        [](const AbilityRecipe& recipe, std::string_view key) { return recipe.Id < key; });
    return found != Recipes.end() && found->Id == id ? &*found : nullptr;
}

const AbilityRecipe* AbilityRecipeSet::FallbackFor(Tattva tattva) const
{
    for (const AbilityRecipe& recipe : Recipes)
    {
        if (recipe.Fallback && recipe.Affinity == tattva)
            return &recipe;
    }
    return nullptr;
}

AbilityRecipeLoad LoadAbilityRecipes(const ContentDb& content)
{
    AbilityRecipeLoad load;
    for (const ContentRecord* record : content.OfType("ability_recipe"))
    {
        AbilityRecipe recipe;
        const std::string error = AwReadRecipe(*record, recipe);
        if (!error.empty())
        {
            load.Error = error;
            load.Set.Recipes.clear();
            return load;
        }
        load.Set.Recipes.push_back(std::move(recipe));
    }
    std::sort(load.Set.Recipes.begin(), load.Set.Recipes.end(),
              [](const AbilityRecipe& a, const AbilityRecipe& b) { return a.Id < b.Id; });
    return load;
}
} // namespace PerfectWorld
