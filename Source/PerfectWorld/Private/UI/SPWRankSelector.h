#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"

class SButton;
DECLARE_DELEGATE_OneParam(FPWProposeRank, int32);
DECLARE_DELEGATE_RetVal_OneParam(TSharedPtr<SWidget>, FPWRankTraversal, bool);
DECLARE_DELEGATE_RetVal_TwoParams(bool, FPWRankAnalogGate, bool, float);

/** A five-position rank picker which participates in focus traversal as one control. */
class SPWRankSelector : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWRankSelector) {}
        SLATE_ATTRIBUTE(int32, CurrentRank)
        SLATE_ATTRIBUTE(int32, ProposedRank)
        SLATE_EVENT(FSimpleDelegate, OnInspect)
        SLATE_EVENT(FPWProposeRank, OnPropose)
        SLATE_EVENT(FSimpleDelegate, OnApply)
        SLATE_EVENT(FSimpleDelegate, OnCancel)
        /** Receives -1 for the previous row and +1 for the next row. */
        SLATE_EVENT(FPWProposeRank, OnMoveRow)
        SLATE_EVENT(FPWRankTraversal, OnTraverse)
        SLATE_EVENT(FPWRankAnalogGate, OnAnalogGate)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnFocusReceived(const FGeometry& Geometry, const FFocusEvent& FocusEvent) override;
    virtual FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual FReply OnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual FReply OnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event) override;
    virtual FNavigationReply OnNavigation(const FGeometry& Geometry, const FNavigationEvent& Event) override;

private:
    TSharedRef<SWidget> BuildPosition(int32 Rank);
    FReply Propose(int32 Rank);
    FReply FocusAndPropose(int32 Rank);
    bool HasPendingProposal() const;
    bool IsActivationKey(const FKey& Key) const;
    void FocusPosition(int32 Rank, EFocusCause Cause);

    TAttribute<int32> CurrentRank;
    TAttribute<int32> ProposedRank;
    FSimpleDelegate OnInspect;
    FPWProposeRank OnPropose;
    FSimpleDelegate OnApply;
    FSimpleDelegate OnCancel;
    FPWProposeRank OnMoveRow;
    FPWRankTraversal OnTraverse;
    FPWRankAnalogGate OnAnalogGate;
    TArray<TSharedPtr<SButton>> Positions;
};
