#include "PerfectWorldCharacterCreatorSubsystem.h"

namespace
{
std::string ToUtf8(const FString& Value)
{
    return std::string(TCHAR_TO_UTF8(*Value));
}
}

FString UPerfectWorldCharacterCreatorSubsystem::ToFString(std::string_view Value)
{
    return FString(UTF8_TO_TCHAR(std::string(Value).c_str()));
}

bool UPerfectWorldCharacterCreatorSubsystem::RecordResult(const PerfectWorld::OperationResult& Result)
{
    LastError = static_cast<EPWCreationError>(Result.Error);
    LastErrorMessage = FText::FromString(ToFString(Result.Message));
    return Result.Succeeded();
}

TArray<FPWTraitView> UPerfectWorldCharacterCreatorSubsystem::GetTraits() const
{
    const PerfectWorld::CreationReview Review = Creator.Review();
    const auto& Definitions = PerfectWorld::CharacterCreator::TraitDefinitions();
    TArray<FPWTraitView> Traits;
    Traits.Reserve(static_cast<int32>(Definitions.size()));
    for (std::size_t Index = 0; Index < Definitions.size(); ++Index)
    {
        FPWTraitView& Trait = Traits.AddDefaulted_GetRef();
        Trait.Id = FName(ToFString(Definitions[Index].Id));
        Trait.DisplayName = FText::FromString(ToFString(Definitions[Index].DisplayName));
        Trait.Group = FName(ToFString(Definitions[Index].Group));
        Trait.Rank = Review.Allocations[Index].Rank;
        Trait.Cost = Review.Allocations[Index].Cost;
    }
    return Traits;
}

bool UPerfectWorldCharacterCreatorSubsystem::SetTraitRank(FName TraitId, int32 Rank)
{
    return RecordResult(Creator.SetTraitRank(ToUtf8(TraitId.ToString()), Rank));
}

bool UPerfectWorldCharacterCreatorSubsystem::ApplyBalancedScholarPreset()
{
    return RecordResult(Creator.ApplyPreset("balanced_scholar"));
}

bool UPerfectWorldCharacterCreatorSubsystem::SetIdentity(const FString& GivenName, const FString& FamilyName,
                                                          const FString& Pronouns, const FString& HomeCity,
                                                          const FString& PrimaryLanguage)
{
    PerfectWorld::IdentityProfile Identity;
    Identity.GivenName = ToUtf8(GivenName);
    Identity.FamilyName = ToUtf8(FamilyName);
    Identity.Pronouns = ToUtf8(Pronouns);
    Identity.HomeCity = ToUtf8(HomeCity);
    Identity.PrimaryLanguage = ToUtf8(PrimaryLanguage);
    return RecordResult(Creator.SetIdentity(MoveTemp(Identity)));
}

bool UPerfectWorldCharacterCreatorSubsystem::SetCosmetics(const FString& BodyPreset, const FString& SkinTone,
                                                           const FString& HairStyle, const FString& HairColor,
                                                           const FString& Outfit)
{
    PerfectWorld::CosmeticProfile Cosmetics;
    Cosmetics.BodyPreset = ToUtf8(BodyPreset);
    Cosmetics.SkinTone = ToUtf8(SkinTone);
    Cosmetics.HairStyle = ToUtf8(HairStyle);
    Cosmetics.HairColor = ToUtf8(HairColor);
    Cosmetics.Outfit = ToUtf8(Outfit);
    return RecordResult(Creator.SetCosmetics(MoveTemp(Cosmetics)));
}

FPWCreationReview UPerfectWorldCharacterCreatorSubsystem::GetCreationReview() const
{
    const PerfectWorld::CreationReview CoreReview = Creator.Review();
    FPWCreationReview Review;
    Review.StartingAge = CoreReview.Age;
    Review.TotalBudget = CoreReview.TotalBudget;
    Review.Spent = CoreReview.Spent;
    Review.Remaining = CoreReview.Remaining;
    Review.RankFourCount = CoreReview.RankFourCount;
    Review.ReadyToStart = CoreReview.ReadyToStart;
    Review.HasStarted = CoreReview.HasStarted;
    return Review;
}

bool UPerfectWorldCharacterCreatorSubsystem::StartLife()
{
    return RecordResult(Creator.StartLife());
}

void UPerfectWorldCharacterCreatorSubsystem::ResetCreation()
{
    Creator.Reset();
    LastError = EPWCreationError::None;
    LastErrorMessage = FText::GetEmpty();
}
