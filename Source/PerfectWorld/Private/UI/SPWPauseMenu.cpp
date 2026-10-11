#include "UI/SPWPauseMenu.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "PerfectWorldFrontEndHUD.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldPauseMenu"

void SPWPauseMenu::Construct(const FArguments& InArgs)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    Hud = InArgs._Hud;

    const auto MenuButton = [](const FText& Label, PWUI::EButtonKind Kind, FOnClicked OnClicked) -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .WidthOverride(360.0f)
            [
                PWUI::MakeButton(Label, Kind, OnClicked, true, 22, 0.0f)
            ];
    };

    ChildSlot
    [
        PWUI::MakePanel(
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [
                PWUI::MakeText(LOCTEXT("Title", "Paused"), 38, Style.TextPrimary, true)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 24.0f, 0.0f, 12.0f)
            [
                MenuButton(LOCTEXT("Resume", "Resume"), PWUI::EButtonKind::Primary,
                           FOnClicked::CreateLambda([this]()
                           {
                               if (APerfectWorldFrontEndHUD* Owner = Hud.Get())
                               {
                                   Owner->ResumeGame();
                               }
                               return FReply::Handled();
                           }))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
            [
                MenuButton(LOCTEXT("Save", "Save"), PWUI::EButtonKind::Secondary,
                           FOnClicked::CreateLambda([this]()
                           {
                               if (APerfectWorldFrontEndHUD* Owner = Hud.Get())
                               {
                                   bool bSucceeded = false;
                                   Status = Owner->SaveActiveGame(bSucceeded);
                                   bStatusIsError = !bSucceeded;
                               }
                               return FReply::Handled();
                           }))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
            [
                MenuButton(LOCTEXT("QuitToTitle", "Quit to Title"), PWUI::EButtonKind::Secondary,
                           FOnClicked::CreateLambda([this]()
                           {
                               if (APerfectWorldFrontEndHUD* Owner = Hud.Get())
                               {
                                   Owner->ReturnToTitle();
                               }
                               return FReply::Handled();
                           }))
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                MenuButton(LOCTEXT("Quit", "Quit"), PWUI::EButtonKind::Secondary,
                           FOnClicked::CreateLambda([this]()
                           {
                               if (APerfectWorldFrontEndHUD* Owner = Hud.Get())
                               {
                                   Owner->QuitGame();
                               }
                               return FReply::Handled();
                           }))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 0.0f)
            [
                SNew(SBox)
                .WidthOverride(360.0f)
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]() { return Status; })
                    .Font(PWUI::Font(16, true))
                    .ColorAndOpacity_Lambda([this]() { return FSlateColor(bStatusIsError ? PWUI::FStyle::Get().Danger : PWUI::FStyle::Get().Good); })
                    .AutoWrapText(true)
                    .Justification(ETextJustify::Center)
                ]
            ],
            FMargin(48.0f, 36.0f))
    ];

    PWUI::FocusWhenReady(AsShared(), AsShared());
}

FReply SPWPauseMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_Special_Right)
    {
        if (APerfectWorldFrontEndHUD* Owner = Hud.Get())
        {
            Owner->ResumeGame();
        }
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
