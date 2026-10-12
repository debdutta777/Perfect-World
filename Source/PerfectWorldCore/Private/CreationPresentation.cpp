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
constexpr std::string_view PresBaseline = "You bring everyday experience, with room to learn as you go.";

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
    {"physical_presence", "appearance_presentation", "Practice finding your feet when meeting someone new.",
     EffectUnit::Percent, EffectDirection::Increase,1, {"Composure window in first-impression interactions", ""}, {"", ""}, {1, 2, 3, 4},
     {"You've had a little practice introducing yourself.",
      "You've practised staying composed during introductions.",
      "You've often met new people and found ways to settle your nerves.",
      "You've had unusually varied practice staying composed through unfamiliar introductions."}},
    {"personal_presentation", "appearance_presentation", "Familiar routines for getting yourself and your clothes ready.",
     EffectUnit::Percent, EffectDirection::Decrease,1, {"Stress gained from formal school and social checks", ""}, {"", ""}, {1, 2, 3, 4},
     {"You've tried a few ways to prepare for formal occasions.",
      "You've practised getting your clothes and belongings ready ahead of time.",
      "You've built reliable preparation routines for school and social occasions.",
      "You've had unusually varied experience preparing for formal occasions."}},
    {"household_resources", "family_resources_support",
     "The household resources available where you grew up.",
     EffectUnit::Rupees, EffectDirection::Increase,1, {"Household reserve for authored household needs", ""},
     {"The reserve pays household needs and is not cash you carry.", ""}, {500, 1000, 2000, 4000},
     {"You grew up with a small household reserve set aside for household needs.",
      "You grew up with a steady household reserve to draw on for household needs.",
      "You grew up with a comfortable household reserve for planned and unexpected needs.",
      "You grew up with an ample household reserve, leaving more room for unexpected needs."}},
    {"guardian_support", "family_resources_support", "Care and encouragement from the adults who look after you.",
     EffectUnit::Percent, EffectDirection::Increase,1, {"Stress recovery from supportive home interactions", ""}, {"", ""}, {1, 2, 3, 4},
     {"You've shared small worries with an adult who looks after you.",
      "You've regularly talked things through with a trusted caregiver.",
      "You've grown up with steady encouragement and time to talk through worries.",
      "You've had unusually consistent, attentive support from the adults who look after you."}},
    {"housing_stability", "family_resources_support", "Familiar surroundings and settled routines that support rest.",
     EffectUnit::Percent, EffectDirection::Increase,1, {"Sleep recovery at the starting home", ""}, {"", ""}, {1, 2, 3, 4},
     {"You've had a familiar place and a simple routine for settling at night.",
      "You've grown used to a settled place and regular bedtime routines.",
      "You've had dependable surroundings and well-established routines for rest.",
      "You've had an unusually settled place to live, with few disruptions to rest."}},
    {"vitality", "physical", "The physical health you bring from your years growing up.",
     EffectUnit::Percent, EffectDirection::Increase,1, {"Maximum Health at start", ""}, {"", ""}, {1, 2, 3, 4},
     {"You've grown up with a little extra physical hardiness.",
      "You've generally felt physically well through your everyday routines.",
      "You've had a sustained stretch of robust physical health.",
      "You've had an unusually robust stretch of physical health while growing up."}},
    {"strength", "physical", "Strength built through lifting, carrying and other physical effort.",
     EffectUnit::Percent, EffectDirection::Increase,1, {"Carry capacity and force-task tolerance", ""}, {"", ""}, {1, 2, 3, 4},
     {"You've had a little practice lifting and carrying things.",
      "You've regularly practised lifting and carrying manageable loads.",
      "You've built strength through steady practice with physical tasks.",
      "You've had unusually sustained practice building strength with safe, manageable loads."}},
    {"agility", "physical", "Stamina built through active days and movement practice.",
     EffectUnit::Percent, EffectDirection::Increase,1, {"Maximum exertion stamina at start", ""}, {"", ""}, {1, 2, 3, 4},
     {"You've made a little time for activities that keep you moving.",
      "You've regularly practised keeping active for longer stretches.",
      "You've built stamina through a steady routine of movement.",
      "You've had unusually sustained practice building stamina through varied movement."}},
    {"analytical_reasoning", "cognition", "Practice taking problems apart and working through the pieces.",
     EffectUnit::Percent, EffectDirection::Increase,1, {"Progress from logic and problem-solving activities", ""}, {"", ""}, {1, 2, 3, 4},
     {"You've tried breaking a few problems into smaller steps.",
      "You've regularly practised working through problems step by step.",
      "You've spent plenty of time comparing explanations and checking your reasoning.",
      "You've had unusually varied practice working through problems and revisiting your reasoning."}},
    {"learning_discipline", "cognition", "Study routines you've developed through practice.",
     EffectUnit::Percent, EffectDirection::Increase,1, {"Focused-study efficiency before fatigue", ""}, {"", ""}, {1, 2, 3, 4},
     {"You've tried short study sessions with planned pauses.",
      "You've practised settling into regular, focused study sessions.",
      "You've built dependable study routines with time to pause and return.",
      "You've had unusually sustained practice refining study routines that work for you."}},
    {"perception", "cognition", "Practice pausing to notice details around you.",
     EffectUnit::Percent, EffectDirection::Increase,1, {"Generous timing on observation prompts", ""}, {"", ""}, {1, 2, 3, 4},
     {"You've spent a little time looking closely at your surroundings.",
      "You've practised checking small details in everyday situations.",
      "You've often taken time to observe carefully before drawing conclusions.",
      "You've had unusually varied practice noticing details in unfamiliar situations."}},
    {"technical_aptitude", "talents", "Early experience with repairs, devices and simple programs.",
     EffectUnit::Xp, EffectDirection::Increase,2, {"Computing starting XP", "Practical Repair starting XP"}, {"", ""}, {10, 25, 45, 70},
     {"You've tried a small repair and a little programming.",
      "You've practised simple repairs and written small programs.",
      "You've spent plenty of time repairing devices and testing your own programs.",
      "You've had unusually varied experience with repair projects and programming."}},
    {"creative_aptitude", "talents", "Practice making things and sharing ideas in your own way.",
     EffectUnit::Xp, EffectDirection::Increase,2, {"Communication starting XP", "Creative skill starting XP"},
     {"", "Applies once the Creative skill exists."}, {10, 25, 45, 70},
     {"You've tried making something and sharing the idea behind it.",
      "You've regularly practised creative work and talked about your choices.",
      "You've spent plenty of time developing creative projects and explaining your ideas.",
      "You've had unusually varied practice making creative work and sharing it with others."}},
    {"athletic_aptitude", "talents", "Experience with exercise, sport and physical training.",
     EffectUnit::Xp, EffectDirection::Increase,1, {"Fitness starting XP", ""}, {"", ""}, {10, 25, 45, 70},
     {"You've tried a little sport or planned exercise.",
      "You've regularly taken part in sport or physical training.",
      "You've made physical training a steady part of your week.",
      "You've had unusually sustained experience with structured physical training."}},
    {"empathy", "social", "Practice listening and noticing how someone might be feeling.",
     EffectUnit::Percent, EffectDirection::Increase,1, {"Generous timing on emotion-reading prompts", ""}, {"", ""}, {1, 2, 3, 4},
     {"You've practised pausing to ask how someone feels.",
      "You've regularly listened and checked your understanding of people's feelings.",
      "You've spent plenty of time listening closely during difficult conversations.",
      "You've had unusually varied practice noticing emotional cues and checking what they mean."}},
    {"confidence", "social", "Practice speaking up, sharing ideas and stating your needs.",
     EffectUnit::Percent, EffectDirection::Decrease,1, {"Stress from public speaking and assertion", ""}, {"", ""}, {1, 2, 3, 4},
     {"You've tried speaking up in a few group conversations.",
      "You've regularly practised sharing your views and stating your needs.",
      "You've often spoken to groups and practised saying where you stand.",
      "You've had unusually varied practice speaking to groups and asserting your needs."}},
    {"community_connections", "social", "Local acquaintances who can help you meet other people.",
     EffectUnit::Leads, EffectDirection::Increase,1, {"Neighborhood contact leads at the start", ""},
     {"Leads are introductions, not automatic friends.", ""}, {1, 2, 3, 4},
     {"You've met one neighbor who can help you make an introduction.",
      "You've got to know a couple of local people who can help with introductions.",
      "You've met several local people through regular neighborhood activities.",
      "You've built a wider circle of local acquaintances through varied neighborhood activities."}},
    {"academic_foundation", "prior_experiences", "Experience exploring and practising what you've studied.",
     EffectUnit::Xp, EffectDirection::Increase,1, {"Academics starting XP", ""}, {"", ""}, {10, 25, 45, 70},
     {"You've spent a little extra time revisiting schoolwork.",
      "You've regularly practised schoolwork and explored questions from lessons.",
      "You've spent plenty of time working through school topics in depth.",
      "You've had unusually varied opportunities to explore and practise school topics."}},
    {"practical_experience", "prior_experiences", "Practice caring for shared spaces and making everyday repairs.",
     EffectUnit::Xp, EffectDirection::Increase,2, {"Practical Repair starting XP", "Household-task familiarity starting XP"}, {"", ""},
     {10, 25, 45, 70},
     {"You've helped with a few everyday tasks and simple repairs.",
      "You've regularly practised everyday tasks and repairs where you live.",
      "You've spent plenty of time maintaining shared spaces and fixing everyday items.",
      "You've had unusually varied practice with everyday upkeep and repair projects."}},
    {"adversity_resilience", "prior_experiences", "Ways you've learned to recover from setbacks with rest or support.",
     EffectUnit::Percent, EffectDirection::Increase,1, {"Recovery from noncombat setbacks after rest or support", ""}, {"", ""}, {1, 2, 3, 4},
     {"You've tried giving yourself time to rest after disappointments.",
      "You've practised resting or seeking support when things go wrong.",
      "You've built familiar routines for resting and accepting support after setbacks.",
      "You've had unusually sustained support and practice finding your footing after setbacks."}},
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
            effect.Meaning = "You grew up in institutional care at a children's home, with no family household "
                             "and no reserve.";
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
