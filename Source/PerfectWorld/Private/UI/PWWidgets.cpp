#include "UI/PWWidgets.h"

#include "Framework/Application/SlateApplication.h"
#include "UI/PWStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

namespace PWUI
{
TSharedRef<STextBlock> MakeText(const TAttribute<FText>& Text, int32 Size, const FLinearColor& Color, bool bBold,
                                bool bWrap)
{
    return SNew(STextBlock)
        .Text(Text)
        .Font(Font(Size, bBold))
        .ColorAndOpacity(FSlateColor(Color))
        .AutoWrapText(bWrap);
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
        const bool bFocus = Button.IsValid() && Button->HasKeyboardFocus();
        const bool bDown = Button.IsValid() && Button->IsPressed();
        const bool bPicked = bSelected.Get(false);

        switch (Kind)
        {
        case EButtonKind::Primary:
            if (!bOn) return &Style.BrushDisabled;
            if (bDown) return &Style.BrushPrimaryPressed;
            return (bHot || bFocus) ? &Style.BrushPrimaryHover : &Style.BrushPrimary;
        case EButtonKind::Card:
            if (!bOn) return &Style.BrushCardDisabled;
            if (bPicked) return (bHot || bFocus) ? &Style.BrushCardSelectedHover : &Style.BrushCardSelected;
            if (bFocus) return &Style.BrushFocusRing;
            return bHot ? &Style.BrushCardHover : &Style.BrushCard;
        case EButtonKind::Chip:
            if (!bOn) return &Style.BrushDisabled;
            if (bPicked) return (bHot || bFocus) ? &Style.BrushCardSelectedHover : &Style.BrushCardSelected;
            if (bDown) return &Style.BrushSecondaryPressed;
            return (bHot || bFocus) ? &Style.BrushSecondaryHover : &Style.BrushSecondary;
        case EButtonKind::Secondary:
        default:
            if (!bOn) return &Style.BrushDisabled;
            if (bDown) return &Style.BrushSecondaryPressed;
            return (bHot || bFocus) ? &Style.BrushSecondaryHover : &Style.BrushSecondary;
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
            SNew(SBorder)
            .Padding(Padding)
            .ShowEffectWhenDisabled(false)
            .BorderImage_Lambda(BrushFor)
            .ForegroundColor_Lambda(ForegroundFor)
            [
                Content
            ]
        ];
    *Handle = Button;
    return Button.ToSharedRef();
}

TSharedRef<SWidget> MakeButton(const TAttribute<FText>& Label, EButtonKind Kind, FOnClicked OnClicked,
                               TAttribute<bool> bEnabled, int32 FontSize, float MinWidth, TAttribute<bool> bSelected)
{
    const TSharedRef<SWidget> Content =
        SNew(SBox)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        .MinDesiredWidth(MinWidth)
        [
            SNew(STextBlock)
            .Text(Label)
            .Font(Font(FontSize, true))
            .ColorAndOpacity(FSlateColor::UseForeground())
            .Justification(ETextJustify::Center)
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
    if (Root->SupportsKeyboardFocus() && Root->IsEnabled() && Root->GetVisibility().IsVisible())
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
    const TSharedRef<int32> Attempts = MakeShared<int32>(0);
    Host->RegisterActiveTimer(
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
            return FSlateApplication::Get().GetKeyboardFocusedWidget() == Found ? EActiveTimerReturnType::Stop
                                                                              : EActiveTimerReturnType::Continue;
        }));
}
} // namespace PWUI
