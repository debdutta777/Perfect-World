#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CharacterCreation.h"
#include "PerfectWorldCreationViews.h"
#include "PerfectWorldCharacterCreatorSubsystem.generated.h"

/**
 * Mirrors PerfectWorld::CreationError by value, so the entries up to and
 * including NothingToConfirm must stay in the same order as the core enum.
 * FileAccess has no core counterpart; only this subsystem reports it, for a
 * save file that could not be read or written at all, so it stays last.
 */
UENUM(BlueprintType)
enum class EPWCreationError : uint8
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
    NothingToConfirm,
    FileAccess
};

/**
 * Mirrors PerfectWorld::ResumePoint by value; keep the order identical.
 * Load Game sends the player back to the screen this names.
 */
UENUM(BlueprintType)
enum class EPWResumePoint : uint8
{
    Appearance,
    Allocation,
    InWorld,
    Identity,
    Stream,
    Review
};

/**
 * The opening room this creation calls for. Always ask for this rather than
 * reading a household's home kit directly: at Household Resources rank 0 the
 * character was raised in institutional care and the chosen template does not
 * describe where they live.
 */
USTRUCT(BlueprintType)
struct FPWOpeningHomePlan
{
    GENERATED_BODY()

    /** The template the player chose, kept even while care overrides it. */
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FName HouseholdTemplateId;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText HouseholdName;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FName HomeKitId;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText HomeKitName;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 ResourceRank = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FName DressingTierId;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText DressingTierName;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText DressingDescription;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    bool bGrewUpInCare = false;
};

/** One authored household template. Choosing a household costs no points. */
USTRUCT(BlueprintType)
struct FPWHouseholdView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FName Id;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText GuardianExamples;

    /** Names the home kit the opening scene should build for this household. */
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FName HomeKitId;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText HomeKitName;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText StoryEmphasis;

    /** TODO(core): HouseholdDefinition does not publish the authored everyday-life text yet. */
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText EverydayTextUnavailableFromCore;

    /** Authored front-end copy, awaiting a portable household-presentation field. */
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText EverydayText;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    bool bSelected = false;
};

USTRUCT(BlueprintType)
struct FPWTraitView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FName Id;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FName Group;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 Rank = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 Cost = 0;
};

USTRUCT(BlueprintType)
struct FPWCreationReview
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 StartingAge = 16;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 TotalBudget = 1000;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 Spent = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 Remaining = 1000;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 RankFourCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    bool ReadyToStart = false;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    bool HasStarted = false;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FName HouseholdTemplateId;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FName HomeKitId;
};

/** One discrete appearance choice (face shape, hair style, build or uniform variant). */
USTRUCT(BlueprintType)
struct FPWAppearanceOption
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Appearance")
    FName Id;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Appearance")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Appearance")
    bool bSelected = false;
};

/**
 * Mirrors PerfectWorld::AppearanceRecord for Blueprint. Colours are carried as
 * FLinearColor for editor-friendly colour pickers rather than as raw ints; the
 * core's 0-255 ColorValue is divided by 255 to build each FLinearColor channel
 * and, going back, each channel is multiplied by 255, rounded to the nearest
 * integer with FMath::RoundToInt, and clamped to 0-255. Alpha is unused and is
 * always 1.
 */
USTRUCT(BlueprintType)
struct FPWAppearanceRecord
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Appearance")
    FName FaceShapeId;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Appearance")
    FName HairStyleId;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Appearance")
    FName BuildId;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Appearance")
    FName UniformVariantId;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Appearance")
    FLinearColor SkinTone = FLinearColor::White;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Appearance")
    FLinearColor HairColor = FLinearColor::White;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Appearance")
    FLinearColor EyeColor = FLinearColor::White;
};

/** Blueprint-facing owner of the deterministic, portable character-creation rules. */
UCLASS()
class PERFECTWORLD_API UPerfectWorldCharacterCreatorSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Perfect World|Creation|Traits") TArray<FPWTraitGroupView> GetTraitGroups() const;
    UFUNCTION(BlueprintPure, Category = "Perfect World|Creation|Traits") FPWTraitDefinitionView GetTraitDefinition(FName TraitId) const;
    UFUNCTION(BlueprintPure, Category = "Perfect World|Creation|Traits") FPWTraitChangePreview PreviewTraitRank(FName TraitId, int32 ProposedRank) const;
    UFUNCTION(BlueprintPure, Category = "Perfect World|Creation|Traits") FPWAllocationPreview PreviewPreset(FName PresetId) const;
    UFUNCTION(BlueprintPure, Category = "Perfect World|Creation|Traits") FPWAllocationPreview PreviewClearAllocation() const;
    UFUNCTION(BlueprintPure, Category = "Perfect World|Creation|Traits") FPWAllocationPreview PreviewResetAllocation() const;
    UFUNCTION(BlueprintPure, Category = "Perfect World|Creation|Traits") FPWAllocationHistory GetAllocationHistory() const;
    UFUNCTION(BlueprintCallable, Category = "Perfect World|Creation|Traits") bool ClearAllocation();
    UFUNCTION(BlueprintCallable, Category = "Perfect World|Creation|Traits") bool ResetAllocation();
    UFUNCTION(BlueprintCallable, Category = "Perfect World|Creation|Traits") bool UndoAllocation();
    UFUNCTION(BlueprintCallable, Category = "Perfect World|Creation|Traits") bool RedoAllocation();

    UFUNCTION(BlueprintPure, Category = "Perfect World|Creation")
    TArray<FPWTraitView> GetTraits() const;

    UFUNCTION(BlueprintCallable, Category = "Perfect World|Creation")
    bool SetTraitRank(FName TraitId, int32 Rank);

    UFUNCTION(BlueprintCallable, Category = "Perfect World|Creation")
    bool ApplyBalancedScholarPreset();

    UFUNCTION(BlueprintCallable, Category = "Perfect World|Creation")
    bool SetIdentity(const FString& GivenName, const FString& FamilyName, const FString& Pronouns,
                     const FString& HomeCity, const FString& PrimaryLanguage);

    /** The current appearance record, read back from the core every call. */
    UFUNCTION(BlueprintPure, Category = "Perfect World|Appearance")
    FPWAppearanceRecord GetAppearance() const;

    /** Pushes a whole appearance record through the core so all validation runs. */
    UFUNCTION(BlueprintCallable, Category = "Perfect World|Appearance")
    bool SetAppearance(const FPWAppearanceRecord& Appearance);

    /** The authored face shapes, with the currently selected one marked. */
    UFUNCTION(BlueprintPure, Category = "Perfect World|Appearance")
    TArray<FPWAppearanceOption> GetFaceShapeOptions() const;

    /** The authored hair styles, with the currently selected one marked. */
    UFUNCTION(BlueprintPure, Category = "Perfect World|Appearance")
    TArray<FPWAppearanceOption> GetHairStyleOptions() const;

    /** The authored builds, with the currently selected one marked. */
    UFUNCTION(BlueprintPure, Category = "Perfect World|Appearance")
    TArray<FPWAppearanceOption> GetBuildOptions() const;

    /** The authored uniform variants, with the currently selected one marked. */
    UFUNCTION(BlueprintPure, Category = "Perfect World|Appearance")
    TArray<FPWAppearanceOption> GetUniformVariantOptions() const;

    /**
     * Narrow per-control helpers: each reads the current appearance record,
     * changes the one field named, and pushes the whole record back through
     * the core so all validation runs there rather than in the UI.
     */
    UFUNCTION(BlueprintCallable, Category = "Perfect World|Appearance")
    bool SetFaceShape(FName FaceShapeId);

    UFUNCTION(BlueprintCallable, Category = "Perfect World|Appearance")
    bool SetHairStyle(FName HairStyleId);

    UFUNCTION(BlueprintCallable, Category = "Perfect World|Appearance")
    bool SetBuild(FName BuildId);

    UFUNCTION(BlueprintCallable, Category = "Perfect World|Appearance")
    bool SetUniformVariant(FName UniformVariantId);

    UFUNCTION(BlueprintCallable, Category = "Perfect World|Appearance")
    bool SetSkinColor(FLinearColor Color);

    UFUNCTION(BlueprintCallable, Category = "Perfect World|Appearance")
    bool SetHairColor(FLinearColor Color);

    UFUNCTION(BlueprintCallable, Category = "Perfect World|Appearance")
    bool SetEyeColor(FLinearColor Color);

    UFUNCTION(BlueprintPure, Category = "Perfect World|Creation")
    FPWCreationReview GetCreationReview() const;

    UFUNCTION(BlueprintCallable, Category = "Perfect World|Creation")
    bool StartLife();

    UFUNCTION(BlueprintCallable, Category = "Perfect World|Creation")
    void ResetCreation();

    UFUNCTION(BlueprintPure, Category = "Perfect World|Creation")
    EPWCreationError GetLastError() const { return LastError; }

    UFUNCTION(BlueprintPure, Category = "Perfect World|Creation")
    FText GetLastErrorMessage() const { return LastErrorMessage; }

    /** The five authored household templates, with the current one marked. */
    UFUNCTION(BlueprintPure, Category = "Perfect World|Creation")
    TArray<FPWHouseholdView> GetHouseholds() const;

    UFUNCTION(BlueprintCallable, Category = "Perfect World|Creation")
    bool SetHouseholdTemplate(FName HouseholdId);

    /** The opening room this creation calls for, household and rank combined. */
    UFUNCTION(BlueprintPure, Category = "Perfect World|Creation")
    FPWOpeningHomePlan GetOpeningHome() const;

    /** Where the player had reached, so Load Game returns them there. */
    UFUNCTION(BlueprintPure, Category = "Perfect World|Save")
    EPWResumePoint GetResumePoint() const;

    /** Records progress through the flow. A confirmed origin stays InWorld. */
    UFUNCTION(BlueprintCallable, Category = "Perfect World|Save")
    void SetResumePoint(EPWResumePoint Point);

    UFUNCTION(BlueprintPure, Category = "Perfect World|Save")
    static int32 GetSaveSlotCount();

    /** Absolute path of a slot file, or empty for an out-of-range slot. */
    UFUNCTION(BlueprintPure, Category = "Perfect World|Save")
    static FString GetSaveSlotPath(int32 SlotIndex);

    UFUNCTION(BlueprintPure, Category = "Perfect World|Save")
    static bool IsSaveSlotOccupied(int32 SlotIndex);

    /** One short line describing a slot, for showing on a slot button. */
    UFUNCTION(BlueprintPure, Category = "Perfect World|Save")
    FText DescribeSaveSlot(int32 SlotIndex) const;

    /** Writes the current record. Replaces whatever the slot held. */
    UFUNCTION(BlueprintCallable, Category = "Perfect World|Save")
    bool SaveToSlot(int32 SlotIndex);

    /**
     * Replaces the current record with the slot's. A missing, unreadable or
     * corrupt file leaves the in-progress creation untouched and reports a
     * message suitable for showing the player.
     */
    UFUNCTION(BlueprintCallable, Category = "Perfect World|Save")
    bool LoadFromSlot(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category = "Perfect World|Save")
    bool DeleteSaveSlot(int32 SlotIndex);

    // ---- Front-end additions (M1-15, M1-23). All rules stay in the portable core. ----

    /** Starts a fresh draft: everything reset, then the canonical Balanced Scholar allocation and Kolkata as the home city. */
    UFUNCTION(BlueprintCallable, Category = "Perfect World|Creation")
    void BeginNewCreation();

    /** The five verified presets in document order; the canonical default comes first. */
    UFUNCTION(BlueprintPure, Category = "Perfect World|Creation")
    TArray<FPWPresetView> GetPresets() const;

    UFUNCTION(BlueprintCallable, Category = "Perfect World|Creation")
    bool ApplyPreset(FName PresetId);

    UFUNCTION(BlueprintPure, Category = "Perfect World|Creation")
    FPWIdentityView GetIdentity() const;

    UFUNCTION(BlueprintCallable, Category = "Perfect World|Creation")
    bool SetGivenName(const FString& GivenName);

    UFUNCTION(BlueprintCallable, Category = "Perfect World|Creation")
    bool SetFamilyName(const FString& FamilyName);

    /** Refused with the core's own message unless the character is exactly 16 on 2 June 2042. */
    UFUNCTION(BlueprintCallable, Category = "Perfect World|Creation")
    bool SetDateOfBirth(int32 Year, int32 Month, int32 Day);

    UFUNCTION(BlueprintCallable, Category = "Perfect World|Creation")
    bool SetPronouns(const FString& Pronouns);

    /** Sets the primary language and keeps any additional languages already listed. */
    UFUNCTION(BlueprintCallable, Category = "Perfect World|Creation")
    bool SetPrimaryLanguage(const FString& PrimaryLanguage);

    UFUNCTION(BlueprintPure, Category = "Perfect World|Creation")
    TArray<FPWStreamView> GetSchoolStreams() const;

    UFUNCTION(BlueprintPure, Category = "Perfect World|Creation")
    FString GetSchoolReason() const;

    /** Free: a stream changes the timetable and nothing else. The reason may be empty. */
    UFUNCTION(BlueprintCallable, Category = "Perfect World|Creation")
    bool SetSchoolStream(FName StreamId, const FString& Reason);

    /** Review page data, including the core's own explanation when Start Life is refused. */
    UFUNCTION(BlueprintPure, Category = "Perfect World|Creation")
    FPWCreationSummary GetCreationSummary() const;

    /** The first save slot with no file, or -1 when all are occupied. */
    UFUNCTION(BlueprintPure, Category = "Perfect World|Save")
    static int32 FindFirstEmptySaveSlot();

    /** The occupied slot written most recently, or -1 when none is occupied. */
    UFUNCTION(BlueprintPure, Category = "Perfect World|Save")
    static int32 FindMostRecentSaveSlot();

    /** Whether the loaded or created life has been confirmed and play may begin. */
    UFUNCTION(BlueprintPure, Category = "Perfect World|Save")
    bool HasLifeStarted() const;

    // EN integration: in lane-life ResolveOriginResidency, clear ResidentLocations and populate only door locations
    // accepted by this creator's HasLaneOriginResidency(LocationId). Remove the synthetic Parent plan; fail closed
    // when the creator/origin is unavailable. Refresh after successful Start AND Load, including ResetWorld;
    // Initialize runs before the life is chosen. Use GetLaneOrigin for the counterpart id and relationship label.
    /** On-demand core resolution; bResolved=false leaves both IDs empty and grants no residency. */
    UFUNCTION(BlueprintPure, Category = "Perfect World|Creation")
    FPWLaneOriginView GetLaneOrigin() const;

    /** Core-authoritative, location-scoped residency; an unresolved origin grants nothing. */
    UFUNCTION(BlueprintPure, Category = "Perfect World|Creation")
    bool HasLaneOriginResidency(FName LocationId) const;

    /** Commits a started record only if its slot write succeeds; preserves the whole draft on failure. */
    UFUNCTION(BlueprintCallable, Category = "Perfect World|Save")
    bool StartLifeAndSaveToSlot(int32 SlotIndex);

private:
    bool RecordResult(const PerfectWorld::OperationResult& Result);
    bool RecordFileError(FText Message);
    static FString ToFString(std::string_view Value);

    PerfectWorld::CharacterCreator Creator;

    UPROPERTY(Transient)
    EPWCreationError LastError = EPWCreationError::None;

    UPROPERTY(Transient)
    FText LastErrorMessage;
};
