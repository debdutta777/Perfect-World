#include "UI/SPWRankSelector.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "Input/NavigationReply.h"
#include "InputCoreTypes.h"
#include "UI/PWStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldRankSelector"

namespace
{
class SPWRankPosition final : public SButton
{
public:
    virtual FReply OnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event) override
    {
        // Bubble these axes to the composite before SWidget converts them into navigation.
        if (Event.GetKey() == EKeys::Gamepad_LeftX || Event.GetKey() == EKeys::Gamepad_LeftY)
            return FReply::Unhandled();
        return SButton::OnAnalogValueChanged(Geometry, Event);
    }
};
}

void SPWRankSelector::Construct(const FArguments& InArgs)
{
    CurrentRank = InArgs._CurrentRank;
    ProposedRank = InArgs._ProposedRank;
    OnInspect = InArgs._OnInspect;
    OnPropose = InArgs._OnPropose;
    OnApply = InArgs._OnApply;
    OnCancel = InArgs._OnCancel;
    OnMoveRow = InArgs._OnMoveRow;
    OnTraverse = InArgs._OnTraverse;
    OnAnalogGate = InArgs._OnAnalogGate;
    Positions.Reset();

    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    TSharedRef<SHorizontalBox> PositionRow = SNew(SHorizontalBox);
    for (int32 Rank = 0; Rank <= 4; ++Rank)
    {
        PositionRow->AddSlot().FillWidth(1.0f).Padding(Rank == 0 ? 0.0f : 3.0f, 0.0f)
        [
            BuildPosition(Rank)
        ];
    }

    ChildSlot
    [
        SNew(SBox).MinDesiredHeight(44.0f)
        [
            SNew(SOverlay)
            + SOverlay::Slot()[PositionRow]
            + SOverlay::Slot()
            [
                SNew(SBorder)
                .BorderImage(&Style.BrushFocusRing)
                .Padding(0.0f)
                .Visibility_Lambda([this]()
                {
                    return HasKeyboardFocus() || HasFocusedDescendants()
                        ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
                })
            ]
        ]
    ];
}

TSharedRef<SWidget> SPWRankSelector::BuildPosition(int32 Rank)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<TWeakPtr<SButton>> Handle = MakeShared<TWeakPtr<SButton>>();
    TSharedPtr<SButton> Button;
    SAssignNew(Button, SPWRankPosition)
        .ButtonStyle(&Style.NoButton)
        .ContentPadding(0.0f)
        .IsFocusable(true)
        .AccessibleText_Lambda([this, Rank]()
        {
            const bool bCurrent = CurrentRank.Get(0) == Rank;
            const bool bProposed = ProposedRank.Get(0) == Rank;
            if (bCurrent && bProposed)
            {
                return FText::Format(LOCTEXT("AccessibleCurrentProposed", "Rank {0}, Current rank, Selected"),
                                     FText::AsNumber(Rank));
            }
            if (bCurrent)
            {
                return FText::Format(LOCTEXT("AccessibleCurrent", "Rank {0}, Current rank, Selected"), FText::AsNumber(Rank));
            }
            if (bProposed)
            {
                return FText::Format(LOCTEXT("AccessibleProposed", "Rank {0}, Proposed"), FText::AsNumber(Rank));
            }
            return FText::Format(LOCTEXT("AccessibleRank", "Rank {0}"), FText::AsNumber(Rank));
        })
        .ToolTipText(FText::Format(LOCTEXT("RankTooltip", "Propose rank {0}"), FText::AsNumber(Rank)))
        .OnClicked_Lambda([this, Rank]() { return FocusAndPropose(Rank); })
        [
            SNew(SBox).MinDesiredWidth(44.0f).MinDesiredHeight(44.0f)
            [
                SNew(SOverlay)
                + SOverlay::Slot()
                [
                    SNew(SBorder)
                    .Padding(2.0f)
                    .BorderImage_Lambda([this, Rank, Handle]() -> const FSlateBrush*
                    {
                        const PWUI::FStyle& S = PWUI::FStyle::Get();
                        const TSharedPtr<SButton> Position = Handle->Pin();
                        if (ProposedRank.Get(0) == Rank) return &S.BrushCardSelected;
                        return Position.IsValid() && Position->IsHovered() ? &S.BrushCardHover : &S.BrushCard;
                    })
                    [
                        SNew(STextBlock)
                        .Text(FText::AsNumber(Rank))
                        .Font(PWUI::Font(PWUI::ETextRole::Control))
                        .Justification(ETextJustify::Center)
                        .ColorAndOpacity(FSlateColor(Style.TextPrimary))
                    ]
                ]
                + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(3.0f)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("CurrentCheck", "\u2713"))
                    .Font(PWUI::Font(PWUI::ETextRole::Meta))
                    .ColorAndOpacity(FSlateColor(Style.Success))
                    .Visibility_Lambda([this, Rank]()
                    {
                        return CurrentRank.Get(0) == Rank ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
                    })
                ]
                + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(2.0f)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ProposedMarker", "Proposed"))
                    .Font(PWUI::Font(9, true))
                    .ColorAndOpacity(FSlateColor(Style.Accent))
                    .Visibility_Lambda([this, Rank]()
                    {
                        return ProposedRank.Get(0) == Rank && CurrentRank.Get(0) != Rank
                            ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
                    })
                ]
                + SOverlay::Slot()
                [
                    SNew(SBorder)
                    .BorderImage(&Style.BrushFocusRing)
                    .Padding(0.0f)
                    .Visibility_Lambda([Handle]()
                    {
                        const TSharedPtr<SButton> Position = Handle->Pin();
                        return Position.IsValid() && Position->HasKeyboardFocus()
                            ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
                    })
                ]
            ]
        ];
    *Handle = Button;
    Positions.Add(Button);
    return Button.ToSharedRef();
}

FReply SPWRankSelector::OnFocusReceived(const FGeometry& Geometry, const FFocusEvent& FocusEvent)
{
    OnInspect.ExecuteIfBound();
    const int32 Rank = FMath::Clamp(ProposedRank.Get(0), 0, Positions.Num() - 1);
    return Positions.IsValidIndex(Rank)
        ? FReply::Handled().SetUserFocus(Positions[Rank].ToSharedRef(), FocusEvent.GetCause())
        : FReply::Handled();
}

FReply SPWRankSelector::OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    // Rank buttons are true accessible focus targets, but the composite owns their keyboard behavior.
    // Handling Accept here prevents SButton from arming a second click for key release.
    if (Event.GetKey() == EKeys::Tab)
    {
        if (Event.IsRepeat()) return FReply::Handled();
        if (OnTraverse.IsBound())
            if (const auto Target = OnTraverse.Execute(Event.IsShiftDown()))
                return FReply::Handled().SetUserFocus(Target.ToSharedRef(), EFocusCause::Navigation);
        return FReply::Handled();
    }
    return OnKeyDown(Geometry, Event);
}

FReply SPWRankSelector::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    const FKey Key = Event.GetKey();
    if (Event.IsRepeat() && (Key == EKeys::Left || Key == EKeys::Right || Key == EKeys::Up || Key == EKeys::Down ||
        Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_DPad_Right ||
        Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_DPad_Down)) return FReply::Handled();
    if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left)
    {
        const int32 Rank = FMath::Clamp(ProposedRank.Get(0) - 1, 0, 4);
        FocusPosition(Rank, EFocusCause::Navigation);
        return Propose(Rank);
    }
    if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right)
    {
        const int32 Rank = FMath::Clamp(ProposedRank.Get(0) + 1, 0, 4);
        FocusPosition(Rank, EFocusCause::Navigation);
        return Propose(Rank);
    }
    if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up)
    {
        OnMoveRow.ExecuteIfBound(-1);
        return FReply::Handled();
    }
    if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down)
    {
        OnMoveRow.ExecuteIfBound(1);
        return FReply::Handled();
    }
    if (IsActivationKey(Key))
    {
        if (!Event.IsRepeat()) OnApply.ExecuteIfBound();
        return FReply::Handled();
    }
    if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
    {
        if (!HasPendingProposal()) return FReply::Unhandled();
        if (!Event.IsRepeat()) OnCancel.ExecuteIfBound();
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(Geometry, Event);
}

FReply SPWRankSelector::OnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event)
{
    // The activation belongs to this composite until release, so it cannot reach a rebuilt row underneath it.
    if (IsActivationKey(Event.GetKey())) return FReply::Handled();
    return SCompoundWidget::OnKeyUp(Geometry, Event);
}

FReply SPWRankSelector::OnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event)
{
    const float Value = Event.GetAnalogValue();
    if (Event.GetKey() == EKeys::Gamepad_LeftX)
    {
        if (OnAnalogGate.IsBound() && OnAnalogGate.Execute(true, Value))
        {
            const int32 Rank = FMath::Clamp(ProposedRank.Get(0) + (Value < 0.0f ? -1 : 1), 0, 4);
            FocusPosition(Rank, EFocusCause::Navigation);
            return Propose(Rank);
        }
        return FReply::Handled();
    }
    if (Event.GetKey() == EKeys::Gamepad_LeftY)
    {
        if (OnAnalogGate.IsBound() && OnAnalogGate.Execute(false, Value))
        {
            OnMoveRow.ExecuteIfBound(Value > 0.0f ? -1 : 1);
        }
        return FReply::Handled();
    }
    return SCompoundWidget::OnAnalogValueChanged(Geometry, Event);
}

FNavigationReply SPWRankSelector::OnNavigation(const FGeometry& Geometry, const FNavigationEvent& Event)
{
    // Treat the five roving descendants as one Tab stop: sequential traversal escapes this subtree.
    if (Event.GetNavigationType() == EUINavigation::Next || Event.GetNavigationType() == EUINavigation::Previous)
    {
        return OnTraverse.IsBound()
            ? FNavigationReply::Explicit(OnTraverse.Execute(Event.GetNavigationType() == EUINavigation::Previous))
            : FNavigationReply::Stop();
    }
    return FNavigationReply::Stop();
}

FReply SPWRankSelector::Propose(int32 Rank)
{
    OnPropose.ExecuteIfBound(FMath::Clamp(Rank, 0, 4));
    return FReply::Handled();
}

FReply SPWRankSelector::FocusAndPropose(int32 Rank)
{
    FocusPosition(Rank, EFocusCause::Mouse);
    OnInspect.ExecuteIfBound();
    return Propose(Rank);
}

void SPWRankSelector::FocusPosition(int32 Rank, EFocusCause Cause)
{
    if (FSlateApplication::IsInitialized() && Positions.IsValidIndex(Rank))
    {
        FSlateApplication::Get().SetKeyboardFocus(Positions[Rank], Cause);
    }
}

bool SPWRankSelector::HasPendingProposal() const
{
    return ProposedRank.Get(0) != CurrentRank.Get(0);
}

bool SPWRankSelector::IsActivationKey(const FKey& Key) const
{
    return Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom;
}

#undef LOCTEXT_NAMESPACE
