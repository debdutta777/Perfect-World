// Trait presentation data, rank cost queries, atomic allocation edits, allocation
// undo/redo and edit previews.
//
// The effect table below is an independent transcription of the "Canonical trait
// catalogue" in Docs/CHARACTER_CREATION.md, so the core table in
// CreationPresentation.cpp is checked against a second statement of the same data.

#include "CharacterCreation.h"
#include "CreationPresentation.h"
#include "CreationSaveSerialization.h"

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

using Ranks = std::array<std::int32_t, TraitCount>;

struct DocEffect
{
    const char* Id;
    const char* Group;
    EffectUnit Unit;
    EffectDirection Direction;
    int Lines;
    std::array<int, 4> Values;
};

const std::vector<DocEffect>& DocTable()
{
    const auto Up = EffectDirection::Increase;
    const auto Down = EffectDirection::Decrease;
    static const std::vector<DocEffect> table{
        {"physical_presence", "appearance_presentation", EffectUnit::Percent, Up, 1, {1, 2, 3, 4}},
        {"personal_presentation", "appearance_presentation", EffectUnit::Percent, Down, 1, {1, 2, 3, 4}},
        {"household_resources", "family_resources_support", EffectUnit::Rupees, Up, 1, {500, 1000, 2000, 4000}},
        {"guardian_support", "family_resources_support", EffectUnit::Percent, Up, 1, {1, 2, 3, 4}},
        {"housing_stability", "family_resources_support", EffectUnit::Percent, Up, 1, {1, 2, 3, 4}},
        {"vitality", "physical", EffectUnit::Percent, Up, 1, {1, 2, 3, 4}},
        {"strength", "physical", EffectUnit::Percent, Up, 1, {1, 2, 3, 4}},
        {"agility", "physical", EffectUnit::Percent, Up, 1, {1, 2, 3, 4}},
        {"analytical_reasoning", "cognition", EffectUnit::Percent, Up, 1, {1, 2, 3, 4}},
        {"learning_discipline", "cognition", EffectUnit::Percent, Up, 1, {1, 2, 3, 4}},
        {"perception", "cognition", EffectUnit::Percent, Up, 1, {1, 2, 3, 4}},
        {"technical_aptitude", "talents", EffectUnit::Xp, Up, 2, {10, 25, 45, 70}},
        {"creative_aptitude", "talents", EffectUnit::Xp, Up, 2, {10, 25, 45, 70}},
        {"athletic_aptitude", "talents", EffectUnit::Xp, Up, 1, {10, 25, 45, 70}},
        {"empathy", "social", EffectUnit::Percent, Up, 1, {1, 2, 3, 4}},
        {"confidence", "social", EffectUnit::Percent, Down, 1, {1, 2, 3, 4}},
        {"community_connections", "social", EffectUnit::Leads, Up, 1, {1, 2, 3, 4}},
        {"academic_foundation", "prior_experiences", EffectUnit::Xp, Up, 1, {10, 25, 45, 70}},
        {"practical_experience", "prior_experiences", EffectUnit::Xp, Up, 2, {10, 25, 45, 70}},
        {"adversity_resilience", "prior_experiences", EffectUnit::Percent, Up, 1, {1, 2, 3, 4}},
    };
    return table;
}

Ranks RanksOf(const CharacterCreator& creator)
{
    return creator.CaptureSave().TraitRanks;
}

std::string SaveText(const CharacterCreator& creator)
{
    return SerializeCreationSave(creator.CaptureSave());
}

CharacterCreator Balanced()
{
    CharacterCreator creator;
    Check(creator.ApplyPreset("balanced_scholar").Succeeded(), "balanced scholar applies");
    return creator;
}

void TestGroupsAndEntries()
{
    const auto& groups = TraitGroups();
    Check(groups.size() == 7, "seven groups");
    std::set<std::string_view> groupIds;
    for (std::size_t index = 0; index < groups.size(); ++index)
    {
        Check(groups[index].DisplayOrder == static_cast<int>(index), "display order is the array order");
        Check(!groups[index].DisplayName.empty(), "group has a display name");
        groupIds.insert(groups[index].Id);
        Check(FindTraitGroup(groups[index].Id) == &groups[index], "group lookup by ID");
    }
    Check(groupIds.size() == 7, "group IDs are unique");
    Check(FindTraitGroup("nope") == nullptr, "unknown group is null");

    std::map<std::string_view, int> memberCount;
    const auto& definitions = CharacterCreator::TraitDefinitions();
    const auto& entries = TraitPresentations();
    for (std::size_t index = 0; index < TraitCount; ++index)
    {
        const TraitPresentationEntry& entry = entries[index];
        Check(entry.TraitId == definitions[index].Id, "presentation order matches the trait definitions");
        Check(entry.GroupId == definitions[index].Group, "group matches the trait definition");
        Check(groupIds.count(entry.GroupId) == 1, "every trait has a real group");
        Check(!entry.Description.empty(), "every trait has a background line");
        ++memberCount[entry.GroupId];
        Check(TraitPresentation(entry.TraitId) == &entry, "lookup by ID");
        for (std::size_t rank = 0; rank < RankCount; ++rank)
        {
            const TraitRankEffect& effect = entry.Ranks[rank];
            Check(effect.Rank == static_cast<int>(rank), "five rank entries in rank order");
            Check(!effect.Meaning.empty(), "every rank has a plain-language meaning");
            Check(RankEffect(entry.TraitId, static_cast<int>(rank)) == &effect, "RankEffect lookup");
        }
    }
    Check(memberCount["appearance_presentation"] == 2 && memberCount["family_resources_support"] == 3 &&
              memberCount["physical"] == 3 && memberCount["cognition"] == 3 && memberCount["talents"] == 3 &&
              memberCount["social"] == 3 && memberCount["prior_experiences"] == 3,
          "group sizes match the catalogue");
    Check(TraitPresentation("missing") == nullptr, "unknown trait is null");
    Check(RankEffect("strength", -1) == nullptr && RankEffect("strength", 5) == nullptr, "rank out of range is null");
    Check(RankEffect("missing", 1) == nullptr, "RankEffect of an unknown trait is null");
}

void TestEffectsMatchDoc()
{
    Check(DocTable().size() == TraitCount, "the transcription covers all twenty traits");
    for (const DocEffect& doc : DocTable())
    {
        const TraitPresentationEntry* entry = TraitPresentation(doc.Id);
        Check(entry != nullptr, "doc trait exists");
        Check(entry->GroupId == doc.Group, "doc group");
        const bool household = std::string_view(doc.Id) == HouseholdResourcesTraitId;
        if (!household)
        {
            Check(entry->Ranks[0].LineCount == 0, "rank 0 carries no effect and no penalty");
        }
        for (int rank = 1; rank <= 4; ++rank)
        {
            const TraitRankEffect& effect = entry->Ranks[static_cast<std::size_t>(rank)];
            Check(effect.LineCount == doc.Lines, "effect line count");
            for (int line = 0; line < effect.LineCount; ++line)
            {
                const EffectLine& stated = effect.Lines[static_cast<std::size_t>(line)];
                Check(stated.Unit == doc.Unit && stated.Direction == doc.Direction, "effect unit and direction");
                Check(stated.Value == doc.Values[static_cast<std::size_t>(rank - 1)], "effect value at R1 to R4");
                Check(!stated.Subject.empty(), "effect names what it changes");
            }
        }
    }

    // A few subjects, checked word for word against the catalogue.
    Check(RankEffect("housing_stability", 3)->Lines[0].Subject == "Sleep recovery at the starting home",
          "housing stability subject");
    Check(RankEffect("technical_aptitude", 4)->Lines[0].Subject == "Computing starting XP" &&
              RankEffect("technical_aptitude", 4)->Lines[1].Subject == "Practical Repair starting XP",
          "technical aptitude names Computing and Practical Repair");
    Check(RankEffect("community_connections", 2)->Lines[0].Value == 2 &&
              RankEffect("community_connections", 2)->Lines[0].Unit == EffectUnit::Leads,
          "two contact leads at rank 2");
    Check(RankEffect("confidence", 1)->Lines[0].Direction == EffectDirection::Decrease, "confidence lowers stress");
    Check(RankEffect("household_resources", 4)->Lines[0].Value == 4000, "rank 4 reserve is 4,000 rupees");

    const TraitRankEffect& care = *RankEffect("household_resources", 0);
    Check(care.Meaning.find("institutional care") != std::string_view::npos &&
              care.Meaning.find("children's home") != std::string_view::npos,
          "household rank 0 states institutional care");
    Check(care.LineCount == 1 && care.Lines[0].Value == 0 && care.Lines[0].Unit == EffectUnit::Rupees,
          "household rank 0 has no reserve");
    Check(RankEffect("strength", 0)->Meaning != care.Meaning, "other traits keep the ordinary baseline at rank 0");
}

void TestCosts()
{
    Check(CumulativeCostAtRank(0) == 0 && CumulativeCostAtRank(1) == 25 && CumulativeCostAtRank(2) == 60 &&
              CumulativeCostAtRank(3) == 110 && CumulativeCostAtRank(4) == 180,
          "cumulative costs");
    Check(CumulativeCostAtRank(-1) == -1 && CumulativeCostAtRank(5) == -1, "cumulative cost out of range");
    const int steps[4] = {25, 35, 50, 70};
    int total = 0;
    for (int rank = 0; rank < 4; ++rank)
    {
        const RankChange next = CostOfNextRank(rank);
        Check(next.Valid && next.Delta == steps[rank], "next-rank increments are 25, 35, 50, 70");
        total += next.Delta;
        Check(total == CumulativeCostAtRank(rank + 1), "increments sum to the cumulative cost");
    }
    Check(!CostOfNextRank(4).Valid && !CostOfNextRank(-1).Valid, "no next rank past the cap");
    for (int from = 0; from <= 4; ++from)
    {
        for (int to = 0; to <= 4; ++to)
        {
            const RankChange change = CostOfRankChange(from, to);
            Check(change.Valid && change.FromRank == from && change.ToRank == to, "valid change");
            Check(change.Delta == CumulativeCostAtRank(to) - CumulativeCostAtRank(from), "delta is the cumulative difference");
            Check(change.Delta == -CostOfRankChange(to, from).Delta, "a refund mirrors the cost");
            Check((change.Delta > 0) == (to > from) && (change.Delta == 0) == (to == from), "sign follows direction");
            for (int mid = 0; mid <= 4; ++mid)
            {
                Check(CostOfRankChange(from, mid).Delta + CostOfRankChange(mid, to).Delta == change.Delta,
                      "incremental costs add up through any middle rank");
            }
        }
    }
    Check(CostOfRankChange(0, 4).Delta == 180 && CostOfRankChange(4, 2).Delta == -120, "worked examples");
    Check(!CostOfRankChange(-1, 2).Valid && !CostOfRankChange(1, 9).Valid && CostOfRankChange(9, 9).Delta == 0,
          "out of range ranks are invalid");

    // The query agrees with what the ledger actually charges.
    CharacterCreator creator;
    Check(creator.SetTraitRank("strength", 2).Succeeded(), "rank 2");
    const int before = creator.Review().Spent;
    Check(creator.SetTraitRank("strength", 4).Succeeded(), "rank 4");
    Check(creator.Review().Spent - before == CostOfRankChange(2, 4).Delta, "ledger matches the query");
}

void TestAtomicEdits()
{
    CharacterCreator creator = Balanced();
    Check(creator.UndoDepth() == 1 && creator.CanUndo() && !creator.CanRedo(), "a preset is one undo step");

    Check(creator.ClearAllocation().Succeeded(), "clear");
    Check(RanksOf(creator) == Ranks{} && creator.Review().Spent == 0, "every trait at rank 0");
    Check(creator.UndoDepth() == 2, "clear is one undo step, not twenty");
    Check(creator.ClearAllocation().Succeeded() && creator.UndoDepth() == 2, "clearing an empty draft adds no step");

    Check(creator.Undo().Succeeded(), "undo clear");
    Check(RanksOf(creator) == CharacterCreator::FindPreset("balanced_scholar")->Ranks && creator.UndoDepth() == 1,
          "one undo undoes the whole clear");

    Check(creator.ApplyPreset("resourceful_maker").Succeeded(), "second preset");
    Check(creator.UndoDepth() == 2, "preset replacing a preset is one step");
    Check(creator.ResetAllocation().Succeeded(), "reset");
    Check(RanksOf(creator) == CharacterCreator::FindPreset(DefaultPresetId)->Ranks, "reset returns to the default preset");
    Check(creator.UndoDepth() == 3, "reset is one step");
    Check(creator.ResetAllocation().Succeeded() && creator.UndoDepth() == 3, "resetting an unchanged draft adds no step");
    Check(creator.Undo().Succeeded() &&
              RanksOf(creator) == CharacterCreator::FindPreset("resourceful_maker")->Ranks,
          "one undo undoes the reset");

    // Rank edits, including ones that change nothing or are refused.
    CharacterCreator edits = Balanced();
    const std::size_t baseline = edits.UndoDepth();
    Check(edits.SetTraitRank("strength", 1).Succeeded() && edits.UndoDepth() == baseline, "an unchanged rank adds no step");
    Check(!edits.SetTraitRank("physical_presence", 4) && edits.UndoDepth() == baseline, "a refused edit adds no step");
    Check(!edits.SetTraitRank("missing", 1) && !edits.ApplyPreset("missing") && edits.UndoDepth() == baseline,
          "unknown IDs add no step");
    Check(edits.SetTraitRank("strength", 0).Succeeded() && edits.UndoDepth() == baseline + 1, "a real edit is one step");
    Check(edits.Undo().Succeeded() && edits.CanRedo(), "undo leaves a redo");
    Check(edits.SetTraitRank("strength", 0).Succeeded() && !edits.CanRedo(), "a new edit drops redo");
}

void TestUndoRedoRoundTrip()
{
    CharacterCreator creator;
    std::vector<Ranks> history{RanksOf(creator)};
    std::uint32_t state = 12345;
    for (int attempt = 0; attempt < 30; ++attempt)
    {
        state = state * 1664525u + 1013904223u;
        const auto trait = static_cast<std::size_t>((state >> 16) % TraitCount);
        const auto rank = static_cast<int>((state >> 8) % 4);
        const bool changed = creator.SetTraitRank(CharacterCreator::TraitDefinitions()[trait].Id, rank).Succeeded() &&
                             RanksOf(creator) != history.back();
        if (changed)
        {
            history.push_back(RanksOf(creator));
        }
    }
    Check(history.size() > 8, "the sequence made real edits");
    Check(creator.UndoDepth() == history.size() - 1, "one step per real edit");

    for (std::size_t index = history.size() - 1; index > 0; --index)
    {
        Check(creator.Undo().Succeeded() && RanksOf(creator) == history[index - 1], "undo restores each earlier draft exactly");
    }
    Check(!creator.CanUndo() && creator.Undo().Error == CreationError::NothingToConfirm, "nothing left to undo");
    for (std::size_t index = 1; index < history.size(); ++index)
    {
        Check(creator.Redo().Succeeded() && RanksOf(creator) == history[index], "redo replays each draft exactly");
        Check(creator.Review().Spent <= CreationBudget, "no value is created along the way");
    }
    Check(!creator.CanRedo() && creator.Redo().Error == CreationError::NothingToConfirm, "nothing left to redo");
    Check(creator.UndoDepth() == history.size() - 1, "full round trip leaves the same history");

    // The history is bounded; the oldest steps fall off.
    CharacterCreator bounded;
    std::vector<Ranks> snaps{RanksOf(bounded)};
    for (int edit = 1; edit <= 40; ++edit)
    {
        Check(bounded.SetTraitRank("strength", edit % 2).Succeeded(), "toggle");
        snaps.push_back(RanksOf(bounded));
    }
    Check(MaxUndoDepth == 32 && bounded.UndoDepth() == 32, "undo history is bounded at 32");
    for (int step = 0; step < 32; ++step)
    {
        Check(bounded.Undo().Succeeded(), "undo within the bound");
    }
    Check(!bounded.CanUndo() && RanksOf(bounded) == snaps[8], "the oldest eight steps were dropped");
    Check(bounded.RedoDepth() == 32, "redo holds the 32 undone steps");
    for (int step = 0; step < 32; ++step)
    {
        Check(bounded.Redo().Succeeded(), "redo");
    }
    Check(RanksOf(bounded) == snaps[40], "redo returns to the latest draft");
}

void TestPreviews()
{
    CharacterCreator creator = Balanced();
    Check(creator.SetTraitRank("strength", 0).Succeeded(), "leave an edit in the history");
    const std::string saveBefore = SaveText(creator);
    const std::size_t undoBefore = creator.UndoDepth();
    const std::size_t redoBefore = creator.RedoDepth();

    for (const CreationPreset& preset : CharacterCreator::Presets())
    {
        const AllocationPreview preview = creator.PreviewPreset(preset.Id);
        Check(preview.Result.Succeeded(), "preview of a valid preset succeeds");
        Check(SaveText(creator) == saveBefore && creator.UndoDepth() == undoBefore && creator.RedoDepth() == redoBefore,
              "previewing never changes the draft or its history");

        CharacterCreator applied = creator;
        Check(applied.ApplyPreset(preset.Id).Succeeded(), "apply");
        const CreationReview review = applied.Review();
        Check(preview.Review.Spent == review.Spent && preview.Review.Remaining == review.Remaining &&
                  preview.Review.RankFourCount == review.RankFourCount &&
                  preview.Review.ReadyToStart == review.ReadyToStart && preview.Review.HasStarted == review.HasStarted &&
                  preview.Review.HouseholdTemplateId == review.HouseholdTemplateId &&
                  preview.Review.HomeKitId == review.HomeKitId && preview.Review.SchoolStreamId == review.SchoolStreamId,
              "preview review equals the applied review");
        Check(preview.Review.Allocations.size() == review.Allocations.size(), "preview lists every trait");
        for (std::size_t index = 0; index < review.Allocations.size(); ++index)
        {
            Check(preview.Review.Allocations[index].Id == review.Allocations[index].Id &&
                      preview.Review.Allocations[index].Rank == review.Allocations[index].Rank &&
                      preview.Review.Allocations[index].Cost == review.Allocations[index].Cost,
                  "preview allocation equals the applied allocation");
        }
        const OpeningHomePlan home = applied.ResolveOpeningHome();
        Check(preview.Home.GrewUpInCare == home.GrewUpInCare && preview.Home.HomeKitId == home.HomeKitId &&
                  preview.Home.DressingTierId == home.DressingTierId && preview.Home.ResourceRank == home.ResourceRank,
              "preview opening home equals the applied opening home");
        Check(preview.Review.Spent == CreationBudget && preview.Review.RankFourCount <= MaxRankFourTraits,
              "every preset previews a full budget");
    }

    const AllocationPreview care = creator.PreviewPreset("independent_generalist");
    Check(care.Home.GrewUpInCare && care.Home.HomeKitId == InstitutionalHomeKitId, "the preview warns of institutional care");
    Check(!creator.ResolveOpeningHome().GrewUpInCare, "while the real draft still has a family home");

    const AllocationPreview cleared = creator.PreviewClear();
    Check(cleared.Result.Succeeded() && cleared.Review.Spent == 0 && cleared.Home.GrewUpInCare, "clear previews zero points and care");
    CharacterCreator clearedReal = creator;
    Check(clearedReal.ClearAllocation().Succeeded() && clearedReal.Review().Spent == cleared.Review.Spent, "preview clear equals clear");
    const AllocationPreview reset = creator.PreviewReset();
    Check(reset.Result.Succeeded() && reset.Review.Spent == CreationBudget && !reset.Home.GrewUpInCare,
          "reset previews the default preset");

    const AllocationPreview unknown = creator.PreviewPreset("missing");
    Check(unknown.Result.Error == CreationError::UnsupportedPreset, "unknown preset previews as the same refusal");
    Check(unknown.Review.Spent == creator.Review().Spent, "a refused preview describes the unchanged draft");
    Check(SaveText(creator) == saveBefore, "still unchanged");
}

void TestRefusedAfterStart()
{
    CharacterCreator creator = Balanced();
    Check(creator.SetTraitRank("strength", 0).Succeeded() && creator.SetTraitRank("strength", 1).Succeeded(), "history");
    Check(creator.StartLife().Succeeded(), "start life");
    const Ranks ranks = RanksOf(creator);
    const std::string saveBefore = SaveText(creator);
    Check(!creator.CanUndo() && !creator.CanRedo(), "no undo or redo once life has started");
    Check(creator.Undo().Error == CreationError::CreationAlreadyStarted, "undo refused");
    Check(creator.Redo().Error == CreationError::CreationAlreadyStarted, "redo refused");
    Check(creator.ClearAllocation().Error == CreationError::CreationAlreadyStarted, "clear refused");
    Check(creator.ResetAllocation().Error == CreationError::CreationAlreadyStarted, "reset refused");
    Check(creator.ApplyPreset("resourceful_maker").Error == CreationError::CreationAlreadyStarted, "preset refused");
    Check(RanksOf(creator) == ranks && SaveText(creator) == saveBefore, "the confirmed origin is untouched");
    Check(creator.PreviewClear().Result.Error == CreationError::CreationAlreadyStarted &&
              creator.PreviewPreset("resourceful_maker").Result.Error == CreationError::CreationAlreadyStarted &&
              creator.PreviewReset().Result.Error == CreationError::CreationAlreadyStarted,
          "previews report the same refusal");
    Check(creator.PreviewClear().Review.Spent == CreationBudget, "and describe the unchanged origin");
}

void TestOtherDraftPartsUntouched()
{
    CharacterCreator creator = Balanced();
    Check(creator.SetHouseholdTemplate("extended_family").Succeeded(), "household");
    Check(creator.SetSchoolStream("commerce", "I like markets").Succeeded(), "stream");
    Check(creator.SetIdentity({"Ananya", "Sen", "she/her", "Kolkata", "Bengali"}).Succeeded(), "identity");
    AppearanceRecord look = AppearanceRecord::Default();
    look.SkinTone.R = 90;
    Check(creator.SetAppearance(look).Succeeded(), "appearance");
    const CharacterCreationSave expected = creator.CaptureSave();

    auto sameApartFromRanks = [&expected](const CharacterCreator& other) {
        const CharacterCreationSave now = other.CaptureSave();
        return now.HouseholdTemplateId == expected.HouseholdTemplateId && now.School.StreamId == expected.School.StreamId &&
               now.School.Reason == expected.School.Reason && now.Identity.GivenName == expected.Identity.GivenName &&
               now.Appearance.SkinTone.R == expected.Appearance.SkinTone.R && now.Resume == expected.Resume;
    };

    Check(creator.ClearAllocation().Succeeded() && sameApartFromRanks(creator), "clear leaves the rest alone");
    Check(creator.ApplyPreset("creative_connector").Succeeded() && sameApartFromRanks(creator), "preset leaves the rest alone");
    Check(creator.ResetAllocation().Succeeded() && sameApartFromRanks(creator), "reset leaves the rest alone");
    while (creator.CanUndo())
    {
        Check(creator.Undo().Succeeded() && sameApartFromRanks(creator), "undo leaves the rest alone");
    }
    while (creator.CanRedo())
    {
        Check(creator.Redo().Succeeded() && sameApartFromRanks(creator), "redo leaves the rest alone");
    }

    // Identity and household edits are not history: undo only ever rewinds ranks.
    Check(creator.SetTraitRank("strength", 0).Succeeded(), "rank edit");
    Check(creator.SetHouseholdTemplate("grandparent_household").Succeeded(), "household edit");
    Check(creator.Undo().Succeeded() && creator.GetTraitRank("strength") == 1, "undo rewinds the rank edit");
    Check(creator.CaptureSave().HouseholdTemplateId == "grandparent_household", "but not the household");
}

void TestSaveUnchanged()
{
    Check(SaveSchemaVersion == 5, "save schema version is unchanged");

    CharacterCreator fresh = Balanced();
    Check(fresh.SetTraitRank("strength", 0).Succeeded(), "edit");
    Check(fresh.SetTraitRank("strength", 1).Succeeded(), "edit back");

    CharacterCreator worked;
    Check(worked.ApplyPreset("resourceful_maker").Succeeded() && worked.ClearAllocation().Succeeded() &&
              worked.ResetAllocation().Succeeded() && worked.Undo().Succeeded() && worked.Redo().Succeeded(),
          "a draft with plenty of history");
    CharacterCreator plain;
    Check(plain.ApplyPreset("balanced_scholar").Succeeded(), "the same draft with no history");
    Check(worked.UndoDepth() != plain.UndoDepth(), "histories differ");

    const std::string expected = SaveText(plain);
    Check(SaveText(worked) == expected && SaveText(fresh) == expected,
          "saves are byte-identical for the same draft whatever the history");
    Check(SaveText(plain) == SaveText(plain), "serialisation is deterministic");

    // Loading a save starts a clean history, and Reset empties it.
    CharacterCreator loaded;
    CharacterCreationSave parsed;
    Check(DeserializeCreationSave(expected, parsed).Succeeded() && loaded.RestoreSave(parsed).Succeeded(), "load");
    Check(loaded.UndoDepth() == 0 && loaded.RedoDepth() == 0 && SaveText(loaded) == expected,
          "a loaded draft has no history and re-saves identically");
    worked.Reset();
    Check(worked.UndoDepth() == 0 && worked.RedoDepth() == 0 && !worked.CanUndo(), "Reset empties the history");
}
} // namespace

int main()
{
    TestGroupsAndEntries();
    TestEffectsMatchDoc();
    TestCosts();
    TestAtomicEdits();
    TestUndoRedoRoundTrip();
    TestPreviews();
    TestRefusedAfterStart();
    TestOtherDraftPartsUntouched();
    TestSaveUnchanged();
    std::cout << "PerfectWorldCreationPresentationTests: " << Checks << " checks passed\n";
    return 0;
}
