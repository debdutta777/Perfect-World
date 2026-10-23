#include "AwakeningInputs.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>

using namespace PerfectWorld;

namespace
{
int R91Assertions = 0;

void R91Expect(bool condition, const char* message)
{
    ++R91Assertions;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

std::vector<ContentFile> R91ReadContent(const std::filesystem::path& root)
{
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            paths.push_back(entry.path());
    std::sort(paths.begin(), paths.end());
    std::vector<ContentFile> files;
    for (const auto& path : paths)
    {
        std::ifstream input(path, std::ios::binary);
        std::ostringstream text;
        text << input.rdbuf();
        files.push_back({"/" + std::filesystem::relative(path, root).generic_string(), text.str()});
    }
    return files;
}

AwakeningState R91Seed(const ContentDb& content, const CharacterCreationSave& creation)
{
    auto seeded = SeedAwakeningInputs(content, creation, 9101);
    R91Expect(seeded.Ok() && seeded.State.Seeded, "creation seeds an authoritative latent owner");
    return seeded.State;
}

void R91Unknown(const AptitudeProjection& projection)
{
    R91Expect(projection.Paths.size() == AptitudePathCount, "unknown projection still names all ten paths");
    for (const auto& path : projection.Paths)
    {
        R91Expect(path.Band == AptitudeBand::InsufficientObservation && path.Coverage == AptitudeCoverage::Unknown &&
                      path.Unobserved.size() == AptitudeFacetsPerPath && path.Strongest.empty(),
                  "no played evidence means unknown, never invented strengths or weaknesses");
        for (const auto& facet : path.Facets)
            R91Expect(facet.State == AptitudeFacetState::Unobserved, "every unobserved facet stays unobserved");
    }
}

void R91Creation(const ContentDb& content, const AbilityRecipeSet& recipes)
{
    CharacterCreationSave creation;
    const auto baseline = R91Seed(content, creation);
    const auto baseResult = ResolveAwakening(BuildAwakeningInput(baseline), recipes);
    R91Unknown(BuildAwakeningInput(baseline).Projection);
    const auto& traits = CharacterCreator::TraitDefinitions();
    for (const std::string_view id : {"technical_aptitude", "creative_aptitude", "athletic_aptitude", "academic_foundation"})
    {
        auto varied = creation;
        const auto found = std::find_if(traits.begin(), traits.end(), [&](const auto& trait) { return trait.Id == id; });
        R91Expect(found != traits.end(), "sensitivity test names an existing trait");
        varied.TraitRanks[static_cast<std::size_t>(found - traits.begin())] = 4;
        const auto state = R91Seed(content, varied);
        const auto result = ResolveAwakening(BuildAwakeningInput(state), recipes);
        R91Expect(state.Stats != baseline.Stats && result.Signals != baseResult.Signals &&
                      result.TattvaMilli != baseResult.TattvaMilli && result.Rating != baseResult.Rating,
                  "documented skill-seeding traits change stats, signals, affinity and capability");
        R91Unknown(BuildAwakeningInput(state).Projection);
    }
    const auto resources = std::find_if(traits.begin(), traits.end(), [](const auto& trait) {
        return trait.Id == HouseholdResourcesTraitId;
    });
    R91Expect(resources != traits.end(), "resources trait exists");
    for (int rank = 0; rank <= 4; ++rank)
    {
        for (const auto& household : CharacterCreator::HouseholdDefinitions())
        {
            auto varied = creation;
            varied.TraitRanks[static_cast<std::size_t>(resources - traits.begin())] = rank;
            varied.HouseholdTemplateId = household.Id;
            varied.Appearance.FaceShapeId = FaceShapes().back().Id;
            varied.Appearance.HairStyleId = HairStyles().back().Id;
            varied.Appearance.BuildId = Builds().back().Id;
            varied.Appearance.UniformVariantId = UniformVariants().back().Id;
            varied.Appearance.SkinTone = {12, 34, 56};
            varied.Appearance.EyeColor = {100, 200, 30};
            varied.Appearance.HairColor = {255, 240, 128};
            const auto state = R91Seed(content, varied);
            R91Expect(state.Stats == baseline.Stats && ResolveAwakening(BuildAwakeningInput(state), recipes) == baseResult,
                      "all resources ranks, household templates and cosmetic changes leave awakening identical");
        }
    }
    // Fully allocated, valid presets also diverge (not just isolated rank probes).
    CharacterCreator scholar;
    CharacterCreator maker;
    R91Expect(scholar.ApplyPreset("balanced_scholar").Succeeded() && maker.ApplyPreset("resourceful_maker").Succeeded(),
              "two complete 1000-point creations are valid");
    R91Expect(ResolveAwakening(BuildAwakeningInput(R91Seed(content, scholar.CaptureSave())), recipes) !=
                  ResolveAwakening(BuildAwakeningInput(R91Seed(content, maker.CaptureSave())), recipes),
              "complete creations produce different awakening results");
    auto invalid = creation;
    invalid.TraitRanks[0] = 5;
    const auto refused = SeedAwakeningInputs(content, invalid, 9101);
    R91Expect(!refused.Ok() && !refused.State.Seeded, "invalid ranks do not become a seeded zero-trait owner");
    R91Expect(!SeedAwakeningInputs(ContentDb{}, creation, 9101).Ok(), "missing content cannot seed a life");
}

std::vector<DayLoopEvent> R91Completions(const ActivityCatalogue& catalogue)
{
    DayLoopState day = NewDayLoopState();
    DayLoopCatalogues cats;
    cats.Activities = &catalogue;
    DayCommand plan;
    plan.Kind = DayCommandKind::PlanDay;
    R91Expect(ApplyDayCommand(day, cats, plan).Ok(), "day is planned");
    const auto activity = std::find_if(catalogue.Defs.begin(), catalogue.Defs.end(), [](const auto& def) {
        return def.AllowanceMilliXp > 0 && !def.Evidence.empty();
    });
    R91Expect(activity != catalogue.Defs.end(), "real content has a learning activity with attribute evidence");
    std::vector<DayLoopEvent> receipts;
    for (int i = 0; i < 2; ++i)
    {
        DayCommand commit;
        commit.Kind = DayCommandKind::Commit;
        commit.ActivityId = activity->Id;
        commit.PlaceKind = activity->Place == ActivityAnyPlace ? "home" : activity->Place;
        R91Expect(ApplyDayCommand(day, cats, commit).Ok(), "real activity commitment accepted");
        DayCommand start;
        start.Kind = DayCommandKind::Start;
        R91Expect(ApplyDayCommand(day, cats, start).Ok(), "real activity starts");
        DayCommand complete;
        complete.Kind = DayCommandKind::Complete;
        const auto done = ApplyDayCommand(day, cats, complete);
        R91Expect(done.Ok(), "real activity completes through the practice rules");
        for (const auto& event : done.Events)
            if (event.Kind == DayLoopEventKind::Completed)
                receipts.push_back(event);
    }
    R91Expect(receipts.size() == 2 && !receipts.front().Credited.empty(), "real completions return post-practice credit");
    return receipts;
}

void R91Credits(const ContentDb& content, const ActivityCatalogue& catalogue, const AbilityRecipeSet& recipes)
{
    const auto receipts = R91Completions(catalogue);
    auto state = R91Seed(content, {});
    const auto baseline = state;
    const auto applied = ApplyActivityCredits(state, receipts);
    R91Expect(applied.Ok() && applied.Applied == 2 && applied.Replayed == 0, "both genuine receipts apply once");
    for (const auto& skill : baseline.Stats.Skills)
    {
        std::int32_t expected = skill.LevelMilli;
        for (const auto& receipt : receipts)
            for (const auto& credit : receipt.Credited)
                if (credit.SkillId == skill.SkillId)
                    expected += credit.MilliXp;
        R91Expect(state.Stats.FindSkill(skill.SkillId)->LevelMilli == expected, "only actual post-practice XP is added");
    }
    for (std::size_t attr = 0; attr < AttributeCount; ++attr)
    {
        std::int64_t expected = 0;
        for (const auto& receipt : receipts)
            for (const auto& credit : receipt.Evidence)
                if (static_cast<std::size_t>(credit.Attr) == attr)
                    expected += credit.Milli;
        R91Expect(state.AttributeEvidenceMilli[attr] == expected && state.Stats.Attributes[attr] == baseline.Stats.Attributes[attr],
                  "attribute evidence is retained once without invented attribute growth");
    }
    const auto after = state;
    const auto replayed = ApplyActivityCredits(state, receipts);
    R91Expect(replayed.Ok() && replayed.Applied == 0 && replayed.Replayed == 2 && state.Stats == after.Stats &&
                  state.AttributeEvidenceMilli == after.AttributeEvidenceMilli &&
                  state.AppliedActivityInstances == after.AppliedActivityInstances,
              "replay adds no learning, evidence or consumed IDs");
    auto reverse = receipts;
    std::reverse(reverse.begin(), reverse.end());
    auto reordered = baseline;
    R91Expect(ApplyActivityCredits(reordered, reverse).Ok() && reordered.Stats == state.Stats &&
                  reordered.AppliedActivityInstances == state.AppliedActivityInstances &&
                  ResolveAwakening(BuildAwakeningInput(reordered), recipes) == ResolveAwakening(BuildAwakeningInput(state), recipes),
              "out-of-order receipts and resolver evaluation are deterministic");
    auto duplicates = baseline;
    const std::array repeated{receipts[0], receipts[0]};
    R91Expect(ApplyActivityCredits(duplicates, repeated).Replayed == 1, "duplicate within a batch also applies once");
    auto malformed = receipts;
    malformed.back().Credited.push_back({"skill.missing", 999});
    auto atomic = baseline;
    const auto refusal = ApplyActivityCredits(atomic, malformed);
    R91Expect(!refusal.Ok() && refusal.Applied == 0 && atomic.Stats == baseline.Stats &&
                  atomic.AppliedActivityInstances.empty() && atomic.AttributeEvidenceMilli == baseline.AttributeEvidenceMilli,
              "invalid second receipt rolls back the whole batch");
    for (int variant = 0; variant < 5; ++variant)
    {
        auto bad = receipts.front();
        if (variant == 0)
            bad.Credited.front().MilliXp = -1;
        else if (variant == 1)
            bad.Credited.push_back(bad.Credited.front());
        else if (variant == 2)
            bad.Evidence.front().Attr = static_cast<Attribute>(AttributeCount);
        else if (variant == 3)
            bad.Evidence.push_back(bad.Evidence.front());
        else
            bad.Instance = 0;
        R91Expect(!ApplyActivityCredits(atomic, std::span(&bad, 1)).Ok() && atomic.Stats == baseline.Stats &&
                      atomic.AppliedActivityInstances.empty(), "malformed credits refuse without partial growth");
    }
    auto ignored = receipts.front();
    ignored.Kind = DayLoopEventKind::Cancelled;
    R91Expect(ApplyActivityCredits(atomic, std::span(&ignored, 1)).Ok() && atomic.Stats == baseline.Stats &&
                  atomic.AppliedActivityInstances.empty(), "cancelled and other non-completion events never grant XP");
    auto empty = receipts.front();
    empty.Credited.clear();
    empty.Evidence.clear();
    empty.ReceiptSeq = 0;
    R91Expect(ApplyActivityCredits(atomic, std::span(&empty, 1)).Applied == 1, "zero-learning completion still consumes its instance");
    R91Expect(ApplyActivityCredits(atomic, std::span(&receipts.front(), 1)).Replayed == 1 && atomic.Stats == baseline.Stats,
              "same instance cannot later acquire new learning even with a zero ReceiptSeq");
    auto unseeded = AwakeningState{};
    R91Expect(!ApplyActivityCredits(unseeded, receipts).Ok(), "default unseeded owner refuses credits");
    auto saturated = baseline;
    const auto& first = receipts.front();
    const auto skill = std::find_if(saturated.Stats.Skills.begin(), saturated.Stats.Skills.end(), [&](const auto& value) {
        return value.SkillId == first.Credited.front().SkillId;
    });
    skill->LevelMilli = std::numeric_limits<std::int32_t>::max() - 1;
    const auto attr = static_cast<std::size_t>(first.Evidence.front().Attr);
    saturated.AttributeEvidenceMilli[attr] = std::numeric_limits<std::int64_t>::max() - 1;
    R91Expect(ApplyActivityCredits(saturated, std::span(&first, 1)).Ok() &&
                  saturated.Stats.FindSkill(first.Credited.front().SkillId)->LevelMilli == std::numeric_limits<std::int32_t>::max() &&
                  saturated.AttributeEvidenceMilli[attr] == std::numeric_limits<std::int64_t>::max(),
              "storage limits saturate without integer overflow");
    R91Unknown(BuildAwakeningInput(state).Projection);
    R91Expect(!state.Granted && state.Conduct.Entries.empty() && state.MakeUp.Phase == AwakeningMakeUpPhase::NotRequired &&
                  !state.MakeUp.At && state.AppointmentAt == baseline.AppointmentAt && state.ExaminationAt == baseline.ExaminationAt,
              "learning never records conduct, grants awakening or schedules a session");
}

void R91EvidenceAndFallback(const ContentDb& content, const AbilityRecipeSet& recipes)
{
    auto state = R91Seed(content, {});
    const auto unknown = ResolveAwakening(BuildAwakeningInput(state), recipes);
    R91Expect(unknown.Ok() && !unknown.ConductObserved && unknown.Ability.Fallback && !unknown.Ability.RecipeId.empty(),
              "zero-conduct zero-trait input uses the existing recipe fallback");
    for (const auto rating : unknown.ConductRating)
        R91Expect(rating == AwakeningConductBudgetPoints / PowerDimensionCount, "existing zero-conduct budget is unpenalised");
    const auto profile = AwakenedStartingProfile(unknown, GameClock::DayNumber(state.AppointmentAt));
    R91Expect(profile && profile->Grade == AwakenedGrade::F && StageWithinGrade(*profile) >= 1,
              "existing starting-profile reader supplies an F-grade stage");
    R91Expect(!state.Granted, "calculating fallback and starting profile provides no supervised-session skip path");
    R91Expect(state.AppointmentAt == GameClock::FromCalendar(2043, 1, 23, 9, 0) &&
                  state.ExaminationAt == GameClock::FromCalendar(2043, 2, 2, 6, 0), "fixed appointment and examination instants");
    const AptitudeEvidence item{"r91.model", AptitudeSource::StructureModel, AptitudeFacet::Analysis,
                                "problem_family.r91", AptitudeResult::Unsound, false, 100};
    R91Expect(AptitudeAddEvidence(state.ProjectionEvidence, item).Ok(), "genuine unsound observation is accepted");
    const auto observed = BuildAwakeningInput(state);
    R91Expect(observed.Projection == AptitudeProject(state.ProjectionEvidence), "adapter preserves exact genuine projection");
    const auto* scholar = observed.Projection.Find(AptitudePath::Scholar);
    R91Expect(scholar && scholar->Facets.front().State == AptitudeFacetState::Observed && scholar->Strongest.empty(),
              "observed unsound evidence is distinct from unobserved and not upgraded to demonstrated strength");
    ConductRecord action{"r91.crossing", "r91.wait_together", ConductAction::Hold, 2, 200, {}};
    R91Expect(RecordConduct(state.Conduct, action).Ok(), "existing conduct ledger accepts an actual action");
    const auto input = BuildAwakeningInput(state);
    R91Expect(input.Conduct == SummariseConduct(state.Conduct) && input.Stats == state.Stats && input.WorldSeed == 9101,
              "adapter copies existing owners with no alternate scoring");
    const auto result = ResolveAwakening(input, recipes);
    R91Expect(result.Ok() && result.ConductObserved && ResolveAwakening(BuildAwakeningInput(state), recipes) == result,
              "actual conduct uses the existing deterministic resolver");
    R91Expect(!state.Granted && state.Conduct.Entries.size() == 1 && state.ProjectionEvidence.Items.size() == 1,
              "building and resolving do not mutate authoritative state or grant");
    // Fixture for the future supervised-session reducer's committed output.
    state.MakeUp.Phase = AwakeningMakeUpPhase::Completed;
    state.MakeUp.At = state.AppointmentAt + MinutesPerDay;
    const auto grantedAt = *state.MakeUp.At + 60;
    const auto starting = AwakenedStartingProfile(result, GameClock::DayNumber(grantedAt));
    R91Expect(starting.has_value(), "actual resolver result supplies a committed starting profile");
    state.Granted = AwakeningGrant{grantedAt, input, result, *starting};
    const auto later = R91Completions(LoadActivityCatalogue(content).Catalogue);
    R91Expect(ApplyActivityCredits(state, later).Ok() && state.Granted->Result == result &&
                  state.Granted->Input.Stats == input.Stats && state.Granted->StartingProfile.Grade == AwakenedGrade::F &&
                  state.ProjectionEvidence.Items == std::vector{item} && state.Conduct.Entries.size() == 1 &&
                  state.MakeUp.Phase == AwakeningMakeUpPhase::Completed && state.MakeUp.At == state.AppointmentAt + MinutesPerDay,
              "later ordinary learning preserves committed grant, genuine projection, conduct and make-up scheduling");
}
} // namespace

int main(int argc, char** argv)
{
    R91Expect(argc == 2, "pass Data/Content directory");
    const auto content = LoadContent(R91ReadContent(argv[1]));
    for (const auto& issue : content.Issues)
        std::cerr << issue.File << ": " << issue.Message << '\n';
    R91Expect(content.Ok(), "real content validates");
    const auto recipes = LoadAbilityRecipes(content.Db);
    const auto activities = LoadActivityCatalogue(content.Db);
    R91Expect(recipes.Ok() && activities.Ok(), "existing ability and activity readers succeed");
    R91Creation(content.Db, recipes.Set);
    R91Credits(content.Db, activities.Catalogue, recipes.Set);
    R91EvidenceAndFallback(content.Db, recipes.Set);
    std::cout << "Awakening inputs: " << R91Assertions << " assertions passed\n";
}
