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
    FileAccess
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

    UFUNCTION(BlueprintCallable, Category = "Perfect World|Creation")
    bool SetCosmetics(const FString& BodyPreset, const FString& SkinTone, const FString& HairStyle,
                      const FString& HairColor, const FString& Outfit);

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
