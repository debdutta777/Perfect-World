#include "UI/SPWTitleScreen.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "Rendering/DrawElements.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "PerfectWorldTitleScreen"

namespace
{
class SPWTitleGradient final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SPWTitleGradient) {}
    SLATE_END_ARGS()
    void Construct(const FArguments&) {}
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
                          FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& WidgetStyle, bool) const override
    {
        const float W = Geometry.GetLocalSize().X;
        TArray<FSlateGradientStop> Stops;
        Stops.Emplace(FVector2D(0, 0), PWUI::Srgb(16, 26, 29, 250) * WidgetStyle.GetColorAndOpacityTint());
        Stops.Emplace(FVector2D(W * .42f, 0), PWUI::Srgb(16, 26, 29, 220) * WidgetStyle.GetColorAndOpacityTint());
        Stops.Emplace(FVector2D(W * .78f, 0), PWUI::Srgb(16, 26, 29, 55) * WidgetStyle.GetColorAndOpacityTint());
        Stops.Emplace(FVector2D(W, 0), FLinearColor::Transparent);
        FSlateDrawElement::MakeGradient(Elements, Layer, Geometry.ToPaintGeometry(), Stops, Orient_Horizontal);
        return Layer;
    }
};

TSharedRef<SWidget> FixedWidth(const TSharedRef<SWidget>& Content)
{
    return SNew(SBox).WidthOverride(560.0f)[Content];
}
} // namespace

void SPWTitleScreen::Construct(const FArguments& InArgs, UPerfectWorldCharacterCreatorSubsystem* InCreator)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    PageReveal = PageTransition.AddCurve(0.0f, 0.18f, ECurveEaseFunction::CubicOut);
    PageTransition.JumpToEnd();
    OnContinue = InArgs._OnContinue;
    ContinueSlot = UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot();
    if (ContinueSlot >= 0 && InCreator) ContinueNote = InCreator->DescribeSaveSlot(ContinueSlot);

    const FSimpleDelegate NewLife = InArgs._OnNewGame;
    const FSimpleDelegate Load = InArgs._OnLoad;
    const FSimpleDelegate Settings = InArgs._OnSettings;
    const FSimpleDelegate Quit = InArgs._OnQuit;
    const bool bHasSave = ContinueSlot >= 0;
    const auto Row = [](const FText& Label, PWUI::EButtonKind Kind, FOnClicked Click)
    {
        return FixedWidth(SNew(SBox).MinDesiredHeight(44.0f)[PWUI::MakeButton(Label, Kind, Click, true, 18)]);
    };

    const TSharedRef<SWidget> Primary = Row(
        bHasSave ? LOCTEXT("Continue", "Continue") : LOCTEXT("NewLifePrimary", "New Life"),
        PWUI::EButtonKind::Primary,
        bHasSave ? FOnClicked::CreateLambda([this]() { if (OnContinue.IsBound()) Status = OnContinue.Execute(); return FReply::Handled(); })
                 : FOnClicked::CreateLambda([NewLife]() { NewLife.ExecuteIfBound(); return FReply::Handled(); }));
    MainFocusTarget = Primary;
    TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)[Primary];
    if (bHasSave)
    {
        Menu->AddSlot().AutoHeight().Padding(18, 0, 8, 12)[PWUI::MakeText(ContinueNote, 14, Style.TextSecondary, false, true)];
        Menu->AddSlot().AutoHeight().Padding(0, 0, 0, 2)
        [Row(LOCTEXT("NewLifeSecondary", "New Life"), PWUI::EButtonKind::TextRow,
             FOnClicked::CreateLambda([NewLife]() { NewLife.ExecuteIfBound(); return FReply::Handled(); }))];
    }
    Menu->AddSlot().AutoHeight().Padding(0, 0, 0, 2)
    [Row(LOCTEXT("Load", "Load"), PWUI::EButtonKind::TextRow, FOnClicked::CreateLambda([Load]() { Load.ExecuteIfBound(); return FReply::Handled(); }))];
    Menu->AddSlot().AutoHeight().Padding(0, 0, 0, 2)
    [Row(LOCTEXT("Settings", "Settings"), PWUI::EButtonKind::TextRow, FOnClicked::CreateLambda([Settings]() { Settings.ExecuteIfBound(); return FReply::Handled(); }))];
    Menu->AddSlot().AutoHeight().Padding(0, 0, 0, 2)
    [Row(LOCTEXT("Credits", "Credits"), PWUI::EButtonKind::TextRow, FOnClicked::CreateSP(this, &SPWTitleScreen::ShowCredits))];
    Menu->AddSlot().AutoHeight()
    [Row(LOCTEXT("Quit", "Quit"), PWUI::EButtonKind::TextRow, FOnClicked::CreateLambda([Quit]() { Quit.ExecuteIfBound(); return FReply::Handled(); }))];
    Menu->AddSlot().AutoHeight().Padding(12, 8, 0, 0)
    [PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return Status; }), 14, Style.Danger, false, true)];

    const TSharedRef<SWidget> MainView = SNew(SScrollBox).ScrollBarStyle(&Style.ScrollBar)
        + SScrollBox::Slot().Padding(FMargin(64, 34, 24, 24))
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [SNew(SScaleBox).Stretch(EStretch::ScaleToFitX).StretchDirection(EStretchDirection::DownOnly)
                [PWUI::MakeText(LOCTEXT("TitleMain", "PERFECT WORLD"), PWUI::ETextRole::Title, Style.TextPrimary)]]
            + SVerticalBox::Slot().AutoHeight().Padding(2, 2, 0, 0)
            [PWUI::MakeText(LOCTEXT("TitleSub", "AWAKENING"), 25, Style.Accent, true)]
            + SVerticalBox::Slot().AutoHeight().Padding(2, 14, 0, 0)
            [PWUI::MakeText(LOCTEXT("LifeLine", "A whole life begins here."), 21, Style.TextPrimary)]
            + SVerticalBox::Slot().AutoHeight().Padding(2, 4, 0, 24)
            [PWUI::MakeText(LOCTEXT("TitlePlace", "Kolkata \u00b7 2 June 2042"), 16, Style.TextSecondary)]
            + SVerticalBox::Slot().AutoHeight()[Menu]
            + SVerticalBox::Slot().AutoHeight().Padding(2, 18, 0, 0)
            [PWUI::MakeText(LOCTEXT("BuildVersion", "0.1.0 · Development build"), 12, Style.TextSecondary)]];

    const TSharedRef<SWidget> Back = Row(LOCTEXT("Back", "Back"), PWUI::EButtonKind::TextRow,
                                         FOnClicked::CreateSP(this, &SPWTitleScreen::CloseCredits));
    CreditsFocusTarget = Back;
    const TSharedRef<SWidget> CreditsView = SNew(SScrollBox).ScrollBarStyle(&Style.ScrollBar)
        + SScrollBox::Slot().Padding(FMargin(64, 48, 24, 24))
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 26)[PWUI::MakeText(LOCTEXT("CreditsHeading", "CREDITS"), PWUI::ETextRole::PageHeading, Style.TextPrimary)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)[PWUI::MakeText(LOCTEXT("CreditsProject", "Perfect World: Awakening"), 22, Style.Accent, true)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[PWUI::MakeText(LOCTEXT("CreditsMadeWith", "Made with Unreal Engine"), 17, Style.TextSecondary)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[PWUI::MakeText(LOCTEXT("CreditsFont", "Hind Siliguri by Indian Type Foundry"), 17, Style.TextSecondary)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 30)[PWUI::MakeText(LOCTEXT("CreditsLicense", "SIL Open Font License 1.1"), 15, Style.TextMuted)]
            + SVerticalBox::Slot().AutoHeight()[Back]];

    ChildSlot[SNew(SOverlay)
        + SOverlay::Slot()[SNew(SPWTitleGradient)]
        + SOverlay::Slot().HAlign(HAlign_Left)
        [SAssignNew(ViewSwitcher, SWidgetSwitcher)
            .RenderTransform_Lambda([this]()
            {
                return FSlateRenderTransform(FVector2f(0.0f, 8.0f * (1.0f - PageReveal.GetLerp())));
            })
            + SWidgetSwitcher::Slot()[MainView] + SWidgetSwitcher::Slot()[CreditsView]]];
    PWUI::FocusWhenReady(AsShared(), MainFocusTarget);
}

void SPWTitleScreen::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    ViewSwitcher->SetRenderOpacity(PageReveal.GetLerp());
}

FReply SPWTitleScreen::ShowCredits()
{
    bShowingCredits = true;
    ViewSwitcher->SetActiveWidgetIndex(1);
    PageTransition.JumpToEnd();
    PageTransition.Play(AsShared());
    PWUI::CancelPendingFocus(AsShared());
    PWUI::FocusWhenReady(AsShared(), CreditsFocusTarget);
    return FReply::Handled();
}

FReply SPWTitleScreen::CloseCredits()
{
    bShowingCredits = false;
    ViewSwitcher->SetActiveWidgetIndex(0);
    PageTransition.JumpToEnd();
    PageTransition.Play(AsShared());
    PWUI::CancelPendingFocus(AsShared());
    PWUI::FocusWhenReady(AsShared(), MainFocusTarget);
    return FReply::Handled();
}

FReply SPWTitleScreen::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    // A held key auto-repeats; menus react to the first press only.
    if (Event.IsRepeat())
    {
        return FReply::Handled();
    }
    if (bShowingCredits && (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::Gamepad_FaceButton_Right)) return CloseCredits();
    return SCompoundWidget::OnKeyDown(Geometry, Event);
}

#undef LOCTEXT_NAMESPACE
