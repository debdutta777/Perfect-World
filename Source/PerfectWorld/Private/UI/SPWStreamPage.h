#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FPWCreationModel;
class SEditableTextBox;

/** Step 4: Science, Commerce or Humanities, and an optional reason. A timetable, not a destiny. */
class SPWStreamPage : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWStreamPage) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, const TSharedRef<FPWCreationModel>& InModel);

private:
    TSharedRef<SWidget> BuildCard(int32 Index);

    /** The stream currently chosen, or the first one when none is marked. */
    FName SelectedStreamId() const;

    TSharedPtr<FPWCreationModel> Model;
    TSharedPtr<SEditableTextBox> ReasonBox;
};
