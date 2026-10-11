#include "UI/SPWFrontEndRoot.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "UI/PWDelegates.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "UI/SPWCreationFlow.h"
#include "UI/SPWLoadPanel.h"
#include "UI/SPWPauseMenu.h"
#include "UI/SPWSettingsPanel.h"
#include "UI/SPWTitleScreen.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldFrontEndRoot"

namespace
{
/** Centres a panel and shrinks it to fit a small window, but never enlarges it. */
TSharedRef<SWidget> RootCentred(const TSharedRef<SWidget>& Content)
{
    return SNew(SScaleBox)
        .Stretch(EStretch::ScaleToFit)
        .StretchDirection(EStretchDirection::DownOnly)
        [
            Content
        ];
}
} // namespace

void SPWFrontEndRoot::Construct(const FArguments& InArgs)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    Hud = InArgs._Hud;

    SetVisibility(TAttribute<EVisibility>::CreateLambda([this]()
    {
        return (Screen == EScreen::None && !bChooserOpen) ? EVisibility::SelfHitTestInvisible : EVisibility::Visible;
    }));

    ChildSlot
    [
        SNew(SOverlay)
        + SOverlay::Slot()
        [
            SNew(SImage)
            .Image(&Style.BrushScrim)
            .Visibility_Lambda([this]() { return Screen == EScreen::None ? EVisibility::Collapsed : EVisibility::Visible; })
        ]
        + SOverlay::Slot()
        [
            SNew(SImage)
            .Image(&Style.BrushScrim)
            .Visibility_Lambda([this]()
            {
                return (Screen == EScreen::None || Screen == EScreen::Title) ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
            })
        ]
        + SOverlay::Slot()
        [
            SAssignNew(ScreenHolder, SBox)
        ]
        + SOverlay::Slot()
        [
            SNew(SBorder)
            .BorderImage(&Style.BrushScrim)
            .Visibility_Lambda([this]() { return bChooserOpen ? EVisibility::Visible : EVisibility::Collapsed; })
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            [
                SAssignNew(ChooserHolder, SBox)
            ]
        ]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.0f, 0.0f, 0.0f, 70.0f)
        [
            SNew(SBorder)
            .BorderImage(&Style.BrushToast)
            .Padding(FMargin(26.0f, 14.0f))
            .Visibility_Lambda([this]() { return Toast.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
            [
                SNew(STextBlock)
                .Text_Lambda([this]() { return Toast; })
                .Font(PWUI::Font(18, true))
                .ColorAndOpacity(FSlateColor(Style.TextPrimary))
            ]
        ]
    ];
}

void SPWFrontEndRoot::SetScreen(EScreen NewScreen, const TSharedRef<SWidget>& Content)
{
    Screen = NewScreen;
    ScreenHolder->SetContent(Content);
}

void SPWFrontEndRoot::ShowTitle()
{
    APerfectWorldFrontEndHUD* Owner = Hud.Get();
    if (Owner == nullptr)
    {
        return;
    }
    bChooserOpen = false;
    SetScreen(EScreen::Title,
              SNew(SPWTitleScreen, Owner->GetCreator())
              .OnNewGame(FSimpleDelegate::CreateSP(this, &SPWFrontEndRoot::HandleNewGame))
              .OnContinue(FPWTryDelegate::CreateSPLambda(this, [this]()
              {
                  APerfectWorldFrontEndHUD* Target = Hud.Get();
                  return Target != nullptr ? Target->ContinueLatest() : FText::GetEmpty();
              }))
              .OnLoad(FSimpleDelegate::CreateSP(this, &SPWFrontEndRoot::ShowLoad))
              .OnSettings(FSimpleDelegate::CreateSP(this, &SPWFrontEndRoot::ShowSettings))
              .OnQuit(FSimpleDelegate::CreateSPLambda(this, [this]()
              {
                  if (APerfectWorldFrontEndHUD* Target = Hud.Get())
                  {
                      Target->QuitGame();
                  }
              })));
}

void SPWFrontEndRoot::ShowLoad()
{
    APerfectWorldFrontEndHUD* Owner = Hud.Get();
    if (Owner == nullptr)
    {
        return;
    }
    SetScreen(EScreen::Load,
              RootCentred(SNew(SPWLoadPanel, Owner->GetCreator())
              .OnLoadSlot(FPWSlotDelegate::CreateSPLambda(this, [this](int32 Slot)
              {
                  APerfectWorldFrontEndHUD* Target = Hud.Get();
                  return Target != nullptr ? Target->LoadSlot(Slot) : FText::GetEmpty();
              }))
              .OnBack(FSimpleDelegate::CreateSP(this, &SPWFrontEndRoot::ShowTitle))));
}

void SPWFrontEndRoot::ShowSettings()
{
    SetScreen(EScreen::Settings,
              RootCentred(SNew(SPWSettingsPanel).OnBack(FSimpleDelegate::CreateSP(this, &SPWFrontEndRoot::ShowTitle))));
}

void SPWFrontEndRoot::ShowPause()
{
    bChooserOpen = false;
    SetScreen(EScreen::Pause, RootCentred(SNew(SPWPauseMenu).Hud(Hud)));
}

void SPWFrontEndRoot::ShowCreation(int32 Step)
{
    APerfectWorldFrontEndHUD* Owner = Hud.Get();
    if (Owner == nullptr)
    {
        return;
    }
    bChooserOpen = false;
    SetScreen(EScreen::Creation,
              RootCentred(
                  SNew(SBox)
                  .WidthOverride(1500.0f)
                  .HeightOverride(940.0f)
                  [
                      SNew(SPWCreationFlow, Owner->GetCreator())
                      .InitialStep(Step)
                      .OnStartLife(FPWTryDelegate::CreateSP(this, &SPWFrontEndRoot::HandleStartLife))
                      .OnBackToTitle(FSimpleDelegate::CreateSP(this, &SPWFrontEndRoot::ShowTitle))
                  ]));
}

void SPWFrontEndRoot::Hide()
{
    bChooserOpen = false;
    SetScreen(EScreen::None, SNullWidget::NullWidget);
}

void SPWFrontEndRoot::ShowToast(const FText& Message)
{
    Toast = Message;
    const int32 Serial = ++ToastSerial;
    RegisterActiveTimer(7.0f, FWidgetActiveTimerDelegate::CreateSPLambda(this, [this, Serial](double, float)
    {
        // Only the newest notice may clear itself.
        if (Serial == ToastSerial)
        {
            Toast = FText::GetEmpty();
        }
        return EActiveTimerReturnType::Stop;
    }));
}

void SPWFrontEndRoot::HandleNewGame()
{
    if (APerfectWorldFrontEndHUD* Owner = Hud.Get())
    {
        Owner->BeginNewCreation();
    }
}

FText SPWFrontEndRoot::HandleStartLife()
{
    APerfectWorldFrontEndHUD* Owner = Hud.Get();
    if (Owner == nullptr)
    {
        return FText::GetEmpty();
    }
    // A new life is written to the first empty slot. When all are used, the
    // player chooses which one to replace; nothing is overwritten silently.
    const int32 Empty = UPerfectWorldCharacterCreatorSubsystem::FindFirstEmptySaveSlot();
    if (Empty >= 0)
    {
        return Owner->StartNewLife(Empty);
    }
    OpenOverwriteChooser();
    return FText::GetEmpty();
}

void SPWFrontEndRoot::OpenOverwriteChooser()
{
    APerfectWorldFrontEndHUD* Owner = Hud.Get();
    if (Owner == nullptr)
    {
        return;
    }
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    UPerfectWorldCharacterCreatorSubsystem* Creator = Owner->GetCreator();

    const TSharedRef<SVerticalBox> Slots = SNew(SVerticalBox);
    const int32 SlotCount = UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotCount();
    for (int32 Slot = 0; Slot < SlotCount; ++Slot)
    {
        const FText Description = Creator != nullptr ? Creator->DescribeSaveSlot(Slot) : FText::GetEmpty();
        const TSharedRef<SWidget> Content =
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                PWUI::MakeText(FText::Format(LOCTEXT("ReplaceSlotFmt", "REPLACE SLOT {0}"), FText::AsNumber(Slot + 1)), 13,
                               Style.Warn, true)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
            [
                PWUI::MakeText(Description, 20, Style.TextPrimary, false, true)
            ];
        Slots->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
        [
            PWUI::MakeContentButton(
                Content, PWUI::EButtonKind::Card,
                FOnClicked::CreateSPLambda(this, [this, Slot]()
                {
                    if (APerfectWorldFrontEndHUD* Target = Hud.Get())
                    {
                        ChooserStatus = Target->StartNewLife(Slot);
                    }
                    return FReply::Handled();
                }),
                false, true, FMargin(24.0f, 16.0f))
        ];
    }

    ChooserStatus = FText::GetEmpty();
    ChooserHolder->SetContent(
        RootCentred(
            SNew(SBox)
            .WidthOverride(720.0f)
            [
                PWUI::MakePanel(
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        PWUI::MakeText(LOCTEXT("ChooserTitle", "All three save slots are in use"), 30, Style.TextPrimary, true)
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 18.0f)
                    [
                        PWUI::MakeText(LOCTEXT("ChooserBody",
                            "Choose the slot this new life will replace. The save currently in that slot will be overwritten."),
                            16, Style.TextSecondary, false, true)
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        Slots
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                    [
                        PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return ChooserStatus; }), 16, Style.Danger, true, true)
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SBox)
                        .HAlign(HAlign_Left)
                        [
                            PWUI::MakeButton(LOCTEXT("Cancel", "Cancel"), PWUI::EButtonKind::Secondary,
                                             FOnClicked::CreateSPLambda(this, [this]()
                                             {
                                                 CloseOverwriteChooser();
                                                 return FReply::Handled();
                                             }),
                                             true, 20, 140.0f)
                        ]
                    ],
                    FMargin(34.0f, 28.0f))
            ]));
    bChooserOpen = true;
    PWUI::FocusWhenReady(AsShared(), ChooserHolder);
}

void SPWFrontEndRoot::CloseOverwriteChooser()
{
    bChooserOpen = false;
    ChooserHolder->SetContent(SNullWidget::NullWidget);
    if (ScreenHolder.IsValid() && ScreenHolder->GetChildren()->Num() > 0)
    {
        PWUI::FocusWhenReady(AsShared(), ScreenHolder);
    }
}

FReply SPWFrontEndRoot::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    if (bChooserOpen && (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right))
    {
        CloseOverwriteChooser();
        return FReply::Handled();
    }
    // While the game is paused the menu owns the keyboard, so Escape (or the controller's menu button) resumes here.
    const bool bMenuKey = InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_Special_Right;
    if (bMenuKey && Hud.IsValid() && Hud->IsPaused())
    {
        Hud->ToggleInWorldMenu();
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
