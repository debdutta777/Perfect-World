#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FPWCreationModel;

/** Step 2: the points bar, the five presets, and all twenty traits grouped with a slider each. */
class SPWTraitsPage : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWTraitsPage) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, const TSharedRef<FPWCreationModel>& InModel);

private:
    TSharedRef<SWidget> BuildPointsBar();
    TSharedRef<SWidget> BuildPresetRow();
    TSharedRef<SWidget> BuildTraitList();
    TSharedRef<SWidget> BuildTraitRow(int32 Index);

    /** Asks the core to set a trait's rank; the core's refusal, if any, becomes the page message. */
    void SetRank(FName TraitId, int32 Rank);
    int32 RankAt(int32 Index) const;
    FText RankSummary(int32 Index) const;

    TSharedPtr<FPWCreationModel> Model;
};
