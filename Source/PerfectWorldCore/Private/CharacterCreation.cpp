#include "CharacterCreation.h"

#include <algorithm>
#include <utility>

namespace PerfectWorld
{
namespace
{
constexpr std::array<std::int32_t, 5> RankCosts{0, 25, 60, 110, 180};

constexpr std::array<TraitDefinition, TraitCount> Definitions{{
    {"physical_presence", "Physical Presence", "appearance_presentation"},
    {"personal_presentation", "Personal Presentation", "appearance_presentation"},
    {"household_resources", "Household Resources", "family_resources_support"},
    {"guardian_support", "Guardian Support", "family_resources_support"},
    {"housing_stability", "Housing Stability", "family_resources_support"},
    {"vitality", "Vitality", "physical"},
    {"strength", "Strength", "physical"},
    {"agility", "Agility", "physical"},
    {"analytical_reasoning", "Analytical Reasoning", "cognition"},
    {"learning_discipline", "Learning Discipline", "cognition"},
    {"perception", "Perception", "cognition"},
    {"technical_aptitude", "Technical Aptitude", "talents"},
    {"creative_aptitude", "Creative Aptitude", "talents"},
    {"athletic_aptitude", "Athletic Aptitude", "talents"},
    {"empathy", "Empathy", "social"},
    {"confidence", "Confidence", "social"},
    {"community_connections", "Community Connections", "social"},
    {"academic_foundation", "Academic Foundation", "prior_experiences"},
    {"practical_experience", "Practical Experience", "prior_experiences"},
    {"adversity_resilience", "Adversity Resilience", "prior_experiences"},
}};

constexpr std::array<HouseholdDefinition, HouseholdTemplateCount> Households{{
    {"parent_household", "Parent household", "One or two parents or stepparents",
     "compact_family_flat", "Compact family flat", "Work schedules and expectations"},
    {"extended_family", "Extended family", "Aunt, uncle or multigenerational adults",
     "row_house", "Row house", "Shared space and many obligations"},
    {"grandparent_household", "Grandparent household", "One or two grandparents",
     "cooperative_flat", "Older cooperative flat", "Care flows in both directions"},
    {"adult_sibling_household", "Adult sibling household", "A sibling aged 21 or older",
     "rented_flat", "Rented flat", "A young household under financial pressure"},
    {"appointed_guardian", "Appointed guardian", "A family friend or hostel warden",
     "staff_quarters", "Staff quarters with private sleeping space",
     "Negotiated trust and formal responsibility"},
}};

// Indexed by Household Resources rank. Rank 0 is a category change, not a
// poorer version of rank 1: it means institutional care with no family
// household. Each tier is therefore self-describing rather than incremental.
// All five are written as places a person lives, never as squalor.
constexpr std::array<HomeDressingTier, HomeDressingTierCount> DressingTiers{{
    {"institutional_care", "Children's home",
     "A children's home run to a steady routine: a bed in a shared dormitory, a study table used in "
     "turns, a footlocker that locks, and staff who keep the timetable and know the residents by name."},
    {"modest", "Modest household",
     "A family home where the main room does everything: a bed, a study corner cleared at mealtimes, "
     "a fan, and storage the household looks after together."},
    {"steady", "Steady household",
     "A family home with a desk the teenager does not have to clear, a shelf kept for school books, "
     "and steady light for studying after dark."},
    {"comfortable", "Comfortable household",
     "A family home with a partitioned study space that stays quiet in the evening, reliable "
     "ventilation, and a computer the household shares."},
    {"ample", "Ample household",
     "A family home with a room the teenager does not share, dependable power through a cut, and a "
     "computer that is theirs."},
}};
} // namespace

std::string_view ResumePointId(ResumePoint point) noexcept
{
    switch (point)
    {
    case ResumePoint::Appearance: return "appearance";
    case ResumePoint::Allocation: return "allocation";
    case ResumePoint::InWorld: return "in_world";
    case ResumePoint::Identity: return "identity";
    case ResumePoint::Stream: return "stream";
    case ResumePoint::Review: return "review";
    }
    return "appearance";
}

bool ParseResumePoint(std::string_view id, ResumePoint& outPoint) noexcept
{
    if (id == "appearance") { outPoint = ResumePoint::Appearance; return true; }
    if (id == "allocation") { outPoint = ResumePoint::Allocation; return true; }
    if (id == "in_world") { outPoint = ResumePoint::InWorld; return true; }
    if (id == "identity") { outPoint = ResumePoint::Identity; return true; }
    if (id == "stream") { outPoint = ResumePoint::Stream; return true; }
    if (id == "review") { outPoint = ResumePoint::Review; return true; }
    return false;
}

OperationResult OperationResult::Success()
{
    return {};
}

OperationResult OperationResult::Failure(CreationError error, std::string message)
{
    return {error, std::move(message)};
}

const std::array<TraitDefinition, TraitCount>& CharacterCreator::TraitDefinitions() noexcept
{
    return Definitions;
}

const std::array<HouseholdDefinition, HouseholdTemplateCount>& CharacterCreator::HouseholdDefinitions() noexcept
{
    return Households;
}

const HouseholdDefinition* CharacterCreator::FindHousehold(std::string_view householdId) noexcept
{
    const auto found = std::find_if(Households.begin(), Households.end(),
        [householdId](const HouseholdDefinition& definition) { return definition.Id == householdId; });
    return found == Households.end() ? nullptr : &*found;
}

std::int32_t CharacterCreator::CostForRank(std::int32_t rank) noexcept
{
    if (rank < 0 || rank > MaxRank)
    {
        return -1;
    }
    return RankCosts[static_cast<std::size_t>(rank)];
}

std::int32_t CharacterCreator::FindTraitIndex(std::string_view traitId) noexcept
{
    const auto found = std::find_if(Definitions.begin(), Definitions.end(),
        [traitId](const TraitDefinition& definition) { return definition.Id == traitId; });
    return found == Definitions.end() ? -1 : static_cast<std::int32_t>(std::distance(Definitions.begin(), found));
}

OperationResult CharacterCreator::ValidateRanks(const std::array<std::int32_t, TraitCount>& ranks,
                                                 bool requireFullBudget)
{
    std::int32_t spent = 0;
    std::int32_t rankFourCount = 0;
    for (const std::int32_t rank : ranks)
    {
        const std::int32_t cost = CostForRank(rank);
        if (cost < 0)
        {
            return OperationResult::Failure(CreationError::RankOutOfRange, "Trait ranks must be between 0 and 4.");
        }
        spent += cost;
        rankFourCount += rank == MaxRank ? 1 : 0;
    }
    if (rankFourCount > MaxRankFourTraits)
    {
        return OperationResult::Failure(CreationError::TooManyRankFourTraits,
                                        "At most four traits may be raised to rank 4.");
    }
    if (spent > CreationBudget)
    {
        return OperationResult::Failure(CreationError::BudgetExceeded,
                                        "This allocation exceeds the 1000-point creation budget.");
    }
    if (requireFullBudget && spent != CreationBudget)
    {
        return OperationResult::Failure(CreationError::BudgetNotFullyAllocated,
                                        "Allocate all 1000 creation points before beginning life.");
    }
    return OperationResult::Success();
}

OperationResult CharacterCreator::SetTraitRank(std::string_view traitId, std::int32_t rank)
{
    if (HasStarted_)
    {
        return OperationResult::Failure(CreationError::CreationAlreadyStarted,
                                        "Character creation is locked after life begins.");
    }
    const std::int32_t index = FindTraitIndex(traitId);
    if (index < 0)
    {
        return OperationResult::Failure(CreationError::UnknownTrait, "Unknown trait id: " + std::string(traitId));
    }
    if (CostForRank(rank) < 0)
    {
        return OperationResult::Failure(CreationError::RankOutOfRange, "Trait ranks must be between 0 and 4.");
    }

    auto proposed = TraitRanks_;
    proposed[static_cast<std::size_t>(index)] = rank;
    if (OperationResult validation = ValidateRanks(proposed, false); !validation)
    {
        return validation;
    }
    TraitRanks_ = proposed;
    return OperationResult::Success();
}

OperationResult CharacterCreator::SetHouseholdTemplate(std::string_view householdId)
{
    if (HasStarted_)
    {
        return OperationResult::Failure(CreationError::CreationAlreadyStarted,
                                        "The household is locked after life begins.");
    }
    if (FindHousehold(householdId) == nullptr)
    {
        return OperationResult::Failure(CreationError::UnknownHousehold,
                                        "Unknown household template: " + std::string(householdId));
    }
    HouseholdTemplateId_ = std::string(householdId);
    return OperationResult::Success();
}

OperationResult CharacterCreator::ApplyPreset(std::string_view presetId)
{
    if (HasStarted_)
    {
        return OperationResult::Failure(CreationError::CreationAlreadyStarted,
                                        "Character creation is locked after life begins.");
    }
    const CreationPreset* preset = FindPreset(presetId);
    if (preset == nullptr)
    {
        return OperationResult::Failure(CreationError::UnsupportedPreset,
                                        "Unknown creation preset: " + std::string(presetId));
    }
    // Presets are validated from the same path as a player's own allocation.
    if (OperationResult validation = ValidateRanks(preset->Ranks, true); !validation)
    {
        return validation;
    }
    TraitRanks_ = preset->Ranks;
    return OperationResult::Success();
}

OperationResult CharacterCreator::StartLife()
{
    if (HasStarted_)
    {
        return OperationResult::Failure(CreationError::CreationAlreadyStarted, "Life has already begun.");
    }
    if (OperationResult validation = ValidateRanks(TraitRanks_, true); !validation)
    {
        return validation;
    }
    HasStarted_ = true;
    Resume_ = ResumePoint::InWorld;
    // Confirming the origin confirms its review page, which states the date of
    // birth and stream, so a migrated draft has nothing left to ask.
    IdentityMigrated_ = false;
    return OperationResult::Success();
}

void CharacterCreator::Reset()
{
    TraitRanks_.fill(0);
    HouseholdTemplateId_ = std::string(DefaultHouseholdTemplateId);
    Identity_ = {};
    School_ = {};
    IdentityMigrated_ = false;
    Appearance_ = AppearanceRecord::Default();
    Resume_ = ResumePoint::Appearance;
    HasStarted_ = false;
}

void CharacterCreator::SetResumePoint(ResumePoint point) noexcept
{
    // A confirmed origin cannot go back to an earlier screen, so the furthest
    // point reached is kept rather than overwritten.
    if (HasStarted_)
    {
        Resume_ = ResumePoint::InWorld;
        return;
    }
    Resume_ = point;
}

CreationReview CharacterCreator::Review() const
{
    CreationReview review;
    review.HasStarted = HasStarted_;
    const HouseholdDefinition& household = Household();
    review.HouseholdTemplateId = household.Id;
    review.HomeKitId = household.HomeKitId;
    if (const SchoolStreamDefinition* stream = FindSchoolStream(School_.StreamId); stream != nullptr)
    {
        review.SchoolStreamId = stream->Id;
    }
    review.Allocations.reserve(TraitCount);
    for (std::size_t index = 0; index < TraitCount; ++index)
    {
        const std::int32_t rank = TraitRanks_[index];
        const std::int32_t cost = CostForRank(rank);
        review.Spent += cost;
        review.RankFourCount += rank == MaxRank ? 1 : 0;
        review.Allocations.push_back({Definitions[index].Id, rank, cost});
    }
    review.Remaining = review.TotalBudget - review.Spent;
    review.ReadyToStart = !review.HasStarted && review.Spent == review.TotalBudget &&
                          review.RankFourCount <= MaxRankFourTraits;
    return review;
}

std::int32_t CharacterCreator::GetTraitRank(std::string_view traitId) const noexcept
{
    const std::int32_t index = FindTraitIndex(traitId);
    return index < 0 ? -1 : TraitRanks_[static_cast<std::size_t>(index)];
}

const std::array<HomeDressingTier, HomeDressingTierCount>& CharacterCreator::HomeDressingTiers() noexcept
{
    return DressingTiers;
}

OpeningHomePlan CharacterCreator::ResolveOpeningHome() const
{
    const HouseholdDefinition& Chosen = Household();
    // Ranks are validated on every edit, so this only guards against a rank
    // arriving from a future trait set with a wider range.
    const std::int32_t Rank = std::clamp(GetTraitRank(HouseholdResourcesTraitId), 0, MaxRank);
    const HomeDressingTier& Tier = DressingTiers[static_cast<std::size_t>(Rank)];

    OpeningHomePlan Plan;
    // The chosen template is always reported, so raising the rank out of care
    // restores the player's choice rather than silently losing it.
    Plan.HouseholdTemplateId = Chosen.Id;
    Plan.ResourceRank = Rank;
    Plan.DressingTierId = Tier.Id;
    Plan.DressingTierName = Tier.DisplayName;
    Plan.DressingDescription = Tier.Description;
    Plan.GrewUpInCare = Rank == 0;

    if (Plan.GrewUpInCare)
    {
        // Rank 0 means no family household at all, so the template names
        // nothing the opening scene can build from.
        Plan.HouseholdName = InstitutionalHouseholdName;
        Plan.HomeKitId = InstitutionalHomeKitId;
        Plan.HomeKitName = InstitutionalHomeKitName;
        return Plan;
    }

    Plan.HouseholdName = Chosen.DisplayName;
    Plan.HomeKitId = Chosen.HomeKitId;
    Plan.HomeKitName = Chosen.HomeKitName;
    return Plan;
}

const HouseholdDefinition& CharacterCreator::Household() const noexcept
{
    // HouseholdTemplateId_ is only ever assigned a validated ID, so the
    // default is a safety net rather than an expected outcome.
    const HouseholdDefinition* found = FindHousehold(HouseholdTemplateId_);
    return found != nullptr ? *found : *FindHousehold(DefaultHouseholdTemplateId);
}

OperationResult CharacterCreator::SetAppearance(AppearanceRecord appearance)
{
    if (HasStarted_)
    {
        return OperationResult::Failure(CreationError::CreationAlreadyStarted,
                                        "Appearance is locked after life begins.");
    }
    // Colours are clamped rather than refused: a slider dragged past its end
    // should settle at the end, not reject the whole edit. Unknown option IDs
    // are a different matter and do fail, because they name art that does not
    // exist.
    appearance.SkinTone = ClampColor(appearance.SkinTone);
    appearance.HairColor = ClampColor(appearance.HairColor);
    appearance.EyeColor = ClampColor(appearance.EyeColor);
    if (const std::string problem = DescribeAppearanceProblem(appearance); !problem.empty())
    {
        return OperationResult::Failure(CreationError::InvalidAppearance, problem);
    }
    Appearance_ = std::move(appearance);
    return OperationResult::Success();
}

CharacterCreationSave CharacterCreator::CaptureSave() const
{
    CharacterCreationSave save;
    save.TraitRanks = TraitRanks_;
    save.HouseholdTemplateId = HouseholdTemplateId_;
    save.Identity = Identity_;
    save.School = School_;
    save.IdentityMigrated = IdentityMigrated_;
    save.Appearance = Appearance_;
    save.Resume = Resume_;
    save.HasStarted = HasStarted_;
    return save;
}

OperationResult CharacterCreator::RestoreSave(const CharacterCreationSave& save)
{
    if (save.SchemaVersion < MinSupportedSaveVersion || save.SchemaVersion > SaveSchemaVersion)
    {
        return OperationResult::Failure(CreationError::UnsupportedSaveVersion,
                                        "The character save uses an unsupported schema version.");
    }
    if (save.Age != CreationAge)
    {
        return OperationResult::Failure(CreationError::InvalidSave,
                                        "The creation save must begin at age 16.");
    }
    if (OperationResult validation = ValidateRanks(save.TraitRanks, save.HasStarted); !validation)
    {
        return OperationResult::Failure(CreationError::InvalidSave, validation.Message);
    }

    // Version 1 predates household selection. Migrate it to the documented
    // default rather than guessing; a version 2 record must name a known
    // template, so a corrupted household ID fails the whole load.
    std::string household{DefaultHouseholdTemplateId};
    if (save.SchemaVersion >= 2)
    {
        if (FindHousehold(save.HouseholdTemplateId) == nullptr)
        {
            return OperationResult::Failure(CreationError::InvalidSave,
                                            "The creation save names an unknown household template: " +
                                                save.HouseholdTemplateId);
        }
        household = save.HouseholdTemplateId;
    }

    // Version 1 and 2 predate the structured appearance record. Their
    // free-form cosmetic strings had no defined mapping to the option IDs, so
    // they migrate to the documented default rather than to a guess.
    AppearanceRecord appearance = AppearanceRecord::Default();
    if (save.SchemaVersion >= 3)
    {
        appearance = save.Appearance;
        appearance.SkinTone = ClampColor(appearance.SkinTone);
        appearance.HairColor = ClampColor(appearance.HairColor);
        appearance.EyeColor = ClampColor(appearance.EyeColor);
        if (const std::string problem = DescribeAppearanceProblem(appearance); !problem.empty())
        {
            return OperationResult::Failure(CreationError::InvalidSave, problem);
        }
    }

    // Version 5 added the date of birth and stream. Older records migrate to
    // the R2 defaults and are marked so the game can ask the player to confirm.
    IdentityProfile identity;
    SchoolChoice school;
    bool migrated = false;
    if (OperationResult resolved = ResolveRestoredIdentity(save, identity, school, migrated); !resolved)
    {
        return resolved;
    }

    TraitRanks_ = save.TraitRanks;
    HouseholdTemplateId_ = std::move(household);
    Identity_ = std::move(identity);
    School_ = std::move(school);
    IdentityMigrated_ = migrated;
    Appearance_ = std::move(appearance);
    HasStarted_ = save.HasStarted;
    // Version 3 and older never recorded a resume point. Derive the best
    // available answer: a confirmed origin resumes in world, and an
    // unconfirmed draft resumes at the allocation screen it was written from.
    Resume_ = save.SchemaVersion >= 4 ? save.Resume
                                      : (save.HasStarted ? ResumePoint::InWorld : ResumePoint::Allocation);
    if (HasStarted_)
    {
        Resume_ = ResumePoint::InWorld;
    }
    return OperationResult::Success();
}
} // namespace PerfectWorld
