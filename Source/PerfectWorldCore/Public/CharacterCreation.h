#pragma once

#include "Appearance.h"
#include "CreationIdentity.h"

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
// Version 1 stored trait ranks only. Version 2 added the household template
// and stable-ID trait ranks. Version 3 replaces the old free-form cosmetic
// strings with a structured appearance record; because those strings had no
// defined mapping to the new option IDs, a version 1 or 2 record loads with
// the default appearance rather than a guessed one. Older records always
// migrate explicitly and are never reinterpreted in place. Version 4 records
// where the player had reached, so Load Game resumes there; older records
// derive it from whether the origin was confirmed. Version 5 adds the date of
// birth, additional languages, the school stream with its reason, and a flag
// marking a record whose date of birth and stream were filled in by migration
// (decision R2) rather than chosen; a record older than 5 migrates to the
// documented defaults and sets that flag.
constexpr std::int32_t SaveSchemaVersion = 5;
constexpr std::int32_t MinSupportedSaveVersion = 1;
constexpr std::size_t TraitCount = 20;
constexpr std::size_t HouseholdTemplateCount = 5;
constexpr std::size_t HomeDressingTierCount = 5;
constexpr std::string_view HouseholdResourcesTraitId = "household_resources";
/** The home kit built when the character was raised in institutional care. */
constexpr std::string_view InstitutionalHomeKitId = "childrens_home_dormitory";
constexpr std::string_view InstitutionalHomeKitName = "Children's home dormitory";
constexpr std::string_view InstitutionalHouseholdName = "Children's home";
constexpr std::int32_t SaveSlotCount = 3;
constexpr std::int32_t MaxRank = 4;
constexpr std::int32_t MaxRankFourTraits = 4;
constexpr std::string_view DefaultHouseholdTemplateId = "parent_household";
constexpr std::size_t CreationPresetCount = 5;
/** The canonical default preset. Docs/CHARACTER_CREATION.md, "Verified presets". */
constexpr std::string_view DefaultPresetId = "balanced_scholar";
/** How many allocation edits the creator remembers for undo (and, separately, redo). */
constexpr std::size_t MaxUndoDepth = 32;

/**
 * Where the player had got to when the slot was written, so Load Game returns
 * them there instead of always restarting at the beginning of creation.
 *
 * InWorld means the origin was confirmed and play had begun. No playable world
 * exists yet, so loading such a record currently lands on the confirmed
 * creation and says so; when rooms exist this is the value that will send the
 * player back into one.
 *
 * Identity, Stream and Review are the screens added around the original two
 * (M0-18). They are appended after InWorld rather than placed in flow order,
 * because the Unreal mirror enum copies these values and the existing three
 * must keep theirs. Nothing orders resume points numerically.
 */
enum class ResumePoint
{
    Appearance,
    Allocation,
    InWorld,
    Identity,
    Stream,
    Review
};

/** Stable text for the save file. Never renumber; the text is the contract. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view ResumePointId(ResumePoint point) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API bool ParseResumePoint(std::string_view id, ResumePoint& outPoint) noexcept;

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
    MalformedSaveText,
    InvalidAppearance,
    InvalidIdentity,
    InvalidDateOfBirth,
    UnknownSchoolStream,
    InvalidSchoolReason,
    NothingToConfirm
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
 * A verified starting allocation from Docs/CHARACTER_CREATION.md. Ranks are in
 * TraitDefinitions() order, one per trait. A preset allocates only the twenty
 * background traits; it never touches appearance, household template, stream
 * or difficulty, and every preset spends exactly CreationBudget points.
 */
struct CreationPreset
{
    std::string_view Id;
    std::string_view DisplayName;
    std::array<std::int32_t, TraitCount> Ranks{};
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
 * The opening home at each Household Resources rank. Rank 0 is not a poorer
 * family home: it means the character grew up in institutional care with no
 * family household, and the chosen household template does not apply. Ranks
 * 1 to 4 are family households of increasing means.
 *
 * Decided 2026-09-20. This deliberately couples the point ledger to how hard
 * the origin is, which the earlier contract avoided on purpose; see
 * Docs/CHARACTER_CREATION.md for the reasoning. Each tier is written to be
 * described with dignity and never as squalor.
 */
struct HomeDressingTier
{
    std::string_view Id;
    std::string_view DisplayName;
    std::string_view Description;
};

/**
 * The opening room to build: the shell the household template calls for, and
 * how far it is furnished. Five shells and five tiers resolved at run time, so
 * the content set stays ten authored pieces rather than twenty-five rooms.
 */
struct OpeningHomePlan
{
    /** The template the player chose. Kept even when care overrides it, so
     *  raising the resources rank restores their choice. */
    std::string_view HouseholdTemplateId;
    std::string_view HouseholdName;
    std::string_view HomeKitId;
    std::string_view HomeKitName;
    std::int32_t ResourceRank = 0;
    std::string_view DressingTierId;
    std::string_view DressingTierName;
    std::string_view DressingDescription;
    /** True at resources rank 0: raised in institutional care, no family
     *  household, and HouseholdTemplateId does not describe where they live. */
    bool GrewUpInCare = false;
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
    /** Shown on review. The stream changes no cost and no stat. */
    std::string_view SchoolStreamId = DefaultSchoolStreamId;
    std::vector<TraitAllocation> Allocations;
};

struct CharacterCreationSave
{
    std::int32_t SchemaVersion = SaveSchemaVersion;
    std::int32_t Age = CreationAge;
    std::array<std::int32_t, TraitCount> TraitRanks{};
    std::string HouseholdTemplateId{DefaultHouseholdTemplateId};
    IdentityProfile Identity;
    SchoolChoice School;
    AppearanceRecord Appearance{AppearanceRecord::Default()};
    bool HasStarted = false;
    ResumePoint Resume = ResumePoint::Appearance;
    /** True when the date of birth and stream are migration defaults the player never chose (R2). */
    bool IdentityMigrated = false;
};

/**
 * What an allocation edit WOULD produce, computed without changing the draft.
 * Result is exactly what the matching real edit would return, so a refusal
 * (for example after life has started) previews as the same refusal. When the
 * edit would be refused, Review and Home describe the unchanged draft.
 */
struct AllocationPreview
{
    OperationResult Result;
    CreationReview Review;
    /** The opening home the previewed allocation resolves to, so a preset that
     *  leaves Household Resources at rank 0 shows institutional care up front. */
    OpeningHomePlan Home;
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
    /** The five verified presets in document order; the default comes first. */
    [[nodiscard]] static const std::array<CreationPreset, CreationPresetCount>& Presets() noexcept;
    /** Null when the ID names no preset. */
    [[nodiscard]] static const CreationPreset* FindPreset(std::string_view presetId) noexcept;

    [[nodiscard]] OperationResult SetTraitRank(std::string_view traitId, std::int32_t rank);
    [[nodiscard]] OperationResult SetHouseholdTemplate(std::string_view householdId);
    [[nodiscard]] OperationResult ApplyPreset(std::string_view presetId);
    [[nodiscard]] OperationResult StartLife();
    void Reset();

    /**
     * Every trait to rank 0 as one transaction (one undo step, not twenty edits).
     * Leaves the draft under budget, so it cannot start life until reallocated.
     * Refused after life begins; clearing an already empty draft is a no-op that
     * adds no undo step.
     */
    [[nodiscard]] OperationResult ClearAllocation();
    /** Back to the default preset (DefaultPresetId) as one transaction. */
    [[nodiscard]] OperationResult ResetAllocation();

    /**
     * Allocation history. Only trait ranks are recorded: rank edits, presets,
     * clear and reset each add one step, and an edit that changes nothing adds
     * none. A new edit discards redo. Identity, household, stream and
     * appearance are never touched. Undo and redo are refused after life begins
     * (CanUndo and CanRedo then report false); with nothing to undo or redo they
     * fail with NothingToConfirm. The history is never saved and is cleared by
     * Reset and RestoreSave.
     */
    [[nodiscard]] bool CanUndo() const noexcept;
    [[nodiscard]] bool CanRedo() const noexcept;
    [[nodiscard]] OperationResult Undo();
    [[nodiscard]] OperationResult Redo();
    [[nodiscard]] std::size_t UndoDepth() const noexcept { return UndoStack_.size(); }
    [[nodiscard]] std::size_t RedoDepth() const noexcept { return RedoStack_.size(); }

    /** What ApplyPreset / ClearAllocation / ResetAllocation would produce. Never changes the draft. */
    [[nodiscard]] AllocationPreview PreviewPreset(std::string_view presetId) const;
    [[nodiscard]] AllocationPreview PreviewClear() const;
    [[nodiscard]] AllocationPreview PreviewReset() const;

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
    [[nodiscard]] const AppearanceRecord& Appearance() const noexcept { return Appearance_; }
    [[nodiscard]] bool HasStarted() const noexcept { return HasStarted_; }

    /** Where the player had reached. StartLife forces this to InWorld. */
    [[nodiscard]] ResumePoint Resume() const noexcept { return Resume_; }
    void SetResumePoint(ResumePoint point) noexcept;

    /**
     * Replaces the whole identity (names, pronouns, languages, date of birth).
     * Refused after life begins, and refused whole when any part is invalid.
     */
    [[nodiscard]] OperationResult SetIdentity(IdentityProfile identity);

    /** Exactly 16 on 2 June 2042: 3 June 2025 to 2 June 2026 inclusive. See AgeOnCampaignStart. */
    [[nodiscard]] OperationResult SetDateOfBirth(const DateOfBirth& birth);
    /** Free text, at most MaxPronounsBytes, no control characters. */
    [[nodiscard]] OperationResult SetPronouns(std::string pronouns);
    /** One primary language and up to MaxAdditionalLanguages others, no repeats. */
    [[nodiscard]] OperationResult SetLanguages(std::string primary, std::vector<std::string> additional);
    /** A stable stream ID from SchoolStreams() and a reason of at most MaxSchoolReasonBytes. Costs nothing and changes no stat. */
    [[nodiscard]] OperationResult SetSchoolStream(std::string_view streamId, std::string reason);

    [[nodiscard]] const SchoolChoice& School() const noexcept { return School_; }
    /** True while the date of birth and stream are migration defaults the player has not confirmed (R2). */
    [[nodiscard]] bool IdentityNeedsConfirmation() const noexcept { return IdentityMigrated_; }
    /**
     * Lets the player confirm or replace the migrated date of birth and stream.
     * This is the one identity edit allowed after life begins, because migrated
     * records are usually already started; it is refused when nothing is waiting
     * to be confirmed, and clears the flag on success.
     */
    [[nodiscard]] OperationResult ConfirmMigratedIdentity(const DateOfBirth& birth, std::string_view streamId,
                                                          std::string reason);

    [[nodiscard]] OperationResult SetAppearance(AppearanceRecord appearance);

    [[nodiscard]] CharacterCreationSave CaptureSave() const;
    [[nodiscard]] OperationResult RestoreSave(const CharacterCreationSave& save);

private:
    [[nodiscard]] static std::int32_t FindTraitIndex(std::string_view traitId) noexcept;
    [[nodiscard]] static OperationResult ValidateRanks(const std::array<std::int32_t, TraitCount>& ranks,
                                                       bool requireFullBudget);
    /**
     * The identity, school choice and migrated flag a record restores to. A v5
     * record is validated and refused when invalid; an older record gets the R2
     * defaults and the migrated flag. Defined in CreationIdentity.cpp.
     */
    [[nodiscard]] static OperationResult ResolveRestoredIdentity(const CharacterCreationSave& save,
                                                                 IdentityProfile& identity, SchoolChoice& school,
                                                                 bool& migrated);

    /** The one place a rank change is committed: records undo, drops redo. Equal ranks are a no-op. */
    void CommitRanks(const std::array<std::int32_t, TraitCount>& proposed);
    void ClearHistory() noexcept;

    std::array<std::int32_t, TraitCount> TraitRanks_{};
    std::vector<std::array<std::int32_t, TraitCount>> UndoStack_;
    std::vector<std::array<std::int32_t, TraitCount>> RedoStack_;
    std::string HouseholdTemplateId_{DefaultHouseholdTemplateId};
    IdentityProfile Identity_;
    SchoolChoice School_;
    AppearanceRecord Appearance_{AppearanceRecord::Default()};
    bool HasStarted_ = false;
    ResumePoint Resume_ = ResumePoint::Appearance;
    bool IdentityMigrated_ = false;
};
} // namespace PerfectWorld
