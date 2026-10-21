#include "UI/SPWQuestPanel.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "UI/PWWidgets.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"

// The quest window's behaviour: choosing, tracking, focus, layout switching and keys. Its widgets are built in SPWQuestPanel.cpp.

namespace
{
bool QwIsBack(const FKey& Key)
{
    return Key == EKeys::Escape || Key == EKeys::J || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right
        || Key == EKeys::Gamepad_LeftShoulder;
}

bool QwIsUp(const FKey& Key)
{
    return Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up;
}

bool QwIsDown(const FKey& Key)
{
    return Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down;
}
} // namespace

bool SPWQuestPanel::SelectRow(const FString& Id)
{
    if (Id == SelectedId || Model.FindRow(Id) == nullptr)
    {
        return false;
    }
    SelectedId = Id;
    if (ListScroll.IsValid())
    {
        if (const TSharedPtr<SWidget>* Button = RowButtons.Find(Id)) ListScroll->ScrollDescendantIntoView(*Button, false);
    }
    if (DetailScroll.IsValid()) DetailScroll->SetScrollOffset(0.0f);
    return true;
}

bool SPWQuestPanel::ToggleTrackSelected()
{
    const FPWQuestRow* Row = GetSelectedRow();
    if (Row == nullptr)
    {
        return !PWQuest::GetTrackedId().IsEmpty();
    }
    return PWQuest::ToggleTracked(Row->Id);
}

bool SPWQuestPanel::RefreshLayout()
{
    const FVector2D S = GetSpace();
    if (!BodyHolder.IsValid() || PWQuest::TwoPane(static_cast<float>(S.X), static_cast<float>(S.Y)) == bTwoPane)
    {
        return false;
    }
    const bool bHadFocus = HasFocusedDescendants();
    BodyHolder->SetContent(BuildBody());
    if (bHadFocus)
    {
        const TSharedPtr<SWidget>* Chosen = RowButtons.Find(SelectedId);
        PWUI::FocusWhenReady(AsShared(), Chosen != nullptr ? TWeakPtr<SWidget>(*Chosen) : BackButton);
    }
    return true;
}

void SPWQuestPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    RefreshLayout();
}

void SPWQuestPanel::OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent)
{
    SCompoundWidget::OnFocusChanging(PreviousFocusPath, NewWidgetPath, InFocusEvent);
    // Moving focus to a quest (by Tab or by the focus navigation) chooses it, so the detail never disagrees with the focused row.
    for (int32 Index = NewWidgetPath.Widgets.Num() - 1; Index >= 0; --Index)
    {
        const TSharedRef<SWidget>& Widget = NewWidgetPath.Widgets[Index].Widget;
        for (const TPair<FString, TSharedPtr<SWidget>>& Pair : RowButtons)
        {
            if (Pair.Value.Get() == &Widget.Get())
            {
                SelectRow(Pair.Key);
                return;
            }
        }
    }
}

void SPWQuestPanel::FocusRow(const FString& Id)
{
    const TSharedPtr<SWidget>* Button = RowButtons.Find(Id);
    if (Button == nullptr || !FSlateApplication::IsInitialized())
    {
        return;
    }
    const TSharedPtr<SWidget> Found = PWUI::FindFirstFocusable(Button->ToSharedRef());
    if (Found.IsValid())
    {
        FSlateApplication::Get().SetKeyboardFocus(Found, EFocusCause::Navigation);
    }
}

bool SPWQuestPanel::MoveSelection(const int32 Step)
{
    if (RowOrder.IsEmpty())
    {
        return false;
    }
    const int32 Index = RowOrder.IndexOfByKey(SelectedId);
    const int32 Target = Index == INDEX_NONE ? 0 : Index + Step;
    if (Target < 0 || Target >= RowOrder.Num())
    {
        return false;
    }
    SelectRow(RowOrder[Target]);
    FocusRow(RowOrder[Target]);
    return true;
}

void SPWQuestPanel::MoveSection(const int32 Step)
{
    const FPWQuestRow* Current = GetSelectedRow();
    int32 Section = Current != nullptr ? static_cast<int32>(Current->Section) : (Step > 0 ? -1 : Model.Sections.Num());
    for (Section += Step; Section >= 0 && Section < Model.Sections.Num(); Section += Step)
    {
        if (!Model.Sections[Section].Rows.IsEmpty())
        {
            const FString Id = Model.Sections[Section].Rows[0].Id;
            SelectRow(Id);
            FocusRow(Id);
            return;
        }
    }
}

void SPWQuestPanel::ScrollDetail(const float Delta)
{
    if (DetailScroll.IsValid())
    {
        DetailScroll->SetScrollOffset(FMath::Clamp(DetailScroll->GetScrollOffset() + Delta, 0.0f, DetailScroll->GetScrollOffsetOfEnd()));
    }
}

void SPWQuestPanel::RequestClose()
{
    OnClose.ExecuteIfBound();
}

FReply SPWQuestPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    constexpr float QwScrollStep = 72.0f;
    const FKey Key = InKeyEvent.GetKey();
    if (QwIsBack(Key))
    {
        // A held key auto-repeats; this window goes back on the first press only.
        if (!InKeyEvent.IsRepeat())
        {
            RequestClose();
        }
        return FReply::Handled();
    }
    if (Key == EKeys::T || Key == EKeys::Gamepad_FaceButton_Left)
    {
        if (!InKeyEvent.IsRepeat())
        {
            ToggleTrackSelected();
        }
        return FReply::Handled();
    }
    // Choosing repeats; at either end the key is left for the focus navigation (Down from the last quest reaches the buttons).
    if (QwIsUp(Key) || QwIsDown(Key))
    {
        return MoveSelection(QwIsUp(Key) ? -1 : 1) ? FReply::Handled() : FReply::Unhandled();
    }
    if (Key == EKeys::PageUp || Key == EKeys::PageDown || Key == EKeys::Gamepad_RightShoulder)
    {
        if (!InKeyEvent.IsRepeat())
        {
            MoveSection(Key == EKeys::PageUp ? -1 : 1);
        }
        return FReply::Handled();
    }
    if (Key == EKeys::Gamepad_RightStick_Up || Key == EKeys::Gamepad_RightStick_Down)
    {
        ScrollDetail(Key == EKeys::Gamepad_RightStick_Up ? -QwScrollStep : QwScrollStep);
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

