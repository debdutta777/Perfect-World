#include "UI/SPWTraitsPage.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "Input/NavigationReply.h"
#include "InputCoreTypes.h"
#include "UI/PWCreationModel.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldTraitsDrawer"

namespace
{
class SPWDrawerScroll final : public SScrollBox
{
public:
    virtual FReply OnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event) override
    {
        if (Event.GetKey() == EKeys::Gamepad_LeftX || Event.GetKey() == EKeys::Gamepad_LeftY)
            return FReply::Unhandled();
        return SScrollBox::OnAnalogValueChanged(Geometry, Event);
    }

    virtual FNavigationReply OnNavigation(const FGeometry& Geometry, const FNavigationEvent& Event) override
    {
        // The flow owns the explicit preset -> preview -> actions sequence.
        return SCompoundWidget::OnNavigation(Geometry, Event);
    }
};
}

void SPWTraitsPage::OpenDrawer(FName Preset, int32 Operation)
{
    if (!OnOpenModal.IsBound()) return;
    DiscardProposal();
    DrawerPreset = Preset;
    DrawerOperation = Operation;
    DrawerPreview = SNew(SBox);
    const auto& Style = PWUI::FStyle::Get();
    const auto Content = SNew(SVerticalBox);
    Content->AddSlot().AutoHeight()[PWUI::MakeText(Operation == 0 ? LOCTEXT("Title", "Choose a starting background") : LOCTEXT("Changes", "Apply changes"), 24, Style.TextPrimary, true, true)];
    Content->AddSlot().AutoHeight().Padding(0, 8, 0, 16)
    [PWUI::MakeText(Operation == 1 ? LOCTEXT("Clear", "All traits will be rank 0. Your opening home will be a children's home. Allocate 1,000 points before starting.") : LOCTEXT("Intro", "Start with an allocation, then make it your own."), 16, Style.TextSecondary, false, true)];
    const auto ScrollContent = SNew(SVerticalBox);
    if (Operation == 0) for (const auto& Entry : Model->Presets)
    {
        const FName Id = Entry.Id;
        ScrollContent->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
        [PWUI::MakeButton(Entry.DisplayName, PWUI::EButtonKind::Chip,
            FOnClicked::CreateLambda([this, Id]() { DrawerPreset = Id; RefreshDrawer(); return FReply::Handled(); }), true, 16, 0,
            TAttribute<bool>::CreateLambda([this, Id]() { return DrawerPreset == Id; }))];
    }
    ScrollContent->AddSlot().AutoHeight().Padding(0, 12, 0, 0)[DrawerPreview.ToSharedRef()];
    Content->AddSlot().FillHeight(1)
    [SNew(SPWDrawerScroll).ScrollBarStyle(&Style.ScrollBar).AllowOverscroll(EAllowOverscroll::No)
        .NavigationDestination(EDescendantScrollDestination::IntoView).ScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll)
        + SScrollBox::Slot()[ScrollContent]];
    const auto Cancel = PWUI::MakeButton(LOCTEXT("Cancel", "Cancel"), PWUI::EButtonKind::Secondary,
        FOnClicked::CreateLambda([this]() { CloseDrawer(); return FReply::Handled(); }), true, 18);
    Content->AddSlot().AutoHeight().Padding(0, 16, 0, 0)
    [SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 12, 0)[Cancel]
        + SHorizontalBox::Slot().FillWidth(1)
        [PWUI::MakeButton(Operation == 0 ? LOCTEXT("ApplyBackground", "Apply background") : LOCTEXT("ApplyChanges", "Apply changes"), PWUI::EButtonKind::Primary,
            FOnClicked::CreateLambda([this]() { ApplyDrawer(); return FReply::Handled(); }),
            TAttribute<bool>::CreateLambda([this]() { return Allocation.bCanApply; }), 18)]];
    RefreshDrawer();
    OnOpenModal.Execute(PWUI::MakeDialog(Content), Cancel);
}

void SPWTraitsPage::RefreshDrawer()
{
    const auto* Creator = Model->GetSubsystem();
    if (!Creator || !DrawerPreview.IsValid()) return;
    Allocation = DrawerOperation == 1 ? Creator->PreviewClearAllocation() : DrawerOperation == 2 ? Creator->PreviewResetAllocation() : Creator->PreviewPreset(DrawerPreset);
    const auto& Style = PWUI::FStyle::Get();
    const auto Content = SNew(SVerticalBox);
    Content->AddSlot().AutoHeight()[PWUI::MakeText(LOCTEXT("Strongest", "Strongest backgrounds"), 20, Style.TextPrimary, true)];
    for (const auto& Trait : Allocation.StrongestTraits)
        Content->AddSlot().AutoHeight().Padding(0, 6, 0, 0)
        [PWUI::MakeText(FText::Format(LOCTEXT("Strength", "{0}: rank {1}"), Trait.DisplayName, FText::AsNumber(Trait.AfterRank)), 16, Style.TextSecondary, false, true)];
    Content->AddSlot().AutoHeight().Padding(0, 16, 0, 6)[PWUI::MakeText(LOCTEXT("Difference", "Changes from your allocation"), 20, Style.TextPrimary, true, true)];
    if (Allocation.Changes.IsEmpty()) Content->AddSlot().AutoHeight()[PWUI::MakeText(LOCTEXT("NoChange", "No allocation changes"), 16, Style.TextSecondary)];
    for (const auto& Change : Allocation.Changes)
        Content->AddSlot().AutoHeight().Padding(0, 4, 0, 0)
        [PWUI::MakeText(FText::Format(LOCTEXT("Change", "{0}: rank {1} → {2}"), Change.DisplayName, FText::AsNumber(Change.BeforeRank), FText::AsNumber(Change.AfterRank)), 16, Style.TextSecondary, false, true)];
    Content->AddSlot().AutoHeight().Padding(0, 16, 0, 6)[PWUI::MakeText(LOCTEXT("Home", "Your opening home"), 20, Style.TextPrimary, true)];
    Content->AddSlot().AutoHeight()[PWUI::MakeText(Allocation.OpeningHome, 16, Style.TextPrimary, false, true)];
    if (Allocation.bGrewUpInCare) Content->AddSlot().AutoHeight().Padding(0, 8, 0, 0)
    [PWUI::MakeText(LOCTEXT("Care", "This background starts with Household Resources at rank 0: you grew up in a children's home."), 16, Style.TextPrimary, false, true)];
    Content->AddSlot().AutoHeight().Padding(0, 16, 0, 6)
    [PWUI::MakeText(FText::Format(LOCTEXT("Totals", "{0} / {1} points allocated · {2} points remaining · Rank 4: {3} / {4}"),
        FText::AsNumber(Allocation.Spent), FText::AsNumber(Allocation.TotalBudget), FText::AsNumber(Allocation.Remaining),
        FText::AsNumber(Allocation.RankFourCount), FText::AsNumber(Allocation.MaxRankFour)), 16, Style.TextSecondary, false, true)];
    Content->AddSlot().AutoHeight()[PWUI::MakeText(Allocation.Message, 16, Style.TextPrimary, false, true)];
    DrawerPreview->SetContent(Content);
}

void SPWTraitsPage::ApplyDrawer()
{
    // Re-preview against the live draft; an old preview is never applied silently.
    const auto Previous = Allocation;
    RefreshDrawer();
    if (Previous.SourceDraftSnapshot != Allocation.SourceDraftSnapshot || !Allocation.bCanApply) return;
    const auto Preset = DrawerPreset;
    const auto Operation = DrawerOperation;
    const bool bApplied = Model->Edit([Preset, Operation](UPerfectWorldCharacterCreatorSubsystem& Creator)
    { return Operation == 1 ? Creator.ClearAllocation() : Operation == 2 ? Creator.ResetAllocation() : Creator.ApplyPreset(Preset); });
    if (!bApplied) { RefreshDrawer(); return; }
    if (Operation == 0) for (const auto& Entry : Model->Presets) if (Entry.Id == Preset)
        Model->Message = FText::Format(LOCTEXT("Applied", "{0} applied. You can undo this change."), Entry.DisplayName);
    DiscardProposal();
    CloseDrawer();
}

void SPWTraitsPage::CloseDrawer()
{
    OnCloseModal.ExecuteIfBound();
    DrawerPreview.Reset();
}

#undef LOCTEXT_NAMESPACE
