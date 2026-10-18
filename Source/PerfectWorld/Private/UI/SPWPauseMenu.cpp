#include "UI/SPWPauseMenu.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "UI/SPWAwakeningSigil.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldPauseMenu"

// Layout and feedback rules the pause menu follows, linked by name so the PerfectWorld.CreationPause automation can
// exercise them without a header (UP-06). The widget calls these same functions.
namespace PWCreationPause
{
int32 PauseDensity(const float Width, const float Height)
{
    // Compact below 760 logical units either way: 1280x720 at 100%, and both common windows at 150% UI scale.
    return (Height < 760.0f || Width < 760.0f) ? 1 : 0;
}

float PauseControlWidth(const float Width, const float Height)
{
    const float Pad = PauseDensity(Width, Height) > 0 ? 24.0f : 48.0f;
    return FMath::Clamp(Width - 32.0f - Pad * 2.0f, 200.0f, 360.0f);
}

float PauseMaxHeight(const float, const float Height)
{
    return FMath::Max(120.0f, Height - 32.0f);
}

FText PauseSaveFeedbackText(const bool bSucceeded, const FText& CoreMessage, const int32 SuccessCount)
{
    if (bSucceeded)
    {
        const FText Base = CoreMessage.IsEmpty() ? LOCTEXT("SavedPlain", "Saved.") : CoreMessage;
        return SuccessCount >= 2 ? FText::Format(LOCTEXT("SavedAgainFmt", "{0} Saved {1} times."), Base, FText::AsNumber(SuccessCount))
                                 : Base;
    }
    return FText::Format(LOCTEXT("NotSavedFmt", "Not saved. {0}"),
                         CoreMessage.IsEmpty() ? LOCTEXT("NotSavedNoReason", "The save did not complete.") : CoreMessage);
}
} // namespace PWCreationPause

namespace
{
FVector2D CpPauseSpaceOverride = FVector2D::ZeroVector;
TFunction<FText(bool&)> CpPauseSaveHook;

/** The logical space the pause card may use: its parent, a centring box, is allotted the whole screen. */
FVector2D CpPauseSpace(const TWeakPtr<SWidget>& Self)
{
    if (CpPauseSpaceOverride.X > 0.0 && CpPauseSpaceOverride.Y > 0.0)
    {
        return CpPauseSpaceOverride;
    }
    const TSharedPtr<SWidget> Pinned = Self.Pin();
    const TSharedPtr<SWidget> Parent = Pinned.IsValid() ? Pinned->GetParentWidget() : nullptr;
    if (Parent.IsValid())
    {
        const FVector2D Size(Parent->GetCachedGeometry().GetLocalSize());
        if (Size.X > 1.0 && Size.Y > 1.0)
        {
            return Size;
        }
    }
    return FVector2D(1920.0, 1080.0);
}

bool CpPauseCompact(const TWeakPtr<SWidget>& Self)
{
    const FVector2D Space = CpPauseSpace(Self);
    return PWCreationPause::PauseDensity(static_cast<float>(Space.X), static_cast<float>(Space.Y)) > 0;
}

struct FCpPauseContext
{
    FText Name;
    FText Home;
    FText Place;
};

/** The living character, taken from the creator's own summary: nothing here is invented. */
FCpPauseContext CpGatherPauseContext(const APerfectWorldFrontEndHUD* Owner)
{
    FCpPauseContext Context;
    Context.Name = LOCTEXT("UnnamedLife", "Your life");
    FText City;
    const UPerfectWorldCharacterCreatorSubsystem* Creator = Owner != nullptr ? Owner->GetCreator() : nullptr;
    if (Creator != nullptr)
    {
        const FPWCreationSummary Summary = Creator->GetCreationSummary();
        Context.Name = Summary.FullName.IsEmpty() ? Context.Name : Summary.FullName;
        City = Summary.HomeCityText;
        const bool bBoth = !Summary.HouseholdName.IsEmpty() && !Summary.StreamName.IsEmpty();
        Context.Home = bBoth ? FText::Format(LOCTEXT("HomeStreamFmt", "{0}  \u00B7  {1}"), Summary.HouseholdName, Summary.StreamName)
                             : (Summary.HouseholdName.IsEmpty() ? Summary.StreamName : Summary.HouseholdName);
    }
    // Only one zone is built so far (the lane and its interiors, the zone every save names), so that is the place.
    const FText Zone = LOCTEXT("ZoneNabaparaLane", "Nabapara Lane");
    Context.Place = City.IsEmpty() ? Zone : FText::Format(LOCTEXT("PlaceFmt", "{0}, {1}"), Zone, City);
    return Context;
}
} // namespace

namespace PWCreationPause
{
void SetPauseSpaceForTest(const float Width, const float Height)
{
    CpPauseSpaceOverride = FVector2D(Width, Height);
}

void SetPauseSaveHookForTest(TFunction<FText(bool&)> Hook)
{
    CpPauseSaveHook = MoveTemp(Hook);
}
} // namespace PWCreationPause

void SPWPauseMenu::Construct(const FArguments& InArgs)
{
    Hud = InArgs._Hud;
    MenuContent = BuildMenuContent();

    const TWeakPtr<SWidget> Self = AsShared();
    ChildSlot
    [
        SAssignNew(ContentHolder, SBox)
        .MaxDesiredHeight_Lambda([Self]()
        {
            const FVector2D Space = CpPauseSpace(Self);
            return FOptionalSize(PWCreationPause::PauseMaxHeight(static_cast<float>(Space.X), static_cast<float>(Space.Y)));
        })
        [
            MenuContent.ToSharedRef()
        ]
    ];

    PWUI::FocusWhenReady(AsShared(), ContentHolder);
}

TSharedRef<SWidget> SPWPauseMenu::BuildMenuContent()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TWeakPtr<SWidget> Self = AsShared();
    const TWeakObjectPtr<APerfectWorldFrontEndHUD> WeakHud = Hud;
    const FCpPauseContext Context = CpGatherPauseContext(Hud.Get());
    const TSharedRef<int32> SaveCount = MakeShared<int32>(0);

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

    const TSharedRef<SWidget> ResumeButton = PWUI::MakeButton(
        LOCTEXT("Resume", "Resume"), PWUI::EButtonKind::Primary,
        FOnClicked::CreateSPLambda(this, [this]()
        {
            if (APerfectWorldFrontEndHUD* Owner = Hud.Get()) Owner->ResumeGame();
            return FReply::Handled();
        }), true, 22, 0.0f);

    const TSharedRef<SWidget> SaveButton = PWUI::MakeButton(
        LOCTEXT("Save", "Save"), PWUI::EButtonKind::Secondary,
        FOnClicked::CreateSPLambda(this, [this, SaveCount]()
        {
            bool bSucceeded = false;
            APerfectWorldFrontEndHUD* Owner = Hud.Get();
            const FText Message = CpPauseSaveHook ? CpPauseSaveHook(bSucceeded)
                : Owner != nullptr ? Owner->SaveActiveGame(bSucceeded) : LOCTEXT("NoGameToSave", "There is no life to save.");
            const bool bChooser = !bSucceeded && Message.IsEmpty(); // the slot chooser took over and reports its own outcome
            *SaveCount = bSucceeded ? *SaveCount + 1 : 0;
            Status = bChooser ? FText::GetEmpty() : PWCreationPause::PauseSaveFeedbackText(bSucceeded, Message, *SaveCount);
            bStatusIsError = !bSucceeded && !bChooser;
            return FReply::Handled();
        }), true, 22, 0.0f);

    const auto CompactVisibility = [Self](const bool bShowWhenCompact) -> EVisibility
    {
        return CpPauseCompact(Self) == bShowWhenCompact ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
    };

    // A labelled line of the context card. The label drops out when space is short; the value never does.
    const auto ContextRow = [&Style, CompactVisibility](const FText& Label, const TAttribute<FText>& Value) -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
            [
                SNew(SBox).WidthOverride(68.0f)
                .Visibility_Lambda([CompactVisibility]() { return CompactVisibility(false); })
                [
                    PWUI::MakeText(Label, PWUI::ETextRole::Meta, Style.Accent)
                ]
            ]
            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Top)
            [
                PWUI::MakeText(Value, 16, Style.TextPrimary, false, true)
            ];
    };

    const TAttribute<FText> TimeText = TAttribute<FText>::CreateLambda([WeakHud]()
    {
        const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(WeakHud.Get());
        return Life != nullptr ? Life->GetClockText() : LOCTEXT("TimeUnavailable", "Time unavailable");
    });

    const TSharedRef<SWidget> ContextCard =
        SNew(SBorder)
        .BorderImage(&Style.BrushInset)
        .Padding_Lambda([Self]() { return CpPauseCompact(Self) ? FMargin(14.0f, 8.0f) : FMargin(18.0f, 14.0f); })
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock)
                .Text(Context.Name)
                .Font_Lambda([Self]() { return PWUI::Font(CpPauseCompact(Self) ? 20 : 26, true); })
                .ColorAndOpacity(FSlateColor(Style.TextPrimary))
                .AutoWrapText(true)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
            [
                SNew(SBox)
                .Visibility_Lambda([CompactVisibility, bHasHome = !Context.Home.IsEmpty()]()
                {
                    return bHasHome ? CompactVisibility(false) : EVisibility::Collapsed;
                })
                [
                    PWUI::MakeText(Context.Home, 15, Style.TextSecondary, false, true)
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
            [
                ContextRow(LOCTEXT("PlaceLabel", "PLACE"), Context.Place)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
            [
                ContextRow(LOCTEXT("TimeLabel", "TIME"), TimeText)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
            [
                SNew(SBox)
                .Visibility_Lambda([CompactVisibility]() { return CompactVisibility(false); })
                [
                    PWUI::MakeText(LOCTEXT("ClockHeld", "Time is held while the game is paused."), 14, Style.TextSecondary, false, true)
                ]
            ]
        ];

    // Save feedback sits directly under the Save button. Success and failure differ by word and mark as well as colour.
    const TSharedRef<SWidget> SaveFeedback =
        SNew(SBorder)
        .Tag(TEXT("CpPauseSaveFeedback"))
        .BorderImage(&Style.BrushInset)
        .Padding(FMargin(0.0f))
        .Visibility_Lambda([this]() { return Status.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SBox).WidthOverride(4.0f)
                [
                    SNew(SImage)
                    .Image_Lambda([this]() { return bStatusIsError ? &PWUI::FStyle::Get().BrushFillDanger : &PWUI::FStyle::Get().BrushFillGood; })
                ]
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(12.0f, 8.0f, 0.0f, 8.0f)
            [
                SNew(STextBlock)
                .Text_Lambda([this]() { return bStatusIsError ? LOCTEXT("FailMark", "!") : LOCTEXT("OkMark", "\u2713"); })
                .Font(PWUI::Font(18, true))
                .ColorAndOpacity_Lambda([this]() { return FSlateColor(bStatusIsError ? PWUI::FStyle::Get().Danger : PWUI::FStyle::Get().Good); })
            ]
            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(10.0f, 8.0f, 12.0f, 8.0f)
            [
                SNew(STextBlock)
                .Text_Lambda([this]() { return Status; })
                .Font(PWUI::Font(16, true))
                .ColorAndOpacity_Lambda([this]() { return FSlateColor(bStatusIsError ? PWUI::FStyle::Get().Danger : PWUI::FStyle::Get().Good); })
                .AutoWrapText(true)
            ]
        ];

    const TSharedRef<SWidget> Column =
        SNew(SBox)
        .WidthOverride_Lambda([Self]()
        {
            const FVector2D Space = CpPauseSpace(Self);
            return PWCreationPause::PauseControlWidth(static_cast<float>(Space.X), static_cast<float>(Space.Y));
        })
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 0.0f, 0.0f, 12.0f)
            [
                SNew(SBox)
                .Visibility_Lambda([CompactVisibility]() { return CompactVisibility(false); })
                [
                    SNew(SPWAwakeningSigil).SizeClass(EPWSigilSizeClass::Seal).Diameter(48)
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [
                PWUI::MakeText(LOCTEXT("Title", "Paused"), 32, Style.TextPrimary, true)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 12.0f)
            [
                ContextCard
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
            [
                ResumeButton
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
            [
                SaveButton
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
            [
                SaveFeedback
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
            [
                ReturnToTitleButton.ToSharedRef()
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                QuitButton.ToSharedRef()
            ]
        ];

    // MakeBracketPanel's brackets, with padding that follows the density.
    const TSharedRef<SWidget> Card =
        SNew(SBorder)
        .Tag(TEXT("CpPauseCard"))
        .BorderImage(&Style.BrushPanel)
        .Padding(0.0f)
        [
            SNew(SOverlay)
            + SOverlay::Slot()
            [
                SNew(SPWCornerBrackets)
            ]
            + SOverlay::Slot()
            [
                SNew(SBox)
                .Padding_Lambda([Self]() { return CpPauseCompact(Self) ? FMargin(24.0f, 18.0f) : FMargin(48.0f, 36.0f); })
                [
                    Column
                ]
            ]
        ];

    // If even the compact card is taller than the screen it scrolls rather than clips.
    return SNew(SScrollBox)
        .ScrollBarStyle(&Style.ScrollBar)
        .AllowOverscroll(EAllowOverscroll::No)
        .NavigationDestination(EDescendantScrollDestination::IntoView)
        .ScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll)
        + SScrollBox::Slot()
        [
            Card
        ];
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
    // Time is held while paused, so a save made in this pause is still the whole of the life.
    const bool bJustSaved = !Status.IsEmpty() && !bStatusIsError;
    const FText Warning = bJustSaved
        ? LOCTEXT("SavedJustNow", "You saved just now, so nothing will be lost.")
        : LOCTEXT("UnsavedProgressWarning", "Unsaved progress since your last save will be lost.");

    return PWUI::MakeDialog(
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
        [
            PWUI::MakeText(Title, 30, Style.TextPrimary, true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 24.0f)
        [
            PWUI::MakeText(Warning, 18, bJustSaved ? Style.Good : Style.TextSecondary, false, true)
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
