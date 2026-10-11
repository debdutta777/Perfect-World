// Trait presentation data and cost queries. Effect numbers are transcribed from
// the "Canonical trait catalogue" in Docs/CHARACTER_CREATION.md (R1 / R2 / R3 /
// R4); the per-rank meanings and one-line backgrounds are plain-language
// source text written for the trait screen. Rank 0 is the ordinary baseline for
// nineteen traits and institutional care for Household Resources.

#include "CreationPresentation.h"

#include <algorithm>

namespace PerfectWorld
{
namespace
{
constexpr std::string_view PresBaseline = "Ordinary baseline: no starting bonus and no penalty.";

struct PresRow
{
    std::string_view Id;
    std::string_view GroupId;
    std::string_view Description;
    EffectUnit Unit;
    EffectDirection Direction;
    std::int32_t LineCount;
    std::array<std::string_view, MaxEffectLines> Subjects;
    std::array<std::string_view, MaxEffectLines> Notes;
    std::array<std::int32_t, 4> Values;
    std::array<std::string_view, 4> Meanings;
};

constexpr std::array<TraitGroup, TraitGroupCount> PresGroups{{
    {"appearance_presentation", "Appearance & presentation", 0},
    {"family_resources_support", "Family resources & support", 1},
    {"physical", "Physical", 2},
    {"cognition", "Cognition", 3},
    {"talents", "Talents", 4},
    {"social", "Social", 5},
    {"prior_experiences", "Prior experiences", 6},
}};

// Rows are in CharacterCreator::TraitDefinitions() order.
constexpr std::array<PresRow, TraitCount> PresRows{{
    {"physical_presence", "appearance_presentation", "A learned bearing that steadies first impressions.",
     EffectUnit::Percent, EffectDirection::Increase,1, {"Composure window in first-impression interactions", ""}, {"", ""}, {1, 2, 3, 4},
     {"A little more composed when meeting someone new.", "Reliably steady in first impressions.",
      "A settled, assured bearing that people notice.", "An exceptional, practiced calm in any first meeting."}},
    {"personal_presentation", "appearance_presentation", "Preparation and care taken over how you present yourself.",
     EffectUnit::Percent, EffectDirection::Decrease,1, {"Stress gained from formal school and social checks", ""}, {"", ""}, {1, 2, 3, 4},
     {"You usually arrive neat and prepared.", "Careful preparation is a habit, and formal occasions worry you less.",
      "Well prepared for almost any formal setting.", "Exceptionally well prepared; formal checks rarely rattle you."}},
    {"household_resources", "family_resources_support",
     "Where you grew up. Rank 0 means institutional care, in a children's home. Ranks 1 to 4 give a family household.",
     EffectUnit::Rupees, EffectDirection::Increase,1, {"Household reserve for authored household needs", ""},
     {"The reserve pays household needs and is not cash you carry.", ""}, {500, 1000, 2000, 4000},
     {"A modest family household with a small reserve for its needs.", "A steady family household with a reserve to draw on.",
      "A comfortable family household with a healthy reserve.", "An ample family household with a large reserve."}},
    {"guardian_support", "family_resources_support", "How much care and backing you get at home.",
     EffectUnit::Percent, EffectDirection::Increase,1, {"Stress recovery from supportive home interactions", ""}, {"", ""}, {1, 2, 3, 4},
     {"A guardian who is quietly on your side.", "A guardian you can reliably turn to.",
      "A strongly supportive home where care is a constant.", "An exceptional bedrock of care and backing at home."}},
    {"housing_stability", "family_resources_support", "How secure and settled your home is.",
     EffectUnit::Percent, EffectDirection::Increase,1, {"Sleep recovery at the starting home", ""}, {"", ""}, {1, 2, 3, 4},
     {"Home is mostly settled and predictable.", "A settled home where you sleep reasonably well.",
      "A secure home that makes rest easy.", "An exceptionally secure home where rest comes easily."}},
    {"vitality", "physical", "The health you start with.",
     EffectUnit::Percent, EffectDirection::Increase,1, {"Maximum Health at start", ""}, {"", ""}, {1, 2, 3, 4},
     {"Slightly sturdier than most.", "Healthy and seldom ill.",
      "Robust health that carries you through hard weeks.", "Exceptionally hardy."}},
    {"strength", "physical", "Raw physical force.",
     EffectUnit::Percent, EffectDirection::Increase,1, {"Carry capacity and force-task tolerance", ""}, {"", ""}, {1, 2, 3, 4},
     {"A little stronger than average.", "Practiced at lifting and hauling.",
      "Strong enough that heavy work is routine.", "Exceptional physical strength for your age."}},
    {"agility", "physical", "Quickness and endurance on your feet.",
     EffectUnit::Percent, EffectDirection::Increase,1, {"Maximum exertion stamina at start", ""}, {"", ""}, {1, 2, 3, 4},
     {"A little lighter on your feet.", "Good stamina and quick movement.",
      "Strong endurance and nimble reflexes.", "Exceptional endurance and quickness."}},
    {"analytical_reasoning", "cognition", "Working carefully through logic and problems.",
     EffectUnit::Percent, EffectDirection::Increase,1, {"Progress from logic and problem-solving activities", ""}, {"", ""}, {1, 2, 3, 4},
     {"You tend to work problems through step by step.", "Practiced at breaking problems down.",
      "A sharp, orderly mind for logic.", "An exceptional gift for logic and problem-solving."}},
    {"learning_discipline", "cognition", "Staying with a study session.",
     EffectUnit::Percent, EffectDirection::Increase,1, {"Focused-study efficiency before fatigue", ""}, {"", ""}, {1, 2, 3, 4},
     {"You can usually stay with a study session.", "Practiced at sustained study.",
      "Strong study habits; focus holds for long stretches.", "Exceptional discipline; you study deeply before tiring."}},
    {"perception", "cognition", "Noticing what others miss.",
     EffectUnit::Percent, EffectDirection::Increase,1, {"Generous timing on observation prompts", ""}, {"", ""}, {1, 2, 3, 4},
     {"You sometimes notice what others miss.", "Practiced at noticing small details.",
      "Keen eyes and ears.", "Exceptionally observant."}},
    {"technical_aptitude", "talents", "A feel for machines, circuits and code.",
     EffectUnit::Xp, EffectDirection::Increase,2, {"Computing starting XP", "Practical Repair starting XP"}, {"", ""}, {10, 25, 45, 70},
     {"A first feel for machines, circuits and code.", "Practiced with tools, devices and simple programs.",
      "A strong technical background.", "Exceptional technical grounding for your age."}},
    {"creative_aptitude", "talents", "Making things and expressing ideas.",
     EffectUnit::Xp, EffectDirection::Increase,2, {"Communication starting XP", "Creative skill starting XP"},
     {"", "Applies once the Creative skill exists."}, {10, 25, 45, 70},
     {"A noticeable creative streak.", "Practiced at making things and expressing ideas.",
      "Creative work is a strong part of your background.", "An exceptional creative background."}},
    {"athletic_aptitude", "talents", "Sport and physical training.",
     EffectUnit::Xp, EffectDirection::Increase,1, {"Fitness starting XP", ""}, {"", ""}, {10, 25, 45, 70},
     {"A bit of sport and training behind you.", "Regular practice in a sport or in training.",
      "Serious training is a strong part of your background.", "An exceptional athletic background."}},
    {"empathy", "social", "Reading how other people feel.",
     EffectUnit::Percent, EffectDirection::Increase,1, {"Generous timing on emotion-reading prompts", ""}, {"", ""}, {1, 2, 3, 4},
     {"You often sense how others feel.", "Practiced at reading people.",
      "Strongly attuned to other people's feelings.", "Exceptionally attuned to others."}},
    {"confidence", "social", "Speaking up and holding your ground.",
     EffectUnit::Percent, EffectDirection::Decrease,1, {"Stress from public speaking and assertion", ""}, {"", ""}, {1, 2, 3, 4},
     {"You can usually speak up when it counts.", "Practiced at speaking up and holding your ground.",
      "Comfortable in front of people.", "Exceptionally self-assured when speaking and standing firm."}},
    {"community_connections", "social", "People in the neighborhood who know you.",
     EffectUnit::Leads, EffectDirection::Increase,1, {"Neighborhood contact leads at the start", ""},
     {"Leads are introductions, not automatic friends.", ""}, {1, 2, 3, 4},
     {"One neighbor who will make an introduction.", "A couple of neighborhood people who know you.",
      "Known by several people around the neighborhood.", "Well known around the neighborhood."}},
    {"academic_foundation", "prior_experiences", "Schoolwork you have already put in.",
     EffectUnit::Xp, EffectDirection::Increase,1, {"Academics starting XP", ""}, {"", ""}, {10, 25, 45, 70},
     {"Some schoolwork already behind you.", "A solid base of schoolwork.",
      "A strong academic record so far.", "An exceptional academic head start."}},
    {"practical_experience", "prior_experiences", "Hands-on practice with household tasks and repairs.",
     EffectUnit::Xp, EffectDirection::Increase,2, {"Practical Repair starting XP", "Household-task familiarity starting XP"}, {"", ""},
     {10, 25, 45, 70},
     {"You have helped with household tasks and repairs.", "Practiced at fixing and managing things at home.",
      "Plenty of hands-on experience.", "Exceptional hands-on experience for your age."}},
    {"adversity_resilience", "prior_experiences", "Bouncing back after setbacks.",
     EffectUnit::Percent, EffectDirection::Increase,1, {"Recovery from noncombat setbacks after rest or support", ""}, {"", ""}, {1, 2, 3, 4},
     {"You tend to bounce back after setbacks.", "Practiced at recovering from hard patches.",
      "Strongly resilient.", "Exceptionally resilient."}},
}};

TraitRankEffect PresBuildRank(const PresRow& row, std::int32_t rank)
{
    TraitRankEffect effect;
    effect.Rank = rank;
    if (rank == 0)
    {
        if (row.Id == HouseholdResourcesTraitId)
        {
            // Not a poorer family home: a different origin. See the 2026-09-20
            // decision in Docs/CHARACTER_CREATION.md.
            effect.Meaning = "Grew up in institutional care, in a children's home. No family household and no "
                             "household reserve; the household template you pick does not apply.";
            effect.LineCount = 1;
            effect.Lines[0] = {row.Subjects[0], row.Unit, row.Direction, 0, "No family household, so no reserve."};
            return effect;
        }
        effect.Meaning = PresBaseline;
        return effect;
    }
    const std::size_t slot = static_cast<std::size_t>(rank - 1);
    effect.Meaning = row.Meanings[slot];
    effect.LineCount = row.LineCount;
    for (std::size_t line = 0; line < static_cast<std::size_t>(row.LineCount); ++line)
    {
        effect.Lines[line] = {row.Subjects[line], row.Unit, row.Direction, row.Values[slot], row.Notes[line]};
    }
    return effect;
}

std::array<TraitPresentationEntry, TraitCount> PresBuildTable()
{
    std::array<TraitPresentationEntry, TraitCount> table{};
    for (std::size_t index = 0; index < TraitCount; ++index)
    {
        const PresRow& row = PresRows[index];
        TraitPresentationEntry& entry = table[index];
        entry.TraitId = row.Id;
        entry.GroupId = row.GroupId;
        entry.Description = row.Description;
        for (std::size_t rank = 0; rank < RankCount; ++rank)
        {
            entry.Ranks[rank] = PresBuildRank(row, static_cast<std::int32_t>(rank));
        }
    }
    return table;
}
} // namespace

const std::array<TraitGroup, TraitGroupCount>& TraitGroups() noexcept
{
    return PresGroups;
}

const TraitGroup* FindTraitGroup(std::string_view groupId) noexcept
{
    const auto found = std::find_if(PresGroups.begin(), PresGroups.end(),
        [groupId](const TraitGroup& group) { return group.Id == groupId; });
    return found == PresGroups.end() ? nullptr : &*found;
}

const std::array<TraitPresentationEntry, TraitCount>& TraitPresentations() noexcept
{
    static const std::array<TraitPresentationEntry, TraitCount> table = PresBuildTable();
    return table;
}

const TraitPresentationEntry* TraitPresentation(std::string_view traitId) noexcept
{
    const auto& table = TraitPresentations();
    const auto found = std::find_if(table.begin(), table.end(),
        [traitId](const TraitPresentationEntry& entry) { return entry.TraitId == traitId; });
    return found == table.end() ? nullptr : &*found;
}

const TraitRankEffect* RankEffect(std::string_view traitId, std::int32_t rank) noexcept
{
    const TraitPresentationEntry* entry = TraitPresentation(traitId);
    if (entry == nullptr || rank < 0 || rank > MaxRank)
    {
        return nullptr;
    }
    return &entry->Ranks[static_cast<std::size_t>(rank)];
}

std::int32_t CumulativeCostAtRank(std::int32_t rank) noexcept
{
    return CharacterCreator::CostForRank(rank);
}

RankChange CostOfRankChange(std::int32_t fromRank, std::int32_t toRank) noexcept
{
    RankChange change;
    const std::int32_t fromCost = CharacterCreator::CostForRank(fromRank);
    const std::int32_t toCost = CharacterCreator::CostForRank(toRank);
    if (fromCost < 0 || toCost < 0)
    {
        return change;
    }
    change.Valid = true;
    change.FromRank = fromRank;
    change.ToRank = toRank;
    change.FromCumulative = fromCost;
    change.ToCumulative = toCost;
    change.Delta = toCost - fromCost;
    return change;
}

RankChange CostOfNextRank(std::int32_t fromRank) noexcept
{
    if (fromRank < 0 || fromRank >= MaxRank)
    {
        return {};
    }
    return CostOfRankChange(fromRank, fromRank + 1);
}
} // namespace PerfectWorld
