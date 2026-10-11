#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FPWCreationModel;

/** Step 1: the five household templates as cards, and the opening home they resolve to. */
class SPWHouseholdPage : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWHouseholdPage) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, const TSharedRef<FPWCreationModel>& InModel);

private:
    TSharedRef<SWidget> BuildCard(int32 Index);

    TSharedPtr<FPWCreationModel> Model;
};
