#include "PerfectWorldCreationAdapter.h"

#include "CreationSaveSerialization.h"
#include "PerfectWorldCharacterCreatorFlow.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"

#define LOCTEXT_NAMESPACE "PerfectWorldCreationAdapter"

namespace
{
FText HomeText(const PerfectWorld::OpeningHomePlan& Home)
{
    return FText::Format(LOCTEXT("OpeningHome", "{0} · {1}. {2}"),
        FText::FromString(PWCreatorFlow::FromUtf8(Home.HomeKitName)),
        FText::FromString(PWCreatorFlow::FromUtf8(Home.DressingTierName)),
        FText::FromString(PWCreatorFlow::FromUtf8(Home.DressingDescription)));
}

} // namespace

FText PWCreationPresentation::EffectText(const PerfectWorld::EffectLine& Line)
{
    FText Value;
    const FText Number = FText::AsNumber(Line.Value);
    switch (Line.Unit)
    {
    case PerfectWorld::EffectUnit::Percent:
        Value = Line.Direction == PerfectWorld::EffectDirection::Decrease
            ? FText::Format(LOCTEXT("PercentLower", "{0}% lower"), Number)
            : FText::Format(LOCTEXT("PercentIncrease", "+{0}%"), Number);
        break;
    case PerfectWorld::EffectUnit::Rupees: Value = FText::Format(LOCTEXT("RupeeValue", "₹{0}"), Number); break;
    case PerfectWorld::EffectUnit::Xp: Value = FText::Format(LOCTEXT("XpIncrease", "+{0} XP"), Number); break;
    case PerfectWorld::EffectUnit::Leads: Value = Number; break;
    }
    FText Text = FText::Format(LOCTEXT("EffectLine", "{0}: {1}."),
        FText::FromString(PWCreatorFlow::FromUtf8(Line.Subject)), Value);
    if (!Line.Note.empty())
        Text = FText::Format(LOCTEXT("EffectWithNote", "{0} {1}"), Text,
            FText::FromString(PWCreatorFlow::FromUtf8(Line.Note)));
    return Text;
}

TArray<FText> PWCreationPresentation::RankEffectTexts(const PerfectWorld::TraitRankEffect& Rank)
{
    TArray<FText> Lines;
    for (int32 Index = 0; Index < Rank.LineCount; ++Index) Lines.Add(EffectText(Rank.Lines[Index]));
    if (Lines.IsEmpty()) Lines.Add(LOCTEXT("Baseline", "No added starting effect. This does not prevent learning through play."));
    return Lines;
}

FText PWCreationPresentation::HouseholdEverydayText(FName HouseholdId)
{
    // TODO(core household presentation): move this authored spec copy into HouseholdDefinition.
    if (HouseholdId == FName(TEXT("parent_household"))) return LOCTEXT("ParentEveryday", "Work schedules and school plans share the same calendar.");
    if (HouseholdId == FName(TEXT("extended_family"))) return LOCTEXT("ExtendedEveryday", "Shared rooms mean making space for one another's plans.");
    if (HouseholdId == FName(TEXT("grandparent_household"))) return LOCTEXT("GrandparentEveryday", "Everyday care can flow in both directions.");
    if (HouseholdId == FName(TEXT("adult_sibling_household"))) return LOCTEXT("SiblingEveryday", "You and your adult sibling work out the routines of a shared home.");
    if (HouseholdId == FName(TEXT("appointed_guardian"))) return LOCTEXT("GuardianEveryday", "You and your guardian agree on routines and responsibilities.");
    return FText::GetEmpty();
}

namespace
{
FName ChangeMessageCode(PerfectWorld::CreationError Error, bool bTraitChange)
{
    switch (Error)
    {
    case PerfectWorld::CreationError::None: return NAME_None;
    case PerfectWorld::CreationError::BudgetExceeded: return TEXT("creation.budget_exceeded");
    case PerfectWorld::CreationError::TooManyRankFourTraits: return TEXT("creation.rank_four_limit");
    case PerfectWorld::CreationError::CreationAlreadyStarted: return TEXT("creation.already_started");
    default: return bTraitChange ? FName(TEXT("creation.trait_change_unexpected")) : FName(TEXT("creation.change_unexpected"));
    }
}

TMap<FName, FText> ChangeMessageArgs(int32 Needed, int32 Remaining)
{
    TMap<FName, FText> Args;
    Args.Add(TEXT("Needed"), FText::AsNumber(Needed));
    Args.Add(TEXT("Remaining"), FText::AsNumber(Remaining));
    Args.Add(TEXT("Maximum"), FText::AsNumber(PerfectWorld::MaxRankFourTraits));
    return Args;
}

FText ChangeErrorText(PerfectWorld::CreationError Error, int32 Needed, int32 Remaining, bool bTraitChange)
{
    switch (Error)
    {
    case PerfectWorld::CreationError::None: return FText::GetEmpty();
    case PerfectWorld::CreationError::BudgetExceeded:
        return FText::Format(LOCTEXT("NeedsPoints", "This change needs {0} points; {1} remain."),
            FText::AsNumber(Needed), FText::AsNumber(Remaining));
    case PerfectWorld::CreationError::TooManyRankFourTraits:
        return FText::Format(LOCTEXT("TooManyRankFour", "You can have at most {0} traits at rank 4. Lower another rank-4 trait first."),
            FText::AsNumber(PerfectWorld::MaxRankFourTraits));
    case PerfectWorld::CreationError::CreationAlreadyStarted:
        return LOCTEXT("CreationStarted", "This life has already started. Its starting background is fixed.");
    default:
        return bTraitChange
            ? LOCTEXT("TraitChangeUnexpected", "This trait couldn't be changed. Please reopen the screen.")
            : LOCTEXT("AllocationChangeUnexpected", "We couldn't complete that change. Please try again.");
    }
}

FPWAllocationPreview AdaptPreview(const PerfectWorld::CharacterCreator& Creator,
                                  const PerfectWorld::AllocationPreview& Source, FName PresetId)
{
    FPWAllocationPreview View;
    View.PresetId = PresetId;
    View.SourceDraftSnapshot = PWCreatorFlow::FromUtf8(PerfectWorld::SerializeCreationSave(Creator.CaptureSave()));
    const PerfectWorld::CreationReview Before = Creator.Review();
    const auto& Definitions = PerfectWorld::CharacterCreator::TraitDefinitions();
    for (std::size_t Index = 0; Index < Definitions.size(); ++Index)
    {
        View.SourceRanks.Add(Before.Allocations[Index].Rank);
        if (Before.Allocations[Index].Rank == Source.Review.Allocations[Index].Rank) continue;
        FPWAllocationChangeView& Change = View.Changes.AddDefaulted_GetRef();
        Change.TraitId = FName(PWCreatorFlow::FromUtf8(Definitions[Index].Id));
        Change.DisplayName = FText::FromString(PWCreatorFlow::FromUtf8(Definitions[Index].DisplayName));
        Change.BeforeRank = Before.Allocations[Index].Rank;
        Change.AfterRank = Source.Review.Allocations[Index].Rank;
    }
    for (int32 Rank = PerfectWorld::MaxRank; Rank >= 0 && View.StrongestTraits.Num() < 3; --Rank)
    {
        for (std::size_t Index = 0; Index < Definitions.size() && View.StrongestTraits.Num() < 3; ++Index)
        {
            if (Source.Review.Allocations[Index].Rank != Rank) continue;
            FPWAllocationChangeView& Trait = View.StrongestTraits.AddDefaulted_GetRef();
            Trait.TraitId = FName(PWCreatorFlow::FromUtf8(Definitions[Index].Id));
            Trait.DisplayName = FText::FromString(PWCreatorFlow::FromUtf8(Definitions[Index].DisplayName));
            Trait.BeforeRank = Before.Allocations[Index].Rank;
            Trait.AfterRank = Rank;
        }
    }
    View.Spent = Source.Review.Spent;
    View.TotalBudget = Source.Review.TotalBudget;
    View.Remaining = Source.Review.Remaining;
    View.RankFourCount = Source.Review.RankFourCount;
    View.MaxRankFour = PerfectWorld::MaxRankFourTraits;
    View.bCanApply = Source.Result.Succeeded();
    View.ErrorCode = static_cast<int32>(Source.Result.Error);
    View.MessageCode = ChangeMessageCode(Source.Result.Error, false);
    View.MessageArgs = ChangeMessageArgs(0, Before.Remaining);
    View.Message = ChangeErrorText(Source.Result.Error, 0, Before.Remaining, false);
    View.OpeningHome = HomeText(Source.Home);
    View.bGrewUpInCare = Source.Home.GrewUpInCare;
    return View;
}
} // namespace

TArray<FPWTraitGroupView> UPerfectWorldCharacterCreatorSubsystem::GetTraitGroups() const
{
    TMap<FName, int32> Totals;
    for (const FPWTraitView& Trait : GetTraits()) Totals.FindOrAdd(Trait.Group) += Trait.Cost;
    TArray<FPWTraitGroupView> Views;
    for (const PerfectWorld::TraitGroup& Group : PerfectWorld::TraitGroups())
    {
        FPWTraitGroupView& View = Views.AddDefaulted_GetRef();
        View.Id = FName(PWCreatorFlow::FromUtf8(Group.Id));
        View.DisplayName = FText::FromString(PWCreatorFlow::FromUtf8(Group.DisplayName));
        View.DisplayOrder = Group.DisplayOrder;
        View.AllocatedPoints = Totals.FindRef(View.Id);
    }
    return Views;
}

FPWTraitDefinitionView UPerfectWorldCharacterCreatorSubsystem::GetTraitDefinition(FName TraitId) const
{
    FPWTraitDefinitionView View;
    const FString Requested = TraitId.ToString();
    const std::string Id = PWCreatorFlow::ToUtf8(Requested);
    const PerfectWorld::TraitPresentationEntry* Presentation = PerfectWorld::TraitPresentation(Id);
    if (!Presentation) return View;
    for (const PerfectWorld::TraitDefinition& Definition : PerfectWorld::CharacterCreator::TraitDefinitions())
    {
        if (Definition.Id != Id) continue;
        View.Id = TraitId;
        View.DisplayName = FText::FromString(PWCreatorFlow::FromUtf8(Definition.DisplayName));
        View.GroupId = FName(PWCreatorFlow::FromUtf8(Presentation->GroupId));
        View.ShortDescription = FText::FromString(PWCreatorFlow::FromUtf8(Presentation->Description));
        break;
    }
    for (const PerfectWorld::TraitRankEffect& Rank : Presentation->Ranks)
    {
        FPWTraitRankView& RankView = View.Ranks.AddDefaulted_GetRef();
        RankView.Rank = Rank.Rank;
        RankView.CumulativeCost = PerfectWorld::CumulativeCostAtRank(Rank.Rank);
        RankView.Meaning = FText::FromString(PWCreatorFlow::FromUtf8(Rank.Meaning));
        RankView.EffectLines = PWCreationPresentation::RankEffectTexts(Rank);
        for (int32 Index = 0; Index < Rank.LineCount; ++Index)
        {
            if (!Rank.Lines[Index].Note.empty())
                RankView.AvailabilityNote = FText::FromString(PWCreatorFlow::FromUtf8(Rank.Lines[Index].Note));
        }
    }
    return View;
}

FPWTraitChangePreview UPerfectWorldCharacterCreatorSubsystem::PreviewTraitRank(FName TraitId, int32 ProposedRank) const
{
    FPWTraitChangePreview View;
    View.TraitId = TraitId;
    View.SourceDraftSnapshot = PWCreatorFlow::FromUtf8(PerfectWorld::SerializeCreationSave(Creator.CaptureSave()));
    const std::string Id = PWCreatorFlow::ToUtf8(TraitId.ToString());
    View.CurrentRank = Creator.GetTraitRank(Id);
    View.ProposedRank = ProposedRank;
    const PerfectWorld::RankChange Change = PerfectWorld::CostOfRankChange(View.CurrentRank, ProposedRank);
    View.CurrentCost = Change.FromCumulative;
    View.ProposedCost = Change.ToCumulative;
    View.Charge = FMath::Max(Change.Delta, 0);
    View.Refund = FMath::Max(-Change.Delta, 0);
    PerfectWorld::CharacterCreator Probe = Creator;
    const PerfectWorld::OperationResult Verdict = Probe.SetTraitRank(Id, ProposedRank);
    for (const PerfectWorld::TraitAllocation& Allocation : Creator.Review().Allocations)
        View.SourceRanks.Add(Allocation.Rank);
    View.bCanApply = Verdict.Succeeded() && View.CurrentRank != ProposedRank;
    View.ErrorCode = static_cast<int32>(Verdict.Error);
    View.MessageCode = ChangeMessageCode(Verdict.Error, true);
    View.MessageArgs = ChangeMessageArgs(View.Charge, Creator.Review().Remaining);
    View.Message = ChangeErrorText(Verdict.Error, View.Charge, Creator.Review().Remaining, true);
    View.OpeningHome = HomeText(Probe.ResolveOpeningHome());
    return View;
}

FPWAllocationPreview UPerfectWorldCharacterCreatorSubsystem::PreviewPreset(FName PresetId) const
{
    return AdaptPreview(Creator, Creator.PreviewPreset(PWCreatorFlow::ToUtf8(PresetId.ToString())), PresetId);
}

FPWAllocationPreview UPerfectWorldCharacterCreatorSubsystem::PreviewClearAllocation() const
{
    return AdaptPreview(Creator, Creator.PreviewClear(), TEXT("clear"));
}

FPWAllocationPreview UPerfectWorldCharacterCreatorSubsystem::PreviewResetAllocation() const
{
    return AdaptPreview(Creator, Creator.PreviewReset(), TEXT("reset"));
}

FPWAllocationHistory UPerfectWorldCharacterCreatorSubsystem::GetAllocationHistory() const
{
    FPWAllocationHistory View;
    View.bCanUndo = Creator.CanUndo();
    View.bCanRedo = Creator.CanRedo();
    return View;
}

bool UPerfectWorldCharacterCreatorSubsystem::ClearAllocation() { return RecordResult(Creator.ClearAllocation()); }
bool UPerfectWorldCharacterCreatorSubsystem::ResetAllocation() { return RecordResult(Creator.ResetAllocation()); }
bool UPerfectWorldCharacterCreatorSubsystem::UndoAllocation() { return RecordResult(Creator.Undo()); }
bool UPerfectWorldCharacterCreatorSubsystem::RedoAllocation() { return RecordResult(Creator.Redo()); }

#undef LOCTEXT_NAMESPACE
