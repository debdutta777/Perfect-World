#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

/**
 * The small lower-centre interaction prompt: "E / X  -  Talk to Farida", with a second quiet line when more than one
 * target is in reach ("Tab / RB for the next"). It shows what it is told and never takes a click or a key. The owner binds
 * attributes that read the interaction component through a weak pointer, so this widget holds no object reference.
 */
class SPWInteractionPrompt : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWInteractionPrompt) {}
        /** The prompt is collapsed while this is false. */
        SLATE_ATTRIBUTE(bool, ShowPrompt)
        SLATE_ATTRIBUTE(FText, Verb)
        /** Empty hides the second line. */
        SLATE_ATTRIBUTE(FText, CycleHint)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
};
