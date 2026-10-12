#include "UI/SPWTraitsPage.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "UI/PWCreationModel.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "UI/SPWRankSelector.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldTraitsPage"

void SPWTraitsPage::Construct(const FArguments& InArgs, const TSharedRef<FPWCreationModel>& InModel)
{
    Model = InModel;
    NarrowLayout = InArgs._NarrowLayout;
    InspectorReveal = InspectorTransition.AddCurve(0, .16f, ECurveEaseFunction::CubicOut);
    OnOpenModal = InArgs._OnOpenModal;
    OnCloseModal = InArgs._OnCloseModal;
    OnFocusFooter = InArgs._OnFocusFooter;
    OnRevealInspector = InArgs._OnRevealInspector;
    if (!Model->Traits.IsEmpty()) Group = Model->Traits[0].Group;
    Inspector = SNew(SBox);
    Body = SNew(SBox);
    Selectors.SetNum(Model->Traits.Num());
    for (int32 Index = 0; Index < Model->Traits.Num(); ++Index) Rows.Add(BuildRow(Index));
    Toolbar = BuildToolbar();
    const auto& Style = PWUI::FStyle::Get();
    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [PWUI::MakeText(LOCTEXT("Title", "What is your background?"), PWUI::ETextRole::PageHeading, Style.TextPrimary)]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 16)
        [PWUI::MakeText(LOCTEXT("Intro", "Spend exactly 1,000 points across twenty traits. You can change any choice before starting."), 16, Style.TextSecondary, false, true)]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)[Toolbar.ToSharedRef()]
        + SVerticalBox::Slot().AutoHeight()[Body.ToSharedRef()]
    ];
    DiscardProposal();
    Reflow();
}

void SPWTraitsPage::Tick(const FGeometry& Geometry, double Time, float Delta)
{
    SCompoundWidget::Tick(Geometry, Time, Delta);
    // Descendant rank positions may receive focus directly from Tab, mouse or accessibility.
    for (int32 Index = 0; Index < Selectors.Num(); ++Index)
        if (Selectors[Index].IsValid() && (Selectors[Index]->HasKeyboardFocus() || Selectors[Index]->HasFocusedDescendants()))
        { SelectTrait(Index); break; }
    Inspector->SetRenderOpacity(InspectorReveal.GetLerp());
    const bool bNewNarrow = NarrowLayout.Get(Geometry.GetLocalSize().X < 1280.0f);
    if (bNewNarrow != bNarrow)
    {
        const auto Focus = FSlateApplication::Get().GetKeyboardFocusedWidget();
        const bool bNavFocused = Navigator.IsValid() && (Navigator->HasKeyboardFocus() || Navigator->HasFocusedDescendants());
        const bool bInspectorFocused = Inspector->HasFocusedDescendants();
        bNarrow = bNewNarrow;
        RefreshInspector();
        Reflow();
        if (bNavFocused) PWUI::FocusWhenReady(AsShared(), Navigator);
        else if (bInspectorFocused) PWUI::FocusWhenReady(AsShared(), Inspector);
        else if (Focus.IsValid() && Focus->IsEnabled()) PWUI::FocusWhenReady(AsShared(), Focus);
    }
}

TSharedRef<SWidget> SPWTraitsPage::BuildToolbar()
{
    const auto Row = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8, 8));
    Row->AddSlot().VAlign(VAlign_Center)[PWUI::MakeText(LOCTEXT("InputHints", "← / → Change rank · Enter / A Apply rank · Esc / B Back"), 12, PWUI::FStyle::Get().TextSecondary, false, true)];
    Row->AddSlot()[PWUI::MakeButton(LOCTEXT("Background", "Choose a starting background"), PWUI::EButtonKind::Secondary,
        FOnClicked::CreateLambda([this]() { OpenDrawer(Model->Presets.IsEmpty() ? NAME_None : Model->Presets[0].Id, 0); return FReply::Handled(); }), true, 16)];
    Row->AddSlot().VAlign(VAlign_Center)[PWUI::MakeText(TAttribute<FText>::CreateLambda([this]()
    {
        for (const auto& Preset : Model->Presets) if (Preset.bMatchesCurrent) return Preset.DisplayName;
        return LOCTEXT("Custom", "Custom");
    }), 16, PWUI::FStyle::Get().TextSecondary)];
    for (bool bRedo : {false, true})
    {
        Row->AddSlot()[PWUI::MakeButton(bRedo ? LOCTEXT("Redo", "Redo") : LOCTEXT("Undo", "Undo"), PWUI::EButtonKind::Secondary,
            FOnClicked::CreateLambda([this, bRedo]() { History(bRedo); return FReply::Handled(); }),
            TAttribute<bool>::CreateLambda([this, bRedo]()
            {
                const auto* Creator = Model->GetSubsystem();
                if (!Creator) return false;
                const auto State = Creator->GetAllocationHistory();
                return bRedo ? State.bCanRedo : State.bCanUndo;
            }), 16)];
    }
    Row->AddSlot()
    [
        SAssignNew(MoreMenu, SComboButton)
        .ButtonStyle(&PWUI::FStyle::Get().NoButton)
        .ButtonContent()[PWUI::MakeText(LOCTEXT("More", "More"), 16, PWUI::FStyle::Get().TextPrimary)]
        .OnGetMenuContent_Lambda([this]() -> TSharedRef<SWidget>
        {
            return SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[PWUI::MakeButton(LOCTEXT("Clear", "Clear allocation"), PWUI::EButtonKind::TextRow,
                    FOnClicked::CreateLambda([this]() { FSlateApplication::Get().DismissAllMenus(); PWUI::FocusFirstFocusable(MoreMenu.ToSharedRef()); OpenDrawer(NAME_None, 1); return FReply::Handled(); }), true, 16)]
                + SVerticalBox::Slot().AutoHeight()[PWUI::MakeButton(LOCTEXT("Reset", "Reset to Balanced Scholar"), PWUI::EButtonKind::TextRow,
                    FOnClicked::CreateLambda([this]() { FSlateApplication::Get().DismissAllMenus(); PWUI::FocusFirstFocusable(MoreMenu.ToSharedRef()); OpenDrawer(NAME_None, 2); return FReply::Handled(); }), true, 16)];
        })
    ];
    Row->AddSlot().VAlign(VAlign_Center)[PWUI::MakeText(TAttribute<FText>::CreateLambda([this]()
    {
        const auto* Creator = Model->GetSubsystem();
        if (!Creator) return LOCTEXT("HistoryUnavailable", "Allocation history is unavailable.");
        if (Model->Summary.bHasStarted) return LOCTEXT("FixedBackground", "This life has already started. Its starting background is fixed.");
        const auto HistoryState = Creator->GetAllocationHistory();
        if (!HistoryState.bCanUndo && !HistoryState.bCanRedo) return LOCTEXT("NoHistory", "Undo and Redo: no allocation changes yet.");
        if (!HistoryState.bCanUndo) return LOCTEXT("NoUndo", "Undo: no earlier allocation.");
        if (!HistoryState.bCanRedo) return LOCTEXT("NoRedo", "Redo: no later allocation.");
        return FText::GetEmpty();
    }), 12, PWUI::FStyle::Get().TextSecondary, false, true)];
    return Row;
}

TSharedRef<SWidget> SPWTraitsPage::BuildNavigator()
{
    const auto List = SNew(SVerticalBox);
    const auto* Creator = Model->GetSubsystem();
    if (Creator) for (const auto& Entry : Creator->GetTraitGroups())
    {
        const FName Id = Entry.Id;
        List->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
        [PWUI::MakeButton(TAttribute<FText>::CreateLambda([this, Id]()
        {
            if (const auto* Source = Model->GetSubsystem()) for (const auto& Item : Source->GetTraitGroups())
                if (Item.Id == Id) return FText::Format(LOCTEXT("Group", "{0}\n{1} points"), Item.DisplayName, FText::AsNumber(Item.AllocatedPoints));
            return FText::GetEmpty();
        }), PWUI::EButtonKind::Chip, FOnClicked::CreateLambda([this, Id]()
        {
            Group = Id;
            FSlateApplication::Get().DismissAllMenus();
            for (int32 Index = 0; Index < Model->Traits.Num(); ++Index) if (Model->Traits[Index].Group == Id) { SelectedIndex = Index; break; }
            DiscardProposal();
            Reflow();
            PWUI::FocusWhenReady(AsShared(), Selectors[SelectedIndex]);
            return FReply::Handled();
        }), true, 14, 0, TAttribute<bool>::CreateLambda([this, Id]() { return Group == Id; }))];
    }
    if (!bNarrow) return List;
    return SNew(SComboButton).ButtonStyle(&PWUI::FStyle::Get().NoButton)
        .ButtonContent()[PWUI::MakeText(TAttribute<FText>::CreateLambda([this]()
        {
            if (const auto* Source = Model->GetSubsystem()) for (const auto& Item : Source->GetTraitGroups())
                if (Item.Id == Group) return FText::Format(LOCTEXT("Selector", "{0} · {1} points"), Item.DisplayName, FText::AsNumber(Item.AllocatedPoints));
            return FText::GetEmpty();
        }), 16, PWUI::FStyle::Get().TextPrimary)]
        .MenuContent()[List];
}

void SPWTraitsPage::Reflow()
{
    // Detach the single inspector before moving it between the column and a row.
    Body->SetContent(SNullWidget::NullWidget);
    Navigator = BuildNavigator();
    const auto List = SNew(SVerticalBox);
    for (int32 Index = 0; Index < Rows.Num(); ++Index) if (Model->Traits[Index].Group == Group)
    {
        List->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[Rows[Index].ToSharedRef()];
        if (bNarrow && Index == SelectedIndex) List->AddSlot().AutoHeight().Padding(0, 0, 0, 16)[Inspector.ToSharedRef()];
    }
    if (bNarrow) Body->SetContent(SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)[Navigator.ToSharedRef()]
        + SVerticalBox::Slot().AutoHeight()[List]);
    else Body->SetContent(SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(.18f).Padding(0, 0, 24, 0)[Navigator.ToSharedRef()]
        + SHorizontalBox::Slot().FillWidth(.47f).Padding(0, 0, 24, 0)[List]
        + SHorizontalBox::Slot().FillWidth(.35f)[Inspector.ToSharedRef()]);
}

TSharedRef<SWidget> SPWTraitsPage::BuildRow(int32 Index)
{
    const auto& Trait = Model->Traits[Index];
    const auto* Creator = Model->GetSubsystem();
    const auto Definition = Creator ? Creator->GetTraitDefinition(Trait.Id) : FPWTraitDefinitionView{};
    const auto& Style = PWUI::FStyle::Get();
    return PWUI::MakePanel(SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[PWUI::MakeText(Trait.DisplayName, 20, Style.TextPrimary, true, true)]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 8)[PWUI::MakeText(Definition.ShortDescription, 14, Style.TextSecondary, false, true)]
        + SVerticalBox::Slot().AutoHeight()
        [SAssignNew(Selectors[Index], SPWRankSelector)
            .CurrentRank_Lambda([this, Index]() { return Model->Traits[Index].Rank; })
            .ProposedRank_Lambda([this, Index]() { return Index == SelectedIndex ? Proposal.ProposedRank : Model->Traits[Index].Rank; })
            .OnInspect(FSimpleDelegate::CreateLambda([this, Index]() { SelectTrait(Index); }))
            .OnPropose(FPWProposeRank::CreateLambda([this, Index](int32 Rank) { Propose(Index, Rank); }))
            .OnApply(FSimpleDelegate::CreateSP(this, &SPWTraitsPage::ApplyRank))
            .OnCancel(FSimpleDelegate::CreateSP(this, &SPWTraitsPage::DiscardProposal))
            .OnMoveRow(FPWProposeRank::CreateLambda([this, Index](int32 Direction)
            {
                for (int32 Next = Index + Direction; Model->Traits.IsValidIndex(Next); Next += Direction)
                    if (Model->Traits[Next].Group == Group) { SelectTrait(Next); FSlateApplication::Get().SetKeyboardFocus(Selectors[Next], EFocusCause::Navigation); break; }
            }))
            .OnTraverse(FPWRankTraversal::CreateLambda([this, Index](bool bReverse) { return TraverseRank(Index, bReverse); }))]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
        [PWUI::MakeText(TAttribute<FText>::CreateLambda([this, Index]()
        { return FText::Format(LOCTEXT("Allocated", "{0} points allocated"), FText::AsNumber(Model->Traits[Index].Cost)); }), 14, Style.TextSecondary)]
        + SVerticalBox::Slot().AutoHeight()
        [PWUI::MakeText(TAttribute<FText>::CreateLambda([this, Index]()
        { return SelectedIndex == Index && !Proposal.bCanApply ? Proposal.Message : FText::GetEmpty(); }), 14, Style.TextPrimary, false, true)], FMargin(16));
}

void SPWTraitsPage::SelectTrait(int32 Index)
{
    if (!Model->Traits.IsValidIndex(Index) || Model->Traits[Index].Group != Group) return;
    if (SelectedIndex == Index) return;
    SelectedIndex = Index;
    bShowRanks = false;
    DiscardProposal();
    if (bNarrow) Reflow();
    if (bNarrow) OnRevealInspector.ExecuteIfBound(Inspector);
}

TSharedPtr<SWidget> SPWTraitsPage::TraverseRank(int32 Index, bool bReverse)
{
    if (bNarrow && !bReverse) return PWUI::FindFirstFocusable(Inspector.ToSharedRef());
    const int32 Direction = bReverse ? -1 : 1;
    for (int32 Next = Index + Direction; Model->Traits.IsValidIndex(Next); Next += Direction)
        if (Model->Traits[Next].Group == Group) return Selectors[Next];
    return PWUI::FindFirstFocusable((bReverse ? Navigator : Inspector).ToSharedRef());
}

void SPWTraitsPage::Propose(int32 Index, int32 Rank)
{
    SelectTrait(Index);
    if (const auto* Creator = Model->GetSubsystem()) Proposal = Creator->PreviewTraitRank(Model->Traits[Index].Id, Rank);
    RefreshInspector();
}

void SPWTraitsPage::DiscardProposal()
{
    if (Model->Traits.IsValidIndex(SelectedIndex)) Propose(SelectedIndex, Model->Traits[SelectedIndex].Rank);
}

void SPWTraitsPage::ApplyRank()
{
    if (!Proposal.bCanApply || Proposal.CurrentRank == Proposal.ProposedRank) return;
    const FName Id = Model->Traits[SelectedIndex].Id;
    const int32 Rank = Proposal.ProposedRank;
    if (const auto* Creator = Model->GetSubsystem())
    {
        const auto Fresh = Creator->PreviewTraitRank(Id, Rank);
        if (Fresh.SourceDraftSnapshot != Proposal.SourceDraftSnapshot) { Proposal = Fresh; RefreshInspector(); return; }
        if (!Fresh.bCanApply) { Proposal = Fresh; RefreshInspector(); return; }
    }
    Model->Edit([Id, Rank](UPerfectWorldCharacterCreatorSubsystem& Creator) { return Creator.SetTraitRank(Id, Rank); });
    DiscardProposal();
    PWUI::FocusWhenReady(AsShared(), Selectors[SelectedIndex]);
}

void SPWTraitsPage::History(bool bRedo)
{
    Model->Edit([bRedo](UPerfectWorldCharacterCreatorSubsystem& Creator) { return bRedo ? Creator.RedoAllocation() : Creator.UndoAllocation(); });
    DiscardProposal();
}

void SPWTraitsPage::InspectResources()
{
    for (int32 Index = 0; Index < Model->Traits.Num(); ++Index) if (Model->Traits[Index].Id == FName(TEXT("household_resources")))
    {
        Group = Model->Traits[Index].Group;
        SelectedIndex = Index;
        DiscardProposal(); Reflow();
        PWUI::FocusWhenReady(AsShared(), Selectors[Index]);
        break;
    }
}

void SPWTraitsPage::FocusRegion(bool bReverse)
{
    const TSharedPtr<SWidget> CurrentRegions[] = {Toolbar, Navigator, Selectors.IsValidIndex(SelectedIndex) ? Selectors[SelectedIndex] : nullptr, Inspector};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(CurrentRegions); ++Index)
        if (CurrentRegions[Index].IsValid() && (CurrentRegions[Index]->HasKeyboardFocus() || CurrentRegions[Index]->HasFocusedDescendants())) Region = Index;
    if (!HasKeyboardFocus() && !HasFocusedDescendants()) Region = 4;
    Region = (Region + (bReverse ? 4 : 1)) % 5;
    if (Region == 4) { OnFocusFooter.ExecuteIfBound(); return; }
    const TSharedPtr<SWidget> Targets[] = {Toolbar, Navigator, Selectors.IsValidIndex(SelectedIndex) ? Selectors[SelectedIndex] : nullptr, Inspector};
    if (Targets[Region].IsValid()) PWUI::FocusWhenReady(AsShared(), Targets[Region]);
}

FReply SPWTraitsPage::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::F6) { FocusRegion(Event.IsShiftDown()); return FReply::Handled(); }
    if (Event.IsControlDown() && !Event.IsRepeat() && (Event.GetKey() == EKeys::Z || Event.GetKey() == EKeys::Y))
    { History(Event.GetKey() == EKeys::Y); return FReply::Handled(); }
    if ((Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::Gamepad_FaceButton_Right) && Proposal.CurrentRank != Proposal.ProposedRank)
    { DiscardProposal(); return FReply::Handled(); }
    return SCompoundWidget::OnKeyDown(Geometry, Event);
}

#undef LOCTEXT_NAMESPACE
