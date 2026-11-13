#include "UI/SPWConversationPanel.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Internationalization/Text.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldConversationPanel"

namespace
{
constexpr float PanelMaxWidth = 980.0f;      // wide enough for a long line at subtitle size, narrow enough to read
constexpr float PanelSideMargin = 24.0f;
constexpr float PanelBottomMargin = 32.0f;
constexpr int32 LineFontSize = 24;           // the line is the thing being read: larger than body text
constexpr float ChoiceAreaHeightFraction = 0.38f; // of the screen height; beyond this the choices scroll and Leave stays put
} // namespace

void SPWConversationPanel::Construct(const FArguments& InArgs)
{
    OnChoose = InArgs._OnChoose;
    OnLeave = InArgs._OnLeave;
    const PWUI::FStyle& Style = PWUI::FStyle::Get();

    // The compound widget fills the screen but only the panel itself takes hits; the street behind stays visible.
    SetVisibility(EVisibility::SelfHitTestInvisible);

    ChildSlot
    [
        SNew(SBox)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Bottom)
        .Padding(FMargin(PanelSideMargin, 0.0f, PanelSideMargin, PanelBottomMargin))
        .Visibility(EVisibility::SelfHitTestInvisible)
        [
            SAssignNew(PanelRoot, SBox)
            .WidthOverride_Lambda([this]()
            {
                const float Available = static_cast<float>(GetCachedGeometry().GetLocalSize().X);
                // Until the first layout pass the cached size is zero; use the full width then.
                return Available > 100.0f ? FMath::Min(PanelMaxWidth, Available - PanelSideMargin * 2.0f) : PanelMaxWidth;
            })
            [
                SNew(SBorder)
                .BorderImage(&Style.BrushPanel)
                .Padding(FMargin(28.0f, 20.0f, 28.0f, 16.0f))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return Speaker; }),
                                       PWUI::ETextRole::SectionHeading, Style.Accent, true)
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 16.0f)
                    [
                        PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return Line; }),
                                       LineFontSize, Style.TextPrimary, false, true)
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SBox)
                        .MaxDesiredHeight_Lambda([this]()
                        {
                            const float Height = static_cast<float>(GetCachedGeometry().GetLocalSize().Y);
                            return Height > 100.0f ? FMath::Max(140.0f, Height * ChoiceAreaHeightFraction) : 320.0f;
                        })
                        [
                            SNew(SScrollBox)
                            .ScrollBarStyle(&Style.ScrollBar)
                            .NavigationDestination(EDescendantScrollDestination::IntoView)
                            .ScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll)
                            + SScrollBox::Slot()
                            [
                                SAssignNew(ChoiceBox, SVerticalBox)
                            ]
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
                    [
                        PWUI::MakeRule()
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 16.0f, 0.0f)
                        [
                            PWUI::MakeText(LOCTEXT("InputHint", "Enter or A chooses. Esc or B leaves."),
                                           PWUI::ETextRole::Meta, Style.TextMuted, true)
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            PWUI::MakeButton(LOCTEXT("Leave", "Leave"), PWUI::EButtonKind::Secondary,
                                FOnClicked::CreateSPLambda(this, [this]()
                                {
                                    RequestLeave();
                                    return FReply::Handled();
                                }),
                                TAttribute<bool>::CreateLambda([this]() { return bCanLeave; }), 20, 120.0f)
                        ]
                    ]
                ]
            ]
        ]
    ];
}

FText SPWConversationPanel::FormatCost(const int32 Minutes)
{
    // The plural rule needs a number argument, not pre-formatted text.
    FFormatOrderedArguments Arguments;
    Arguments.Add(FFormatArgumentValue(Minutes));
    return FText::Format(LOCTEXT("ChoiceCost", "· {0} {0}|plural(one=minute,other=minutes)"), Arguments);
}

TSharedRef<SWidget> SPWConversationPanel::MakeChoiceRow(const int32 Index, const FPWConversationChoice& Choice)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
    Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 14.0f, 0.0f)
    [
        PWUI::MakeText(FText::AsNumber(Index + 1), PWUI::ETextRole::Meta, Style.TextMuted)
    ];
    Row->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center)
    [
        PWUI::MakeText(Choice.Label, 20, Style.TextPrimary, false, true)
    ];
    // A free exchange says nothing; a committed one says how long it takes before the player picks it.
    if (Choice.CostMinutes > 0)
    {
        Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(16.0f, 0.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(FormatCost(Choice.CostMinutes), PWUI::ETextRole::Control, Style.Accent)
        ];
    }
    return PWUI::MakeContentButton(
        Row, PWUI::EButtonKind::Card,
        FOnClicked::CreateSPLambda(this, [this, Index]()
        {
            PickChoice(Index);
            return FReply::Handled();
        }),
        false, true, FMargin(16.0f, 8.0f));
}

void SPWConversationPanel::SetView(const FPWConversationView& View)
{
    Speaker = View.Speaker;
    Line = View.Line;
    bCanLeave = View.bCanLeave;

    ChoiceIds.Reset();
    ChoiceBox->ClearChildren();
    for (int32 Index = 0; Index < View.Choices.Num(); ++Index)
    {
        const FPWConversationChoice& Choice = View.Choices[Index];
        ChoiceIds.Add(Choice.Id);
        ChoiceBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Index + 1 < View.Choices.Num() ? 8.0f : 0.0f)
        [
            MakeChoiceRow(Index, Choice)
        ];
    }
    // The first choice, or Leave when the line offers none. New widgets cannot take focus until arranged once.
    PWUI::FocusWhenReady(AsShared(), PanelRoot);
}

void SPWConversationPanel::PickChoice(const int32 Index)
{
    if (ChoiceIds.IsValidIndex(Index))
    {
        OnChoose.ExecuteIfBound(ChoiceIds[Index]);
    }
}

void SPWConversationPanel::RequestLeave()
{
    if (bCanLeave)
    {
        OnLeave.ExecuteIfBound();
    }
}

FReply SPWConversationPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    const bool bLeaveKey = Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right;
    int32 NumberIndex = INDEX_NONE;
    if (Key == EKeys::One) NumberIndex = 0;
    else if (Key == EKeys::Two) NumberIndex = 1;
    else if (Key == EKeys::Three) NumberIndex = 2;

    if (bLeaveKey || NumberIndex != INDEX_NONE)
    {
        // A held key auto-repeats; this panel reacts to the first press only.
        if (!InKeyEvent.IsRepeat())
        {
            if (bLeaveKey)
            {
                RequestLeave();
            }
            else
            {
                PickChoice(NumberIndex);
            }
        }
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
