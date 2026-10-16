#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

/**
 * The small top-right clock, for example "Mon 2 Jun · 07:42". It shows what it is told and never takes a click or a key. The owner
 * binds attributes that read the world clock through a weak pointer, so this widget holds no object reference.
 *
 * Motion: none. The text changes in place when the minute changes; nothing ticks, fades or pulses, so there is nothing for the
 * reduced-motion setting to turn off.
 */
class SPWClockReadout : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWClockReadout) {}
        /** The clock text. Empty hides the readout. */
        SLATE_ATTRIBUTE(FText, TimeText)
        /** The readout is collapsed while this is false (under the menus). */
        SLATE_ATTRIBUTE(bool, ShowReadout)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
};
