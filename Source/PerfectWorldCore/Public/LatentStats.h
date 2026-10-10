#pragma once

#include "CharacterCreation.h"
#include "ContentRegistry.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * Latent stats: six attributes and a catalogue of skills, with the starting
 * values a new character gets from creation (ROADMAP M2-02; SYSTEMS_CATALOGUE
 * "Attributes, skills and latent growth"; SCHOOL_AND_PROGRESSION "Latent stat
 * progression").
 *
 * The catalogue is content, not code: Data/Content/attribute/*.json and
 * Data/Content/skill/*.json, checked by the one content validator. This module
 * reads those records and turns a set of trait ranks into starting values. It
 * is a pure function with integer maths only; it is not yet a member of
 * WorldState, and the later growth steps (M2-03 activities, M2-04 the practice
 * window) will build the persisted StatsState around it.
 *
 * Units. Everything is Milli (x1000). An attribute is Milli on a scale whose
 * ceiling is the attribute's own maxMilli. A skill's level is LevelMilli,
 * accumulated experience in milli-XP: the creator documents trait packages in
 * whole XP ("+10 / +25 / +45 / +70 starting XP"), and one XP is MilliPerXp
 * milli-XP, so 25 XP is a LevelMilli of 25,000. (The catalogue names the field
 * LevelMilli without fixing how it relates to XP; this is the chosen reading.)
 *
 * Fairness. Starting values depend on exactly one input: the twenty trait
 * ranks. Religion, language, family background, school stream, appearance,
 * pronouns, name, date of birth and household template are not inputs, so none
 * can change a result (SCHOOL_AND_PROGRESSION: "Religion, language, family
 * background and stream must never carry innate stat multipliers"). Household
 * Resources is a trait, but the documents give it no effect on any attribute or
 * skill (only a household reserve and which home is built), so it seeds none.
 * The validator refuses any content field that would multiply or modify by
 * those dimensions.
 */

constexpr std::size_t AttributeCount = 6;
constexpr std::int32_t MilliPerXp = 1000;

/** The six attributes, in the order the catalogue lists them. */
enum class Attribute : std::uint8_t
{
    Body,
    Agility,
    Intelligence,
    Perception,
    Willpower,
    Charisma
};

/** The content record ID of an attribute, for example "attribute.body". Empty for a value outside the enum. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view AttributeContentId(Attribute attribute) noexcept;

struct SkillLevel
{
    std::string SkillId;
    std::int32_t LevelMilli = 0;

    friend bool operator==(const SkillLevel&, const SkillLevel&) = default;
};

struct LatentStats
{
    /** Indexed by Attribute. Milli, between 0 and the attribute's maxMilli. */
    std::array<std::int32_t, AttributeCount> Attributes{};
    /** One entry for every skill in the catalogue, sorted by SkillId. */
    std::vector<SkillLevel> Skills;

    [[nodiscard]] std::int32_t AttributeMilli(Attribute attribute) const noexcept
    {
        return Attributes[static_cast<std::size_t>(attribute)];
    }

    /** The skill's entry, or nullptr when the catalogue has no such skill. */
    [[nodiscard]] PERFECTWORLDCORE_API const SkillLevel* FindSkill(std::string_view skillId) const noexcept;

    friend bool operator==(const LatentStats&, const LatentStats&) = default;
};

struct LatentStatsResult
{
    /** Empty on success; otherwise why no stats could be computed. */
    std::string Error;
    LatentStats Stats;

    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

using TraitRankArray = std::array<std::int32_t, TraitCount>;

/**
 * The starting attributes and skill levels for a set of trait ranks, indexed in
 * CharacterCreator::TraitDefinitions() order. Refuses (never coerces) a rank
 * outside 0 to 4, and content that lacks one of the six attributes.
 */
[[nodiscard]] PERFECTWORLDCORE_API LatentStatsResult ComputeStartingStats(const ContentDb& content,
                                                                          const TraitRankArray& traitRanks);

/** The same, reading only the trait ranks of a creation record. Every other field is ignored by design. */
[[nodiscard]] PERFECTWORLDCORE_API LatentStatsResult ComputeStartingStats(const ContentDb& content,
                                                                          const CharacterCreationSave& creation);
} // namespace PerfectWorld
