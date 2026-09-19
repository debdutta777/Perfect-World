#include "CharacterCreation.h"
#include <cstdlib>
#include <iostream>
#include <random>
#include <set>
#include <limits>

using namespace PerfectWorld;
int Checks = 0;
void Check(bool condition, const char* message)
{
    ++Checks;
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

int main()
{
    CharacterCreator c;
    Check(c.Review().Age == 16 && c.Review().Remaining == 1000, "new creation age and budget");
    Check(!c.StartLife(), "unallocated creation cannot start");
    std::set<std::string_view> ids;
    for (const auto& t : c.TraitDefinitions()) ids.insert(t.Id);
    Check(ids.size() == 20, "unique stable trait IDs");
    Check(!c.SetTraitRank("missing", 1), "unknown ID rejected");
    Check(!c.SetTraitRank("strength", -1), "negative rank rejected");
    Check(!c.SetTraitRank("strength", std::numeric_limits<int>::max()), "huge rank rejected");
    Check(c.Review().Spent == 0, "invalid edits leave draft intact");
    Check(c.ApplyPreset("balanced_scholar").Succeeded(), "default preset valid");
    Check(c.Review().Spent == 1000 && c.Review().ReadyToStart, "preset exactly 1000");
    Check(c.SetTraitRank("physical_presence", 4).Error == CreationError::BudgetExceeded,
          "over-budget rank rejected");
    Check(c.GetTraitRank("physical_presence") == 1 && c.Review().Spent == 1000,
          "overspend is atomic");
    for (int i = 0; i < 100; ++i)
    {
        Check(c.SetTraitRank("physical_presence", 0).Succeeded(), "refund edit");
        Check(c.Review().Remaining == 25, "refund exact");
        Check(c.SetTraitRank("physical_presence", 1).Succeeded(), "reapply edit");
        Check(c.Review().Remaining == 0, "no refund farming");
    }
    Check(c.SetIdentity({"Ananya", "Sen", "she/her", "Kolkata", "Bengali"}).Succeeded(), "identity free");
    Check(c.SetCosmetics({"frame_01", "tone_07", "braid", "black", "school_blue"}).Succeeded(), "cosmetics free");
    Check(c.Review().Spent == 1000, "identity has no budget effect");
    auto saved = c.CaptureSave();
    CharacterCreator restored;
    Check(restored.RestoreSave(saved).Succeeded(), "draft save roundtrip");
    Check(restored.Identity().HomeCity == "Kolkata", "identity retained");
    Check(restored.StartLife().Succeeded(), "valid life starts");
    Check(!restored.StartLife() && !restored.SetTraitRank("strength", 0), "start cannot replay or edit");
    Check(!restored.SetIdentity({}) && !restored.SetCosmetics({}), "origin identity locked");
    Check(!restored.ApplyPreset("balanced_scholar"), "preset locked after start");
    CharacterCreator loaded;
    Check(loaded.RestoreSave(restored.CaptureSave()).Succeeded() && loaded.HasStarted(), "committed record roundtrip");
    saved.SchemaVersion = 999;
    Check(!loaded.RestoreSave(saved) && loaded.HasStarted(), "bad schema atomic");
    saved.SchemaVersion = 1; saved.Age = 17;
    Check(!loaded.RestoreSave(saved), "wrong opening age rejected");
    saved.Age = 16; saved.TraitRanks[0] = -1;
    Check(!loaded.RestoreSave(saved), "invalid saved rank rejected");
    saved = CharacterCreationSave{}; saved.HasStarted = true;
    Check(!loaded.RestoreSave(saved), "incomplete committed save rejected");
    c.Reset();
    for (int i = 0; i < 4; ++i) Check(c.SetTraitRank(c.TraitDefinitions()[i].Id, 4).Succeeded(), "four premium allowed");
    Check(c.SetTraitRank(c.TraitDefinitions()[4].Id, 4).Error == CreationError::TooManyRankFourTraits,
          "fifth premium rejected even below budget");
    Check(c.Review().Spent == 720 && c.Review().RankFourCount == 4, "premium rejection atomic");
    Check(!c.ApplyPreset("unknown") && c.Review().Spent == 720, "unknown preset atomic");
    std::mt19937 rng(20260919);
    // Exercise sequences of valid/invalid edits with an independent ledger oracle.
    for (int i = 0; i < 10000; ++i)
    {
        const int index = static_cast<int>(rng() % 20);
        const int rank = static_cast<int>(rng() % 7) - 1;
        const auto before = c.CaptureSave();
        const auto result = c.SetTraitRank(c.TraitDefinitions()[index].Id, rank);
        const auto after = c.CaptureSave();
        if (!result) Check(before.TraitRanks == after.TraitRanks, "rejected random edit atomic");
        int spent = 0, premium = 0;
        constexpr int costs[] = {0, 25, 60, 110, 180};
        for (int r : after.TraitRanks)
        {
            Check(r >= 0 && r <= 4, "rank invariant");
            spent += costs[r]; premium += r == 4;
        }
        Check(spent <= 1000 && premium <= 4, "budget and cap invariant");
        Check(c.Review().Spent == spent && c.Review().Remaining == 1000-spent, "ledger matches oracle");
    }
    c.Reset();
    Check(c.Review().Remaining == 1000 && c.Identity().GivenName.empty(), "reset creates fresh draft");
    std::cout << "PASS: " << Checks << " checks, including 10000 deterministic edit sequences\n";
}
