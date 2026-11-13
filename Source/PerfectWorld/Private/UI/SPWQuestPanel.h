#pragma once

#include "CoreMinimal.h"
#include "UI/PWQuestText.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SBox;
class SScrollBox;

/**
 * The quest window: Today, Promises and Discoveries on the left, the chosen one in full on the right (what, who, where, when,
 * status), and a Track button that puts one objective on the HUD line under the minimap area (SPWQuestTracker). It holds no rules.
 * Everything it shows is the portable QuestLogView through PWQuest::BuildModel, so it never adds state to the world; the tracked
 * id is session-only (PWQuest::GetTrackedId).
 *
 * It sits inside the pause menu's content holder the way the Today journal does, and opening it holds time. Opened from the world
 * with J (or the controller's left shoulder button) the front end HUD pauses first and the pause menu shows this window straight
 * away; closing it then resumes. Opened from the pause menu's Quests entry, closing it returns to the menu.
 *
 * Layout: screen aligned, scales with the viewport. Wide windows (700+ logical units across, which is every window at the shipped
 * sizes) put the list and the detail side by side; narrower ones stack them. Both panes scroll inside the card, so Back never leaves
 * the screen. The arrangement is re-checked every frame, so a resize or the pause menu's first real measurement settles it.
 *
 * Keys while it has focus:
 *   Up, Down, D-pad, left stick   choose the previous or next quest (past the last one, Down moves on to the buttons)
 *   Page Up, Page Down, RB        the first quest of the previous or next section (RB goes forward)
 *   T, X                          track or stop tracking the chosen quest
 *   Esc, J, B, menu button, LB    go back
 *   right stick                   scroll the detail
 * Enter or A presses the focused button. No motion is used, so reduced motion is respected by construction.
 */
class SPWQuestPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWQuestPanel) : _Space(FVector2D(1920.0, 1080.0)) {}
        SLATE_ARGUMENT(FPWQuestModel, Model)
        /** The logical size of the screen the panel may use. The pause menu passes the size it already measures. */
        SLATE_ATTRIBUTE(FVector2D, Space)
        SLATE_EVENT(FSimpleDelegate, OnClose)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
    virtual void OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent) override;

    const FPWQuestModel& GetModel() const { return Model; }
    /** The id of the chosen row, or empty when there is none. */
    const FString& GetSelectedId() const { return SelectedId; }
    /** Chooses a row by id; an unknown id is ignored. Returns whether the choice changed. */
    bool SelectRow(const FString& Id);
    /** Tracks the chosen row, or stops tracking it. Returns whether something is tracked afterwards. */
    bool ToggleTrackSelected();
    /** Re-checks the pane arrangement against the space now; returns true when it changed. */
    bool RefreshLayout();
    bool IsTwoPane() const { return bTwoPane; }

private:
    TSharedRef<SWidget> BuildBody();
    TSharedRef<SWidget> BuildList();
    TSharedRef<SWidget> BuildDetail();
    TSharedRef<SWidget> BuildRow(const FPWQuestRow& Row);
    FVector2D GetSpace() const;
    const FPWQuestRow* GetSelectedRow() const { return Model.FindRow(SelectedId); }
    /** Moves the choice by Step rows. Returns false at either end, so the key can fall through to the focus navigation. */
    bool MoveSelection(int32 Step);
    void MoveSection(int32 Step);
    void FocusRow(const FString& Id);
    void ScrollDetail(float Delta);
    void RequestClose();

    FPWQuestModel Model;
    TAttribute<FVector2D> Space;
    FSimpleDelegate OnClose;
    FString SelectedId;
    /** The row ids in display order, and each row's button for focus and scrolling. */
    TArray<FString> RowOrder;
    TMap<FString, TSharedPtr<SWidget>> RowButtons;
    TSharedPtr<SBox> BodyHolder;
    TSharedPtr<SScrollBox> ListScroll;
    TSharedPtr<SScrollBox> DetailScroll;
    TWeakPtr<SWidget> BackButton;
    bool bTwoPane = true;
};
