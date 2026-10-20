#include "UI/TitleR5Cards.h"

#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "UI/TitleR5Model.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "PerfectWorldTitleR5Cards"

namespace
{
/** Two lines of the summary as one, with a middle dot between them; whichever is empty is left out. */
FText Tr5Join(const FText& A, const FText& B)
{
    if (A.IsEmpty() || B.IsEmpty())
    {
        return A.IsEmpty() ? B : A;
    }
    return FText::Format(LOCTEXT("JoinFmt", "{0}  ·  {1}"), A, B);
}

void Tr5AddLine(const TSharedRef<SVerticalBox>& Box, const FText& Text, const int32 Size, const FLinearColor& Color,
                const bool bBold, const float Gap)
{
    if (Text.IsEmpty())
    {
        return;
    }
    Box->AddSlot().AutoHeight().Padding(0.0f, Gap, 0.0f, 0.0f)[PWUI::MakeText(Text, Size, Color, bBold, true)];
}

FText Tr5NoteText(const PWTitleR5::FSlotSummary& Summary)
{
    // A failure carries its own mark, so it never relies on colour alone.
    return Summary.State == PWTitleR5::ESlotState::Unreadable
        ? FText::Format(LOCTEXT("ErrorMark", "! {0}"), Summary.Note)
        : Summary.Note;
}

FLinearColor Tr5NoteColour(const PWTitleR5::FSlotSummary& Summary)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    switch (Summary.State)
    {
    case PWTitleR5::ESlotState::Unreadable: return Style.Danger;
    case PWTitleR5::ESlotState::Recovered: return Style.Warn;
    default: return Style.TextSecondary;
    }
}
} // namespace

namespace PWTitleR5
{
TSharedRef<SWidget> MakeSummaryBody(const FSlotSummary& Summary, const TAttribute<bool>& bCompact)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const bool bUnreadable = Summary.State == ESlotState::Unreadable;
    const FText Saved = Tr5Join(Summary.SavedAt, Summary.SlotLabel);

    const TSharedRef<SVerticalBox> Full = SNew(SVerticalBox);
    const TSharedRef<SVerticalBox> Compact = SNew(SVerticalBox);
    if (bUnreadable)
    {
        // Nothing of the life can be said; show which slot, when the file was written and why it cannot load.
        for (const TSharedRef<SVerticalBox>& Box : {Full, Compact})
        {
            Tr5AddLine(Box, Summary.SlotLabel, 15, Style.Accent, true, 0.0f);
            Tr5AddLine(Box, Summary.SavedAt, 14, Style.TextMuted, false, 2.0f);
            Tr5AddLine(Box, Tr5NoteText(Summary), 15, Tr5NoteColour(Summary), false, 4.0f);
        }
    }
    else
    {
        Tr5AddLine(Full, Summary.Name, 20, Style.TextPrimary, true, 0.0f);
        Tr5AddLine(Full, Summary.Age, 15, Style.TextSecondary, false, 2.0f);
        Tr5AddLine(Full, Summary.GameTime, 17, Style.TextPrimary, false, 4.0f);
        Tr5AddLine(Full, Summary.Place, 15, Style.TextSecondary, false, 2.0f);
        Tr5AddLine(Full, Saved, 14, Style.TextMuted, false, 4.0f);
        Tr5AddLine(Full, Tr5NoteText(Summary), 14, Tr5NoteColour(Summary), false, 4.0f);

        Tr5AddLine(Compact, Summary.Name, 18, Style.TextPrimary, true, 0.0f);
        Tr5AddLine(Compact, Summary.Age, 14, Style.TextSecondary, false, 1.0f);
        Tr5AddLine(Compact, Tr5Join(Summary.GameTime, Summary.Place), 15, Style.TextPrimary, false, 2.0f);
        Tr5AddLine(Compact, Saved, 13, Style.TextMuted, false, 2.0f);
        Tr5AddLine(Compact, Tr5NoteText(Summary), 13, Tr5NoteColour(Summary), false, 2.0f);
    }

    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [SNew(SBox).Visibility_Lambda([bCompact]() { return bCompact.Get(false) ? EVisibility::Collapsed : EVisibility::Visible; })[Full]]
        + SVerticalBox::Slot().AutoHeight()
        [SNew(SBox).Visibility_Lambda([bCompact]() { return bCompact.Get(false) ? EVisibility::Visible : EVisibility::Collapsed; })[Compact]];
}
} // namespace PWTitleR5

#undef LOCTEXT_NAMESPACE
