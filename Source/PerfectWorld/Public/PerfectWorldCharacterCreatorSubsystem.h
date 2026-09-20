#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CharacterCreation.h"
#include "PerfectWorldCharacterCreatorSubsystem.generated.h"

/**
 * Mirrors PerfectWorld::CreationError by value, so the entries up to and
 * including MalformedSaveText must stay in the same order as the core enum.
 * FileAccess has no core counterpart; only this subsystem reports it, for a
 * save file that could not be read or written at all.
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
    InWorld
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
