#pragma once

#include "CoreMinimal.h"
#include "Animation/CurveSequence.h"
#include "UI/PWDelegates.h"
#include "Widgets/SCompoundWidget.h"

class FPWCreationModel;
class SVerticalBox;

/** Household selection and its resolved opening home. */
class SPWHouseholdPage : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWHouseholdPage) {}
        SLATE_ATTRIBUTE(bool, NarrowLayout)
        SLATE_EVENT(FSimpleDelegate, OnEditResources)
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
    TSharedRef<SWidget> BuildHouseholdRow(int32 Index);
    TSharedRef<SWidget> BuildDetails();
    FText EverydayText() const;
    TSharedPtr<FPWCreationModel> Model;
    TSharedPtr<SVerticalBox> Root;
    TArray<TSharedPtr<SWidget>> HouseholdRows;
    TSharedPtr<SWidget> Details;
    TSharedPtr<SWidget> EditResourcesWidget;
    FSimpleDelegate OnEditResources;
    FPWWidgetDelegate OnFocusDetails;
    FSimpleDelegate OnFocusFooter;
    int32 Region = 0;
    FCurveSequence DetailTransition;
    FCurveHandle DetailReveal;
    FName LastHousehold;
    int32 LastResourceRank = INDEX_NONE;
    FText HouseholdProblem;
    bool bNarrow = false;
    bool bLayoutInitialized = false;
    TAttribute<bool> NarrowLayout;
};
