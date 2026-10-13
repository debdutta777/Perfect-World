#include "UI/SPWCreationFlow.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "UI/PWCreationModel.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "UI/SPWHouseholdPage.h"
#include "UI/SPWIdentityPage.h"
#include "UI/SPWReviewPage.h"
#include "UI/SPWStreamPage.h"
#include "UI/SPWTraitsPage.h"
#include "UI/SPWAwakeningSigil.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldCreationFlowUI"

void SPWCreationFlow::Construct(const FArguments& InArgs, UPerfectWorldCharacterCreatorSubsystem* InSubsystem)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    Model = MakeShared<FPWCreationModel>(InSubsystem);
    NarrowLayout = InArgs._NarrowLayout;
    OnStartLife = InArgs._OnStartLife;
    OnBackToTitle = InArgs._OnBackToTitle;
    CurrentStep = FMath::Clamp(InArgs._InitialStep, 0, PWUI::CreationStepCount - 1);
    PageReveal = PageTransition.AddCurve(0.0f, 0.16f, ECurveEaseFunction::CubicOut);
    ModalReveal = ModalTransition.AddCurve(0.0f, 0.18f, ECurveEaseFunction::CubicOut);

    const FText StepNames[PWUI::CreationStepCount] = {
        LOCTEXT("StepHousehold", "Household"), LOCTEXT("StepTraits", "Traits"), LOCTEXT("StepIdentity", "Identity"),
        LOCTEXT("StepStream", "Stream"), LOCTEXT("StepReview", "Review")};

    const TSharedRef<SHorizontalBox> StepRow = SNew(SHorizontalBox);
    const EPWSigilPictogram Pictograms[PWUI::CreationStepCount] = {
        EPWSigilPictogram::Doorway, EPWSigilPictogram::Ledger, EPWSigilPictogram::Person,
        EPWSigilPictogram::Paths, EPWSigilPictogram::FirstMorning};
    for (int32 Step = 0; Step < PWUI::CreationStepCount; ++Step)
    {
        const auto StepButton = PWUI::MakeContentButton(
                PWUI::MakeText(FText::Format(LOCTEXT("StepChipFmt", "{0}  {1}"), FText::AsNumber(Step + 1), StepNames[Step]),
                               15, Style.TextPrimary, false, true),
                PWUI::EButtonKind::Chip,
                FOnClicked::CreateLambda([this, Step]()
                {
                    GoToStep(Step);
                    return FReply::Handled();
                }),
                TAttribute<bool>::CreateLambda([this, Step]() { return CurrentStep == Step; }), true, FMargin(8));
#if WITH_ACCESSIBILITY
        StepButton->SetAccessibleBehavior(EAccessibleBehavior::Custom, TAttribute<FText>::CreateLambda([this, Step, Name = StepNames[Step]]()
        {
            const FText Status = CurrentStep == Step ? LOCTEXT("StepCurrent", "Current")
                : Model->Summary.bReadyToStart ? LOCTEXT("StepComplete", "Complete") : LOCTEXT("StepAttention", "Needs attention");
            return FText::Format(LOCTEXT("AccessibleStep", "Step {0}: {1}, {2}"), FText::AsNumber(Step + 1), Name, Status);
        }));
#endif
        StepRow->AddSlot().FillWidth(1.0f).Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
            [SNew(SPWAwakeningSigil).SizeClass(EPWSigilSizeClass::Seal).Diameter(32).Pictogram(Pictograms[Step])]
            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[StepButton]];
    }

    Pages = SNew(SWidgetSwitcher)
        .WidgetIndex_Lambda([this]() { return CurrentStep; })
        + SWidgetSwitcher::Slot()[SAssignNew(HouseholdPage, SPWHouseholdPage, Model.ToSharedRef())
            .NarrowLayout(NarrowLayout)
            .OnEditResources(FSimpleDelegate::CreateSP(this, &SPWCreationFlow::EditResources))
            .OnFocusDetails(FPWWidgetDelegate::CreateSP(this, &SPWCreationFlow::FocusDetails))
            .OnFocusFooter(FSimpleDelegate::CreateLambda([this]() { PWUI::FocusWhenReady(AsShared(), FooterControls); }))]
        + SWidgetSwitcher::Slot()[SAssignNew(TraitsPage, SPWTraitsPage, Model.ToSharedRef())
            .NarrowLayout(NarrowLayout)
            .OnOpenModal(FPWCreationModalDelegate::CreateSP(this, &SPWCreationFlow::OpenAllocationModal))
            .OnCloseModal(FSimpleDelegate::CreateSP(this, &SPWCreationFlow::CloseAllocationModal))
            .OnFocusFooter(FSimpleDelegate::CreateLambda([this]() { PWUI::FocusWhenReady(AsShared(), FooterControls); }))
            .OnRevealInspector(FPWCreationRevealDelegate::CreateLambda([this](TSharedPtr<SWidget> Target)
            {
                if (PageBody.IsValid()) PageBody->ScrollDescendantIntoView(Target, false, EDescendantScrollDestination::TopOrLeft);
            }))]
        + SWidgetSwitcher::Slot()[SNew(SPWIdentityPage, Model.ToSharedRef())]
        + SWidgetSwitcher::Slot()[SAssignNew(StreamPage, SPWStreamPage, Model.ToSharedRef())
            .NarrowLayout(NarrowLayout)
            .OnFocusDetails(FPWWidgetDelegate::CreateSP(this, &SPWCreationFlow::FocusDetails))
            .OnFocusFooter(FSimpleDelegate::CreateLambda([this]() { PWUI::FocusWhenReady(AsShared(), FooterControls); }))]
        + SWidgetSwitcher::Slot()
        [
            SNew(SPWReviewPage, Model.ToSharedRef())
            .OnJumpToStep(FPWStepDelegate::CreateSP(this, &SPWCreationFlow::GoToStep))
        ];

    const TSharedRef<SWidget> BackButton = PWUI::MakeButton(
        TAttribute<FText>::CreateLambda([this]()
        {
            return CurrentStep == 0 ? LOCTEXT("BackToTitle", "Back to title") : LOCTEXT("Back", "Back");
        }),
        PWUI::EButtonKind::Secondary,
        FOnClicked::CreateLambda([this]()
        {
            GoBack();
            return FReply::Handled();
        }),
        true, 20, 150.0f);

    const TSharedRef<SWidget> NextButton = PWUI::MakeButton(
        LOCTEXT("Next", "Next"), PWUI::EButtonKind::Primary,
        FOnClicked::CreateLambda([this]()
        {
            GoToStep(CurrentStep + 1);
            return FReply::Handled();
        }),
        true, 20, 150.0f);
    NextButton->SetVisibility(TAttribute<EVisibility>::CreateLambda(
        [this]() { return CurrentStep < PWUI::CreationStepCount - 1 ? EVisibility::Visible : EVisibility::Collapsed; }));
    BackButton->SetVisibility(TAttribute<EVisibility>::CreateLambda(
        [this]() { return bConfirmingStart ? EVisibility::Collapsed : EVisibility::Visible; }));

    const TSharedRef<SWidget> StartButton = PWUI::MakeButton(
        LOCTEXT("StartLife", "Start Life"), PWUI::EButtonKind::Primary,
        FOnClicked::CreateLambda([this]()
        {
            BeginStartConfirmation();
            return FReply::Handled();
        }),
        TAttribute<bool>::CreateLambda([this]() { return Model->Summary.bReadyToStart; }), 22, 200.0f);
    StartButton->SetVisibility(TAttribute<EVisibility>::CreateLambda([this]()
    {
        return CurrentStep == PWUI::CreationStepCount - 1 && !bConfirmingStart ? EVisibility::Visible : EVisibility::Collapsed;
    }));

    const TSharedRef<SWidget> CancelButton = PWUI::MakeButton(
        LOCTEXT("CancelStart", "Not yet"), PWUI::EButtonKind::Secondary,
        FOnClicked::CreateLambda([this]()
        {
            CancelStartConfirmation();
            return FReply::Handled();
        }),
        true, 20, 150.0f);
    CancelButton->SetVisibility(TAttribute<EVisibility>::CreateLambda(
        [this]() { return bConfirmingStart ? EVisibility::Visible : EVisibility::Collapsed; }));
    CancelStartButton = CancelButton;

    const TSharedRef<SWidget> ConfirmButton = PWUI::MakeButton(
        LOCTEXT("ConfirmStart", "Begin this life"), PWUI::EButtonKind::Primary,
        FOnClicked::CreateLambda([this]()
        {
            // The confirmation stays up while Start Life runs, so a slot chooser opened from here remembers a
            // visible button to return focus to; RequestStartLife closes it once the outcome is known.
            RequestStartLife();
            return FReply::Handled();
        }),
        true, 22, 220.0f);
    ConfirmButton->SetVisibility(TAttribute<EVisibility>::CreateLambda(
        [this]() { return bConfirmingStart ? EVisibility::Visible : EVisibility::Collapsed; }));

    ChildSlot
    [
        SNew(SOverlay)
        + SOverlay::Slot()
        [
        SNew(SBox).IsEnabled_Lambda([this]() { return !bAllocationModal; })
        [
        SNew(SBorder).BorderImage(&Style.BrushPanel)
        .Padding_Lambda([this]() { return NarrowLayout.Get(false) ? FMargin(16) : FMargin(32); })
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
            [PWUI::MakeText(LOCTEXT("NewLife", "New life"), PWUI::ETextRole::Meta, Style.TextSecondary)]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                [
                    SNew(SBox).MinDesiredHeight_Lambda([this]() { return NarrowLayout.Get(false) ? 48.f : 64.f; })[StepRow]
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 20.0f)
            [
                PWUI::MakeRule()
            ]
            + SVerticalBox::Slot().FillHeight(1.0f)
            [
                SAssignNew(PageBody, SScrollBox)
                .ScrollBarStyle(&Style.ScrollBar)
                .AllowOverscroll(EAllowOverscroll::No)
                .NavigationDestination(EDescendantScrollDestination::IntoView)
                .ScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll)
                + SScrollBox::Slot().Padding(0.0f, 0.0f, 12.0f, 0.0f)
                [
                    Pages.ToSharedRef()
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
            [
                SNew(SBox).Visibility_Lambda([this]() { return CurrentStep == 1 ? EVisibility::Visible : EVisibility::Collapsed; })
                [TraitsPage->BuildBudget()]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 16.0f)
            [
                PWUI::MakeRule()
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SAssignNew(FooterControls, SBox)
                .MinDesiredHeight_Lambda([this]() { return NarrowLayout.Get(false) ? 56.f : 72.f; })
                [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 24.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]()
                    {
                        return bConfirmingStart
                            ? LOCTEXT("ConfirmPrompt", "Begin this life? Your choices are then fixed and the story starts.")
                            : Model->Message;
                    })
                    .Font(PWUI::Font(17, true))
                    .ColorAndOpacity_Lambda([this]()
                    {
                        const PWUI::FStyle& S = PWUI::FStyle::Get();
                        return FSlateColor(bConfirmingStart ? S.TextPrimary : S.Danger);
                    })
                    .AutoWrapText(true)
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)
                [
                    BackButton
                ]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    NextButton
                ]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    StartButton
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)
                [
                    CancelButton
                ]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    ConfirmButton
                ]
                ]
            ]
        ]
        ]
        ]
        + SOverlay::Slot()
        [
            SNew(SBorder).BorderImage(&Style.BrushScrim).Padding(16)
            .Visibility_Lambda([this]() { return bAllocationModal ? EVisibility::Visible : EVisibility::Collapsed; })
            .HAlign(HAlign_Right).VAlign(VAlign_Fill)
            [
                SAssignNew(AllocationModal, SBox)
                .WidthOverride_Lambda([this]()
                {
                    const float Width = GetCachedGeometry().GetLocalSize().X;
                    return FMath::Max(1.0f, NarrowLayout.Get(Width < 1320.0f) ? Width - 32.0f : Width * .55f);
                })
            ]
        ]
    ];

    PageBody->SetIsFocusable(true);
    GoToStep(CurrentStep);
}

void SPWCreationFlow::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    if (PWUI::IsReducedMotion())
    {
        PageTransition.JumpToEnd();
        ModalTransition.JumpToEnd();
    }
    PageBody->SetRenderOpacity(PWUI::MotionLerp(PageReveal.GetLerp()));
    if (bAllocationModal) AllocationModal->SetRenderOpacity(PWUI::MotionLerp(ModalReveal.GetLerp()));
}

void SPWCreationFlow::GoToStep(int32 Step)
{
    if (bAllocationModal) return;
    PWUI::CancelPendingFocus(AsShared());
    PageTransition.JumpToEnd();
    CurrentStep = FMath::Clamp(Step, 0, PWUI::CreationStepCount - 1);
    bConfirmingStart = false;
    Model->ClearMessage();

    // Remember where the player got to, so a save written now resumes on this page.
    if (UPerfectWorldCharacterCreatorSubsystem* Creator = Model->GetSubsystem())
    {
        if (!Creator->HasLifeStarted())
        {
            EPWResumePoint Point = EPWResumePoint::Appearance;
            switch (static_cast<PWUI::ECreationStep>(CurrentStep))
            {
            case PWUI::ECreationStep::Household: Point = EPWResumePoint::Appearance; break;
            case PWUI::ECreationStep::Traits: Point = EPWResumePoint::Allocation; break;
            case PWUI::ECreationStep::Identity: Point = EPWResumePoint::Identity; break;
            case PWUI::ECreationStep::Stream: Point = EPWResumePoint::Stream; break;
            case PWUI::ECreationStep::Review: Point = EPWResumePoint::Review; break;
            }
            Creator->SetResumePoint(Point);
        }
    }
    Model->Refresh();
    if (TraitsPage.IsValid()) TraitsPage->DiscardProposal();
    if (PageBody.IsValid()) PageBody->ScrollToStart();
    PWUI::PlayMotion(PageTransition, AsShared());
    FocusActivePage();
}

void SPWCreationFlow::GoBack()
{
    if (CurrentStep <= 0)
    {
        OnBackToTitle.ExecuteIfBound();
        return;
    }
    GoToStep(CurrentStep - 1);
}

void SPWCreationFlow::RequestStartLife()
{
    if (!OnStartLife.IsBound())
    {
        return;
    }
    const FText Problem = OnStartLife.Execute();
    if (!Problem.IsEmpty())
    {
        // Refused: drop the confirmation and return focus to the page, so focus never rests on a collapsed button.
        Model->Message = Problem;
        bConfirmingStart = false;
        FocusActivePage();
    }
}

void SPWCreationFlow::BeginStartConfirmation()
{
    if (!Model->Summary.bReadyToStart)
    {
        return;
    }
    Model->ClearMessage();
    PWUI::CancelPendingFocus(AsShared());
    bConfirmingStart = true;
    // The Cancel button only becomes visible on the next layout pass, so focus it once it can take focus.
    if (CancelStartButton.IsValid())
    {
        PWUI::FocusWhenReady(AsShared(), CancelStartButton);
    }
}

void SPWCreationFlow::CancelStartConfirmation()
{
    bConfirmingStart = false;
    FocusActivePage();
}

void SPWCreationFlow::FocusActivePage()
{
    if (Pages.IsValid() && Pages->GetActiveWidget().IsValid())
    {
        PWUI::FocusWhenReady(AsShared(), Pages->GetActiveWidget());
    }
}

FReply SPWCreationFlow::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    // A held key auto-repeats; menus react to the first press only.
    if (InKeyEvent.IsRepeat())
    {
        return FReply::Handled();
    }
    if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
    {
        const auto Focus = FSlateApplication::Get().GetKeyboardFocusedWidget();
        if (Focus.IsValid() && Focus->GetTypeAsString().Contains(TEXT("EditableText")))
        {
            // The text editor handles IME/Escape first. If it bubbles, close editing before page Back.
            return FReply::Handled().SetUserFocus(PageBody.ToSharedRef(), EFocusCause::Navigation);
        }
        if (bConfirmingStart)
        {
            CancelStartConfirmation();
        }
        else
        {
            if (CurrentStep != 1 || !TraitsPage->CancelPendingProposal()) GoBack();
        }
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
