#include "PerfectWorldCharacterCreatorFlow.h"

#include "CreationIdentity.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"

#define LOCTEXT_NAMESPACE "PerfectWorldCreationFlow"

namespace
{
FText FlowMonthName(int32 Month)
{
    switch (Month)
    {
    case 1: return LOCTEXT("MonthJan", "January");
    case 2: return LOCTEXT("MonthFeb", "February");
    case 3: return LOCTEXT("MonthMar", "March");
    case 4: return LOCTEXT("MonthApr", "April");
    case 5: return LOCTEXT("MonthMay", "May");
    case 6: return LOCTEXT("MonthJun", "June");
    case 7: return LOCTEXT("MonthJul", "July");
    case 8: return LOCTEXT("MonthAug", "August");
    case 9: return LOCTEXT("MonthSep", "September");
    case 10: return LOCTEXT("MonthOct", "October");
    case 11: return LOCTEXT("MonthNov", "November");
    default: return LOCTEXT("MonthDec", "December");
    }
}

/** Free text shown on review: the player's words, or a plain "not set" note. */
FText FlowTextOrNotSet(const FString& Value)
{
    return Value.IsEmpty() ? LOCTEXT("NotSet", "Not set") : FText::FromString(Value);
}
} // namespace

void UPerfectWorldCharacterCreatorSubsystem::BeginNewCreation()
{
    Creator.Reset();
    // The canonical default: a fresh creator yields the verified Balanced Scholar
    // allocation (Docs/CHARACTER_CREATION.md, "UX acceptance checks").
    RecordResult(Creator.ApplyPreset(PerfectWorld::DefaultPresetId));
    PerfectWorld::IdentityProfile Identity = Creator.Identity();
    Identity.HomeCity = "Kolkata";
    RecordResult(Creator.SetIdentity(MoveTemp(Identity)));
    LastError = EPWCreationError::None;
    LastErrorMessage = FText::GetEmpty();
}

TArray<FPWPresetView> UPerfectWorldCharacterCreatorSubsystem::GetPresets() const
{
    const PerfectWorld::CreationReview Review = Creator.Review();
    TArray<FPWPresetView> Result;
    for (const PerfectWorld::CreationPreset& Preset : PerfectWorld::CharacterCreator::Presets())
    {
        FPWPresetView& View = Result.AddDefaulted_GetRef();
        View.Id = FName(PWCreatorFlow::FromUtf8(Preset.Id));
        View.DisplayName = FText::FromString(PWCreatorFlow::FromUtf8(Preset.DisplayName));
        bool bMatches = Review.Allocations.size() == Preset.Ranks.size();
        for (std::size_t Index = 0; bMatches && Index < Preset.Ranks.size(); ++Index)
        {
            bMatches = Review.Allocations[Index].Rank == Preset.Ranks[Index];
        }
        View.bMatchesCurrent = bMatches;
    }
    return Result;
}

bool UPerfectWorldCharacterCreatorSubsystem::ApplyPreset(FName PresetId)
{
    return RecordResult(Creator.ApplyPreset(PWCreatorFlow::ToUtf8(PresetId.ToString())));
}

FPWIdentityView UPerfectWorldCharacterCreatorSubsystem::GetIdentity() const
{
    const PerfectWorld::IdentityProfile& Source = Creator.Identity();
    FPWIdentityView View;
    View.GivenName = PWCreatorFlow::FromUtf8(Source.GivenName);
    View.FamilyName = PWCreatorFlow::FromUtf8(Source.FamilyName);
    View.Pronouns = PWCreatorFlow::FromUtf8(Source.Pronouns);
    View.HomeCity = PWCreatorFlow::FromUtf8(Source.HomeCity);
    View.PrimaryLanguage = PWCreatorFlow::FromUtf8(Source.PrimaryLanguage);
    View.BirthYear = Source.Birth.Year;
    View.BirthMonth = Source.Birth.Month;
    View.BirthDay = Source.Birth.Day;
    View.AgeOnCampaignStart = static_cast<int32>(PerfectWorld::AgeOnCampaignStart(Source.Birth));
    View.EarliestYear = PerfectWorld::EarliestDateOfBirth.Year;
    View.EarliestMonth = PerfectWorld::EarliestDateOfBirth.Month;
    View.EarliestDay = PerfectWorld::EarliestDateOfBirth.Day;
    View.LatestYear = PerfectWorld::LatestDateOfBirth.Year;
    View.LatestMonth = PerfectWorld::LatestDateOfBirth.Month;
    View.LatestDay = PerfectWorld::LatestDateOfBirth.Day;
    return View;
}

bool UPerfectWorldCharacterCreatorSubsystem::SetGivenName(const FString& GivenName)
{
    PerfectWorld::IdentityProfile Identity = Creator.Identity();
    Identity.GivenName = PWCreatorFlow::ToUtf8(GivenName);
    return RecordResult(Creator.SetIdentity(MoveTemp(Identity)));
}

bool UPerfectWorldCharacterCreatorSubsystem::SetFamilyName(const FString& FamilyName)
{
    PerfectWorld::IdentityProfile Identity = Creator.Identity();
    Identity.FamilyName = PWCreatorFlow::ToUtf8(FamilyName);
    return RecordResult(Creator.SetIdentity(MoveTemp(Identity)));
}

bool UPerfectWorldCharacterCreatorSubsystem::SetDateOfBirth(int32 Year, int32 Month, int32 Day)
{
    PerfectWorld::DateOfBirth Birth;
    Birth.Year = Year;
    Birth.Month = Month;
    Birth.Day = Day;
    return RecordResult(Creator.SetDateOfBirth(Birth));
}

bool UPerfectWorldCharacterCreatorSubsystem::SetPronouns(const FString& Pronouns)
{
    return RecordResult(Creator.SetPronouns(PWCreatorFlow::ToUtf8(Pronouns)));
}

bool UPerfectWorldCharacterCreatorSubsystem::SetPrimaryLanguage(const FString& PrimaryLanguage)
{
    std::vector<std::string> Additional = Creator.Identity().AdditionalLanguages;
    return RecordResult(Creator.SetLanguages(PWCreatorFlow::ToUtf8(PrimaryLanguage), MoveTemp(Additional)));
}

TArray<FPWStreamView> UPerfectWorldCharacterCreatorSubsystem::GetSchoolStreams() const
{
    TArray<FPWStreamView> Result;
    for (const PerfectWorld::SchoolStreamDefinition& Stream : PerfectWorld::SchoolStreams())
    {
        FPWStreamView& View = Result.AddDefaulted_GetRef();
        View.Id = FName(PWCreatorFlow::FromUtf8(Stream.Id));
        View.DisplayName = FText::FromString(PWCreatorFlow::FromUtf8(Stream.DisplayName));
        View.TimetableSummary = FText::FromString(PWCreatorFlow::FromUtf8(Stream.TimetableSummary));
        View.bSelected = Stream.Id == Creator.School().StreamId;
    }
    return Result;
}

FString UPerfectWorldCharacterCreatorSubsystem::GetSchoolReason() const
{
    return PWCreatorFlow::FromUtf8(Creator.School().Reason);
}

bool UPerfectWorldCharacterCreatorSubsystem::SetSchoolStream(FName StreamId, const FString& Reason)
{
    return RecordResult(Creator.SetSchoolStream(PWCreatorFlow::ToUtf8(StreamId.ToString()), PWCreatorFlow::ToUtf8(Reason)));
}

FPWCreationSummary UPerfectWorldCharacterCreatorSubsystem::GetCreationSummary() const
{
    const PerfectWorld::CreationReview Review = Creator.Review();
    const PerfectWorld::IdentityProfile& Identity = Creator.Identity();
    const FPWOpeningHomePlan Home = GetOpeningHome();

    FPWCreationSummary Summary;
    Summary.TotalBudget = Review.TotalBudget;
    Summary.Spent = Review.Spent;
    Summary.Remaining = Review.Remaining;
    Summary.RankFourCount = Review.RankFourCount;
    Summary.MaxRankFour = PerfectWorld::MaxRankFourTraits;
    Summary.bReadyToStart = Review.ReadyToStart;
    Summary.bHasStarted = Review.HasStarted;

    // Ask the core why Start Life would be refused by trying it on a scratch copy,
    // so the explanation is the core's own text and the live creation is untouched.
    if (!Review.ReadyToStart)
    {
        PerfectWorld::CharacterCreator Probe = Creator;
        const PerfectWorld::OperationResult Verdict = Probe.StartLife();
        if (!Verdict.Succeeded())
        {
            Summary.BlockingMessage = FText::FromString(PWCreatorFlow::FromUtf8(Verdict.Message));
        }
    }

    const FString Given = PWCreatorFlow::FromUtf8(Identity.GivenName).TrimStartAndEnd();
    const FString Family = PWCreatorFlow::FromUtf8(Identity.FamilyName).TrimStartAndEnd();
    FString Full = Given;
    if (!Given.IsEmpty() && !Family.IsEmpty())
    {
        Full += TEXT(" ");
    }
    Full += Family;
    Summary.FullName = Full.IsEmpty() ? LOCTEXT("Unnamed", "Unnamed") : FText::FromString(Full);

    Summary.BirthDateText = FText::Format(LOCTEXT("BirthDateFmt", "{0} {1} {2}"), FText::AsNumber(Identity.Birth.Day, &FNumberFormattingOptions::DefaultNoGrouping()),
                                          FlowMonthName(Identity.Birth.Month),
                                          FText::AsNumber(Identity.Birth.Year, &FNumberFormattingOptions::DefaultNoGrouping()));
    Summary.AgeOnCampaignStart = static_cast<int32>(PerfectWorld::AgeOnCampaignStart(Identity.Birth));
    Summary.PronounsText = FlowTextOrNotSet(PWCreatorFlow::FromUtf8(Identity.Pronouns));
    Summary.LanguageText = FlowTextOrNotSet(PWCreatorFlow::FromUtf8(Identity.PrimaryLanguage));
    Summary.HomeCityText = FlowTextOrNotSet(PWCreatorFlow::FromUtf8(Identity.HomeCity));

    if (const PerfectWorld::SchoolStreamDefinition* Stream = PerfectWorld::FindSchoolStream(Creator.School().StreamId))
    {
        Summary.StreamName = FText::FromString(PWCreatorFlow::FromUtf8(Stream->DisplayName));
    }
    Summary.StreamReason = FText::FromString(PWCreatorFlow::FromUtf8(Creator.School().Reason));

    Summary.bGrewUpInCare = Home.bGrewUpInCare;
    Summary.HouseholdName = Home.HouseholdName;
    Summary.GuardianText = Home.bGrewUpInCare ? LOCTEXT("CareGuardian", "A children's-home staff caregiver")
                                              : FText::FromString(PWCreatorFlow::FromUtf8(Creator.Household().GuardianExamples));
    Summary.OpeningHomeText = FText::Format(LOCTEXT("OpeningHomeFmt", "{0} ({1}). {2}"), Home.HomeKitName,
                                            Home.DressingTierName, Home.DressingDescription);

    const auto& Definitions = PerfectWorld::CharacterCreator::TraitDefinitions();
    for (std::size_t Index = 0; Index < Review.Allocations.size() && Index < Definitions.size(); ++Index)
    {
        if (Review.Allocations[Index].Rank > 0)
        {
            FPWReviewTraitLine& Line = Summary.Traits.AddDefaulted_GetRef();
            Line.Id = FName(PWCreatorFlow::FromUtf8(Definitions[Index].Id));
            Line.DisplayName = FText::FromString(PWCreatorFlow::FromUtf8(Definitions[Index].DisplayName));
            Line.Rank = Review.Allocations[Index].Rank;
            Line.Cost = Review.Allocations[Index].Cost;
        }
    }

    if (Home.bGrewUpInCare)
    {
        Summary.CareNote = LOCTEXT("CareWarning",
            "Household Resources rank 0: you grew up in institutional care, in a children's home with no family guardian. "
            "Raise that trait to live with the household you chose instead.");
        Summary.Warnings.Add(Summary.CareNote);
    }
    if (Given.IsEmpty())
    {
        Summary.Warnings.Add(LOCTEXT("NoGivenName", "No given name entered yet: the character will appear as Unnamed."));
    }
    if (Creator.IdentityNeedsConfirmation())
    {
        Summary.Warnings.Add(LOCTEXT("MigratedIdentity",
            "This older save was given a default date of birth and school stream. Check them before you start."));
    }
    return Summary;
}

int32 UPerfectWorldCharacterCreatorSubsystem::FindFirstEmptySaveSlot()
{
    for (int32 Slot = 0; Slot < PerfectWorld::SaveSlotCount; ++Slot)
    {
        if (!IsSaveSlotOccupied(Slot))
        {
            return Slot;
        }
    }
    return -1;
}

int32 UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot()
{
    int32 Best = -1;
    FDateTime BestTime = FDateTime::MinValue();
    for (int32 Slot = 0; Slot < PerfectWorld::SaveSlotCount; ++Slot)
    {
        if (!IsSaveSlotOccupied(Slot))
        {
            continue;
        }
        // A slot whose primary was lost but whose backup survives still counts.
        const FString Path = GetSaveSlotPath(Slot);
        const FDateTime Written = FMath::Max(IFileManager::Get().GetTimeStamp(*Path),
                                             IFileManager::Get().GetTimeStamp(*(Path + TEXT(".bak"))));
        if (Best < 0 || Written > BestTime)
        {
            Best = Slot;
            BestTime = Written;
        }
    }
    return Best;
}

bool UPerfectWorldCharacterCreatorSubsystem::HasLifeStarted() const
{
    return Creator.HasStarted();
}

#undef LOCTEXT_NAMESPACE
