#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Unreal supplies this export macro; standalone CMake builds do not.
#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
constexpr std::int32_t CreationAge = 16;
constexpr std::int32_t CreationBudget = 1000;
// Version 1 stored trait ranks only. Version 2 adds the household template and
// is written by stable trait ID; version 1 records migrate to the default
// household rather than being reinterpreted.
constexpr std::int32_t SaveSchemaVersion = 2;
constexpr std::int32_t MinSupportedSaveVersion = 1;
constexpr std::size_t TraitCount = 20;
constexpr std::size_t HouseholdTemplateCount = 5;
constexpr std::size_t HomeDressingTierCount = 5;
constexpr std::string_view HouseholdResourcesTraitId = "household_resources";
constexpr std::int32_t SaveSlotCount = 3;
constexpr std::int32_t MaxRank = 4;
constexpr std::int32_t MaxRankFourTraits = 4;
constexpr std::string_view DefaultHouseholdTemplateId = "parent_household";

enum class CreationError
{
    None,
    UnknownTrait,
    RankOutOfRange,
    BudgetExceeded,
    TooManyRankFourTraits,
    BudgetNotFullyAllocated,
    CreationAlreadyStarted,
    UnsupportedPreset,
    UnsupportedSaveVersion,
    InvalidSave,
    UnknownHousehold,
    MalformedSaveText
};

struct PERFECTWORLDCORE_API OperationResult
{
    CreationError Error = CreationError::None;
    std::string Message;

    [[nodiscard]] bool Succeeded() const noexcept { return Error == CreationError::None; }
    explicit operator bool() const noexcept { return Succeeded(); }

    static OperationResult Success();
    static OperationResult Failure(CreationError error, std::string message);
};

struct TraitDefinition
{
    std::string_view Id;
    std::string_view DisplayName;
    std::string_view Group;
};

/**
 * One of the five authored household templates. Every template shares the same
 * baseline access to a bed, meals, storage, school and a guardian; the template
 * chooses narrative emphasis and which home kit the opening scene builds.
 * Choosing a household is free and never costs creation points.
 */
struct HouseholdDefinition
{
    std::string_view Id;
    std::string_view DisplayName;
    std::string_view GuardianExamples;
    std::string_view HomeKitId;
    std::string_view HomeKitName;
    std::string_view StoryEmphasis;
};

/**
 * How far the opening home is furnished, derived from the Household Resources
 * rank. Every tier describes what it adds to a decent working home: rank 0 is
 * an ordinary baseline, never deprivation, so the opening scene never uses
 * poverty as visual shorthand.
 */
struct HomeDressingTier
{
    std::string_view Id;
    std::string_view DisplayName;
    std::string_view Adds;
};

/**
 * The opening room to build: the shell the household template calls for, and
 * how far it is furnished. Five shells and five tiers resolved at run time, so
 * the content set stays ten authored pieces rather than twenty-five rooms.
 */
struct OpeningHomePlan
{
    std::string_view HouseholdTemplateId;
    std::string_view HouseholdName;
    std::string_view HomeKitId;
    std::string_view HomeKitName;
    std::int32_t ResourceRank = 0;
    std::string_view DressingTierId;
    std::string_view DressingTierName;
    std::string_view DressingAdds;
};

struct IdentityProfile
{
    std::string GivenName;
    std::string FamilyName;
    std::string Pronouns;
    std::string HomeCity;
    std::string PrimaryLanguage;
};

struct CosmeticProfile
{
    std::string BodyPreset;
    std::string SkinTone;
    std::string HairStyle;
    std::string HairColor;
    std::string Outfit;
};

struct TraitAllocation
{
    std::string_view Id;
    std::int32_t Rank = 0;
    std::int32_t Cost = 0;
};

struct CreationReview
{
    std::int32_t Age = CreationAge;
    std::int32_t TotalBudget = CreationBudget;
    std::int32_t Spent = 0;
    std::int32_t Remaining = CreationBudget;
    std::int32_t RankFourCount = 0;
    bool ReadyToStart = false;
    bool HasStarted = false;
    std::string_view HouseholdTemplateId = DefaultHouseholdTemplateId;
    std::string_view HomeKitId;
    std::vector<TraitAllocation> Allocations;
};

struct CharacterCreationSave
{
    std::int32_t SchemaVersion = SaveSchemaVersion;
    std::int32_t Age = CreationAge;
    std::array<std::int32_t, TraitCount> TraitRanks{};
    std::string HouseholdTemplateId{DefaultHouseholdTemplateId};
    IdentityProfile Identity;
    CosmeticProfile Cosmetics;
    bool HasStarted = false;
};

class PERFECTWORLDCORE_API CharacterCreator
{
public:
    CharacterCreator() = default;

    [[nodiscard]] static const std::array<TraitDefinition, TraitCount>& TraitDefinitions() noexcept;
    [[nodiscard]] static const std::array<HouseholdDefinition, HouseholdTemplateCount>& HouseholdDefinitions() noexcept;
    [[nodiscard]] static const HouseholdDefinition* FindHousehold(std::string_view householdId) noexcept;
    [[nodiscard]] static const std::array<HomeDressingTier, HomeDressingTierCount>& HomeDressingTiers() noexcept;
    [[nodiscard]] static std::int32_t CostForRank(std::int32_t rank) noexcept;

    [[nodiscard]] OperationResult SetTraitRank(std::string_view traitId, std::int32_t rank);
    [[nodiscard]] OperationResult SetHouseholdTemplate(std::string_view householdId);
    [[nodiscard]] OperationResult ApplyPreset(std::string_view presetId);
    [[nodiscard]] OperationResult StartLife();
    void Reset();

    [[nodiscard]] CreationReview Review() const;
    [[nodiscard]] std::int32_t GetTraitRank(std::string_view traitId) const noexcept;
    [[nodiscard]] const HouseholdDefinition& Household() const noexcept;

    /**
     * Which opening room this creation calls for. Combines the chosen household
     * template with the current Household Resources rank. Valid at any time, so
     * the creator can preview it before life begins.
     */
    [[nodiscard]] OpeningHomePlan ResolveOpeningHome() const;
    [[nodiscard]] const IdentityProfile& Identity() const noexcept { return Identity_; }
    [[nodiscard]] const CosmeticProfile& Cosmetics() const noexcept { return Cosmetics_; }
    [[nodiscard]] bool HasStarted() const noexcept { return HasStarted_; }

    [[nodiscard]] OperationResult SetIdentity(IdentityProfile identity);
    [[nodiscard]] OperationResult SetCosmetics(CosmeticProfile cosmetics);

    [[nodiscard]] CharacterCreationSave CaptureSave() const;
    [[nodiscard]] OperationResult RestoreSave(const CharacterCreationSave& save);

private:
    [[nodiscard]] static std::int32_t FindTraitIndex(std::string_view traitId) noexcept;
    [[nodiscard]] static OperationResult ValidateRanks(const std::array<std::int32_t, TraitCount>& ranks,
                                                       bool requireFullBudget);

    std::array<std::int32_t, TraitCount> TraitRanks_{};
    std::string HouseholdTemplateId_{DefaultHouseholdTemplateId};
    IdentityProfile Identity_;
    CosmeticProfile Cosmetics_;
    bool HasStarted_ = false;
};
} // namespace PerfectWorld
