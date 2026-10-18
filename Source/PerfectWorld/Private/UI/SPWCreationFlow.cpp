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
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldCreationFlowUI"

// Header and footer rules of the creation flow, linked by name so the PerfectWorld.CreationPause automation can exercise
// them without a header (UP-06). The widget calls these same functions; sizes are the flow's own logical size.
namespace
{
FVector2D CpFlowSpaceOverride = FVector2D::ZeroVector;

/** The flow's own logical size; before the first layout it reports a roomy window so nothing starts squeezed. */
FVector2D CpFlowSpace(const SWidget& Flow)
{
    if (CpFlowSpaceOverride.X > 0.0 && CpFlowSpaceOverride.Y > 0.0) return CpFlowSpaceOverride;
    const FVector2D Size(Flow.GetCachedGeometry().GetLocalSize());
    return Size.X > 1.0 && Size.Y > 1.0 ? Size : FVector2D(1920.0, 1080.0);
}
} // namespace

namespace PWCreationPause
{
FText CreationStepName(const int32 Step)
{
    switch (Step)
    {
    case 0: return LOCTEXT("StepHousehold", "Household");
    case 1: return LOCTEXT("StepTraits", "Traits");
    case 2: return LOCTEXT("StepIdentity", "Identity");
    case 3: return LOCTEXT("StepStream", "Stream");
    default: return LOCTEXT("StepReview", "Review");
    }
}

/** Below 1000 units wide, steps other than the current one show their number alone, so the current name keeps its room. */
bool CreationRailNumbersOnly(const float Width) { return Width < 1000.0f; }

/** The small "New life" label and some spacing go first when the flow is short (1280x720 at 150% is 448 units tall). */
bool CreationCompactChrome(const float Height) { return Height < 560.0f; }

/** The label of the forward button: it names the page it leads to. */
FText CreationNextLabel(const int32 Step)
{
    return FText::Format(LOCTEXT("NextFmt", "Next: {0}"), CreationStepName(FMath::Min(Step + 1, PWUI::CreationStepCount - 1)));
}

void SetCreationSpaceForTest(const float Width, const float Height) { CpFlowSpaceOverride = FVector2D(Width, Height); }
} // namespace PWCreationPause

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

    const auto Compact = [this]() { return PWCreationPause::CreationCompactChrome(static_cast<float>(CpFlowSpace(*this).Y)); };
    const auto NumbersOnly = [this]() { return PWCreationPause::CreationRailNumbersOnly(static_cast<float>(CpFlowSpace(*this).X)); };
    const TSharedRef<SHorizontalBox> StepRow = SNew(SHorizontalBox);
    const EPWSigilPictogram Pictograms[PWUI::CreationStepCount] = {
        EPWSigilPictogram::Doorway, EPWSigilPictogram::Ledger, EPWSigilPictogram::Person,
        EPWSigilPictogram::Paths, EPWSigilPictogram::FirstMorning};
    for (int32 Step = 0; Step < PWUI::CreationStepCount; ++Step)
    {
        const FText StepName = PWCreationPause::CreationStepName(Step);
        // The current step reads in full and bold; steps already visited keep ivory; steps ahead step back to the secondary
        // tone (still well above 4.5:1 on the chip). Every chip stays a button, so navigation is unchanged.
        const TSharedRef<STextBlock> ChipText = SNew(STextBlock)
            .Text_Lambda([this, Step, StepName, NumbersOnly]()
            {
                return Step != CurrentStep && NumbersOnly() ? FText::AsNumber(Step + 1)
                    : FText::Format(LOCTEXT("StepChipFmt", "{0}  {1}"), FText::AsNumber(Step + 1), StepName);
            })
            .Font_Lambda([this, Step]() { return PWUI::Font(15, Step == CurrentStep); })
            .ColorAndOpacity_Lambda([this, Step]() { return FSlateColor(Step > CurrentStep ? PWUI::FStyle::Get().TextSecondary : PWUI::FStyle::Get().TextPrimary); })
            .TextShapingMethod(ETextShapingMethod::FullShaping)
            .AutoWrapText(true);
        const auto StepButton = PWUI::MakeContentButton(
                ChipText,
                PWUI::EButtonKind::Chip,
                FOnClicked::CreateLambda([this, Step]()
                {
                    GoToStep(Step);
                    return FReply::Handled();
                }),
                TAttribute<bool>::CreateLambda([this, Step]() { return CurrentStep == Step; }), true, FMargin(8));
#if WITH_ACCESSIBILITY
        StepButton->SetAccessibleBehavior(EAccessibleBehavior::Custom, TAttribute<FText>::CreateLambda([this, Step, StepName]()
        {
            const FText Status = CurrentStep == Step ? LOCTEXT("StepCurrent", "Current")
                : Step < CurrentStep ? LOCTEXT("StepVisited", "Visited") : LOCTEXT("StepAhead", "Not yet visited");
            return FText::Format(LOCTEXT("AccessibleStep", "Step {0}: {1}, {2}"), FText::AsNumber(Step + 1), StepName, Status);
        }));
#endif
        // A short rule under each chip fills up to the current step: progress without a second control to focus.
        const TSharedRef<SWidget> Track = SNew(SBox).HeightOverride(3.0f)
            [SNew(SImage).Image_Lambda([this, Step]() { return Step <= CurrentStep ? &PWUI::FStyle::Get().BrushAccentRule : &PWUI::FStyle::Get().BrushRule; })];
        StepRow->AddSlot()
        .FillWidth(TAttribute<float>::CreateLambda([this, Step, NumbersOnly]() { return Step != CurrentStep ? 1.0f : (NumbersOnly() ? 3.0f : 1.7f); }))
        .Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
                [SNew(SBox)
                    .Visibility_Lambda([this]() { return CpFlowSpace(*this).X >= 640.0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
                    [SNew(SPWAwakeningSigil).SizeClass(EPWSigilSizeClass::Seal).Diameter(32).Pictogram(Pictograms[Step])]]
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[StepButton]]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)[Track]];
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
        TAttribute<FText>::CreateLambda([this]() { return PWCreationPause::CreationNextLabel(CurrentStep); }),
        PWUI::EButtonKind::Primary,
        FOnClicked::CreateLambda([this]()
        {
            GoToStep(CurrentStep + 1);
            return FReply::Handled();
        }),
        true, 20, 210.0f);
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
            // The confirmation stays up while Start Life runs (a slot chooser remembers a visible button to return to).
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
            + SVerticalBox::Slot().AutoHeight()
            .Padding(TAttribute<FMargin>::CreateLambda([Compact]() { return Compact() ? FMargin(0, 0, 0, 4) : FMargin(0, 0, 0, 10); }))
            [
                // Where the player is, said first: the step's number and name, large beside the small "New life" label.
                SNew(SHorizontalBox)
                .Tag(TEXT("CpStepHeader"))
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(0.0f, 0.0f, 12.0f, 3.0f)
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]() { return FText::Format(LOCTEXT("StepOfFmt", "Step {0} of {1}"), FText::AsNumber(CurrentStep + 1), FText::AsNumber(PWUI::CreationStepCount)); })
                    .Font(PWUI::Font(16, true))
                    .ColorAndOpacity(FSlateColor(Style.Accent))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]() { return PWCreationPause::CreationStepName(CurrentStep); })
                    .Font_Lambda([Compact]() { return PWUI::Font(Compact() ? 20 : 22, true); })
                    .ColorAndOpacity(FSlateColor(Style.TextPrimary))
                ]
                + SHorizontalBox::Slot().FillWidth(1.0f)[SNullWidget::NullWidget]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(0.0f, 0.0f, 0.0f, 3.0f)
                [
                    SNew(SBox).Visibility_Lambda([Compact]() { return Compact() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
                    [PWUI::MakeText(LOCTEXT("NewLife", "New life"), PWUI::ETextRole::Meta, Style.TextSecondary)]
                ]
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                [
                    SNew(SBox).MinDesiredHeight_Lambda([this]() { return NarrowLayout.Get(false) ? 48.f : 64.f; })[StepRow]
                ]
            ]
            + SVerticalBox::Slot().AutoHeight()
            .Padding(TAttribute<FMargin>::CreateLambda([Compact]() { return Compact() ? FMargin(0, 8, 0, 10) : FMargin(0, 16, 0, 20); }))
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
            + SVerticalBox::Slot().AutoHeight()
            .Padding(TAttribute<FMargin>::CreateLambda([Compact]() { return Compact() ? FMargin(0, 8, 0, 8) : FMargin(0, 16, 0, 16); }))
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
