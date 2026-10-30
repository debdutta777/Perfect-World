#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SBox;
class SScrollBox;

/**
 * The flat reading view (ROADMAP M1-19): a calm, paper-like sheet centred on the screen, in the project palette (ivory paper,
 * dark ink, one amber rule). It is screen-aligned, so there is no perspective distortion, and it shows the pages it is given
 * and decides nothing.
 *
 * Text size: the page text is wrapped inside a scroll box, so a long page or a large size never pushes the footer off the
 * screen. The player can step the text through three sizes with A- and A+ (the choice lasts for the session), and the
 * whole panel follows the viewport's DPI scale like every other screen.
 *
 * Keys while it has focus: Left/Right arrows, the D-pad and the shoulder buttons turn the page; Up/Down, Page Up/Page Down
 * scroll a long page; Enter or A presses the focused button; Escape, B and the controller menu button close. Close is
 * outside the scroll box and never scrolls out of sight.
 */
class SPWReadingPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWReadingPanel) {}
        SLATE_EVENT(FSimpleDelegate, OnClose)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Replaces the title and pages, shows the first page and moves focus to Next (or Close when there is only one page). */
    void SetContent(const FText& InTitle, const TArray<FText>& InPages);

    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
    void ShowPage(int32 NewIndex);
    void StepPage(int32 Delta);
    void StepTextSize(int32 Delta);
    void ScrollText(float Delta);
    void RequestClose();
    bool HasPrevious() const { return PageIndex > 0; }
    bool HasNext() const { return PageIndex + 1 < Pages.Num(); }

    FSimpleDelegate OnClose;

    FText Title;
    TArray<FText> Pages;
    int32 PageIndex = 0;

    /** The sheet. Slate keeps a pointer to a brush, so the brush must live as long as the widget. */
    FSlateBrush PaperBrush;

    TSharedPtr<SBox> PanelRoot;
    TSharedPtr<SBox> NextHolder;
    TSharedPtr<SBox> CloseHolder;
    TSharedPtr<SScrollBox> TextScroll;
};
