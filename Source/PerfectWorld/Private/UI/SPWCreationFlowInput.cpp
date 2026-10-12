#include "UI/SPWCreationFlow.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "Input/NavigationReply.h"
#include "InputCoreTypes.h"
#include "UI/PWWidgets.h"
#include "UI/SPWTraitsPage.h"
#include "UI/SPWHouseholdPage.h"
#include "UI/SPWStreamPage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SNullWidget.h"

namespace
{
void GatherModalActions(const TSharedRef<SWidget>& Widget, TArray<TSharedPtr<SWidget>>& Actions)
{
    if (!Widget->GetVisibility().IsVisible() || !Widget->IsEnabled()) return;
    const bool bScrollRegion = Widget->GetType() == FName(TEXT("SScrollBox")) || Widget->GetType() == FName(TEXT("SPWDrawerScroll"));
    if (Widget->SupportsKeyboardFocus() && !bScrollRegion) Actions.Add(Widget);
    if (FChildren* Children = Widget->GetChildren())
        for (int32 Index = 0; Index < Children->Num(); ++Index) GatherModalActions(Children->GetChildAt(Index), Actions);
    if (Widget->SupportsKeyboardFocus() && bScrollRegion) Actions.Add(Widget);
}

TSharedPtr<SScrollBox> FindModalScroll(const TSharedRef<SWidget>& Widget)
{
    if (Widget->GetType() == FName(TEXT("SScrollBox")) || Widget->GetType() == FName(TEXT("SPWDrawerScroll")))
        return StaticCastSharedRef<SScrollBox>(Widget);
    if (FChildren* Children = Widget->GetChildren())
        for (int32 Index = 0; Index < Children->Num(); ++Index)
            if (auto Found = FindModalScroll(Children->GetChildAt(Index))) return Found;
    return nullptr;
}
}

void SPWCreationFlow::OpenAllocationModal(TSharedRef<SWidget> Content, TSharedPtr<SWidget> SafeAction)
{
    PWUI::CancelPendingFocus();
    ModalInvoker = FSlateApplication::Get().GetKeyboardFocusedWidget();
    bAllocationModal = true;
    AllocationModal->SetContent(Content);
    ModalScroll = FindModalScroll(Content);
    if (ModalScroll.IsValid()) ModalScroll->SetIsFocusable(true);
    ModalTransition.Play(AsShared());
    if (SafeAction.IsValid()) PWUI::FocusWhenReady(AsShared(), SafeAction);
}

void SPWCreationFlow::CloseAllocationModal()
{
    PWUI::CancelPendingFocus();
    bAllocationModal = false;
    AllocationModal->SetContent(SNullWidget::NullWidget);
    ModalScroll.Reset();
    ModalTransition.JumpToEnd();
    if (ModalInvoker.IsValid() && ModalInvoker->GetCachedGeometry().GetLocalSize().X > 0)
        PWUI::FocusWhenReady(AsShared(), ModalInvoker);
    else FocusActivePage();
}

void SPWCreationFlow::EditResources()
{
    GoToStep(1);
    TraitsPage->InspectResources();
}

void SPWCreationFlow::FocusDetails(TSharedPtr<SWidget> Target)
{
    PWUI::FocusWhenReady(AsShared(), PageBody);
    PageBody->ScrollDescendantIntoView(Target, false, EDescendantScrollDestination::TopOrLeft);
}

void SPWCreationFlow::TraverseModal(bool bReverse)
{
    TArray<TSharedPtr<SWidget>> Actions;
    GatherModalActions(AllocationModal.ToSharedRef(), Actions);
    if (Actions.IsEmpty()) return;
    const int32 Current = Actions.IndexOfByKey(FSlateApplication::Get().GetKeyboardFocusedWidget());
    const int32 Next = Current == INDEX_NONE ? 0 : (Current + (bReverse ? Actions.Num() - 1 : 1)) % Actions.Num();
    FSlateApplication::Get().SetKeyboardFocus(Actions[Next], EFocusCause::Navigation);
}

bool SPWCreationFlow::ScrollModal(float Amount)
{
    const float Offset = ModalScroll->GetScrollOffset();
    const float End = FMath::Max(0.f, ModalScroll->GetScrollOffsetOfEnd());
    if ((Amount < 0.f && Offset <= 0.f) || (Amount > 0.f && Offset >= End)) return false;
    ModalScroll->SetScrollOffset(FMath::Clamp(Offset + Amount, 0.f, End));
    return true;
}

FReply SPWCreationFlow::OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    const FKey Key = Event.GetKey();
    if (bAllocationModal)
    {
        if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
        {
            ConsumedRelease = Key;
            if (!Event.IsRepeat()) CloseAllocationModal();
            return FReply::Handled();
        }
        if (ModalScroll.IsValid() && ModalScroll->HasKeyboardFocus() &&
            (Key == EKeys::Up || Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_DPad_Down))
        {
            const bool bUp = Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up;
            // Repeats deliberately scroll, but cannot leave the preview at its boundary.
            if (!ScrollModal(bUp ? -48.f : 48.f) && !Event.IsRepeat()) TraverseModal(bUp);
            return FReply::Handled();
        }
        if (Event.IsRepeat() && (Key == EKeys::Left || Key == EKeys::Right ||
            Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_DPad_Right)) return FReply::Handled();
        if (Key == EKeys::Tab || Key == EKeys::F6 || Key == EKeys::Up || Key == EKeys::Down ||
            Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_DPad_Down)
        {
            if (!Event.IsRepeat()) TraverseModal(Event.IsShiftDown() || Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up);
            return FReply::Handled();
        }
        if (Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Gamepad_RightShoulder || Event.IsControlDown()) return FReply::Handled();
    }
    else if (!bConfirmingStart && Key == EKeys::F6 && (CurrentStep == 0 || CurrentStep == 1 || CurrentStep == 3))
    {
        if (Event.IsRepeat()) return FReply::Handled();
        const bool bFromFooter = FooterControls->HasKeyboardFocus() || FooterControls->HasFocusedDescendants();
        if (CurrentStep == 0) HouseholdPage->FocusRegion(Event.IsShiftDown(), bFromFooter);
        else if (CurrentStep == 1) TraitsPage->FocusRegion(Event.IsShiftDown());
        else StreamPage->FocusRegion(Event.IsShiftDown(), bFromFooter);
        return FReply::Handled();
    }
    else if (!bConfirmingStart && (Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Gamepad_RightShoulder))
    {
        const auto Focus = FSlateApplication::Get().GetKeyboardFocusedWidget();
        if (Focus.IsValid() && Focus->GetTypeAsString().Contains(TEXT("EditableText"))) return FReply::Unhandled();
        if (!Event.IsRepeat()) GoToStep(CurrentStep + (Key == EKeys::Gamepad_LeftShoulder ? -1 : 1));
        return FReply::Handled();
    }
    else if (!bConfirmingStart && Event.IsControlDown() && (Key == EKeys::Z || Key == EKeys::Y))
    {
        const auto Focus = FSlateApplication::Get().GetKeyboardFocusedWidget();
        if (!Focus.IsValid() || !Focus->GetTypeAsString().Contains(TEXT("EditableText")))
            return TraitsPage->OnKeyDown(Geometry, Event);
    }
    if (!bAllocationModal && PageBody->HasKeyboardFocus())
    {
        const bool bUp = Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::PageUp;
        const bool bDown = Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::PageDown;
        if (bUp || bDown)
        {
            const float Amount = Key == EKeys::PageUp || Key == EKeys::PageDown ? PageBody->GetCachedGeometry().GetLocalSize().Y * .8f : 48.f;
            PageBody->SetScrollOffset(FMath::Max(0.f, PageBody->GetScrollOffset() + (bUp ? -Amount : Amount)));
            return FReply::Handled();
        }
    }
    return SCompoundWidget::OnPreviewKeyDown(Geometry, Event);
}

FReply SPWCreationFlow::OnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (ConsumedRelease.IsValid() && Event.GetKey() == ConsumedRelease)
    {
        ConsumedRelease = FKey();
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyUp(Geometry, Event);
}

FReply SPWCreationFlow::OnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event)
{
    if (bAllocationModal && ModalScroll.IsValid() && ModalScroll->HasKeyboardFocus() && Event.GetKey() == EKeys::Gamepad_LeftY)
    {
        if (FMath::Abs(Event.GetAnalogValue()) > .35f)
            ScrollModal(-Event.GetAnalogValue() * 24.f); // The stick only scrolls; analog events arrive every frame.
        return FReply::Handled();
    }
    if (!bAllocationModal && PageBody->HasKeyboardFocus() && Event.GetKey() == EKeys::Gamepad_LeftY)
    {
        if (FMath::Abs(Event.GetAnalogValue()) > .35f)
            PageBody->SetScrollOffset(FMath::Max(0.f, PageBody->GetScrollOffset() - Event.GetAnalogValue() * 24.f));
        return FReply::Handled();
    }
    return SCompoundWidget::OnAnalogValueChanged(Geometry, Event);
}

FNavigationReply SPWCreationFlow::OnNavigation(const FGeometry& Geometry, const FNavigationEvent& Event)
{
    if (bAllocationModal)
    {
        TArray<TSharedPtr<SWidget>> Actions;
        GatherModalActions(AllocationModal.ToSharedRef(), Actions);
        if (Actions.IsEmpty()) return FNavigationReply::Stop();
        const int32 Current = Actions.IndexOfByKey(FSlateApplication::Get().GetKeyboardFocusedWidget());
        const bool bReverse = Event.GetNavigationType() == EUINavigation::Up || Event.GetNavigationType() == EUINavigation::Left || Event.GetNavigationType() == EUINavigation::Previous;
        const int32 Next = Current == INDEX_NONE ? 0 : (Current + (bReverse ? Actions.Num() - 1 : 1)) % Actions.Num();
        return FNavigationReply::Explicit(Actions[Next]);
    }
    return SCompoundWidget::OnNavigation(Geometry, Event);
}
