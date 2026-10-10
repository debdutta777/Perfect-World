// ROADMAP M0-22: the five verified presets from Docs/CHARACTER_CREATION.md.
//
// The expected rank groups below are transcribed from the document's "Verified
// presets" section, listed by stable trait ID exactly as written there, so the
// table in CreationPresets.cpp (which is in trait order) is checked against an
// independent statement of the same data.

#include "CharacterCreation.h"

#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace PerfectWorld;

namespace
{
int Checks = 0;

void Check(bool condition, const char* message)
{
    ++Checks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

struct PresetCase
{
    const char* Id;
    const char* DisplayName;
    std::vector<const char*> Rank4;
    std::vector<const char*> Rank3;
    std::vector<const char*> Rank2;
    std::vector<const char*> Rank1;
    std::vector<const char*> Rank0;
    int RankFourCount;
};

const std::vector<PresetCase>& Cases()
{
    static const std::vector<PresetCase> cases{
        {"balanced_scholar", "Balanced Scholar", {},
         {"guardian_support", "learning_discipline", "empathy", "academic_foundation"},
         {"housing_stability", "vitality", "agility", "analytical_reasoning", "technical_aptitude",
          "adversity_resilience"},
         {"physical_presence", "personal_presentation", "household_resources", "strength", "perception",
          "creative_aptitude", "confidence", "practical_experience"},
         {"athletic_aptitude", "community_connections"}, 0},
        {"resourceful_maker", "Resourceful Maker", {"practical_experience"},
         {"technical_aptitude", "analytical_reasoning", "perception", "adversity_resilience"},
         {"guardian_support", "strength", "community_connections"},
         {"personal_presentation", "household_resources", "housing_stability", "vitality", "agility",
          "learning_discipline", "empathy", "confidence"},
         {"physical_presence", "creative_aptitude", "athletic_aptitude", "academic_foundation"}, 1},
        {"athletic_organizer", "Athletic Organizer", {"athletic_aptitude"},
         {"agility", "vitality", "confidence", "community_connections"},
         {"strength", "guardian_support", "empathy"},
         {"physical_presence", "personal_presentation", "housing_stability", "perception", "learning_discipline",
          "practical_experience", "adversity_resilience", "academic_foundation"},
         {"household_resources", "analytical_reasoning", "technical_aptitude", "creative_aptitude"}, 1},
        {"creative_connector", "Creative Connector", {},
         {"creative_aptitude", "empathy", "community_connections", "personal_presentation"},
         {"physical_presence", "guardian_support", "perception", "learning_discipline", "confidence",
          "adversity_resilience"},
         {"household_resources", "housing_stability", "vitality", "agility", "analytical_reasoning",
          "technical_aptitude", "academic_foundation", "practical_experience"},
         {"strength", "athletic_aptitude"}, 0},
        {"independent_generalist", "Independent Generalist", {},
         {"housing_stability", "vitality", "analytical_reasoning", "empathy", "adversity_resilience"},
         {"agility", "perception", "technical_aptitude", "community_connections", "practical_experience"},
         {"physical_presence", "personal_presentation", "guardian_support", "learning_discipline", "confidence",
          "academic_foundation"},
         {"household_resources", "strength", "creative_aptitude", "athletic_aptitude"}, 0},
    };
    return cases;
}

std::map<std::string, int> ExpectedRanks(const PresetCase& preset)
{
    std::map<std::string, int> ranks;
    const std::pair<int, const std::vector<const char*>*> groups[] = {
        {4, &preset.Rank4}, {3, &preset.Rank3}, {2, &preset.Rank2}, {1, &preset.Rank1}, {0, &preset.Rank0}};
    for (const auto& [rank, ids] : groups)
    {
        for (const char* id : *ids)
        {
            Check(ranks.emplace(id, rank).second, "a trait appears in only one rank group of a preset");
        }
    }
    return ranks;
}

void TestDocumentShape()
{
    // The document's own arithmetic, recomputed from the group sizes.
    for (const PresetCase& preset : Cases())
    {
        const std::map<std::string, int> ranks = ExpectedRanks(preset);
        Check(ranks.size() == TraitCount, "a documented preset names all twenty traits exactly once");
        int total = 0;
        for (const auto& [id, rank] : ranks)
        {
            total += CharacterCreator::CostForRank(rank);
        }
        Check(total == CreationBudget, "the documented rank groups total exactly 1,000 by the cost table");
    }
}

void TestPresetList()
{
    const auto& presets = CharacterCreator::Presets();
    Check(presets.size() == CreationPresetCount && presets.size() == Cases().size(), "five presets are listed");
    Check(presets.front().Id == DefaultPresetId && DefaultPresetId == "balanced_scholar",
          "the canonical default comes first");

    std::set<std::string_view> ids;
    std::set<std::string_view> names;
    for (std::size_t index = 0; index < presets.size(); ++index)
    {
        const CreationPreset& preset = presets[index];
        const PresetCase& expected = Cases()[index];
        Check(preset.Id == expected.Id, "preset IDs are stable and in document order");
        Check(preset.DisplayName == expected.DisplayName, "display names match the document headings");
        Check(!preset.Id.empty() && !preset.DisplayName.empty(), "every entry has an ID and a name");
        ids.insert(preset.Id);
        names.insert(preset.DisplayName);
        Check(CharacterCreator::FindPreset(preset.Id) == &preset, "FindPreset returns the listed entry");
    }
    Check(ids.size() == presets.size() && names.size() == presets.size(), "preset IDs and names are unique");
    Check(CharacterCreator::FindPreset("unknown") == nullptr && CharacterCreator::FindPreset("") == nullptr &&
              CharacterCreator::FindPreset("Balanced_Scholar") == nullptr,
          "FindPreset refuses unknown IDs, including a different case");
}

void TestRanksMatchDocument()
{
    const auto& definitions = CharacterCreator::TraitDefinitions();
    for (const PresetCase& expected : Cases())
    {
        const CreationPreset* preset = CharacterCreator::FindPreset(expected.Id);
        Check(preset != nullptr, "documented preset exists");
        const std::map<std::string, int> ranks = ExpectedRanks(expected);
        for (std::size_t index = 0; index < TraitCount; ++index)
        {
            const auto found = ranks.find(std::string(definitions[index].Id));
            Check(found != ranks.end(), "every catalogue trait is ranked by the documented preset");
            Check(preset->Ranks[index] == found->second, "preset rank matches the document, by trait ID");
        }
    }
}

void TestEveryPresetIsValid()
{
    for (const CreationPreset& preset : CharacterCreator::Presets())
    {
        int total = 0;
        int rankFour = 0;
        for (const std::int32_t rank : preset.Ranks)
        {
            Check(rank >= 0 && rank <= MaxRank, "preset ranks stay within 0 to 4");
            total += CharacterCreator::CostForRank(rank);
            rankFour += rank == MaxRank ? 1 : 0;
        }
        Check(total == CreationBudget, "each preset totals exactly 1,000 by the real cost table");
        Check(rankFour <= MaxRankFourTraits, "each preset respects the rank-4 cap");
    }
}

void TestApplyThenReview()
{
    for (const PresetCase& expected : Cases())
    {
        CharacterCreator creator;
        Check(creator.ApplyPreset(expected.Id).Succeeded(), "every preset applies");
        const CreationReview review = creator.Review();
        Check(review.Spent == 1000 && review.Remaining == 0, "the review shows 1,000 spent and none remaining");
        Check(review.ReadyToStart && !review.HasStarted, "a preset is ready to start");
        Check(review.RankFourCount == expected.RankFourCount, "the review shows the documented rank-4 count");
        Check(review.Allocations.size() == TraitCount, "the review lists every trait");
        int summed = 0;
        for (const TraitAllocation& allocation : review.Allocations)
        {
            summed += allocation.Cost;
            Check(allocation.Cost == CharacterCreator::CostForRank(allocation.Rank), "allocation cost follows rank");
            Check(creator.GetTraitRank(allocation.Id) == allocation.Rank, "GetTraitRank agrees with the review");
        }
        Check(summed == 1000, "allocation costs sum to 1,000");

        // A preset makes a character that can actually begin life.
        Check(creator.StartLife().Succeeded() && creator.HasStarted(), "a preset can start life");
        CharacterCreator reloaded;
        Check(reloaded.RestoreSave(creator.CaptureSave()).Succeeded() &&
                  reloaded.Review().Spent == 1000 && reloaded.HasStarted(),
              "a preset survives a save round trip");
        Check(!reloaded.ApplyPreset(expected.Id), "a preset is refused after life begins");
    }
}

void TestPresetReplacesTheDraft()
{
    CharacterCreator creator;
    // A hand-built draft, then a preset: the preset replaces it entirely.
    Check(creator.SetTraitRank("strength", 4).Succeeded() && creator.SetTraitRank("empathy", 4).Succeeded(),
          "a hand-built draft");
    Check(creator.ApplyPreset("creative_connector").Succeeded(), "a preset over a draft");
    Check(creator.GetTraitRank("strength") == 0 && creator.GetTraitRank("empathy") == 3,
          "the preset replaces the draft allocation rather than merging with it");
    Check(creator.Review().Spent == 1000, "the replaced draft totals 1,000");

    // Changing presets back and forth never adds or loses points.
    for (int round = 0; round < 3; ++round)
    {
        for (const CreationPreset& preset : CharacterCreator::Presets())
        {
            Check(creator.ApplyPreset(preset.Id).Succeeded() && creator.Review().Spent == 1000,
                  "toggling presets neither creates nor loses value");
        }
    }

    // Fully editable afterwards, within the same rules.
    Check(creator.ApplyPreset("athletic_organizer").Succeeded(), "preset before editing");
    Check(creator.SetTraitRank("analytical_reasoning", 1).Error == CreationError::BudgetExceeded,
          "a full preset cannot be raised without lowering something");
    Check(creator.SetTraitRank("strength", 1).Succeeded() && creator.Review().Spent == 965,
          "lowering strength from 2 to 1 refunds 35");
    Check(creator.SetTraitRank("analytical_reasoning", 1).Succeeded() && creator.Review().Spent == 990,
          "raising analytical reasoning from 0 to 1 costs 25");
}

void TestUnknownAndSeparation()
{
    CharacterCreator creator;
    Check(creator.ApplyPreset("resourceful_maker").Succeeded(), "preset before a refused one");
    const CreationReview before = creator.Review();
    for (const char* bad : {"", "unknown", "Balanced_Scholar", "balanced_scholar ", "independent generalist"})
    {
        const OperationResult result = creator.ApplyPreset(bad);
        Check(result.Error == CreationError::UnsupportedPreset, "an unknown preset is refused as unsupported");
    }
    const CreationReview after = creator.Review();
    bool same = before.Spent == after.Spent;
    for (std::size_t index = 0; index < TraitCount; ++index)
    {
        same = same && before.Allocations[index].Rank == after.Allocations[index].Rank;
    }
    Check(same, "a refused preset leaves the draft untouched");

    // Presets never change appearance, household template, stream or identity.
    CharacterCreator separate;
    Check(separate.SetHouseholdTemplate("extended_family").Succeeded(), "household chosen");
    const AppearanceRecord appearance = separate.Appearance();
    const SchoolChoice school = separate.School();
    const IdentityProfile identity = separate.Identity();
    for (const CreationPreset& preset : CharacterCreator::Presets())
    {
        Check(separate.ApplyPreset(preset.Id).Succeeded(), "preset applies");
        Check(separate.Household().Id == "extended_family", "a preset leaves the household template alone");
        Check(separate.School().StreamId == school.StreamId, "a preset leaves the school stream alone");
        Check(separate.Appearance().SkinTone == appearance.SkinTone,"a preset leaves appearance alone");
        Check(separate.Identity().HomeCity == identity.HomeCity, "a preset leaves identity alone");
    }
}

void TestHouseholdConsequence()
{
    // Household Resources Rank 0 means institutional care (2026-09-20). The
    // document lists household_resources at Rank 0 for Athletic Organizer and
    // Independent Generalist, and its 2026-09-20 note names only the latter;
    // the other three presets keep a family household.
    for (const PresetCase& expected : Cases())
    {
        bool documentedRankZero = false;
        for (const char* id : expected.Rank0)
        {
            documentedRankZero = documentedRankZero || std::string(id) == HouseholdResourcesTraitId;
        }
        CharacterCreator creator;
        Check(creator.ApplyPreset(expected.Id).Succeeded(), "preset for the opening home");
        Check(creator.ResolveOpeningHome().GrewUpInCare == documentedRankZero,
              "a preset grows up in care exactly when the document gives it Household Resources Rank 0");
        const std::string id = expected.Id;
        Check(documentedRankZero == (id == "athletic_organizer" || id == "independent_generalist"),
              "two presets leave Household Resources at Rank 0");
    }
}
} // namespace

int main()
{
    TestDocumentShape();
    TestPresetList();
    TestRanksMatchDocument();
    TestEveryPresetIsValid();
    TestApplyThenReview();
    TestPresetReplacesTheDraft();
    TestUnknownAndSeparation();
    TestHouseholdConsequence();
    std::cout << "PASS: " << Checks << " checks\n";
}
