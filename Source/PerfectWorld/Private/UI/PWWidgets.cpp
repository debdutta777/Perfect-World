#include "UI/PWWidgets.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "UI/PWStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PWWidgets"

namespace PWUI
{
namespace
{
struct FPendingFocus
{
    TWeakPtr<SWidget> Host;
    TWeakPtr<FActiveTimerHandle> Timer;
};

TArray<FPendingFocus> PendingFocus;

class SDialogShadow final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SDialogShadow) {}
    SLATE_END_ARGS()

    void Construct(const FArguments&)
    {
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Layers); ++Index)
        {
            const float Alpha = 0.026f + static_cast<float>(Index) * 0.012f;
            Layers[Index] = FSlateRoundedBoxBrush(FStyle::Get().Background.CopyWithNewOpacity(Alpha), 20.0f);
        }
    }

    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
                          FSlateWindowElementList& Elements, int32 LayerId, const FWidgetStyle& WidgetStyle,
                          bool) const override
    {
        const FVector2D Size = Geometry.GetLocalSize();
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Layers); ++Index)
        {
            const float Spread = 24.0f - static_cast<float>(Index) * 3.0f;
            const float Offset = 8.0f - static_cast<float>(Index);
            const FVector2D PaintSize(Size.X + Spread * 2.0f, Size.Y + Spread * 2.0f);
            const FVector2D PaintOffset(-Spread, Offset - Spread);
            FSlateDrawElement::MakeBox(Elements, LayerId, Geometry.ToPaintGeometry(PaintSize, FSlateLayoutTransform(PaintOffset)),
                                       &Layers[Index], ESlateDrawEffect::None, WidgetStyle.GetColorAndOpacityTint());
        }
        return LayerId;
    }

private:
    FSlateBrush Layers[6];
};
}

TSharedRef<STextBlock> MakeText(const TAttribute<FText>& Text, int32 Size, const FLinearColor& Color, bool bBold,
                                bool bWrap)
{
    return SNew(STextBlock)
        .Text(Text)
        .Font(Font(Size, bBold))
        .ColorAndOpacity(FSlateColor(Color))
        .TextShapingMethod(ETextShapingMethod::FullShaping)
        .AutoWrapText(bWrap);
}

TSharedRef<STextBlock> MakeText(const TAttribute<FText>& Text, ETextRole Role, const FLinearColor& Color, bool bWrap)
{
    return SNew(STextBlock).Text(Text).Font(Font(Role)).ColorAndOpacity(FSlateColor(Color))
        .TextShapingMethod(ETextShapingMethod::FullShaping).AutoWrapText(bWrap);
}

TSharedRef<SWidget> MakeContentButton(TSharedRef<SWidget> Content, EButtonKind Kind, FOnClicked OnClicked,
                                      TAttribute<bool> bSelected, TAttribute<bool> bEnabled, const FMargin& Padding)
{
    // The lambdas below ask the button itself whether it is hovered, pressed or
    // focused, so they hold a handle that is filled in once the button exists.
    const TSharedRef<TWeakPtr<SButton>> Handle = MakeShared<TWeakPtr<SButton>>();

    const auto BrushFor = [Handle, Kind, bSelected, bEnabled]() -> const FSlateBrush*
    {
        const FStyle& Style = FStyle::Get();
        const TSharedPtr<SButton> Button = Handle->Pin();
        const bool bOn = bEnabled.Get(true);
        const bool bHot = Button.IsValid() && Button->IsHovered();
        const bool bDown = Button.IsValid() && Button->IsPressed();
        const bool bPicked = bSelected.Get(false);

        switch (Kind)
        {
        case EButtonKind::Primary:
            if (!bOn) return &Style.BrushDisabled;
            if (bDown) return &Style.BrushPrimaryPressed;
            return bHot ? &Style.BrushPrimaryHover : &Style.BrushPrimary;
        case EButtonKind::Card:
            if (!bOn) return &Style.BrushCardDisabled;
            if (bDown) return &Style.BrushCardPressed;
            if (bPicked) return bHot ? &Style.BrushCardSelectedHover : &Style.BrushCardSelected;
            return bHot ? &Style.BrushCardHover : &Style.BrushCard;
        case EButtonKind::TextRow:
            if (!bOn) return &Style.BrushTextRow;
            if (bDown) return &Style.BrushTextRowPressed;
            if (bPicked) return bHot ? &Style.BrushCardSelectedHover : &Style.BrushCardSelected;
            return bHot ? &Style.BrushTextRowHover : &Style.BrushTextRow;
        case EButtonKind::Chip:
            if (!bOn) return &Style.BrushDisabled;
            if (bDown) return &Style.BrushSecondaryPressed;
            if (bPicked) return bHot ? &Style.BrushCardSelectedHover : &Style.BrushCardSelected;
            return bHot ? &Style.BrushSecondaryHover : &Style.BrushSecondary;
        case EButtonKind::Secondary:
        default:
            if (!bOn) return &Style.BrushDisabled;
            if (bDown) return &Style.BrushSecondaryPressed;
            return bHot ? &Style.BrushSecondaryHover : &Style.BrushSecondary;
        }
    };

    const auto ForegroundFor = [Kind, bEnabled]() -> FSlateColor
    {
        const FStyle& Style = FStyle::Get();
        if (!bEnabled.Get(true))
        {
            return FSlateColor(Style.TextMuted);
        }
        return FSlateColor(Kind == EButtonKind::Primary ? Style.OnAccent : Style.TextPrimary);
    };

    TSharedPtr<SButton> Button;
    SAssignNew(Button, SButton)
        .ButtonStyle(&FStyle::Get().NoButton)
        .ContentPadding(FMargin(0.0f))
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Fill)
        .OnClicked(OnClicked)
        .IsEnabled(bEnabled)
        [
            SNew(SBox).MinDesiredHeight(44.0f)
            [
                SNew(SOverlay)
                + SOverlay::Slot()
                [
                    SNew(SBorder).Padding(Padding).ShowEffectWhenDisabled(false)
                    .BorderImage_Lambda(BrushFor).ForegroundColor_Lambda(ForegroundFor)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[Content]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.0f, 0.0f, 0.0f, 0.0f)
                        [
                            SNew(STextBlock).Text(LOCTEXT("SelectedCheckMark", "\u2713")).Font(Font(ETextRole::Control))
                            .ColorAndOpacity(FSlateColor(FStyle::Get().Accent))
                            .Visibility_Lambda([Kind, bSelected]()
                            {
                                return (Kind == EButtonKind::Card || Kind == EButtonKind::TextRow || Kind == EButtonKind::Chip) && bSelected.Get(false)
                                    ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
                            })
                        ]
                    ]
                ]
                + SOverlay::Slot()
                [
                    SNew(SBorder).Padding(0.0f).BorderImage(&FStyle::Get().BrushFocusRing)
                    .Visibility_Lambda([Handle]()
                    {
                        const TSharedPtr<SButton> FocusButton = Handle->Pin();
                        return FocusButton.IsValid() && FocusButton->HasKeyboardFocus()
                            ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
                    })
                ]
                + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center)
                [
                    SNew(SBox).WidthOverride(3.0f).HeightOverride(18.0f)
                    .Visibility_Lambda([Handle, Kind]()
                    {
                        const TSharedPtr<SButton> RowButton = Handle->Pin();
                        return Kind == EButtonKind::TextRow && RowButton.IsValid()
                            && (RowButton->IsHovered() || RowButton->HasKeyboardFocus())
                            ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
                    })
                    [
                        SNew(SImage).Image(&FStyle::Get().BrushAccentRule)
                    ]
                ]
            ]
        ];
    *Handle = Button;
#if WITH_ACCESSIBILITY
    // Weak: Slate keeps this attribute in a locked registry, and a strong reference here made destroying the button
    // destroy Content from inside that registry's removal, re-entering its lock (a game-thread deadlock on entering
    // the world as the title screen was torn down).
    const TWeakPtr<SWidget> WeakContent = Content;
    Button->SetAccessibleBehavior(EAccessibleBehavior::Custom, TAttribute<FText>::CreateLambda([WeakContent, bSelected]()
    {
        const TSharedPtr<SWidget> Pinned = WeakContent.Pin();
        const FText Description = Pinned.IsValid() ? Pinned->GetAccessibleText(EAccessibleType::Summary) : FText::GetEmpty();
        return bSelected.Get(false) ? FText::Format(LOCTEXT("AccessibleSelected", "{0}, Selected"), Description) : Description;
    }));
#endif
    return Button.ToSharedRef();
}

TSharedRef<SWidget> MakeButton(const TAttribute<FText>& Label, EButtonKind Kind, FOnClicked OnClicked,
                               TAttribute<bool> bEnabled, int32 FontSize, float MinWidth, TAttribute<bool> bSelected)
{
    const TSharedRef<SWidget> Content =
        SNew(SBox)
        .HAlign(Kind == EButtonKind::TextRow ? HAlign_Left : HAlign_Center)
        .VAlign(VAlign_Center)
        .MinDesiredWidth(MinWidth)
        [
            SNew(STextBlock)
            .Text(Label)
            .Font(Font(FontSize, EFontWeight::Medium))
            .ColorAndOpacity(FSlateColor::UseForeground())
            .Justification(Kind == EButtonKind::TextRow ? ETextJustify::Left : ETextJustify::Center)
            .AutoWrapText(true)
        ];
    return MakeContentButton(Content, Kind, OnClicked, bSelected, bEnabled, FMargin(22.0f, 12.0f));
}

TSharedRef<SWidget> MakePanel(TSharedRef<SWidget> Content, const FMargin& Padding)
{
    return SNew(SBorder)
        .BorderImage(&FStyle::Get().BrushPanel)
        .Padding(Padding)
        [
            Content
        ];
}

TSharedRef<SWidget> MakeDialog(TSharedRef<SWidget> Content, const FMargin& Padding)
{
    return SNew(SOverlay)
        + SOverlay::Slot()
        [
            SNew(SDialogShadow)
        ]
        + SOverlay::Slot().Padding(0.0f, 0.0f, 0.0f, 4.0f)
        [
            SNew(SBorder).BorderImage(&FStyle::Get().BrushDialog).Padding(Padding)[Content]
        ];
}

TSharedRef<SWidget> MakeRule()
{
    return SNew(SBox)
        .HeightOverride(1.0f)
        [
            SNew(SImage).Image(&FStyle::Get().BrushRule)
        ];
}

TSharedPtr<SWidget> FindFirstFocusable(const TSharedRef<SWidget>& Root)
{
    if (!Root->IsEnabled() || !Root->GetVisibility().IsVisible())
    {
        return nullptr;
    }
    if (Root->SupportsKeyboardFocus())
    {
        return Root;
    }
    FChildren* Children = Root->GetChildren();
    if (Children == nullptr)
    {
        return nullptr;
    }
    for (int32 Index = 0; Index < Children->Num(); ++Index)
    {
        if (const TSharedPtr<SWidget> Found = FindFirstFocusable(Children->GetChildAt(Index)))
        {
            return Found;
        }
    }
    return nullptr;
}

void FocusFirstFocusable(const TSharedRef<SWidget>& Root)
{
    if (!FSlateApplication::IsInitialized())
    {
        return;
    }
    if (const TSharedPtr<SWidget> Target = FindFirstFocusable(Root))
    {
        FSlateApplication::Get().SetKeyboardFocus(Target, EFocusCause::SetDirectly);
    }
}

void FocusWhenReady(const TSharedRef<SWidget>& Host, const TWeakPtr<SWidget>& Target)
{
    CancelPendingFocus();
    const TSharedRef<int32> Attempts = MakeShared<int32>(0);
    const TSharedRef<FActiveTimerHandle> Timer = Host->RegisterActiveTimer(
        0.05f,
        FWidgetActiveTimerDelegate::CreateLambda([Target, Attempts](double, float)
        {
            const TSharedPtr<SWidget> Root = Target.Pin();
            if (!Root.IsValid() || !FSlateApplication::IsInitialized() || ++(*Attempts) > 20)
            {
                return EActiveTimerReturnType::Stop;
            }
            const TSharedPtr<SWidget> Found = FindFirstFocusable(Root.ToSharedRef());
            if (!Found.IsValid())
            {
                return EActiveTimerReturnType::Continue;
            }
            FSlateApplication::Get().SetKeyboardFocus(Found, EFocusCause::SetDirectly);
            return FSlateApplication::Get().GetKeyboardFocusedWidget() == Found || Found->HasFocusedDescendants() ? EActiveTimerReturnType::Stop
                                                                              : EActiveTimerReturnType::Continue;
        }));
    PendingFocus.Add({Host, Timer});
}

void CancelPendingFocus()
{
    for (const FPendingFocus& Pending : PendingFocus)
    {
        const TSharedPtr<SWidget> Host = Pending.Host.Pin();
        const TSharedPtr<FActiveTimerHandle> Timer = Pending.Timer.Pin();
        if (Host.IsValid() && Timer.IsValid()) Host->UnRegisterActiveTimer(Timer.ToSharedRef());
    }
    PendingFocus.Reset();
}

void CancelPendingFocus(const TSharedRef<SWidget>& Host)
{
    for (int32 Index = PendingFocus.Num() - 1; Index >= 0; --Index)
    {
        const TSharedPtr<SWidget> PendingHost = PendingFocus[Index].Host.Pin();
        const TSharedPtr<FActiveTimerHandle> Timer = PendingFocus[Index].Timer.Pin();
        if (!PendingHost.IsValid() || PendingHost == Host)
        {
            if (PendingHost.IsValid() && Timer.IsValid()) PendingHost->UnRegisterActiveTimer(Timer.ToSharedRef());
            PendingFocus.RemoveAtSwap(Index);
        }
    }
}
} // namespace PWUI

#undef LOCTEXT_NAMESPACE
