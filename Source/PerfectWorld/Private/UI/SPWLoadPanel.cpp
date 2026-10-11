#include "UI/SPWLoadPanel.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldLoadPanel"

void SPWLoadPanel::Construct(const FArguments& InArgs, UPerfectWorldCharacterCreatorSubsystem* InCreator)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    OnLoadSlot = InArgs._OnLoadSlot;
    OnBack = InArgs._OnBack;

    const int32 SlotCount = UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotCount();
    const TSharedRef<SVerticalBox> Slots = SNew(SVerticalBox);
    for (int32 Slot = 0; Slot < SlotCount; ++Slot)
    {
        const bool bOccupied = UPerfectWorldCharacterCreatorSubsystem::IsSaveSlotOccupied(Slot);
        Occupied.Add(bOccupied);
        Descriptions.Add(InCreator != nullptr ? InCreator->DescribeSaveSlot(Slot) : LOCTEXT("NoCreator", "Unavailable"));

        const TSharedRef<SWidget> Content =
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                PWUI::MakeText(FText::Format(LOCTEXT("SlotFmt", "SLOT {0}"), FText::AsNumber(Slot + 1)), 13, Style.Accent, true)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
            [
                PWUI::MakeText(Descriptions[Slot], 22, bOccupied ? Style.TextPrimary : Style.TextMuted, bOccupied, true)
            ];

        Slots->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
        [
            PWUI::MakeContentButton(
                Content, PWUI::EButtonKind::Card,
                FOnClicked::CreateLambda([this, Slot]()
                {
                    if (OnLoadSlot.IsBound())
                    {
                        Status = OnLoadSlot.Execute(Slot);
                    }
                    return FReply::Handled();
                }),
                false, bOccupied, FMargin(24.0f, 18.0f))
        ];
    }

    ChildSlot
    [
        SNew(SBox)
        .WidthOverride(760.0f)
        [
            PWUI::MakePanel(
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    PWUI::MakeText(LOCTEXT("Title", "Load"), 36, Style.TextPrimary, true)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 20.0f)
                [
                    PWUI::MakeText(LOCTEXT("Subtitle", "Choose a save to continue from."), 17, Style.TextSecondary)
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    Slots
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
                [
                    PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return Status; }), 16, Style.Danger, true, true)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 0.0f)
                [
                    SNew(SBox)
                    .HAlign(HAlign_Left)
                    [
                        PWUI::MakeButton(LOCTEXT("Back", "Back"), PWUI::EButtonKind::Secondary,
                                         FOnClicked::CreateLambda([this]()
                                         {
                                             OnBack.ExecuteIfBound();
                                             return FReply::Handled();
                                         }),
                                         true, 20, 140.0f)
                    ]
                ],
                FMargin(36.0f, 30.0f))
        ]
    ];

    PWUI::FocusWhenReady(AsShared(), AsShared());
}

FReply SPWLoadPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    // A held key auto-repeats; menus react to the first press only.
    if (InKeyEvent.IsRepeat())
    {
        return FReply::Handled();
    }
    if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
    {
        OnBack.ExecuteIfBound();
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
