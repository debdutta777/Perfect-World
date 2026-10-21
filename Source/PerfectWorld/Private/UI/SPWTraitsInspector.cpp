#include "UI/SPWTraitsPage.h"

#include "UI/PWCreationModel.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldTraitsInspector"

TSharedRef<SWidget> SPWTraitsPage::BuildRank(const FPWTraitRankView& Rank, const FText& Label)
{
    const auto& Style = PWUI::FStyle::Get();
    const auto Column = SNew(SVerticalBox);
    Column->AddSlot().AutoHeight()[PWUI::MakeText(Label, 16, Style.TextSecondary, true)];
    Column->AddSlot().AutoHeight().Padding(0, 4, 0, 8)
    [PWUI::MakeText(FText::Format(LOCTEXT("Meaning", "Rank {0}: {1}"), FText::AsNumber(Rank.Rank), Rank.Meaning), 18, Style.TextPrimary, true, true)];
    for (const FText& Line : Rank.EffectLines)
        Column->AddSlot().AutoHeight().Padding(0, 0, 0, 6)[PWUI::MakeText(Line, 16, Style.TextPrimary, false, true)];
    if (Rank.EffectLines.IsEmpty()) Column->AddSlot().AutoHeight()
    [PWUI::MakeText(LOCTEXT("Baseline", "No added starting effect. This does not prevent learning through play."), 16, Style.TextPrimary, false, true)];
    Column->AddSlot().AutoHeight().Padding(0, 6, 0, 0)
    [PWUI::MakeText(FText::Format(LOCTEXT("Cost", "{0} points allocated"), FText::AsNumber(Rank.CumulativeCost)), 14, Style.TextSecondary)];
    return Column;
}

TSharedRef<SWidget> SPWTraitsPage::BuildInspector()
{
    const auto* Creator = Model->GetSubsystem();
    if (!Creator || !Model->Traits.IsValidIndex(SelectedIndex)) return SNew(SBox);
    const auto Definition = Creator->GetTraitDefinition(Model->Traits[SelectedIndex].Id);
    const auto& Style = PWUI::FStyle::Get();
    const auto Content = SNew(SVerticalBox);
    Content->AddSlot().AutoHeight()[PWUI::MakeText(Definition.DisplayName, 24, Style.TextPrimary, true, true)];
    Content->AddSlot().AutoHeight().Padding(0, 8, 0, 12)
    [PWUI::MakeText(LOCTEXT("Compare", "Current → Proposed"), 16, Style.TextSecondary, true)];
    if (Definition.Ranks.IsValidIndex(Proposal.CurrentRank) && Definition.Ranks.IsValidIndex(Proposal.ProposedRank))
    {
        if (bNarrow) Content->AddSlot().AutoHeight()
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[BuildRank(Definition.Ranks[Proposal.CurrentRank], LOCTEXT("Current", "Current"))]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 0)[BuildRank(Definition.Ranks[Proposal.ProposedRank], LOCTEXT("Proposed", "Proposed"))]];
        else Content->AddSlot().AutoHeight()
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 12, 0)[BuildRank(Definition.Ranks[Proposal.CurrentRank], LOCTEXT("Current", "Current"))]
            + SHorizontalBox::Slot().FillWidth(1)[BuildRank(Definition.Ranks[Proposal.ProposedRank], LOCTEXT("Proposed", "Proposed"))]];
    }
    Content->AddSlot().AutoHeight().Padding(0, 16, 0, 8)
    [PWUI::MakeText(FText::Format(LOCTEXT("Allocation", "Allocated: {0} → {1} points"), FText::AsNumber(Proposal.CurrentCost), FText::AsNumber(Proposal.ProposedCost)), 16, Style.TextSecondary, false, true)];
    FText Change = LOCTEXT("Unchanged", "No change");
    if (Proposal.Charge > 0) Change = FText::Format(LOCTEXT("Charge", "Costs {0} points"), FText::AsNumber(Proposal.Charge));
    else if (Proposal.Refund > 0) Change = FText::Format(LOCTEXT("Refund", "Returns {0} points"), FText::AsNumber(Proposal.Refund));
    Content->AddSlot().AutoHeight()[PWUI::MakeText(Change, 18, Style.TextPrimary, true, true)];
    Content->AddSlot().AutoHeight().Padding(0, 8, 0, 8)[PWUI::MakeText(Proposal.Message, 16, Style.TextPrimary, false, true)];
    Content->AddSlot().AutoHeight()
    [PWUI::MakeButton(LOCTEXT("Apply", "Apply rank"), PWUI::EButtonKind::Primary,
        FOnClicked::CreateLambda([this]() { ApplyRank(); return FReply::Handled(); }), Proposal.bCanApply, 16)];
    LadderToggle = PWUI::MakeButton(bShowRanks ? LOCTEXT("Hide", "Hide all ranks") : LOCTEXT("Show", "Show all ranks"), PWUI::EButtonKind::Secondary,
        FOnClicked::CreateLambda([this]()
        {
            bShowRanks = !bShowRanks;
            RefreshInspector();
            PWUI::FocusWhenReady(AsShared(), LadderToggle);
            return FReply::Handled();
        }), true, 16);
    Content->AddSlot().AutoHeight().Padding(0, 8, 0, 8)[LadderToggle.ToSharedRef()];
    if (bShowRanks) for (const auto& Rank : Definition.Ranks)
        Content->AddSlot().AutoHeight().Padding(0, 8, 0, 8)[BuildRank(Rank, FText::GetEmpty())];
    if (Definition.Id == FName(TEXT("household_resources")))
        Content->AddSlot().AutoHeight().Padding(0, 8, 0, 8)[PWUI::MakeText(Proposal.OpeningHome, 16, Style.TextPrimary, false, true)];
    Content->AddSlot().AutoHeight().Padding(0, 8, 0, 0)
    [PWUI::MakeText(LOCTEXT("Freedom", "These are starting circumstances. They do not choose your career or supernatural ability."), 14, Style.TextSecondary, false, true)];
    return PWUI::MakePanel(Content, FMargin(16));
}

void SPWTraitsPage::RefreshInspector()
{
    if (Inspector.IsValid())
    {
        Inspector->SetContent(BuildInspector());
        PWUI::PlayMotion(InspectorTransition, AsShared());
    }
}

TSharedRef<SWidget> SPWTraitsPage::BuildBudget()
{
    const auto& Style = PWUI::FStyle::Get();
    const auto Row = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(16, 6));
    Row->AddSlot()[PWUI::MakeText(TAttribute<FText>::CreateLambda([this]()
    { return FText::Format(LOCTEXT("Remaining", "{0} points remaining"), FText::AsNumber(Model->Summary.Remaining)); }), 18, Style.TextPrimary, true)];
    Row->AddSlot()[PWUI::MakeText(TAttribute<FText>::CreateLambda([this]()
    { return FText::Format(LOCTEXT("Spent", "{0} / {1} points allocated"), FText::AsNumber(Model->Summary.Spent), FText::AsNumber(Model->Summary.TotalBudget)); }), 16, Style.TextSecondary)];
    Row->AddSlot()[PWUI::MakeText(TAttribute<FText>::CreateLambda([this]()
    { return FText::Format(LOCTEXT("Slots", "Rank 4: {0} / {1}"), FText::AsNumber(Model->Summary.RankFourCount), FText::AsNumber(Model->Summary.MaxRankFour)); }), 16, Style.TextSecondary)];
    for (int32 Slot = 1; Slot <= Model->Summary.MaxRankFour; ++Slot)
    {
        const auto Mark = PWUI::MakeText(TAttribute<FText>::CreateLambda([this, Slot]()
        { return FText::Format(LOCTEXT("SlotMark", "{0} {1}"), FText::AsNumber(Slot), Slot <= Model->Summary.RankFourCount ? LOCTEXT("UsedMark", "●") : LOCTEXT("AvailableMark", "○")); }), 14, Style.TextPrimary);
        Mark->SetToolTipText(TAttribute<FText>::CreateLambda([this, Slot]()
        { return FText::Format(LOCTEXT("Slot", "Rank 4 slot {0}: {1}"), FText::AsNumber(Slot), Slot <= Model->Summary.RankFourCount ? LOCTEXT("Used", "Used") : LOCTEXT("Available", "Available")); }));
#if WITH_ACCESSIBILITY
        Mark->SetAccessibleBehavior(EAccessibleBehavior::Custom, TAttribute<FText>::CreateLambda([this, Slot]()
        { return FText::Format(LOCTEXT("Slot", "Rank 4 slot {0}: {1}"), FText::AsNumber(Slot), Slot <= Model->Summary.RankFourCount ? LOCTEXT("Used", "Used") : LOCTEXT("Available", "Available")); }));
#endif
        Row->AddSlot()[Mark];
    }
    Row->AddSlot()[PWUI::MakeText(TAttribute<FText>::CreateLambda([this]()
    { return Model->Summary.Remaining == 0 ? LOCTEXT("All", "✓ All points allocated") : FText::GetEmpty(); }), 16, Style.TextPrimary)];
    // The narrow inspector can be taller than the body viewport. Keep its action available beside the budget.
    Row->AddSlot()[SNew(SBox).Visibility_Lambda([this]() { return bNarrow ? EVisibility::Visible : EVisibility::Collapsed; })
        [PWUI::MakeButton(LOCTEXT("StickyApply", "Apply rank"), PWUI::EButtonKind::Primary,
            FOnClicked::CreateLambda([this]() { ApplyRank(); return FReply::Handled(); }),
            TAttribute<bool>::CreateLambda([this]() { return Proposal.bCanApply; }), 16)]];
    Row->AddSlot()[SNew(SBox).Visibility_Lambda([this]() { return bNarrow ? EVisibility::Visible : EVisibility::Collapsed; })
        [PWUI::MakeText(TAttribute<FText>::CreateLambda([this]()
        {
            if (!Proposal.Message.IsEmpty()) return Proposal.Message;
            if (Proposal.Charge > 0) return FText::Format(LOCTEXT("StickyCharge", "Costs {0} points"), FText::AsNumber(Proposal.Charge));
            if (Proposal.Refund > 0) return FText::Format(LOCTEXT("StickyRefund", "Returns {0} points"), FText::AsNumber(Proposal.Refund));
            return LOCTEXT("StickyNoChange", "No change");
        }), 14, Style.TextSecondary, false, true)]];
    return SAssignNew(BudgetBox, SBox).MinDesiredHeight(56)[PWUI::MakePanel(Row, FMargin(16, 8))];
}

#undef LOCTEXT_NAMESPACE
