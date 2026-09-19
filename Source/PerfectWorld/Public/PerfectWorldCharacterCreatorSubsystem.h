#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CharacterCreation.h"
#include "PerfectWorldCharacterCreatorSubsystem.generated.h"

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
    InvalidSave
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

private:
    bool RecordResult(const PerfectWorld::OperationResult& Result);
    static FString ToFString(std::string_view Value);

    PerfectWorld::CharacterCreator Creator;

    UPROPERTY(Transient)
    EPWCreationError LastError = EPWCreationError::None;

    UPROPERTY(Transient)
    FText LastErrorMessage;
};
