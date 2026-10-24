#include "UI/SPWReadingPanel.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Internationalization/Text.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldReadingPanel"

namespace
{
constexpr float SheetMaxWidth = 860.0f;          // a comfortable line length for 20 to 32 point text
constexpr float SheetSideMargin = 24.0f;
constexpr float TextAreaHeightFraction = 0.5f;   // of the screen height; the page scrolls inside it, the footer stays put
constexpr float KeyScrollStep = 120.0f;          // Slate units per arrow key press when a long page scrolls
constexpr int32 TextSizes[] = {20, 26, 32};
constexpr int32 TextSizeCount = UE_ARRAY_COUNT(TextSizes);

/** Chosen with A- and A+; kept for the session so a player who needs larger text sets it once. */
int32 GTextSizeIndex = 0;

/** Ink on paper. The paper is the palette's ivory, the ink its near-black, so contrast is high without a pure white page. */
FLinearColor PaperColor() { return PWUI::FStyle::Get().TextPrimary; }
FLinearColor InkColor() { return PWUI::FStyle::Get().OnAccent; }
FLinearColor MutedInkColor() { return PWUI::Srgb(0x4E, 0x55, 0x52); }
} // namespace

void SPWReadingPanel::Construct(const FArguments& InArgs)
{
    OnClose = InArgs._OnClose;
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    PaperBrush = FSlateRoundedBoxBrush(PaperColor(), 10.0f, Style.Accent, 1.5f);

    // The compound widget fills the screen; the scrim below it takes every click so the street cannot be clicked through.
    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(&Style.BrushScrim)
        .Padding(FMargin(SheetSideMargin))
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SAssignNew(PanelRoot, SBox)
            .WidthOverride_Lambda([this]()
            {
                const float Available = static_cast<float>(GetCachedGeometry().GetLocalSize().X);
                // Until the first layout pass the cached size is zero; use the full width then.
                return Available > 100.0f ? FMath::Min(SheetMaxWidth, Available - SheetSideMargin * 2.0f) : SheetMaxWidth;
            })
            [
                SNew(SBorder)
                .BorderImage(&PaperBrush)
                .Padding(FMargin(32.0f, 24.0f, 32.0f, 20.0f))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 16.0f, 0.0f)
                        [
                            PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return Title; }),
                                           PWUI::ETextRole::PageHeading, InkColor(), true)
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            PWUI::MakeText(TAttribute<FText>::CreateLambda([this]()
                            {
                                return FText::Format(LOCTEXT("PageOf", "Page {0} of {1}"),
                                                     FText::AsNumber(PageIndex + 1), FText::AsNumber(FMath::Max(1, Pages.Num())));
                            }), PWUI::ETextRole::Control, MutedInkColor())
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 12.0f)
                    [
                        SNew(SBorder).BorderImage(&Style.BrushAccentRule).Padding(FMargin(0.0f, 1.0f, 0.0f, 0.0f))
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SBox)
                        .HeightOverride_Lambda([this]()
                        {
                            const float Height = static_cast<float>(GetCachedGeometry().GetLocalSize().Y);
                            return Height > 100.0f ? FMath::Max(160.0f, Height * TextAreaHeightFraction) : 320.0f;
                        })
                        [
                            SAssignNew(TextScroll, SScrollBox)
                            .ScrollBarStyle(&Style.ScrollBar)
                            + SScrollBox::Slot().Padding(0.0f, 0.0f, 14.0f, 0.0f)
                            [
                                SNew(STextBlock)
                                .Text(TAttribute<FText>::CreateLambda([this]()
                                {
                                    return Pages.IsValidIndex(PageIndex) ? Pages[PageIndex] : FText::GetEmpty();
                                }))
                                .Font(TAttribute<FSlateFontInfo>::CreateLambda([]()
                                {
                                    return PWUI::Font(TextSizes[GTextSizeIndex], PWUI::EFontWeight::Regular);
                                }))
                                .ColorAndOpacity(FSlateColor(InkColor()))
                                .TextShapingMethod(ETextShapingMethod::FullShaping)
                                .LineHeightPercentage(1.2f)
                                .AutoWrapText(true)
                            ]
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
                    [
                        SNew(SBorder).BorderImage(&Style.BrushAccentRule).Padding(FMargin(0.0f, 1.0f, 0.0f, 0.0f))
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
                    [
                        PWUI::MakeText(LOCTEXT("InputHint", "Left and Right turn the page. Esc or B closes."),
                                       PWUI::ETextRole::Meta, MutedInkColor(), true)
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
                        [
                            PWUI::MakeButton(LOCTEXT("SmallerText", "A-"), PWUI::EButtonKind::Secondary,
                                FOnClicked::CreateSPLambda(this, [this]()
                                {
                                    StepTextSize(-1);
                                    return FReply::Handled();
                                }),
                                TAttribute<bool>::CreateLambda([]() { return GTextSizeIndex > 0; }), 18, 44.0f)
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            PWUI::MakeButton(LOCTEXT("LargerText", "A+"), PWUI::EButtonKind::Secondary,
                                FOnClicked::CreateSPLambda(this, [this]()
                                {
                                    StepTextSize(1);
                                    return FReply::Handled();
                                }),
                                TAttribute<bool>::CreateLambda([]() { return GTextSizeIndex + 1 < TextSizeCount; }), 18, 44.0f)
                        ]
                        + SHorizontalBox::Slot().FillWidth(1.0f)
                        [
                            SNew(SSpacer)
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
                        [
                            PWUI::MakeButton(LOCTEXT("Previous", "Previous"), PWUI::EButtonKind::Secondary,
                                FOnClicked::CreateSPLambda(this, [this]()
                                {
                                    StepPage(-1);
                                    return FReply::Handled();
                                }),
                                TAttribute<bool>::CreateLambda([this]() { return HasPrevious(); }), 18, 96.0f)
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
                        [
                            SAssignNew(NextHolder, SBox)
                            [
                                PWUI::MakeButton(LOCTEXT("Next", "Next"), PWUI::EButtonKind::Primary,
                                    FOnClicked::CreateSPLambda(this, [this]()
                                    {
                                        StepPage(1);
                                        return FReply::Handled();
                                    }),
                                    TAttribute<bool>::CreateLambda([this]() { return HasNext(); }), 18, 96.0f)
                            ]
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            SAssignNew(CloseHolder, SBox)
                            [
                                PWUI::MakeButton(LOCTEXT("Close", "Close"), PWUI::EButtonKind::Secondary,
                                    FOnClicked::CreateSPLambda(this, [this]()
                                    {
                                        RequestClose();
                                        return FReply::Handled();
                                    }),
                                    true, 18, 96.0f)
                            ]
                        ]
                    ]
                ]
            ]
        ]
    ];
}

void SPWReadingPanel::SetContent(const FText& InTitle, const TArray<FText>& InPages)
{
    Title = InTitle;
    Pages = InPages;
    ShowPage(0);
}

void SPWReadingPanel::ShowPage(const int32 NewIndex)
{
    PageIndex = Pages.Num() > 0 ? FMath::Clamp(NewIndex, 0, Pages.Num() - 1) : 0;
    if (TextScroll.IsValid())
    {
        TextScroll->SetScrollOffset(0.0f);
    }
    // Next when there is a next page, otherwise Close. New widgets cannot take focus until arranged once.
    PWUI::FocusWhenReady(AsShared(), HasNext() ? TWeakPtr<SWidget>(NextHolder) : TWeakPtr<SWidget>(CloseHolder));
}

void SPWReadingPanel::StepPage(const int32 Delta)
{
    const int32 Target = PageIndex + Delta;
    if (Target != PageIndex && Pages.IsValidIndex(Target))
    {
        ShowPage(Target);
    }
}

void SPWReadingPanel::StepTextSize(const int32 Delta)
{
    GTextSizeIndex = FMath::Clamp(GTextSizeIndex + Delta, 0, TextSizeCount - 1);
    // The button that was just pressed may have become disabled; give focus a home that is still enabled.
    PWUI::FocusWhenReady(AsShared(), HasNext() ? TWeakPtr<SWidget>(NextHolder) : TWeakPtr<SWidget>(CloseHolder));
}

void SPWReadingPanel::ScrollText(const float Delta)
{
    if (TextScroll.IsValid())
    {
        TextScroll->SetScrollOffset(FMath::Max(0.0f, TextScroll->GetScrollOffset() + Delta));
    }
}

void SPWReadingPanel::RequestClose()
{
    OnClose.ExecuteIfBound();
}

FReply SPWReadingPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    const bool bCloseKey = Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right;
    const bool bPreviousKey = Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftShoulder
        || Key == EKeys::Gamepad_LeftStick_Left;
    const bool bNextKey = Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_RightShoulder
        || Key == EKeys::Gamepad_LeftStick_Right;

    if (bCloseKey || bPreviousKey || bNextKey)
    {
        // A held key auto-repeats; this panel reacts to the first press only.
        if (!InKeyEvent.IsRepeat())
        {
            if (bCloseKey)
            {
                RequestClose();
            }
            else
            {
                StepPage(bNextKey ? 1 : -1);
            }
        }
        return FReply::Handled();
    }

    // A long page (or large text) scrolls; repeats are welcome here.
    if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up)
    {
        ScrollText(-KeyScrollStep);
        return FReply::Handled();
    }
    if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down)
    {
        ScrollText(KeyScrollStep);
        return FReply::Handled();
    }
    if (Key == EKeys::PageUp || Key == EKeys::PageDown)
    {
        const float Height = static_cast<float>(MyGeometry.GetLocalSize().Y);
        ScrollText((Key == EKeys::PageUp ? -1.0f : 1.0f) * FMath::Max(KeyScrollStep, Height * TextAreaHeightFraction * 0.9f));
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
