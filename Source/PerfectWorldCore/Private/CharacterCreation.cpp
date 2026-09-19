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

constexpr std::array<std::int32_t, TraitCount> BalancedScholarRanks{
    1, 1, 1, 3, 2,
    2, 1, 2,
    2, 3, 1,
    2, 1, 0,
    3, 1, 0,
    3, 1, 2
};
} // namespace

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

OperationResult CharacterCreator::ApplyPreset(std::string_view presetId)
{
    if (HasStarted_)
    {
        return OperationResult::Failure(CreationError::CreationAlreadyStarted,
                                        "Character creation is locked after life begins.");
    }
    if (presetId != "balanced_scholar")
    {
        return OperationResult::Failure(CreationError::UnsupportedPreset,
                                        "Unknown creation preset: " + std::string(presetId));
    }
    if (OperationResult validation = ValidateRanks(BalancedScholarRanks, true); !validation)
    {
        return validation;
    }
    TraitRanks_ = BalancedScholarRanks;
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
    return OperationResult::Success();
}

void CharacterCreator::Reset()
{
    TraitRanks_.fill(0);
    Identity_ = {};
    Cosmetics_ = {};
    HasStarted_ = false;
}

CreationReview CharacterCreator::Review() const
{
    CreationReview review;
    review.HasStarted = HasStarted_;
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

OperationResult CharacterCreator::SetIdentity(IdentityProfile identity)
{
    if (HasStarted_)
    {
        return OperationResult::Failure(CreationError::CreationAlreadyStarted,
                                        "Identity is locked after life begins.");
    }
    Identity_ = std::move(identity);
    return OperationResult::Success();
}

OperationResult CharacterCreator::SetCosmetics(CosmeticProfile cosmetics)
{
    if (HasStarted_)
    {
        return OperationResult::Failure(CreationError::CreationAlreadyStarted,
                                        "Appearance is locked after life begins.");
    }
    Cosmetics_ = std::move(cosmetics);
    return OperationResult::Success();
}

CharacterCreationSave CharacterCreator::CaptureSave() const
{
    CharacterCreationSave save;
    save.TraitRanks = TraitRanks_;
    save.Identity = Identity_;
    save.Cosmetics = Cosmetics_;
    save.HasStarted = HasStarted_;
    return save;
}

OperationResult CharacterCreator::RestoreSave(const CharacterCreationSave& save)
{
    if (save.SchemaVersion != SaveSchemaVersion)
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

    TraitRanks_ = save.TraitRanks;
    Identity_ = save.Identity;
    Cosmetics_ = save.Cosmetics;
    HasStarted_ = save.HasStarted;
    return OperationResult::Success();
}
} // namespace PerfectWorld
