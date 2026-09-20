#include "PerfectWorldCharacterCreatorSubsystem.h"

#include "CreationSaveSerialization.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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
    Review.HouseholdTemplateId = FName(ToFString(CoreReview.HouseholdTemplateId));
    Review.HomeKitId = FName(ToFString(CoreReview.HomeKitId));
    return Review;
}

TArray<FPWHouseholdView> UPerfectWorldCharacterCreatorSubsystem::GetHouseholds() const
{
    const std::string_view Selected = Creator.Household().Id;
    const auto& Definitions = PerfectWorld::CharacterCreator::HouseholdDefinitions();
    TArray<FPWHouseholdView> Households;
    Households.Reserve(static_cast<int32>(Definitions.size()));
    for (const PerfectWorld::HouseholdDefinition& Definition : Definitions)
    {
        FPWHouseholdView& View = Households.AddDefaulted_GetRef();
        View.Id = FName(ToFString(Definition.Id));
        View.DisplayName = FText::FromString(ToFString(Definition.DisplayName));
        View.GuardianExamples = FText::FromString(ToFString(Definition.GuardianExamples));
        View.HomeKitId = FName(ToFString(Definition.HomeKitId));
        View.HomeKitName = FText::FromString(ToFString(Definition.HomeKitName));
        View.StoryEmphasis = FText::FromString(ToFString(Definition.StoryEmphasis));
        View.bSelected = Definition.Id == Selected;
    }
    return Households;
}

bool UPerfectWorldCharacterCreatorSubsystem::SetHouseholdTemplate(FName HouseholdId)
{
    return RecordResult(Creator.SetHouseholdTemplate(ToUtf8(HouseholdId.ToString())));
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

bool UPerfectWorldCharacterCreatorSubsystem::RecordFileError(FText Message)
{
    LastError = EPWCreationError::FileAccess;
    LastErrorMessage = MoveTemp(Message);
    return false;
}

int32 UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotCount()
{
    return PerfectWorld::SaveSlotCount;
}

FString UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotPath(int32 SlotIndex)
{
    const std::string FileName = PerfectWorld::SaveSlotFileName(SlotIndex);
    if (FileName.empty())
    {
        return FString();
    }
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), ToFString(FileName));
}

bool UPerfectWorldCharacterCreatorSubsystem::IsSaveSlotOccupied(int32 SlotIndex)
{
    const FString Path = GetSaveSlotPath(SlotIndex);
    return !Path.IsEmpty() && IFileManager::Get().FileExists(*Path);
}

FText UPerfectWorldCharacterCreatorSubsystem::DescribeSaveSlot(int32 SlotIndex) const
{
    const FString Path = GetSaveSlotPath(SlotIndex);
    if (Path.IsEmpty())
    {
        return FText::FromString(TEXT("No such slot"));
    }
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *Path))
    {
        return FText::FromString(TEXT("Empty"));
    }

    // Describe without disturbing the draft: parse into a scratch record only.
    PerfectWorld::CharacterCreationSave Parsed;
    if (!PerfectWorld::DeserializeCreationSave(ToUtf8(Text), Parsed).Succeeded())
    {
        return FText::FromString(TEXT("Unreadable file"));
    }
    const PerfectWorld::HouseholdDefinition* Household =
        PerfectWorld::CharacterCreator::FindHousehold(Parsed.HouseholdTemplateId);
    const FString HouseholdName =
        Household != nullptr ? ToFString(Household->DisplayName) : TEXT("Unknown household");
    const FString Name = Parsed.Identity.GivenName.empty() ? TEXT("Unnamed")
                                                           : ToFString(Parsed.Identity.GivenName);
    return FText::FromString(FString::Printf(TEXT("%s | %s | %s"), *Name, *HouseholdName,
                                             Parsed.HasStarted ? TEXT("Origin confirmed") : TEXT("Draft")));
}

bool UPerfectWorldCharacterCreatorSubsystem::SaveToSlot(int32 SlotIndex)
{
    const FString Path = GetSaveSlotPath(SlotIndex);
    if (Path.IsEmpty())
    {
        return RecordFileError(FText::FromString(TEXT("That save slot does not exist.")));
    }

    const std::string Text = PerfectWorld::SerializeCreationSave(Creator.CaptureSave());
    // Write beside the slot and move into place, so an interrupted save cannot
    // leave a half-written file where a readable record used to be.
    const FString TempPath = Path + TEXT(".tmp");
    if (!FFileHelper::SaveStringToFile(ToFString(Text), *TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        return RecordFileError(FText::FromString(TEXT("This save slot could not be written to disk.")));
    }
    if (!IFileManager::Get().Move(*Path, *TempPath, true))
    {
        IFileManager::Get().Delete(*TempPath, false, false, true);
        return RecordFileError(FText::FromString(TEXT("This save slot could not be replaced; the earlier record is unchanged.")));
    }

    LastError = EPWCreationError::None;
    LastErrorMessage = FText::FromString(FString::Printf(TEXT("Saved to slot %d."), SlotIndex + 1));
    return true;
}

bool UPerfectWorldCharacterCreatorSubsystem::LoadFromSlot(int32 SlotIndex)
{
    const FString Path = GetSaveSlotPath(SlotIndex);
    if (Path.IsEmpty())
    {
        return RecordFileError(FText::FromString(TEXT("That save slot does not exist.")));
    }
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *Path))
    {
        return RecordFileError(FText::FromString(FString::Printf(
            TEXT("Slot %d is empty or could not be read. The creation in progress is unchanged."), SlotIndex + 1)));
    }

    // Parse and validate before touching the live creator, so a corrupt file
    // costs the player nothing.
    PerfectWorld::CharacterCreationSave Parsed;
    if (!RecordResult(PerfectWorld::DeserializeCreationSave(ToUtf8(Text), Parsed)))
    {
        return false;
    }
    return RecordResult(Creator.RestoreSave(Parsed));
}

bool UPerfectWorldCharacterCreatorSubsystem::DeleteSaveSlot(int32 SlotIndex)
{
    const FString Path = GetSaveSlotPath(SlotIndex);
    if (Path.IsEmpty())
    {
        return RecordFileError(FText::FromString(TEXT("That save slot does not exist.")));
    }
    if (!IFileManager::Get().FileExists(*Path))
    {
        LastError = EPWCreationError::None;
        LastErrorMessage = FText::FromString(FString::Printf(TEXT("Slot %d was already empty."), SlotIndex + 1));
        return true;
    }
    if (!IFileManager::Get().Delete(*Path, false, true, true))
    {
        return RecordFileError(FText::FromString(TEXT("This save slot could not be deleted.")));
    }
    LastError = EPWCreationError::None;
    LastErrorMessage = FText::FromString(FString::Printf(TEXT("Deleted slot %d."), SlotIndex + 1));
    return true;
}
