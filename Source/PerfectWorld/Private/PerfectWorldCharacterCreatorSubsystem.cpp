#include "PerfectWorldCharacterCreatorSubsystem.h"

#include "CreationSaveSerialization.h"
#include "PerfectWorldCreationAdapter.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "SaveStore.h"

namespace
{
std::string ToUtf8(const FString& Value)
{
    return std::string(TCHAR_TO_UTF8(*Value));
}

FString FromUtf8(std::string_view Value)
{
    return FString(UTF8_TO_TCHAR(std::string(Value).c_str()));
}

/** 0-255 -> 0-1, matching the rounding documented on FPWAppearanceRecord. */
FLinearColor ToLinearColor(const PerfectWorld::ColorValue& Color)
{
    return FLinearColor(Color.R / 255.f, Color.G / 255.f, Color.B / 255.f, 1.f);
}

/** 0-1 -> 0-255: multiply, round to nearest int, clamp. */
PerfectWorld::ColorValue ToColorValue(const FLinearColor& Color)
{
    auto ToChannel = [](float Value)
    {
        return FMath::Clamp(FMath::RoundToInt(Value * 255.f), 0, 255);
    };
    PerfectWorld::ColorValue Result;
    Result.R = ToChannel(Color.R);
    Result.G = ToChannel(Color.G);
    Result.B = ToChannel(Color.B);
    return Result;
}

FPWAppearanceRecord ToUnrealAppearance(const PerfectWorld::AppearanceRecord& Record)
{
    FPWAppearanceRecord View;
    View.FaceShapeId = FName(FromUtf8(Record.FaceShapeId));
    View.HairStyleId = FName(FromUtf8(Record.HairStyleId));
    View.BuildId = FName(FromUtf8(Record.BuildId));
    View.UniformVariantId = FName(FromUtf8(Record.UniformVariantId));
    View.SkinTone = ToLinearColor(Record.SkinTone);
    View.HairColor = ToLinearColor(Record.HairColor);
    View.EyeColor = ToLinearColor(Record.EyeColor);
    return View;
}

/** The slot store resolves a relative path against the process directory, so hand it an absolute one. */
std::string ToSlotPathUtf8(const FString& Path)
{
    return ToUtf8(FPaths::ConvertRelativePathToFull(Path));
}

/** The store's validator: only text that parses as a creation record is a usable slot. */
bool AcceptCreationSaveText(std::string_view Text, std::string& Error)
{
    PerfectWorld::CharacterCreationSave Scratch;
    const PerfectWorld::OperationResult Result = PerfectWorld::DeserializeCreationSave(Text, Scratch);
    if (!Result.Succeeded())
    {
        Error = Result.Message;
        return false;
    }
    return true;
}

PerfectWorld::AppearanceRecord ToCoreAppearance(const FPWAppearanceRecord& View)
{
    PerfectWorld::AppearanceRecord Record;
    Record.FaceShapeId = ToUtf8(View.FaceShapeId.ToString());
    Record.HairStyleId = ToUtf8(View.HairStyleId.ToString());
    Record.BuildId = ToUtf8(View.BuildId.ToString());
    Record.UniformVariantId = ToUtf8(View.UniformVariantId.ToString());
    Record.SkinTone = ToColorValue(View.SkinTone);
    Record.HairColor = ToColorValue(View.HairColor);
    Record.EyeColor = ToColorValue(View.EyeColor);
    return Record;
}

template <std::size_t N>
TArray<FPWAppearanceOption> BuildOptionList(const std::array<PerfectWorld::AppearanceOption, N>& Options,
                                             const std::string& SelectedId)
{
    TArray<FPWAppearanceOption> Result;
    Result.Reserve(static_cast<int32>(N));
    for (const PerfectWorld::AppearanceOption& Option : Options)
    {
        FPWAppearanceOption& View = Result.AddDefaulted_GetRef();
        View.Id = FName(FromUtf8(Option.Id));
        View.DisplayName = FText::FromString(FromUtf8(Option.DisplayName));
        View.bSelected = Option.Id == SelectedId;
    }
    return Result;
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

FPWAppearanceRecord UPerfectWorldCharacterCreatorSubsystem::GetAppearance() const
{
    return ToUnrealAppearance(Creator.Appearance());
}

bool UPerfectWorldCharacterCreatorSubsystem::SetAppearance(const FPWAppearanceRecord& Appearance)
{
    return RecordResult(Creator.SetAppearance(ToCoreAppearance(Appearance)));
}

TArray<FPWAppearanceOption> UPerfectWorldCharacterCreatorSubsystem::GetFaceShapeOptions() const
{
    return BuildOptionList(PerfectWorld::FaceShapes(), Creator.Appearance().FaceShapeId);
}

TArray<FPWAppearanceOption> UPerfectWorldCharacterCreatorSubsystem::GetHairStyleOptions() const
{
    return BuildOptionList(PerfectWorld::HairStyles(), Creator.Appearance().HairStyleId);
}

TArray<FPWAppearanceOption> UPerfectWorldCharacterCreatorSubsystem::GetBuildOptions() const
{
    return BuildOptionList(PerfectWorld::Builds(), Creator.Appearance().BuildId);
}

TArray<FPWAppearanceOption> UPerfectWorldCharacterCreatorSubsystem::GetUniformVariantOptions() const
{
    return BuildOptionList(PerfectWorld::UniformVariants(), Creator.Appearance().UniformVariantId);
}

bool UPerfectWorldCharacterCreatorSubsystem::SetFaceShape(FName FaceShapeId)
{
    PerfectWorld::AppearanceRecord Record = Creator.Appearance();
    Record.FaceShapeId = ToUtf8(FaceShapeId.ToString());
    return RecordResult(Creator.SetAppearance(MoveTemp(Record)));
}

bool UPerfectWorldCharacterCreatorSubsystem::SetHairStyle(FName HairStyleId)
{
    PerfectWorld::AppearanceRecord Record = Creator.Appearance();
    Record.HairStyleId = ToUtf8(HairStyleId.ToString());
    return RecordResult(Creator.SetAppearance(MoveTemp(Record)));
}

bool UPerfectWorldCharacterCreatorSubsystem::SetBuild(FName BuildId)
{
    PerfectWorld::AppearanceRecord Record = Creator.Appearance();
    Record.BuildId = ToUtf8(BuildId.ToString());
    return RecordResult(Creator.SetAppearance(MoveTemp(Record)));
}

bool UPerfectWorldCharacterCreatorSubsystem::SetUniformVariant(FName UniformVariantId)
{
    PerfectWorld::AppearanceRecord Record = Creator.Appearance();
    Record.UniformVariantId = ToUtf8(UniformVariantId.ToString());
    return RecordResult(Creator.SetAppearance(MoveTemp(Record)));
}

bool UPerfectWorldCharacterCreatorSubsystem::SetSkinColor(FLinearColor Color)
{
    PerfectWorld::AppearanceRecord Record = Creator.Appearance();
    Record.SkinTone = ToColorValue(Color);
    return RecordResult(Creator.SetAppearance(MoveTemp(Record)));
}

bool UPerfectWorldCharacterCreatorSubsystem::SetHairColor(FLinearColor Color)
{
    PerfectWorld::AppearanceRecord Record = Creator.Appearance();
    Record.HairColor = ToColorValue(Color);
    return RecordResult(Creator.SetAppearance(MoveTemp(Record)));
}

bool UPerfectWorldCharacterCreatorSubsystem::SetEyeColor(FLinearColor Color)
{
    PerfectWorld::AppearanceRecord Record = Creator.Appearance();
    Record.EyeColor = ToColorValue(Color);
    return RecordResult(Creator.SetAppearance(MoveTemp(Record)));
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
        View.EverydayText = PWCreationPresentation::HouseholdEverydayText(View.Id);
        View.bSelected = Definition.Id == Selected;
    }
    return Households;
}

bool UPerfectWorldCharacterCreatorSubsystem::SetHouseholdTemplate(FName HouseholdId)
{
    return RecordResult(Creator.SetHouseholdTemplate(ToUtf8(HouseholdId.ToString())));
}

EPWResumePoint UPerfectWorldCharacterCreatorSubsystem::GetResumePoint() const
{
    return static_cast<EPWResumePoint>(Creator.Resume());
}

void UPerfectWorldCharacterCreatorSubsystem::SetResumePoint(EPWResumePoint Point)
{
    Creator.SetResumePoint(static_cast<PerfectWorld::ResumePoint>(Point));
}

FPWOpeningHomePlan UPerfectWorldCharacterCreatorSubsystem::GetOpeningHome() const
{
    const PerfectWorld::OpeningHomePlan CorePlan = Creator.ResolveOpeningHome();
    FPWOpeningHomePlan Plan;
    Plan.HouseholdTemplateId = FName(ToFString(CorePlan.HouseholdTemplateId));
    Plan.HouseholdName = FText::FromString(ToFString(CorePlan.HouseholdName));
    Plan.HomeKitId = FName(ToFString(CorePlan.HomeKitId));
    Plan.HomeKitName = FText::FromString(ToFString(CorePlan.HomeKitName));
    Plan.ResourceRank = CorePlan.ResourceRank;
    Plan.DressingTierId = FName(ToFString(CorePlan.DressingTierId));
    Plan.DressingTierName = FText::FromString(ToFString(CorePlan.DressingTierName));
    Plan.DressingDescription = FText::FromString(ToFString(CorePlan.DressingDescription));
    Plan.bGrewUpInCare = CorePlan.GrewUpInCare;
    return Plan;
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
    // A slot whose primary was lost but whose backup survives still loads, so it is occupied.
    const FString Path = GetSaveSlotPath(SlotIndex);
    return !Path.IsEmpty() &&
           (IFileManager::Get().FileExists(*Path) || IFileManager::Get().FileExists(*(Path + TEXT(".bak"))));
}

FText UPerfectWorldCharacterCreatorSubsystem::DescribeSaveSlot(int32 SlotIndex) const
{
    const FString Path = GetSaveSlotPath(SlotIndex);
    if (Path.IsEmpty())
    {
        return FText::FromString(TEXT("No such slot"));
    }
    const PerfectWorld::SaveReadResult Read = PerfectWorld::ReadSaveFile(ToSlotPathUtf8(Path), AcceptCreationSaveText);
    if (Read.Status == PerfectWorld::SaveReadStatus::Missing)
    {
        return FText::FromString(TEXT("Empty"));
    }

    // Describe without disturbing the draft: parse into a scratch record only.
    PerfectWorld::CharacterCreationSave Parsed;
    if (!Read.Succeeded() || !PerfectWorld::DeserializeCreationSave(Read.Text, Parsed).Succeeded())
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

    // The portable store writes a temp file, reads it back, validates it, keeps
    // the previous version as .bak and only then replaces the slot (M1-04).
    const std::string Text = PerfectWorld::SerializeCreationSave(Creator.CaptureSave());
    const PerfectWorld::SaveOperationResult Written =
        PerfectWorld::WriteSaveFile(ToSlotPathUtf8(Path), Text, AcceptCreationSaveText);
    if (!Written)
    {
        return RecordFileError(FText::FromString(ToFString(Written.Message)));
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
    const PerfectWorld::SaveReadResult Read = PerfectWorld::ReadSaveFile(ToSlotPathUtf8(Path), AcceptCreationSaveText);
    if (Read.Status == PerfectWorld::SaveReadStatus::Missing)
    {
        return RecordFileError(FText::FromString(FString::Printf(
            TEXT("Slot %d is empty. The creation in progress is unchanged."), SlotIndex + 1)));
    }
    if (!Read.Succeeded())
    {
        return RecordFileError(FText::FromString(
            ToFString(Read.Message) + TEXT(" The creation in progress is unchanged.")));
    }

    // Parse and validate before touching the live creator, so a corrupt file
    // costs the player nothing.
    PerfectWorld::CharacterCreationSave Parsed;
    if (!RecordResult(PerfectWorld::DeserializeCreationSave(Read.Text, Parsed)) || !RecordResult(Creator.RestoreSave(Parsed)))
    {
        return false;
    }
    if (Read.Status == PerfectWorld::SaveReadStatus::RecoveredFromBackup)
    {
        LastErrorMessage = FText::FromString(ToFString(Read.Message));
    }
    return true;
}

bool UPerfectWorldCharacterCreatorSubsystem::DeleteSaveSlot(int32 SlotIndex)
{
    const FString Path = GetSaveSlotPath(SlotIndex);
    if (Path.IsEmpty())
    {
        return RecordFileError(FText::FromString(TEXT("That save slot does not exist.")));
    }
    if (!IsSaveSlotOccupied(SlotIndex))
    {
        LastError = EPWCreationError::None;
        LastErrorMessage = FText::FromString(FString::Printf(TEXT("Slot %d was already empty."), SlotIndex + 1));
        return true;
    }
    const PerfectWorld::SaveOperationResult Deleted = PerfectWorld::DeleteSaveFiles(ToSlotPathUtf8(Path));
    if (!Deleted)
    {
        return RecordFileError(FText::FromString(ToFString(Deleted.Message)));
    }
    LastError = EPWCreationError::None;
    LastErrorMessage = FText::FromString(FString::Printf(TEXT("Deleted slot %d."), SlotIndex + 1));
    return true;
}
