#pragma once

#include "CoreMinimal.h"
#include "UI/PWQuestText.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

/**
 * The small HUD line for the tracked quest, drawn at the top right under the clock and the minimap area: the quest's title and, under
 * it, when or where it can be done, for example "Be at school by the first bell" over "By 10:00". Nothing is shown when nothing is
 * tracked, when the tracked quest is gone, or when it is done. The tracked id is the session's (PWQuest::GetTrackedId), set from the quest
 * window; this widget never changes it and keeps no state of the world.
 *
 * It re-reads the portable projection only when the world clock's minute or the tracked id changes, so it costs nothing while the clock
 * is held. It never takes a click or a key (hit-test invisible), and it has no motion, so reduced motion is respected by construction.
 */
class SPWQuestTracker : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWQuestTracker) : _TopOffset(76.0f) {}
        /** Any object of the played world; the widget reads the lane-life subsystem through it, weakly. */
        SLATE_ARGUMENT(TWeakObjectPtr<const UObject>, WorldContext)
        /** The line is hidden while this is false (under the menus). */
        SLATE_ATTRIBUTE(bool, ShowTracker)
        /** Logical units from the top of the screen, so the owner can clear the clock readout and the minimap. */
        SLATE_ATTRIBUTE(float, TopOffset)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

    /** Re-reads now if the clock minute or the tracked id changed. Tick calls it; tests call it directly. */
    void Refresh();
    const FPWQuestTrackedLine& GetLine() const { return Line; }

private:
    FText GetTitleText() const;
    FText GetWhenText() const;

    TWeakObjectPtr<const UObject> WorldContext;
    TAttribute<bool> ShowTracker;
    FPWQuestTrackedLine Line;
    int64 ReadAtMinute = -1;
    uint32 ReadAtRevision = 0;
    bool bRead = false;
};
