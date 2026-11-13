// Core-owned presentation data for the creator screens: trait group totals and
// strongest-trait ranking (CreationPresentation.h), and the authored school
// stream copy (StreamPresentation.h). The interface formats these and computes
// none of them.

#include "CharacterCreation.h"
#include "CreationPresentation.h"
#include "StreamPresentation.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
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

std::int32_t SumOf(const std::array<std::int32_t, TraitGroupCount>& totals)
{
    std::int32_t sum = 0;
    for (const std::int32_t total : totals)
    {
        Check(total >= 0, "no group total is negative");
        sum += total;
    }
    return sum;
}

std::vector<TraitAllocation> Listed(std::initializer_list<TraitAllocation> items)
{
    return std::vector<TraitAllocation>(items);
}

const TraitAllocation* Find(const std::vector<TraitAllocation>& allocations, std::string_view id)
{
    for (const TraitAllocation& allocation : allocations)
    {
        if (allocation.Id == id)
        {
            return &allocation;
        }
    }
    return nullptr;
}

void CheckStrongestOrdering(const std::vector<TraitAllocation>& allocations, std::size_t count)
{
    const std::vector<std::string_view> strongest = StrongestTraits(allocations, count);
    std::size_t nonZero = 0;
    for (const TraitAllocation& allocation : allocations)
    {
        nonZero += allocation.Rank > 0 ? 1 : 0;
    }
    Check(strongest.size() == std::min(count, nonZero), "strongest has min(count, non-zero traits) entries");
    std::set<std::string_view> seen;
    const TraitAllocation* previous = nullptr;
    for (const std::string_view id : strongest)
    {
        const TraitAllocation* current = Find(allocations, id);
        Check(current != nullptr && current->Rank > 0, "strongest lists only allocated traits");
        Check(seen.insert(id).second, "strongest lists each trait once");
        if (previous != nullptr)
        {
            Check(previous->Rank >= current->Rank, "strongest is ordered by rank");
            if (previous->Rank == current->Rank)
            {
                Check(previous < current, "ties keep catalogue order");
            }
        }
        previous = current;
    }
    // Nothing left out outranks the last entry kept.
    if (strongest.size() == count && previous != nullptr)
    {
        for (const TraitAllocation& allocation : allocations)
        {
            if (seen.count(allocation.Id) == 0)
            {
                Check(allocation.Rank <= previous->Rank, "no omitted trait outranks a kept one");
            }
        }
    }
}

void TestGroupTotalsForPresets()
{
    for (const CreationPreset& preset : CharacterCreator::Presets())
    {
        CharacterCreator creator;
        Check(creator.ApplyPreset(preset.Id).Succeeded(), "preset applies");
        const CreationReview review = creator.Review();
        Check(SumOf(GroupAllocationTotals(review.Allocations)) == review.Spent, "preset group totals sum to spent");
        Check(review.Spent == CreationBudget, "every preset spends the whole budget");

        const AllocationPreview preview = CharacterCreator().PreviewPreset(preset.Id);
        Check(preview.Result.Succeeded(), "preview of a preset succeeds");
        Check(SumOf(preview.GroupTotals) == preview.Review.Spent, "preview group totals sum to spent");
        Check(preview.GroupTotals == GroupAllocationTotals(preview.Review.Allocations),
              "preview totals equal the standalone query");
        Check(preview.StrongestTraits == StrongestTraits(preview.Review.Allocations, StrongestTraitCount),
              "preview strongest equals the standalone query");
        Check(!preview.StrongestTraits.empty() && preview.StrongestTraits.size() <= StrongestTraitCount,
              "every preset has one to three strongest traits");
        CheckStrongestOrdering(preview.Review.Allocations, StrongestTraitCount);
    }
}

void TestGroupTotalsAttribution()
{
    CharacterCreator creator;
    Check(creator.SetTraitRank("vitality", 2).Succeeded(), "vitality 2");
    Check(creator.SetTraitRank("household_resources", 1).Succeeded(), "resources 1");
    Check(creator.SetTraitRank("perception", 3).Succeeded(), "perception 3");
    const auto totals = GroupAllocationTotals(creator.Review().Allocations);
    Check(totals[0] == 0, "appearance group is empty");
    Check(totals[1] == CharacterCreator::CostForRank(1), "family group holds the resources cost");
    Check(totals[2] == CharacterCreator::CostForRank(2), "physical group holds the vitality cost");
    Check(totals[3] == CharacterCreator::CostForRank(3), "cognition group holds the perception cost");
    Check(SumOf(totals) == creator.Review().Spent, "attribution sums to spent");

    const auto empty = GroupAllocationTotals({});
    Check(SumOf(empty) == 0, "no allocations gives zero totals");
    const auto unknown = GroupAllocationTotals(Listed({{"not_a_trait", 2, 60}}));
    Check(SumOf(unknown) == 0, "an unknown trait is ignored");
}

void TestGroupTotalsForRandomAllocations()
{
    std::uint32_t state = 12345U;
    const auto next = [&state]() {
        state = state * 1664525U + 1013904223U;
        return state >> 8;
    };
    const auto& definitions = CharacterCreator::TraitDefinitions();
    for (int round = 0; round < 200; ++round)
    {
        CharacterCreator creator;
        for (int edit = 0; edit < 60; ++edit)
        {
            const std::size_t index = next() % definitions.size();
            const std::int32_t rank = static_cast<std::int32_t>(next() % (MaxRank + 1));
            // Refusals (budget, rank-4 limit) are fine: the draft stays valid either way.
            (void)creator.SetTraitRank(definitions[index].Id, rank);
        }
        const CreationReview review = creator.Review();
        Check(review.Spent <= CreationBudget, "a random draft stays within budget");
        Check(SumOf(GroupAllocationTotals(review.Allocations)) == review.Spent, "random group totals sum to spent");
        CheckStrongestOrdering(review.Allocations, StrongestTraitCount);
        CheckStrongestOrdering(review.Allocations, 1);
        CheckStrongestOrdering(review.Allocations, TraitCount + 5);
    }
}

void TestStrongestTraits()
{
    // Catalogue order: vitality, strength, agility, perception. Strength is the only rank 4.
    const std::vector<TraitAllocation> allocations = Listed({
        {"vitality", 2, 60},
        {"strength", 4, 180},
        {"agility", 2, 60},
        {"perception", 1, 25},
        {"empathy", 0, 0},
    });
    const std::vector<std::string_view> top = StrongestTraits(allocations, 3);
    Check(top.size() == 3 && top[0] == "strength" && top[1] == "vitality" && top[2] == "agility",
          "highest rank first, then ties in listed order");
    const std::vector<std::string_view> four = StrongestTraits(allocations, 10);
    Check(four.size() == 4 && four[3] == "perception", "rank 0 traits are excluded");
    Check(StrongestTraits(allocations, 0).empty(), "a count of zero gives none");
    Check(StrongestTraits(allocations, 1).size() == 1 && StrongestTraits(allocations, 1)[0] == "strength",
          "a count of one gives the single strongest");

    // Cost breaks a rank tie before catalogue order does.
    const std::vector<TraitAllocation> costly = Listed({{"vitality", 2, 60}, {"strength", 2, 70}});
    const std::vector<std::string_view> byCost = StrongestTraits(costly, 2);
    Check(byCost.size() == 2 && byCost[0] == "strength", "equal rank is ordered by cost");

    CharacterCreator zero;
    Check(StrongestTraits(zero.Review().Allocations, StrongestTraitCount).empty(), "zero allocation has no strongest");
    const AllocationPreview cleared = CharacterCreator().PreviewClear();
    Check(cleared.StrongestTraits.empty(), "a cleared preview has no strongest traits");
    Check(SumOf(cleared.GroupTotals) == 0, "a cleared preview has zero group totals");

    // The catalogue-order tie rule on a real preset.
    CharacterCreator balanced;
    Check(balanced.ApplyPreset("balanced_scholar").Succeeded(), "balanced scholar applies");
    CheckStrongestOrdering(balanced.Review().Allocations, StrongestTraitCount);
}

void TestStreamPresentation()
{
    const auto& streams = SchoolStreams();
    const auto& table = StreamPresentations();
    Check(table.size() == streams.size(), "one presentation per stream");
    std::set<std::string_view> summaries;
    for (std::size_t index = 0; index < streams.size(); ++index)
    {
        const StreamPresentationEntry* entry = StreamPresentation(streams[index].Id);
        Check(entry != nullptr && entry == &table[index], "presentation is found by stream ID, in stream order");
        Check(entry->StreamId == streams[index].Id, "presentation names its stream");
        Check(!entry->CardSummary.empty(), "card summary is written");
        summaries.insert(entry->CardSummary);
        Check(!entry->SampleDayCaption.empty() && !entry->ClosingCaption.empty() && !entry->RouteHeading.empty() &&
                  !entry->RouteFlexibilityText.empty() && !entry->EqualStartingXPText.empty(),
              "captions and routes copy are written");
        for (const StreamSampleRow& row : entry->SampleDay)
        {
            Check(!row.Label.empty() && !row.Description.empty(), "every sample-day row has a label and a description");
        }
        // Only class time differs between streams; the other rows are shared.
        const StreamPresentationEntry& first = table[0];
        for (std::size_t row = 0; row < StreamSampleRowCount; ++row)
        {
            Check(entry->SampleDay[row].Label == first.SampleDay[row].Label, "sample-day labels are shared");
            if (entry->SampleDay[row].Label != "Class time")
            {
                Check(entry->SampleDay[row].Description == first.SampleDay[row].Description,
                      "only the class-time activity differs");
            }
        }
        Check(entry->RouteFlexibilityText == first.RouteFlexibilityText &&
                  entry->EqualStartingXPText == first.EqualStartingXPText && entry->SampleDayCaption == first.SampleDayCaption &&
                  entry->ClosingCaption == first.ClosingCaption,
              "shared copy is identical across streams");
    }
    Check(summaries.size() == streams.size(), "card summaries differ per stream");
    Check(table[0].SampleDay[1].Description != table[1].SampleDay[1].Description &&
              table[1].SampleDay[1].Description != table[2].SampleDay[1].Description,
          "class-time activities differ per stream");

    // Spec text, verbatim.
    const StreamPresentationEntry* science = StreamPresentation("science");
    Check(science != nullptr && science->CardSummary == "Explore physical systems through models and practical investigation.",
          "science summary");
    Check(science->SampleDay[0].Label == "Assembly" && science->SampleDay[1].Label == "Class time" &&
              science->SampleDay[2].Label == "Shared work" && science->SampleDay[3].Label == "After school",
          "sample-day labels and order");
    Check(science->SampleDay[1].Description == "Model water flow and investigate a measurement.", "science class time");
    Check(StreamPresentation("commerce")->SampleDay[1].Description == "Reconcile a daybook and explain a discrepancy.",
          "commerce class time");
    Check(StreamPresentation("humanities")->SampleDay[1].Description ==
              "Compare two accounts and build a supported interpretation.",
          "humanities class time");
    Check(science->SampleDayCaption == "An illustration of school life at Nabapara Vidyalaya, not a fixed schedule.",
          "sample-day caption");
    Check(science->ClosingCaption == "At most two classes a day need foreground play. Other periods follow your timetable.",
          "closing caption");
    Check(science->RouteHeading == "Room to change direction", "routes heading");

    // No invented bell times.
    for (const StreamPresentationEntry& entry : table)
    {
        for (const StreamSampleRow& row : entry.SampleDay)
        {
            const std::string text = std::string(row.Label) + " " + std::string(row.Description);
            Check(text.find(':') == std::string::npos && text.find_first_of("0123456789") == std::string::npos,
                  "sample-day rows carry no bell times");
        }
    }

    Check(StreamPresentation("") == nullptr, "an empty stream ID is refused");
    Check(StreamPresentation("arts") == nullptr, "an unknown stream ID is refused");
    Check(StreamPresentation("Science") == nullptr, "stream IDs are exact");
}
} // namespace

int main()
{
    TestGroupTotalsForPresets();
    TestGroupTotalsAttribution();
    TestGroupTotalsForRandomAllocations();
    TestStrongestTraits();
    TestStreamPresentation();
    std::cout << "PerfectWorldStreamPresentationTests: " << Checks << " checks passed\n";
    return 0;
}
