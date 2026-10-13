#include "UI/SPWFrontEndRoot.h"

#include "Framework/Application/SlateApplication.h"
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
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldFrontEndRoot"

void SPWFrontEndRoot::Construct(const FArguments& InArgs)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    Hud = InArgs._Hud;
    PageReveal = PageTransition.AddCurve(0.0f, 0.16f, ECurveEaseFunction::CubicOut);

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
            .Visibility_Lambda([this]() { return (Screen == EScreen::None || Screen == EScreen::Title) ? EVisibility::Collapsed : EVisibility::Visible; })
        ]
        + SOverlay::Slot()
        [
            SAssignNew(ScreenHolder, SBox)
            .IsEnabled_Lambda([this]() { return !bChooserOpen; })
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
                SNew(SBox)
                .MaxDesiredWidth(720.0f)
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]() { return Toast; })
                    .Font(PWUI::Font(18, true))
                    .ColorAndOpacity(FSlateColor(Style.TextPrimary))
                    .AutoWrapText(true)
                ]
            ]
        ]
    ];
}

void SPWFrontEndRoot::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    if (PWUI::IsReducedMotion()) PageTransition.JumpToEnd();
    ScreenHolder->SetRenderOpacity(PWUI::MotionLerp(PageReveal.GetLerp()));
}

void SPWFrontEndRoot::SetScreen(EScreen NewScreen, const TSharedRef<SWidget>& Content)
{
    PWUI::CancelPendingFocus();
    PageTransition.JumpToEnd();
    Screen = NewScreen;
    ScreenHolder->SetContent(Content);
    if (NewScreen != EScreen::None && NewScreen != EScreen::Creation) PWUI::PlayMotion(PageTransition, AsShared());
    if (NewScreen != EScreen::None) PWUI::FocusWhenReady(AsShared(), Content);
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
              SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SPWLoadPanel, Owner->GetCreator())
              .OnLoadSlot(FPWSlotDelegate::CreateSPLambda(this, [this](int32 Slot)
              {
                  APerfectWorldFrontEndHUD* Target = Hud.Get();
                  return Target != nullptr ? Target->LoadSlot(Slot) : FText::GetEmpty();
              }))
              .OnBack(FSimpleDelegate::CreateSP(this, &SPWFrontEndRoot::ShowTitle))]);
}

void SPWFrontEndRoot::ShowSettings()
{
    SetScreen(EScreen::Settings,
              SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SPWSettingsPanel).OnBack(FSimpleDelegate::CreateSP(this, &SPWFrontEndRoot::ShowTitle))]);
}

void SPWFrontEndRoot::ShowPause()
{
    bChooserOpen = false;
    SetScreen(EScreen::Pause, SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SPWPauseMenu).Hud(Hud)]);
}

void SPWFrontEndRoot::ShowCreation(int32 Step)
{
    APerfectWorldFrontEndHUD* Owner = Hud.Get();
    if (Owner == nullptr)
    {
        return;
    }
    bChooserOpen = false;
    TSharedPtr<SPWCreationFlow> Flow;
    SetScreen(EScreen::Creation,
              SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
              [
                  SNew(SBox)
                  .WidthOverride_Lambda([this]()
                  {
                      const float Width = GetCachedGeometry().GetLocalSize().X;
                      return Width < 1440.0f ? FMath::Max(1.0f, Width - 48.0f) : Width * .9f;
                  })
                  .HeightOverride_Lambda([this]()
                  {
                      const FVector2D Size = GetCachedGeometry().GetLocalSize();
                      return Size.X < 1440.0f ? FMath::Max(1.0f, Size.Y - 32.0f) : Size.Y * .9f;
                  })
                  [
                      SAssignNew(Flow, SPWCreationFlow, Owner->GetCreator())
                        .NarrowLayout_Lambda([this]() { return GetCachedGeometry().GetLocalSize().X < 1440.f; })
                      .InitialStep(Step)
                      .OnStartLife(FPWTryDelegate::CreateSP(this, &SPWFrontEndRoot::HandleStartLife))
                      .OnBackToTitle(FSimpleDelegate::CreateSP(this, &SPWFrontEndRoot::ShowTitle))
                  ]]);
    Flow->FocusActivePage();
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

void SPWFrontEndRoot::OpenSaveChooser()
{
    bSavingExistingLife = true;
    ChooserInvokingControl = FSlateApplication::Get().GetKeyboardFocusedWidget();
    ChooserStatus = FText::GetEmpty();
    BuildOverwriteChooser();
}

void SPWFrontEndRoot::OpenOverwriteChooser()
{
    bSavingExistingLife = false;
    ChooserInvokingControl = FSlateApplication::Get().GetKeyboardFocusedWidget();
    ChooserStatus = FText::GetEmpty();
    BuildOverwriteChooser();
}

void SPWFrontEndRoot::BuildOverwriteChooser(int32 SelectedSlot)
{
    APerfectWorldFrontEndHUD* Owner = Hud.Get();
    if (Owner == nullptr) return;
    PWUI::CancelPendingFocus();
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    UPerfectWorldCharacterCreatorSubsystem* Creator = Owner->GetCreator();
    const TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    Body->AddSlot().AutoHeight()
    [
        PWUI::MakeText(SelectedSlot < 0 ? LOCTEXT("ChooseTitle", "Choose a save slot")
                                       : LOCTEXT("ConfirmTitle", "Replace this save?"),
                       30, Style.TextPrimary, true)
    ];
    Body->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 16.0f)
    [
        PWUI::MakeText(LOCTEXT("Warning", "The save in the chosen slot will be overwritten. Your current life stays open until you confirm."),
                       18, Style.TextSecondary, false, true)
    ];
    if (SelectedSlot < 0)
    {
        for (int32 Slot = 0; Slot < UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotCount(); ++Slot)
        {
            Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
            [
                PWUI::MakeContentButton(
                    PWUI::MakeText(FText::Format(LOCTEXT("Slot", "Slot {0}: {1}"), FText::AsNumber(Slot + 1),
                        Creator != nullptr ? Creator->DescribeSaveSlot(Slot) : FText::GetEmpty()),
                        18, Style.TextPrimary, false, true),
                    PWUI::EButtonKind::Card,
                    FOnClicked::CreateSPLambda(this, [this, Slot]()
                    {
                        BuildOverwriteChooser(Slot);
                        return FReply::Handled();
                    }))
            ];
        }
    }
    else
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 24.0f)
        [
            PWUI::MakeText(FText::Format(LOCTEXT("ChosenSlot", "Slot {0}: {1}"), FText::AsNumber(SelectedSlot + 1),
                Creator != nullptr ? Creator->DescribeSaveSlot(SelectedSlot) : FText::GetEmpty()),
                18, Style.TextPrimary, false, true)
        ];
    }
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
    [
        PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return ChooserStatus; }),
                       18, Style.Danger, false, true)
    ];
    const TSharedRef<SWidget> Cancel = PWUI::MakeButton(
        LOCTEXT("Cancel", "Cancel"), PWUI::EButtonKind::Secondary,
        FOnClicked::CreateSPLambda(this, [this]()
        {
            CloseOverwriteChooser();
            return FReply::Handled();
        }));
    const TSharedRef<SHorizontalBox> Actions = SNew(SHorizontalBox);
    Actions->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)[Cancel];
    if (SelectedSlot >= 0)
    {
        Actions->AddSlot().AutoWidth()
        [
            PWUI::MakeButton(LOCTEXT("Replace", "Replace this save"), PWUI::EButtonKind::Primary,
                FOnClicked::CreateSPLambda(this, [this, SelectedSlot]()
                {
                    if (APerfectWorldFrontEndHUD* Target = Hud.Get())
                    {
                        if (bSavingExistingLife)
                        {
                            bool bSucceeded = false;
                            ChooserStatus = Target->SaveGameToSlot(SelectedSlot, bSucceeded);
                            if (bSucceeded)
                            {
                                const FText Notice = ChooserStatus;
                                CloseOverwriteChooser();
                                ShowToast(Notice);
                            }
                        }
                        else
                        {
                            ChooserStatus = Target->StartNewLife(SelectedSlot);
                        }
                    }
                    return FReply::Handled();
                }))
        ];
    }
    Body->AddSlot().AutoHeight()[Actions];
    ChooserHolder->SetContent(
        SNew(SBox)
        .WidthOverride_Lambda([this]()
        {
            return FMath::Min(720.0f, static_cast<float>(GetCachedGeometry().GetLocalSize().X) * 0.9f);
        })
        .MaxDesiredHeight_Lambda([this]()
        {
            return static_cast<float>(GetCachedGeometry().GetLocalSize().Y) * 0.9f;
        })
        [
            SNew(SScrollBox)
            .ScrollBarStyle(&Style.ScrollBar)
            .NavigationDestination(EDescendantScrollDestination::IntoView)
            .ScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll)
            + SScrollBox::Slot()
            [
                PWUI::MakeDialog(Body, FMargin(32.0f, 24.0f))
            ]
        ]);
    bChooserOpen = true;
    PWUI::FocusWhenReady(AsShared(), Cancel);
}

void SPWFrontEndRoot::CloseOverwriteChooser()
{
    PWUI::CancelPendingFocus();
    bChooserOpen = false;
    ChooserHolder->SetContent(SNullWidget::NullWidget);
    // Return focus to the control that opened the chooser, unless it has since been hidden or disabled; then the
    // screen's first focusable control takes it, so focus never sits on nothing.
    const TSharedPtr<SWidget> Invoker = ChooserInvokingControl.Pin();
    const bool bInvokerUsable = Invoker.IsValid() && PWUI::FindFirstFocusable(Invoker.ToSharedRef()).IsValid();
    PWUI::FocusWhenReady(AsShared(), bInvokerUsable ? TWeakPtr<SWidget>(Invoker) : TWeakPtr<SWidget>(ScreenHolder));
    ChooserInvokingControl.Reset();
}

FReply SPWFrontEndRoot::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    // A held key auto-repeats; menus react to the first press only.
    if (InKeyEvent.IsRepeat())
    {
        return FReply::Handled();
    }
    if (bChooserOpen && (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right
        || InKeyEvent.GetKey() == EKeys::Gamepad_Special_Right))
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
