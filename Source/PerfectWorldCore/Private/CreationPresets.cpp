// The five verified presets from Docs/CHARACTER_CREATION.md, "Verified presets".
//
// Ranks are listed in CharacterCreator::TraitDefinitions() order:
//   physical_presence, personal_presentation, household_resources,
//   guardian_support, housing_stability,
//   vitality, strength, agility,
//   analytical_reasoning, learning_discipline, perception,
//   technical_aptitude, creative_aptitude, athletic_aptitude,
//   empathy, confidence, community_connections,
//   academic_foundation, practical_experience, adversity_resilience.
//
// The IDs of the four presets after Balanced Scholar are the document headings
// in snake_case; the document names only balanced_scholar explicitly.

#include "CharacterCreation.h"

#include <algorithm>

namespace PerfectWorld
{
namespace
{
constexpr std::array<CreationPreset, CreationPresetCount> PresetTable{{
    // 4x110 + 6x60 + 8x25 = 1,000. Rank-4 count 0.
    {"balanced_scholar", "Balanced Scholar",
     {1, 1, 1, 3, 2,
      2, 1, 2,
      2, 3, 1,
      2, 1, 0,
      3, 1, 0,
      3, 1, 2}},
    // 1x180 + 4x110 + 3x60 + 8x25 = 1,000. Rank-4 count 1.
    {"resourceful_maker", "Resourceful Maker",
     {0, 1, 1, 2, 1,
      1, 2, 1,
      3, 1, 3,
      3, 0, 0,
      1, 1, 2,
      0, 4, 3}},
    // 1x180 + 4x110 + 3x60 + 8x25 = 1,000. Rank-4 count 1.
    {"athletic_organizer", "Athletic Organizer",
     {1, 1, 0, 2, 1,
      3, 2, 3,
      0, 1, 1,
      0, 0, 4,
      2, 3, 3,
      1, 1, 1}},
    // 4x110 + 6x60 + 8x25 = 1,000. Rank-4 count 0.
    {"creative_connector", "Creative Connector",
     {2, 3, 1, 2, 1,
      1, 0, 1,
      1, 2, 2,
      1, 3, 0,
      3, 2, 3,
      1, 1, 2}},
    // 5x110 + 5x60 + 6x25 = 1,000. Rank-4 count 0. Household Resources is
    // Rank 0, which means institutional care (document note, 2026-09-20).
    {"independent_generalist", "Independent Generalist",
     {1, 1, 0, 1, 3,
      3, 0, 2,
      3, 1, 2,
      2, 0, 0,
      3, 1, 2,
      1, 2, 3}},
}};
} // namespace

const std::array<CreationPreset, CreationPresetCount>& CharacterCreator::Presets() noexcept
{
    return PresetTable;
}

const CreationPreset* CharacterCreator::FindPreset(std::string_view presetId) noexcept
{
    const auto found = std::find_if(PresetTable.begin(), PresetTable.end(),
        [presetId](const CreationPreset& preset) { return preset.Id == presetId; });
    return found == PresetTable.end() ? nullptr : &*found;
}
} // namespace PerfectWorld
