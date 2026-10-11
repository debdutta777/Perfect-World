#include "UI/SPWCreationFlow.h"

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
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldCreationFlowUI"

void SPWCreationFlow::Construct(const FArguments& InArgs, UPerfectWorldCharacterCreatorSubsystem* InSubsystem)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    Model = MakeShared<FPWCreationModel>(InSubsystem);
    OnStartLife = InArgs._OnStartLife;
    OnBackToTitle = InArgs._OnBackToTitle;
    CurrentStep = FMath::Clamp(InArgs._InitialStep, 0, PWUI::CreationStepCount - 1);

    const FText StepNames[PWUI::CreationStepCount] = {
        LOCTEXT("StepHousehold", "Household"), LOCTEXT("StepTraits", "Traits"), LOCTEXT("StepIdentity", "Identity"),
        LOCTEXT("StepStream", "Stream"), LOCTEXT("StepReview", "Review")};

    const TSharedRef<SHorizontalBox> StepRow = SNew(SHorizontalBox);
    for (int32 Step = 0; Step < PWUI::CreationStepCount; ++Step)
    {
        StepRow->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
        [
            PWUI::MakeButton(
                FText::Format(LOCTEXT("StepChipFmt", "{0}  {1}"), FText::AsNumber(Step + 1), StepNames[Step]),
                PWUI::EButtonKind::Chip,
                FOnClicked::CreateLambda([this, Step]()
                {
                    GoToStep(Step);
                    return FReply::Handled();
                }),
                true, 16, 0.0f,
                TAttribute<bool>::CreateLambda([this, Step]() { return CurrentStep == Step; }))
        ];
    }

    Pages = SNew(SWidgetSwitcher)
        .WidgetIndex_Lambda([this]() { return CurrentStep; })
        + SWidgetSwitcher::Slot()[SNew(SPWHouseholdPage, Model.ToSharedRef())]
        + SWidgetSwitcher::Slot()[SNew(SPWTraitsPage, Model.ToSharedRef())]
        + SWidgetSwitcher::Slot()[SNew(SPWIdentityPage, Model.ToSharedRef())]
        + SWidgetSwitcher::Slot()[SNew(SPWStreamPage, Model.ToSharedRef())]
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

    const TSharedRef<SWidget> StartButton = PWUI::MakeButton(
        LOCTEXT("StartLife", "Start Life"), PWUI::EButtonKind::Primary,
        FOnClicked::CreateLambda([this]()
        {
            RequestStartLife();
            return FReply::Handled();
        }),
        TAttribute<bool>::CreateLambda([this]() { return Model->Summary.bReadyToStart; }), 22, 200.0f);
    StartButton->SetVisibility(TAttribute<EVisibility>::CreateLambda(
        [this]() { return CurrentStep == PWUI::CreationStepCount - 1 ? EVisibility::Visible : EVisibility::Collapsed; }));

    ChildSlot
    [
        PWUI::MakePanel(
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 28.0f, 0.0f)
                [
                    PWUI::MakeText(LOCTEXT("NewLife", "NEW LIFE"), 15, Style.Accent, true)
                ]
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                [
                    StepRow
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 20.0f)
            [
                PWUI::MakeRule()
            ]
            + SVerticalBox::Slot().FillHeight(1.0f)
            [
                Pages.ToSharedRef()
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 16.0f)
            [
                PWUI::MakeRule()
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 24.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]() { return Model->Message; })
                    .Font(PWUI::Font(17, true))
                    .ColorAndOpacity(FSlateColor(Style.Danger))
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
            ],
            FMargin(36.0f, 28.0f))
    ];

    GoToStep(CurrentStep);
}

void SPWCreationFlow::GoToStep(int32 Step)
{
    CurrentStep = FMath::Clamp(Step, 0, PWUI::CreationStepCount - 1);
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
        Model->Message = Problem;
    }
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
    if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
    {
        GoBack();
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
