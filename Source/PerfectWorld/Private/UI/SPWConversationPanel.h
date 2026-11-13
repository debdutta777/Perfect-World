#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldConversationPresenter.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SBox;
class SVerticalBox;

/** The player picked the choice with this id. */
DECLARE_DELEGATE_OneParam(FPWConversationChooseDelegate, FName);

/**
 * The flat lower-screen conversation panel (LANE_LIFE_PLAN section B): speaker, the line at subtitle size, up to three
 * choices that each say what they cost, and an always-visible Leave. It shows the view it is given and decides nothing.
 *
 * Keys while it has focus: arrow keys and D-pad move between the choices and Leave, Enter or A picks, the number keys
 * 1 to 3 pick directly, Escape, B and the controller menu button leave. A narrow screen keeps the choices in a scroll box
 * and Leave outside it, so Leave never scrolls out of sight.
 */
class SPWConversationPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWConversationPanel) {}
        SLATE_EVENT(FPWConversationChooseDelegate, OnChoose)
        SLATE_EVENT(FSimpleDelegate, OnLeave)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Replaces the speaker, line and choices, and moves focus to the first choice (or Leave when there are none). */
    void SetView(const FPWConversationView& View);

    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
    TSharedRef<SWidget> MakeChoiceRow(int32 Index, const FPWConversationChoice& Choice);
    static FText FormatCost(int32 Minutes);
    void PickChoice(int32 Index);
    void RequestLeave();

    FPWConversationChooseDelegate OnChoose;
    FSimpleDelegate OnLeave;

    FText Speaker;
    FText Line;
    bool bCanLeave = true;
    TArray<FName> ChoiceIds;
    TSharedPtr<SBox> PanelRoot;
    TSharedPtr<SVerticalBox> ChoiceBox;
};
