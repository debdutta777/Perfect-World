#pragma once

#include "CoreMinimal.h"
#include "Animation/CurveSequence.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "Widgets/SCompoundWidget.h"

class FPWCreationModel;
class SBox;
class SComboButton;
DECLARE_DELEGATE_TwoParams(FPWCreationModalDelegate, TSharedRef<SWidget>, TSharedPtr<SWidget>);
DECLARE_DELEGATE_OneParam(FPWCreationRevealDelegate, TSharedPtr<SWidget>);

/** Proposals are local; every ledger edit goes through the subsystem. */
class SPWTraitsPage : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWTraitsPage) {}
        SLATE_ATTRIBUTE(bool, NarrowLayout)
        SLATE_EVENT(FPWCreationModalDelegate, OnOpenModal)
        SLATE_EVENT(FSimpleDelegate, OnCloseModal)
        SLATE_EVENT(FSimpleDelegate, OnFocusFooter)
        SLATE_EVENT(FPWCreationRevealDelegate, OnRevealInspector)
    SLATE_END_ARGS()
    void Construct(const FArguments& InArgs, const TSharedRef<FPWCreationModel>& InModel);
    virtual void Tick(const FGeometry&, double, float) override;
    virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override;
    TSharedRef<SWidget> BuildBudget();
    void InspectResources();
    void DiscardProposal();
    void FocusRegion(bool bReverse);

private:
    TSharedRef<SWidget> BuildToolbar();
    TSharedRef<SWidget> BuildNavigator();
    TSharedRef<SWidget> BuildRow(int32 Index);
    TSharedRef<SWidget> BuildInspector();
    TSharedRef<SWidget> BuildRank(const FPWTraitRankView& Rank, const FText& Label);
    void Reflow();
    void RefreshInspector();
    void SelectTrait(int32 Index);
    TSharedPtr<SWidget> TraverseRank(int32 Index, bool bReverse);
    void Propose(int32 Index, int32 Rank);
    void ApplyRank();
    void History(bool bRedo);
    void OpenDrawer(FName Preset, int32 Operation);
    void RefreshDrawer();
    void CloseDrawer();
    void ApplyDrawer();

    TSharedPtr<FPWCreationModel> Model;
    TSharedPtr<SBox> Body, Inspector, DrawerPreview;
    TSharedPtr<SWidget> Toolbar, Navigator;
    TSharedPtr<SComboButton> MoreMenu;
    TArray<TSharedPtr<SWidget>> Selectors, Rows;
    FPWTraitChangePreview Proposal;
    FPWAllocationPreview Allocation;
    FCurveSequence InspectorTransition;
    FCurveHandle InspectorReveal;
    FPWCreationModalDelegate OnOpenModal;
    FSimpleDelegate OnCloseModal;
    FSimpleDelegate OnFocusFooter;
    FPWCreationRevealDelegate OnRevealInspector;
    FName Group, DrawerPreset;
    int32 SelectedIndex = 0, DrawerOperation = 0, Region = 0;
    bool bNarrow = false, bShowRanks = false;
    TAttribute<bool> NarrowLayout;
};
