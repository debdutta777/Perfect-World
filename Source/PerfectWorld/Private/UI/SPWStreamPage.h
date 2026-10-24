#pragma once

#include "CoreMinimal.h"
#include "Animation/CurveSequence.h"
#include "UI/PWDelegates.h"
#include "Widgets/SCompoundWidget.h"

class FPWCreationModel;
class SEditableTextBox;
class SVerticalBox;
struct FPWStreamView;

/** School-stream selection, sample day, route context and optional note. */
class SPWStreamPage : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWStreamPage) {}
        SLATE_ATTRIBUTE(bool, NarrowLayout)
        SLATE_EVENT(FPWWidgetDelegate, OnFocusDetails)
        SLATE_EVENT(FSimpleDelegate, OnFocusFooter)
    SLATE_END_ARGS()
    void Construct(const FArguments& InArgs, const TSharedRef<FPWCreationModel>& InModel);
    virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;
    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
    virtual FNavigationReply OnNavigation(const FGeometry&, const FNavigationEvent&) override;
    void FocusRegion(bool bReverse, bool bFromFooter = false);
private:
    void RebuildLayout(bool bUseNarrowLayout);
    TSharedRef<SWidget> BuildCard(int32 Index);
    TSharedRef<SWidget> BuildDetails();
    /** Rebuilds the sample-day rows from the selected stream's view. */
    void PopulateSampleRows();
    void CommitReason(const FText& Text, ETextCommit::Type CommitType);
    FName SelectedStreamId() const;
    const FPWStreamView* SelectedStream() const;
    TSharedPtr<FPWCreationModel> Model;
    TSharedPtr<SVerticalBox> Root;
    TArray<TSharedPtr<SWidget>> Cards;
    TArray<TSharedPtr<SWidget>> CardContents;
    TSharedPtr<SWidget> Details;
    TSharedPtr<SWidget> SampleDayRegion;
    TSharedPtr<SVerticalBox> SampleRowsBox;
    TSharedPtr<SEditableTextBox> ReasonBox;
    FCurveSequence DetailTransition;
    FCurveHandle DetailReveal;
    FName LastStream;
    FText ReasonProblem, StreamProblem;
    FPWWidgetDelegate OnFocusDetails;
    FSimpleDelegate OnFocusFooter;
    int32 Region = 0;
    bool bNarrow = false;
    bool bLayoutInitialized = false;
    TAttribute<bool> NarrowLayout;
};
