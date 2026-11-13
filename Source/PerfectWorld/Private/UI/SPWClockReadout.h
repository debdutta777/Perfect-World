#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

/**
 * The small top-right clock, for example "Mon 2 Jun · 07:42". It shows what it is told and never takes a click or a key. The owner
 * binds attributes that read the world clock through a weak pointer, so this widget holds no object reference.
 *
 * The wallet (R8-05) shares the clock's visibility, so it is hidden under the same menus and never takes input either.
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
        /** R8-05: the wallet ("Wallet ₹120"), in its own small pill to the left of the clock. Empty or unset hides it. */
        SLATE_ATTRIBUTE(FText, WalletText)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
};
