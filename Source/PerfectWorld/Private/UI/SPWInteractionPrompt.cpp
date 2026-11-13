#include "UI/SPWInteractionPrompt.h"

#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldInteractionPrompt"

namespace
{
constexpr float PromptBottomMargin = 150.0f; // clear of the crosshair area and of the conversation panel's footprint
} // namespace

void SPWInteractionPrompt::Construct(const FArguments& InArgs)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TAttribute<bool> Show = InArgs._ShowPrompt;
    const TAttribute<FText> Verb = InArgs._Verb;
    const TAttribute<FText> CycleHint = InArgs._CycleHint;

    // Nothing here is clickable or focusable: the prompt can never take input from the world behind it.
    SetVisibility(EVisibility::HitTestInvisible);

    ChildSlot
    [
        SNew(SBox)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Bottom)
        .Padding(FMargin(0.0f, 0.0f, 0.0f, PromptBottomMargin))
        .Visibility_Lambda([Show]() { return Show.Get(false) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
        [
            SNew(SBorder)
            .BorderImage(&Style.BrushToast)
            .Padding(FMargin(20.0f, 10.0f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        PWUI::MakeText(LOCTEXT("InteractKeys", "E / X"), PWUI::ETextRole::Control, Style.Accent)
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f)
                    [
                        PWUI::MakeText(LOCTEXT("Separator", "·"), PWUI::ETextRole::Control, Style.TextMuted)
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        PWUI::MakeText(TAttribute<FText>::CreateLambda([Verb]() { return Verb.Get(FText::GetEmpty()); }),
                                       20, Style.TextPrimary)
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 4.0f, 0.0f, 0.0f)
                [
                    SNew(SBox)
                    .Visibility_Lambda([CycleHint]() { return CycleHint.Get(FText::GetEmpty()).IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
                    [
                        PWUI::MakeText(TAttribute<FText>::CreateLambda([CycleHint]() { return CycleHint.Get(FText::GetEmpty()); }),
                                       PWUI::ETextRole::Meta, Style.TextSecondary)
                    ]
                ]
            ]
        ]
    ];
}

#undef LOCTEXT_NAMESPACE
