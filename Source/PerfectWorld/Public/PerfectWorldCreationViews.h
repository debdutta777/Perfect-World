#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldCreationViews.generated.h"

/**
 * Plain-data views the creation front end reads. Each is built from the portable
 * core on demand and carries no rules of its own: the core stays authoritative
 * and the UI only displays what these hold.
 */

USTRUCT(BlueprintType)
struct FPWTraitRankView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") int32 Rank = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") int32 CumulativeCost = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FText Meaning;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") TArray<FText> EffectLines;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FText AvailabilityNote;
};

USTRUCT(BlueprintType)
struct FPWTraitDefinitionView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FName Id;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FText DisplayName;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FName GroupId;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FText ShortDescription;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") TArray<FPWTraitRankView> Ranks;
};

USTRUCT(BlueprintType)
struct FPWTraitGroupView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FName Id;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FText DisplayName;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") int32 DisplayOrder = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") int32 AllocatedPoints = 0;
};

USTRUCT(BlueprintType)
struct FPWTraitChangePreview
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FName TraitId;
    /** Exact core save snapshot, including household/identity/start state; never displayed. */
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FString SourceDraftSnapshot;
    /** Ledger snapshot to compare before applying because core has no draft revision yet. */
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") TArray<int32> SourceRanks;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") int32 CurrentRank = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") int32 ProposedRank = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") int32 CurrentCost = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") int32 ProposedCost = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") int32 Charge = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") int32 Refund = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") bool bCanApply = false;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") int32 ErrorCode = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FName MessageCode;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") TMap<FName, FText> MessageArgs;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FText Message;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FText OpeningHome;
};

USTRUCT(BlueprintType)
struct FPWAllocationChangeView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FName TraitId;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FText DisplayName;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") int32 BeforeRank = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") int32 AfterRank = 0;
};

USTRUCT(BlueprintType)
struct FPWAllocationPreview
{
    GENERATED_BODY()
    /** TODO(core): no draft revision is exposed yet, so the adapter does not invent one. */
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") bool bDraftRevisionUnavailableFromCore = true;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FName PresetId;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FString SourceDraftSnapshot;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") TArray<int32> SourceRanks;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") TArray<FPWAllocationChangeView> Changes;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") TArray<FPWAllocationChangeView> StrongestTraits;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") int32 Spent = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") int32 TotalBudget = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") int32 Remaining = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") int32 RankFourCount = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") int32 MaxRankFour = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") bool bCanApply = false;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") int32 ErrorCode = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FName MessageCode;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") TMap<FName, FText> MessageArgs;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FText Message;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FText OpeningHome;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") bool bGrewUpInCare = false;
};

USTRUCT(BlueprintType)
struct FPWAllocationHistory
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") bool bCanUndo = false;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") bool bCanRedo = false;
};

/** One verified starting allocation (Docs/CHARACTER_CREATION.md, "Verified presets"). */
USTRUCT(BlueprintType)
struct FPWPresetView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FName Id;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText DisplayName;

    /** True when the current ranks match this preset exactly. */
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    bool bMatchesCurrent = false;
};

/** One of the authored school streams. A stream sets a timetable, not a destiny. */
USTRUCT(BlueprintType)
struct FPWStreamSampleDayView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FText Label;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FText Description;
};

USTRUCT(BlueprintType)
struct FPWStreamView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FName Id;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText TimetableSummary;

    /** The one-sentence summary on the stream card, from the core's stream presentation. */
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText CardSummary;

    /** True only if the core published no sample day for this stream; the screen then shows none. */
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    bool bSampleDayUnavailableFromCore = true;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    TArray<FPWStreamSampleDayView> SampleDay;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FText SampleDayCaption;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FText ClosingCaption;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FText RouteHeading;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FText RouteFlexibilityText;
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation") FText EqualStartingXPText;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    bool bSelected = false;
};

/** Everything the identity screen edits, plus the window the date of birth must fall in. */
USTRUCT(BlueprintType)
struct FPWIdentityView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FString GivenName;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FString FamilyName;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FString Pronouns;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FString HomeCity;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FString PrimaryLanguage;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 BirthYear = 2026;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 BirthMonth = 1;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 BirthDay = 1;

    /** Whole years old on 2 June 2042, which the core requires to be 16. */
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 AgeOnCampaignStart = 16;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 EarliestYear = 2025;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 EarliestMonth = 6;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 EarliestDay = 3;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 LatestYear = 2026;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 LatestMonth = 6;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 LatestDay = 2;
};

/** A trait that is above rank 0, as shown on the review page. */
USTRUCT(BlueprintType)
struct FPWReviewTraitLine
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FName Id;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 Rank = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 Cost = 0;
};

/** The whole creation, summarised for the review page. */
USTRUCT(BlueprintType)
struct FPWCreationSummary
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 TotalBudget = 1000;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 Spent = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 Remaining = 1000;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 RankFourCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 MaxRankFour = 4;

    /** The core's verdict: the only thing that enables Start Life. */
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    bool bReadyToStart = false;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    bool bHasStarted = false;

    /** The core's own explanation of why Start Life is refused, or empty when it is allowed. */
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText BlockingMessage;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText FullName;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText BirthDateText;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    int32 AgeOnCampaignStart = 16;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText PronounsText;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText LanguageText;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText HomeCityText;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText StreamName;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText StreamReason;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText HouseholdName;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText GuardianText;

    /** The resolved opening home: kit, how far it is furnished, and what that means. */
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText OpeningHomeText;

    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    bool bGrewUpInCare = false;

    /** Set only while bGrewUpInCare: what Household Resources rank 0 means and how to change it. */
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    FText CareNote;

    /** Every trait above rank 0, with its rank and cumulative cost. */
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    TArray<FPWReviewTraitLine> Traits;

    /** Plain-language notes the player should read before starting. */
    UPROPERTY(BlueprintReadOnly, Category = "Perfect World|Creation")
    TArray<FText> Warnings;
};
