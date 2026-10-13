#include "UI/SPWPauseMenu.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "PerfectWorldFrontEndHUD.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "UI/SPWAwakeningSigil.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldPauseMenu"

void SPWPauseMenu::Construct(const FArguments& InArgs)
{
    Hud = InArgs._Hud;
    MenuContent = BuildMenuContent();

    ChildSlot
    [
        SAssignNew(ContentHolder, SBox)
        [
            MenuContent.ToSharedRef()
        ]
    ];

    PWUI::FocusWhenReady(AsShared(), ContentHolder);
}

TSharedRef<SWidget> SPWPauseMenu::BuildMenuContent()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const auto MenuButton = [](const FText& Label, PWUI::EButtonKind Kind, FOnClicked OnClicked) -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .WidthOverride(360.0f)
            [
                PWUI::MakeButton(Label, Kind, OnClicked, true, 22, 0.0f)
            ];
    };

    ReturnToTitleButton = PWUI::MakeButton(
        LOCTEXT("ReturnToTitle", "Return to Title"), PWUI::EButtonKind::Secondary,
        FOnClicked::CreateSPLambda(this, [this]()
        {
            BeginExitConfirmation(EPendingExit::ReturnToTitle, ReturnToTitleButton.ToSharedRef());
            return FReply::Handled();
        }), true, 22, 0.0f);
    QuitButton = PWUI::MakeButton(
        LOCTEXT("Quit", "Quit"), PWUI::EButtonKind::Secondary,
        FOnClicked::CreateSPLambda(this, [this]()
        {
            BeginExitConfirmation(EPendingExit::QuitGame, QuitButton.ToSharedRef());
            return FReply::Handled();
        }), true, 22, 0.0f);

    TSharedRef<SWidget> Content = PWUI::MakeBracketPanel(
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 12)
            [SNew(SPWAwakeningSigil).SizeClass(EPWSigilSizeClass::Seal).Diameter(48)]
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
                SNew(SBox)
                .WidthOverride(360.0f)
                [
                    ReturnToTitleButton.ToSharedRef()
                ]
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SBox)
                .WidthOverride(360.0f)
                [
                    QuitButton.ToSharedRef()
                ]
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
            FMargin(48.0f, 36.0f));

    return Content;
}

TSharedRef<SWidget> SPWPauseMenu::BuildExitConfirmation()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const FText Title = PendingExit == EPendingExit::ReturnToTitle
        ? LOCTEXT("ReturnToTitleConfirmTitle", "Return to Title?")
        : LOCTEXT("QuitConfirmTitle", "Quit Perfect World?");
    const FText ConfirmLabel = PendingExit == EPendingExit::ReturnToTitle
        ? LOCTEXT("ConfirmReturnToTitle", "Return to Title")
        : LOCTEXT("ConfirmQuit", "Quit");

    return PWUI::MakeDialog(
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
        [
            PWUI::MakeText(Title, 30, Style.TextPrimary, true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 24.0f)
        [
            PWUI::MakeText(LOCTEXT("UnsavedProgressWarning", "Unsaved progress since your last save will be lost."),
                           18, Style.TextSecondary, false, true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
        [
            SNew(SBox).WidthOverride(360.0f)
            [
                PWUI::MakeButton(LOCTEXT("CancelExit", "Cancel"), PWUI::EButtonKind::Primary,
                    FOnClicked::CreateSPLambda(this, [this]()
                    {
                        CancelExitConfirmation();
                        return FReply::Handled();
                    }), true, 22, 0.0f)
            ]
        ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SBox).WidthOverride(360.0f)
            [
                PWUI::MakeButton(ConfirmLabel, PWUI::EButtonKind::Secondary,
                    FOnClicked::CreateSPLambda(this, [this]()
                    {
                        ConfirmExit();
                        return FReply::Handled();
                    }), true, 22, 0.0f)
            ]
        ],
        FMargin(48.0f, 36.0f));
}

void SPWPauseMenu::BeginExitConfirmation(EPendingExit Exit, const TSharedRef<SWidget>& InvokingWidget)
{
    PWUI::CancelPendingFocus(AsShared());
    PendingExit = Exit;
    InvokingControl = InvokingWidget;
    ContentHolder->SetContent(BuildExitConfirmation());
    PWUI::FocusWhenReady(AsShared(), ContentHolder);
}

void SPWPauseMenu::CancelExitConfirmation()
{
    PWUI::CancelPendingFocus(AsShared());
    const TWeakPtr<SWidget> RestoreFocus = InvokingControl;
    PendingExit = EPendingExit::None;
    ContentHolder->SetContent(MenuContent.ToSharedRef());
    InvokingControl.Reset();
    PWUI::FocusWhenReady(AsShared(), RestoreFocus);
}

void SPWPauseMenu::ConfirmExit()
{
    if (APerfectWorldFrontEndHUD* Owner = Hud.Get())
    {
        if (PendingExit == EPendingExit::ReturnToTitle)
        {
            Owner->ReturnToTitle();
        }
        else if (PendingExit == EPendingExit::QuitGame)
        {
            Owner->QuitGame();
        }
    }
}

FReply SPWPauseMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    // A held key auto-repeats; menus react to the first press only.
    if (InKeyEvent.IsRepeat())
    {
        return FReply::Handled();
    }
    const FKey Key = InKeyEvent.GetKey();
    if (PendingExit != EPendingExit::None
        && (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right))
    {
        CancelExitConfirmation();
        return FReply::Handled();
    }
    if (Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right)
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
