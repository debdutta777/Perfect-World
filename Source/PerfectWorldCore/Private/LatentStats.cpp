#include "LatentStats.h"

#include <algorithm>
#include <limits>

namespace PerfectWorld
{
namespace
{
constexpr std::array<std::string_view, AttributeCount> LatentAttributeIds{
    "attribute.body", "attribute.agility", "attribute.intelligence",
    "attribute.perception", "attribute.willpower", "attribute.charisma"};

std::int64_t LatentIntField(const ContentRecord& record, std::string_view name)
{
    const JsonValue* value = record.Data.Find(name);
    return value != nullptr && value->Integer ? *value->Integer : 0;
}

/** The value a record's seeds give for the ranks: each seed names a trait and four values for ranks 1 to 4. */
std::int64_t LatentSeedSum(const ContentRecord& record, std::string_view valuesField, const TraitRankArray& ranks)
{
    const JsonValue* seeds = record.Data.Find("seeds");
    if (seeds == nullptr || !seeds->IsArray())
    {
        return 0;
    }
    const auto& traits = CharacterCreator::TraitDefinitions();
    std::int64_t sum = 0;
    for (const JsonValue& seed : seeds->Array)
    {
        const JsonValue* trait = seed.Find("trait");
        const JsonValue* values = seed.Find(valuesField);
        if (trait == nullptr || !trait->IsString() || values == nullptr || !values->IsArray())
        {
            continue;
        }
        const auto found = std::find_if(traits.begin(), traits.end(), [&](const TraitDefinition& t) { return t.Id == trait->String; });
        if (found == traits.end())
        {
            continue;
        }
        const std::int32_t rank = ranks[static_cast<std::size_t>(found - traits.begin())];
        if (rank < 1 || static_cast<std::size_t>(rank) > values->Array.size())
        {
            continue; // rank 0 is the ordinary baseline: no effect
        }
        const std::optional<std::int64_t>& value = values->Array[static_cast<std::size_t>(rank) - 1].Integer;
        sum += value ? *value : 0;
    }
    return sum;
}
} // namespace

std::string_view AttributeContentId(Attribute attribute) noexcept
{
    const auto index = static_cast<std::size_t>(attribute);
    return index < AttributeCount ? LatentAttributeIds[index] : std::string_view();
}

const SkillLevel* LatentStats::FindSkill(std::string_view skillId) const noexcept
{
    const auto found = std::lower_bound(Skills.begin(), Skills.end(), skillId,
                                        [](const SkillLevel& skill, std::string_view id) { return skill.SkillId < id; });
    return found != Skills.end() && found->SkillId == skillId ? &*found : nullptr;
}

LatentStatsResult ComputeStartingStats(const ContentDb& content, const TraitRankArray& traitRanks)
{
    LatentStatsResult result;
    for (const std::int32_t rank : traitRanks)
    {
        if (rank < 0 || rank > MaxRank)
        {
            result.Error = "Trait ranks must be between 0 and 4.";
            return result;
        }
    }

    for (std::size_t i = 0; i < AttributeCount; ++i)
    {
        const ContentRecord* record = content.Find(LatentAttributeIds[i]);
        if (record == nullptr || record->Type != "attribute")
        {
            result.Error = "The content has no attribute record " + std::string(LatentAttributeIds[i]) + ".";
            result.Stats = {};
            return result;
        }
        const std::int64_t value = LatentIntField(*record, "startMilli") + LatentSeedSum(*record, "milliByRank", traitRanks);
        const std::int64_t ceiling = std::min<std::int64_t>(LatentIntField(*record, "maxMilli"), std::numeric_limits<std::int32_t>::max());
        result.Stats.Attributes[i] = static_cast<std::int32_t>(std::clamp<std::int64_t>(value, 0, ceiling));
    }

    // content.OfType walks an ordered map, so the skills come out sorted by ID.
    for (const ContentRecord* record : content.OfType("skill"))
    {
        const std::int64_t xp = LatentIntField(*record, "baselineXp") + LatentSeedSum(*record, "xpByRank", traitRanks);
        const std::int64_t milli = std::clamp<std::int64_t>(xp * MilliPerXp, 0, std::numeric_limits<std::int32_t>::max());
        result.Stats.Skills.push_back({record->Id, static_cast<std::int32_t>(milli)});
    }
    return result;
}

LatentStatsResult ComputeStartingStats(const ContentDb& content, const CharacterCreationSave& creation)
{
    return ComputeStartingStats(content, creation.TraitRanks);
}
} // namespace PerfectWorld
