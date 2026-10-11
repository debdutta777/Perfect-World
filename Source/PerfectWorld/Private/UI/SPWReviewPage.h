#pragma once

#include "CoreMinimal.h"
#include "UI/PWDelegates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FPWCreationModel;

/** Step 5: everything chosen, in one place, with warnings and the core's verdict on starting. */
class SPWReviewPage : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWReviewPage) {}
        /** Asked to open an earlier step so the player can change something. */
        SLATE_EVENT(FPWStepDelegate, OnJumpToStep)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, const TSharedRef<FPWCreationModel>& InModel);

private:
    TSharedRef<SWidget> BuildSection(const FText& Heading, int32 StepToEdit, const TSharedRef<SWidget>& Body);
    TSharedRef<SWidget> BuildTraitsPanel();
    TSharedRef<SWidget> BuildVerdict();

    TSharedPtr<FPWCreationModel> Model;
    FPWStepDelegate OnJumpToStep;
};
