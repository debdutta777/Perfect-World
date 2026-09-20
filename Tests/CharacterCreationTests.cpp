#include "CharacterCreation.h"
#include "CreationSaveSerialization.h"
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
    Check(c.Review().HouseholdTemplateId == "parent_household", "reset restores default household");

    // Household templates are narrative, free, and validated by stable ID.
    std::set<std::string_view> householdIds;
    std::set<std::string_view> homeKitIds;
    for (const auto& h : c.HouseholdDefinitions()) { householdIds.insert(h.Id); homeKitIds.insert(h.HomeKitId); }
    Check(householdIds.size() == 5 && homeKitIds.size() == 5, "five distinct households and home kits");
    Check(!c.SetHouseholdTemplate("mansion"), "unknown household rejected");
    Check(c.Review().HouseholdTemplateId == "parent_household", "household rejection atomic");
    Check(c.SetHouseholdTemplate("grandparent_household").Succeeded(), "household selectable");
    Check(c.Review().Remaining == 1000, "household costs no creation points");
    Check(c.Review().HomeKitId == "cooperative_flat", "household selects its home kit");

    // Text round trip, by stable trait ID.
    Check(c.ApplyPreset("balanced_scholar").Succeeded(), "preset for serialization");
    Check(c.SetIdentity({"Ananya \"Ana\"", "Sen\\Roy", "she/her", "Kolkata", "Bengali"}).Succeeded(),
          "identity with quotes and backslashes");
    const std::string text = SerializeCreationSave(c.CaptureSave());
    Check(text.find("\"household_resources\": 1") != std::string::npos, "ranks written by stable ID");
    Check(text.find("\"householdTemplateId\": \"grandparent_household\"") != std::string::npos,
          "household written");
    CharacterCreationSave parsed;
    Check(DeserializeCreationSave(text, parsed).Succeeded(), "text round trip parses");
    CharacterCreator fromText;
    Check(fromText.RestoreSave(parsed).Succeeded(), "parsed record restores");
    Check(fromText.Review().Spent == 1000, "round trip keeps the ledger");
    Check(fromText.Identity().GivenName == "Ananya \"Ana\"", "round trip keeps escaped identity");
    Check(fromText.Identity().FamilyName == "Sen\\Roy", "round trip keeps backslashes");
    Check(fromText.Review().HouseholdTemplateId == "grandparent_household", "round trip keeps household");

    // A corrupt file must be reported, never silently reinterpreted.
    CharacterCreationSave untouched = parsed;
    CharacterCreationSave target = parsed;
    for (const char* bad : {"", "{", "not json at all", "{}", "{\"schemaVersion\": 2}",
                            "{\"schemaVersion\": \"two\", \"traitRanks\": {}}",
                            "{\"schemaVersion\": 2, \"traitRanks\": {}}",
                            "{\"schemaVersion\": 2, \"surprise\": 1}"})
    {
        Check(DeserializeCreationSave(bad, target).Error == CreationError::MalformedSaveText,
              "corrupt text rejected");
    }
    Check(target.HouseholdTemplateId == untouched.HouseholdTemplateId &&
          target.TraitRanks == untouched.TraitRanks, "failed parse leaves the target untouched");

    // Truncating anywhere must fail rather than produce a half record. The
    // record ends with a closing brace and a newline, so the only prefix that
    // is still a whole record is the one missing that final newline.
    CharacterCreationSave withoutNewline;
    Check(DeserializeCreationSave(std::string_view(text).substr(0, text.size() - 1), withoutNewline).Succeeded(),
          "a record without its trailing newline still parses");
    for (std::size_t cut = 1; cut + 1 < text.size(); ++cut)
    {
        CharacterCreationSave partial;
        if (DeserializeCreationSave(std::string_view(text).substr(0, cut), partial).Succeeded())
        {
            Check(false, "a truncated save must not parse");
        }
    }

    // A renamed or missing trait must fail loudly instead of shifting ranks.
    std::string renamed = text;
    const std::size_t at = renamed.find("\"vitality\"");
    Check(at != std::string::npos, "test fixture contains vitality");
    renamed.replace(at, 10, "\"vitalityX\"");
    Check(DeserializeCreationSave(renamed, target).Error == CreationError::MalformedSaveText,
          "renamed trait rejected");

    // An unknown household in a version 2 file fails; version 1 migrates.
    CharacterCreationSave strangeHousehold = parsed;
    strangeHousehold.HouseholdTemplateId = "penthouse";
    CharacterCreator loader;
    Check(loader.RestoreSave(strangeHousehold).Error == CreationError::InvalidSave,
          "unknown household in save rejected");
    CharacterCreationSave legacy = parsed;
    legacy.SchemaVersion = 1;
    legacy.HouseholdTemplateId = "penthouse";
    Check(loader.RestoreSave(legacy).Succeeded(), "version 1 record still loads");
    Check(loader.Review().HouseholdTemplateId == "parent_household",
          "version 1 migrates to the default household");
    legacy.SchemaVersion = 3;
    Check(loader.RestoreSave(legacy).Error == CreationError::UnsupportedSaveVersion,
          "future schema version rejected");

    // Slots are bounded and named distinctly.
    Check(SaveSlotCount == 3, "three creation slots");
    std::set<std::string> slotNames;
    for (std::int32_t slot = 0; slot < SaveSlotCount; ++slot)
    {
        Check(IsValidSaveSlot(slot), "slot in range");
        slotNames.insert(SaveSlotFileName(slot));
    }
    Check(slotNames.size() == 3, "distinct slot file names");
    Check(!IsValidSaveSlot(-1) && !IsValidSaveSlot(SaveSlotCount), "out-of-range slot rejected");
    Check(SaveSlotFileName(SaveSlotCount).empty(), "out-of-range slot has no file name");

    // The opening room comes from the household template plus the Household
    // Resources rank: five shells and five tiers, not twenty-five rooms.
    std::set<std::string_view> tierIds;
    for (const auto& t : c.HomeDressingTiers()) tierIds.insert(t.Id);
    Check(tierIds.size() == 5, "five distinct dressing tiers");
    CharacterCreator home;
    // Resources rank 0 means institutional care, not a poorer family home, so
    // a fresh creation starts in care until points are spent on the household.
    Check(home.ResolveOpeningHome().ResourceRank == 0, "default resources rank");
    Check(home.ResolveOpeningHome().GrewUpInCare, "rank 0 means raised in care");
    Check(home.ResolveOpeningHome().HomeKitId == "childrens_home_dormitory", "rank 0 builds the care shell");
    Check(home.ResolveOpeningHome().DressingTierId == "institutional_care", "rank 0 names the care tier");
    for (std::int32_t rank = 0; rank <= 4; ++rank)
    {
        Check(home.SetTraitRank("household_resources", rank).Succeeded(), "set resources rank");
        const auto plan = home.ResolveOpeningHome();
        Check(plan.ResourceRank == rank, "plan reports the resources rank");
        Check(plan.DressingTierId == home.HomeDressingTiers()[static_cast<std::size_t>(rank)].Id,
              "tier follows the resources rank");
        Check(plan.GrewUpInCare == (rank == 0), "only rank 0 is raised in care");
        Check(!plan.DressingDescription.empty() && !plan.DressingTierName.empty(), "tier is described");
    }

    // Above rank 0 the shell follows the household and the tier follows the
    // rank, and the two stay independent.
    Check(home.SetHouseholdTemplate("appointed_guardian").Succeeded(), "switch household for plan");
    Check(home.ResolveOpeningHome().HomeKitId == "staff_quarters", "shell follows household");
    Check(home.ResolveOpeningHome().ResourceRank == 4, "switching household keeps the rank");
    Check(home.SetTraitRank("household_resources", 1).Succeeded(), "lower resources rank");
    Check(home.ResolveOpeningHome().HomeKitId == "staff_quarters", "lowering rank keeps the shell");
    Check(home.ResolveOpeningHome().DressingTierId == "modest", "lowering rank changes the tier");
    Check(home.Review().HouseholdTemplateId == "appointed_guardian", "review agrees with the plan");

    // Dropping to rank 0 overrides the template, and raising it back restores
    // the player's choice rather than losing it.
    Check(home.SetTraitRank("household_resources", 0).Succeeded(), "drop to care");
    Check(home.ResolveOpeningHome().GrewUpInCare, "rank 0 overrides any template");
    Check(home.ResolveOpeningHome().HomeKitId == "childrens_home_dormitory", "care overrides the shell");
    Check(home.ResolveOpeningHome().HouseholdTemplateId == "appointed_guardian",
          "care still remembers the chosen template");
    Check(home.SetTraitRank("household_resources", 2).Succeeded(), "raise back out of care");
    Check(!home.ResolveOpeningHome().GrewUpInCare && home.ResolveOpeningHome().HomeKitId == "staff_quarters",
          "raising the rank restores the chosen household");

    // Every household and rank combination resolves to a named shell and tier.
    for (const auto& h : c.HouseholdDefinitions())
    {
        CharacterCreator each;
        Check(each.SetHouseholdTemplate(h.Id).Succeeded(), "each household selectable");
        for (std::int32_t rank = 0; rank <= 4; ++rank)
        {
            Check(each.SetTraitRank("household_resources", rank).Succeeded(), "each rank settable");
            const auto plan = each.ResolveOpeningHome();
            const std::string_view expected = rank == 0 ? std::string_view("childrens_home_dormitory") : h.HomeKitId;
            Check(plan.HomeKitId == expected && !plan.HomeKitName.empty(), "combination names a shell");
            Check(!plan.DressingTierId.empty(), "combination names a tier");
        }
    }

    // The household is locked once life begins, like every other origin field.
    CharacterCreator committed;
    Check(committed.ApplyPreset("balanced_scholar").Succeeded(), "preset for lock test");
    Check(committed.StartLife().Succeeded(), "life starts for lock test");
    Check(!committed.SetHouseholdTemplate("row_house_family"), "household locked after start");
    std::cout << "PASS: " << Checks << " checks, including 10000 deterministic edit sequences\n";
}
