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
constexpr std::int32_t SaveSchemaVersion = 1;
constexpr std::size_t TraitCount = 20;
constexpr std::int32_t MaxRank = 4;
constexpr std::int32_t MaxRankFourTraits = 4;

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
    InvalidSave
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
    std::vector<TraitAllocation> Allocations;
};

struct CharacterCreationSave
{
    std::int32_t SchemaVersion = SaveSchemaVersion;
    std::int32_t Age = CreationAge;
    std::array<std::int32_t, TraitCount> TraitRanks{};
    IdentityProfile Identity;
    CosmeticProfile Cosmetics;
    bool HasStarted = false;
};

class PERFECTWORLDCORE_API CharacterCreator
{
public:
    CharacterCreator() = default;

    [[nodiscard]] static const std::array<TraitDefinition, TraitCount>& TraitDefinitions() noexcept;
    [[nodiscard]] static std::int32_t CostForRank(std::int32_t rank) noexcept;

    [[nodiscard]] OperationResult SetTraitRank(std::string_view traitId, std::int32_t rank);
    [[nodiscard]] OperationResult ApplyPreset(std::string_view presetId);
    [[nodiscard]] OperationResult StartLife();
    void Reset();

    [[nodiscard]] CreationReview Review() const;
    [[nodiscard]] std::int32_t GetTraitRank(std::string_view traitId) const noexcept;
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
    IdentityProfile Identity_;
    CosmeticProfile Cosmetics_;
    bool HasStarted_ = false;
};
} // namespace PerfectWorld
