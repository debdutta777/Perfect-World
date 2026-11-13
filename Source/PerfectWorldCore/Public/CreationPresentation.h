#pragma once

// Structured presentation data for the trait screen: the seven groups, a
// one-line background for each of the twenty traits, and for every rank 0 to 4
// a plain-language meaning plus the effect lines exactly as
// Docs/CHARACTER_CREATION.md states them. Numbers and units are data, so the
// interface formats them and never does its own arithmetic. All strings are
// English source text; localisation happens in the Unreal layer, keyed by the
// stable IDs. Rules (costs, validation, edits) stay in CharacterCreation.h.

#include "CharacterCreation.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace PerfectWorld
{
/** How many "Strongest backgrounds" the preset preview lists. */
constexpr std::size_t StrongestTraitCount = 3;
constexpr std::size_t MaxEffectLines = 2;
constexpr std::size_t RankCount = static_cast<std::size_t>(MaxRank) + 1;

/** One of the doc's seven domains. Id matches TraitDefinition::Group; never renumber DisplayOrder. */
struct TraitGroup
{
    std::string_view Id;
    std::string_view DisplayName;
    /** 0 to 6, the order the groups are listed in the document and shown on screen. */
    std::int32_t DisplayOrder = 0;
};

/** What the Value counts. The interface decides how to write it ("+4%", "Rs 4,000", "+70 XP"). */
enum class EffectUnit
{
    /** Whole percentage points. */
    Percent,
    /** Rupees of household reserve, not player cash. */
    Rupees,
    /** Starting skill XP. */
    Xp,
    /** Neighborhood contact leads. */
    Leads
};

enum class EffectDirection
{
    /** The named quantity rises (+Value). */
    Increase,
    /** The named quantity falls (Value lower), e.g. stress gained. */
    Decrease
};

/** One stated effect at one rank, e.g. {"Sleep recovery at the starting home", Percent, Increase, 3}. */
struct EffectLine
{
    std::string_view Subject;
    EffectUnit Unit = EffectUnit::Percent;
    EffectDirection Direction = EffectDirection::Increase;
    std::int32_t Value = 0;
    /** Optional caveat in plain language; empty when there is none. */
    std::string_view Note;
};

/** A rank's plain-language meaning and its effects. Rank 0 of most traits has no lines. */
struct TraitRankEffect
{
    std::int32_t Rank = 0;
    std::string_view Meaning;
    std::int32_t LineCount = 0;
    std::array<EffectLine, MaxEffectLines> Lines{};
};

struct TraitPresentationEntry
{
    std::string_view TraitId;
    /** A TraitGroup::Id. */
    std::string_view GroupId;
    /** One line saying what the background is, in plain language. */
    std::string_view Description;
    /** Indexed by rank, 0 to MaxRank. */
    std::array<TraitRankEffect, RankCount> Ranks{};
};

/** The seven groups in display order. */
[[nodiscard]] PERFECTWORLDCORE_API const std::array<TraitGroup, TraitGroupCount>& TraitGroups() noexcept;
/** Null when the ID names no group. */
[[nodiscard]] PERFECTWORLDCORE_API const TraitGroup* FindTraitGroup(std::string_view groupId) noexcept;
/** Presentation for every trait, in CharacterCreator::TraitDefinitions() order. */
[[nodiscard]] PERFECTWORLDCORE_API const std::array<TraitPresentationEntry, TraitCount>& TraitPresentations() noexcept;
/** Null when the ID names no trait. */
[[nodiscard]] PERFECTWORLDCORE_API const TraitPresentationEntry* TraitPresentation(std::string_view traitId) noexcept;
/** Null when the trait is unknown or the rank is outside 0 to MaxRank. */
[[nodiscard]] PERFECTWORLDCORE_API const TraitRankEffect* RankEffect(std::string_view traitId, std::int32_t rank) noexcept;

/** Cumulative points a trait costs at this rank (0, 25, 60, 110, 180); -1 when the rank is out of range. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t CumulativeCostAtRank(std::int32_t rank) noexcept;

/** Moving one trait between two ranks, all from the core cost table. */
struct RankChange
{
    /** False when either rank is outside 0 to MaxRank; every other field is then 0. */
    bool Valid = false;
    std::int32_t FromRank = 0;
    std::int32_t ToRank = 0;
    std::int32_t FromCumulative = 0;
    std::int32_t ToCumulative = 0;
    /** ToCumulative - FromCumulative: positive is points to spend, negative is points refunded. */
    std::int32_t Delta = 0;
};

[[nodiscard]] PERFECTWORLDCORE_API RankChange CostOfRankChange(std::int32_t fromRank, std::int32_t toRank) noexcept;
/** Points to raise a trait one rank (the "next rank" price); Valid is false at MaxRank. */
[[nodiscard]] PERFECTWORLDCORE_API RankChange CostOfNextRank(std::int32_t fromRank) noexcept;

/**
 * Points spent in each trait group, in TraitGroups() display order, summing the
 * Cost of every allocation whose trait is known. Allocations naming no known
 * trait are ignored. For a valid review the seven totals sum to Review.Spent.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::array<std::int32_t, TraitGroupCount>
GroupAllocationTotals(const std::vector<TraitAllocation>& allocations) noexcept;

/**
 * Up to count trait IDs, best first: higher rank, then higher cost, with ties
 * kept in the order the allocations are listed (catalogue order for a review).
 * Rank 0 traits are never included, so an empty allocation gives an empty list.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<std::string_view>
StrongestTraits(const std::vector<TraitAllocation>& allocations, std::size_t count);
} // namespace PerfectWorld
