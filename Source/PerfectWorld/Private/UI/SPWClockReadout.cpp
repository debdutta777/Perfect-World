#include "UI/SPWClockReadout.h"

#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

void SPWClockReadout::Construct(const FArguments& InArgs)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TAttribute<FText> TimeText = InArgs._TimeText;
    const TAttribute<bool> Show = InArgs._ShowReadout;

    // Nothing here is clickable or focusable: the readout can never take input from the world behind it.
    SetVisibility(EVisibility::HitTestInvisible);

    ChildSlot
    [
        SNew(SBox)
        .HAlign(HAlign_Right)
        .VAlign(VAlign_Top)
        .Padding(FMargin(0.0f, PWUI::Spacing::Large, PWUI::Spacing::Large, 0.0f))
        .Visibility_Lambda([TimeText, Show]()
        {
            return Show.Get(false) && !TimeText.Get(FText::GetEmpty()).IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
        })
        [
            SNew(SBorder)
            .BorderImage(&Style.BrushToast)
            .Padding(FMargin(PWUI::Spacing::Compact, PWUI::Spacing::XSmall + 2.0f))
            [
                PWUI::MakeText(TAttribute<FText>::CreateLambda([TimeText]() { return TimeText.Get(FText::GetEmpty()); }),
                               PWUI::ETextRole::Meta, Style.TextSecondary)
            ]
        ]
    ];
}
